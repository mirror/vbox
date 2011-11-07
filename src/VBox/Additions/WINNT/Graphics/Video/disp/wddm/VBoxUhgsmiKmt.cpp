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

#include "VBoxDispD3DCmn.h"

#include <iprt/mem.h>
#include <iprt/err.h>

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_KMT
{
    VBOXUHGSMI_BUFFER_PRIVATE_BASE BasePrivate;
    PVBOXUHGSMI_PRIVATE_KMT pHgsmi;
    CRITICAL_SECTION CritSect;
    UINT aLockPageIndices[1];
} VBOXUHGSMI_BUFFER_PRIVATE_KMT, *PVBOXUHGSMI_BUFFER_PRIVATE_KMT;

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC
{
    VBOXUHGSMI_BUFFER Base;
    PVBOXUHGSMI_PRIVATE_KMT pHgsmi;
    VBOXVIDEOCM_UM_ALLOC Alloc;
} VBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC, *PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC;

#define VBOXUHGSMIKMT_GET_BUFFER(_p) VBOXUHGSMIKMT_GET_PRIVATE(_p, VBOXUHGSMI_BUFFER_PRIVATE_KMT)
#define VBOXUHGSMIKMTESC_GET_PRIVATE(_p, _t) ((_t*)(((uint8_t*)_p) - RT_OFFSETOF(_t, Base)))
#define VBOXUHGSMIKMTESC_GET_BUFFER(_p) VBOXUHGSMIKMTESC_GET_PRIVATE(_p, VBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC)

DECLCALLBACK(int) vboxUhgsmiKmtBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_DESTROYALLOCATION DdiDealloc;
    DdiDealloc.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiDealloc.hResource = NULL;
    DdiDealloc.phAllocationList = &pBuffer->BasePrivate.hAllocation;
    DdiDealloc.AllocationCount = 1;
    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTDestroyAllocation(&DdiDealloc);
    Assert(!Status);
    if (!Status)
    {
        if (pBuffer->BasePrivate.Base.bSynchCreated)
        {
            CloseHandle(pBuffer->BasePrivate.Base.hSynch);
        }
        RTMemFree(pBuffer);
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_LOCK DdiLock = {0};
    DdiLock.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiLock.hAllocation = pBuffer->BasePrivate.hAllocation;
    DdiLock.PrivateDriverData = NULL;

    EnterCriticalSection(&pBuffer->CritSect);

    int rc = vboxUhgsmiBaseLockData(pBuf, offLock, cbLock, fFlags,
                                         &DdiLock.Flags, &DdiLock.NumPages, pBuffer->aLockPageIndices);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    if (DdiLock.NumPages)
        DdiLock.pPages = pBuffer->aLockPageIndices;
    else
        DdiLock.pPages = NULL;

    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTLock(&DdiLock);
    Assert(!Status);
    LeaveCriticalSection(&pBuffer->CritSect);
    if (!Status)
    {
        *pvLock = (void*)(((uint8_t*)DdiLock.pData) + (offLock & 0xfff));
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferUnlock(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_UNLOCK DdiUnlock;

    DdiUnlock.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiUnlock.NumAllocations = 1;
    DdiUnlock.phAllocations = &pBuffer->BasePrivate.hAllocation;
    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTUnlock(&DdiUnlock);
    Assert(!Status);
    if (!Status)
        return VINF_SUCCESS;
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf,
        VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType, HVBOXUHGSMI_SYNCHOBJECT hSynch,
        PVBOXUHGSMI_BUFFER* ppBuf)
{
    bool bSynchCreated = false;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    int rc = vboxUhgsmiBaseEventChkCreate(enmSynchType, &hSynch, &bSynchCreated);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 12;
    Assert(cPages);

    PVBOXUHGSMI_PRIVATE_KMT pPrivate = VBOXUHGSMIKMT_GET(pHgsmi);
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuf = (PVBOXUHGSMI_BUFFER_PRIVATE_KMT)RTMemAllocZ(RT_OFFSETOF(VBOXUHGSMI_BUFFER_PRIVATE_KMT, aLockPageIndices[cPages]));
    Assert(pBuf);
    if (pBuf)
    {
        struct
        {
            D3DKMT_CREATEALLOCATION DdiAlloc;
            D3DDDI_ALLOCATIONINFO DdiAllocInfo;
            VBOXWDDM_ALLOCINFO AllocInfo;
        } Buf;
        memset(&Buf, 0, sizeof (Buf));
        Buf.DdiAlloc.hDevice = pPrivate->Device.hDevice;
        Buf.DdiAlloc.NumAllocations = 1;
        Buf.DdiAlloc.pAllocationInfo = &Buf.DdiAllocInfo;
        Buf.DdiAllocInfo.pPrivateDriverData = &Buf.AllocInfo;
        Buf.DdiAllocInfo.PrivateDriverDataSize = sizeof (Buf.AllocInfo);
        Buf.AllocInfo.enmType = VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER;
        Buf.AllocInfo.cbBuffer = cbBuf;
        Buf.AllocInfo.hSynch = (uint64_t)hSynch;
        Buf.AllocInfo.enmSynchType = enmSynchType;

        HRESULT hr = pPrivate->Callbacks.pfnD3DKMTCreateAllocation(&Buf.DdiAlloc);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            InitializeCriticalSection(&pBuf->CritSect);

            Assert(Buf.DdiAllocInfo.hAllocation);
            pBuf->BasePrivate.Base.pfnLock = vboxUhgsmiKmtBufferLock;
            pBuf->BasePrivate.Base.pfnUnlock = vboxUhgsmiKmtBufferUnlock;
//            pBuf->Base.pfnAdjustValidDataRange = vboxUhgsmiKmtBufferAdjustValidDataRange;
            pBuf->BasePrivate.Base.pfnDestroy = vboxUhgsmiKmtBufferDestroy;

            pBuf->BasePrivate.Base.hSynch = hSynch;
            pBuf->BasePrivate.Base.enmSynchType = enmSynchType;
            pBuf->BasePrivate.Base.cbBuffer = cbBuf;
            pBuf->BasePrivate.Base.bSynchCreated = bSynchCreated;

            pBuf->pHgsmi = pPrivate;
            pBuf->BasePrivate.hAllocation = Buf.DdiAllocInfo.hAllocation;

            *ppBuf = &pBuf->BasePrivate.Base;

            return VINF_SUCCESS;
        }
        else
        {
            rc = VERR_OUT_OF_RESOURCES;
        }

        RTMemFree(pBuf);
    }
    else
        rc = VERR_NO_MEMORY;

    if (bSynchCreated)
        CloseHandle(hSynch);

    return rc;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferSubmitAsynch(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    PVBOXUHGSMI_PRIVATE_KMT pHg = VBOXUHGSMIKMT_GET(pHgsmi);
    UINT cbDmaCmd = pHg->Context.CommandBufferSize;
    int rc = vboxUhgsmiBaseDmaFill(aBuffers, cBuffers,
            pHg->Context.pCommandBuffer, &cbDmaCmd,
            pHg->Context.pAllocationList, pHg->Context.AllocationListSize,
            pHg->Context.pPatchLocationList, pHg->Context.PatchLocationListSize);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    D3DKMT_RENDER DdiRender = {0};
    DdiRender.hContext = pHg->Context.hContext;
    DdiRender.CommandLength = cbDmaCmd;
    DdiRender.AllocationCount = cBuffers;
    Assert(DdiRender.CommandLength);
    Assert(DdiRender.CommandLength < UINT32_MAX/2);

    HRESULT hr = pHg->Callbacks.pfnD3DKMTRender(&DdiRender);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        pHg->Context.CommandBufferSize = DdiRender.NewCommandBufferSize;
        pHg->Context.pCommandBuffer = DdiRender.pNewCommandBuffer;
        pHg->Context.AllocationListSize = DdiRender.NewAllocationListSize;
        pHg->Context.pAllocationList = DdiRender.pNewAllocationList;
        pHg->Context.PatchLocationListSize = DdiRender.NewPatchLocationListSize;
        pHg->Context.pPatchLocationList = DdiRender.pNewPatchLocationList;

        return VINF_SUCCESS;
    }

    return VERR_GENERAL_FAILURE;
}


DECLCALLBACK(int) vboxUhgsmiKmtEscBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC pBuffer = VBOXUHGSMIKMTESC_GET_BUFFER(pBuf);
    *pvLock = (void*)(pBuffer->Alloc.pvData + offLock);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxUhgsmiKmtEscBufferUnlock(PVBOXUHGSMI_BUFFER pBuf)
{
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxUhgsmiKmtEscBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC pBuffer = VBOXUHGSMIKMTESC_GET_BUFFER(pBuf);
    PVBOXUHGSMI_PRIVATE_KMT pPrivate = pBuffer->pHgsmi;
    D3DKMT_ESCAPE DdiEscape = {0};
    VBOXDISPIFESCAPE_UHGSMI_DEALLOCATE DeallocInfo = {0};
    DdiEscape.hAdapter = pPrivate->Adapter.hAdapter;
    DdiEscape.hDevice = pPrivate->Device.hDevice;
    DdiEscape.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //Buf.DdiEscape.Flags.HardwareAccess = 1;
    DdiEscape.pPrivateDriverData = &DeallocInfo;
    DdiEscape.PrivateDriverDataSize = sizeof (DeallocInfo);
    DdiEscape.hContext = pPrivate->Context.hContext;

    DeallocInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_DEALLOCATE;
    DeallocInfo.hAlloc = pBuffer->Alloc.hAlloc;

    HRESULT hr = pPrivate->Callbacks.pfnD3DKMTEscape(&DdiEscape);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        if (pBuffer->Base.bSynchCreated)
        {
            CloseHandle(pBuffer->Base.hSynch);
        }
        RTMemFree(pBuffer);
        return VINF_SUCCESS;
    }

    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtEscBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf,
        VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType, HVBOXUHGSMI_SYNCHOBJECT hSynch,
        PVBOXUHGSMI_BUFFER* ppBuf)
{
    bool bSynchCreated = false;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    int rc = vboxUhgsmiBaseEventChkCreate(enmSynchType, &hSynch, &bSynchCreated);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 12;
    Assert(cPages);

    PVBOXUHGSMI_PRIVATE_KMT pPrivate = VBOXUHGSMIKMT_GET(pHgsmi);
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC pBuf = (PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC)RTMemAllocZ(sizeof (VBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC));
    Assert(pBuf);
    if (pBuf)
    {
        struct
        {
            D3DKMT_ESCAPE DdiEscape;
            VBOXDISPIFESCAPE_UHGSMI_ALLOCATE AllocInfo;
        } Buf;
        memset(&Buf, 0, sizeof (Buf));
        Buf.DdiEscape.hAdapter = pPrivate->Adapter.hAdapter;
        Buf.DdiEscape.hDevice = pPrivate->Device.hDevice;
        Buf.DdiEscape.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
        //Buf.DdiEscape.Flags.HardwareAccess = 1;
        Buf.DdiEscape.pPrivateDriverData = &Buf.AllocInfo;
        Buf.DdiEscape.PrivateDriverDataSize = sizeof (Buf.AllocInfo);
        Buf.DdiEscape.hContext = pPrivate->Context.hContext;

        Buf.AllocInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_ALLOCATE;
        Buf.AllocInfo.Alloc.cbData = cbBuf;
        Buf.AllocInfo.Alloc.hSynch = (uint64_t)hSynch;
        Buf.AllocInfo.Alloc.enmSynchType = enmSynchType;

        HRESULT hr = pPrivate->Callbacks.pfnD3DKMTEscape(&Buf.DdiEscape);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            pBuf->Alloc = Buf.AllocInfo.Alloc;
            Assert(pBuf->Alloc.pvData);
            pBuf->pHgsmi = pPrivate;
            pBuf->Base.pfnLock = vboxUhgsmiKmtEscBufferLock;
            pBuf->Base.pfnUnlock = vboxUhgsmiKmtEscBufferUnlock;
//            pBuf->Base.pfnAdjustValidDataRange = vboxUhgsmiKmtBufferAdjustValidDataRange;
            pBuf->Base.pfnDestroy = vboxUhgsmiKmtEscBufferDestroy;

            pBuf->Base.hSynch = hSynch;
            pBuf->Base.enmSynchType = enmSynchType;
            pBuf->Base.cbBuffer = Buf.AllocInfo.Alloc.cbData;
            pBuf->Base.bSynchCreated = bSynchCreated;

            *ppBuf = &pBuf->Base;

            return VINF_SUCCESS;
        }
        else
        {
            rc = VERR_OUT_OF_RESOURCES;
        }

        RTMemFree(pBuf);
    }
    else
        rc = VERR_NO_MEMORY;

    if (bSynchCreated)
        CloseHandle(hSynch);

    return rc;
}

DECLCALLBACK(int) vboxUhgsmiKmtEscBufferSubmitAsynch(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    /* we no chromium will not submit more than three buffers actually,
     * for simplicity allocate it statically on the stack  */
    struct
    {
        VBOXDISPIFESCAPE_UHGSMI_SUBMIT SubmitInfo;
        VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE aBufInfos[3];
    } Buf;

    if (cBuffers > RT_ELEMENTS(Buf.aBufInfos) + 1)
    {
        Assert(0);
        return VERR_INVALID_PARAMETER;
    }


    PVBOXUHGSMI_PRIVATE_KMT pPrivate = VBOXUHGSMIKMT_GET(pHgsmi);
    D3DKMT_ESCAPE DdiEscape = {0};

    DdiEscape.hAdapter = pPrivate->Adapter.hAdapter;
    DdiEscape.hDevice = pPrivate->Device.hDevice;
    DdiEscape.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //Buf.DdiEscape.Flags.HardwareAccess = 1;
    DdiEscape.pPrivateDriverData = &Buf.SubmitInfo;
    DdiEscape.PrivateDriverDataSize = RT_OFFSETOF(VBOXDISPIFESCAPE_UHGSMI_SUBMIT, aBuffers[cBuffers]);
    DdiEscape.hContext = pPrivate->Context.hContext;

    Buf.SubmitInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_SUBMIT;
    Buf.SubmitInfo.EscapeHdr.u32CmdSpecific = cBuffers;
    for (UINT i = 0; i < cBuffers; ++i)
    {
        VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *pSubmInfo = &Buf.SubmitInfo.aBuffers[i];
        PVBOXUHGSMI_BUFFER_SUBMIT pBufInfo = &aBuffers[i];
        PVBOXUHGSMI_BUFFER_PRIVATE_KMT_ESC pBuf = VBOXUHGSMIKMTESC_GET_BUFFER(pBufInfo->pBuf);
        pSubmInfo->hAlloc = pBuf->Alloc.hAlloc;
        pSubmInfo->Info.fSubFlags = pBufInfo->fFlags;
        if (pBufInfo->fFlags.bEntireBuffer)
        {
            pSubmInfo->Info.offData = 0;
            pSubmInfo->Info.cbData = pBuf->Base.cbBuffer;
        }
        else
        {
            pSubmInfo->Info.offData = pBufInfo->offData;
            pSubmInfo->Info.cbData = pBufInfo->cbData;
        }
    }

    HRESULT hr = pPrivate->Callbacks.pfnD3DKMTEscape(&DdiEscape);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        return VINF_SUCCESS;
    }

    return VERR_GENERAL_FAILURE;
}

static HRESULT vboxUhgsmiKmtEngineCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D)
{
    HRESULT hr = vboxDispKmtCallbacksInit(&pHgsmi->Callbacks);
    if (hr == S_OK)
    {
        hr = vboxDispKmtOpenAdapter(&pHgsmi->Callbacks, &pHgsmi->Adapter);
        if (hr == S_OK)
        {
            hr = vboxDispKmtCreateDevice(&pHgsmi->Adapter, &pHgsmi->Device);
            if (hr == S_OK)
            {
                hr = vboxDispKmtCreateContext(&pHgsmi->Device, &pHgsmi->Context,
                        bD3D ? VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_3D : VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL,
                                NULL, 0);
                if (hr == S_OK)
                {
                    return S_OK;
                }
                else
                {
                    WARN(("vboxDispKmtCreateContext failed, hr(0x%x)", hr));
                }
                vboxDispKmtDestroyDevice(&pHgsmi->Device);
            }
            else
            {
                WARN(("vboxDispKmtCreateDevice failed, hr(0x%x)", hr));
            }
            vboxDispKmtCloseAdapter(&pHgsmi->Adapter);
        }
        else
        {
            WARN(("vboxDispKmtOpenAdapter failed, hr(0x%x)", hr));
        }

        vboxDispKmtCallbacksTerm(&pHgsmi->Callbacks);
    }
    else
    {
        WARN(("vboxDispKmtCallbacksInit failed, hr(0x%x)", hr));
    }
    return hr;
}
HRESULT vboxUhgsmiKmtCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D)
{
    pHgsmi->BasePrivate.Base.pfnBufferCreate = vboxUhgsmiKmtBufferCreate;
    pHgsmi->BasePrivate.Base.pfnBufferSubmitAsynch = vboxUhgsmiKmtBufferSubmitAsynch;
#ifdef VBOX_CRHGSMI_WITH_D3DDEV
    pHgsmi->BasePrivate.hClient = NULL;
#endif
    return vboxUhgsmiKmtEngineCreate(pHgsmi, bD3D);
}

HRESULT vboxUhgsmiKmtEscCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D)
{
    pHgsmi->BasePrivate.Base.pfnBufferCreate = vboxUhgsmiKmtEscBufferCreate;
    pHgsmi->BasePrivate.Base.pfnBufferSubmitAsynch = vboxUhgsmiKmtEscBufferSubmitAsynch;
#ifdef VBOX_CRHGSMI_WITH_D3DDEV
    pHgsmi->BasePrivate.hClient = NULL;
#endif
    return vboxUhgsmiKmtEngineCreate(pHgsmi, bD3D);
}

HRESULT vboxUhgsmiKmtDestroy(PVBOXUHGSMI_PRIVATE_KMT pHgsmi)
{
    HRESULT hr = vboxDispKmtDestroyContext(&pHgsmi->Context);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vboxDispKmtDestroyDevice(&pHgsmi->Device);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hr = vboxDispKmtCloseAdapter(&pHgsmi->Adapter);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                hr = vboxDispKmtCallbacksTerm(&pHgsmi->Callbacks);
                Assert(hr == S_OK);
                if (hr == S_OK)
                    return S_OK;
            }
        }
    }
    return hr;
}
