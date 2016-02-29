/* $Id$ */
/** @file
 * BS3Kit - Bs3TestSkipped
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#include "bs3kit-template-header.h"
#include "bs3-cmn-test.h"


/**
 * Equivalent to RTTestSkippedV.
 */
BS3_DECL(void) Bs3TestSkippedV(const char *pszFormat, va_list va)
{
    /* Just mark it as skipped and deal with it when the sub-test is done. */
    BS3_DATA_NM(g_fbBs3SubTestSkipped) = true;

    /* The reason why it was skipp is optional. */
    if (pszFormat)
    {
        bool fNewLine = false;
        Bs3StrFormatV(pszFormat, va, bs3TestFailedStrOutput, &fNewLine);
    }
}


/**
 * Equivalent to RTTestSkipped.
 */
BS3_DECL(void) Bs3TestSkippedF(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    Bs3TestSkippedV(pszFormat, va);
    va_end(va);
}


/**
 * Equivalent to RTTestSkipped.
 */
BS3_DECL(void) Bs3TestSkipped(const char *pszWhy)
{
    Bs3TestSkippedF(pszWhy ? "%s" : NULL, pszWhy);
}

