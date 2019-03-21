/* $Id$ */
/** @file
 * IPRT - RTFileCopyAttributes, generic implementation.
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
#include "internal/iprt.h"
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>


RTDECL(int) RTFileCopyAttributes(RTFILE hFileSrc, RTFILE hFileDst, uint32_t fFlags)
{
    AssertReturn(fFlags == 0, VERR_INVALID_FLAGS);

    RTFSOBJINFO ObjInfo;
    int rc = RTFileQueryInfo(hFileSrc, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        /*
         * The file mode.
         */
        rc = RTFileSetMode(hFileDst, ObjInfo.Attr.fMode);
        if (RT_FAILURE(rc) && (ObjInfo.Attr.fMode & (RTFS_UNIX_ALL_ACCESS_PERMS & ~RTFS_UNIX_ALL_ACCESS_PERMS)))
            rc = RTFileSetMode(hFileDst, ObjInfo.Attr.fMode & ~(RTFS_UNIX_ALL_ACCESS_PERMS & ~RTFS_UNIX_ALL_ACCESS_PERMS));

        /*
         * As many of the timestamps as we are able to, except the birth time.
         * Ignoring the status here because of hardend builds on older linux
         * systems (see RTFileSetTimes).
         */
        RTFileSetTimes(hFileDst, &ObjInfo.AccessTime, &ObjInfo.ModificationTime, &ObjInfo.ChangeTime, NULL /*pBirthTime*/);
    }
    return rc;
}

