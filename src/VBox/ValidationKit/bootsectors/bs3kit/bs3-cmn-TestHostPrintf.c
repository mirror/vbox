/* $Id$ */
/** @file
 * BS3Kit - BS3TestPrintf, BS3TestPrintfV
 */

/*
 * Copyright (C) 2007-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Output state for Bs3TestHostPrintfV. */
typedef struct BS3TESTHOSTPRINTF
{
    bool     fNewCmd;
} BS3TESTHOSTPRINTF;


/**
 * @callback_method_impl{FNBS3STRFORMATOUTPUT, Prints to screen and VMMDev}
 */
static BS3_DECL_CALLBACK(size_t) bs3TestPrintfStrOutput(char ch, void BS3_FAR *pvUser)
{
    BS3TESTHOSTPRINTF BS3_FAR *pState = (BS3TESTHOSTPRINTF BS3_FAR *)pvUser;

    /*
     * VMMDev first.  We do line by line processing to avoid running out of
     * string buffer on the host side.
     */
    if (g_fbBs3VMMDevTesting)
    {
        if (ch != '\n' && !pState->fNewCmd)
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, ch);
        else if (ch != '\0')
        {
            if (pState->fNewCmd)
            {
#if ARCH_BITS == 16
                ASMOutU16(VMMDEV_TESTING_IOPORT_CMD, (uint16_t)VMMDEV_TESTING_CMD_PRINT);
#else
                ASMOutU32(VMMDEV_TESTING_IOPORT_CMD, VMMDEV_TESTING_CMD_PRINT);
#endif
                pState->fNewCmd = false;
            }
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, ch);
            if (ch == '\n')
            {
                ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, '\0');
                pState->fNewCmd = true;
            }
        }
    }

    return ch != '\0';
}


#undef Bs3TestHostPrintfV
BS3_CMN_DEF(void, Bs3TestHostPrintfV,(const char BS3_FAR *pszFormat, va_list BS3_FAR va))
{
    BS3TESTHOSTPRINTF State;
    State.fNewCmd = true;
    Bs3StrFormatV(pszFormat, va, bs3TestPrintfStrOutput, &State);
}



#undef Bs3TestHostPrintf
BS3_CMN_DEF(void, Bs3TestHostPrintf,(const char BS3_FAR *pszFormat, ...))
{
    va_list va;
    va_start(va, pszFormat);
    BS3_CMN_NM(Bs3TestHostPrintfV)(pszFormat, va);
    va_end(va);
}

