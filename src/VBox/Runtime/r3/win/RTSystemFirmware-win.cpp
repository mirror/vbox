/* $Id$ */
/** @file
 * IPRT - System firmware information, Win32.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/

#include <iprt/nt/nt-and-windows.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/ldr.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * These are the FirmwareType* defines found in the Vista Platform SDK and returned
 * by GetProductInfo().
 *
 * We define them ourselves because we don't necessarily have any Vista PSDK around.
 */
typedef enum RTWINFWTYPE
{
    kRTWinFirmwareTypeUnknown = 0,
    kRTWinFirmwareTypeBios    = 1,
    kRTWinFirmwareTypeUefi    = 2,
    kRTWinFirmwareTypeMax     = 3
} RTWINFWTYPE;

/** Function pointer for dynamic import of GetFirmwareType(). */
typedef BOOL (WINAPI *PFNGETFIRMWARETYPE)(RTWINFWTYPE *);

/** Defines the UEFI Globals UUID. */
#define VBOX_UEFI_UUID_GLOBALS L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}"
/** Defines an UEFI dummy UUID. */
#define VBOX_UEFI_UUID_DUMMY   L"{00000000-0000-0000-0000-000000000000}"


static int rtSystemFirmwareGetPrivileges(LPCTSTR pcszPrivilege)
{
    HANDLE hToken;
    BOOL fRc = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
    if (!fRc)
        return RTErrConvertFromWin32(GetLastError());

    int rc = VINF_SUCCESS;

    TOKEN_PRIVILEGES tokenPriv;
    fRc = LookupPrivilegeValue(NULL, pcszPrivilege, &tokenPriv.Privileges[0].Luid);
    if (fRc)
    {
        tokenPriv.PrivilegeCount = 1;
        tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        fRc = AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, 0, (PTOKEN_PRIVILEGES)NULL, 0);
        if (!fRc)
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    CloseHandle(hToken);

    return rc;
}


RTDECL(int) RTSystemFirmwareQueryType(PRTSYSFWTYPE pFirmwareType)
{
    AssertPtrReturn(pFirmwareType, VERR_INVALID_POINTER);

    RTSYSFWTYPE fwType = RTSYSFWTYPE_UNKNOWN;

    RTLDRMOD hKernel32 = NIL_RTLDRMOD;
    int rc = RTLdrLoadSystem("Kernel32.dll", /* fNoUnload = */ true, &hKernel32);
    if (RT_SUCCESS(rc))
    {
        PFNGETFIRMWARETYPE pfnGetFirmwareType;
        rc = RTLdrGetSymbol(hKernel32, "GetFirmwareType", (void **)&pfnGetFirmwareType); /* Only >= Windows 8. */
        if (RT_SUCCESS(rc))
        {
            RTWINFWTYPE winFwType;
            if (pfnGetFirmwareType(&winFwType))
            {
                switch (winFwType)
                {
                    case kRTWinFirmwareTypeBios:
                        fwType = RTSYSFWTYPE_BIOS;
                        break;

                    case kRTWinFirmwareTypeUefi:
                        fwType = RTSYSFWTYPE_UEFI;
                        break;

                    default: /* Huh? */
                        fwType = RTSYSFWTYPE_UNKNOWN;
                        break;
                }
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }
        else /* Fallback for OSes < Windows 8. */
        {
            rc = rtSystemFirmwareGetPrivileges(SE_SYSTEM_ENVIRONMENT_NAME);
            if (RT_SUCCESS(rc))
            {
                PCWCHAR pcwszGUID = VBOX_UEFI_UUID_GLOBALS; /* UEFI Globals. */
                PCWCHAR pcwszName = L"";

                uint8_t uEnabled = 0;
                DWORD dwRet = GetFirmwareEnvironmentVariableW(pcwszName, pcwszGUID, &uEnabled, sizeof(uEnabled));
                if (dwRet) /* Returns the bytes written. */
                {
                    Assert(dwRet == sizeof(uEnabled));
                    fwType = RT_BOOL(uEnabled) ? RTSYSFWTYPE_UEFI : RTSYSFWTYPE_BIOS;
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
        }

        RTLdrClose(hKernel32);
    }

    if (RT_SUCCESS(rc))
        *pFirmwareType = fwType;

    return rc;
}


RTDECL(void) RTSystemFirmwareValueFree(PRTSYSFWVALUE pValue)
{
    if (!pValue)
        return;

    /** @todo Implement cleanup here. */
}


RTDECL(int) RTSystemFirmwareValueGet(RTSYSFWPROP enmProp, PRTSYSFWVALUE *ppValue)
{
    int rc = rtSystemFirmwareGetPrivileges(SE_SYSTEM_ENVIRONMENT_NAME);
    if (RT_FAILURE(rc))
        return rc;

    PRTSYSFWVALUE pValue = (PRTSYSFWVALUE)RTMemAlloc(sizeof(RTSYSFWVALUE));
    if (!pValue)
        return VERR_NO_MEMORY;

    char *pszName = NULL;
    DWORD dwSize  = 0;

    switch (enmProp)
    {
        case RTSYSFWPROP_SECURE_BOOT:
        {
            rc = RTStrAAppend(&pszName, "SecureBoot");
            if (RT_FAILURE(rc))
                break;

            pValue->enmType = RTSYSFWVALUETYPE_BOOLEAN;
            dwSize = 1;
            break;
        }

        case RTSYSFWPROP_BOOT_CURRENT:
            RT_FALL_THROUGH();
        case RTSYSFWPROP_BOOT_ORDER:
            RT_FALL_THROUGH();
        case RTSYSFWPROP_BOOT_NEXT:
            RT_FALL_THROUGH();
        case RTSYSFWPROP_TIMEOUT:
            RT_FALL_THROUGH();
        case RTSYSFWPROP_PLATFORM_LANG:
            rc = VERR_NOT_IMPLEMENTED;
            break;

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszName;
        rc = RTStrToUtf16(pszName, &pwszName);
        if (RT_SUCCESS(rc))
        {
            void *pvBuf = RTMemAlloc(dwSize);
            DWORD dwBuf = dwSize;

            if (pvBuf)
            {
                DWORD dwRet = GetFirmwareEnvironmentVariableW(pwszName, VBOX_UEFI_UUID_GLOBALS, pvBuf, dwBuf);
                if (dwRet)
                {
                    switch (pValue->enmType)
                    {
                        case RTSYSFWVALUETYPE_BOOLEAN:
                            pValue->u.fVal = RT_BOOL(*(uint8_t *)pvBuf);
                            break;

                        case RTSYSFWVALUETYPE_INVALID:
                            RT_FALL_THROUGH();
                        default:
                            AssertFailed();
                            break;
                    }
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());

                RTMemFree(pvBuf);
            }
            else
                rc = VERR_NO_MEMORY;

            RTUtf16Free(pwszName);
        }
    }

    RTStrFree(pszName);

    if (RT_SUCCESS(rc))
    {
        *ppValue = pValue;
    }
    else
        RTSystemFirmwareValueFree(pValue);

    return rc;
}

