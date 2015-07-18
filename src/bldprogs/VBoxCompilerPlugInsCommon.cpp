/* $Id$ */
/** @file
 * VBoxCompilerPlugInsCommon - Code common to the compiler plug-ins.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define VBOX_COMPILER_PLUG_IN_AGNOSTIC
#include "VBoxCompilerPlugIns.h"

#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define MY_ISDIGIT(c) ((c) >= '0' && (c) <= '9')



/**
 * Does the actual format string checking.
 *
 * @todo    Move this to different file common to both GCC and CLANG later.
 *
 * @param   pState              The format string checking state.
 * @param   pszFmt              The format string.
 */
void MyCheckFormatCString(PVFMTCHKSTATE pState, const char *pszFmt)
{
    dprintf("checker2: \"%s\" at %s:%d col %d\n", pszFmt,
            MYSTATE_FMT_FILE(pState), MYSTATE_FMT_LINE(pState), MYSTATE_FMT_COLUMN(pState));
    pState->pszFmt = pszFmt;

    unsigned iArg = 0;
    for (;;)
    {
        /*
         * Skip to the next argument.
         * Quits the loop with the first char following the '%' in 'ch'.
         */
        char ch;
        for (;;)
        {
            ch = *pszFmt++;
            if (ch == '%')
            {
                ch = *pszFmt++;
                if (ch != '%')
                    break;
            }
            else if (ch == '\0')
            {
                VFmtChkVerifyEndOfArgs(pState, iArg);
                return;
            }
        }
        const char * const pszPct = pszFmt - 2;

        /*
         * Flags
         */
        uint32_t fFlags = 0;
        for (;;)
        {
            uint32_t fFlag;
            switch (ch)
            {
                case '#':   fFlag = RTSTR_F_SPECIAL;      break;
                case '-':   fFlag = RTSTR_F_LEFT;         break;
                case '+':   fFlag = RTSTR_F_PLUS;         break;
                case ' ':   fFlag = RTSTR_F_BLANK;        break;
                case '0':   fFlag = RTSTR_F_ZEROPAD;      break;
                case '\'':  fFlag = RTSTR_F_THOUSAND_SEP; break;
                default:    fFlag = 0;                    break;
            }
            if (!fFlag)
                break;
            if (fFlags & fFlag)
                VFmtChkWarnFmt(pState, pszPct, "duplicate flag '%c'", ch);
            fFlags |= fFlag;
            ch = *pszFmt++;
        }

        /*
         * Width.
         */
        int cchWidth = -1;
        if (MY_ISDIGIT(ch))
        {
            cchWidth = ch - '0';
            while (   (ch = *pszFmt++) != '\0'
                   && MY_ISDIGIT(ch))
            {
                cchWidth *= 10;
                cchWidth += ch - '0';
            }
            fFlags |= RTSTR_F_WIDTH;
        }
        else if (ch == '*')
        {
            VFmtChkRequireIntArg(pState, pszPct, iArg, "width should be an 'int' sized argument");
            iArg++;
            cchWidth = 0;
            fFlags |= RTSTR_F_WIDTH;
            ch = *pszFmt++;
        }

        /*
         * Precision
         */
        int cchPrecision = -1;
        if (ch == '.')
        {
            ch = *pszFmt++;
            if (MY_ISDIGIT(ch))
            {
                cchPrecision = ch - '0';
                while (   (ch = *pszFmt++) != '\0'
                       && MY_ISDIGIT(ch))
                {
                    cchPrecision *= 10;
                    cchPrecision += ch - '0';
                }
            }
            else if (ch == '*')
            {
                VFmtChkRequireIntArg(pState, pszPct, iArg, "precision should be an 'int' sized argument");
                iArg++;
                cchPrecision = 0;
                ch = *pszFmt++;
            }
            else
                VFmtChkErrFmt(pState, pszPct, "Missing precision value, only got the '.'");
            if (cchPrecision < 0)
            {
                VFmtChkErrFmt(pState, pszPct, "Negative precision value: %d", cchPrecision);
                cchPrecision = 0;
            }
            fFlags |= RTSTR_F_PRECISION;
        }

        /*
         * Argument size.
         */
        char chSize = ch;
        switch (ch)
        {
            default:
                chSize = '\0';
                break;

            case 'z':
            case 'L':
            case 'j':
            case 't':
                ch = *pszFmt++;
                break;

            case 'l':
                ch = *pszFmt++;
                if (ch == 'l')
                {
                    chSize = 'L';
                    ch = *pszFmt++;
                }
                break;

            case 'h':
                ch = *pszFmt++;
                if (ch == 'h')
                {
                    chSize = 'H';
                    ch = *pszFmt++;
                }
                break;

            case 'I': /* Used by Win32/64 compilers. */
                if (   pszFmt[0] == '6'
                    && pszFmt[1] == '4')
                {
                    pszFmt += 2;
                    chSize = 'L';
                }
                else if (   pszFmt[0] == '3'
                         && pszFmt[1] == '2')
                {
                    pszFmt += 2;
                    chSize = 0;
                }
                else
                    chSize = 'j';
                ch = *pszFmt++;
                break;

            case 'q': /* Used on BSD platforms. */
                chSize = 'L';
                ch = *pszFmt++;
                break;
        }

        /*
         * The type.
         */
        switch (ch)
        {
            /*
             * Nested extensions.
             */
            case 'M': /* replace the format string (not stacked yet). */
            {
                if (*pszFmt)
                    VFmtChkErrFmt(pState, pszFmt, "Characters following '%%M' will be ignored");
                if (chSize != '\0')
                    VFmtChkWarnFmt(pState, pszFmt, "'%%M' does not support any size flags (%c)", chSize);
                if (fFlags != 0)
                    VFmtChkWarnFmt(pState, pszFmt, "'%%M' does not support any format flags (%#x)", fFlags);
                if (VFmtChkRequireStringArg(pState, pszPct, iArg, "'%M' expects a format string"))
                    VFmtChkHandleReplacementFormatString(pState, pszPct, iArg);
                return;
            }

            case 'N': /* real nesting. */
            {
                if (chSize != '\0')
                    VFmtChkWarnFmt(pState, pszFmt, "'%%N' does not support any size flags (%c)", chSize);
                if (fFlags != 0)
                    VFmtChkWarnFmt(pState, pszFmt, "'%%N' does not support any format flags (%#x)", fFlags);
                VFmtChkRequireStringArg(pState, pszPct, iArg,        "'%N' expects a string followed by a va_list pointer");
                VFmtChkRequireVaListPtrArg(pState, pszPct, iArg + 1, "'%N' expects a string followed by a va_list pointer");
                iArg += 2;
                break;
            }

            default:
                VFmtChkRequirePresentArg(pState, pszPct, iArg, "Expected argument");
                iArg++;
                break;
        }
    }
}

