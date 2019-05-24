/* $Id$ */
/** @file
 * IPRT - RTPathParentLength
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
#include <iprt/path.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/string.h>

#define RTPATH_TEMPLATE_CPP_H "RTPathParentLength.cpp.h"
#include "rtpath-expand-template.cpp.h"



RTDECL(size_t) RTPathParentLengthEx(const char *pszPath, uint32_t fFlags)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(pszPath, 0);
    AssertReturn(*pszPath, 0);
    AssertReturn(RTPATH_STR_F_IS_VALID(fFlags, 0), 0);
    Assert(!(fFlags & RTPATH_STR_F_NO_END));

    /*
     * Invoke the worker for the selected path style.
     */
    switch (fFlags & RTPATH_STR_F_STYLE_MASK)
    {
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
        case RTPATH_STR_F_STYLE_HOST:
#endif
        case RTPATH_STR_F_STYLE_DOS:
            return rtPathParentLengthStyleDos(pszPath, fFlags);

#if RTPATH_STYLE != RTPATH_STR_F_STYLE_DOS
        case RTPATH_STR_F_STYLE_HOST:
#endif
        case RTPATH_STR_F_STYLE_UNIX:
            return rtPathParentLengthStyleUnix(pszPath, fFlags);

        default:
            AssertFailedReturn(0); /* impossible */
    }
}




/**
 * Determins the length of the path specifying the parent directory, including
 * trailing path separator (if present).
 *
 * @returns Parent directory part of the path, 0 if no parent.
 * @param   pszPath         The path to examine.
 *
 * @note    Currently ignores UNC and may therefore return the server or
 *          double-slash prefix as parent.
 */
size_t RTPathParentLength(const char *pszPath)
{
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
    return rtPathParentLengthStyleDos(pszPath, 0);
#else
    return rtPathParentLengthStyleUnix(pszPath, 0);
#endif
}

