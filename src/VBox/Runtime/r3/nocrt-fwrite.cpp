/* $Id$ */
/** @file
 * IPRT - No-CRT - fwrite().
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
#include <iprt/nocrt/stdio.h>
#include <iprt/nocrt/errno.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/stream.h>


#undef fwrite
size_t RT_NOCRT(fwrite)(void const *pvBuf, size_t cbItem, size_t cItems, FILE *pFile)
{
    size_t cbToWrite = cbItem * cItems;
    AssertReturnStmt(cbToWrite >= cbItem && cbItem >= cItems, errno = ERANGE, 0);
    size_t cbWritten = 0;
    int rc = RTStrmWriteEx(pFile, pvBuf, cbToWrite, &cbWritten);
    if (RT_SUCCESS(rc))
        return cbWritten;
    errno = RTErrConvertToErrno(rc);
    return 0;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(fwrite);

