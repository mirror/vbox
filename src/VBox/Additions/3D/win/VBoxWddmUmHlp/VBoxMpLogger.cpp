/* $Id$ */
/** @file
 * VBox WDDM Display logger implementation
 */

/*
 * Copyright (C) 2018-2020 Oracle Corporation
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
 * can not do CreateFile/Read/Write by default.
 * This is why we use miniport escape functionality to issue backdoor log string to the miniport
 * and submit it to host via standard r0 backdoor logging api accordingly
 */

#include "UmHlpInternal.h"

#include <../../../common/wddm/VBoxMPIf.h>

#include <stdio.h>

DECLCALLBACK(void) VBoxDispMpLoggerLog(const char *pszString)
{
    D3DKMTFUNCTIONS const *d3dkmt = D3DKMTFunctions();
    if (d3dkmt->pfnD3DKMTEscape == NULL)
        return;

    D3DKMT_HANDLE hAdapter;
    NTSTATUS Status = vboxDispKmtOpenAdapter(&hAdapter);
    Assert(Status == STATUS_SUCCESS);
    if (Status == 0)
    {
        uint32_t cbString = (uint32_t)strlen(pszString) + 1;
        uint32_t cbCmd = RT_UOFFSETOF_DYN(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[cbString]);
        PVBOXDISPIFESCAPE_DBGPRINT pCmd = (PVBOXDISPIFESCAPE_DBGPRINT)malloc(cbCmd);
        Assert(pCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGPRINT;
            pCmd->EscapeHdr.u32CmdSpecific = 0;
            memcpy(pCmd->aStringBuf, pszString, cbString);

            D3DKMT_ESCAPE EscapeData;
            memset(&EscapeData, 0, sizeof(EscapeData));
            EscapeData.hAdapter = hAdapter;
            // EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
            // EscapeData.Flags.HardwareAccess = 0;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            // EscapeData.hContext = NULL;

            Status = d3dkmt->pfnD3DKMTEscape(&EscapeData);
            Assert(Status == STATUS_SUCCESS);

            free(pCmd);
        }

        Status = vboxDispKmtCloseAdapter(hAdapter);
        Assert(Status == STATUS_SUCCESS);
    }
}

DECLCALLBACK(void) VBoxDispMpLoggerLogF(const char *pszString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, pszString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), pszString, pArgList);
    va_end(pArgList);

    VBoxDispMpLoggerLog(szBuffer);
}

/*
 * Prefix the output string with module name and pid/tid.
 */
static const char *vboxUmLogGetModuleName(void)
{
    static int s_fModuleNameInited = 0;
    static char s_szModuleName[MAX_PATH];

    if (!s_fModuleNameInited)
    {
        const DWORD cchName = GetModuleFileNameA(NULL, s_szModuleName, RT_ELEMENTS(s_szModuleName));
        if (cchName == 0)
        {
            return "<no module>";
        }
        s_fModuleNameInited = 1;
    }
    return &s_szModuleName[0];
}

DECLCALLBACK(void) VBoxWddmUmLog(const char *pszString)
{
    char szBuffer[4096];
    const int cbBuffer = sizeof(szBuffer);
    char *pszBuffer = &szBuffer[0];

    int cbWritten = _snprintf(pszBuffer, cbBuffer, "['%s' 0x%lx.0x%lx]: ",
                              vboxUmLogGetModuleName(), GetCurrentProcessId(), GetCurrentThreadId());
    if (cbWritten < 0 || cbWritten >= cbBuffer)
    {
        Assert(0);
        pszBuffer[0] = 0;
        cbWritten = 0;
    }

    const size_t cbLeft = cbBuffer - cbWritten;
    const size_t cbString = strlen(pszString) + 1;
    if (cbString <= cbLeft)
    {
        memcpy(pszBuffer + cbWritten, pszString, cbString);
    }
    else
    {
        memcpy(pszBuffer + cbWritten, pszString, cbLeft - 1);
        pszBuffer[cbWritten + cbLeft - 1] = 0;
    }

    VBoxDispMpLoggerLog(szBuffer);
}
