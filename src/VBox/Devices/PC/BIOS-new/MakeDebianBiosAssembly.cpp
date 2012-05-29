/* $Id$ */
/** @file
 * MakeDebianBiosAssembly - Generate Assembly Source for Debian-minded Distros.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dbg.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/stream.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * A BIOS segment.
 */
typedef struct BIOSSEG
{
    char        szName[32];
    char        szClass[32];
    char        szGroup[32];
    RTFAR16     Address;
    uint32_t    uFlatAddr;
    uint32_t    cb;
} BIOSSEG;
/** Pointer to a BIOS segment. */
typedef BIOSSEG *PBIOSSEG;


/**
 * Pointer to a BIOS map parser handle.
 */
typedef struct BIOSMAP
{
    /** The stream pointer. */
    PRTSTREAM   hStrm;
    /** The file name. */
    const char *pszMapFile;
    /** Set when EOF has been reached. */
    bool        fEof;
    /** The current line number (0 based).*/
    uint32_t    iLine;
    /** The length of the current line. */
    uint32_t    cch;
    /** The offset of the first non-white character on the line. */
    uint32_t    offNW;
    /** The line buffer. */
    char        szLine[16384];
} BIOSMAP;
/** Pointer to a BIOS map parser handle. */
typedef BIOSMAP *PBIOSMAP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned         g_cVerbose = 1 /*0*/;
/** Pointer to the BIOS image. */
static uint8_t const   *g_pbImg;
/** The size of the BIOS image. */
static size_t           g_cbImg;

/** Debug module for the map file.  */
static RTDBGMOD         g_hMapMod = NIL_RTDBGMOD;
/** The number of BIOS segments found in the map file. */
static uint32_t         g_cSegs = 0;
/** Array of BIOS segments from the map file. */
static BIOSSEG          g_aSegs[32];

/** The output stream. */
static PRTSTREAM        g_hStrmOutput = NULL;


static bool outputPrintfV(const char *pszFormat, va_list va)
{
    int rc = RTStrmPrintfV(g_hStrmOutput, pszFormat, va);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Output error: %Rrc\n", rc);
        return false;
    }
    return true;
}


static bool outputPrintf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    bool fRc = outputPrintfV(pszFormat, va);
    va_end(va);
    return fRc;
}


/**
 * Opens the output file for writing.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszOutput           Path to the output file.
 */
static RTEXITCODE OpenOutputFile(const char *pszOutput)
{
    if (!pszOutput)
        g_hStrmOutput = g_pStdOut;
    else
    {
        int rc = RTStrmOpen(pszOutput, "w", &g_hStrmOutput);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to open output file '%s': %Rrc", pszOutput, rc);
    }
    return RTEXITCODE_SUCCESS;
}


/**
 * Output the disassembly file header.
 *
 * @returns @c true on success,
 */
static bool disFileHeader(void)
{
    bool fRc = true;
    fRc = fRc && outputPrintf("; $Id$ \n"
                              ";; @file\n"
                              "; Auto Generated source file. Do not edit.\n"
                              ";\n"
                              "\n"
                              "org 0xf000\n"
                              "\n"
                              );
    return fRc;
}


static bool disByteData(uint32_t uFlatAddr, uint32_t cb)
{
    uint8_t const  *pb = &g_pbImg[uFlatAddr - 0xf0000];
    size_t          cbOnLine = 0;
    while (cb-- > 0)
    {
        bool fRc;
        if (cbOnLine >= 16)
        {
            fRc = outputPrintf("\n"
                               "  db    0%02xh", *pb);
            cbOnLine = 1;
        }
        else if (!cbOnLine)
        {
            fRc = outputPrintf("  db    0%02xh", *pb);
            cbOnLine = 1;
        }
        else
        {
            fRc = outputPrintf(", 0%02xh", *pb);
            cbOnLine++;
        }
        if (!fRc)
            return false;
        pb++;
    }
    return outputPrintf("\n");
}



static bool disCopySegmentGap(uint32_t uFlatAddr, uint32_t cbPadding)
{
    if (g_cVerbose > 0)
        RTStrmPrintf(g_pStdErr, "Padding %#x bytes at %#x\n", cbPadding, uFlatAddr);
    return disByteData(uFlatAddr, cbPadding);
}


static bool disDataSegment(uint32_t iSeg)
{
    return disByteData(g_aSegs[iSeg].uFlatAddr, g_aSegs[iSeg].cb);
}


static bool disCodeSegment(uint32_t iSeg)
{
    return disDataSegment(iSeg);
}



static RTEXITCODE DisassembleBiosImage(void)
{
    if (!outputPrintf(""))
        return RTEXITCODE_FAILURE;

    /*
     * Work the image segment by segment.
     */
    bool        fRc       = true;
    uint32_t    uFlatAddr = 0xf0000;
    for (uint32_t iSeg = 0; iSeg < g_cSegs && fRc; iSeg++)
    {
        /* Is there a gap between the segments? */
        if (uFlatAddr < g_aSegs[iSeg].uFlatAddr)
        {
            fRc = disCopySegmentGap(uFlatAddr, g_aSegs[iSeg].uFlatAddr - uFlatAddr);
            if (!fRc)
                break;
            uFlatAddr = g_aSegs[iSeg].uFlatAddr;
        }
        else if (uFlatAddr > g_aSegs[iSeg].uFlatAddr)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Overlapping segments: %u and %u; uFlatAddr=%#x\n", iSeg - 1, iSeg, uFlatAddr);

        /* Disassemble the segment. */
        fRc = outputPrintf("\n"
                           "section %s progbits vstart=%#x align=1\n",
                           g_aSegs[iSeg].szName, g_aSegs[iSeg].uFlatAddr);
        if (!fRc)
            return RTEXITCODE_FAILURE;
        if (!strcmp(g_aSegs[iSeg].szClass, "DATA"))
            fRc = disDataSegment(iSeg);
        else
            fRc = disCodeSegment(iSeg);

        /* Advance. */
        uFlatAddr += g_aSegs[iSeg].cb;
    }

    /* Final gap. */
    if (uFlatAddr < 0x100000)
        fRc = disCopySegmentGap(uFlatAddr, 0x100000 - uFlatAddr);
    else if (uFlatAddr > 0x100000)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Last segment spills beyond 1MB; uFlatAddr=%#x\n", uFlatAddr);

    if (!fRc)
        return RTEXITCODE_FAILURE;
    return RTEXITCODE_SUCCESS;
}



/**
 * Parses the symbol file for the BIOS.
 *
 * This is in ELF/DWARF format.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosSym          Path to the sym file.
 */
static RTEXITCODE ParseSymFile(const char *pszBiosSym)
{
    /** @todo use RTDbg* later. (Just checking for existance currently.) */
    PRTSTREAM hStrm;
    int rc = RTStrmOpen(pszBiosSym, "rb", &hStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening '%s': %Rrc", pszBiosSym, rc);
    RTStrmClose(hStrm);
    return RTEXITCODE_SUCCESS;
}


/**
 * Reads a line from the file.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapReadLine(PBIOSMAP pMap)
{
    int rc = RTStrmGetLine(pMap->hStrm, pMap->szLine, sizeof(pMap->szLine));
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_EOF)
        {
            pMap->fEof      = true;
            pMap->cch       = 0;
            pMap->offNW     = 0;
            pMap->szLine[0] = '\0';
        }
        else
            RTMsgError("%s:%d: Read error %Rrc", pMap->pszMapFile, pMap->iLine + 1, rc);
        return false;
    }
    pMap->iLine++;
    pMap->cch   = strlen(pMap->szLine);

    /* Check out leading white space. */
    if (!RT_C_IS_SPACE(pMap->szLine[0]))
        pMap->offNW = 0;
    else
    {
        uint32_t off = 1;
        while (RT_C_IS_SPACE(pMap->szLine[off]))
            off++;
        pMap->offNW = off;
    }

    return true;
}


/**
 * Checks if it is an empty line.
 * @returns @c true if empty, @c false if not.
 * @param   pMap                The map file handle.
 */
static bool mapIsEmptyLine(PBIOSMAP pMap)
{
    Assert(pMap->offNW <= pMap->cch);
    return pMap->offNW == pMap->cch;
}


/**
 * Reads ahead in the map file until a non-empty line or EOF is encountered.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapSkipEmptyLines(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLine(pMap))
            return false;
        if (pMap->offNW < pMap->cch)
            return true;
    }
}


/**
 * Reads ahead in the map file until an empty line or EOF is encountered.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapSkipNonEmptyLines(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLine(pMap))
            return false;
        if (pMap->offNW == pMap->cch)
            return true;
    }
}


/**
 * Strips the current line.
 *
 * The string length may change.
 *
 * @returns Pointer to the first non-space character.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapStripCurrentLine(PBIOSMAP pMap, size_t *pcch)
{
    char *psz    = &pMap->szLine[pMap->offNW];
    char *pszEnd = &pMap->szLine[pMap->cch];
    while (   (uintptr_t)pszEnd > (uintptr_t)psz
           && RT_C_IS_SPACE(pszEnd[-1]))
    {
        *--pszEnd = '\0';
        pMap->cch--;
    }
    if (pcch)
        *pcch = pszEnd - psz;
    return psz;
}


/**
 * Reads a line from the file and right strips it.
 *
 * @returns Pointer to szLine on success, @c NULL + msg on failure, @c NULL on
 *          EOF.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapReadLineStripRight(PBIOSMAP pMap, size_t *pcch)
{
    if (!mapReadLine(pMap))
        return NULL;
    mapStripCurrentLine(pMap, NULL);
    if (pcch)
        *pcch = pMap->cch;
    return pMap->szLine;
}


/**
 * mapReadLine() + mapStripCurrentLine().
 *
 * @returns Pointer to the first non-space character in the new line. NULL on
 *          read error (bitched already) or end of file.
 * @param   pMap                The map file handle.
 * @param   pcch                Where to return the length of the unstripped
 *                              part.  Optional.
 */
static char *mapReadLineStrip(PBIOSMAP pMap, size_t *pcch)
{
    if (!mapReadLine(pMap))
        return NULL;
    return mapStripCurrentLine(pMap, pcch);
}


/**
 * Parses a word, copying it into the supplied buffer, and skipping any spaces
 * following it.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pszBuf              The output buffer.
 * @param   cbBuf               The size of the output buffer.
 */
static bool mapParseWord(char **ppszCursor, char *pszBuf, size_t cbBuf)
{
    /* Check that we start on a non-blank. */
    char *pszStart  = *ppszCursor;
    if (!*pszStart || RT_C_IS_SPACE(*pszStart))
        return false;

    /* Find the end of the word. */
    char *psz = pszStart + 1;
    while (*psz && !RT_C_IS_SPACE(*psz))
        psz++;

    /* Copy it. */
    size_t cchWord = (uintptr_t)psz - (uintptr_t)pszStart;
    if (cchWord >= cbBuf)
        return false;
    memcpy(pszBuf, pszStart, cchWord);
    pszBuf[cchWord] = '\0';

    /* Skip blanks following it. */
    while (RT_C_IS_SPACE(*psz))
        psz++;
    *ppszCursor = psz;
    return true;
}


/**
 * Parses an 16:16 address.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pAddr               Where to return the address.
 */
static bool mapParseAddress(char **ppszCursor, PRTFAR16 pAddr)
{
    char szWord[32];
    if (!mapParseWord(ppszCursor, szWord, sizeof(szWord)))
        return false;
    size_t cchWord = strlen(szWord);

    /* An address is at least 16:16 format. It may be 16:32. It may also be flagged. */
    size_t cchAddr = 4 + 1 + 4;
    if (cchWord < cchAddr)
        return false;
    if (   !RT_C_IS_XDIGIT(szWord[0])
        || !RT_C_IS_XDIGIT(szWord[1])
        || !RT_C_IS_XDIGIT(szWord[2])
        || !RT_C_IS_XDIGIT(szWord[3])
        || szWord[4] != ':'
        || !RT_C_IS_XDIGIT(szWord[5])
        || !RT_C_IS_XDIGIT(szWord[6])
        || !RT_C_IS_XDIGIT(szWord[7])
        || !RT_C_IS_XDIGIT(szWord[8])
       )
        return false;
    if (   cchWord > cchAddr
        && RT_C_IS_XDIGIT(szWord[9])
        && RT_C_IS_XDIGIT(szWord[10])
        && RT_C_IS_XDIGIT(szWord[11])
        && RT_C_IS_XDIGIT(szWord[12]))
        cchAddr += 4;

    /* Drop flag if present. */
    if (cchWord > cchAddr)
    {
        if (RT_C_IS_XDIGIT(szWord[4+1+4]))
            return false;
        szWord[cchAddr] = '\0';
        cchWord = cchAddr;
    }

    /* Convert it. */
    szWord[4] = '\0';
    int rc1 = RTStrToUInt16Full(szWord,     16, &pAddr->sel);
    if (rc1 != VINF_SUCCESS)
        return false;

    int rc2 = RTStrToUInt16Full(szWord + 5, 16, &pAddr->off);
    if (rc2 != VINF_SUCCESS)
        return false;
    return true;
}


/**
 * Parses a size.
 *
 * @returns @c true on success, @c false on failure.
 * @param   ppszCursor          Pointer to the cursor variable.
 * @param   pcb                 Where to return the size.
 */
static bool mapParseSize(char **ppszCursor, uint32_t *pcb)
{
    char szWord[32];
    if (!mapParseWord(ppszCursor, szWord, sizeof(szWord)))
        return false;
    size_t cchWord = strlen(szWord);
    if (cchWord != 8)
        return false;

    int rc = RTStrToUInt32Full(szWord, 16, pcb);
    if (rc != VINF_SUCCESS)
        return false;
    return true;
}


/**
 * Parses a section box and the following column header.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap            Map file handle.
 * @param   pszSectionNm    The expected section name.
 * @param   cColumns        The number of columns.
 * @param   ...             The column names.
 */
static bool mapSkipThruColumnHeadings(PBIOSMAP pMap, const char *pszSectionNm, uint32_t cColumns, ...)
{
    if (   mapIsEmptyLine(pMap)
        && !mapSkipEmptyLines(pMap))
        return false;

    /* +------------+ */
    size_t cch;
    char  *psz = mapStripCurrentLine(pMap, &cch);
    if (!psz)
        return false;

    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
    {
        RTMsgError("%s:%d: Expected section box: +-----...", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    /* |   pszSectionNm   | */
    psz = mapReadLineStrip(pMap, &cch);
    if (!psz)
        return false;

    size_t cchSectionNm = strlen(pszSectionNm);
    if (   psz[0] != '|'
        || psz[1] != ' '
        || psz[2] != ' '
        || psz[3] != ' '
        || psz[cch - 4] != ' '
        || psz[cch - 3] != ' '
        || psz[cch - 2] != ' '
        || psz[cch - 1] != '|'
        || cch != 1 + 3 + cchSectionNm + 3 + 1
        || strncmp(&psz[4], pszSectionNm, cchSectionNm)
        )
    {
        RTMsgError("%s:%d: Expected section box: |   %s   |", pMap->pszMapFile, pMap->iLine, pszSectionNm);
        return false;
    }

    /* +------------+ */
    psz = mapReadLineStrip(pMap, &cch);
    if (!psz)
        return false;
    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected section box: +-----...", pMap->pszMapFile, pMap->iLine);

    /* There may be a few lines describing the table notation now, surrounded by blank lines. */
    do
    {
        psz = mapReadLineStripRight(pMap, &cch);
        if (!psz)
            return false;
    } while (   *psz == '\0'
             || (   !RT_C_IS_SPACE(psz[0])
                 && RT_C_IS_SPACE(psz[1])
                 && psz[2] == '='
                 && RT_C_IS_SPACE(psz[3]))
            );

    /* Should have the column heading now. */
    va_list va;
    va_start(va, cColumns);
    for (uint32_t i = 0; i < cColumns; i++)
    {
        const char *pszColumn = va_arg(va, const char *);
        size_t      cchColumn = strlen(pszColumn);
        if (   strncmp(psz, pszColumn, cchColumn)
            || (    psz[cchColumn] != '\0'
                && !RT_C_IS_SPACE(psz[cchColumn])))
        {
            va_end(va);
            RTMsgError("%s:%d: Expected column '%s' found '%s'", pMap->pszMapFile, pMap->iLine, pszColumn, psz);
            return false;
        }
        psz += cchColumn;
        while (RT_C_IS_SPACE(*psz))
            psz++;
    }
    va_end(va);

    /* The next line is the underlining. */
    psz = mapReadLineStripRight(pMap, &cch);
    if (!psz)
        return false;
    if (*psz != '=' || psz[cch - 1] != '=')
    {
        RTMsgError("%s:%d: Expected column header underlining", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    /* Skip one blank line. */
    psz = mapReadLineStripRight(pMap, &cch);
    if (!psz)
        return false;
    if (*psz)
    {
        RTMsgError("%s:%d: Expected blank line beneath the column headers", pMap->pszMapFile, pMap->iLine);
        return false;
    }

    return true;
}


/**
 * Parses a segment list.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapParseSegments(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLineStripRight(pMap, NULL))
            return false;

        /* The end? The line should be empty. Expectes segment name to not
           start with a space. */
        if (!pMap->szLine[0] || RT_C_IS_SPACE(pMap->szLine[0]))
        {
            if (!pMap->szLine[0])
                return true;
            RTMsgError("%s:%u: Malformed segment line", pMap->pszMapFile, pMap->iLine);
            return false;
        }

        /* Parse the segment line. */
        uint32_t iSeg = g_cSegs;
        if (iSeg >= RT_ELEMENTS(g_aSegs))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Too many segments", pMap->pszMapFile, pMap->iLine);

        char *psz = pMap->szLine;
        if (!mapParseWord(&psz, g_aSegs[iSeg].szName, sizeof(g_aSegs[iSeg].szName)))
            RTMsgError("%s:%u: Segment name parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, g_aSegs[iSeg].szClass, sizeof(g_aSegs[iSeg].szClass)))
            RTMsgError("%s:%u: Segment class parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, g_aSegs[iSeg].szGroup, sizeof(g_aSegs[iSeg].szGroup)))
            RTMsgError("%s:%u: Segment group parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseAddress(&psz, &g_aSegs[iSeg].Address))
            RTMsgError("%s:%u: Segment address parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseSize(&psz, &g_aSegs[iSeg].cb))
            RTMsgError("%s:%u: Segment size parser error", pMap->pszMapFile, pMap->iLine);
        else
        {
            g_aSegs[iSeg].uFlatAddr = ((uint32_t)g_aSegs[iSeg].Address.sel << 4) + g_aSegs[iSeg].Address.off;
            g_cSegs++;
            if (g_cVerbose > 2)
                RTStrmPrintf(g_pStdErr, "read segment at %08x / %04x:%04x LB %04x %s / %s / %s\n",
                             g_aSegs[iSeg].uFlatAddr,
                             g_aSegs[iSeg].Address.sel,
                             g_aSegs[iSeg].Address.off,
                             g_aSegs[iSeg].cb,
                             g_aSegs[iSeg].szName,
                             g_aSegs[iSeg].szClass,
                             g_aSegs[iSeg].szGroup);

            while (RT_C_IS_SPACE(*psz))
                psz++;
            if (!*psz)
                continue;
            RTMsgError("%s:%u: Junk at end of line", pMap->pszMapFile, pMap->iLine);
        }
        return false;
    }
}


/**
 * Sorts the segment array by flat address and adds them to the debug module.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 */
static bool mapSortAndAddSegments(void)
{
    for (uint32_t i = 0; i < g_cSegs; i++)
    {
        for (uint32_t j = i + 1; j < g_cSegs; j++)
            if (g_aSegs[j].uFlatAddr < g_aSegs[i].uFlatAddr)
            {
                BIOSSEG Tmp = g_aSegs[i];
                g_aSegs[i] = g_aSegs[j];
                g_aSegs[j] = Tmp;
            }
        if (g_cVerbose > 0)
            RTStrmPrintf(g_pStdErr, "segment at %08x / %04x:%04x LB %04x %s / %s / %s\n",
                         g_aSegs[i].uFlatAddr,
                         g_aSegs[i].Address.sel,
                         g_aSegs[i].Address.off,
                         g_aSegs[i].cb,
                         g_aSegs[i].szName,
                         g_aSegs[i].szClass,
                         g_aSegs[i].szGroup);

        RTDBGSEGIDX idx = i;
        int rc = RTDbgModSegmentAdd(g_hMapMod, g_aSegs[i].uFlatAddr, g_aSegs[i].cb, g_aSegs[i].szName, 0 /*fFlags*/, &idx);
        if (RT_FAILURE(rc))
        {
            RTMsgError("RTDbgModSegmentAdd failed on %s: %Rrc", g_aSegs[i].szName);
            return false;
        }
    }
    return true;
}


/**
 * Parses a segment list.
 *
 * @returns @c true on success, @c false + msg on failure, @c false on eof.
 * @param   pMap                The map file handle.
 */
static bool mapParseSymbols(PBIOSMAP pMap)
{
    for (;;)
    {
        if (!mapReadLineStripRight(pMap, NULL))
            return false;

        /* The end? The line should be empty. Expectes segment name to not
           start with a space. */
        if (!pMap->szLine[0] || RT_C_IS_SPACE(pMap->szLine[0]))
        {
            if (!pMap->szLine[0])
                return true;
            RTMsgError("%s:%u: Malformed symbol line", pMap->pszMapFile, pMap->iLine);
            return false;
        }

        /* Skip the module name lines for now. */
        if (!strncmp(pMap->szLine, RT_STR_TUPLE("Module: ")))
            continue;

        /* Parse the segment line. */
        char    szName[4096];
        RTFAR16 Addr;
        char   *psz = pMap->szLine;
        if (!mapParseAddress(&psz, &Addr))
            RTMsgError("%s:%u: Symbol address parser error", pMap->pszMapFile, pMap->iLine);
        else if (!mapParseWord(&psz, szName, sizeof(szName)))
            RTMsgError("%s:%u: Symbol name parser error", pMap->pszMapFile, pMap->iLine);
        else
        {
            uint32_t uFlatAddr = ((uint32_t)Addr.sel << 4) + Addr.off;
            if (uFlatAddr == 0)
                continue;

            int rc = RTDbgModSymbolAdd(g_hMapMod, szName, RTDBGSEGIDX_RVA, uFlatAddr, 0 /*cb*/,  0 /*fFlags*/, NULL);
            if (RT_SUCCESS(rc) || rc == VERR_DBG_ADDRESS_CONFLICT)
            {

                if (g_cVerbose > 2)
                    RTStrmPrintf(g_pStdErr, "read symbol - %08x %s\n", uFlatAddr, szName);
                while (RT_C_IS_SPACE(*psz))
                    psz++;
                if (!*psz)
                    continue;
                RTMsgError("%s:%u: Junk at end of line", pMap->pszMapFile, pMap->iLine);
            }
            else
                RTMsgError("%s:%u: RTDbgModSymbolAdd failed: %Rrc", pMap->pszMapFile, pMap->iLine, rc);
        }
        return false;
    }
}


/**
 * Parses the given map file.
 *
 * @returns RTEXITCODE_SUCCESS and lots of globals, or RTEXITCODE_FAILURE and a
 *          error message.
 * @param   pMap                The map file handle.
 */
static RTEXITCODE mapParseFile(PBIOSMAP pMap)
{
    int rc = RTDbgModCreate(&g_hMapMod, "VBoxBios", 0 /*cbSeg*/, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDbgModCreate failed: %Rrc", rc);

    /*
     * Read the header.
     */
    if (!mapReadLine(pMap))
        return RTEXITCODE_FAILURE;
    if (strncmp(pMap->szLine, RT_STR_TUPLE("Open Watcom Linker Version")))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unexpected map-file header: '%s'", pMap->szLine);
    if (   !mapSkipNonEmptyLines(pMap)
        || !mapSkipEmptyLines(pMap))
        return RTEXITCODE_FAILURE;

    /*
     * Skip groups.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Groups", 3, "Group", "Address", "Size", NULL))
        return RTEXITCODE_FAILURE;
    if (!mapSkipNonEmptyLines(pMap))
        return RTEXITCODE_FAILURE;

    /*
     * Parse segments.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Segments", 5, "Segment", "Class", "Group", "Address", "Size"))
        return RTEXITCODE_FAILURE;
    if (!mapParseSegments(pMap))
        return RTEXITCODE_FAILURE;
    if (!mapSortAndAddSegments())
        return RTEXITCODE_FAILURE;

    /*
     * Parse symbols.
     */
    if (!mapSkipThruColumnHeadings(pMap, "Memory Map", 2, "Address", "Symbol"))
        return RTEXITCODE_FAILURE;
    if (!mapParseSymbols(pMap))
        return RTEXITCODE_FAILURE;

    /* Ignore the rest of the file. */
    return RTEXITCODE_SUCCESS;
}


/**
 * Parses the linker map file for the BIOS.
 *
 * This is generated by the Watcom linker.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosMap          Path to the map file.
 */
static RTEXITCODE ParseMapFile(const char *pszBiosMap)
{
    BIOSMAP Map;
    Map.pszMapFile = pszBiosMap;
    Map.hStrm      = NULL;
    Map.iLine      = 0;
    Map.fEof       = false;
    Map.cch        = 0;
    Map.offNW      = 0;
    int rc = RTStrmOpen(pszBiosMap, "r", &Map.hStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening '%s': %Rrc", pszBiosMap, rc);
    RTEXITCODE rcExit = mapParseFile(&Map);
    RTStrmClose(Map.hStrm);
    return rcExit;
}


/**
 * Reads the BIOS image into memory (g_pbImg and g_cbImg).
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosImg          Path to the image file.
 */
static RTEXITCODE ReadBiosImage(const char *pszBiosImg)
{
    void  *pvImg;
    size_t cbImg;
    int rc = RTFileReadAll(pszBiosImg, &pvImg, &cbImg);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading '%s': %Rrc", pszBiosImg, rc);
    if (cbImg != _64K)
    {
        RTFileReadAllFree(pvImg, cbImg);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The BIOS image %u bytes intead of 64KB", cbImg);
    }

    g_pbImg = (uint8_t *)pvImg;
    g_cbImg = cbImg;
    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--bios-image",               'i',                    RTGETOPT_REQ_STRING },
        { "--bios-map",                 'm',                    RTGETOPT_REQ_STRING },
        { "--bios-sym",                 's',                    RTGETOPT_REQ_STRING },
        { "--output",                   'o',                    RTGETOPT_REQ_STRING },
        { "--verbose",                  'v',                    RTGETOPT_REQ_NOTHING },
        { "--quiet",                    'q',                    RTGETOPT_REQ_NOTHING },
    };

    const char *pszBiosMap = NULL;
    const char *pszBiosSym = NULL;
    const char *pszBiosImg = NULL;
    const char *pszOutput  = NULL;

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'i':
                if (pszBiosImg)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-image is given more than once");
                pszBiosImg = ValueUnion.psz;
                break;

            case 'm':
                if (pszBiosMap)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-map is given more than once");
                pszBiosMap = ValueUnion.psz;
                break;

            case 's':
                if (pszBiosSym)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-sym is given more than once");
                pszBiosSym = ValueUnion.psz;
                break;

            case 'o':
                if (pszOutput)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--output is given more than once");
                pszOutput = ValueUnion.psz;
                break;

            case 'v':
                g_cVerbose++;
                break;

            case 'q':
                g_cVerbose = 0;
                break;

            case 'H':
                RTPrintf("usage: %Rbn --bios-image <file.img> --bios-map <file.map> [--output <file.asm>]\n",
                         argv[0]);
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                RTPrintf("r%u\n", RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Got it all?
     */
    if (!pszBiosImg)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-image is required");
    if (!pszBiosMap)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-map is required");
    if (!pszBiosSym)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-sym is required");

    /*
     * Do the job.
     */
    RTEXITCODE rcExit;
    rcExit = ReadBiosImage(pszBiosImg);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = ParseMapFile(pszBiosMap);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = ParseSymFile(pszBiosSym);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = OpenOutputFile(pszOutput);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = DisassembleBiosImage();

    return rcExit;
}

