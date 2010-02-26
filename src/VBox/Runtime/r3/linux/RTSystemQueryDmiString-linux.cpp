/* $Id$ */
/** @file
 * IPRT - RTSystemQueryDmiString, linux ring-3.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/linux/sysfs.h>

#include <errno.h>


RTDECL(int) RTSystemQueryDmiString(RTSYSDMISTR enmString, char *pszBuf, size_t cbBuf)
{
    AssertPtrReturn(pszBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf > 0, VERR_INVALID_PARAMETER);
    *pszBuf = '\0';
    AssertReturn(enmString > RTSYSDMISTR_INVALID && enmString < RTSYSDMISTR_END, VERR_INVALID_PARAMETER);

    const char *pszSysFsName;
    switch (enmString)
    {
        case RTSYSDMISTR_PRODUCT_NAME:      pszSysFsName = "id/product_name"; break;
        case RTSYSDMISTR_PRODUCT_VERSION:   pszSysFsName = "id/product_version"; break;
        case RTSYSDMISTR_PRODUCT_UUID:      pszSysFsName = "id/product_uuid"; break;
        case RTSYSDMISTR_PRODUCT_SERIAL:    pszSysFsName = "id/product_serial"; break;
        default:
            return VERR_NOT_SUPPORTED;
    }

    int rc;
    int fd = RTLinuxSysFsOpen("devices/virtual/dmi/%s", pszSysFsName);
    if (fd < 0)
        fd = RTLinuxSysFsOpen("class/dmi/%s", pszSysFsName);
    if (fd >= 0)
    {
        size_t cbRead;
        rc = RTLinuxSysFsReadFile(fd, pszBuf, cbBuf, &cbRead);
        if (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW)
        {
            /* The file we're reading may end with a newline, remove it. */
            if (cbRead == cbBuf)
                pszBuf[cbRead - 1] = '\0';
            else
            {
                AssertRC(rc);
                pszBuf[cbRead] = '\0';
                if (cbRead > 0 && pszBuf[cbRead - 1] == '\n')
                    pszBuf[cbRead - 1] = '\0';
            }
        }
        RTLinuxSysFsClose(fd);
    }
    else
    {
        rc = RTErrConvertFromErrno(errno);
        switch (rc)
        {
            case VINF_SUCCESS:
                AssertFailed();
            case VERR_FILE_NOT_FOUND:
            case VERR_PATH_NOT_FOUND:
            case VERR_IS_A_DIRECTORY:
                rc = VERR_NOT_SUPPORTED;
                break;
            case VERR_PERMISSION_DENIED:
            case VERR_ACCESS_DENIED:
                rc = VERR_ACCESS_DENIED;
                break;
        }
    }

    return rc;
}
RT_EXPORT_SYMBOL(RTSystemQueryDmiString);


