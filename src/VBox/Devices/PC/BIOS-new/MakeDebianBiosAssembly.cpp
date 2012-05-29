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

/** The number of BIOS segments found in the map file. */
static uint32_t         g_cSegs = 0;
/** Array of BIOS segments from the map file. */
static BIOSSEG          g_aSegs[32];





static RTEXITCODE DisassembleBiosImage(void)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "DisassembleBiosImage is not implemented");
}

static RTEXITCODE OpenOutputFile(const char *pszOutput)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "OpenOutputFile is not implemented");
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




static bool SkipEmptyLines(PRTSTREAM hStrm, uint32_t *piLine, char *pszLine, size_t cbLine)
{
    for (;;)
    {
        int rc = RTStrmGetLine(hStrm, pszLine, cbLine);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Error reading map-file: %Rrc", rc);
            return false;
        }

        *piLine += 1;
        const char *psz = RTStrStripL(pszLine);
        if (*psz)
            return true;
    }
}


static bool SkipNonEmptyLines(PRTSTREAM hStrm, uint32_t *piLine, char *pszLine, size_t cbLine)
{
    for (;;)
    {
        int rc = RTStrmGetLine(hStrm, pszLine, cbLine);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Error reading map-file: %Rrc", rc);
            return false;
        }

        *piLine += 1;
        const char *psz = RTStrStripL(pszLine);
        if (!*psz)
            return true;
    }
}


static char *ReadMapLineR(const char *pszBiosMap, PRTSTREAM hStrm, uint32_t *piLine,
                          char *pszLine, size_t cbLine, size_t *pcchLine)
{
    int rc = RTStrmGetLine(hStrm, pszLine, cbLine);
    if (RT_FAILURE(rc))
    {
        RTMsgError("%s:%d: Read error: %Rrc", pszBiosMap, *piLine, rc);
        return NULL;
    }
    *piLine += 1;

    char  *psz = RTStrStripR(pszLine);
    *pcchLine = strlen(psz);
    return psz;
}


static char *ReadMapLineLR(const char *pszBiosMap, PRTSTREAM hStrm, uint32_t *piLine,
                          char *pszLine, size_t cbLine, size_t *pcchLine)
{
    int rc = RTStrmGetLine(hStrm, pszLine, cbLine);
    if (RT_FAILURE(rc))
    {
        RTMsgError("%s:%d: Read error: %Rrc", pszBiosMap, *piLine, rc);
        return NULL;
    }
    *piLine += 1;

    char  *psz = RTStrStrip(pszLine);
    *pcchLine = strlen(psz);
    return psz;
}


static char *ReadMapLine(const char *pszBiosMap, PRTSTREAM hStrm, uint32_t *piLine,
                         char *pszLine, size_t cbLine, size_t *pcchLine)
{
    int rc = RTStrmGetLine(hStrm, pszLine, cbLine);
    if (RT_FAILURE(rc))
    {
        RTMsgError("%s:%d: Read error: %Rrc", pszBiosMap, *piLine, rc);
        return NULL;
    }
    *piLine += 1;

    *pcchLine = strlen(pszLine);
    return pszLine;
}


static bool ParseWord(char **ppszCursor, char *pszBuf, size_t cbBuf)
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


static bool ParseAddress(char **ppszCursor, PRTFAR16 pAddr)
{
    char szWord[32];
    if (!ParseWord(ppszCursor, szWord, sizeof(szWord)))
        return false;
    size_t cchWord = strlen(szWord);

    /* Check the first 4+1+4 chars. */
    if (cchWord < 4 + 1 + 4)
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

    /* Drop annotation. */
    if (cchWord > 4+1+4)
    {
        if (RT_C_IS_XDIGIT(szWord[4+1+4]))
            return false;
        szWord[4+1+4] = '\0';
        cchWord = 4 + 1 + 4;
    }

    /* Convert it. */
    szWord[4] = '\0';
    int rc1 = RTStrToUInt16Ex(szWord,     NULL, 16, &pAddr->sel);  AssertRC(rc1);
    int rc2 = RTStrToUInt16Ex(szWord + 5, NULL, 16, &pAddr->off);  AssertRC(rc2);
    return true;
}


static bool ParseSize(char **ppszCursor, uint32_t *pcb)
{
    char szWord[32];
    if (!ParseWord(ppszCursor, szWord, sizeof(szWord)))
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
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pszBiosMap      The input file name.
 * @param   hStrm           The input stream.
 * @param   piLine          Pointer to the current line number variable.
 * @param   pszSectionNm    The expected section name.
 * @param   cColumns        The number of columns.
 * @param   ...             The column names.
 */
static RTEXITCODE SkipThruColumnHeadings(const char *pszBiosMap, PRTSTREAM hStrm, uint32_t *piLine,
                                         const char *pszSectionNm, uint32_t cColumns, ...)
{
    char szLine[16384];
    if (!SkipEmptyLines(hStrm, piLine, szLine, sizeof(szLine)))
        return RTEXITCODE_FAILURE;

    /* +------------+ */
    char  *psz = RTStrStrip(szLine);
    size_t cch = strlen(psz);
    if (!psz)
        return RTEXITCODE_FAILURE;

    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected section box: +-----...", pszBiosMap, *piLine);

    /* |   pszSectionNm   | */
    psz = ReadMapLineLR(pszBiosMap, hStrm, piLine, szLine, sizeof(szLine), &cch);
    if (!psz)
        return RTEXITCODE_FAILURE;

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
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected section box: |   %s   |", pszBiosMap, *piLine, pszSectionNm);

    /* +------------+ */
    psz = ReadMapLineLR(pszBiosMap, hStrm, piLine, szLine, sizeof(szLine), &cch);
    if (!psz)
        return RTEXITCODE_FAILURE;
    if (   psz[0] != '+'
        || psz[1] != '-'
        || psz[2] != '-'
        || psz[3] != '-'
        || psz[cch - 4] != '-'
        || psz[cch - 3] != '-'
        || psz[cch - 2] != '-'
        || psz[cch - 1] != '+'
       )
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected section box: +-----...", pszBiosMap, *piLine);

    /* There may be a few lines describing the table notation now, surrounded by blank lines. */
    do
    {
        psz = ReadMapLineR(pszBiosMap, hStrm, piLine, szLine, sizeof(szLine), &cch);
        if (!psz)
            return RTEXITCODE_FAILURE;
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
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected column '%s' found '%s'",
                                  pszBiosMap, *piLine, pszColumn, psz);
        }
        psz += cchColumn;
        while (RT_C_IS_SPACE(*psz))
            psz++;
    }
    va_end(va);

    /* The next line is the underlining. */
    psz = ReadMapLineR(pszBiosMap, hStrm, piLine, szLine, sizeof(szLine), &cch);
    if (!psz)
        return RTEXITCODE_FAILURE;
    if (*psz != '=' || psz[cch - 1] != '=')
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected column header underlining", pszBiosMap, *piLine);

    /* Skip one blank line. */
    psz = ReadMapLineR(pszBiosMap, hStrm, piLine, szLine, sizeof(szLine), &cch);
    if (!psz)
        return RTEXITCODE_FAILURE;
    if (*psz)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%d: Expected blank line beneath the column headers", pszBiosMap, *piLine);

    return RTEXITCODE_SUCCESS;
}



static RTEXITCODE ParseMapFileSegments(const char *pszBiosMap, PRTSTREAM hStrm, uint32_t *piLine)
{
    for (;;)
    {
        /* Read the next line and right strip it. */
        char szLine[16384];
        int rc = RTStrmGetLine(hStrm, szLine, sizeof(szLine));
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Read error: %Rrc", pszBiosMap, *piLine, rc);

        *piLine += 1;
        RTStrStripR(szLine);

        /* The end? The line should be empty. Expectes segment name to not
           start with a space. */
        if (!szLine[0] || RT_C_IS_SPACE(szLine[0]))
        {
            if (szLine[0])
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Malformed segment line", pszBiosMap, *piLine);
            return RTEXITCODE_SUCCESS;
        }

        /* Parse the segment line. */
        uint32_t iSeg = g_cSegs;
        if (iSeg >= RT_ELEMENTS(g_aSegs))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Too many segments", pszBiosMap, *piLine);

        char *psz = szLine;
        if (ParseWord(&psz, g_aSegs[iSeg].szName, sizeof(g_aSegs[iSeg].szName)))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Segment name parser error", pszBiosMap, *piLine);
        if (ParseWord(&psz, g_aSegs[iSeg].szClass, sizeof(g_aSegs[iSeg].szClass)))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Segment class parser error", pszBiosMap, *piLine);
        if (ParseWord(&psz, g_aSegs[iSeg].szGroup, sizeof(g_aSegs[iSeg].szGroup)))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Segment group parser error", pszBiosMap, *piLine);
        if (ParseAddress(&psz, &g_aSegs[iSeg].Address))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Segment address parser error", pszBiosMap, *piLine);
        if (ParseSize(&psz, &g_aSegs[iSeg].cb))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Segment size parser error", pszBiosMap, *piLine);

        g_cSegs++;

        while (RT_C_IS_SPACE(*psz))
            psz++;
        if (*psz)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s:%u: Junk at end of line", pszBiosMap, *piLine);

    }
}


static RTEXITCODE ParseMapFileInner(PBIOSMAP pMap)
{
    uint32_t    iLine = 1;
    char        szLine[16384];
    const char *psz;
    PRTSTREAM   hStrm = pMap->hStrm; /** @todo rewrite the rest. */
    const char *pszBiosMap = pMap->pszMapFile; /** @todo rewrite the rest. */

    /*
     * Read the header.
     */
    int rc = RTStrmGetLine(hStrm, szLine, sizeof(szLine));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading map-file header: %Rrc", rc);
    if (strncmp(szLine, RT_STR_TUPLE("Open Watcom Linker Version")))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unexpected map-file header: '%s'", szLine);
    if (   !SkipNonEmptyLines(hStrm, &iLine, szLine, sizeof(szLine))
        || !SkipEmptyLines(hStrm, &iLine, szLine, sizeof(szLine)) )
        return RTEXITCODE_FAILURE;

    /*
     * Skip groups.
     */
    if (SkipThruColumnHeadings(pszBiosMap, hStrm, &iLine, "Groups", 3, "Group", "Address", "Size", NULL) != RTEXITCODE_SUCCESS)
        return RTEXITCODE_FAILURE;
    if (!SkipNonEmptyLines(hStrm, &iLine, szLine, sizeof(szLine)))
        return RTEXITCODE_FAILURE;

    /*
     * Parse segments.
     */
    if (   SkipThruColumnHeadings(pszBiosMap, hStrm, &iLine, "Segments", 5, "Segment", "Class", "Group", "Address", "Size")
        != RTEXITCODE_SUCCESS)
        return RTEXITCODE_FAILURE;

    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading map-file group: %Rrc", rc);





    return RTMsgErrorExit(RTEXITCODE_FAILURE, "ParseMapFileInner is not fully implemented");
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
    Map.cch        = 0;
    Map.offNW      = 0;
    int rc = RTStrmOpen(pszBiosMap, "rt", &Map.hStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening '%s': %Rrc", pszBiosMap, rc);
    RTEXITCODE rcExit = ParseMapFileInner(&Map);
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
                if (pszBiosMap)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--bios-sym is given more than once");
                pszBiosMap = ValueUnion.psz;
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

