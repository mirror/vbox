/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Path Convertions, generic.
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
#define LOG_GROUP RTLOGGROUP_PATH
#include <iprt/string.h>
#include "internal/path.h"


int rtPathToNative(char **ppszNativePath, const char *pszPath)
{
    return RTStrUtf8ToCurrentCP(ppszNativePath, pszPath);
}

int rtPathToNativeEx(char **ppszNativePath, const char *pszPath, const char *pszBasePath)
{
    NOREF(pszBasePath);
    return RTStrUtf8ToCurrentCP(ppszNativePath, pszPath);
}

void rtPathFreeNative(char *pszNativePath)
{
    if (pszNativePath)
        RTStrFree(pszNativePath);
}


int rtPathFromNative(char **pszPath, const char *pszNativePath)
{
    return RTStrCurrentCPToUtf8(pszPath, pszNativePath);
}


int rtPathFromNativeEx(char **pszPath, const char *pszNativePath, const char *pszBasePath)
{
    NOREF(pszBasePath);
    return RTStrCurrentCPToUtf8(pszPath, pszNativePath);
}

