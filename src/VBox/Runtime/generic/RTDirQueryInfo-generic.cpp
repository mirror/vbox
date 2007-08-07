/* $Id$ */
/** @file
 * innotek Portable Runtime - RTDirQueryInfo, generic implementation.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_DIR
#ifdef RT_OS_WINDOWS /* dir.h has host specific stuff */
# include <Windows.h>
#else
# include <dirent.h>
#endif

#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/dir.h"


RTR3DECL(int) RTDirQueryInfo(PRTDIR pDir, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    /*
     * Validate and digest input.
     */
    if (!rtDirValidHandle(pDir))
        return VERR_INVALID_PARAMETER;
    return RTPathQueryInfo(pDir->pszPath, pObjInfo, enmAdditionalAttribs);
}

