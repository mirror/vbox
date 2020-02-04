/* $Id$ */
/** @file
 * IPRT - System firmware information, linux.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/system.h>

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/linux/sysfs.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Defines the UEFI Globals UUID that is used here as filename suffix (case sensitive). */
#define VBOX_UEFI_UUID_GLOBALS "8be4df61-93ca-11d2-aa0d-00e098032b8c"


RTDECL(int) RTSystemQueryFirmwareType(PRTSYSFWTYPE penmFirmwareType)
{
    if (RTLinuxSysFsExists("firmware/efi/"))
        *penmFirmwareType = RTSYSFWTYPE_UEFI;
    else if (RTLinuxSysFsExists(""))
        *penmFirmwareType = RTSYSFWTYPE_BIOS;
    else
    {
        *penmFirmwareType = RTSYSFWTYPE_INVALID;
        return VERR_NOT_SUPPORTED;
    }
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSystemQueryFirmwareType);


RTDECL(int) RTSystemQueryFirmwareBoolean(RTSYSFWBOOL enmBoolean, bool *pfValue)
{
    *pfValue = false;

    /*
     * Translate the property to variable base filename.
     */
    const char *pszName;
    switch (enmBoolean)
    {
        case RTSYSFWBOOL_SECURE_BOOT:
            pszName = "firmware/efi/efivars/SecureBoot";
            break;

        default:
            AssertReturn(enmBoolean > RTSYSFWBOOL_INVALID && enmBoolean < RTSYSFWBOOL_END, VERR_INVALID_PARAMETER);
            return VERR_SYS_UNSUPPORTED_FIRMWARE_PROPERTY;

    }

    /*
     * Try open and read the variable value.
     */
    RTFILE hFile;
    int rc = RTLinuxSysFsOpen(&hFile, "%s-" VBOX_UEFI_UUID_GLOBALS, pszName);
    /** @todo try other suffixes if file-not-found. */
    if (RT_SUCCESS(rc))
    {
        uint8_t abBuf[16];
        size_t  cbRead = 0;
        rc = RTLinuxSysFsReadFile(hFile, abBuf, sizeof(abBuf), &cbRead);
        *pfValue = cbRead > 1 && abBuf[cbRead - 1] != 0;
        RTFileClose(hFile);
    }
    else if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
        rc = VINF_SUCCESS;
    else if (rc == VERR_PERMISSION_DENIED)
        rc = VERR_NOT_SUPPORTED;

    return rc;
}
RT_EXPORT_SYMBOL(RTSystemQueryFirmwareBoolean);

