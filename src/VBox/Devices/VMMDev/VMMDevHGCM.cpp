/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager device
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/log.h>

#include "VMMDevHGCM.h"

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
    /* Pointer to guest request. */
    VMMDevHGCMRequestHeader *pHeader;

    /* Pointer to converted host parameters in case of a Call request. */
    VBOXHGCMSVCPARM *paHostParms;
    
    /* Linear pointer parameters information. */
    int cLinPtrs;
    
    /* Pointer to descriptions of linear pointers.  */
    VBOXHGCMLINPTR *paLinPtrs;
};

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
        
        rc = pDevIns->pDevHlp->pfnPhysGCPtr2HCPtr (pDevIns, GCPtr, &HCPtr);
//        rc = PGMPhysGCPtr2HCPtr (pVM, GCPtr, &HCPtr);
        
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

int vmmdevHGCMConnect (VMMDevState *pVMMDevState, VMMDevHGCMConnect *pHGCMConnect)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAlloc (sizeof (struct VBOXHGCMCMD));

    if (pCmd)
    {
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

int vmmdevHGCMDisconnect (VMMDevState *pVMMDevState, VMMDevHGCMDisconnect *pHGCMDisconnect)
{
    int rc = VINF_SUCCESS;

    PVBOXHGCMCMD pCmd = (PVBOXHGCMCMD)RTMemAlloc (sizeof (struct VBOXHGCMCMD));

    if (pCmd)
    {
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


int vmmdevHGCMCall (VMMDevState *pVMMDevState, VMMDevHGCMCall *pHGCMCall)
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
#if 0
            case VMMDevHGCMParmType_Virt16Addr:
            case VMMDevHGCMParmType_VirtAddr:
#endif
            {
                cbCmdSize += pGuestParm->u.Pointer.size;
                
                if (pGuestParm->type != VMMDevHGCMParmType_LinAddr_In)
                {
                    cLinPtrs++;
                    cLinPtrPages += (pGuestParm->u.Pointer.size + PAGE_SIZE - 1) / PAGE_SIZE;
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

                     rc = pVMMDevState->pDevIns->pDevHlp->pfnPhys2HCVirt (pVMMDevState->pDevIns, physAddr,
                                                                          size, &pHostParm->u.pointer.addr);

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
                         {
                             rc = pVMMDevState->pDevIns->pDevHlp->pfnPhysReadGCVirt (pVMMDevState->pDevIns, pcBuf,
                                                                                     linearAddr, size);
                         }
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

#define PDMIHGCMPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState *) ((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, HGCMPort)) )

DECLCALLBACK(void) hgcmCompleted (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmd)
{
    int rc = VINF_SUCCESS;

    VMMDevHGCMRequestHeader *pHeader = pCmd->pHeader;

    VMMDevState *pVMMDevState = PDMIHGCMPORT_2_VMMDEVSTATE(pInterface);

    /* Setup return codes. */
    pHeader->result = result;

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

//                        RTGCPTR linearAddr = pGuestParm->u.Pointer.u.linearAddr;
//
//                        rc = pVMMDevState->pDevIns->pDevHlp->pfnPhysWriteGCVirt (pVMMDevState->pDevIns,
//                                                                                 linearAddr,
//                                                                                 pHostParm->u.pointer.addr,
//                                                                                 size);
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

    /* Mark request as processed*/
    pHeader->fu32Flags |= VBOX_HGCM_REQ_DONE;

    VMMDevNotifyGuest (pVMMDevState, VMMDEV_EVENT_HGCM);

    if (pCmd->paLinPtrs)
    {
        RTMemFree (pCmd->paLinPtrs);
    }
        
    RTMemFree (pCmd);

    return;
}
