/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Oracle Corporation
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
#include "VBoxDispD3D.h"
#include "../../../include/VBoxDisplay.h"

#include <iprt/list.h>


typedef struct VBOXDISPCM_SESSION
{
    HANDLE hEvent;
    CRITICAL_SECTION CritSect;
    RTLISTNODE CtxList;
} VBOXDISPCM_SESSION, *PVBOXDISPCM_SESSION;

typedef struct VBOXDISPCM_MGR
{
    VBOXDISPCM_SESSION Session;
} VBOXDISPCM_MGR, *PVBOXDISPCM_MGR;

/* the cm is one per process */
static VBOXDISPCM_MGR g_pVBoxCmMgr;

HRESULT vboxDispCmSessionTerm(PVBOXDISPCM_SESSION pSession)
{
    Assert(RTListIsEmpty(&pSession->CtxList));
    BOOL bRc = CloseHandle(pSession->hEvent);
    Assert(bRc);
    if (bRc)
    {
        DeleteCriticalSection(&pSession->CritSect);
        return S_OK;
    }
    DWORD winEr = GetLastError();
    HRESULT hr = HRESULT_FROM_WIN32(winEr);
    return hr;
}

HRESULT vboxDispCmSessionInit(PVBOXDISPCM_SESSION pSession)
{
    HANDLE hEvent = CreateEvent(NULL,
            FALSE, /* BOOL bManualReset */
            FALSE, /* BOOL bInitialState */
            NULL /* LPCTSTR lpName */
            );
    Assert(hEvent);
    if (hEvent)
    {
        pSession->hEvent = hEvent;
        InitializeCriticalSection(&pSession->CritSect);
        RTListInit(&pSession->CtxList);
        return S_OK;
    }
    DWORD winEr = GetLastError();
    HRESULT hr = HRESULT_FROM_WIN32(winEr);
    return hr;
}

void vboxDispCmSessionCtxAdd(PVBOXDISPCM_SESSION pSession, PVBOXWDDMDISP_CONTEXT pContext)
{
    EnterCriticalSection(&pSession->CritSect);
    RTListAppend(&pSession->CtxList, &pContext->ListNode);
    LeaveCriticalSection(&pSession->CritSect);
}

void vboxDispCmSessionCtxRemoveLocked(PVBOXDISPCM_SESSION pSession, PVBOXWDDMDISP_CONTEXT pContext)
{
    RTListNodeRemove(&pContext->ListNode);
}

void vboxDispCmSessionCtxRemove(PVBOXDISPCM_SESSION pSession, PVBOXWDDMDISP_CONTEXT pContext)
{
    EnterCriticalSection(&pSession->CritSect);
    vboxDispCmSessionCtxRemoveLocked(pSession, pContext);
    LeaveCriticalSection(&pSession->CritSect);
}

HRESULT vboxDispCmInit()
{
    HRESULT hr = vboxDispCmSessionInit(&g_pVBoxCmMgr.Session);
    Assert(hr == S_OK);
    return hr;
}

HRESULT vboxDispCmTerm()
{
    HRESULT hr = vboxDispCmSessionTerm(&g_pVBoxCmMgr.Session);
    Assert(hr == S_OK);
    return hr;
}

HRESULT vboxDispCmCtxCreate(PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_CONTEXT pContext)
{
    VBOXWDDM_CREATECONTEXT_INFO Info;
    Info.hUmEvent = (uint64_t)g_pVBoxCmMgr.Session.hEvent;
    Info.u64UmInfo = (uint64_t)pContext;

    pContext->ContextInfo.NodeOrdinal = 0;
    pContext->ContextInfo.EngineAffinity = 0;
    pContext->ContextInfo.Flags.Value = 0;
    pContext->ContextInfo.pPrivateDriverData = &Info;
    pContext->ContextInfo.PrivateDriverDataSize = sizeof (Info);
    pContext->ContextInfo.hContext = 0;
    pContext->ContextInfo.pCommandBuffer = NULL;
    pContext->ContextInfo.CommandBufferSize = 0;
    pContext->ContextInfo.pAllocationList = NULL;
    pContext->ContextInfo.AllocationListSize = 0;
    pContext->ContextInfo.pPatchLocationList = NULL;
    pContext->ContextInfo.PatchLocationListSize = 0;

    HRESULT hr = S_OK;
    hr = pDevice->RtCallbacks.pfnCreateContextCb(pDevice->hDevice, &pContext->ContextInfo);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        vboxDispCmSessionCtxAdd(&g_pVBoxCmMgr.Session, pContext);
        pContext->pDevice = pDevice;
    }
    return hr;
}

HRESULT vboxDispCmSessionCtxDestroy(PVBOXDISPCM_SESSION pSession, PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_CONTEXT pContext)
{
    EnterCriticalSection(&pSession->CritSect);
    Assert(pContext->ContextInfo.hContext);
    D3DDDICB_DESTROYCONTEXT DestroyContext;
    DestroyContext.hContext = pDevice->DefaultContext.ContextInfo.hContext;
    HRESULT hr = pDevice->RtCallbacks.pfnDestroyContextCb(pDevice->hDevice, &DestroyContext);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        vboxDispCmSessionCtxRemoveLocked(pSession, pContext);
    }
    LeaveCriticalSection(&pSession->CritSect);
    return hr;
}

HRESULT vboxDispCmCtxDestroy(PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_CONTEXT pContext)
{
    return vboxDispCmSessionCtxDestroy(&g_pVBoxCmMgr.Session, pDevice, pContext);
}

HRESULT vboxDispCmSessionCmdGet(PVBOXDISPCM_SESSION pSession, void *pvCmd, uint32_t cbCmd, DWORD dwMilliseconds)
{
    Assert(cbCmd >= VBOXDISPIFESCAPE_SIZE(sizeof (VBOXWDDM_GETVBOXVIDEOCMCMD_HDR)));
    if (cbCmd < VBOXDISPIFESCAPE_SIZE(sizeof (VBOXWDDM_GETVBOXVIDEOCMCMD_HDR)))
        return E_INVALIDARG;

    DWORD dwResult = WaitForSingleObject(pSession->hEvent, dwMilliseconds);
    switch(dwResult)
    {
        case WAIT_OBJECT_0:
        {
            PVBOXDISPIFESCAPE pEscape = (PVBOXDISPIFESCAPE)pvCmd;
            HRESULT hr = S_OK;
            D3DDDICB_ESCAPE DdiEscape;
            DdiEscape.Flags.Value = 0;
            DdiEscape.pPrivateDriverData = pvCmd;
            DdiEscape.PrivateDriverDataSize = cbCmd;

            pEscape->escapeCode = VBOXESC_GETVBOXVIDEOCMCMD;
            /* lock to ensure the context is not distructed */
            EnterCriticalSection(&pSession->CritSect);
            /* use any context for identifying the kernel CmSession. We're using the first one */
            PVBOXWDDMDISP_CONTEXT pContext = RTListNodeGetFirst(&pSession->CtxList, VBOXWDDMDISP_CONTEXT, ListNode);
            if (pContext)
            {
                PVBOXWDDMDISP_DEVICE pDevice = pContext->pDevice;
                DdiEscape.hDevice = pDevice->hDevice;
                DdiEscape.hContext = pContext->ContextInfo.hContext;
                Assert (DdiEscape.hContext);
                Assert (DdiEscape.hDevice);
                hr = pDevice->RtCallbacks.pfnEscapeCb(pDevice->pAdapter->hAdapter, &DdiEscape);
                Assert(hr == S_OK);
                LeaveCriticalSection(&pSession->CritSect);
            }
            else
            {
                LeaveCriticalSection(&pSession->CritSect);
                PVBOXWDDM_GETVBOXVIDEOCMCMD_HDR pHdr = VBOXDISPIFESCAPE_DATA(pEscape, VBOXWDDM_GETVBOXVIDEOCMCMD_HDR);
                pHdr->cbCmdsReturned = 0;
                pHdr->cbRemainingCmds = 0;
                pHdr->cbRemainingFirstCmd = 0;
            }

            return hr;
        }
        case WAIT_TIMEOUT:
        {
            PVBOXDISPIFESCAPE pEscape = (PVBOXDISPIFESCAPE)pvCmd;
            PVBOXWDDM_GETVBOXVIDEOCMCMD_HDR pHdr = VBOXDISPIFESCAPE_DATA(pEscape, VBOXWDDM_GETVBOXVIDEOCMCMD_HDR);
            pHdr->cbCmdsReturned = 0;
            pHdr->cbRemainingCmds = 0;
            pHdr->cbRemainingFirstCmd = 0;
            return S_OK;
        }
        default:
            AssertBreakpoint();
            return E_FAIL;
    }
}

HRESULT vboxDispCmCmdGet(void *pvCmd, uint32_t cbCmd, DWORD dwMilliseconds)
{
    return vboxDispCmSessionCmdGet(&g_pVBoxCmMgr.Session, pvCmd, cbCmd, dwMilliseconds);
}
