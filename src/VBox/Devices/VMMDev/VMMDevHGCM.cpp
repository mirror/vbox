/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager device
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/log.h>

#include "VMMDevHGCM.h"

typedef enum _VBOXHGCMCMDTYPE
{
    VBOXHGCMCMDTYPE_LOADSTATE,
    VBOXHGCMCMDTYPE_CONNECT,
    VBOXHGCMCMDTYPE_DISCONNECT,
    VBOXHGCMCMDTYPE_CALL,
    VBOXHGCMCMDTYPE_SizeHack = 0x7fffffff
} VBOXHGCMCMDTYPE;

/* Information about a linear ptr parameter. */
typedef struct _VBOXHGCMLINPTR
{
    /* Index of the parameter. */
    int iParm;
    
    /* Offset in the first physical page of the region. */
    size_t cbOffsetFirstPage;
    
    /* How many pages. */
    uint32_t cPages;
    
    /* Pointer to array of the GC physical addresses for these pages. 
     * It is assumed that the physical address of the locked resident
     * guest page does not change.
     */
    RTGCPHYS *paPages;
    
} VBOXHGCMLINPTR;

struct VBOXHGCMCMD
{
    /* Active commands, list is protected by critsectHGCMCmdList. */
    struct VBOXHGCMCMD *pNext;
    struct VBOXHGCMCMD *pPrev;

    /* The type of the command. */
    VBOXHGCMCMDTYPE enmCmdType;

    /* GC physical address of the guest request. */
    RTGCPHYS        GCPhys;

    /* Request packet size */
    uint32_t        cbSize;

    /* Pointer to converted host parameters in case of a Call request. */
    VBOXHGCMSVCPARM *paHostParms;

    /* Linear pointer parameters information. */
    int cLinPtrs;

    /* Pointer to descriptions of linear pointers.  */
    VBOXHGCMLINPTR *paLinPtrs;
};

static int vmmdevHGCMCmdListLock (VMMDevState *pVMMDevState)
{
    int rc = RTCritSectEnter (&pVMMDevState->critsectHGCMCmdList);
    AssertRC (rc);
    return rc;
}

static void vmmdevHGCMCmdListUnlock (VMMDevState *pVMMDevState)
{
    int rc = RTCritSectLeave (&pVMMDevState->critsectHGCMCmdList);
    AssertRC (rc);
}

static int vmmdevHGCMAddCommand (VMMDevState *pVMMDevState, PVBOXHGCMCMD pCmd, RTGCPHYS GCPhys, uint32_t cbSize, VBOXHGCMCMDTYPE enmCmdType)
{
    /* PPDMDEVINS pDevIns = pVMMDevState->pDevIns; */

    int rc = vmmdevHGCMCmdListLock (pVMMDevState);
    
    if (VBOX_SUCCESS (rc))
    {
        LogFlowFunc(("%p type %d\n", pCmd, enmCmdType));

        /* Insert at the head of the list. The vmmdevHGCMLoadStateDone depends on this. */
        pCmd->pNext = pVMMDevState->pHGCMCmdList;
        pCmd->pPrev = NULL;
        
        if (pVMMDevState->pHGCMCmdList)
        {
            pVMMDevState->pHGCMCmdList->pPrev = pCmd;
        }
        
        pVMMDevState->pHGCMCmdList = pCmd;
        
        pCmd->enmCmdType = enmCmdType;
        pCmd->GCPhys = GCPhys;
        pCmd->cbSize = cbSize;
        
        /* Automatically enable HGCM events, if there are HGCM commands. */
        if (   enmCmdType == VBOXHGCMCMDTYPE_CONNECT
            || enmCmdType == VBOXHGCMCMDTYPE_DISCONNECT
            || enmCmdType == VBOXHGCMCMDTYPE_CALL)
        {
            Log(("vmmdevHGCMAddCommand: u32HGCMEnabled = %d\n", pVMMDevState->u32HGCMEnabled));
            if (ASMAtomicCmpXchgU32(&pVMMDevState->u32HGCMEnabled, 1, 0))
            {
                 VMMDevCtlSetGuestFilterMask (pVMMDevState, VMMDEV_EVENT_HGCM, 0);
            }
        }

        vmmdevHGCMCmdListUnlock (pVMMDevState);
    }
    
    return rc;
}

static int vmmdevHGCMRemoveCommand (VMMDevState *pVMMDevState, PVBOXHGCMCMD pCmd)
{
    /* PPDMDEVINS pDevIns = pVMMDevState->pDevIns; */

    int rc = vmmdevHGCMCmdListLock (pVMMDevState);
    
    if (VBOX_SUCCESS (rc))
    {
        LogFlowFunc(("%p\n", pCmd));

        if (pCmd->pNext)
        {
            pCmd->pNext->pPrev = pCmd->pPrev;
        }
        else
        {
           /* Tail, do nothing. */
        }
        
        if (pCmd->pPrev)
        {
            pCmd->pPrev->pNext = pCmd->pNext;
        }
        else
        {
            pVMMDevState->pHGCMCmdList = pCmd->pNext;
        }

        vmmdevHGCMCmdListUnlock (pVMMDevState);
    }
    
    return rc;
}

static int vmmdevHGCMSaveLinPtr (PPDMDEVINS pDevIns,
                                 uint32_t iParm,
                                 RTGCPTR GCPtr,
                                 uint32_t u32Size,
                                 uint32_t iLinPtr,
                                 VBOXHGCMLINPTR *paLinPtrs,
                                 RTGCPHYS **ppPages)
{
    int rc = VINF_SUCCESS;
    
    AssertRelease (u32Size > 0);
    
    VBOXHGCMLINPTR *pLinPtr = &paLinPtrs[iLinPtr];
    
    /* Take the offset into the current page also into account! */
    u32Size += GCPtr & PAGE_OFFSET_MASK;

    uint32_t cPages = (u32Size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    Log(("vmmdevHGCMSaveLinPtr: parm %d: %VGv %d = %d pages\n", iParm, GCPtr, u32Size, cPages));
    
    pLinPtr->iParm             = iParm;
    pLinPtr->cbOffsetFirstPage = (RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK;
    pLinPtr->cPages            = cPages;
    pLinPtr->paPages           = *ppPages;
    
    *ppPages += cPages;
    
    uint32_t iPage = 0;
    
    GCPtr &= PAGE_BASE_GC_MASK;
    
    /* Gonvert the guest linear pointers of pages to HC addresses. */
    while (iPage < cPages)
    {
        /* convert */
        RTGCPHYS GCPhys;
        
        rc = PDMDevHlpPhysGCPtr2GCPhys(pDevIns, GCPtr, &GCPhys);
        
        Log(("vmmdevHGCMSaveLinPtr: Page %d: %VGv -> %VGp. %Vrc\n", iPage, GCPtr, GCPhys, rc));
    
        if (VBOX_FAILURE (rc))
        {
            break;
        }

        /* store */
        pLinPtr->paPages[iPage++] = GCPhys;

        /* next */
        GCPtr += PAGE_SIZE;
    }
    
    AssertRelease (iPage == cPages);
    
    return rc;
}

static int vmmdevHGCMWriteLinPtr (PPDMDEVINS pDevIns,
                                  uint32_t iParm,
                                  void *pvHost,
                                  uint32_t u32Size,
                                  uint32_t iLinPtr,
                                  VBOXHGCMLINPTR *paLinPtrs)
{
    int rc = VINF_SUCCESS;
    
    VBOXHGCMLINPTR *pLinPtr = &paLinPtrs[iLinPtr];
    
    AssertRelease (u32Size > 0 && iParm == (uint32_t)pLinPtr->iParm);
    
    RTGCPHYS GCPhysDst = pLinPtr->paPages[0] + pLinPtr->cbOffsetFirstPage;
    uint8_t *pu8Src    = (uint8_t *)pvHost;
    
    Log(("vmmdevHGCMWriteLinPtr: parm %d: size %d, cPages = %d\n", iParm, u32Size, pLinPtr->cPages));
    
    uint32_t iPage = 0;
    
    while (iPage < pLinPtr->cPages)
    {
        /* copy */
        size_t cbWrite = iPage == 0?
                             PAGE_SIZE - pLinPtr->cbOffsetFirstPage:
                             PAGE_SIZE;
                             
        Log(("vmmdevHGCMWriteLinPtr: page %d: dst %VGp, src %p, cbWrite %d\n", iPage, GCPhysDst, pu8Src, cbWrite));
        
        iPage++;
        
        if (cbWrite >= u32Size)
        {
            PDMDevHlpPhysWrite(pDevIns, GCPhysDst, pu8Src, u32Size);
            u32Size = 0;
            break;
        }
        
        PDMDevHlpPhysWrite(pDevIns, GCPhysDst, pu8Src, cbWrite);

        /* next */
        u32Size    -= cbWrite;
        pu8Src     += cbWrite;
        
        GCPhysDst   = pLinPtr->paPages[iPage];
    }
    
    AssertRelease (iPage == pLinPtr->cPages);
    Assert(u32Size == 0);
    
    return rc;
}

int vmmdevHGCMConnect (VMMDevState *pVMMDevState, VMMDevHGCMConnect *pHGCMConnect, RTGCPHYS GCPhys)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD) + pHGCMConnect->header.header.size);

    if (pCmd)
    {
        VMMDevHGCMConnect *pHGCMConnectCopy = (VMMDevHGCMConnect *)(pCmd+1);
        
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPhys, pHGCMConnect->header.header.size, VBOXHGCMCMDTYPE_CONNECT);

        memcpy(pHGCMConnectCopy, pHGCMConnect, pHGCMConnect->header.header.size);

        pCmd->paHostParms = NULL;
        pCmd->cLinPtrs = 0;
        pCmd->paLinPtrs = NULL;

        /* Only allow the guest to use existing services! */
        Assert(pHGCMConnect->loc.type == VMMDevHGCMLoc_LocalHost_Existing);
        pHGCMConnect->loc.type = VMMDevHGCMLoc_LocalHost_Existing;

        rc = pVMMDevState->pHGCMDrv->pfnConnect (pVMMDevState->pHGCMDrv, pCmd, &pHGCMConnectCopy->loc, &pHGCMConnectCopy->u32ClientID);
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

int vmmdevHGCMDisconnect (VMMDevState *pVMMDevState, VMMDevHGCMDisconnect *pHGCMDisconnect, RTGCPHYS GCPhys)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD));

    if (pCmd)
    {
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPhys, pHGCMDisconnect->header.header.size, VBOXHGCMCMDTYPE_DISCONNECT);

        pCmd->paHostParms = NULL;
        pCmd->cLinPtrs = 0;
        pCmd->paLinPtrs = NULL;

        rc = pVMMDevState->pHGCMDrv->pfnDisconnect (pVMMDevState->pHGCMDrv, pCmd, pHGCMDisconnect->u32ClientID);
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}


int vmmdevHGCMCall (VMMDevState *pVMMDevState, VMMDevHGCMCall *pHGCMCall, RTGCPHYS GCPhys, bool f64Bits)
{
    int rc = VINF_SUCCESS;

    Log(("vmmdevHGCMCall: client id = %d, function = %d, %s bit\n", pHGCMCall->u32ClientID, pHGCMCall->u32Function, f64Bits? "64": "32"));

    /* Compute size and allocate memory block to hold:
     *    struct VBOXHGCMCMD
     *    VBOXHGCMSVCPARM[cParms]
     *    memory buffers for pointer parameters.
     */

    uint32_t cParms = pHGCMCall->cParms;

    Log(("vmmdevHGCMCall: cParms = %d\n", cParms));

    /*
     * Compute size of required memory buffer.
     */

    uint32_t cbCmdSize = sizeof (struct VBOXHGCMCMD) + cParms * sizeof (VBOXHGCMSVCPARM);

    uint32_t i;

    uint32_t cLinPtrs = 0;
    uint32_t cLinPtrPages  = 0;

    if (f64Bits)
    {
#ifdef VBOX_WITH_64_BITS_GUESTS
        HGCMFunctionParameter64 *pGuestParm = VMMDEV_HGCM_CALL_PARMS64(pHGCMCall);
#else
        HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
        AssertFailed (); /* This code should not be called in this case */
#endif /* VBOX_WITH_64_BITS_GUESTS */

        /* Look for pointer parameters, which require a host buffer. */
        for (i = 0; i < cParms && VBOX_SUCCESS(rc); i++, pGuestParm++)
        {
            switch (pGuestParm->type)
            {
                case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                {
                    cbCmdSize += pGuestParm->u.Pointer.size;
                
                    if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                    {
                        cLinPtrs++;
                        /* Take the offset into the current page also into account! */
                        cLinPtrPages += ((pGuestParm->u.Pointer.u.linearAddr & PAGE_OFFSET_MASK)
                                          + pGuestParm->u.Pointer.size + PAGE_SIZE - 1) / PAGE_SIZE;
                    }
                
                    Log(("vmmdevHGCMCall: linptr size = %d\n", pGuestParm->u.Pointer.size));
                } break;

                case VMMDevHGCMParmType_32bit:
                case VMMDevHGCMParmType_64bit:
                case VMMDevHGCMParmType_PhysAddr:
                {
                } break;

                default:
                {
                    AssertMsgFailed(("vmmdevHGCMCall: invalid parameter type %x\n", pGuestParm->type));
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
            }
        }
    }
    else
    {
#ifdef VBOX_WITH_64_BITS_GUESTS
        HGCMFunctionParameter32 *pGuestParm = VMMDEV_HGCM_CALL_PARMS32(pHGCMCall);
#else
        HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
#endif /* VBOX_WITH_64_BITS_GUESTS */

        /* Look for pointer parameters, which require a host buffer. */
        for (i = 0; i < cParms && VBOX_SUCCESS(rc); i++, pGuestParm++)
        {
            switch (pGuestParm->type)
            {
                case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                {
                    cbCmdSize += pGuestParm->u.Pointer.size;
                
                    if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                    {
                        cLinPtrs++;
                        /* Take the offset into the current page also into account! */
                        cLinPtrPages += ((pGuestParm->u.Pointer.u.linearAddr & PAGE_OFFSET_MASK)
                                          + pGuestParm->u.Pointer.size + PAGE_SIZE - 1) / PAGE_SIZE;
                    }
                
                    Log(("vmmdevHGCMCall: linptr size = %d\n", pGuestParm->u.Pointer.size));
                } break;

                case VMMDevHGCMParmType_32bit:
                case VMMDevHGCMParmType_64bit:
                case VMMDevHGCMParmType_PhysAddr:
                {
                } break;

                default:
                {
                    AssertMsgFailed(("vmmdevHGCMCall: invalid parameter type %x\n", pGuestParm->type));
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
            }
        }
    }

    if (VBOX_FAILURE (rc))
    {
        return rc;
    }

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAlloc (cbCmdSize);

    if (pCmd == NULL)
    {
        return VERR_NO_MEMORY;
    }

    memset (pCmd, 0, sizeof (*pCmd));
    
    pCmd->paHostParms = NULL;
    pCmd->cLinPtrs    = cLinPtrs;
    
    if (cLinPtrs > 0)
    {
        pCmd->paLinPtrs = (VBOXHGCMLINPTR *)RTMemAlloc (  sizeof (VBOXHGCMLINPTR) * cLinPtrs
                                                          + sizeof (RTGCPHYS) * cLinPtrPages);
    
        if (pCmd->paLinPtrs == NULL)
        {
            RTMemFree (pCmd);
            return VERR_NO_MEMORY;
        }
    }
    else
    {
        pCmd->paLinPtrs = NULL;
    }

    /* Process parameters, changing them to host context pointers for easy
     * processing by connector. Guest must insure that the pointed data is actually
     * in the guest RAM and remains locked there for entire request processing.
     */

    if (cParms != 0)
    {
        /* Compute addresses of host parms array and first memory buffer. */
        VBOXHGCMSVCPARM *pHostParm = (VBOXHGCMSVCPARM *)((char *)pCmd + sizeof (struct VBOXHGCMCMD));
        
        uint8_t *pcBuf = (uint8_t *)pHostParm + cParms * sizeof (VBOXHGCMSVCPARM);

        pCmd->paHostParms = pHostParm;

        uint32_t iLinPtr = 0;
        RTGCPHYS *pPages  = (RTGCPHYS *)((uint8_t *)pCmd->paLinPtrs + sizeof (VBOXHGCMLINPTR) *cLinPtrs);

        if (f64Bits)
        {
#ifdef VBOX_WITH_64_BITS_GUESTS
            HGCMFunctionParameter64 *pGuestParm = VMMDEV_HGCM_CALL_PARMS64(pHGCMCall);
#else
            HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
            AssertFailed (); /* This code should not be called in this case */
#endif /* VBOX_WITH_64_BITS_GUESTS */


            for (i = 0; i < cParms && VBOX_SUCCESS(rc); i++, pGuestParm++, pHostParm++)
            {
                switch (pGuestParm->type)
                {
                     case VMMDevHGCMParmType_32bit:
                     {
                         uint32_t u32 = pGuestParm->u.value32;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_32BIT;
                         pHostParm->u.uint32 = u32;

                         Log(("vmmdevHGCMCall: uint32 guest parameter %u\n", u32));
                         break;
                     } 

                     case VMMDevHGCMParmType_64bit:
                     {
                         uint64_t u64 = pGuestParm->u.value64;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_64BIT;
                         pHostParm->u.uint64 = u64;

                         Log(("vmmdevHGCMCall: uint64 guest parameter %llu\n", u64));
                         break;
                     } 

                     case VMMDevHGCMParmType_PhysAddr:
                     {
                         uint32_t size = pGuestParm->u.Pointer.size;
#ifdef LOG_ENABLED
                         RTGCPHYS physAddr = pGuestParm->u.Pointer.u.physAddr;
#endif

                         pHostParm->type = VBOX_HGCM_SVC_PARM_PTR;
                         pHostParm->u.pointer.size = size;

                         AssertFailed();
                         /* rc = PDMDevHlpPhys2HCVirt (pVMMDevState->pDevIns, physAddr, size, &pHostParm->u.pointer.addr); */

                         Log(("vmmdevHGCMCall: PhysAddr guest parameter %VGp\n", physAddr));
                         break;
                     } 

                     case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                     case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                     case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                     {
                         uint32_t size = pGuestParm->u.Pointer.size;
                         RTGCPTR linearAddr = pGuestParm->u.Pointer.u.linearAddr;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_PTR;
                         pHostParm->u.pointer.size = size;

                         /* Copy guest data to an allocated buffer, so
                          * services can use the data.
                          */

                         if (size == 0)
                         {
                             pHostParm->u.pointer.addr = (void *) 0xfeeddead;
                         }
                         else
                         {
                             /* Don't overdo it */
                             if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_Out)
                                 rc = PDMDevHlpPhysReadGCVirt(pVMMDevState->pDevIns, pcBuf, linearAddr, size);
                             else
                                 rc = VINF_SUCCESS;

                             if (VBOX_SUCCESS(rc))
                             {
                                 pHostParm->u.pointer.addr = pcBuf;
                                 pcBuf += size;
                             
                                 if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                                 {
                                     /* Remember the guest physical pages that belong to the virtual address 
                                      * region.
                                      */
                                     rc = vmmdevHGCMSaveLinPtr (pVMMDevState->pDevIns, i, linearAddr, size, iLinPtr++, pCmd->paLinPtrs, &pPages);
                                 }
                             }
                         }

                         Log(("vmmdevHGCMCall: LinAddr guest parameter %VGv, rc = %Vrc\n", linearAddr, rc));
                         break;
                     }

                    /* just to shut up gcc */
                    default:
                        break;
                }
            }
        }
        else
        {
#ifdef VBOX_WITH_64_BITS_GUESTS
            HGCMFunctionParameter32 *pGuestParm = VMMDEV_HGCM_CALL_PARMS32(pHGCMCall);
#else
            HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
#endif /* VBOX_WITH_64_BITS_GUESTS */

            for (i = 0; i < cParms && VBOX_SUCCESS(rc); i++, pGuestParm++, pHostParm++)
            {
                switch (pGuestParm->type)
                {
                     case VMMDevHGCMParmType_32bit:
                     {
                         uint32_t u32 = pGuestParm->u.value32;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_32BIT;
                         pHostParm->u.uint32 = u32;

                         Log(("vmmdevHGCMCall: uint32 guest parameter %u\n", u32));
                         break;
                     } 

                     case VMMDevHGCMParmType_64bit:
                     {
                         uint64_t u64 = pGuestParm->u.value64;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_64BIT;
                         pHostParm->u.uint64 = u64;

                         Log(("vmmdevHGCMCall: uint64 guest parameter %llu\n", u64));
                         break;
                     } 

                     case VMMDevHGCMParmType_PhysAddr:
                     {
                         uint32_t size = pGuestParm->u.Pointer.size;
#ifdef LOG_ENABLED
                         RTGCPHYS physAddr = pGuestParm->u.Pointer.u.physAddr;
#endif

                         pHostParm->type = VBOX_HGCM_SVC_PARM_PTR;
                         pHostParm->u.pointer.size = size;

                         AssertFailed();
                         /* rc = PDMDevHlpPhys2HCVirt (pVMMDevState->pDevIns, physAddr, size, &pHostParm->u.pointer.addr); */

                         Log(("vmmdevHGCMCall: PhysAddr guest parameter %VGp\n", physAddr));
                         break;
                     } 

                     case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                     case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                     case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                     {
                         uint32_t size = pGuestParm->u.Pointer.size;
                         RTGCPTR linearAddr = pGuestParm->u.Pointer.u.linearAddr;

                         pHostParm->type = VBOX_HGCM_SVC_PARM_PTR;
                         pHostParm->u.pointer.size = size;

                         /* Copy guest data to an allocated buffer, so
                          * services can use the data.
                          */

                         if (size == 0)
                         {
                             pHostParm->u.pointer.addr = (void *) 0xfeeddead;
                         }
                         else
                         {
                             /* Don't overdo it */
                             if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_Out)
                                 rc = PDMDevHlpPhysReadGCVirt(pVMMDevState->pDevIns, pcBuf, linearAddr, size);
                             else
                                 rc = VINF_SUCCESS;

                             if (VBOX_SUCCESS(rc))
                             {
                                 pHostParm->u.pointer.addr = pcBuf;
                                 pcBuf += size;
                             
                                 if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                                 {
                                     /* Remember the guest physical pages that belong to the virtual address 
                                      * region.
                                      */
                                     rc = vmmdevHGCMSaveLinPtr (pVMMDevState->pDevIns, i, linearAddr, size, iLinPtr++, pCmd->paLinPtrs, &pPages);
                                 }
                             }
                         }

                         Log(("vmmdevHGCMCall: LinAddr guest parameter %VGv, rc = %Vrc\n", linearAddr, rc));
                         break;
                     }

                    /* just to shut up gcc */
                    default:
                        break;
                }
            }
        }
    }

    if (VBOX_SUCCESS (rc))
    {
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPhys, pHGCMCall->header.header.size, VBOXHGCMCMDTYPE_CALL);

        /* Pass the function call to HGCM connector for actual processing */
        rc = pVMMDevState->pHGCMDrv->pfnCall (pVMMDevState->pHGCMDrv, pCmd, pHGCMCall->u32ClientID, pHGCMCall->u32Function, cParms, pCmd->paHostParms);
    }
    else
    {
        if (pCmd->paLinPtrs)
        {
            RTMemFree (pCmd->paLinPtrs);
        }
        
        RTMemFree (pCmd);
    }

    return rc;
}

static int vmmdevHGCMCmdVerify (PVBOXHGCMCMD pCmd, VMMDevHGCMRequestHeader *pHeader)
{
    switch (pCmd->enmCmdType)
    {
        case VBOXHGCMCMDTYPE_CONNECT:
            if (pHeader->header.requestType == VMMDevReq_HGCMConnect) return VINF_SUCCESS;
            break;

        case VBOXHGCMCMDTYPE_DISCONNECT:
            if (pHeader->header.requestType == VMMDevReq_HGCMDisconnect) return VINF_SUCCESS;
            break;

        case VBOXHGCMCMDTYPE_CALL:
#ifdef VBOX_WITH_64_BITS_GUESTS
            if (   pHeader->header.requestType == VMMDevReq_HGCMCall32
                || pHeader->header.requestType == VMMDevReq_HGCMCall64) return VINF_SUCCESS;
#else
            if (   pHeader->header.requestType == VMMDevReq_HGCMCall) return VINF_SUCCESS;
#endif /* VBOX_WITH_64_BITS_GUESTS */

            break;

        default:
            AssertFailed ();
    }

    LogRel(("VMMDEV: Invalid HGCM command: pCmd->enmCmdType = 0x%08X, pHeader->header.requestType = 0x%08X\n",
          pCmd->enmCmdType, pHeader->header.requestType));
    return VERR_INVALID_PARAMETER;
}

#define PDMIHGCMPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState *) ((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, HGCMPort)) )

DECLCALLBACK(void) hgcmCompletedWorker (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmd)
{
    VMMDevState             *pVMMDevState = PDMIHGCMPORT_2_VMMDEVSTATE(pInterface);
    VMMDevHGCMRequestHeader *pHeader;
    int                      rc = VINF_SUCCESS;

    pHeader = (VMMDevHGCMRequestHeader *)RTMemAllocZ (pCmd->cbSize);
    Assert(pHeader);
    if (pHeader == NULL)
        return;

    PDMDevHlpPhysRead(pVMMDevState->pDevIns, (RTGCPHYS)pCmd->GCPhys, pHeader, pCmd->cbSize);

    if (result != VINF_HGCM_SAVE_STATE)
    {
        /* Setup return codes. */
        pHeader->result = result;
        
        /* Verify the request type. */
        rc = vmmdevHGCMCmdVerify (pCmd, pHeader);

        if (VBOX_SUCCESS (rc))
        {
            /* Update parameters and data buffers. */
           
            switch (pHeader->header.requestType)
            {
#ifdef VBOX_WITH_64_BITS_GUESTS
            case VMMDevReq_HGCMCall64:
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pHeader;

                uint32_t cParms = pHGCMCall->cParms;

                VBOXHGCMSVCPARM *pHostParm = pCmd->paHostParms;

                uint32_t i;
                uint32_t iLinPtr = 0;

                HGCMFunctionParameter64 *pGuestParm = VMMDEV_HGCM_CALL_PARMS64(pHGCMCall);

                for (i = 0; i < cParms; i++, pGuestParm++, pHostParm++)
                {
                    switch (pGuestParm->type)
                    {
                        case VMMDevHGCMParmType_32bit:
                        {
                            pGuestParm->u.value32 = pHostParm->u.uint32;
                        } break;

                        case VMMDevHGCMParmType_64bit:
                        {
                            pGuestParm->u.value64 = pHostParm->u.uint64;
                        } break;

                        case VMMDevHGCMParmType_PhysAddr:
                        {
                            /* do nothing */
                        } break;

                        case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                        case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                        case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                        {
                            /* Copy buffer back to guest memory. */
                            uint32_t size = pGuestParm->u.Pointer.size;

                            if (size > 0 && pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                            {
                                /* Use the saved page list. */
                                rc = vmmdevHGCMWriteLinPtr (pVMMDevState->pDevIns, i, pHostParm->u.pointer.addr, size, iLinPtr++, pCmd->paLinPtrs);
                                AssertReleaseRC(rc);
                            }
                        } break;

                        default:
                        {
                            /* This indicates that the guest request memory was corrupted. */
                            AssertReleaseMsgFailed(("hgcmCompleted: invalid parameter type %08X\n", pGuestParm->type));
                        }
                    }
                }
                break;
            }

            case VMMDevReq_HGCMCall32:
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pHeader;

                uint32_t cParms = pHGCMCall->cParms;

                VBOXHGCMSVCPARM *pHostParm = pCmd->paHostParms;

                uint32_t i;
                uint32_t iLinPtr = 0;

                HGCMFunctionParameter32 *pGuestParm = VMMDEV_HGCM_CALL_PARMS32(pHGCMCall);

                for (i = 0; i < cParms; i++, pGuestParm++, pHostParm++)
                {
                    switch (pGuestParm->type)
                    {
                        case VMMDevHGCMParmType_32bit:
                        {
                            pGuestParm->u.value32 = pHostParm->u.uint32;
                        } break;

                        case VMMDevHGCMParmType_64bit:
                        {
                            pGuestParm->u.value64 = pHostParm->u.uint64;
                        } break;

                        case VMMDevHGCMParmType_PhysAddr:
                        {
                            /* do nothing */
                        } break;

                        case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                        case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                        case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                        {
                            /* Copy buffer back to guest memory. */
                            uint32_t size = pGuestParm->u.Pointer.size;

                            if (size > 0 && pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                            {
                                /* Use the saved page list. */
                                rc = vmmdevHGCMWriteLinPtr (pVMMDevState->pDevIns, i, pHostParm->u.pointer.addr, size, iLinPtr++, pCmd->paLinPtrs);
                                AssertReleaseRC(rc);
                            }
                        } break;

                        default:
                        {
                            /* This indicates that the guest request memory was corrupted. */
                            AssertReleaseMsgFailed(("hgcmCompleted: invalid parameter type %08X\n", pGuestParm->type));
                        }
                    }
                }
                break;
            }
#else
            case VMMDevReq_HGCMCall:
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pHeader;

                uint32_t cParms = pHGCMCall->cParms;

                VBOXHGCMSVCPARM *pHostParm = pCmd->paHostParms;

                uint32_t i;
                uint32_t iLinPtr = 0;

                HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);

                for (i = 0; i < cParms; i++, pGuestParm++, pHostParm++)
                {
                    switch (pGuestParm->type)
                    {
                        case VMMDevHGCMParmType_32bit:
                        {
                            pGuestParm->u.value32 = pHostParm->u.uint32;
                        } break;

                        case VMMDevHGCMParmType_64bit:
                        {
                            pGuestParm->u.value64 = pHostParm->u.uint64;
                        } break;

                        case VMMDevHGCMParmType_PhysAddr:
                        {
                            /* do nothing */
                        } break;

                        case VMMDevHGCMParmType_LinAddr_In:  /* In (read) */
                        case VMMDevHGCMParmType_LinAddr_Out: /* Out (write) */
                        case VMMDevHGCMParmType_LinAddr:     /* In & Out */
                        {
                            /* Copy buffer back to guest memory. */
                            uint32_t size = pGuestParm->u.Pointer.size;

                            if (size > 0 && pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                            {
                                /* Use the saved page list. */
                                rc = vmmdevHGCMWriteLinPtr (pVMMDevState->pDevIns, i, pHostParm->u.pointer.addr, size, iLinPtr++, pCmd->paLinPtrs);
                                AssertReleaseRC(rc);
                            }
                        } break;

                        default:
                        {
                            /* This indicates that the guest request memory was corrupted. */
                            AssertReleaseMsgFailed(("hgcmCompleted: invalid parameter type %08X\n", pGuestParm->type));
                        }
                    }
                }
                break;
            }
#endif /* VBOX_WITH_64_BITS_GUESTS */
            case VMMDevReq_HGCMConnect:
            {
                VMMDevHGCMConnect *pHGCMConnectCopy = (VMMDevHGCMConnect *)(pCmd+1);

                /* save the client id in the guest request packet */
                VMMDevHGCMConnect *pHGCMConnect = (VMMDevHGCMConnect *)pHeader;
                pHGCMConnect->u32ClientID = pHGCMConnectCopy->u32ClientID;
                break;
            }

            default:
                /* make gcc happy */
                break;
            }
        }
        else
        {
            /* Return error to the guest. */
            pHeader->header.rc = rc;
        }

        /* Mark request as processed*/
        pHeader->fu32Flags |= VBOX_HGCM_REQ_DONE;

        /* Write back the request */
        PDMDevHlpPhysWrite(pVMMDevState->pDevIns, pCmd->GCPhys, pHeader, pCmd->cbSize);

        /* It it assumed that VMMDev saves state after the HGCM services. */
        vmmdevHGCMRemoveCommand (pVMMDevState, pCmd);

        /* Now, when the command was removed from the internal list, notify the guest. */
        VMMDevNotifyGuest (pVMMDevState, VMMDEV_EVENT_HGCM);

        if (pCmd->paLinPtrs)
        {
            RTMemFree (pCmd->paLinPtrs);
        }
        
        RTMemFree (pCmd);
    }
    RTMemFree(pHeader);

    return;
}

DECLCALLBACK(void) hgcmCompleted (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmd)
{
    VMMDevState *pVMMDevState = PDMIHGCMPORT_2_VMMDEVSTATE(pInterface);

    /* Not safe to execute asynchroneously; forward to EMT */
    int rc = VMR3ReqCallEx(PDMDevHlpGetVM(pVMMDevState->pDevIns), NULL, 0, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID,
                           (PFNRT)hgcmCompletedWorker, 3, pInterface, result, pCmd);
    AssertRC(rc);
}

/* @thread EMT */
int vmmdevHGCMSaveState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM)
{
    /* Save information about pending requests.
     * Only GCPtrs are of interest.
     */
    int rc = VINF_SUCCESS;

    LogFlowFunc(("\n"));

    /* Compute how many commands are pending. */
    uint32_t cCmds = 0;

    PVBOXHGCMCMD pIter = pVMMDevState->pHGCMCmdList;

    while (pIter)
    {
        LogFlowFunc (("pIter %p\n", pIter));
        cCmds++;
        pIter = pIter->pNext;
    }

    LogFlowFunc(("cCmds = %d\n", cCmds));

    /* Save number of commands. */
    rc = SSMR3PutU32(pSSM, cCmds);
    AssertRCReturn(rc, rc);

    if (cCmds > 0)
    {
        pIter = pVMMDevState->pHGCMCmdList;

        while (pIter)
        {
            PVBOXHGCMCMD pNext = pIter->pNext;

            LogFlowFunc (("Saving %VGp\n", pIter->GCPhys));
            rc = SSMR3PutGCPhys(pSSM, pIter->GCPhys);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, pIter->cbSize);
            AssertRCReturn(rc, rc);

            vmmdevHGCMRemoveCommand (pVMMDevState, pIter);

            pIter = pNext;
        }
    }

    return rc;
}

/* @thread EMT */
int vmmdevHGCMLoadState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("\n"));

    /* Read how many commands were pending. */
    uint32_t cCmds = 0;
    rc = SSMR3GetU32(pSSM, &cCmds);
    AssertRCReturn(rc, rc);

    LogFlowFunc(("cCmds = %d\n", cCmds));

    while (cCmds--)
    {
        RTGCPHYS GCPhys;
        uint32_t cbSize;

        rc = SSMR3GetGCPhys(pSSM, &GCPhys);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetU32(pSSM, &cbSize);
        AssertRCReturn(rc, rc);

        LogFlowFunc (("Restoring %VGp size %x bytes\n", GCPhys, cbSize));

        PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD));
        AssertReturn(pCmd, VERR_NO_MEMORY);

        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPhys, cbSize, VBOXHGCMCMDTYPE_LOADSTATE);
    }

    return rc;
}

/* @thread EMT */
int vmmdevHGCMLoadStateDone(VMMDevState *pVMMDevState, PSSMHANDLE pSSM)
{
    LogFlowFunc(("\n"));

    /* Reissue pending requests. */
    PPDMDEVINS pDevIns = pVMMDevState->pDevIns;

    int rc = vmmdevHGCMCmdListLock (pVMMDevState);

    if (VBOX_SUCCESS (rc))
    {
        PVBOXHGCMCMD pIter = pVMMDevState->pHGCMCmdList;

        while (pIter)
        {
            LogFlowFunc (("pIter %p\n", pIter));

            PVBOXHGCMCMD pNext = pIter->pNext;

            VMMDevRequestHeader *requestHeader = (VMMDevRequestHeader *)RTMemAllocZ (pIter->cbSize);
            Assert(requestHeader);
            if (requestHeader == NULL)
                return VERR_NO_MEMORY;

            PDMDevHlpPhysRead(pDevIns, (RTGCPHYS)pIter->GCPhys, requestHeader, pIter->cbSize);

            /* the structure size must be greater or equal to the header size */
            if (requestHeader->size < sizeof(VMMDevRequestHeader))
            {
                Log(("VMMDev request header size too small! size = %d\n", requestHeader->size));
            }
            else
            {
                /* check the version of the header structure */
                if (requestHeader->version != VMMDEV_REQUEST_HEADER_VERSION)
                {
                    Log(("VMMDev: guest header version (0x%08X) differs from ours (0x%08X)\n", requestHeader->version, VMMDEV_REQUEST_HEADER_VERSION));
                }
                else
                {
                    Log(("VMMDev request issued: %d\n", requestHeader->requestType));

                    switch (requestHeader->requestType)
                    {
                        case VMMDevReq_HGCMConnect:
                        {
                            if (requestHeader->size < sizeof(VMMDevHGCMConnect))
                            {
                                AssertMsgFailed(("VMMDevReq_HGCMConnect structure has invalid size!\n"));
                                requestHeader->rc = VERR_INVALID_PARAMETER;
                            }
                            else if (!pVMMDevState->pHGCMDrv)
                            {
                                Log(("VMMDevReq_HGCMConnect HGCM Connector is NULL!\n"));
                                requestHeader->rc = VERR_NOT_SUPPORTED;
                            }
                            else
                            {
                                VMMDevHGCMConnect *pHGCMConnect = (VMMDevHGCMConnect *)requestHeader;

                                Log(("VMMDevReq_HGCMConnect\n"));

                                requestHeader->rc = vmmdevHGCMConnect (pVMMDevState, pHGCMConnect, pIter->GCPhys);
                            }
                            break;
                        }

                        case VMMDevReq_HGCMDisconnect:
                        {
                            if (requestHeader->size < sizeof(VMMDevHGCMDisconnect))
                            {
                                AssertMsgFailed(("VMMDevReq_HGCMDisconnect structure has invalid size!\n"));
                                requestHeader->rc = VERR_INVALID_PARAMETER;
                            }
                            else if (!pVMMDevState->pHGCMDrv)
                            {
                                Log(("VMMDevReq_HGCMDisconnect HGCM Connector is NULL!\n"));
                                requestHeader->rc = VERR_NOT_SUPPORTED;
                            }
                            else
                            {
                                VMMDevHGCMDisconnect *pHGCMDisconnect = (VMMDevHGCMDisconnect *)requestHeader;

                                Log(("VMMDevReq_VMMDevHGCMDisconnect\n"));
                                requestHeader->rc = vmmdevHGCMDisconnect (pVMMDevState, pHGCMDisconnect, pIter->GCPhys);
                            }
                            break;
                        }

#ifdef VBOX_WITH_64_BITS_GUESTS
                        case VMMDevReq_HGCMCall64:
                        case VMMDevReq_HGCMCall32:
#else
                        case VMMDevReq_HGCMCall:
#endif /* VBOX_WITH_64_BITS_GUESTS */
                        {
                            if (requestHeader->size < sizeof(VMMDevHGCMCall))
                            {
                                AssertMsgFailed(("VMMDevReq_HGCMCall structure has invalid size!\n"));
                                requestHeader->rc = VERR_INVALID_PARAMETER;
                            }
                            else if (!pVMMDevState->pHGCMDrv)
                            {
                                Log(("VMMDevReq_HGCMCall HGCM Connector is NULL!\n"));
                                requestHeader->rc = VERR_NOT_SUPPORTED;
                            }
                            else
                            {
                                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)requestHeader;

                                Log(("VMMDevReq_HGCMCall: sizeof (VMMDevHGCMRequest) = %04X\n", sizeof (VMMDevHGCMCall)));

                                Log(("%.*Vhxd\n", requestHeader->size, requestHeader));

#ifdef VBOX_WITH_64_BITS_GUESTS
                                bool f64Bits = (requestHeader->requestType == VMMDevReq_HGCMCall64);
#else
                                bool f64Bits = false;
#endif /* VBOX_WITH_64_BITS_GUESTS */
                                requestHeader->rc = vmmdevHGCMCall (pVMMDevState, pHGCMCall, pIter->GCPhys, f64Bits);
                            }
                            break;
                        }
                        default:
                            AssertMsgFailed(("Unknown request type %x during LoadState\n", requestHeader->requestType));
                            LogRel(("VMMDEV: Ignoring unknown request type %x during LoadState\n", requestHeader->requestType));
                    }
                }
            }

            /* Write back the request */
            PDMDevHlpPhysWrite(pDevIns, pIter->GCPhys, requestHeader, pIter->cbSize);
            RTMemFree(requestHeader);

            vmmdevHGCMRemoveCommand (pVMMDevState, pIter);
            RTMemFree(pIter);
            pIter = pNext;
        }

        vmmdevHGCMCmdListUnlock (pVMMDevState);
    }
    
    return rc;
}
