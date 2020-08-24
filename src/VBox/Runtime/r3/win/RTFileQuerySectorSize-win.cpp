/* $Id$ */
/** @file
 * IPRT - RTFileQuerySectorSize, Windows.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
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
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>

#include <iprt/win/windows.h>



RTDECL(int) RTFileQuerySectorSize(RTFILE hFile, uint32_t *pcbSector)
{
    AssertPtrReturn(pcbSector, VERR_INVALID_PARAMETER);

    DISK_GEOMETRY DriveGeo;
    RT_ZERO(DriveGeo);
    DWORD cbDriveGeo = 0;
    if (DeviceIoControl((HANDLE)RTFileToNative(hFile),
                        IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                        &DriveGeo, sizeof(DriveGeo), &cbDriveGeo, NULL))
    {
        AssertReturn(DriveGeo.BytesPerSector > 0, VERR_INVALID_PARAMETER);
        *pcbSector = DriveGeo.BytesPerSector;
        return VINF_SUCCESS;
    }
    int rc = RTErrConvertFromWin32(GetLastError());
    AssertMsgFailed(("%d / %Rrc\n", GetLastError(), rc));
    return rc;
}

