/* $Id$ */
/** @file
 * IPRT Testcase / Tool - Source Code Massager.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scm.h"


/**
 * Worker for isBlankLine.
 *
 * @returns true if blank, false if not.
 * @param   pchLine     Pointer to the start of the line.
 * @param   cchLine     The (encoded) length of the line, excluding EOL char.
 */
static bool isBlankLineSlow(const char *pchLine, size_t cchLine)
{
    /*
     * From the end, more likely to hit a non-blank char there.
     */
    while (cchLine-- > 0)
        if (!RT_C_IS_BLANK(pchLine[cchLine]))
            return false;
    return true;
}

/**
 * Helper for checking whether a line is blank.
 *
 * @returns true if blank, false if not.
 * @param   pchLine     Pointer to the start of the line.
 * @param   cchLine     The (encoded) length of the line, excluding EOL char.
 */
DECLINLINE(bool) isBlankLine(const char *pchLine, size_t cchLine)
{
    if (cchLine == 0)
        return true;
    /*
     * We're more likely to fine a non-space char at the end of the line than
     * at the start, due to source code indentation.
     */
    if (pchLine[cchLine - 1])
        return false;

    /*
     * Don't bother inlining loop code.
     */
    return isBlankLineSlow(pchLine, cchLine);
}


/**
 * Strip trailing blanks (space & tab).
 *
 * @returns True if modified, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_StripTrailingBlanks(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    if (!pSettings->fStripTrailingBlanks)
        return false;

    bool        fModified = false;
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pchLine;
    while ((pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol)) != NULL)
    {
        int rc;
        if (    cchLine == 0
            ||  !RT_C_IS_BLANK(pchLine[cchLine - 1]) )
            rc = ScmStreamPutLine(pOut, pchLine, cchLine, enmEol);
        else
        {
            cchLine--;
            while (cchLine > 0 && RT_C_IS_BLANK(pchLine[cchLine - 1]))
                cchLine--;
            rc = ScmStreamPutLine(pOut, pchLine, cchLine, enmEol);
            fModified = true;
        }
        if (RT_FAILURE(rc))
            return false;
    }
    if (fModified)
        ScmVerbose(pState, 2, " * Stripped trailing blanks\n");
    return fModified;
}

/**
 * Expand tabs.
 *
 * @returns True if modified, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_ExpandTabs(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    if (!pSettings->fConvertTabs)
        return false;

    size_t const    cchTab = pSettings->cchTab;
    bool            fModified = false;
    SCMEOL          enmEol;
    size_t          cchLine;
    const char     *pchLine;
    while ((pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol)) != NULL)
    {
        int rc;
        const char *pchTab = (const char *)memchr(pchLine, '\t', cchLine);
        if (!pchTab)
            rc = ScmStreamPutLine(pOut, pchLine, cchLine, enmEol);
        else
        {
            size_t      offTab   = 0;
            const char *pchChunk = pchLine;
            for (;;)
            {
                size_t  cchChunk = pchTab - pchChunk;
                offTab += cchChunk;
                ScmStreamWrite(pOut, pchChunk, cchChunk);

                size_t  cchToTab = cchTab - offTab % cchTab;
                ScmStreamWrite(pOut, g_szTabSpaces, cchToTab);
                offTab += cchToTab;

                pchChunk = pchTab + 1;
                size_t  cchLeft  = cchLine - (pchChunk - pchLine);
                pchTab = (const char *)memchr(pchChunk, '\t', cchLeft);
                if (!pchTab)
                {
                    rc = ScmStreamPutLine(pOut, pchChunk, cchLeft, enmEol);
                    break;
                }
            }

            fModified = true;
        }
        if (RT_FAILURE(rc))
            return false;
    }
    if (fModified)
        ScmVerbose(pState, 2, " * Expanded tabs\n");
    return fModified;
}

/**
 * Worker for rewrite_ForceNativeEol, rewrite_ForceLF and rewrite_ForceCRLF.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 * @param   enmDesiredEol       The desired end of line indicator type.
 * @param   pszDesiredSvnEol    The desired svn:eol-style.
 */
static bool rewrite_ForceEol(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings,
                             SCMEOL enmDesiredEol, const char *pszDesiredSvnEol)
{
    if (!pSettings->fConvertEol)
        return false;

    bool        fModified = false;
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pchLine;
    while ((pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol)) != NULL)
    {
        if (   enmEol != enmDesiredEol
            && enmEol != SCMEOL_NONE)
        {
            fModified = true;
            enmEol = enmDesiredEol;
        }
        int rc = ScmStreamPutLine(pOut, pchLine, cchLine, enmEol);
        if (RT_FAILURE(rc))
            return false;
    }
    if (fModified)
        ScmVerbose(pState, 2, " * Converted EOL markers\n");

    /* Check svn:eol-style if appropriate */
    if (   pSettings->fSetSvnEol
        && ScmSvnIsInWorkingCopy(pState))
    {
        char *pszEol;
        int rc = ScmSvnQueryProperty(pState, "svn:eol-style", &pszEol);
        if (   (RT_SUCCESS(rc) && strcmp(pszEol, pszDesiredSvnEol))
            || rc == VERR_NOT_FOUND)
        {
            if (rc == VERR_NOT_FOUND)
                ScmVerbose(pState, 2, " * Setting svn:eol-style to %s (missing)\n", pszDesiredSvnEol);
            else
                ScmVerbose(pState, 2, " * Setting svn:eol-style to %s (was: %s)\n", pszDesiredSvnEol, pszEol);
            int rc2 = ScmSvnSetProperty(pState, "svn:eol-style", pszDesiredSvnEol);
            if (RT_FAILURE(rc2))
                RTMsgError("ScmSvnSetProperty: %Rrc\n", rc2); /** @todo propagate the error somehow... */
        }
        if (RT_SUCCESS(rc))
            RTStrFree(pszEol);
    }

    /** @todo also check the subversion svn:eol-style state! */
    return fModified;
}

/**
 * Force native end of line indicator.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_ForceNativeEol(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    return rewrite_ForceEol(pState, pIn, pOut, pSettings, SCMEOL_CRLF, "native");
#else
    return rewrite_ForceEol(pState, pIn, pOut, pSettings, SCMEOL_LF,   "native");
#endif
}

/**
 * Force the stream to use LF as the end of line indicator.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_ForceLF(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    return rewrite_ForceEol(pState, pIn, pOut, pSettings, SCMEOL_LF, "LF");
}

/**
 * Force the stream to use CRLF as the end of line indicator.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_ForceCRLF(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    return rewrite_ForceEol(pState, pIn, pOut, pSettings, SCMEOL_CRLF, "CRLF");
}

/**
 * Strip trailing blank lines and/or make sure there is exactly one blank line
 * at the end of the file.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 *
 * @remarks ASSUMES trailing white space has been removed already.
 */
bool rewrite_AdjustTrailingLines(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    if (   !pSettings->fStripTrailingLines
        && !pSettings->fForceTrailingLine
        && !pSettings->fForceFinalEol)
        return false;

    size_t const cLines = ScmStreamCountLines(pIn);

    /* Empty files remains empty. */
    if (cLines <= 1)
        return false;

    /* Figure out if we need to adjust the number of lines or not. */
    size_t cLinesNew = cLines;

    if (   pSettings->fStripTrailingLines
        && ScmStreamIsWhiteLine(pIn, cLinesNew - 1))
    {
        while (   cLinesNew > 1
               && ScmStreamIsWhiteLine(pIn, cLinesNew - 2))
            cLinesNew--;
    }

    if (    pSettings->fForceTrailingLine
        && !ScmStreamIsWhiteLine(pIn, cLinesNew - 1))
        cLinesNew++;

    bool fFixMissingEol = pSettings->fForceFinalEol
                       && ScmStreamGetEolByLine(pIn, cLinesNew - 1) == SCMEOL_NONE;

    if (   !fFixMissingEol
        && cLines == cLinesNew)
        return false;

    /* Copy the number of lines we've arrived at. */
    ScmStreamRewindForReading(pIn);

    size_t cCopied = RT_MIN(cLinesNew, cLines);
    ScmStreamCopyLines(pOut, pIn, cCopied);

    if (cCopied != cLinesNew)
    {
        while (cCopied++ < cLinesNew)
            ScmStreamPutLine(pOut, "", 0, ScmStreamGetEol(pIn));
    }
    /* Fix missing EOL if required. */
    else if (fFixMissingEol)
    {
        if (ScmStreamGetEol(pIn) == SCMEOL_LF)
            ScmStreamWrite(pOut, "\n", 1);
        else
            ScmStreamWrite(pOut, "\r\n", 2);
    }

    ScmVerbose(pState, 2, " * Adjusted trailing blank lines\n");
    return true;
}

/**
 * Make sure there is no svn:executable keyword on the current file.
 *
 * @returns false - the state carries these kinds of changes.
 * @param   pState              The rewriter state.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_SvnNoExecutable(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    RT_NOREF2(pIn, pOut);
    if (   !pSettings->fSetSvnExecutable
        || !ScmSvnIsInWorkingCopy(pState))
        return false;

    int rc = ScmSvnQueryProperty(pState, "svn:executable", NULL);
    if (RT_SUCCESS(rc))
    {
        ScmVerbose(pState, 2, " * removing svn:executable\n");
        rc = ScmSvnDelProperty(pState, "svn:executable");
        if (RT_FAILURE(rc))
            RTMsgError("ScmSvnSetProperty: %Rrc\n", rc); /** @todo error propagation here.. */
    }
    return false;
}

/**
 * Make sure the Id and Revision keywords are expanded.
 *
 * @returns false - the state carries these kinds of changes.
 * @param   pState              The rewriter state.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_SvnKeywords(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    RT_NOREF2(pIn, pOut);
    if (   !pSettings->fSetSvnKeywords
        || !ScmSvnIsInWorkingCopy(pState))
        return false;

    char *pszKeywords;
    int rc = ScmSvnQueryProperty(pState, "svn:keywords", &pszKeywords);
    if (    RT_SUCCESS(rc)
        && (   !strstr(pszKeywords, "Id") /** @todo need some function for finding a word in a string.  */
            || !strstr(pszKeywords, "Revision")) )
    {
        if (!strstr(pszKeywords, "Id") && !strstr(pszKeywords, "Revision"))
            rc = RTStrAAppend(&pszKeywords, " Id Revision");
        else if (!strstr(pszKeywords, "Id"))
            rc = RTStrAAppend(&pszKeywords, " Id");
        else
            rc = RTStrAAppend(&pszKeywords, " Revision");
        if (RT_SUCCESS(rc))
        {
            ScmVerbose(pState, 2, " * changing svn:keywords to '%s'\n", pszKeywords);
            rc = ScmSvnSetProperty(pState, "svn:keywords", pszKeywords);
            if (RT_FAILURE(rc))
                RTMsgError("ScmSvnSetProperty: %Rrc\n", rc); /** @todo error propagation here.. */
        }
        else
            RTMsgError("RTStrAppend: %Rrc\n", rc); /** @todo error propagation here.. */
        RTStrFree(pszKeywords);
    }
    else if (rc == VERR_NOT_FOUND)
    {
        ScmVerbose(pState, 2, " * setting svn:keywords to 'Id Revision'\n");
        rc = ScmSvnSetProperty(pState, "svn:keywords", "Id Revision");
        if (RT_FAILURE(rc))
            RTMsgError("ScmSvnSetProperty: %Rrc\n", rc); /** @todo error propagation here.. */
    }
    else if (RT_SUCCESS(rc))
        RTStrFree(pszKeywords);

    return false;
}

/**
 * Makefile.kup are empty files, enforce this.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_Makefile_kup(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    RT_NOREF2(pOut, pSettings);

    /* These files should be zero bytes. */
    if (pIn->cb == 0)
        return false;
    ScmVerbose(pState, 2, " * Truncated file to zero bytes\n");
    return true;
}

/**
 * Rewrite a kBuild makefile.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 *
 * @todo
 *
 * Ideas for Makefile.kmk and Config.kmk:
 *      - sort if1of/ifn1of sets.
 *      - line continuation slashes should only be preceded by one space.
 */
bool rewrite_Makefile_kmk(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    RT_NOREF4(pState, pIn, pOut, pSettings);
    return false;
}


static bool isFlowerBoxSectionMarker(PSCMSTREAM pIn, const char *pchLine, size_t cchLine,
                                     const char **ppchText, size_t *pcchText)
{
    *ppchText = NULL;
    *pcchText = 0;

    /*
     * The first line.
     */
    if (pchLine[0] != '/')
        return false;
    size_t offLine = 1;
    while (offLine < cchLine && pchLine[offLine] == '*')
        offLine++;
    if (offLine < 20)                   /* (Code below depend on a reasonable minimum here.) */
        return false;
    while (offLine < cchLine && RT_C_IS_BLANK(pchLine[offLine]))
        offLine++;
    if (offLine != cchLine)
        return false;

    size_t const cchBox = cchLine;

    /*
     * The next line, extracting the text.
     */
    SCMEOL enmEol;
    pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol);
    if (cchLine < cchBox - 3)
        return false;

    offLine = 0;
    if (RT_C_IS_BLANK(pchLine[0]))
        offLine = RT_C_IS_BLANK(pchLine[1]) ? 2 : 1;

    if (pchLine[offLine] != '*')
        return false;
    offLine++;

    if (!RT_C_IS_BLANK(pchLine[offLine + 1]))
        return false;
    offLine++;

    while (offLine < cchLine && RT_C_IS_BLANK(pchLine[offLine]))
        offLine++;
    if (offLine >= cchLine)
        return false;
    if (!RT_C_IS_UPPER(pchLine[offLine]))
        return false;

    *ppchText = &pchLine[offLine];
    size_t const offText = offLine;

    /* From the end now. */
    offLine = cchLine - 1;
    while (RT_C_IS_BLANK(pchLine[offLine]))
        offLine--;

    if (pchLine[offLine] != '*')
        return false;
    offLine--;
    if (!RT_C_IS_BLANK(pchLine[offLine]))
        return false;
    offLine--;
    while (RT_C_IS_BLANK(pchLine[offLine]))
        offLine--;
    *pcchText = offLine - offText + 1;

    /*
     * Third line closes the box.
     */
    pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol);
    if (cchLine < cchBox - 3)
        return false;

    offLine = 0;
    if (RT_C_IS_BLANK(pchLine[0]))
        offLine = RT_C_IS_BLANK(pchLine[1]) ? 2 : 1;
    while (offLine < cchLine && pchLine[offLine] == '*')
        offLine++;
    if (offLine < cchBox - 4)
        return false;

    if (pchLine[offLine] != '/')
        return false;
    offLine++;

    while (offLine < cchLine && RT_C_IS_BLANK(pchLine[offLine]))
        offLine++;
    if (offLine != cchLine)
        return false;

    return true;
}


/**
 * Flower box marker comments in C and C++ code.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 */
bool rewrite_FixFlowerBoxMarkers(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{
    if (!pSettings->fFixFlowerBoxMarkers)
        return false;

    /*
     * Work thru the file line by line looking for flower box markers.
     */
    size_t      cChanges = 0;
    size_t      cBlankLines = 0;
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pchLine;
    while ((pchLine = ScmStreamGetLine(pIn, &cchLine, &enmEol)) != NULL)
    {
        /*
         * Get a likely match for a first line.
         */
        if (   pchLine[0] == '/'
            && cchLine > 20
            && pchLine[1] == '*'
            && pchLine[2] == '*'
            && pchLine[3] == '*')
        {
            size_t const offSaved = ScmStreamTell(pIn);
            char const  *pchText;
            size_t       cchText;
            if (isFlowerBoxSectionMarker(pIn, pchLine, cchLine, &pchText, &cchText))
            {
                while (cBlankLines < pSettings->cMinBlankLinesBeforeFlowerBoxMakers)
                {
                    ScmStreamPutEol(pOut, enmEol);
                    cBlankLines++;
                }

                ScmStreamPutCh(pOut, '/');
                ScmStreamWrite(pOut, g_szAsterisks, pSettings->cchWidth - 1);
                ScmStreamPutEol(pOut, enmEol);

                static const char s_szLead[] = "*   ";
                ScmStreamWrite(pOut, s_szLead, sizeof(s_szLead) - 1);
                ScmStreamWrite(pOut, pchText, cchText);
                size_t offCurPlus1 = sizeof(s_szLead) - 1 + cchText + 1;
                ScmStreamWrite(pOut, g_szSpaces, offCurPlus1 < pSettings->cchWidth ? pSettings->cchWidth - offCurPlus1 : 1);
                ScmStreamPutCh(pOut, '*');
                ScmStreamPutEol(pOut, enmEol);

                ScmStreamWrite(pOut, g_szAsterisks, pSettings->cchWidth - 1);
                ScmStreamPutCh(pOut, '/');
                ScmStreamPutEol(pOut, enmEol);

                cChanges++;
                cBlankLines = 0;
                continue;
            }

            int rc = ScmStreamSeekAbsolute(pIn, offSaved);
            if (RT_FAILURE(rc))
                return false;
        }

        int rc = ScmStreamPutLine(pOut, pchLine, cchLine, enmEol);
        if (RT_FAILURE(rc))
            return false;

        /* Do blank line accounting so we can ensure at least two blank lines
           before each section marker. */
        if (!isBlankLine(pchLine, cchLine))
            cBlankLines = 0;
        else
            cBlankLines++;
    }
    if (cChanges > 0)
        ScmVerbose(pState, 2, " * Converted %zu flower boxer markers\n", cChanges);
    return cChanges != 0;
}

/**
 * Rewrite a C/C++ source or header file.
 *
 * @returns true if modifications were made, false if not.
 * @param   pIn                 The input stream.
 * @param   pOut                The output stream.
 * @param   pSettings           The settings.
 *
 * @todo
 *
 * Ideas for C/C++:
 *      - space after if, while, for, switch
 *      - spaces in for (i=0;i<x;i++)
 *      - complex conditional, bird style.
 *      - remove unnecessary parentheses.
 *      - sort defined RT_OS_*||  and RT_ARCH
 *      - sizeof without parenthesis.
 *      - defined without parenthesis.
 *      - trailing spaces.
 *      - parameter indentation.
 *      - space after comma.
 *      - while (x--); -> multi line + comment.
 *      - else statement;
 *      - space between function and left parenthesis.
 *      - TODO, XXX, @todo cleanup.
 *      - Space before/after '*'.
 *      - ensure new line at end of file.
 *      - Indentation of precompiler statements (#ifdef, #defines).
 *      - space between functions.
 *      - string.h -> iprt/string.h, stdarg.h -> iprt/stdarg.h, etc.
 */
bool rewrite_C_and_CPP(PSCMRWSTATE pState, PSCMSTREAM pIn, PSCMSTREAM pOut, PCSCMSETTINGSBASE pSettings)
{

    RT_NOREF4(pState, pIn, pOut, pSettings);
    return false;
}

