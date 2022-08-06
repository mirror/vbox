/* $Id$ */
/** @file
 * IPRT - No-CRT - read().
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#define IPRT_NO_CRT_FOR_3RD_PARTY
#include "internal/nocrt.h"
#include <iprt/nocrt/fcntl.h>
#include <iprt/nocrt/errno.h>
#include <iprt/assert.h>
#include <iprt/assertcompile.h>
#include <iprt/errcore.h>
#include <iprt/file.h>


#undef open
int RT_NOCRT(open)(const char *pszFilename, uint64_t fFlags, ... /*RTFMODE fMode*/)
{
    /*
     * Make fFlags into proper RTFILE_O_XXX.
     */
    /* Make sure we got exactly one RTFILE_O_ACTION_MASK value: */
    AssertCompile(O_CREAT == RTFILE_O_OPEN_CREATE); AssertCompile(RT_IS_POWER_OF_TWO(O_CREAT));
    AssertCompile(O_EXCL == RTFILE_O_CREATE); AssertCompile(RT_IS_POWER_OF_TWO(O_EXCL));
    if (fFlags & O_CREAT)
    {
        if (fFlags & O_EXCL)
            fFlags &= ~(uint64_t)O_CREAT;
        va_list va;
        va_start(va, fFlags);
        int fMode = va_arg(va, int);
        va_end(va);
        fFlags &= ~(uint64_t)RTFILE_O_CREATE_MODE_MASK;
        fFlags |= ((uint64_t)fMode << RTFILE_O_CREATE_MODE_SHIFT) & RTFILE_O_CREATE_MODE_MASK;
    }
    else
    {
        if (fFlags & O_EXCL)
            fFlags &= ~(uint64_t)O_EXCL;
        fFlags |= RTFILE_O_OPEN;
    }

    /* Close on exec / inherit flag needs inverting: */
    AssertCompile(O_CLOEXEC == RTFILE_O_INHERIT);
    fFlags ^= O_CLOEXEC;

    /* Add deny selection: */
    fFlags |= RTFILE_O_DENY_NONE;

    /*
     * Try open it.
     */
    RTFILE hFile = NIL_RTFILE;
    int rc = RTFileOpen(&hFile, pszFilename, fFlags);
    if (RT_SUCCESS(rc))
    {
        intptr_t fd = RTFileToNative(hFile);
        if ((int)fd == fd)
            return (int)fd;

        AssertMsgFailed(("fd=%zd (%p)\n", fd, fd));
        RTFileClose(hFile);
        rc = VERR_INTERNAL_ERROR;
    }
    errno = RTErrConvertToErrno(rc);
    return -1;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(open);

