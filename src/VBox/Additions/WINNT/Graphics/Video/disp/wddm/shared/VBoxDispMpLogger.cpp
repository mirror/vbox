/* $Id$ */

/** @file
 * VBox WDDM Display backdoor logger implementation
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* We're unable to use standard r3 vbgl-based backdoor logging API because win8 Metro apps
 * can not do CreateFile/Read/Write by default
 * this is why we use miniport escape functionality to issue backdoor log string to the miniport
 * and submit it to host via standard r0 backdoor logging api accordingly */
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
#       include <iprt/win/windows.h>
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
#       include <iprt/win/windows.h>
#   endif
#include <VBoxDispMpLogger.h>
#include <d3d9types.h>
#include <D3dumddi.h>
#include <d3dhal.h>
#include <../../../common/wddm/VBoxMPIf.h>
#include <VBoxDispKmt.h>

#define VBOX_VIDEO_LOG_NAME "VBoxDispMpLogger"
#include <../../../common/VBoxVideoLog.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>

#include <stdio.h>

typedef enum
{
    VBOXDISPMPLOGGER_STATE_UNINITIALIZED = 0,
    VBOXDISPMPLOGGER_STATE_INITIALIZING,
    VBOXDISPMPLOGGER_STATE_INITIALIZED,
    VBOXDISPMPLOGGER_STATE_UNINITIALIZING
} VBOXDISPMPLOGGER_STATE;

typedef struct VBOXDISPMPLOGGER
{
    VBOXDISPKMT_CALLBACKS KmtCallbacks;
    VBOXDISPMPLOGGER_STATE enmState;
} VBOXDISPMPLOGGER, *PVBOXDISPMPLOGGER;

static VBOXDISPMPLOGGER g_VBoxDispMpLogger = {0};

static PVBOXDISPMPLOGGER vboxDispMpLoggerGet()
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_INITIALIZING, VBOXDISPMPLOGGER_STATE_UNINITIALIZED))
    {
        HRESULT hr = vboxDispKmtCallbacksInit(&g_VBoxDispMpLogger.KmtCallbacks);
        if (hr == S_OK)
        {
            /* we are on Vista+
             * check if we can Open Adapter, i.e. WDDM driver is installed */
            VBOXDISPKMT_ADAPTER Adapter;
            hr = vboxDispKmtOpenAdapter(&g_VBoxDispMpLogger.KmtCallbacks, &Adapter);
            if (hr == S_OK)
            {
                ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_INITIALIZED);
                vboxDispKmtCloseAdapter(&Adapter);
                return &g_VBoxDispMpLogger;
            }
            vboxDispKmtCallbacksTerm(&g_VBoxDispMpLogger.KmtCallbacks);
        }
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState) == VBOXDISPMPLOGGER_STATE_INITIALIZED)
    {
        return &g_VBoxDispMpLogger;
    }
    return NULL;
}

VBOXDISPMPLOGGER_DECL(int) VBoxDispMpLoggerInit(void)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return VERR_NOT_SUPPORTED;
    return VINF_SUCCESS;
}

VBOXDISPMPLOGGER_DECL(int) VBoxDispMpLoggerTerm(void)
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_UNINITIALIZING, VBOXDISPMPLOGGER_STATE_INITIALIZED))
    {
        vboxDispKmtCallbacksTerm(&g_VBoxDispMpLogger.KmtCallbacks);
        ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_UNINITIALIZED);
        return S_OK;
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState) == VBOXDISPMPLOGGER_STATE_UNINITIALIZED)
    {
        return S_OK;
    }
    return VERR_NOT_SUPPORTED;
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerLog(const char *pszString)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    VBOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vboxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbString = (uint32_t)strlen(pszString) + 1;
        uint32_t cbCmd = RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[cbString]);
        PVBOXDISPIFESCAPE_DBGPRINT pCmd = (PVBOXDISPIFESCAPE_DBGPRINT)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGPRINT;
            memcpy(pCmd->aStringBuf, pszString, cbString);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vboxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerLogF(const char *pszString, ...)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, pszString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), pszString, pArgList);
    va_end(pArgList);

    VBoxDispMpLoggerLog(szBuffer);
}

static void vboxDispMpLoggerDumpBuf(void *pvBuf, uint32_t cbBuf, VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE enmBuf)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    VBOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vboxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbCmd = RT_OFFSETOF(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[cbBuf]);
        PVBOXDISPIFESCAPE_DBGDUMPBUF pCmd = (PVBOXDISPIFESCAPE_DBGDUMPBUF)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGDUMPBUF;
            pCmd->enmType = enmBuf;
#ifdef VBOX_WDDM_WOW64
            pCmd->Flags.WoW64 = 1;
#endif
            memcpy(pCmd->aBuf, pvBuf, cbBuf);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vboxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerDumpD3DCAPS9(struct _D3DCAPS9 *pCaps)
{
    vboxDispMpLoggerDumpBuf(pCaps, sizeof (*pCaps), VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9);
}
