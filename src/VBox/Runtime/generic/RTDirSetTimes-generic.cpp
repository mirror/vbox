/* $Id$ */
/** @file
 * InnoTek Portable Runtime - RTDirSetTimes, generic implementation.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_DIR
#ifdef __WIN__ /* dir.h has host specific stuff */
# include <Windows.h>
#else
# include <dirent.h>
#endif

#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/dir.h"



RTR3DECL(int) RTDirSetTimes(PRTDIR pDir, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                            PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    /*
     * Validate and digest input.
     */
    if (!rtDirValidHandle(pDir))
        return VERR_INVALID_PARAMETER;
    return RTPathSetTimes(pDir->pszPath, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
}

