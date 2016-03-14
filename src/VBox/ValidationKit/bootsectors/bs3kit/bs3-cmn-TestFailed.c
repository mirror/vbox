/* $Id$ */
/** @file
 * BS3Kit - Bs3TestFailed, Bs3TestFailedF, Bs3TestFailedV.
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
#include <iprt/asm-amd64-x86.h>


/**
 * @impl_callback_method{FNBS3STRFORMATOUTPUT,
 *      Used by Bs3TestFailedV and Bs3TestSkippedV.}
 */
BS3_DECL_CALLBACK(size_t) bs3TestFailedStrOutput(char ch, void BS3_FAR *pvUser)
{
    bool *pfNewLine = (bool *)pvUser;

    /*
     * VMMDev first.  We postpone newline processing here so we can strip one
     * trailing newline.
     */
    if (BS3_DATA_NM(g_fbBs3VMMDevTesting))
    {
        if (*pfNewLine && ch != '\0')
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, '\n');
        if (ch != '\n')
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, ch);
    }

    /*
     * Console next.
     */
    if (ch != 0)
    {
        Bs3PrintChr(ch);
        *pfNewLine = ch == '\n';
    }
    /* We're called with '\0' to indicate end-of-string. Supply trailing
       newline if necessary. */
    else if (!*pfNewLine)
        Bs3PrintChr('\n');

    return 1;
}


/**
 * Equivalent to RTTestIFailedV.
 */
BS3_DECL(void) Bs3TestFailedV(const char *pszFormat, va_list va)
{
    bool fNewLine;

    if (!++BS3_DATA_NM(g_cusBs3TestErrors))
        BS3_DATA_NM(g_cusBs3TestErrors)++;

    if (BS3_DATA_NM(g_fbBs3VMMDevTesting))
        ASMOutU32(VMMDEV_TESTING_IOPORT_CMD, VMMDEV_TESTING_CMD_FAILED);

    fNewLine = false;
    Bs3StrFormatV(pszFormat, va, bs3TestFailedStrOutput, &fNewLine);
}


/**
 * Equivalent to RTTestIFailedF.
 */
BS3_DECL(void) Bs3TestFailedF(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    Bs3TestFailedV(pszFormat, va);
    va_end(va);
}


/**
 * Equivalent to RTTestIFailed.
 */
BS3_DECL(void) Bs3TestFailed(const char *pszMessage)
{
    Bs3TestFailedF("%s", pszMessage);
}

