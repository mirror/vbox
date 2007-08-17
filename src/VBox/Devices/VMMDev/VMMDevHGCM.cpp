/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager device
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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
    
    /* Pointer to array of the HC addresses for these pages. 
     * It is assumed that the HC address of the locked resident
     * guest physical page does not change.
     */
    RTHCPTR *paPages;
    
} VBOXHGCMLINPTR;

struct VBOXHGCMCMD
{
    /* Active commands, list is protected by critsectHGCMCmdList. */
    struct VBOXHGCMCMD *pNext;
    struct VBOXHGCMCMD *pPrev;

    /* The type of the command. */
    VBOXHGCMCMDTYPE enmCmdType;

    /* GC pointer of the guest request. */
    RTGCPHYS GCPtr;

    /* HC pointer to guest request. */
    VMMDevHGCMRequestHeader *pHeader;

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

static int vmmdevHGCMAddCommand (VMMDevState *pVMMDevState, PVBOXHGCMCMD pCmd, RTGCPHYS GCPtr, VBOXHGCMCMDTYPE enmCmdType)
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
        pCmd->GCPtr = GCPtr;
        
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
                                 RTHCPTR **ppPages)
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
        RTHCPTR HCPtr;
        
        rc = PDMDevHlpPhysGCPtr2HCPtr(pDevIns, GCPtr, &HCPtr);
        
        Log(("vmmdevHGCMSaveLinPtr: Page %d: %VGv -> %p. %Vrc\n", iPage, GCPtr, HCPtr, rc));
    
        if (VBOX_FAILURE (rc))
        {
            break;
        }

        /* store */
        pLinPtr->paPages[iPage++] = HCPtr;

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
    
    uint8_t *pu8Dst = (uint8_t *)pLinPtr->paPages[0] + pLinPtr->cbOffsetFirstPage;
    uint8_t *pu8Src = (uint8_t *)pvHost;
    
    Log(("vmmdevHGCMWriteLinPtr: parm %d: size %d, cPages = %d\n", iParm, u32Size, pLinPtr->cPages));
    
    uint32_t iPage = 0;
    
    while (iPage < pLinPtr->cPages)
    {
        /* copy */
        size_t cbWrite = iPage == 0?
                             PAGE_SIZE - pLinPtr->cbOffsetFirstPage:
                             PAGE_SIZE;
                             
        Log(("vmmdevHGCMWriteLinPtr: page %d: dst %p, src %p, cbWrite %d\n", iPage, pu8Dst, pu8Src, cbWrite));
        
        iPage++;
        
        if (cbWrite >= u32Size)
        {
            memcpy (pu8Dst, pu8Src, u32Size);
            u32Size = 0;
            break;
        }
        
        memcpy (pu8Dst, pu8Src, cbWrite);

        /* next */
        u32Size    -= cbWrite;
        pu8Src     += cbWrite;
        
        pu8Dst = (uint8_t *)pLinPtr->paPages[iPage];
    }
    
    AssertRelease (iPage == pLinPtr->cPages);
    Assert(u32Size == 0);
    
    return rc;
}

int vmmdevHGCMConnect (VMMDevState *pVMMDevState, VMMDevHGCMConnect *pHGCMConnect, RTGCPHYS GCPtr)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD));

    if (pCmd)
    {
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPtr, VBOXHGCMCMDTYPE_CONNECT);

        pCmd->pHeader = &pHGCMConnect->header;
        pCmd->paHostParms = NULL;
        pCmd->cLinPtrs = 0;
        pCmd->paLinPtrs = NULL;

        /* Only allow the guest to use existing services! */
        Assert(pHGCMConnect->loc.type == VMMDevHGCMLoc_LocalHost_Existing);
        pHGCMConnect->loc.type = VMMDevHGCMLoc_LocalHost_Existing;

        rc = pVMMDevState->pHGCMDrv->pfnConnect (pVMMDevState->pHGCMDrv, pCmd, &pHGCMConnect->loc, &pHGCMConnect->u32ClientID);
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

int vmmdevHGCMDisconnect (VMMDevState *pVMMDevState, VMMDevHGCMDisconnect *pHGCMDisconnect, RTGCPHYS GCPtr)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD));

    if (pCmd)
    {
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPtr, VBOXHGCMCMDTYPE_DISCONNECT);

        pCmd->pHeader = &pHGCMDisconnect->header;
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


int vmmdevHGCMCall (VMMDevState *pVMMDevState, VMMDevHGCMCall *pHGCMCall, RTGCPHYS GCPtr)
{
    int rc = VINF_SUCCESS;

    Log(("vmmdevHGCMCall: client id = %d, function = %d\n", pHGCMCall->u32ClientID, pHGCMCall->u32Function));

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

    HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);

    /* Look for pointer parameters, which require a host buffer. */
    uint32_t i;

    uint32_t cLinPtrs = 0;
    uint32_t cLinPtrPages  = 0;

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
    
    pCmd->pHeader     = &pHGCMCall->header;
    pCmd->paHostParms = NULL;
    pCmd->cLinPtrs    = cLinPtrs;
    
    if (cLinPtrs > 0)
    {
        pCmd->paLinPtrs = (VBOXHGCMLINPTR *)RTMemAlloc (  sizeof (VBOXHGCMLINPTR) * cLinPtrs
                                                          + sizeof (RTHCPTR) * cLinPtrPages);
    
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

        pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);

        uint32_t iLinPtr = 0;
        RTHCPTR *pPages  = (RTHCPTR *)((uint8_t *)pCmd->paLinPtrs + sizeof (VBOXHGCMLINPTR) *cLinPtrs);

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
                 } break;

                 case VMMDevHGCMParmType_64bit:
                 {
                     uint64_t u64 = pGuestParm->u.value64;

                     pHostParm->type = VBOX_HGCM_SVC_PARM_64BIT;
                     pHostParm->u.uint64 = u64;

                     Log(("vmmdevHGCMCall: uint64 guest parameter %llu\n", u64));
                 } break;

                 case VMMDevHGCMParmType_PhysAddr:
                 {
                     uint32_t size = pGuestParm->u.Pointer.size;
                     RTGCPHYS physAddr = pGuestParm->u.Pointer.u.physAddr;

                     pHostParm->type = VBOX_HGCM_SVC_PARM_PTR;
                     pHostParm->u.pointer.size = size;

                     rc = PDMDevHlpPhys2HCVirt (pVMMDevState->pDevIns, physAddr, size, &pHostParm->u.pointer.addr);

                     Log(("vmmdevHGCMCall: PhysAddr guest parameter %VGp\n", physAddr));
                 } break;

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
                 } break;

                /* just to shut up gcc */
                default:
                    break;
            }
        }
    }

    if (VBOX_SUCCESS (rc))
    {
        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPtr, VBOXHGCMCMDTYPE_CALL);

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

static int vmmdevHGCMCmdVerify (PVBOXHGCMCMD pCmd)
{
    VMMDevHGCMRequestHeader *pHeader = pCmd->pHeader;

    switch (pCmd->enmCmdType)
    {
        case VBOXHGCMCMDTYPE_CONNECT:
            if (pHeader->header.requestType == VMMDevReq_HGCMConnect) return VINF_SUCCESS;
            break;

        case VBOXHGCMCMDTYPE_DISCONNECT:
            if (pHeader->header.requestType == VMMDevReq_HGCMDisconnect) return VINF_SUCCESS;
            break;

        case VBOXHGCMCMDTYPE_CALL:
            if (pHeader->header.requestType == VMMDevReq_HGCMCall) return VINF_SUCCESS;
            break;

        default:
            AssertFailed ();
    }

    LogRel(("VMMDEV: Invalid HGCM command: pCmd->enmCmdType = 0x%08X, pHeader->header.requestType = 0x%08X\n",
          pCmd->enmCmdType, pHeader->header.requestType));
    return VERR_INVALID_PARAMETER;
}

#define PDMIHGCMPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState *) ((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, HGCMPort)) )

DECLCALLBACK(void) hgcmCompleted (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmd)
{
    int rc = VINF_SUCCESS;

    VMMDevHGCMRequestHeader *pHeader = pCmd->pHeader;

    VMMDevState *pVMMDevState = PDMIHGCMPORT_2_VMMDEVSTATE(pInterface);

    if (result != VINF_HGCM_SAVE_STATE)
    {
        /* Setup return codes. */
        pHeader->result = result;
        
        /* Verify the request type. */
        rc = vmmdevHGCMCmdVerify (pCmd);

        if (VBOX_SUCCESS (rc))
        {
            /* Update parameters and data buffers. */

            if (pHeader->header.requestType == VMMDevReq_HGCMCall)
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pHeader;

                uint32_t cParms = pHGCMCall->cParms;

                HGCMFunctionParameter *pGuestParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);

                VBOXHGCMSVCPARM *pHostParm = pCmd->paHostParms;

                uint32_t i;
                uint32_t iLinPtr = 0;

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
            }
        }
        else
        {
            /* Return error to the guest. */
            pHeader->header.rc = rc;
        }

        /* Mark request as processed*/
        pHeader->fu32Flags |= VBOX_HGCM_REQ_DONE;

        VMMDevNotifyGuest (pVMMDevState, VMMDEV_EVENT_HGCM);

        /* It it assumed that VMMDev saves state after the HGCM services. */
        vmmdevHGCMRemoveCommand (pVMMDevState, pCmd);

        if (pCmd->paLinPtrs)
        {
            RTMemFree (pCmd->paLinPtrs);
        }
        
        RTMemFree (pCmd);
    }

    return;
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

            LogFlowFunc (("Saving %VGp\n", pIter->GCPtr));
            rc = SSMR3PutGCPtr(pSSM, pIter->GCPtr);
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
        RTGCPHYS GCPtr;
        rc = SSMR3GetGCPtr(pSSM, &GCPtr);
        AssertRCReturn(rc, rc);

        LogFlowFunc (("Restoring %VGp\n", GCPtr));

        PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAllocZ (sizeof (struct VBOXHGCMCMD));
        AssertReturn(pCmd, VERR_NO_MEMORY);

        vmmdevHGCMAddCommand (pVMMDevState, pCmd, GCPtr, VBOXHGCMCMDTYPE_LOADSTATE);
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

            VMMDevRequestHeader *requestHeader = NULL;
            rc = PDMDevHlpPhys2HCVirt(pDevIns, pIter->GCPtr, 0, (PRTHCPTR)&requestHeader);

            if (VBOX_FAILURE(rc) || !requestHeader)
            {
                AssertMsgFailed(("VMMDev::LoadStateDone: could not convert guest physical address to host virtual!!! rc = %Vrc\n", rc));
            }
            else
            {
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

                                    requestHeader->rc = vmmdevHGCMConnect (pVMMDevState, pHGCMConnect, pIter->GCPtr);
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
                                    requestHeader->rc = vmmdevHGCMDisconnect (pVMMDevState, pHGCMDisconnect, pIter->GCPtr);
                                }
                                break;
                            }

                            case VMMDevReq_HGCMCall:
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

                                    requestHeader->rc = vmmdevHGCMCall (pVMMDevState, pHGCMCall, pIter->GCPtr);
                                }
                                break;
                            }
                            default:
                               AssertReleaseFailed();
                        }
                    }
                }
            }

            vmmdevHGCMRemoveCommand (pVMMDevState, pIter);
            pIter = pNext;
        }

        vmmdevHGCMCmdListUnlock (pVMMDevState);
    }
    
    return rc;
}
