/* $Id$ */
/** @file
 * BS3Kit - BS3TestPrintf, BS3TestPrintfV
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
 * @impl_callback_method{FNBS3STRFORMATOUTPUT, Prints to screen and VMMDev}
 */
static BS3_DECL_CALLBACK(size_t) bs3TestPrintfStrOutput(char ch, void BS3_FAR *pvUser)
{
    /*
     * VMMDev first.  We do line by line processing to avoid running out of
     * string buffer on the host side.
     */
    if (BS3_DATA_NM(g_fbBs3VMMDevTesting))
    {
        bool *pfNewCmd = (bool *)pvUser;
        if (ch != '\n' && !*pfNewCmd)
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, ch);
        else if (ch != '\0')
        {
            if (*pfNewCmd)
            {
                ASMOutU32(VMMDEV_TESTING_IOPORT_CMD, VMMDEV_TESTING_CMD_PRINT);
                *pfNewCmd = false;
            }
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, ch);
            if (ch == '\n')
            {
                ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, '\0');
                *pfNewCmd = true;
            }
        }
    }

    /*
     * Console next.
     */
    if (ch != '\0')
        Bs3PrintChr(ch);
    return 1;
}



BS3_DECL(void) Bs3TestPrintfV(const char BS3_FAR *pszFormat, va_list va)
{
    bool fNewCmd = true;
    Bs3StrFormatV(pszFormat, va, bs3TestPrintfStrOutput, &fNewCmd);
}



BS3_DECL(void) Bs3TestPrintf(const char BS3_FAR *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    Bs3TestPrintfV(pszFormat, va);
    va_end(va);
}

