/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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

DECLCALLBACK(int) vboxUhgsmiBaseEscBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuffer = VBOXUHGSMIESCBASE_GET_BUFFER(pBuf);
    *pvLock = (void*)(pBuffer->Alloc.pvData + offLock);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxUhgsmiBaseEscBufferUnlock(PVBOXUHGSMI_BUFFER pBuf)
{
    return VINF_SUCCESS;
}

int vboxUhgsmiBaseBufferTerm(PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuffer)
{
    PVBOXUHGSMI_PRIVATE_BASE pPrivate = VBOXUHGSMIBASE_GET(pBuffer->PrivateBase.pHgsmi);
    VBOXDISPIFESCAPE_UHGSMI_DEALLOCATE DeallocInfo = {0};
    DeallocInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_DEALLOCATE;
    DeallocInfo.hAlloc = pBuffer->Alloc.hAlloc;
    return vboxCrHgsmiPrivateEscape(pPrivate, &DeallocInfo, sizeof (DeallocInfo), FALSE);
}

static int vboxUhgsmiBaseEventChkCreate(VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType, HANDLE *phSynch)
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

int vboxUhgsmiKmtEscBufferInit(PVBOXUHGSMI_PRIVATE_BASE pPrivate, PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuffer, uint32_t cbBuf, VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType, PFNVBOXUHGSMI_BUFFER_DESTROY pfnDestroy)
{
    HANDLE hSynch = NULL;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    int rc = vboxUhgsmiBaseEventChkCreate(fUhgsmiType, &hSynch);
    if (RT_FAILURE(rc))
    {
        WARN(("vboxUhgsmiBaseEventChkCreate failed, rc %d", rc));
        return rc;
    }

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 12;
    Assert(cPages);

    VBOXDISPIFESCAPE_UHGSMI_ALLOCATE AllocInfo = {0};
    AllocInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_ALLOCATE;
    AllocInfo.Alloc.cbData = cbBuf;
    AllocInfo.Alloc.hSynch = (uint64_t)hSynch;
    AllocInfo.Alloc.fUhgsmiType = fUhgsmiType;

    rc = vboxCrHgsmiPrivateEscape(pPrivate, &AllocInfo, sizeof (AllocInfo), FALSE);
    if (RT_FAILURE(rc))
    {
        if (hSynch)
            CloseHandle(hSynch);
        WARN(("vboxCrHgsmiPrivateEscape failed, rc %d", rc));
        return rc;
    }

    pBuffer->Alloc = AllocInfo.Alloc;
    Assert(pBuffer->Alloc.pvData);
    pBuffer->PrivateBase.pHgsmi = pPrivate;
    pBuffer->PrivateBase.Base.pfnLock = vboxUhgsmiBaseEscBufferLock;
    pBuffer->PrivateBase.Base.pfnUnlock = vboxUhgsmiBaseEscBufferUnlock;
    pBuffer->PrivateBase.Base.pfnDestroy = pfnDestroy;
    pBuffer->PrivateBase.Base.fType = fUhgsmiType;
    pBuffer->PrivateBase.Base.cbBuffer = AllocInfo.Alloc.cbData;
    pBuffer->hSynch = hSynch;
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxUhgsmiBaseEscBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuffer = VBOXUHGSMIESCBASE_GET_BUFFER(pBuf);
    int rc = vboxUhgsmiBaseBufferTerm(pBuffer);
    if (RT_FAILURE(rc))
    {
        WARN(("vboxUhgsmiBaseBufferTerm failed rc %d", rc));
        return rc;
    }

    RTMemFree(pBuffer);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vboxUhgsmiBaseEscBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf, VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType, PVBOXUHGSMI_BUFFER* ppBuf)
{
    *ppBuf = NULL;

    PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuffer = (PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE)RTMemAllocZ(sizeof (*pBuffer));
    if (!pBuffer)
    {
        WARN(("RTMemAllocZ failed"));
        return VERR_NO_MEMORY;
    }

    PVBOXUHGSMI_PRIVATE_BASE pPrivate = VBOXUHGSMIBASE_GET(pHgsmi);
    int rc = vboxUhgsmiKmtEscBufferInit(pPrivate, pBuffer, cbBuf, fUhgsmiType, vboxUhgsmiBaseEscBufferDestroy);
    if (RT_SUCCESS(rc))
    {
        *ppBuf = &pBuffer->PrivateBase.Base;
        return VINF_SUCCESS;
    }

    WARN(("vboxUhgsmiKmtEscBufferInit failed, rc %d", rc));
    RTMemFree(pBuffer);
    return rc;
}

DECLCALLBACK(int) vboxUhgsmiBaseEscBufferSubmit(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    /* we no chromium will not submit more than three buffers actually,
     * for simplicity allocate it statically on the stack  */
    struct
    {
        VBOXDISPIFESCAPE_UHGSMI_SUBMIT SubmitInfo;
        VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE aBufInfos[3];
    } Buf;

    if (!cBuffers || cBuffers > RT_ELEMENTS(Buf.aBufInfos) + 1)
    {
        WARN(("invalid cBuffers!"));
        return VERR_INVALID_PARAMETER;
    }

    HANDLE hSynch = VBOXUHGSMIESCBASE_GET_BUFFER(aBuffers[0].pBuf)->hSynch;
    if (!hSynch)
    {
        WARN(("the fist buffer is not command!"));
        return VERR_INVALID_PARAMETER;
    }

    PVBOXUHGSMI_PRIVATE_BASE pPrivate = VBOXUHGSMIBASE_GET(pHgsmi);
    Buf.SubmitInfo.EscapeHdr.escapeCode = VBOXESC_UHGSMI_SUBMIT;
    Buf.SubmitInfo.EscapeHdr.u32CmdSpecific = cBuffers;
    for (UINT i = 0; i < cBuffers; ++i)
    {
        VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *pSubmInfo = &Buf.SubmitInfo.aBuffers[i];
        PVBOXUHGSMI_BUFFER_SUBMIT pBufInfo = &aBuffers[i];
        PVBOXUHGSMI_BUFFER_PRIVATE_ESC_BASE pBuf = VBOXUHGSMIESCBASE_GET_BUFFER(pBufInfo->pBuf);
        pSubmInfo->hAlloc = pBuf->Alloc.hAlloc;
        pSubmInfo->Info.bDoNotSignalCompletion = 0;
        if (pBufInfo->fFlags.bEntireBuffer)
        {
            pSubmInfo->Info.offData = 0;
            pSubmInfo->Info.cbData = pBuf->PrivateBase.Base.cbBuffer;
        }
        else
        {
            pSubmInfo->Info.offData = pBufInfo->offData;
            pSubmInfo->Info.cbData = pBufInfo->cbData;
        }
    }

    int rc = vboxCrHgsmiPrivateEscape(pPrivate, &Buf.SubmitInfo, RT_OFFSETOF(VBOXDISPIFESCAPE_UHGSMI_SUBMIT, aBuffers[cBuffers]), FALSE);
    if (RT_SUCCESS(rc))
    {
        DWORD dwResult = WaitForSingleObject(hSynch, INFINITE);
        if (dwResult == WAIT_OBJECT_0)
            return VINF_SUCCESS;
        WARN(("wait failed, (0x%x)", dwResult));
        return VERR_GENERAL_FAILURE;
    }
    else
    {
        WARN(("vboxCrHgsmiPrivateEscape failed, rc (%d)", rc));
    }

    return VERR_GENERAL_FAILURE;
}

/* Cr calls have <= 3args, we try to allocate it on stack first */
typedef struct VBOXCRHGSMI_CALLDATA
{
    VBOXDISPIFESCAPE_CRHGSMICTLCON_CALL CallHdr;
    HGCMFunctionParameter aArgs[3];
} VBOXCRHGSMI_CALLDATA, *PVBOXCRHGSMI_CALLDATA;

int vboxCrHgsmiPrivateCtlConCall(struct VBOXUHGSMI_PRIVATE_BASE *pHgsmi, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    VBOXCRHGSMI_CALLDATA Buf;
    PVBOXCRHGSMI_CALLDATA pBuf;
    int cbBuffer = cbCallInfo + RT_OFFSETOF(VBOXCRHGSMI_CALLDATA, CallHdr.CallInfo);

    if (cbBuffer <= sizeof (Buf))
        pBuf = &Buf;
    else
    {
        pBuf = (PVBOXCRHGSMI_CALLDATA)RTMemAlloc(cbBuffer);
        if (!pBuf)
        {
            WARN(("RTMemAlloc failed!"));
            return VERR_NO_MEMORY;
        }
    }

    pBuf->CallHdr.EscapeHdr.escapeCode = VBOXESC_CRHGSMICTLCON_CALL;
    pBuf->CallHdr.EscapeHdr.u32CmdSpecific = 0;
    memcpy(&pBuf->CallHdr.CallInfo, pCallInfo, cbCallInfo);

    int rc = vboxCrHgsmiPrivateEscape(pHgsmi, pBuf, cbBuffer, FALSE);
    if (RT_SUCCESS(rc))
    {
        memcpy(pCallInfo, &pBuf->CallHdr.CallInfo, cbCallInfo);
        rc = VINF_SUCCESS;
    }
    else
    {
        WARN(("vboxCrHgsmiPrivateEscape failed, rc (%d)", rc));
   }
    /* cleanup */
    if (pBuf != &Buf)
        RTMemFree(pBuf);

    return rc;
}

int vboxCrHgsmiPrivateCtlConGetClientID(struct VBOXUHGSMI_PRIVATE_BASE *pHgsmi, uint32_t *pu32ClientID)
{
    VBOXDISPIFESCAPE GetId = {0};
    GetId.escapeCode = VBOXESC_CRHGSMICTLCON_GETCLIENTID;

    int rc = vboxCrHgsmiPrivateEscape(pHgsmi, &GetId, sizeof (GetId), FALSE);
    if (RT_SUCCESS(rc))
    {
        Assert(GetId.u32CmdSpecific);
        *pu32ClientID = GetId.u32CmdSpecific;
        return VINF_SUCCESS;
    }
    else
    {
        *pu32ClientID = 0;
        WARN(("vboxCrHgsmiPrivateEscape failed, rc (%d)", rc));
    }
    return rc;
}
