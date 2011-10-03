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

/* @todo: move this to VBoxDispD3DCmn.h ? */
#   if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#       define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#       define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#       define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#       define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#       pragma warning(disable : 4163)
#       include <windows.h>
#       pragma warning(default : 4163)
#       undef  _InterlockedExchange
#       undef  _InterlockedExchangeAdd
#       undef  _InterlockedCompareExchange
#       undef  _InterlockedAddLargeStatistic
#       undef  _interlockedbittestandset
#       undef  _interlockedbittestandreset
#       undef  _interlockedbittestandset64
#       undef  _interlockedbittestandreset64
#   else
#       include <windows.h>
#   endif

#include "VBoxDispD3DCmn.h"

#include <stdio.h>
#include <stdarg.h>

#include <iprt/asm.h>

#ifdef VBOXWDDMDISP_DEBUG
#define VBOXWDDMDISP_DEBUG_DUMP_DEFAULT 0
DWORD g_VBoxVDbgFDumpSetTexture = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpDrawPrim = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpTexBlt = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpBlt = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpRtSynch = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpFlush = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpShared = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpLock = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFDumpUnlock = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;

DWORD g_VBoxVDbgFBreakShared = VBOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VBoxVDbgFBreakDdi = 0;

DWORD g_VBoxVDbgFLogRel = 0;
DWORD g_VBoxVDbgFLog = 0;
DWORD g_VBoxVDbgFLogFlow = 0;

DWORD g_VBoxVDbgPid = 0;
typedef enum
{
    VBOXDISPDBG_STATE_UNINITIALIZED = 0,
    VBOXDISPDBG_STATE_INITIALIZING,
    VBOXDISPDBG_STATE_INITIALIZED,
} VBOXDISPDBG_STATE;

typedef struct VBOXDISPDBG
{
    VBOXDISPKMT_CALLBACKS KmtCallbacks;
    VBOXDISPDBG_STATE enmState;
} VBOXDISPDBG, *PVBOXDISPDBG;

static VBOXDISPDBG g_VBoxDispDbg = {0};

PVBOXDISPDBG vboxDispDbgGet()
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VBoxDispDbg.enmState, VBOXDISPDBG_STATE_INITIALIZING, VBOXDISPDBG_STATE_UNINITIALIZED))
    {
        HRESULT hr = vboxDispKmtCallbacksInit(&g_VBoxDispDbg.KmtCallbacks);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispDbg.enmState, VBOXDISPDBG_STATE_INITIALIZED);
            return &g_VBoxDispDbg;
        }
        else
        {
            ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispDbg.enmState, VBOXDISPDBG_STATE_UNINITIALIZED);
        }
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VBoxDispDbg.enmState) == VBOXDISPDBG_STATE_INITIALIZED)
    {
        return &g_VBoxDispDbg;
    }
    Assert(0);
    return NULL;
}

void vboxDispLogDrv(char * szString)
{
    PVBOXDISPDBG pDbg = vboxDispDbgGet();
    if (!pDbg)
    {
        /* do not use WARN her esince this would lead to a recursion */
        WARN_BREAK();
        return;
    }

    VBOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vboxDispKmtOpenAdapter(&pDbg->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbString = (uint32_t)strlen(szString) + 1;
        uint32_t cbCmd = RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[cbString]);
        PVBOXDISPIFESCAPE_DBGPRINT pCmd = (PVBOXDISPIFESCAPE_DBGPRINT)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGPRINT;
            memcpy(pCmd->aStringBuf, szString, cbString);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pDbg->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                WARN_BREAK();
            }

            RTMemFree(pCmd);
        }
        else
        {
            WARN_BREAK();
        }
        hr = vboxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            WARN_BREAK();
        }
    }
}

void vboxDispLogDrvF(char * szString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
    va_end(pArgList);

    vboxDispLogDrv(szBuffer);
}

void vboxDispLogDbgPrintF(char * szString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}


VOID vboxVDbgDoDumpSurfRectByAlloc(const char * pPrefix, PVBOXWDDMDISP_ALLOCATION pAlloc, const RECT *pRect, const char* pSuffix)
{
    vboxVDbgDoDumpSurfRectByRc(pPrefix, pAlloc->pRc, pAlloc->iAlloc, pRect, pSuffix);
}

VOID vboxVDbgDoPrintDumpCmd(const void *pvData, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch)
{
    vboxVDbgPrint(("<?dml?><exec cmd=\"!vbvdbg.ms 0x%p 0n%d 0n%d 0n%d 0n%d\">surface info</exec>, ( !vbvdbg.ms 0x%p 0n%d 0n%d 0n%d 0n%d )\n",
            pvData, width, height, bpp, pitch,
            pvData, width, height, bpp, pitch));
}

VOID vboxVDbgDoDumpAllocRect(const char * pPrefix, PVBOXWDDMDISP_ALLOCATION pAlloc, const RECT *pRect, const char* pSuffix)
{
    if (pPrefix)
    {
        vboxVDbgPrint(("%s", pPrefix));
    }

    Assert(pAlloc->hAllocation);

    HANDLE hSharedHandle = pAlloc->hSharedHandle;

    vboxVDbgPrint(("SharedHandle: (0x%p)\n", hSharedHandle));

    D3DDDICB_LOCK LockData;
    LockData.hAllocation = pAlloc->hAllocation;
    LockData.PrivateDriverData = 0;
    LockData.NumPages = 0;
    LockData.pPages = NULL;
    LockData.pData = NULL; /* out */
    LockData.Flags.Value = 0;
    LockData.Flags.LockEntire =1;
    LockData.Flags.ReadOnly = 1;

    PVBOXWDDMDISP_DEVICE pDevice = pAlloc->pRc->pDevice;

    HRESULT hr = pDevice->RtCallbacks.pfnLockCb(pDevice->hDevice, &LockData);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        UINT bpp = vboxWddmCalcBitsPerPixel(pAlloc->SurfDesc.format);
        vboxVDbgDoPrintDumpCmd(LockData.pData, pAlloc->D3DWidth, pAlloc->SurfDesc.height, bpp, pAlloc->SurfDesc.pitch);
        if (pRect)
        {
            Assert(pRect->right > pRect->left);
            Assert(pRect->bottom > pRect->top);
            vboxVDbgDoPrintRect("rect: ", pRect, "\n");
            vboxVDbgDoPrintDumpCmd(((uint8_t*)LockData.pData) + (pRect->top * pAlloc->SurfDesc.pitch) + ((pRect->left * bpp) >> 3),
                    pRect->right - pRect->left, pRect->bottom - pRect->top, bpp, pAlloc->SurfDesc.pitch);
        }
        Assert(0);

        D3DDDICB_UNLOCK DdiUnlock;

        DdiUnlock.NumAllocations = 1;
        DdiUnlock.phAllocations = &pAlloc->hAllocation;

        hr = pDevice->RtCallbacks.pfnUnlockCb(pDevice->hDevice, &DdiUnlock);
        Assert(hr == S_OK);
    }
    if (pSuffix)
    {
        vboxVDbgPrint(("%s\n", pSuffix));
    }
}


VOID vboxVDbgDoDumpSurfRectByRc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const RECT *pRect, const char* pSuffix)
{
    Assert(pRc->cAllocations > iAlloc);
    BOOL bReleaseSurf = false;
    IDirect3DSurface9 *pSurf;
    HRESULT hr = vboxWddmSurfGet(pRc, iAlloc, &pSurf);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        vboxVDbgDoDumpSurfRect(pPrefix, pSurf, pRect, pSuffix, true);
        pSurf->Release();
    }
}

VOID vboxVDbgDoDumpSurfRect(const char * pPrefix, IDirect3DSurface9 *pSurf, const RECT *pRect, const char * pSuffix, bool bBreak)
{
    if (pPrefix)
    {
        vboxVDbgPrint(("%s", pPrefix));
    }

    D3DSURFACE_DESC Desc;
    HRESULT hr = pSurf->GetDesc(&Desc);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        D3DLOCKED_RECT Lr;
        hr = pSurf->LockRect(&Lr, NULL, D3DLOCK_READONLY);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            UINT bpp = vboxWddmCalcBitsPerPixel((D3DDDIFORMAT)Desc.Format);
            vboxVDbgDoPrintDumpCmd(Lr.pBits, Desc.Width, Desc.Height, bpp, Lr.Pitch);
            if (pRect)
            {
                Assert(pRect->right > pRect->left);
                Assert(pRect->bottom > pRect->top);
                vboxVDbgDoPrintRect("rect: ", pRect, "\n");
                vboxVDbgDoPrintDumpCmd(((uint8_t*)Lr.pBits) + (pRect->top * Lr.Pitch) + ((pRect->left * bpp) >> 3),
                        pRect->right - pRect->left, pRect->bottom - pRect->top, bpp, Lr.Pitch);
            }

            if (bBreak)
            {
                Assert(0);

                hr = pSurf->UnlockRect();
                Assert(hr == S_OK);
            }
        }
    }

    if (pSuffix)
    {
        vboxVDbgPrint(("%s", pSuffix));
    }
}

VOID vboxVDbgDoDumpSurf(const char * pPrefix, IDirect3DSurface9 *pSurf, const char * pSuffix)
{
    vboxVDbgDoDumpSurfRect(pPrefix, pSurf, NULL, pSuffix, true);
}

#define VBOXVDBG_STRCASE(_t) \
        case _t: return #_t;
#define VBOXVDBG_STRCASE_UNKNOWN() \
        default: Assert(0); return "Unknown";

const char* vboxVDbgStrCubeFaceType(D3DCUBEMAP_FACES enmFace)
{
    switch (enmFace)
    {
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_X);
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_X);
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_Y);
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_Y);
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_Z);
    VBOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_Z);
    VBOXVDBG_STRCASE_UNKNOWN();
    }
}

VOID vboxVDbgDoDumpRcRect(const char * pPrefix, IDirect3DResource9 *pRc, const RECT *pRect, const char * pSuffix)
{
    if (pPrefix)
    {
        vboxVDbgPrint(("%s", pPrefix));
    }

    switch (pRc->GetType())
    {
        case D3DRTYPE_TEXTURE:
        {
            vboxVDbgPrint(("this is a texture\n"));

            IDirect3DTexture9 *pTex = (IDirect3DTexture9*)pRc;
            IDirect3DSurface9 *pSurf;
            HRESULT hr = pTex->GetSurfaceLevel(0, &pSurf);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                vboxVDbgDoDumpSurfRect("", pSurf, pRect, "\n", true);
                pSurf->Release();
            }
            break;
        }
        case D3DRTYPE_CUBETEXTURE:
        {
            vboxVDbgPrint(("this is a cube texture\n"));

            IDirect3DCubeTexture9 *pCubeTex = (IDirect3DCubeTexture9*)pRc;
            IDirect3DSurface9 *apSurf[6] = {0};
            for (UINT i = D3DCUBEMAP_FACE_POSITIVE_X; i < D3DCUBEMAP_FACE_POSITIVE_X + 6; ++i)
            {
                vboxVDbgPrint(("face %s: ", vboxVDbgStrCubeFaceType((D3DCUBEMAP_FACES)i)));

                HRESULT hr = pCubeTex->GetCubeMapSurface((D3DCUBEMAP_FACES)i, 0, &apSurf[i]);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    vboxVDbgDoDumpSurfRect("", apSurf[i], pRect, "\n", false);
                }
                else
                {
                    Assert(0);
                }
            }

            Assert(0);

            for (UINT i = D3DCUBEMAP_FACE_POSITIVE_X; i < D3DCUBEMAP_FACE_POSITIVE_X + 6; ++i)
            {
                apSurf[i]->UnlockRect();
                apSurf[i]->Release();
            }

            break;
        }
        case D3DRTYPE_SURFACE:
        {
            vboxVDbgPrint(("this is a surface\n"));
            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)pRc;
            vboxVDbgDoDumpSurfRect("", pSurf, pRect, "\n", true);
            break;
        }
        default:
            vboxVDbgPrint(("unsupported rc type\n"));
            Assert(0);
    }

    if (pSuffix)
    {
        vboxVDbgPrint(("%s", pSuffix));
    }
}

VOID vboxVDbgDoDumpRcRectByAlloc(const char * pPrefix, const PVBOXWDDMDISP_ALLOCATION pAlloc, IDirect3DResource9 *pD3DIf, const RECT *pRect, const char* pSuffix)
{
    if (pPrefix)
        vboxVDbgPrint(("%s", pPrefix));

    if (!pD3DIf)
    {
        pD3DIf = (IDirect3DResource9*)pAlloc->pD3DIf;
    }

    vboxVDbgPrint(("Rc(0x%p), pAlloc(0x%x), pD3DIf(0x%p), SharedHandle(0x%p)\n", pAlloc->pRc, pAlloc, pD3DIf, pAlloc->pRc->aAllocations[0].hSharedHandle));

    vboxVDbgDoDumpRcRect("", pD3DIf, pRect, pSuffix);
}

VOID vboxVDbgDoDumpRcRectByRc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, const RECT *pRect, const char* pSuffix)
{
    vboxVDbgDoDumpRcRectByAlloc(pPrefix, &pRc->aAllocations[0], NULL, pRect, pSuffix);
}

VOID vboxVDbgDoDumpTex(const char * pPrefix, IDirect3DBaseTexture9 *pTexBase, const char * pSuffix)
{
    vboxVDbgDoDumpRcRect(pPrefix, pTexBase, NULL, pSuffix);
}

VOID vboxVDbgDoDumpRt(const char * pPrefix, PVBOXWDDMDISP_DEVICE pDevice, const char * pSuffix)
{
    for (UINT i = 0; i < pDevice->cRTs; ++i)
    {
        IDirect3DSurface9 *pRt;
        PVBOXWDDMDISP_ALLOCATION pAlloc = pDevice->apRTs[i];
        IDirect3DDevice9 *pDeviceIf = pDevice->pDevice9If;
        HRESULT hr = pDeviceIf->GetRenderTarget(i, &pRt);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
//            Assert(pAlloc->pD3DIf == pRt);
            vboxVDbgDoDumpRcRectByAlloc(pPrefix, pAlloc, NULL, NULL, "\n");
            pRt->Release();
        }
        else
        {
            vboxVDbgPrint((__FUNCTION__": ERROR getting rt: 0x%x", hr));
        }
    }
}

VOID vboxVDbgDoDumpLockUnlockSurfTex(const char * pPrefix, const PVBOXWDDMDISP_ALLOCATION pAlloc, const char * pSuffix, bool fBreak)
{
    if (pPrefix)
    {
        vboxVDbgPrint(("%s", pPrefix));
    }

    Assert(!pAlloc->hSharedHandle);

    UINT bpp = vboxWddmCalcBitsPerPixel(pAlloc->SurfDesc.format);
    uint32_t width, height, pitch;
    RECT Rect, *pRect;
    void *pvData;
    Assert(!pAlloc->LockInfo.fFlags.RangeValid);
    Assert(!pAlloc->LockInfo.fFlags.BoxValid);
    if (pAlloc->LockInfo.fFlags.AreaValid)
    {
        pRect = &pAlloc->LockInfo.Area;
        width = pAlloc->LockInfo.Area.left - pAlloc->LockInfo.Area.right;
        height = pAlloc->LockInfo.Area.bottom - pAlloc->LockInfo.Area.top;
    }
    else
    {
        width = pAlloc->SurfDesc.width;
        height = pAlloc->SurfDesc.height;
        Rect.top = 0;
        Rect.bottom = height;
        Rect.left = 0;
        Rect.right = width;
        pRect = &Rect;
    }

    if (pAlloc->LockInfo.fFlags.NotifyOnly)
    {
        pitch = pAlloc->SurfDesc.pitch;
        pvData = ((uint8_t*)pAlloc->pvMem) + pitch*pRect->top + ((bpp*pRect->left) >> 3);
    }
    else
    {
        pvData = pAlloc->LockInfo.pvData;
    }

    vboxVDbgPrint(("pRc(0x%p) iAlloc(%d), type(%d), cLocks(%d)\n", pAlloc->pRc, pAlloc->iAlloc, pAlloc->enmD3DIfType, pAlloc->LockInfo.cLocks));

    vboxVDbgDoPrintDumpCmd(pvData, width, height, bpp, pitch);

    if (fBreak)
    {
        Assert(0);
    }

    if (pSuffix)
    {
        vboxVDbgPrint(("%s", pSuffix));
    }

}

VOID vboxVDbgDoDumpLockSurfTex(const char * pPrefix, const D3DDDIARG_LOCK* pData, const char * pSuffix, bool fBreak)
{
    const PVBOXWDDMDISP_RESOURCE pRc = (const PVBOXWDDMDISP_RESOURCE)pData->hResource;
    const PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    pAlloc->LockInfo.pvData = pData->pSurfData;
    vboxVDbgDoDumpLockUnlockSurfTex(pPrefix, pAlloc, pSuffix, fBreak);
}

VOID vboxVDbgDoDumpUnlockSurfTex(const char * pPrefix, const D3DDDIARG_UNLOCK* pData, const char * pSuffix, bool fBreak)
{
    const PVBOXWDDMDISP_RESOURCE pRc = (const PVBOXWDDMDISP_RESOURCE)pData->hResource;
    const PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    vboxVDbgDoDumpLockUnlockSurfTex(pPrefix, pAlloc, pSuffix, fBreak);
}

void vboxVDbgDoPrintAlloc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const char * pSuffix)
{
    Assert(pRc->cAllocations > iAlloc);
    const PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[iAlloc];
    BOOL bPrimary = pRc->RcDesc.fFlags.Primary;
    BOOL bFrontBuf = FALSE;
    if (bPrimary)
    {
        PVBOXWDDMDISP_SWAPCHAIN pSwapchain = vboxWddmSwapchainForAlloc(pAlloc);
        Assert(pSwapchain);
        bFrontBuf = (vboxWddmSwapchainGetFb(pSwapchain)->pAlloc == pAlloc);
    }
    vboxVDbgPrint(("%s D3DWidth(%d), width(%d), height(%d), format(%d), usage(%s), %s", pPrefix,
            pAlloc->D3DWidth, pAlloc->SurfDesc.width, pAlloc->SurfDesc.height, pAlloc->SurfDesc.format,
            bPrimary ?
                    (bFrontBuf ? "Front Buffer" : "Back Buffer")
                    : "?Everage? Alloc",
            pSuffix));
}

void vboxVDbgDoPrintRect(const char * pPrefix, const RECT *pRect, const char * pSuffix)
{
    vboxVDbgPrint(("%s left(%d), top(%d), right(%d), bottom(%d) %s", pPrefix, pRect->left, pRect->top, pRect->right, pRect->bottom, pSuffix));
}
#endif

#ifdef VBOXWDDMDISP_DEBUG_VEHANDLER

static PVOID g_VBoxWDbgVEHandler = NULL;
LONG WINAPI vboxVDbgVectoredHandler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
    PCONTEXT pContextRecord = pExceptionInfo->ContextRecord;
    switch (pExceptionRecord->ExceptionCode)
    {
        case 0xc0000005: /* only access violation and debug exceptions actually matter */
        case 0xc0000003:
            AssertRelease(0);
            break;
        default:
            break;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void vboxVDbgVEHandlerRegister()
{
    Assert(!g_VBoxWDbgVEHandler);
    g_VBoxWDbgVEHandler = AddVectoredExceptionHandler(1,vboxVDbgVectoredHandler);
    Assert(g_VBoxWDbgVEHandler);
}

void vboxVDbgVEHandlerUnregister()
{
    Assert(g_VBoxWDbgVEHandler);
    ULONG uResult = RemoveVectoredExceptionHandler(g_VBoxWDbgVEHandler);
    Assert(uResult);
    g_VBoxWDbgVEHandler = NULL;
}

#endif
