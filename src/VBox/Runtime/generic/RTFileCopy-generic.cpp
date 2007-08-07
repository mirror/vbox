/* $Id$ */
/** @file
 * innotek Portable Runtime - RTFileCopy, generic implementation.
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
#include <iprt/file.h>


/**
 * Copies a file.
 *
 * @returns VERR_ALREADY_EXISTS if the destination file exists.
 * @returns VBox Status code.
 *
 * @param   pszSrc      The path to the source file.
 * @param   pszDst      The path to the destination file.
 *                      This file will be created.
 */
RTDECL(int) RTFileCopy(const char *pszSrc, const char *pszDst)
{
    return RTFileCopyEx(pszSrc, pszDst, NULL, NULL);
}

