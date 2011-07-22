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
#include "VBoxDispMp.h"

#include <iprt/assert.h>

typedef struct VBOXVIDEOCM_ITERATOR
{
    PVBOXVIDEOCM_CMD_HDR pCur;
    uint32_t cbRemain;
} VBOXVIDEOCM_ITERATOR, *PVBOXVIDEOCM_ITERATOR;

typedef struct VBOXDISPMP
{
    CRITICAL_SECTION CritSect;
    PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD pEscapeCmd;
    uint32_t cbEscapeCmd;
    VBOXVIDEOCM_ITERATOR Iterator;
} VBOXDISPMP, *PVBOXDISPMP;

DECLINLINE(void) vboxVideoCmIterInit(PVBOXVIDEOCM_ITERATOR pIter, PVBOXVIDEOCM_CMD_HDR pStart, uint32_t cbCmds)
{
    pIter->pCur = pStart;
    pIter->cbRemain= cbCmds;
}

DECLINLINE(PVBOXVIDEOCM_CMD_HDR) vboxVideoCmIterNext(PVBOXVIDEOCM_ITERATOR pIter)
{
    if (pIter->cbRemain)
    {
        PVBOXVIDEOCM_CMD_HDR pCur = pIter->pCur;
        Assert(pIter->cbRemain  >= pIter->pCur->cbCmd);
        pIter->cbRemain -= pIter->pCur->cbCmd;
        pIter->pCur = (PVBOXVIDEOCM_CMD_HDR)(((uint8_t*)pIter->pCur) + pIter->pCur->cbCmd);
        return pCur;
    }
    return NULL;
}

DECLINLINE(VOID) vboxVideoCmIterCopyToBack(PVBOXVIDEOCM_ITERATOR pIter, PVBOXVIDEOCM_CMD_HDR pCur)
{
    memcpy((((uint8_t*)pIter->pCur) + pIter->cbRemain), pCur, pCur->cbCmd);
    pIter->cbRemain += pCur->cbCmd;
}

DECLINLINE(bool) vboxVideoCmIterHasNext(PVBOXVIDEOCM_ITERATOR pIter)
{
    return !!(pIter->cbRemain);
}

static VBOXDISPMP g_VBoxDispMp;

DECLCALLBACK(HRESULT) vboxDispMpEnableEvents()
{
    EnterCriticalSection(&g_VBoxDispMp.CritSect);
    g_VBoxDispMp.pEscapeCmd = NULL;
    g_VBoxDispMp.cbEscapeCmd = 0;
    vboxVideoCmIterInit(&g_VBoxDispMp.Iterator, NULL, 0);
#ifdef VBOX_WITH_CRHGSMI
    vboxUhgsmiGlobalSetCurrent();
#endif
    LeaveCriticalSection(&g_VBoxDispMp.CritSect);
    return S_OK;
}


DECLCALLBACK(HRESULT) vboxDispMpDisableEvents()
{
    EnterCriticalSection(&g_VBoxDispMp.CritSect);
    if (g_VBoxDispMp.pEscapeCmd)
    {
        RTMemFree(g_VBoxDispMp.pEscapeCmd);
        g_VBoxDispMp.pEscapeCmd = NULL;
    }
#ifdef VBOX_WITH_CRHGSMI
    vboxUhgsmiGlobalClearCurrent();
#endif
    LeaveCriticalSection(&g_VBoxDispMp.CritSect);
    return S_OK;
}

#define VBOXDISPMP_BUF_INITSIZE 4000
#define VBOXDISPMP_BUF_INCREASE 4096
#define VBOXDISPMP_BUF_MAXSIZE  ((4096*4096)-96)

DECLCALLBACK(HRESULT) vboxDispMpGetRegions(PVBOXDISPMP_REGIONS pRegions, DWORD dwMilliseconds)
{
    HRESULT hr = S_OK;
    EnterCriticalSection(&g_VBoxDispMp.CritSect);
    PVBOXVIDEOCM_CMD_HDR pHdr = vboxVideoCmIterNext(&g_VBoxDispMp.Iterator);
    if (!pHdr)
    {
        if (!g_VBoxDispMp.pEscapeCmd)
        {
            g_VBoxDispMp.pEscapeCmd = (PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD)RTMemAlloc(VBOXDISPMP_BUF_INITSIZE);
            Assert(g_VBoxDispMp.pEscapeCmd);
            if (g_VBoxDispMp.pEscapeCmd)
                g_VBoxDispMp.cbEscapeCmd = VBOXDISPMP_BUF_INITSIZE;
            else
            {
                LeaveCriticalSection(&g_VBoxDispMp.CritSect);
                return E_OUTOFMEMORY;
            }
        }

        do
        {
            hr =  vboxDispCmCmdGet(g_VBoxDispMp.pEscapeCmd, g_VBoxDispMp.cbEscapeCmd, dwMilliseconds);
            Assert(hr == S_OK || (dwMilliseconds != INFINITE && hr == WAIT_TIMEOUT));
            if (hr == S_OK)
            {
                if (g_VBoxDispMp.pEscapeCmd->Hdr.cbCmdsReturned)
                {
                    pHdr = (PVBOXVIDEOCM_CMD_HDR)(((uint8_t*)g_VBoxDispMp.pEscapeCmd) + sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD));
                    vboxVideoCmIterInit(&g_VBoxDispMp.Iterator, pHdr, g_VBoxDispMp.pEscapeCmd->Hdr.cbCmdsReturned);
                    pHdr = vboxVideoCmIterNext(&g_VBoxDispMp.Iterator);
                    Assert(pHdr);
                    break;
                }
                else
                {
                    Assert(g_VBoxDispMp.pEscapeCmd->Hdr.cbRemainingCmds);
                    Assert(g_VBoxDispMp.pEscapeCmd->Hdr.cbRemainingFirstCmd);
                    RTMemFree(g_VBoxDispMp.pEscapeCmd);
                    uint32_t newSize = RT_MAX(g_VBoxDispMp.cbEscapeCmd + VBOXDISPMP_BUF_INCREASE, g_VBoxDispMp.pEscapeCmd->Hdr.cbRemainingFirstCmd);
                    if (newSize < VBOXDISPMP_BUF_MAXSIZE)
                        newSize = RT_MAX(newSize, RT_MIN(g_VBoxDispMp.pEscapeCmd->Hdr.cbRemainingCmds, VBOXDISPMP_BUF_MAXSIZE));
                    Assert(g_VBoxDispMp.cbEscapeCmd < newSize);
                    g_VBoxDispMp.pEscapeCmd = (PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD)RTMemAlloc(newSize);
                    Assert(g_VBoxDispMp.pEscapeCmd);
                    if (g_VBoxDispMp.pEscapeCmd)
                        g_VBoxDispMp.cbEscapeCmd = newSize;
                    else
                    {
                        g_VBoxDispMp.pEscapeCmd = NULL;
                        g_VBoxDispMp.cbEscapeCmd = 0;
                        hr = E_OUTOFMEMORY;
                        break;
                    }
                }
            }
            else
                break;
        } while (1);
    }

    if (hr == S_OK)
    {
        Assert(pHdr);
        VBOXWDDMDISP_CONTEXT *pContext = (VBOXWDDMDISP_CONTEXT*)pHdr->u64UmData;
        PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)(((uint8_t*)pHdr) + sizeof (VBOXVIDEOCM_CMD_HDR));
        PVBOXWDDMDISP_SWAPCHAIN pSwapchain = (PVBOXWDDMDISP_SWAPCHAIN)pCmdInternal->hSwapchainUm;
        /* the miniport driver should ensure all swapchain-involved commands are completed before swapchain termination,
         * so we should have it always valid here */
        Assert(pSwapchain);
        Assert(pSwapchain->hWnd);
        pRegions->hWnd = pSwapchain->hWnd;
        pRegions->pRegions = &pCmdInternal->Cmd;
    }
    LeaveCriticalSection(&g_VBoxDispMp.CritSect);
    return hr;
}

static DECLCALLBACK(void) vboxDispMpLog(LPCSTR pszMsg)
{
    vboxDispCmLog(pszMsg);
}

VBOXDISPMP_DECL(HRESULT) VBoxDispMpGetCallbacks(uint32_t u32Version, PVBOXDISPMP_CALLBACKS pCallbacks)
{
    Assert(u32Version == VBOXDISPMP_VERSION);
    if (u32Version != VBOXDISPMP_VERSION)
        return E_INVALIDARG;

    pCallbacks->pfnEnableEvents = vboxDispMpEnableEvents;
    pCallbacks->pfnDisableEvents = vboxDispMpDisableEvents;
    pCallbacks->pfnGetRegions = vboxDispMpGetRegions;
    pCallbacks->pfnLog = vboxDispMpLog;
    return S_OK;
}

HRESULT vboxDispMpInternalInit()
{
    memset(&g_VBoxDispMp, 0, sizeof (g_VBoxDispMp));
    InitializeCriticalSection(&g_VBoxDispMp.CritSect);
    return S_OK;
}

HRESULT vboxDispMpInternalTerm()
{
    vboxDispMpDisableEvents();
    DeleteCriticalSection(&g_VBoxDispMp.CritSect);
    return S_OK;
}

HRESULT vboxDispMpInternalCancel(VBOXWDDMDISP_CONTEXT *pContext, PVBOXWDDMDISP_SWAPCHAIN pSwapchain)
{
    HRESULT hr = S_OK;
    EnterCriticalSection(&g_VBoxDispMp.CritSect);
    do
    {
        /* the pEscapeCmd is used as an indicator to whether the events capturing is active */
        if (!g_VBoxDispMp.pEscapeCmd)
            break;

        /* copy the iterator data to restore it back later */
        VBOXVIDEOCM_ITERATOR IterCopy = g_VBoxDispMp.Iterator;

        /* first check if we have matching elements */
        PVBOXVIDEOCM_CMD_HDR pHdr;
        bool fHasMatch = false;
        while (pHdr = vboxVideoCmIterNext(&g_VBoxDispMp.Iterator))
        {
            VBOXWDDMDISP_CONTEXT *pCurContext = (VBOXWDDMDISP_CONTEXT*)pHdr->u64UmData;
            if (pCurContext != pContext)
                continue;

            if (!pSwapchain)
            {
                fHasMatch = true;
                break;
            }

            PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)(((uint8_t*)pHdr) + sizeof (VBOXVIDEOCM_CMD_HDR));
            PVBOXWDDMDISP_SWAPCHAIN pCurSwapchain = (PVBOXWDDMDISP_SWAPCHAIN)pCmdInternal->hSwapchainUm;
            if (pCurSwapchain != pSwapchain)
                continue;

            fHasMatch = true;
            break;
        }

        /* restore the iterator */
        g_VBoxDispMp.Iterator = IterCopy;

        if (!fHasMatch)
            break;

        /* there are elements to remove */
        PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD pEscapeCmd = (PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD)RTMemAlloc(g_VBoxDispMp.cbEscapeCmd);
        if (!pEscapeCmd)
        {
            WARN(("no memory"));
            hr = E_OUTOFMEMORY;
            break;
        }
        /* copy the header data */
        *pEscapeCmd = *g_VBoxDispMp.pEscapeCmd;
        /* now copy the command data filtering out the canceled commands */
        pHdr = (PVBOXVIDEOCM_CMD_HDR)(((uint8_t*)pEscapeCmd) + sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD));
        vboxVideoCmIterInit(&g_VBoxDispMp.Iterator, pHdr, 0);
        while (pHdr = vboxVideoCmIterNext(&IterCopy))
        {
            VBOXWDDMDISP_CONTEXT *pCurContext = (VBOXWDDMDISP_CONTEXT*)pHdr->u64UmData;
            if (pCurContext != pContext)
            {
                vboxVideoCmIterCopyToBack(&g_VBoxDispMp.Iterator, pHdr);
                continue;
            }

            if (!pSwapchain)
            {
                /* match, just continue */
                continue;
            }

            PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)(((uint8_t*)pHdr) + sizeof (VBOXVIDEOCM_CMD_HDR));
            PVBOXWDDMDISP_SWAPCHAIN pCurSwapchain = (PVBOXWDDMDISP_SWAPCHAIN)pCmdInternal->hSwapchainUm;
            if (pCurSwapchain != pSwapchain)
            {
                vboxVideoCmIterCopyToBack(&g_VBoxDispMp.Iterator, pHdr);
                continue;
            }
            /* match, just continue */
        }

        Assert(g_VBoxDispMp.pEscapeCmd);
        RTMemFree(g_VBoxDispMp.pEscapeCmd);
        Assert(pEscapeCmd);
        g_VBoxDispMp.pEscapeCmd = pEscapeCmd;
    } while (0);

    LeaveCriticalSection(&g_VBoxDispMp.CritSect);
    return hr;
}
