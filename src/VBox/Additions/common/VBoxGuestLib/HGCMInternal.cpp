/* $Revision$ */
/** @file
 * VBoxGuestLib - Host-Guest Communication Manager internal functions, implemented by VBoxGuest
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

/* Entire file is ifdef'ed with VBGL_VBOXGUEST */
#ifdef VBGL_VBOXGUEST

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBGLInternal.h"
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/string.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The max parameter buffer size for a user request. */
#define VBGLR0_MAX_HGCM_USER_PARM       _1M
/** The max parameter buffer size for a kernel request. */
#define VBGLR0_MAX_HGCM_KERNEL_PARM     (16*_1M)
#ifdef RT_OS_LINUX
/** Linux needs to use bounce buffers since RTR0MemObjLockUser has unwanted
 *  side effects. */
# define USE_BOUNCH_BUFFERS
#endif



/* These functions can be only used by VBoxGuest. */

DECLVBGL(int) VbglR0HGCMInternalConnect (VBoxGuestHGCMConnectInfo *pConnectInfo,
                                         VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData,
                                         uint32_t u32AsyncData)
{
    VMMDevHGCMConnect *pHGCMConnect;
    int rc;

    if (!pConnectInfo || !pAsyncCallback)
        return VERR_INVALID_PARAMETER;

    pHGCMConnect = NULL;

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMConnect, sizeof (VMMDevHGCMConnect), VMMDevReq_HGCMConnect);

    if (RT_SUCCESS(rc))
    {
        /* Initialize request memory */
        pHGCMConnect->header.fu32Flags = 0;

        memcpy (&pHGCMConnect->loc, &pConnectInfo->Loc, sizeof (HGCMServiceLocation));
        pHGCMConnect->u32ClientID = 0;

        /* Issue request */
        rc = VbglGRPerform (&pHGCMConnect->header.header);

        if (RT_SUCCESS(rc))
        {
            /* Check if host decides to process the request asynchronously. */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Wait for request completion interrupt notification from host */
                pAsyncCallback (&pHGCMConnect->header, pvAsyncData, u32AsyncData);
            }

            pConnectInfo->result = pHGCMConnect->header.result;

            if (RT_SUCCESS (pConnectInfo->result))
                pConnectInfo->u32ClientID = pHGCMConnect->u32ClientID;
        }

        VbglGRFree (&pHGCMConnect->header.header);
    }

    return rc;
}


DECLR0VBGL(int) VbglR0HGCMInternalDisconnect (VBoxGuestHGCMDisconnectInfo *pDisconnectInfo,
                                              VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    VMMDevHGCMDisconnect *pHGCMDisconnect;
    int rc;

    if (!pDisconnectInfo || !pAsyncCallback)
        return VERR_INVALID_PARAMETER;

    pHGCMDisconnect = NULL;

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMDisconnect, sizeof (VMMDevHGCMDisconnect), VMMDevReq_HGCMDisconnect);

    if (RT_SUCCESS(rc))
    {
        /* Initialize request memory */
        pHGCMDisconnect->header.fu32Flags = 0;

        pHGCMDisconnect->u32ClientID = pDisconnectInfo->u32ClientID;

        /* Issue request */
        rc = VbglGRPerform (&pHGCMDisconnect->header.header);

        if (RT_SUCCESS(rc))
        {
            /* Check if host decides to process the request asynchronously. */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Wait for request completion interrupt notification from host */
                pAsyncCallback (&pHGCMDisconnect->header, pvAsyncData, u32AsyncData);
            }

            pDisconnectInfo->result = pHGCMDisconnect->header.result;
        }

        VbglGRFree (&pHGCMDisconnect->header.header);
    }

    return rc;
}

#if 0 /* new code using page list and whatnot. */

/**
 * Lock info structure used by VbglR0HGCMInternalCall and its helpers.
 */
struct VbglR0ParmInfo
{
    uint32_t cLockBufs;
    struct
    {
        uint32_t    iParm;
        RTR0MEMOBJ  hObj;
    } aLockBufs[10];
};

/**
 * Preprocesses the HGCM call, validating and locking/buffering parameters.
 *
 * @returns VBox status code.
 *
 * @param   pCallInfo       The call info.
 * @param   cbCallInfo      The size of the call info structure.
 * @param   fIsUser         Is it a user request or kernel request.
 * @param   pcbExtra        Where to return the extra request space needed for
 *                          physical page lists.
 */
static int vbglR0HGCMInternalPreprocessCall(VBoxGuestHGCMCallInfo const *pCallInfo, uint32_t cbCallInfo,
                                            bool fIsUser, struct VbglR0ParmInfo *pParmInfo,  size_t *pcbExtra)
{
    HGCMFunctionParameter const *pSrcParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);
    uint32_t    cParms = pCallInfo->cParms;
    uint32_t    iParm;

    /*
     * Lock down the any linear buffers so we can get their addresses
     * and figure out how much extra storage we need for page lists.
     */
    *pcbExtra = 0;
    pParmInfo->cLockBufs = 0;
    for (iParm = 0; iParm < cParms; iParm++, pSrcParm++)
    {
        switch (pSrcParm->type)
        {
            case VMMDevHGCMParmType_32bit:
            case VMMDevHGCMParmType_64bit:
                break;

            case VMMDevHGCMParmType_PageList:
                if (fIsUser)
                    return VERR_INVALID_PARAMETER;
                if (pSrcParm->u.PageList.size)
                {
                    HGCMPageListInfo *pPgLst;

                    if (pSrcParm->u.Pointer.size > VBGLR0_MAX_HGCM_KERNEL_PARM)
                        return VERR_OUT_OF_RANGE;
                    AssertReturn(   pSrcParm->u.PageList.offset >= pCallInfo->cParms * sizeof(HGCMFunctionParameter)
                                 && pSrcParm->u.PageList.offset < cbCallInfo - sizeof(HGCMPageListInfo), VERR_INVALID_PARAMETER);
                    pPgLst = (HGCMPageListInfo *)((uint8_t *)pCallInfo + pSrcParm->u.PageList.offset);
                    AssertReturn(RT_OFFSETOF(HGCMPageListInfo, aPages[pPgLst->cPages]) + pSrcParm->u.PageList.offset <= cbCallInfo, VERR_INVALID_PARAMETER);
                    AssertReturn(pPgLst->offFirstPage < PAGE_SIZE, VERR_INVALID_PARAMETER);
                    AssertReturn(pPgLst->flags > VBOX_HGCM_F_PARM_DIRECTION_NONE && pPgLst->flags <= VBOX_HGCM_F_PARM_DIRECTION_BOTH, VERR_INVALID_PARAMETER);

                    *pcbExtra += RT_OFFSETOF(HGCMPageListInfo, aPages[pPgLst->cPages]);
                }
                break;

            case VMMDevHGCMParmType_LinAddr_Locked_In:
            case VMMDevHGCMParmType_LinAddr_Locked_Out:
            case VMMDevHGCMParmType_LinAddr_Locked:
                if (fIsUser)
                    return VERR_INVALID_PARAMETER;
                if (!VBGLR0_CAN_USE_PHYS_PAGE_LIST())
                {
                    if (pSrcParm->u.Pointer.size > VBGLR0_MAX_HGCM_KERNEL_PARM)
                        return VERR_OUT_OF_RANGE;
                    break;
                }
                /* fall thru */

            case VMMDevHGCMParmType_LinAddr_In:
            case VMMDevHGCMParmType_LinAddr_Out:
            case VMMDevHGCMParmType_LinAddr:
                if (pSrcParm->u.Pointer.size > (fIsUser ? VBGLR0_MAX_HGCM_USER_PARM : VBGLR0_MAX_HGCM_KERNEL_PARM))
                    return VERR_OUT_OF_RANGE;
                if (pSrcParm->u.Pointer.size != 0)
                {
                    RTR0MEMOBJ hObj;
                    int rc;

                    AssertReturn(pParmInfo->cLockBufs < RT_ELEMENTS(pParmInfo->aLockBufs), VERR_INVALID_PARAMETER);
                    if (!fIsUser)
                        rc = RTR0MemObjLockKernel(&hObj,
                                                  (void *)pSrcParm->u.Pointer.u.linearAddr,
                                                  pSrcParm->u.Pointer.size);
                    else
                    {
#ifdef USE_BOUNCH_BUFFERS
                        rc = RTR0MemObjAllocPage(&hObj,
                                                 pSrcParm->u.Pointer.size,
                                                 false /*fExecutable*/);
                        if (    RT_SUCCESS(rc)
                            &&  pSrcParm->type != VMMDevHGCMParmType_LinAddr_Out
                            &&  pSrcParm->type != VMMDevHGCMParmType_LinAddr_Locked_Out)
                        {
                            rc = RTR0MemUserCopyFrom(RTR0MemObjAddress(hObj),
                                                     pSrcParm->u.Pointer.u.linearAddr,
                                                     pSrcParm->u.Pointer.size + 2);
                            if (RT_FAILURE(rc))
                                RTR0MemObjFree(hObj, false /*fFreeMappings*/);
                        }
#else
                        rc = RTR0MemObjLockUser(&hObj,
                                                (RTR3PTR)pSrcParm->u.Pointer.u.linearAddr,
                                                pSrcParm->u.Pointer.size,
                                                NIL_RTR0PROCESS);
#endif
                    }
                    if (RT_FAILURE(rc))
                        return rc;

                    pParmInfo->aLockBufs[pParmInfo->cLockBufs].iParm = iParm;
                    pParmInfo->aLockBufs[pParmInfo->cLockBufs].hObj  = hObj;
                    pParmInfo->cLockBufs++;

                    if (VBGLR0_CAN_USE_PHYS_PAGE_LIST())
                    {
                        size_t cPages = RTR0MemObjSize(hObj);
                        *pcbExtra += RT_OFFSETOF(HGCMPageListInfo, aPages[cPages]);
                    }
                }
                break;

            default:
                return VERR_INVALID_PARAMETER;
        }
    }

    return VINF_SUCCESS;
}

/**
 * Translates locked linear address to the normal type.
 * The locked types are only for the guest side and not handled by the host.
 *
 * @returns normal linear address type.
 * @param   enmType     The type.
 */
static HGCMFunctionParameterType vbglR0HGCMInternalConvertLinAddrType(HGCMFunctionParameterType enmType)
{
    switch (enmType)
    {
        case VMMDevHGCMParmType_LinAddr_Locked_In:
            return VMMDevHGCMParmType_LinAddr_In;
        case VMMDevHGCMParmType_LinAddr_Locked_Out:
            return VMMDevHGCMParmType_LinAddr_Out;
        case VMMDevHGCMParmType_LinAddr_Locked:
            return VMMDevHGCMParmType_LinAddr;
        default:
            return enmType;
    }
}

/**
 * Translates linear address types to page list direction flags.
 *
 * @returns page list flags.
 * @param   enmType     The type.
 */
static uint32_t vbglR0HGCMInternalLinAddrTypeToPageListFlags(HGCMFunctionParameterType enmType)
{
    switch (enmType)
    {
        case VMMDevHGCMParmType_LinAddr_In:
        case VMMDevHGCMParmType_LinAddr_Locked_In:
            return VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;

        case VMMDevHGCMParmType_LinAddr_Out:
        case VMMDevHGCMParmType_LinAddr_Locked_Out:
            return VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;

        default: AssertFailed();
        case VMMDevHGCMParmType_LinAddr:
        case VMMDevHGCMParmType_LinAddr_Locked:
            return VBOX_HGCM_F_PARM_DIRECTION_BOTH;
    }
}


/**
 * Initializes the call request that we're sending to the host.
 *
 * @returns VBox status code.
 *
 * @param   pCallInfo       The call info.
 * @param   cbCallInfo      The size of the call info structure.
 * @param   fIsUser         Is it a user request or kernel request.
 * @param   pcbExtra        Where to return the extra request space needed for
 *                          physical page lists.
 */
static void vbglR0HGCMInternalInitCall(VMMDevHGCMCall *pHGCMCall, VBoxGuestHGCMCallInfo const *pCallInfo, uint32_t cbCallInfo,
                                       bool fIsUser, struct VbglR0ParmInfo *pParmInfo)
{
    HGCMFunctionParameter const *pSrcParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);
    HGCMFunctionParameter       *pDstParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
    uint32_t    cParms   = pCallInfo->cParms;
    uint32_t    offExtra = (uintptr_t)(pDstParm + cParms) - (uintptr_t)pHGCMCall;
    uint32_t    iLockBuf = 0;
    uint32_t    iParm;


    /*
     * The call request headers.
     */
    pHGCMCall->header.fu32Flags = 0;
    pHGCMCall->header.result    = VINF_SUCCESS;

    pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
    pHGCMCall->u32Function = pCallInfo->u32Function;
    pHGCMCall->cParms      = cParms;

    /*
     * The parameters.
     */
    for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pSrcParm++, pDstParm++)
    {
        switch (pSrcParm->type)
        {
            case VMMDevHGCMParmType_32bit:
            case VMMDevHGCMParmType_64bit:
                *pDstParm = *pSrcParm;
                break;

            case VMMDevHGCMParmType_PageList:
                pDstParm->type = VMMDevHGCMParmType_PageList;
                pDstParm->u.PageList.size = pSrcParm->u.PageList.size;
                if (pSrcParm->u.PageList.size)
                {
                    HGCMPageListInfo const *pSrcPgLst = (HGCMPageListInfo *)((uint8_t *)pCallInfo + pSrcParm->u.PageList.offset);
                    HGCMPageListInfo       *pDstPgLst = (HGCMPageListInfo *)((uint8_t *)pHGCMCall + offExtra);
                    uint32_t const          cPages    = pSrcPgLst->cPages;
                    uint32_t                iPage;

                    pDstParm->u.PageList.offset = offExtra;
                    pDstPgLst->flags            = pSrcPgLst->flags;
                    pDstPgLst->offFirstPage     = pSrcPgLst->offFirstPage;
                    pDstPgLst->cPages           = cPages;
                    for (iPage = 0; iPage < cPages; iPage++)
                        pDstPgLst->aPages[iPage] = pSrcPgLst->aPages[iPage];

                    offExtra += RT_OFFSETOF(HGCMPageListInfo, aPages[cPages]);
                }
                else
                    pDstParm->u.PageList.offset = 0;
                break;

            case VMMDevHGCMParmType_LinAddr_Locked_In:
            case VMMDevHGCMParmType_LinAddr_Locked_Out:
            case VMMDevHGCMParmType_LinAddr_Locked:
                if (!VBGLR0_CAN_USE_PHYS_PAGE_LIST())
                {
                    *pDstParm = *pSrcParm;
                    break;
                }
                /* fall thru */

            case VMMDevHGCMParmType_LinAddr_In:
            case VMMDevHGCMParmType_LinAddr_Out:
            case VMMDevHGCMParmType_LinAddr:
                if (pSrcParm->u.Pointer.size != 0)
                {
                    RTR0MEMOBJ hObj = pParmInfo->aLockBufs[iLockBuf].hObj;
                    Assert(iParm == pParmInfo->aLockBufs[iLockBuf].iParm);

                    if (VBGLR0_CAN_USE_PHYS_PAGE_LIST())
                    {
                        HGCMPageListInfo   *pDstPgLst = (HGCMPageListInfo *)((uint8_t *)pHGCMCall + offExtra);
                        size_t const        cPages    = RTR0MemObjSize(hObj) >> PAGE_SHIFT;
                        size_t              iPage;

                        pDstParm->type = VMMDevHGCMParmType_PageList;
                        pDstParm->u.PageList.size   = pSrcParm->u.Pointer.size;
                        pDstParm->u.PageList.offset = offExtra;
                        pDstPgLst->flags            = vbglR0HGCMInternalLinAddrTypeToPageListFlags(pSrcParm->type);
#ifdef USE_BOUNCH_BUFFERS
                        if (fIsUser)
                            pDstPgLst->offFirstPage = 0;
                        else
#endif
                            pDstPgLst->offFirstPage = pSrcParm->u.Pointer.u.linearAddr & PAGE_OFFSET_MASK;
                        pDstPgLst->cPages           = cPages; Assert(pDstPgLst->cPages == cPages);
                        for (iPage = 0; iPage < cPages; iPage++)
                        {
                            pDstPgLst->aPages[iPage] = RTR0MemObjGetPagePhysAddr(hObj, iPage);
                            Assert(pDstPgLst->aPages[iPage] != NIL_RTHCPHYS);
                        }

                        offExtra += RT_OFFSETOF(HGCMPageListInfo, aPages[cPages]);
                    }
                    else
                    {
                        pDstParm->type = vbglR0HGCMInternalConvertLinAddrType(pSrcParm->type);
                        pDstParm->u.Pointer.size = pSrcParm->u.Pointer.size;
#ifdef USE_BOUNCH_BUFFERS
                        if (fIsUser)
                            pDstParm->u.Pointer.u.linearAddr = (uintptr_t)RTR0MemObjAddress(hObj);
                        else
#endif
                            pDstParm->u.Pointer.u.linearAddr = pSrcParm->u.Pointer.u.linearAddr;
                    }
                    iLockBuf++;
                }
                else
                {
                    pDstParm->type = vbglR0HGCMInternalConvertLinAddrType(pSrcParm->type);
                    pDstParm->u.Pointer.size = 0;
                    pDstParm->u.Pointer.u.linearAddr = 0;
                }
                break;

            default:
                AssertFailed();
                pDstParm->type = VMMDevHGCMParmType_Invalid;
                break;
        }
    }
}


/**
 * Performs the call and completion wait.
 *
 * @returns VBox status code of this operation, not necessarily the call.
 *
 * @param   pHGCMCall           The HGCM call info.
 * @param   pfnAsyncCallback    The async callback that will wait for the call
 *                              to complete.
 * @param   pvAsyncData         Argument for the callback.
 * @param   u32AsyncData        Argument for the callback.
 */
static int vbglR0HGCMInternalDoCall(VMMDevHGCMCall *pHGCMCall, VBGLHGCMCALLBACK *pfnAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    int rc;

    Log(("calling VbglGRPerform\n"));
    rc = VbglGRPerform(&pHGCMCall->header.header);
    Log(("VbglGRPerform rc = %Rrc (header rc=%d)\n", rc, pHGCMCall->header.result));

    /*
     * If the call failed, but as a result of the request itself, then pretend
     * success. Upper layers will interpret the result code in the packet.
     */
    if (    RT_FAILURE(rc)
        &&  rc == pHGCMCall->header.result)
    {
        Assert(pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE);
        rc = VINF_SUCCESS;
    }

    /*
     * Check if host decides to process the request asynchronously,
     * if so, we wait for it to complete using the caller supplied callback.
     */
    if (    RT_SUCCESS(rc)
        &&  rc == VINF_HGCM_ASYNC_EXECUTE)
    {
        Log(("Processing HGCM call asynchronously\n"));
        /** @todo timeout vs. interrupted. */
        pfnAsyncCallback(&pHGCMCall->header, pvAsyncData, u32AsyncData);

        /*
         * If the request isn't completed by the time the callback returns
         * we will have to try cancel it.
         */
        if (!(pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE))
        {
            /** @todo use a new request for this! See @bugref{4052}. */
            pHGCMCall->header.fu32Flags |= VBOX_HGCM_REQ_CANCELLED;
            pHGCMCall->header.header.requestType = VMMDevReq_HGCMCancel;
            VbglGRPerform(&pHGCMCall->header.header);
            rc = VERR_INTERRUPTED;
        }
    }
    return rc;
}


/**
 * Copies the result of the call back to the caller info structure and user
 * buffers (if using bounce buffers).
 *
 * @returns rc, unless RTR0MemUserCopyTo fails.
 * @param   pCallInfo           Call info structure to update.
 * @param   pHGCMCall           HGCM call request.
 * @param   pParmInfo           Paramter locking/buffering info.
 * @param   fIsUser             Is it a user (true) or kernel request.
 * @param   rc                  The current result code. Passed along to
 *                              preserve informational status codes.
 */
static int vbglR0HGCMInternalCopyBackResult(VBoxGuestHGCMCallInfo *pCallInfo, VMMDevHGCMCall const *pHGCMCall,
                                            struct VbglR0ParmInfo *pParmInfo, bool fIsUser, int rc)
{
    HGCMFunctionParameter const *pSrcParm = VMMDEV_HGCM_CALL_PARMS(pHGCMCall);
    HGCMFunctionParameter       *pDstParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);
    uint32_t    cParms   = pCallInfo->cParms;
#ifdef USE_BOUNCH_BUFFERS
    uint32_t    iLockBuf = 0;
#endif
    uint32_t    iParm;

    /*
     * The call result.
     */
    pCallInfo->result = pHGCMCall->header.result;

    /*
     * Copy back parameters.
     */
    for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pSrcParm++, pDstParm++)
    {
        switch (pDstParm->type)
        {
            case VMMDevHGCMParmType_32bit:
            case VMMDevHGCMParmType_64bit:
                *pDstParm = *pSrcParm;
                break;

            case VMMDevHGCMParmType_PageList:
                pDstParm->u.PageList.size = pSrcParm->u.PageList.size;
                break;

            case VMMDevHGCMParmType_LinAddr_Locked_In:
            case VMMDevHGCMParmType_LinAddr_In:
#ifdef USE_BOUNCH_BUFFERS
                if (    fIsUser
                    &&  iLockBuf < pParmInfo->cLockBufs
                    &&  iParm   == pParmInfo->aLockBufs[iLockBuf].iParm)
                    iLockBuf++;
#endif
                pDstParm->u.Pointer.size = pSrcParm->u.Pointer.size;
                break;

            case VMMDevHGCMParmType_LinAddr_Locked_Out:
            case VMMDevHGCMParmType_LinAddr_Locked:
                if (!VBGLR0_CAN_USE_PHYS_PAGE_LIST())
                {
                    pDstParm->u.Pointer.size = pSrcParm->u.Pointer.size;
                    break;
                }
                /* fall thru */

            case VMMDevHGCMParmType_LinAddr_Out:
            case VMMDevHGCMParmType_LinAddr:
            {
#ifdef USE_BOUNCH_BUFFERS
                if (fIsUser)
                {
                    if (    pSrcParm->u.Pointer.size != 0
                        &&  pDstParm->u.Pointer.size != 0)
                    {
                        Assert(pParmInfo->aLockBufs[iLockBuf].iParm == iParm);
                        int rc2 = RTR0MemUserCopyTo((RTR3PTR)pDstParm->u.Pointer.u.linearAddr,
                                                    RTR0MemObjAddress(pParmInfo->aLockBufs[iLockBuf].hObj),
                                                    RT_MIN(pSrcParm->u.Pointer.size, pDstParm->u.Pointer.size));
                        if (RT_FAILURE(rc2))
                            return rc2;
                        iLockBuf++;
                    }
                    else if (   iLockBuf < pParmInfo->cLockBufs
                             && iParm   == pParmInfo->aLockBufs[iLockBuf].iParm)
                        iLockBuf++;
                }
#endif
                pDstParm->u.Pointer.size = pSrcParm->u.Pointer.size;
                break;
            }

            default:
                AssertFailed();
                rc = VERR_INTERNAL_ERROR_4;
                break;
        }
    }

#ifdef USE_BOUNCH_BUFFERS
    Assert(!fIsUser || pParmInfo->cLockBufs == iLockBuf);
#endif
    return rc;
}

DECLR0VBGL(int) VbglR0HGCMInternalCall(VBoxGuestHGCMCallInfo *pCallInfo, uint32_t cbCallInfo, uint32_t fFlags,
                                       VBGLHGCMCALLBACK *pfnAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    bool                    fIsUser = (fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER;
    struct VbglR0ParmInfo   ParmInfo;
    size_t                  cbExtra;
    int                     rc;

    /*
     * Basic validation.
     */
    AssertMsgReturn(!pCallInfo || !pfnAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS || !(fFlags & ~VBGLR0_HGCMCALL_F_MODE_MASK),
                    ("pCallInfo=%p pfnAsyncCallback=%p fFlags=%#x\n", pCallInfo, pfnAsyncCallback, fFlags),
                    VERR_INVALID_PARAMETER);
    AssertReturn(   cbCallInfo >= sizeof(VBoxGuestHGCMCallInfo)
                 || cbCallInfo >= pCallInfo->cParms * sizeof(HGCMFunctionParameter),
                 VERR_INVALID_PARAMETER);

    Log (("VbglR0HGCMInternalCall: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d, fFlags=%#x\n",
          pCallInfo->cParms, pCallInfo->u32Function, fFlags));

    /*
     * Preprocess the parameters, locking/buffering stuff.
     */
    rc = vbglR0HGCMInternalPreprocessCall(pCallInfo, cbCallInfo, fIsUser, &ParmInfo, &cbExtra);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate the request buffer and recreate the call request.
         */
        VMMDevHGCMCall *pHGCMCall;
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pHGCMCall,
                         sizeof(VMMDevHGCMCall) + pCallInfo->cParms * sizeof(HGCMFunctionParameter) + cbExtra,
                         VMMDevReq_HGCMCall);
        if (RT_SUCCESS(rc))
        {
            vbglR0HGCMInternalInitCall(pHGCMCall, pCallInfo, cbCallInfo, fIsUser, &ParmInfo);

            /*
             * Perform the call.
             */
            rc = vbglR0HGCMInternalDoCall(pHGCMCall, pfnAsyncCallback, pvAsyncData, u32AsyncData);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Copy back the result (parameters and buffers that changed).
                 */
                rc = vbglR0HGCMInternalCopyBackResult(pCallInfo, pHGCMCall, &ParmInfo, fIsUser, rc);
            }

            VbglGRFree(&pHGCMCall->header.header);
        }
    }

    /*
     * Copy back buffered data and release buffers & locks.
     */
    if (ParmInfo.cLockBufs)
        while (ParmInfo.cLockBufs-- > 0)
            RTR0MemObjFree(ParmInfo.aLockBufs[ParmInfo.cLockBufs].hObj, false /*fFreeMappings*/);

    return rc;
}


#  if ARCH_BITS == 64
DECLR0VBGL(int) VbglR0HGCMInternalCall32(VBoxGuestHGCMCallInfo *pCallInfo, uint32_t cbCallInfo, uint32_t fFlags,
                                         VBGLHGCMCALLBACK *pfnAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    VBoxGuestHGCMCallInfo   *pCallInfo64;
    HGCMFunctionParameter   *pParm64;
    HGCMFunctionParameter32 *pParm32;
    uint32_t                 cParms;
    uint32_t                 iParm;
    int rc;

    /*
     * Input validation.
     */
    AssertMsgReturn(!pCallInfo || !pfnAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS || !(fFlags & ~VBGLR0_HGCMCALL_F_MODE_MASK),
                    ("pCallInfo=%p pAsyncCallback=%p fFlags=%#x\n", pCallInfo, pfnAsyncCallback, fFlags),
                    VERR_INVALID_PARAMETER);
    AssertReturn(   cbCallInfo >= sizeof(VBoxGuestHGCMCallInfo)
                 || cbCallInfo >= pCallInfo->cParms * sizeof(HGCMFunctionParameter32),
                 VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_KERNEL, VERR_INVALID_PARAMETER);

    cParms = pCallInfo->cParms;
    Log(("VbglR0HGCMInternalCall32: cParms=%d, u32Function=%d, fFlags=%#x\n", cParms, pCallInfo->u32Function, fFlags));

    /*
     * The simple approach, allocate a temporary request and convert the parameters.
     */
    pCallInfo64 = (VBoxGuestHGCMCallInfo *)RTMemTmpAllocZ(sizeof(*pCallInfo64) + cParms * sizeof(HGCMFunctionParameter));
    if (!pCallInfo64)
        return VERR_NO_TMP_MEMORY;

    *pCallInfo64 = *pCallInfo;
    pParm32 = VBOXGUEST_HGCM_CALL_PARMS32(pCallInfo);
    pParm64 = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo64);
    for (iParm = 0; iParm < cParms; iParm++, pParm32++, pParm64++)
    {
        switch (pParm32->type)
        {
            case VMMDevHGCMParmType_32bit:
                pParm64->type      = VMMDevHGCMParmType_32bit;
                pParm64->u.value32 = pParm32->u.value32;
                break;

            case VMMDevHGCMParmType_64bit:
                pParm64->type      = VMMDevHGCMParmType_64bit;
                pParm64->u.value64 = pParm32->u.value64;
                break;

            case VMMDevHGCMParmType_LinAddr_Out:
            case VMMDevHGCMParmType_LinAddr:
            case VMMDevHGCMParmType_LinAddr_In:
                pParm64->type                   = pParm32->type;
                pParm64->u.Pointer.size         = pParm32->u.Pointer.size;
                pParm64->u.Pointer.u.linearAddr = pParm32->u.Pointer.u.linearAddr;
                break;

            default:
                rc = VERR_INVALID_PARAMETER;
                break;
        }
        if (RT_FAILURE(rc))
            break;
    }
    if (RT_SUCCESS(rc))
    {
        rc = VbglR0HGCMInternalCall(pCallInfo64, sizeof(*pCallInfo64) + cParms * sizeof(HGCMFunctionParameter), fFlags,
                                    pfnAsyncCallback, pvAsyncData, u32AsyncData);

        /*
         * Copy back.
         */
        for (iParm = 0; iParm < cParms; iParm++, pParm32++, pParm64++)
        {
            switch (pParm32->type)
            {
                case VMMDevHGCMParmType_32bit:
                    pParm32->u.value32 = pParm32->u.value32;
                    break;

                case VMMDevHGCMParmType_64bit:
                    pParm32->u.value64 = pParm64->u.value64;
                    break;

                case VMMDevHGCMParmType_LinAddr_Out:
                case VMMDevHGCMParmType_LinAddr:
                case VMMDevHGCMParmType_LinAddr_In:
                    pParm32->u.Pointer.size = pParm64->u.Pointer.size;
                    break;

                default:
                    rc = VERR_INTERNAL_ERROR_3;
                    break;
            }
        }
        *pCallInfo = *pCallInfo64;
    }

    RTMemTmpFree(pCallInfo64);
    return rc;
}
#  endif /* ARCH_BITS == 64 */

# else /* old code: */

/** @todo merge with the one below (use a header file). Too lazy now. */
DECLR0VBGL(int) VbglR0HGCMInternalCall (VBoxGuestHGCMCallInfo *pCallInfo, uint32_t cbCallInfo, uint32_t fFlags,
                                        VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    VMMDevHGCMCall *pHGCMCall;
    uint32_t cbParms;
    HGCMFunctionParameter *pParm;
    unsigned iParm;
    int rc;

    AssertMsgReturn(!pCallInfo || !pAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS || !(fFlags & ~VBGLR0_HGCMCALL_F_MODE_MASK),
                    ("pCallInfo=%p pAsyncCallback=%p fFlags=%#x\n", pCallInfo, pAsyncCallback, fFlags),
                    VERR_INVALID_PARAMETER);

    Log (("VbglR0HGCMInternalCall: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d, fFlags=%#x\n",
          pCallInfo->cParms, pCallInfo->u32Function, fFlags));

    pHGCMCall = NULL;

    if (cbCallInfo == 0)
    {
        /* Caller did not specify the size (a valid value should be at least sizeof(VBoxGuestHGCMCallInfo)).
         * Compute the size.
         */
        cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter);
    }
    else if (cbCallInfo < sizeof (VBoxGuestHGCMCallInfo))
    {
        return VERR_INVALID_PARAMETER;
    }
    else
    {
        cbParms = cbCallInfo - sizeof (VBoxGuestHGCMCallInfo);
    }

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMCall, sizeof (VMMDevHGCMCall) + cbParms, VMMDevReq_HGCMCall);

    Log (("VbglR0HGCMInternalCall: Allocated gr %p, rc = %Rrc, cbParms = %d\n", pHGCMCall, rc, cbParms));

    if (RT_SUCCESS(rc))
    {
        void *apvCtx[VBOX_HGCM_MAX_PARMS];
        memset (apvCtx, 0, sizeof(void *) * pCallInfo->cParms);

        /* Initialize request memory */
        pHGCMCall->header.fu32Flags = 0;
        pHGCMCall->header.result    = VINF_SUCCESS;

        pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
        pHGCMCall->u32Function = pCallInfo->u32Function;
        pHGCMCall->cParms      = pCallInfo->cParms;

        if (cbParms)
        {
            /* Lock user buffers. */
            pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

            for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
            {
                switch (pParm->type)
                {
                case VMMDevHGCMParmType_32bit:
                case VMMDevHGCMParmType_64bit:
                    break;

                case VMMDevHGCMParmType_LinAddr_Locked_In:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr_In;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked_Out:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr_Out;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr;
                    break;

                case VMMDevHGCMParmType_PageList:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    break;

                case VMMDevHGCMParmType_LinAddr_In:
                case VMMDevHGCMParmType_LinAddr_Out:
                case VMMDevHGCMParmType_LinAddr:
                    /* PORTME: When porting this to Darwin and other systems where the entire kernel isn't mapped
                       into every process, all linear address will have to be converted to physical SG lists at
                       this point. Care must also be taken on these guests to not mix kernel and user addresses
                       in HGCM calls, or we'll end up locking the wrong memory. If VMMDev/HGCM gets a linear address
                       it will assume that it's in the current memory context (i.e. use CR3 to translate it).

                       These kind of problems actually applies to some patched linux kernels too, including older
                       fedora releases. (The patch is the infamous 4G/4G patch, aka 4g4g, by Ingo Molnar.) */
                    rc = vbglLockLinear (&apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size,
                                         (pParm->type == VMMDevHGCMParmType_LinAddr_In) ? false : true /* write access */,
                                         fFlags);
                    break;

                default:
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                if (RT_FAILURE (rc))
                    break;
            }
            memcpy (VMMDEV_HGCM_CALL_PARMS(pHGCMCall), VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), cbParms);
        }

        /* Check that the parameter locking was ok. */
        if (RT_SUCCESS(rc))
        {
            Log (("calling VbglGRPerform\n"));

            /* Issue request */
            rc = VbglGRPerform (&pHGCMCall->header.header);

            Log (("VbglGRPerform rc = %Rrc (header rc=%d)\n", rc, pHGCMCall->header.result));

            /** If the call failed, but as a result of the request itself, then pretend success
             *  Upper layers will interpret the result code in the packet.
             */
            if (RT_FAILURE(rc) && rc == pHGCMCall->header.result)
            {
                Assert(pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE);
                rc = VINF_SUCCESS;
            }

            if (RT_SUCCESS(rc))
            {
                /* Check if host decides to process the request asynchronously. */
                if (rc == VINF_HGCM_ASYNC_EXECUTE)
                {
                    /* Wait for request completion interrupt notification from host */
                    Log (("Processing HGCM call asynchronously\n"));
                    pAsyncCallback (&pHGCMCall->header, pvAsyncData, u32AsyncData);
                }

                if (pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE)
                {
                    if (cbParms)
                    {
                        memcpy (VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), VMMDEV_HGCM_CALL_PARMS(pHGCMCall), cbParms);
                    }
                    pCallInfo->result = pHGCMCall->header.result;
                }
                else
                {
                    /* The callback returns without completing the request,
                     * that means the wait was interrrupted. That can happen
                     * if the request times out, the system reboots or the
                     * VBoxService ended abnormally.
                     *
                     * Cancel the request, the host will not write to the
                     * memory related to the cancelled request.
                     */
                    Log (("Cancelling HGCM call\n"));
                    pHGCMCall->header.fu32Flags |= VBOX_HGCM_REQ_CANCELLED;

                    pHGCMCall->header.header.requestType = VMMDevReq_HGCMCancel;
                    VbglGRPerform (&pHGCMCall->header.header);
                }
            }
        }

        /* Unlock user buffers. */
        pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

        for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
        {
            if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
                || pParm->type == VMMDevHGCMParmType_LinAddr_Out
                || pParm->type == VMMDevHGCMParmType_LinAddr)
            {
                if (apvCtx[iParm] != NULL)
                {
                    vbglUnlockLinear (apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size);
                }
            }
            else
                Assert(!apvCtx[iParm]);
        }

        if ((pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_CANCELLED) == 0)
            VbglGRFree (&pHGCMCall->header.header);
        else
            rc = VERR_INTERRUPTED;
    }

    return rc;
}

#  if ARCH_BITS == 64
/** @todo merge with the one above (use a header file). Too lazy now. */
DECLR0VBGL(int) VbglR0HGCMInternalCall32 (VBoxGuestHGCMCallInfo *pCallInfo, uint32_t cbCallInfo, uint32_t fFlags,
                                          VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    VMMDevHGCMCall *pHGCMCall;
    uint32_t cbParms;
    HGCMFunctionParameter32 *pParm;
    unsigned iParm;
    int rc;

    AssertMsgReturn(!pCallInfo || !pAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS || !(fFlags & ~VBGLR0_HGCMCALL_F_MODE_MASK),
                    ("pCallInfo=%p pAsyncCallback=%p fFlags=%#x\n", pCallInfo, pAsyncCallback, fFlags),
                    VERR_INVALID_PARAMETER);

    Log (("VbglR0HGCMInternalCall32: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d, fFlags=%#x\n",
          pCallInfo->cParms, pCallInfo->u32Function, fFlags));

    pHGCMCall = NULL;

    if (cbCallInfo == 0)
    {
        /* Caller did not specify the size (a valid value should be at least sizeof(VBoxGuestHGCMCallInfo)).
         * Compute the size.
         */
        cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter32);
    }
    else if (cbCallInfo < sizeof (VBoxGuestHGCMCallInfo))
    {
        return VERR_INVALID_PARAMETER;
    }
    else
    {
        cbParms = cbCallInfo - sizeof (VBoxGuestHGCMCallInfo);
    }

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMCall, sizeof (VMMDevHGCMCall) + cbParms, VMMDevReq_HGCMCall32);

    Log (("VbglR0HGCMInternalCall32: Allocated gr %p, rc = %Rrc, cbParms = %d\n", pHGCMCall, rc, cbParms));

    if (RT_SUCCESS(rc))
    {
        void *apvCtx[VBOX_HGCM_MAX_PARMS];
        memset (apvCtx, 0, sizeof(void *) * pCallInfo->cParms);

        /* Initialize request memory */
        pHGCMCall->header.fu32Flags = 0;
        pHGCMCall->header.result    = VINF_SUCCESS;

        pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
        pHGCMCall->u32Function = pCallInfo->u32Function;
        pHGCMCall->cParms      = pCallInfo->cParms;

        if (cbParms)
        {
            /* Lock user buffers. */
            pParm = VBOXGUEST_HGCM_CALL_PARMS32(pCallInfo);

            for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
            {
                switch (pParm->type)
                {
                case VMMDevHGCMParmType_32bit:
                case VMMDevHGCMParmType_64bit:
                    break;

                case VMMDevHGCMParmType_LinAddr_Locked_In:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr_In;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked_Out:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr_Out;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    else
                        pParm->type = VMMDevHGCMParmType_LinAddr;
                    break;

                case VMMDevHGCMParmType_PageList:
                    if ((fFlags & VBGLR0_HGCMCALL_F_MODE_MASK) == VBGLR0_HGCMCALL_F_USER)
                        rc = VERR_INVALID_PARAMETER;
                    break;

                case VMMDevHGCMParmType_LinAddr_In:
                case VMMDevHGCMParmType_LinAddr_Out:
                case VMMDevHGCMParmType_LinAddr:
                    /* PORTME: When porting this to Darwin and other systems where the entire kernel isn't mapped
                       into every process, all linear address will have to be converted to physical SG lists at
                       this point. Care must also be taken on these guests to not mix kernel and user addresses
                       in HGCM calls, or we'll end up locking the wrong memory. If VMMDev/HGCM gets a linear address
                       it will assume that it's in the current memory context (i.e. use CR3 to translate it).

                       These kind of problems actually applies to some patched linux kernels too, including older
                       fedora releases. (The patch is the infamous 4G/4G patch, aka 4g4g, by Ingo Molnar.) */
                    rc = vbglLockLinear (&apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size,
                                         (pParm->type == VMMDevHGCMParmType_LinAddr_In) ? false : true /* write access */,
                                         fFlags);
                    break;

                default:
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                if (RT_FAILURE (rc))
                    break;
            }
            memcpy (VMMDEV_HGCM_CALL_PARMS32(pHGCMCall), VBOXGUEST_HGCM_CALL_PARMS32(pCallInfo), cbParms);
        }

        /* Check that the parameter locking was ok. */
        if (RT_SUCCESS(rc))
        {
            Log (("calling VbglGRPerform\n"));

            /* Issue request */
            rc = VbglGRPerform (&pHGCMCall->header.header);

            Log (("VbglGRPerform rc = %Rrc (header rc=%d)\n", rc, pHGCMCall->header.result));

            /** If the call failed, but as a result of the request itself, then pretend success
             *  Upper layers will interpret the result code in the packet.
             */
            if (RT_FAILURE(rc) && rc == pHGCMCall->header.result)
            {
                Assert(pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE);
                rc = VINF_SUCCESS;
            }

            if (RT_SUCCESS(rc))
            {
                /* Check if host decides to process the request asynchronously. */
                if (rc == VINF_HGCM_ASYNC_EXECUTE)
                {
                    /* Wait for request completion interrupt notification from host */
                    Log (("Processing HGCM call asynchronously\n"));
                    pAsyncCallback (&pHGCMCall->header, pvAsyncData, u32AsyncData);
                }

                if (pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE)
                {
                    if (cbParms)
                        memcpy (VBOXGUEST_HGCM_CALL_PARMS32(pCallInfo), VMMDEV_HGCM_CALL_PARMS32(pHGCMCall), cbParms);

                    pCallInfo->result = pHGCMCall->header.result;
                }
                else
                {
                    /* The callback returns without completing the request,
                     * that means the wait was interrrupted. That can happen
                     * if the request times out, the system reboots or the
                     * VBoxService ended abnormally.
                     *
                     * Cancel the request, the host will not write to the
                     * memory related to the cancelled request.
                     */
                    Log (("Cancelling HGCM call\n"));
                    pHGCMCall->header.fu32Flags |= VBOX_HGCM_REQ_CANCELLED;

                    pHGCMCall->header.header.requestType = VMMDevReq_HGCMCancel;
                    VbglGRPerform (&pHGCMCall->header.header);
                }
            }
        }

        /* Unlock user buffers. */
        pParm = VBOXGUEST_HGCM_CALL_PARMS32(pCallInfo);

        for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
        {
            if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
                || pParm->type == VMMDevHGCMParmType_LinAddr_Out
                || pParm->type == VMMDevHGCMParmType_LinAddr)
            {
                if (apvCtx[iParm] != NULL)
                {
                    vbglUnlockLinear (apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size);
                }
            }
            else
                Assert(!apvCtx[iParm]);
        }

        if ((pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_CANCELLED) == 0)
            VbglGRFree (&pHGCMCall->header.header);
        else
            rc = VERR_INTERRUPTED;
    }

    return rc;
}
#  endif /* ARCH_BITS == 64 */

# endif /* old code */

#endif /* VBGL_VBOXGUEST */

