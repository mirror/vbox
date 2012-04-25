/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxUhgsmiBase_h__
#define ___VBoxUhgsmiBase_h__

#include <VBox/VBoxUhgsmi.h>
#include <VBox/VBoxCrHgsmi.h>

#include <windows.h>
#include <D3dkmthk.h>
//#include <D3dumddi.h>
#include "common/wddm/VBoxMPIf.h"


#ifndef VBOX_WITH_CRHGSMI
#error "VBOX_WITH_CRHGSMI not defined!"
#endif

typedef DECLCALLBACK(int) FNVBOXCRHGSMI_CTLCON_CALL(struct VBOXUHGSMI_PRIVATE_BASE *pHgsmi, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo);
typedef FNVBOXCRHGSMI_CTLCON_CALL *PFNVBOXCRHGSMI_CTLCON_CALL;

#define VBoxCrHgsmiPrivateCtlConCall(_pHgsmi, _pCallInfo, _cbCallInfo) (_pHgsmi->pfnCtlConCall((_pHgsmi), (_pCallInfo), (_cbCallInfo)))


typedef DECLCALLBACK(int) FNVBOXCRHGSMI_CTLCON_GETCLIENTID(struct VBOXUHGSMI_PRIVATE_BASE *pHgsmi, uint32_t *pu32ClientID);
typedef FNVBOXCRHGSMI_CTLCON_GETCLIENTID *PFNVBOXCRHGSMI_CTLCON_GETCLIENTID;

#define VBoxCrHgsmiPrivateCtlConGetClientID(_pHgsmi, _pu32ClientID) (_pHgsmi->pfnCtlConGetClientID((_pHgsmi), (_pu32ClientID)))


typedef struct VBOXUHGSMI_PRIVATE_BASE
{
    VBOXUHGSMI Base;
    PFNVBOXCRHGSMI_CTLCON_CALL pfnCtlConCall;
    PFNVBOXCRHGSMI_CTLCON_GETCLIENTID pfnCtlConGetClientID;
#ifdef VBOX_CRHGSMI_WITH_D3DDEV
    HVBOXCRHGSMI_CLIENT hClient;
#endif
} VBOXUHGSMI_PRIVATE_BASE, *PVBOXUHGSMI_PRIVATE_BASE;

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_BASE
{
    VBOXUHGSMI_BUFFER Base;
    HANDLE hSynch;
    D3DKMT_HANDLE hAllocation;
} VBOXUHGSMI_BUFFER_PRIVATE_BASE, *PVBOXUHGSMI_BUFFER_PRIVATE_BASE;

#define VBOXUHGSMIBASE_GET_PRIVATE(_p, _t) ((_t*)(((uint8_t*)_p) - RT_OFFSETOF(_t, Base)))
#define VBOXUHGSMIBASE_GET(_p) VBOXUHGSMIBASE_GET_PRIVATE(_p, VBOXUHGSMI_PRIVATE_BASE)
#define VBOXUHGSMIBASE_GET_BUFFER(_p) VBOXUHGSMIBASE_GET_PRIVATE(_p, VBOXUHGSMI_BUFFER_PRIVATE_BASE)


DECLINLINE(int) vboxUhgsmiBaseLockData(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags,
                                    D3DDDICB_LOCKFLAGS *pfFlags, UINT *pNumPages, UINT* pPages)
{
    D3DDDICB_LOCKFLAGS fLockFlags;
    fLockFlags.Value = 0;
    if (fFlags.bLockEntire)
    {
        Assert(!offLock);
        fLockFlags.LockEntire = 1;
    }
    else
    {
        if (!cbLock)
        {
            Assert(0);
            return VERR_INVALID_PARAMETER;
        }
        if (offLock + cbLock > pBuf->cbBuffer)
        {
            Assert(0);
            return VERR_INVALID_PARAMETER;
        }

        uint32_t iFirstPage = offLock >> 12;
        uint32_t iAfterLastPage = (cbLock + 0xfff) >> 12;
        uint32_t cPages = iAfterLastPage - iFirstPage;
        uint32_t cBufPages = pBuf->cbBuffer >> 12;
        Assert(cPages <= (cBufPages));

        if (cPages == cBufPages)
        {
            *pNumPages = 0;
            fLockFlags.LockEntire = 1;
        }
        else
        {
            *pNumPages = cPages;
            for (UINT i = 0, j = iFirstPage; i < cPages; ++i, ++j)
            {
                pPages[i] = j;
            }
        }

    }

    fLockFlags.ReadOnly = fFlags.bReadOnly;
    fLockFlags.WriteOnly = fFlags.bWriteOnly;
    fLockFlags.DonotWait = fFlags.bDonotWait;
//    fLockFlags.Discard = fFlags.bDiscard;
    *pfFlags = fLockFlags;
    return VINF_SUCCESS;
}

DECLINLINE(int) vboxUhgsmiBaseEventChkCreate(VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType, HANDLE *phSynch)
{
    *phSynch = NULL;

    if (fUhgsmiType.fCommand)
    {
        *phSynch = CreateEvent(
                  NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes */
                  FALSE, /* BOOL bManualReset */
                  FALSE, /* BOOL bInitialState */
                  NULL /* LPCTSTR lpName */
            );
        Assert(*phSynch);
        if (!*phSynch)
        {
            DWORD winEr = GetLastError();
            /* todo: translate winer */
            return VERR_GENERAL_FAILURE;
        }
    }
    return VINF_SUCCESS;
}

DECLINLINE(int) vboxUhgsmiBaseDmaFill(PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers,
        VOID* pCommandBuffer, UINT *pCommandBufferSize,
        D3DDDI_ALLOCATIONLIST *pAllocationList, UINT AllocationListSize,
        D3DDDI_PATCHLOCATIONLIST *pPatchLocationList, UINT PatchLocationListSize)
{
    const uint32_t cbDmaCmd = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD, aBufInfos[cBuffers]);
    if (*pCommandBufferSize < cbDmaCmd)
    {
        Assert(0);
        return VERR_GENERAL_FAILURE;
    }
    if (AllocationListSize < cBuffers)
    {
        Assert(0);
        return VERR_GENERAL_FAILURE;
    }

    *pCommandBufferSize = cbDmaCmd;

    PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD pHdr = (PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD)pCommandBuffer;
    pHdr->Base.enmCmd = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
    pHdr->Base.u32CmdReserved = cBuffers;

    PVBOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO pBufSubmInfo = pHdr->aBufInfos;

    for (uint32_t i = 0; i < cBuffers; ++i)
    {
        PVBOXUHGSMI_BUFFER_SUBMIT pBufInfo = &aBuffers[i];
        PVBOXUHGSMI_BUFFER_PRIVATE_BASE pBuffer = VBOXUHGSMIBASE_GET_BUFFER(pBufInfo->pBuf);

        memset(pAllocationList, 0, sizeof (D3DDDI_ALLOCATIONLIST));
        pAllocationList->hAllocation = pBuffer->hAllocation;
        pAllocationList->Value = 0;
        pAllocationList->WriteOperation = !pBufInfo->fFlags.bHostReadOnly;
        pAllocationList->DoNotRetireInstance = pBufInfo->fFlags.bDoNotRetire;
        pBufSubmInfo->bDoNotSignalCompletion = 0;
        if (pBufInfo->fFlags.bEntireBuffer)
        {
            pBufSubmInfo->offData = 0;
            pBufSubmInfo->cbData = pBuffer->Base.cbBuffer;
        }
        else
        {
            pBufSubmInfo->offData = pBufInfo->offData;
            pBufSubmInfo->cbData = pBufInfo->cbData;
        }

        ++pAllocationList;
        ++pBufSubmInfo;
    }

    return VINF_SUCCESS;
}

#endif /* #ifndef ___VBoxUhgsmiBase_h__ */
