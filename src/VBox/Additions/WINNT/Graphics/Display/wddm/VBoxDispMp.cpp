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
#include "VBoxDispMp.h"

#include <iprt/assert.h>

typedef struct VBOXVIDEOCM_ITERATOR
{
    PVBOXVIDEOCM_CMD_HDR pCur;
    uint32_t cbRemain;
} VBOXVIDEOCM_ITERATOR, *PVBOXVIDEOCM_ITERATOR;

typedef struct VBOXDISPMP
{
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

DECLINLINE(bool) vboxVideoCmIterHasNext(PVBOXVIDEOCM_ITERATOR pIter)
{
    return !!(pIter->cbRemain);
}

static VBOXDISPMP g_VBoxDispMp;

DECLCALLBACK(HRESULT) vboxDispMpEnableEvents()
{
    g_VBoxDispMp.pEscapeCmd = NULL;
    g_VBoxDispMp.cbEscapeCmd = 0;
    vboxVideoCmIterInit(&g_VBoxDispMp.Iterator, NULL, 0);
    return S_OK;
}


DECLCALLBACK(HRESULT) vboxDispMpDisableEvents()
{
    if (g_VBoxDispMp.pEscapeCmd)
        RTMemFree(g_VBoxDispMp.pEscapeCmd);
    return S_OK;
}

#define VBOXDISPMP_BUF_INITSIZE 4000
#define VBOXDISPMP_BUF_INCREASE 4096
#define VBOXDISPMP_BUF_MAXSIZE  ((4096*4096)-96)

DECLCALLBACK(HRESULT) vboxDispMpGetRegions(PVBOXDISPMP_REGIONS pRegions, DWORD dwMilliseconds)
{
    HRESULT hr = S_OK;
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
                return E_OUTOFMEMORY;
        }

        do
        {
            hr =  vboxDispCmCmdGet(g_VBoxDispMp.pEscapeCmd, g_VBoxDispMp.cbEscapeCmd, dwMilliseconds);
            Assert(hr == S_OK);
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
        Assert(pContext->pDevice->hWnd);
        pRegions->hWnd = pContext->pDevice->hWnd;
        pRegions->pRegions = (PVBOXVIDEOCM_CMD_RECTS)(((uint8_t*)pHdr) + sizeof (VBOXVIDEOCM_CMD_HDR));
    }
    return hr;
}

VBOXDISPMP_DECL(HRESULT) VBoxDispMpGetCallbacks(uint32_t u32Version, PVBOXDISPMP_CALLBACKS pCallbacks)
{
    Assert(u32Version == VBOXDISPMP_VERSION);
    if (u32Version != VBOXDISPMP_VERSION)
        return E_INVALIDARG;

    pCallbacks->pfnEnableEvents = vboxDispMpEnableEvents;
    pCallbacks->pfnDisableEvents = vboxDispMpDisableEvents;
    pCallbacks->pfnGetRegions = vboxDispMpGetRegions;
    return S_OK;
}
