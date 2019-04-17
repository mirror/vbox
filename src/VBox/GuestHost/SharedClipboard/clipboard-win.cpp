/* $Id$ */
/** @file
 * Shared Clipboard: Windows-specific functions for clipboard handling.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/ldr.h>
#include <iprt/thread.h>

#include <VBox/log.h>
#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#include <VBox/GuestHost/clipboard-helper.h>

/**
 * Opens the clipboard of a specific window.
 *
 * @returns VBox status code.
 * @param   hWnd                Handle of window to open clipboard for.
 */
int VBoxClipboardWinOpen(HWND hWnd)
{
    /* "OpenClipboard fails if another window has the clipboard open."
     * So try a few times and wait up to 1 second.
     */
    BOOL fOpened = FALSE;

    LogFlowFunc(("hWnd=%p\n", hWnd));

    int i = 0;
    for (;;)
    {
        if (OpenClipboard(hWnd))
        {
            fOpened = TRUE;
            break;
        }

        if (i >= 10) /* sleep interval = [1..512] ms */
            break;

        RTThreadSleep(1 << i);
        ++i;
    }

#ifdef LOG_ENABLED
    if (i > 0)
        LogFlowFunc(("%d times tried to open clipboard\n", i + 1));
#endif

    int rc;
    if (fOpened)
        rc = VINF_SUCCESS;
    else
    {
        const DWORD dwLastErr = GetLastError();
        rc = RTErrConvertFromWin32(dwLastErr);
        LogFunc(("Failed to open clipboard, rc=%Rrc (0x%x)\n", rc, dwLastErr));
    }

    return rc;
}

/**
 * Closes the clipboard for the current thread.
 *
 * @returns VBox status code.
 */
int VBoxClipboardWinClose(void)
{
    int rc;

    const BOOL fRc = CloseClipboard();
    if (RT_UNLIKELY(fRc))
    {
        const DWORD dwLastErr = GetLastError();
        rc = RTErrConvertFromWin32(dwLastErr);
        LogFunc(("Failed with %Rrc (0x%x)\n", rc, dwLastErr));
    }
    else
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Clears the clipboard for the current thread.
 *
 * @returns VBox status code.
 */
int VBoxClipboardWinClear(void)
{
    int rc;

    const BOOL fRc = EmptyClipboard();
    if (RT_UNLIKELY(fRc))
    {
        const DWORD dwLastErr = GetLastError();
        rc = RTErrConvertFromWin32(dwLastErr);
        LogFunc(("Failed with %Rrc (0x%x)\n", rc, dwLastErr));
    }
    else
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Checks and initializes function pointer which are required for using
 * the new clipboard API.
 *
 * @returns VBox status code.
 * @param   pAPI                Where to store the retrieved function pointers.
 *                              Will be set to NULL if the new API is not available.
 */
int VBoxClipboardWinCheckAndInitNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI)
{
    RTLDRMOD hUser32 = NIL_RTLDRMOD;
    int rc = RTLdrLoadSystem("User32.dll", /* fNoUnload = */ true, &hUser32);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hUser32, "AddClipboardFormatListener", (void **)&pAPI->pfnAddClipboardFormatListener);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hUser32, "RemoveClipboardFormatListener", (void **)&pAPI->pfnRemoveClipboardFormatListener);
        }

        RTLdrClose(hUser32);
    }

    if (RT_SUCCESS(rc))
    {
        LogFunc(("New Clipboard API enabled\n"));
    }
    else
    {
        RT_BZERO(pAPI, sizeof(VBOXCLIPBOARDWINAPINEW));
        LogFunc(("New Clipboard API not available; rc=%Rrc\n", rc));
    }

    return rc;
}

/**
 * Returns if the new clipboard API is available or not.
 *
 * @returns @c true if the new API is available, or @c false if not.
 * @param   pAPI                Structure used for checking if the new clipboard API is available or not.
 */
bool VBoxClipboardWinIsNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI)
{
    if (!pAPI)
        return false;
    return pAPI->pfnAddClipboardFormatListener != NULL;
}

/**
 * Adds ourselves into the chain of cliboard listeners.
 *
 * @returns VBox status code.
 * @param   pCtx                Windows clipboard context to use to add ourselves.
 */
int VBoxClipboardWinAddToCBChain(PVBOXCLIPBOARDWINCTX pCtx)
{
    const PVBOXCLIPBOARDWINAPINEW pAPI = &pCtx->newAPI;

    BOOL fRc;
    if (VBoxClipboardWinIsNewAPI(pAPI))
    {
        fRc = pAPI->pfnAddClipboardFormatListener(pCtx->hWnd);
    }
    else
    {
        pCtx->hWndNextInChain = SetClipboardViewer(pCtx->hWnd);
        fRc = pCtx->hWndNextInChain != NULL;
    }

    int rc = VINF_SUCCESS;

    if (!fRc)
    {
        const DWORD dwLastErr = GetLastError();
        rc = RTErrConvertFromWin32(dwLastErr);
        LogFunc(("Failed with %Rrc (0x%x)\n", rc, dwLastErr));
    }

    return rc;
}

/**
 * Remove ourselves from the chain of cliboard listeners
 *
 * @returns VBox status code.
 * @param   pCtx                Windows clipboard context to use to remove ourselves.
 */
int VBoxClipboardWinRemoveFromCBChain(PVBOXCLIPBOARDWINCTX pCtx)
{
    if (!pCtx->hWnd)
        return VINF_SUCCESS;

    const PVBOXCLIPBOARDWINAPINEW pAPI = &pCtx->newAPI;

    BOOL fRc;
    if (VBoxClipboardWinIsNewAPI(pAPI))
    {
        fRc = pAPI->pfnRemoveClipboardFormatListener(pCtx->hWnd);
    }
    else
    {
        fRc = ChangeClipboardChain(pCtx->hWnd, pCtx->hWndNextInChain);
        if (fRc)
            pCtx->hWndNextInChain = NULL;
    }

    int rc = VINF_SUCCESS;

    if (!fRc)
    {
        const DWORD dwLastErr = GetLastError();
        rc = RTErrConvertFromWin32(dwLastErr);
        LogFunc(("Failed with %Rrc (0x%x)\n", rc, dwLastErr));
    }

    return rc;
}

/**
 * Callback which is invoked when we have successfully pinged ourselves down the
 * clipboard chain.  We simply unset a boolean flag to say that we are responding.
 * There is a race if a ping returns after the next one is initiated, but nothing
 * very bad is likely to happen.
 *
 * @param   hWnd                Window handle to use for this callback. Not used currently.
 * @param   uMsg                Message to handle. Not used currently.
 * @param   dwData              Pointer to user-provided data. Contains our Windows clipboard context.
 * @param   lResult             Additional data to pass. Not used currently.
 */
VOID CALLBACK VBoxClipboardWinChainPingProc(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult)
{
    RT_NOREF(hWnd);
    RT_NOREF(uMsg);
    RT_NOREF(lResult);

    /** @todo r=andy Why not using SetWindowLongPtr for keeping the context? */
    PVBOXCLIPBOARDWINCTX pCtx = (PVBOXCLIPBOARDWINCTX)dwData;
    AssertPtrReturnVoid(pCtx);

    pCtx->oldAPI.fCBChainPingInProcess = FALSE;
}

/**
 * Retrieves all supported clipboard formats of a specific clipboard.
 *
 * @returns VBox status code.
 * @param   pCtx                Windows clipboard context to retrieve formats for.
 * @param   puFormats           Where to store the retrieved formats of type VBOX_SHARED_CLIPBOARD_FMT_ (bitmask).
 */
int VBoxClipboardWinGetFormats(PVBOXCLIPBOARDWINCTX pCtx, uint32_t *puFormats)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(puFormats, VERR_INVALID_POINTER);

    if (!pCtx)
    {
        *puFormats = 0;
        return VINF_SUCCESS;
    }

    uint32_t uFormats = 0;

    /* Query list of available formats and report to host. */
    int rc = VBoxClipboardWinOpen(pCtx->hWnd);
    if (RT_SUCCESS(rc))
    {
        UINT uCurFormat = 0;
        while ((uCurFormat = EnumClipboardFormats(uCurFormat)) != 0)
        {
            LogFlowFunc(("uFormat = 0x%08X\n", uCurFormat));
            switch (uCurFormat)
            {
                case CF_UNICODETEXT:
                case CF_TEXT:
                    uFormats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
                    break;

                case CF_DIB:
                case CF_BITMAP:
                    uFormats |= VBOX_SHARED_CLIPBOARD_FMT_BITMAP;
                    break;

                default:
                {
                    if (uCurFormat >= 0xC000) /** @todo r=andy Find a define for this. */
                    {
                        TCHAR szFormatName[256]; /** @todo r=andy Is this enough? */
                        int cActual = GetClipboardFormatName(uCurFormat, szFormatName, sizeof(szFormatName)/sizeof (TCHAR));
                        if (cActual)
                        {
                            if (strcmp (szFormatName, "HTML Format") == 0)
                            {
                                uFormats |= VBOX_SHARED_CLIPBOARD_FMT_HTML;
                            }
                        }
                    }
                    break;
                }
            }
        }

        int rc2 = VBoxClipboardWinClose();
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        LogFunc(("Failed with rc=%Rrc\n", rc));
    }
    else
        *puFormats = uFormats;

    return rc;
}

