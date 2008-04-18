/* $Id$ */
/** @file
 * innotek Portable Runtime - File System, Generic Stubs.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#define LOG_GROUP RTLOGGROUP_FS

#include <iprt/fs.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/assert.h>
#include "internal/fs.h"



RTR3DECL(int) RTFsQuerySizes(const char *pszFsPath, RTFOFF *pcbTotal, RTFOFF *pcbFree,
                             uint32_t *pcbBlock, uint32_t *pcbSector)
{
    if (pcbTotal)
        *pcbTotal = _2G;
    if (pcbFree)
        *pcbFree = _1G;
    if (pcbBlock)
        *pcbBlock = _4K;
    if (pcbSector)
        *pcbSector = 512;
    LogFlow(("RTFsQuerySizes: success stub!\n"));
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFsQuerySerial(const char *pszFsPath, uint32_t *pu32Serial)
{
    if (pu32Serial)
        *pu32Serial = 0xc0ffee;
    LogFlow(("RTFsQuerySerial: success stub!\n"));
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFsQueryProperties(const char *pszFsPath, PRTFSPROPERTIES pProperties)
{
    pProperties->cbMaxComponent = 255;
    pProperties->fCaseSensitive = true;
    pProperties->fCompressed = false;
    pProperties->fFileCompression = false;
    pProperties->fReadOnly = false;
    pProperties->fRemote = false;
    pProperties->fSupportsUnicode = true;
    LogFlow(("RTFsQueryProperties: success stub!\n"));
    return VINF_SUCCESS;
}

