/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Unicode Specification Reader.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/types.h>
#include <iprt/stdarg.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/**
 * Strip a line.
 * @returns pointer to first non-blank char.
 * @param   pszLine     The line string to strip.
 */
static char *StripLine(char *pszLine)
{
    while (*pszLine == ' ' || *pszLine == '\t')
        pszLine++;

    char *psz = strchr(pszLine, '#');
    if (psz)
        *psz = '\0';
    else
        psz = strchr(pszLine, '\0');
    while (psz > pszLine)
    {
        switch (psz[-1])
        {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                *--psz = '\0';
                continue;
        }
        break;
    }

    return pszLine;
}


/**
 * Checks if the line is blank or a comment line and should be skipped.
 * @returns true/false.
 * @param   pszLine     The line to consider.
 */
static bool IsCommentOrBlankLine(const char *pszLine)
{
    while (*pszLine == ' ' || *pszLine == '\t' || *pszLine == '\n' || *pszLine == '\r')
        pszLine++;
    return *pszLine == '#' || *pszLine == '\0';
}


/**
 * Get the first field in the string.
 *
 * @returns Pointer to the next field.
 * @param   ppsz        Where to store the pointer to the next field.
 * @param   pszLine     The line string. (could also be *ppsz from a FirstNext call)
 */
static char *FirstField(char **ppsz, char *pszLine)
{
    char *psz = strchr(pszLine, ';');
    if (!psz)
        *ppsz = psz = strchr(pszLine, '\0');
    else
    {
        *psz = '\0';
        *ppsz = psz + 1;
    }

    /* strip */
    while (*pszLine == ' ' || *pszLine == '\t' || *pszLine == '\r' || *pszLine == '\n')
        pszLine++;
    while (psz > pszLine)
    {
        switch (psz[-1])
        {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                *--psz = '\0';
                continue;
        }
        break;
    }
    return pszLine;
}


/**
 * Get the next field in a field enumeration.
 *
 * @returns Pointer to the next field.
 * @param   ppsz        Where to get and store the string postition.
 */
static char *NextField(char **ppsz)
{
    return FirstField(ppsz, *ppsz);
}


/**
 * Converts a code point field to a number.
 * @returns Code point.
 * @param   psz     The field string.
 */
static RTUNICP ToNum(const char *psz)
{
    char *pszEnd = NULL;
    unsigned long ul = strtoul(psz, &pszEnd, 16);
    if (pszEnd && *pszEnd)
        fprintf(stderr, "warning: failed converting '%s' to a number!\n", psz);
    return (RTUNICP)ul;
}


/**
 * Same as ToNum except that if the field is empty the Default is returned.
 */
static RTUNICP ToNumDefault(const char *psz, RTUNICP Default)
{
    if (*psz)
        return ToNum(psz);
    return Default;
}


/**
 * Converts a code point range to numbers.
 * @returns The start code point.\
 * @returns ~(RTUNICP)0 on failure.
 * @param   psz     The field string.
 * @param   pLast   Where to store the last code point in the range.
 */
static RTUNICP ToRange(const char *psz, PRTUNICP pLast)
{
    char *pszEnd = NULL;
    unsigned long ulStart = strtoul(psz, &pszEnd, 16);
    unsigned long ulLast = ulStart;
    if (pszEnd && *pszEnd)
    {
        if (*pszEnd == '.')
        {
            while (*pszEnd == '.')
                pszEnd++;
            ulLast = strtoul(pszEnd, &pszEnd, 16);
            if (pszEnd && *pszEnd)
            {
                fprintf(stderr, "warning: failed converting '%s' to a number!\n", psz);
                return ~(RTUNICP)0;
            }
        }
        else
        {
            fprintf(stderr, "warning: failed converting '%s' to a number!\n", psz);
            return ~(RTUNICP)0;
        }
    }
    *pLast = (RTUNICP)ulLast;
    return (RTUNICP)ulStart;

}


/**
 * Duplicate a string, optimize certain strings to save memory.
 *
 * @returns Pointer to string copy.
 * @param   pszStr      The string to duplicate.
 */
static char *DupStr(const char *pszStr)
{
    if (!*pszStr)
        return (char*)"";
    char *psz = strdup(pszStr);
    if (psz)
        return psz;

    fprintf(stderr, "out of memory!\n");
    exit(1);
}


/**
 * Array of all possible and impossible unicode code points as of 4.1
 */
struct CPINFO
{
    RTUNICP     CodePoint;
    RTUNICP     SimpleUpperCaseMapping;
    RTUNICP     SimpleLowerCaseMapping;
    RTUNICP     SimpleTitleCaseMapping;
    const char *pszName;
    /** Set if this is an unused entry */
    unsigned    fNullEntry : 1;

    unsigned    fAlphabetic : 1;
    unsigned    fASCIIHexDigit : 1;
    unsigned    fBidiControl : 1;
    unsigned    fDash : 1;
    unsigned    fDefaultIgnorableCodePoint : 1;
    unsigned    fDeprecated : 1;
    unsigned    fDiacritic : 1;
    unsigned    fExtender : 1;
    unsigned    fGraphemeBase : 1;
    unsigned    fGraphemeExtend : 1;
    unsigned    fGraphemeLink : 1;
    unsigned    fHexDigit : 1;
    unsigned    fHyphen : 1;
    unsigned    fIDContinue : 1;
    unsigned    fIdeographic : 1;
    unsigned    fIDSBinaryOperator : 1;
    unsigned    fIDStart : 1;
    unsigned    fIDSTrinaryOperator : 1;
    unsigned    fJoinControl : 1;
    unsigned    fLogicalOrderException : 1;
    unsigned    fLowercase : 1;
    unsigned    fMath : 1;
    unsigned    fNoncharacterCodePoint : 1;
    unsigned    fOtherAlphabetic : 1;
    unsigned    fOtherDefaultIgnorableCodePoint : 1;
    unsigned    fOtherGraphemeExtend : 1;
    unsigned    fOtherIDContinue : 1;
    unsigned    fOtherIDStart : 1;
    unsigned    fOtherLowercase : 1;
    unsigned    fOtherMath : 1;
    unsigned    fOtherUppercase : 1;
    unsigned    fPatternSyntax : 1;
    unsigned    fPatternWhiteSpace : 1;
    unsigned    fQuotationMark : 1;
    unsigned    fRadical : 1;
    unsigned    fSoftDotted : 1;
    unsigned    fSTerm : 1;
    unsigned    fTerminalPunctuation : 1;
    unsigned    fUnifiedIdeograph : 1;
    unsigned    fUppercase : 1;
    unsigned    fVariationSelector : 1;
    unsigned    fWhiteSpace : 1;
    unsigned    fXIDContinue : 1;
    unsigned    fXIDStart : 1;

    /* unprocess stuff, so far. */
    const char *pszGeneralCategory;
    const char *pszCanonicalCombiningClass;
    const char *pszBidiClass;
    const char *pszDecompositionType;
    const char *pszDecompositionMapping;
    const char *pszNumericType;
    const char *pszNumericValue;
    const char *pszBidiMirrored;
    const char *pszUnicode1Name;
    const char *pszISOComment;
} g_aCPInfo[0xf0000];


/**
 * Creates a 'null' entry at i.
 * @param   i       The entry in question.
 */
static void NullEntry(unsigned i)
{
    g_aCPInfo[i].CodePoint = i;
    g_aCPInfo[i].fNullEntry = 1;
    g_aCPInfo[i].pszName = "";
    g_aCPInfo[i].SimpleUpperCaseMapping = i;
    g_aCPInfo[i].SimpleLowerCaseMapping = i;
    g_aCPInfo[i].SimpleTitleCaseMapping = i;
    g_aCPInfo[i].pszGeneralCategory = "";
    g_aCPInfo[i].pszCanonicalCombiningClass = "";
    g_aCPInfo[i].pszBidiClass = "";
    g_aCPInfo[i].pszDecompositionType = "";
    g_aCPInfo[i].pszDecompositionMapping = "";
    g_aCPInfo[i].pszNumericType = "";
    g_aCPInfo[i].pszNumericValue = "";
    g_aCPInfo[i].pszBidiMirrored = "";
    g_aCPInfo[i].pszUnicode1Name = "";
    g_aCPInfo[i].pszISOComment = "";
}


/**
 * Read the UnicodeData.txt file.
 * @returns 0 on success.
 * @returns !0 on failure.
 * @param   pszFilename     The name of the file.
 */
static int ReadUnicodeData(const char *pszFilename)
{
    /*
     * Open input.
     */
    FILE *pFile = fopen(pszFilename, "r");
    if (!pFile)
    {
        printf("uniread: failed to open '%s' for reading\n", pszFilename);
        return 1;
    }

    /*
     * Parse the input and spit out the output.
     */
    char szLine[4096];
    RTUNICP i = 0;
    while (fgets(szLine, sizeof(szLine), pFile) != NULL)
    {
        if (IsCommentOrBlankLine(szLine))
            continue;

        char *pszCurField;
        char *pszCodePoint = FirstField(&pszCurField, StripLine(szLine)); /* 0 */
        char *pszName = NextField(&pszCurField);                          /* 1 */
        char *pszGeneralCategory = NextField(&pszCurField);               /* 2 */
        char *pszCanonicalCombiningClass = NextField(&pszCurField);       /* 3 */
        char *pszBidiClass = NextField(&pszCurField);                     /* 4 */
        char *pszDecompositionType = NextField(&pszCurField);             /* 5 */
        char *pszDecompositionMapping = NextField(&pszCurField);          /* 6 */
        char *pszNumericType = NextField(&pszCurField);                   /* 7 */
        char *pszNumericValue = NextField(&pszCurField);                  /* 8 */
        char *pszBidiMirrored = NextField(&pszCurField);                  /* 9 */
        char *pszUnicode1Name = NextField(&pszCurField);                  /* 10 */
        char *pszISOComment = NextField(&pszCurField);                    /* 11 */
        char *pszSimpleUpperCaseMapping = NextField(&pszCurField);        /* 12 */
        char *pszSimpleLowerCaseMapping = NextField(&pszCurField);        /* 13 */
        char *pszSimpleTitleCaseMapping = NextField(&pszCurField);        /* 14 */

        RTUNICP CodePoint = ToNum(pszCodePoint);
        if (CodePoint >= ELEMENTS(g_aCPInfo))
            continue;

        /* catchup? */
        while (i < CodePoint)
            NullEntry(i++);
        if (i != CodePoint)
        {
            fprintf(stderr, "unitest: error: i=%d CodePoint=%u\n", i, CodePoint);
            fclose(pFile);
            return 1;
        }

        /* this one */
        g_aCPInfo[i].CodePoint = i;
        g_aCPInfo[i].fNullEntry = 0;
        g_aCPInfo[i].pszName                    = DupStr(pszName);
        g_aCPInfo[i].SimpleUpperCaseMapping     = ToNumDefault(pszSimpleUpperCaseMapping, CodePoint);
        g_aCPInfo[i].SimpleLowerCaseMapping     = ToNumDefault(pszSimpleLowerCaseMapping, CodePoint);
        g_aCPInfo[i].SimpleTitleCaseMapping     = ToNumDefault(pszSimpleTitleCaseMapping, CodePoint);
        g_aCPInfo[i].pszGeneralCategory         = DupStr(pszGeneralCategory);
        g_aCPInfo[i].pszCanonicalCombiningClass = DupStr(pszCanonicalCombiningClass);
        g_aCPInfo[i].pszBidiClass               = DupStr(pszBidiClass);
        g_aCPInfo[i].pszDecompositionType       = DupStr(pszDecompositionType);
        g_aCPInfo[i].pszDecompositionMapping    = DupStr(pszDecompositionMapping);
        g_aCPInfo[i].pszNumericType             = DupStr(pszNumericType);
        g_aCPInfo[i].pszNumericValue            = DupStr(pszNumericValue);
        g_aCPInfo[i].pszBidiMirrored            = DupStr(pszBidiMirrored);
        g_aCPInfo[i].pszUnicode1Name            = DupStr(pszUnicode1Name);
        g_aCPInfo[i].pszISOComment              = DupStr(pszISOComment);
        i++;
    }
    /* catchup? */
    while (i < ELEMENTS(g_aCPInfo))
        NullEntry(i++);
    fclose(pFile);

    return 0;
}


/**
 * Applies a property to a code point.
 *
 * @param   StartCP     The code point.
 * @param   pszProperty The property name.
 */
static void ApplyProperty(RTUNICP StartCP, const char *pszProperty)
{
    if (StartCP >= ELEMENTS(g_aCPInfo))
        return;
    struct CPINFO *pCPInfo = &g_aCPInfo[StartCP];
    /* string switch */
    if (!strcmp(pszProperty, "ASCII_Hex_Digit")) pCPInfo->fASCIIHexDigit = 1;
    else if (!strcmp(pszProperty, "Bidi_Control")) pCPInfo->fBidiControl = 1;
    else if (!strcmp(pszProperty, "Dash")) pCPInfo->fDash = 1;
    else if (!strcmp(pszProperty, "Deprecated")) pCPInfo->fDeprecated = 1;
    else if (!strcmp(pszProperty, "Diacritic")) pCPInfo->fDiacritic = 1;
    else if (!strcmp(pszProperty, "Extender")) pCPInfo->fExtender = 1;
    else if (!strcmp(pszProperty, "Grapheme_Link")) pCPInfo->fGraphemeLink = 1;
    else if (!strcmp(pszProperty, "Hex_Digit")) pCPInfo->fHexDigit = 1;
    else if (!strcmp(pszProperty, "Hyphen")) pCPInfo->fHyphen = 1;
    else if (!strcmp(pszProperty, "Ideographic")) pCPInfo->fIdeographic = 1;
    else if (!strcmp(pszProperty, "IDS_Binary_Operator")) pCPInfo->fIDSBinaryOperator = 1;
    else if (!strcmp(pszProperty, "IDS_Trinary_Operator")) pCPInfo->fIDSTrinaryOperator = 1;
    else if (!strcmp(pszProperty, "Join_Control")) pCPInfo->fJoinControl = 1;
    else if (!strcmp(pszProperty, "Logical_Order_Exception")) pCPInfo->fLogicalOrderException = 1;
    else if (!strcmp(pszProperty, "Noncharacter_Code_Point")) pCPInfo->fNoncharacterCodePoint = 1;
    else if (!strcmp(pszProperty, "Other_Alphabetic")) pCPInfo->fOtherAlphabetic = 1;
    else if (!strcmp(pszProperty, "Other_Default_Ignorable_Code_Point")) pCPInfo->fOtherDefaultIgnorableCodePoint = 1;
    else if (!strcmp(pszProperty, "Other_Grapheme_Extend")) pCPInfo->fOtherGraphemeExtend = 1;
    else if (!strcmp(pszProperty, "Other_ID_Continue")) pCPInfo->fOtherIDContinue = 1;
    else if (!strcmp(pszProperty, "Other_ID_Start")) pCPInfo->fOtherIDStart = 1;
    else if (!strcmp(pszProperty, "Other_Lowercase")) pCPInfo->fOtherLowercase = 1;
    else if (!strcmp(pszProperty, "Other_Math")) pCPInfo->fOtherMath = 1;
    else if (!strcmp(pszProperty, "Other_Uppercase")) pCPInfo->fOtherUppercase = 1;
    else if (!strcmp(pszProperty, "Alphabetic")) pCPInfo->fAlphabetic = 1;
    else if (!strcmp(pszProperty, "Default_Ignorable_Code_Point")) pCPInfo->fDefaultIgnorableCodePoint = 1;
    else if (!strcmp(pszProperty, "Grapheme_Base")) pCPInfo->fGraphemeBase = 1;
    else if (!strcmp(pszProperty, "Grapheme_Extend")) pCPInfo->fGraphemeExtend = 1;
    else if (!strcmp(pszProperty, "ID_Continue")) pCPInfo->fIDContinue = 1;
    else if (!strcmp(pszProperty, "ID_Start")) pCPInfo->fIDStart = 1;
    else if (!strcmp(pszProperty, "XID_Continue")) pCPInfo->fXIDContinue = 1;
    else if (!strcmp(pszProperty, "XID_Start")) pCPInfo->fXIDStart = 1;
    else if (!strcmp(pszProperty, "Lowercase")) pCPInfo->fLowercase = 1;
    else if (!strcmp(pszProperty, "Math")) pCPInfo->fMath = 1;
    else if (!strcmp(pszProperty, "Uppercase")) pCPInfo->fUppercase = 1;
    else if (!strcmp(pszProperty, "Pattern_Syntax")) pCPInfo->fPatternSyntax = 1;
    else if (!strcmp(pszProperty, "Pattern_White_Space")) pCPInfo->fPatternWhiteSpace = 1;
    else if (!strcmp(pszProperty, "Quotation_Mark")) pCPInfo->fQuotationMark = 1;
    else if (!strcmp(pszProperty, "Radical")) pCPInfo->fRadical = 1;
    else if (!strcmp(pszProperty, "Soft_Dotted")) pCPInfo->fSoftDotted = 1;
    else if (!strcmp(pszProperty, "STerm")) pCPInfo->fSTerm = 1;
    else if (!strcmp(pszProperty, "Terminal_Punctuation")) pCPInfo->fTerminalPunctuation = 1;
    else if (!strcmp(pszProperty, "Unified_Ideograph")) pCPInfo->fUnifiedIdeograph = 1;
    else if (!strcmp(pszProperty, "Variation_Selector")) pCPInfo->fVariationSelector = 1;
    else if (!strcmp(pszProperty, "White_Space")) pCPInfo->fWhiteSpace = 1;
    else
        fprintf(stderr, "uniread: Unknown property '%s'\n", pszProperty);
}


/**
 * Reads a property file.
 *
 * There are several property files, this code can read all
 * of those but will only make use of the properties it recognizes.
 *
 * @returns 0 on success.
 * @returns !0 on failure.
 * @param   pszFilename     The name of the file.
 */
static int ReadProperties(const char *pszFilename)
{
    /*
     * Open input.
     */
    FILE *pFile = fopen(pszFilename, "r");
    if (!pFile)
    {
        printf("uniread: failed to open '%s' for reading\n", pszFilename);
        return 1;
    }

    /*
     * Parse the input and spit out the output.
     */
    char szLine[4096];
    while (fgets(szLine, sizeof(szLine), pFile) != NULL)
    {
        if (IsCommentOrBlankLine(szLine))
            continue;
        char *pszCurField;
        char *pszRange = FirstField(&pszCurField, StripLine(szLine));
        char *pszProperty = NextField(&pszCurField);
        if (!*pszProperty)
            continue;

        RTUNICP LastCP;
        RTUNICP StartCP = ToRange(pszRange, &LastCP);
        if (StartCP == ~(RTUNICP)0)
            continue;

        while (StartCP <= LastCP)
            ApplyProperty(StartCP++, pszProperty);
    }

    fclose(pFile);

    return 0;
}


/**
 * Append a flag to the string.
 */
static char *AppendFlag(char *psz, const char *pszFlag)
{
    char *pszEnd = strchr(psz, '\0');
    if (pszEnd != psz)
    {
        *pszEnd++ = ' ';
        *pszEnd++ = '|';
        *pszEnd++ = ' ';
    }
    strcpy(pszEnd, pszFlag);
    return psz;
}

/**
 * Calcs the flags for a code point.
 * @returns true if there is a flag.
 * @returns false if the isn't.
 */
static bool CalcFlags(struct CPINFO *pInfo, char *pszFlags)
{
    pszFlags[0] = '\0';
    /** @todo read the specs on this other vs standard stuff, and check out the finer points */
    if (pInfo->fAlphabetic || pInfo->fOtherAlphabetic)
        AppendFlag(pszFlags, "RTUNI_ALPHA");
    if (pInfo->fHexDigit || pInfo->fASCIIHexDigit)
        AppendFlag(pszFlags, "RTUNI_XDIGIT");
    if (!strcmp(pInfo->pszGeneralCategory, "Nd"))
        AppendFlag(pszFlags, "RTUNI_DDIGIT");
    if (pInfo->fWhiteSpace)
        AppendFlag(pszFlags, "RTUNI_WSPACE");
    if (pInfo->fUppercase || pInfo->fOtherUppercase)
        AppendFlag(pszFlags, "RTUNI_UPPER");
    if (pInfo->fLowercase || pInfo->fOtherLowercase)
        AppendFlag(pszFlags, "RTUNI_LOWER");
    //if (pInfo->fNumeric)
    //    AppendFlag(pszFlags, "RTUNI_NUMERIC");
    if (!*pszFlags)
    {
        pszFlags[0] = '0';
        pszFlags[1] = '\0';
        return false;
    }
    return true;
}

/** the data store for stream two. */
static char g_szStream2[10240];
static unsigned g_offStream2 = 0;

/**
 * Initializes the 2nd steam.
 */
static void Stream2Init(void)
{
    g_szStream2[0] = '\0';
    g_offStream2 = 0;
}

/**
 * Flushes the 2nd stream to stdout.
 */
static int Stream2Flush(void)
{
    fwrite(g_szStream2, 1, g_offStream2, stdout);
    return 0;
}

/**
 * printf to the 2nd stream.
 */
static int Stream2Printf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = vsprintf(&g_szStream2[g_offStream2], pszFormat, va);
    va_end(va);
    g_offStream2 += cch;
    if (g_offStream2 >= sizeof(g_szStream2))
    {
        fprintf(stderr, "error: stream2 overflow!\n");
        exit(1);
    }
    return cch;
}


/**
 * Print the unidata.cpp file header and include list.
 */
int PrintHeader(const char *argv0)
{
    /*
     * Print file header.
     */
    printf("/** @file\n"
           " *\n"
           " * Incredibly Portable Runtime - Unicode Tables\n"
           " *\n"
           " *      Automatically Generated by %s (" __DATE__ " " __TIME__ ")\n"
           " */\n\n"
           "/*\n"
           " * Copyright (C) 2006-2008 Sun Microsystems, Inc.\n"
           " *\n"
           " * This file is part of VirtualBox Open Source Edition (OSE), as\n"
           " * available from http://www.virtualbox.org. This file is free software;\n"
           " * you can redistribute it and/or modify it under the terms of the GNU\n"
           " * General Public License as published by the Free Software Foundation,\n"
           " * in version 2 as it comes in the \"COPYING\" file of the VirtualBox OSE\n"
           " * distribution. VirtualBox OSE is distributed in the hope that it will\n"
           " * be useful, but WITHOUT ANY WARRANTY of any kind.\n"
           " *\n"
           "\n"
           "#include <iprt/uni.h>\n"
           "\n",
           argv0);
    return 0;
}


/**
 * Print the flag tables.
 */
int PrintFlags(void)
{
    /*
     * Print flags table.
     */
    Stream2Init();
    Stream2Printf("const RTUNIFLAGSRANGE g_aRTUniFlagRanges[] =\n"
                  "{\n");
    RTUNICP i = 0;
    int iStart = -1;
    while (i < ELEMENTS(g_aCPInfo))
    {
        /* figure how far off the next chunk is */
        char szFlags[256];
        unsigned iNonNull = i;
        while (     (g_aCPInfo[iNonNull].fNullEntry || !CalcFlags(&g_aCPInfo[iNonNull], szFlags))
               &&   iNonNull < ELEMENTS(g_aCPInfo)
               &&   iNonNull >= 256)
            iNonNull++;
        if (iNonNull - i > 4096 || iNonNull == ELEMENTS(g_aCPInfo))
        {
            if (iStart >= 0)
            {
                printf("};\n\n");
                Stream2Printf("    { 0x%06x, 0x%06x, &g_afRTUniFlags0x%06x[0] },\n", iStart, i, iStart);
                iStart = -1;
            }
            i = iNonNull;
        }
        else
        {
            if (iStart < 0)
            {
                printf("static const uint8_t g_afRTUniFlags0x%06x[] = \n"
                       "{\n", i);
                iStart = i;
            }
            CalcFlags(&g_aCPInfo[i], szFlags);
            printf("    %50s, /* U+%06x: %s*/\n", szFlags, g_aCPInfo[i].CodePoint, g_aCPInfo[i].pszName);
            i++;
        }
    }
    Stream2Printf("    { ~(RTUNICP)0, ~(RTUNICP)0, NULL }\n"
                  "};\n\n\n");
    printf("\n");
    return Stream2Flush();
}


/**
 * Prints the upper case tables.
 */
static int PrintUpper(void)
{
    Stream2Init();
    Stream2Printf("const RTUNICASERANGE g_aRTUniUpperRanges[] =\n"
                  "{\n");
    RTUNICP i = 0;
    int iStart = -1;
    while (i < ELEMENTS(g_aCPInfo))
    {
        /* figure how far off the next chunk is */
        unsigned iSameCase = i;
        while (     g_aCPInfo[iSameCase].SimpleUpperCaseMapping == g_aCPInfo[iSameCase].CodePoint
               &&   iSameCase < ELEMENTS(g_aCPInfo)
               &&   iSameCase >= 256)
            iSameCase++;
        if (iSameCase - i > 4096/sizeof(RTUNICP) || iSameCase == ELEMENTS(g_aCPInfo))
        {
            if (iStart >= 0)
            {
                printf("};\n\n");
                Stream2Printf("    { 0x%06x, 0x%06x, &g_afRTUniUpper0x%06x[0] },\n", iStart, i, iStart);
                iStart = -1;
            }
            i = iSameCase;
        }
        else
        {
            if (iStart < 0)
            {
                printf("static const RTUNICP g_afRTUniUpper0x%06x[] = \n"
                       "{\n", i);
                iStart = i;
            }
            printf("    0x%02x, /* U+%06x: %s*/\n", g_aCPInfo[i].SimpleUpperCaseMapping, g_aCPInfo[i].CodePoint, g_aCPInfo[i].pszName);
            i++;
        }
    }
    Stream2Printf("    { ~(RTUNICP)0, ~(RTUNICP)0, NULL }\n"
                  "};\n\n\n");
    printf("\n");
    return Stream2Flush();
}


/**
 * Prints the lowercase tables.
 */
static int PrintLower(void)
{
    Stream2Init();
    Stream2Printf("const RTUNICASERANGE g_aRTUniLowerRanges[] =\n"
                  "{\n");
    RTUNICP i = 0;
    int iStart = -1;
    while (i < ELEMENTS(g_aCPInfo))
    {
        /* figure how far off the next chunk is */
        unsigned iSameCase = i;
        while (     g_aCPInfo[iSameCase].SimpleLowerCaseMapping == g_aCPInfo[iSameCase].CodePoint
               &&   iSameCase < ELEMENTS(g_aCPInfo)
               &&   iSameCase >= 256)
            iSameCase++;
        if (iSameCase - i > 4096/sizeof(RTUNICP) || iSameCase == ELEMENTS(g_aCPInfo))
        {
            if (iStart >= 0)
            {
                printf("};\n\n");
                Stream2Printf("    { 0x%06x, 0x%06x, &g_afRTUniLower0x%06x[0] },\n", iStart, i, iStart);
                iStart = -1;
            }
            i = iSameCase;
        }
        else
        {
            if (iStart < 0)
            {
                printf("static const RTUNICP g_afRTUniLower0x%06x[] = \n"
                       "{\n", i);
                iStart = i;
            }
            printf("    0x%02x, /* U+%06x: %s*/\n", g_aCPInfo[i].SimpleLowerCaseMapping, g_aCPInfo[i].CodePoint, g_aCPInfo[i].pszName);
            i++;
        }
    }
    Stream2Printf("    { ~(RTUNICP)0, ~(RTUNICP)0, NULL }\n"
                  "};\n\n\n");
    printf("\n");
    return Stream2Flush();
}


int main(int argc, char **argv)
{
    /*
     * Parse args.
     */
    if (argc <= 1)
    {
        printf("usage: %s [UnicodeData.txt [DerivedCoreProperties.txt [PropList.txt]]]\n", argv[0]);
        return 1;
    }

    const char *pszUnicodeData              = "UnicodeData.txt";
    const char *pszDerivedCoreProperties    = "DerivedCoreProperties.txt";
    const char *pszPropList                 = "PropList.txt";
    int iFile = 0;
    for (int argi = 1;  argi < argc; argi++)
    {
        if (argv[argi][0] != '-')
        {
            switch (iFile++)
            {
                case 0: pszUnicodeData = argv[argi]; break;
                case 1: pszDerivedCoreProperties = argv[argi]; break;
                case 2: pszPropList = argv[argi]; break;
                default:
                    printf("uniread: syntax error at '%s': too many filenames\n", argv[argi]);
                    return 1;
            }
        }
        else
        {
            printf("uniread: syntax error at '%s': Unknown argument\n", argv[argi]);
            return 1;
        }
    }

    /*
     * Read the data.
     */
    int rc = ReadUnicodeData(pszUnicodeData);
    if (rc)
        return rc;
    rc = ReadProperties(pszPropList);
    if (rc)
        return rc;
    rc = ReadProperties(pszDerivedCoreProperties);
    if (rc)
        return rc;

    /*
     * Print stuff.
     */
    rc = PrintHeader(argv[0]);
    if (rc)
        return rc;
    rc = PrintFlags();
    if (rc)
        return rc;
    rc = PrintUpper();
    if (rc)
        return rc;
    rc = PrintLower();
    if (rc)
        return rc;

    /* done */
    fflush(stdout);

    return rc;
}

