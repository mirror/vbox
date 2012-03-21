/* $Id$ */
/** @file
 * IPRT Testcase / Tool - VBox Tracepoint Compiler.
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
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scmstream.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Code/data stability.
 */
typedef enum kVTGStability
{
    kVTGStability_Invalid = 0,
    kVTGStability_Internal,
    kVTGStability_Private,
    kVTGStability_Obsolete,
    kVTGStability_External,
    kVTGStability_Unstable,
    kVTGStability_Evolving,
    kVTGStability_Stable,
    kVTGStability_Standard
} kVTGStability;

/**
 * Data dependency.
 */
typedef enum kVTGClass
{
    kVTGClass_Invalid = 0,
    kVTGClass_Unknown,
    kVTGClass_Cpu,
    kVTGClass_Platform,
    kVTGClass_Group,
    kVTGClass_Isa,
    kVTGClass_Common
} kVTGClass;

typedef struct VTGATTRS
{
    kVTGStability   enmCode;
    kVTGStability   enmData;
    kVTGClass       enmDataDep;
} VTGATTRS;
typedef VTGATTRS *PVTGATTRS;


typedef struct VTGARG
{
    RTLISTNODE      ListEntry;
    const char     *pszName;
    const char     *pszType;
} VTGARG;
typedef VTGARG *PVTGARG;

typedef struct VTGPROBE
{
    RTLISTNODE      ListEntry;
    const char     *pszName;
    RTLISTANCHOR    ArgHead;
    uint32_t        cArgs;
} VTGPROBE;
typedef VTGPROBE *PVTGPROBE;

typedef struct VTGPROVIDER
{
    RTLISTNODE      ListEntry;
    const char     *pszName;

    VTGATTRS        AttrSelf;
    VTGATTRS        AttrModules;
    VTGATTRS        AttrFunctions;
    VTGATTRS        AttrName;
    VTGATTRS        AttrArguments;

    RTLISTANCHOR    ProbeHead;
} VTGPROVIDER;
typedef VTGPROVIDER *PVTGPROVIDER;

/**
 * A string table string.
 */
typedef struct VTGSTRING
{
    /** The string space core. */
    RTSTRSPACECORE  Core;
    /** The string table offset. */
    uint32_t        offStrTab;
    /** The actual string. */
    char            szString[1];
} VTGSTRING;
typedef VTGSTRING *PVTGSTRING;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The string space organizing the string table strings. Each node is a VTGSTRING. */
static RTSTRSPACE       g_StrSpace = NULL;
/** Used by the string table enumerator to set VTGSTRING::offStrTab. */
static uint32_t         g_offStrTab;
/** List of providers created by the parser. */
static RTLISTANCHOR     g_ProviderHead;

/** @name Options
 * @{ */
static enum
{
    kVBoxTpGAction_Nothing,
    kVBoxTpGAction_GenerateHeader,
    kVBoxTpGAction_GenerateObject
}                           g_enmAction                 = kVBoxTpGAction_Nothing;
static uint32_t             g_cBits                     = ARCH_BITS;
static bool                 g_fApplyCpp                 = false;
static uint32_t             g_cVerbosity                = 0;
static const char          *g_pszOutput                 = NULL;
static const char          *g_pszScript                 = NULL;
static const char          *g_pszTempAsm                = NULL;
#ifdef RT_OS_DARWIN
static const char          *g_pszAssembler              = "yasm";
static const char          *g_pszAssemblerFmtOpt        = "--oformat";
static const char           g_szAssemblerFmtVal32[]     = "macho32";
static const char           g_szAssemblerFmtVal64[]     = "macho64";
#elif defined(RT_OS_OS2)
static const char          *pszAssembler                = "nasm.exe";
static const char          *pszAssemblerFmtOpt          = "-f";
static const char           g_szAssemblerFmtVal32[]     = "obj";
static const char           g_szAssemblerFmtVal64[]     = "elf64";
#elif defined(RT_OS_WINDOWS)
static const char          *g_pszAssembler              = "yasm.exe";
static const char          *g_pszAssemblerFmtOpt        = "--oformat";
static const char           g_szAssemblerFmtVal32[]     = "win32";
static const char           g_szAssemblerFmtVal64[]     = "win64";
#else
static const char          *g_pszAssembler              = "yasm";
static const char          *g_pszAssemblerFmtOpt        = "--oformat";
static const char           g_szAssemblerFmtVal32[]     = "elf32";
static const char           g_szAssemblerFmtVal64[]     = "elf64";
#endif
static const char          *g_pszAssemblerFmtVal        = RT_CONCAT(g_szAssemblerFmtVal, ARCH_BITS);
static const char          *g_pszAssemblerOutputOpt     = "-o";
static unsigned             g_cAssemblerOptions         = 0;
static const char          *g_apszAssemblerOptions[32];
/** @} */


/**
 * Inserts a string into the string table, reusing any matching existing string
 * if possible.
 *
 * @returns Read only string.
 * @param   pch                 The string to insert (need not be terminated).
 * @param   cch                 The length of the string.
 */
static const char *strtabInsertN(const char *pch, size_t cch)
{
    PVTGSTRING pStr = (PVTGSTRING)RTStrSpaceGetN(&g_StrSpace, pch, cch);
    if (pStr)
        return pStr->szString;

    /*
     * Create a new entry.
     */
    pStr = (PVTGSTRING)RTMemAlloc(RT_OFFSETOF(VTGSTRING, szString[cch + 1]));
    if (!pStr)
        return NULL;

    pStr->Core.pszString = pStr->szString;
    memcpy(pStr->szString, pch, cch);
    pStr->szString[cch]  = '\0';
    pStr->offStrTab      = UINT32_MAX;

    bool fRc = RTStrSpaceInsert(&g_StrSpace, &pStr->Core);
    Assert(fRc); NOREF(fRc);
    return pStr->szString;
}


static RTEXITCODE generateInvokeAssembler(const char *pszOutput, const char *pszTempAsm)
{
    RTPrintf("Todo invoke the assembler\n");
    return RTEXITCODE_SKIPPED;
}


static RTEXITCODE generateFile(const char *pszOutput, const char *pszWhat,
                               RTEXITCODE (*pfnGenerator)(PSCMSTREAM))
{
    SCMSTREAM Strm;
    int rc = ScmStreamInitForWriting(&Strm, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ScmStreamInitForWriting returned %Rrc when generating the %s file",
                              rc, pszWhat);

    RTEXITCODE rcExit = pfnGenerator(&Strm);
    if (RT_FAILURE(ScmStreamGetStatus(&Strm)))
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Stream error %Rrc generating the %s file",
                                ScmStreamGetStatus(&Strm), pszWhat);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        rc = ScmStreamWriteToFile(&Strm, "%s", pszOutput);
        if (RT_FAILURE(rc))
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "ScmStreamWriteToFile returned %Rrc when writing '%s' (%s)",
                                    rc, pszOutput, pszWhat);
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            if (g_cVerbosity > 0)
                RTMsgInfo("Successfully generated '%s'.", pszOutput);
            if (g_cVerbosity > 1)
            {
                RTMsgInfo("================ %s - start ================", pszWhat);
                ScmStreamRewindForReading(&Strm);
                const char *pszLine;
                size_t      cchLine;
                SCMEOL      enmEol;
                while ((pszLine = ScmStreamGetLine(&Strm, &cchLine, &enmEol)) != NULL)
                    RTPrintf("%.*s\n", cchLine, pszLine);
                RTMsgInfo("================ %s - end   ================", pszWhat);
            }
        }
    }
    ScmStreamDelete(&Strm);
    return rcExit;
}

static ssize_t ScmStreamPrintfV(PSCMSTREAM pStream, const char *pszFormat, va_list va)
{
    char   *psz;
    ssize_t cch = RTStrAPrintfV(&psz, pszFormat, va);
    if (cch)
    {
        int rc = ScmStreamWrite(pStream, psz, cch);
        RTStrFree(psz);
        if (RT_FAILURE(rc))
            cch = rc;
    }
    return cch;
}

static ssize_t ScmStreamPrintf(PSCMSTREAM pStream, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    ssize_t cch = ScmStreamPrintfV(pStream, pszFormat, va);
    va_end(va);
    return cch;
}


/**
 * @callback_method_impl{FNRTSTRSPACECALLBACK, Writes the string table strings.}
 */
static DECLCALLBACK(int) generateAssemblyStrTabCallback(PRTSTRSPACECORE pStr, void *pvUser)
{
    PVTGSTRING pVtgStr = (PVTGSTRING)pStr;
    PSCMSTREAM pStrm   = (PSCMSTREAM)pvUser;

    pVtgStr->offStrTab = g_offStrTab;
    g_offStrTab += pVtgStr->Core.cchString + 1;

    ScmStreamPrintf(pStrm,
                    "    db '%s', 0 ; off=%u len=%zu\n",
                    pVtgStr->szString, pVtgStr->offStrTab, pVtgStr->Core.cchString);
    return VINF_SUCCESS;
}


static RTEXITCODE generateAssembly(PSCMSTREAM pStrm)
{
    if (g_cVerbosity > 0)
        RTMsgInfo("Generating assembly code...");

    /*
     * Write the file header.
     */
    ScmStreamPrintf(pStrm,
                    "; $Id$ \n"
                    ";; @file\n"
                    "; Automatically generated from %s. Do NOT edit!\n"
                    ";\n"
                    "\n"
                    "%include \"iprt/asmdefs.h\"\n"
                    "\n"
                    "%ifdef ASM_FORMAT_OMF\n"
                    " segment VTGObject public CLASS=VTGObject align=16 use32"
                    ,
                    g_pszScript);

    /*
     * Dump the string table to set the offsets before we use them anywhere.
     */
    ScmStreamPrintf(pStrm,
                    ";\n"
                    "; The string table.\n"
                    ";\n"
                    "BEGINDATA\n"
                    "GLOBALNAME g_achVTGStringTable\n");
    g_offStrTab = 0;
    RTStrSpaceEnumerate(&g_StrSpace, generateAssemblyStrTabCallback, pStrm);
    ScmStreamPrintf(pStrm,
                    "GLOBALNAME g_achVTGStringTable_End\n");

    /*
     * Declare the probe enable flags.
     */
    ScmStreamPrintf(pStrm,
                    ";\n"
                    "; Probe enabled flags.  Since these will be accessed all the time\n"
                    "; they are placed together to get some more cache and TLB hits when\n"
                    "; the probes are disabled."
                    ";\n"
                    "BEGINDATA\n"
                    "ALIGNDATA(16)\n"
                    "GLOBALNAME g_afVTGProbeEnabled\n"
                    );
    PVTGPROVIDER pProv;
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        PVTGPROBE pProbe;
        RTListForEach(&pProv->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            ScmStreamPrintf(pStrm,
                            "GLOBALNAME g_fVTGProbeEnabled_%s_%s\n"
                            "    db 0\n",
                            pProv->pszName, pProbe->pszName);
        }
    }
    ScmStreamPrintf(pStrm, "GLOBALNAME g_afVTGProbeEnabled_End\n");

    /*
     * Declare the probe data.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; Prob data.\n"
                    ";\n"
                    "BEGINDATA\n"
                    "GLOBALNAME g_abVTGProbeData\n"
                    "\n");
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        PVTGPROBE pProbe;
        RTListForEach(&pProv->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            /** @todo  */
            //ScmStreamPrintf(pStrm,
            //                "GLOBALNAME g_fVTGProbeEnabled_%s_%s\n"
            //                "    db 0\n",
            //                pProv->pszName, pProbe->pszName);
        }
    }
    ScmStreamPrintf(pStrm, "GLOBALNAME g_abVTGProbeData_End\n");

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE generateObject(const char *pszOutput, const char *pszTempAsm)
{
    if (!pszTempAsm)
    {
        size_t cch = strlen(pszOutput);
        char  *psz = (char *)alloca(cch + sizeof(".asm"));
        memcpy(psz, pszOutput, cch);
        memcpy(psz + cch, ".asm", sizeof(".asm"));
        pszTempAsm = psz;
    }

    RTEXITCODE rcExit = generateFile(pszTempAsm, "assembly", generateAssembly);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = generateInvokeAssembler(pszOutput, pszTempAsm);
    RTFileDelete(pszTempAsm);
    return rcExit;
}


static RTEXITCODE generateProbeDefineName(char *pszBuf, size_t cbBuf, const char *pszProvider, const char *pszProbe)
{
    size_t cbMax = strlen(pszProvider) + 1 + strlen(pszProbe) + 1;
    if (cbMax > cbBuf || cbMax > 80)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Probe '%s' in provider '%s' ends up with a too long defined\n", pszProbe, pszProvider);

    while (*pszProvider)
        *pszBuf++ = RT_C_TO_UPPER(*pszProvider++);

    *pszBuf++ = '_';

    while (*pszProbe)
    {
        if (pszProbe[0] == '_' && pszProbe[1] == '_')
            pszProbe++;
        *pszBuf++ = RT_C_TO_UPPER(*pszProbe++);
    }

    *pszBuf = '\0';
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE generateHeaderInner(PSCMSTREAM pStrm)
{
    /*
     * Calc the double inclusion blocker define and then write the file header.
     */
    char szTmp[4096];
    const char *pszName = RTPathFilename(g_pszScript);
    size_t      cchName = strlen(pszName);
    if (cchName >= sizeof(szTmp) - 64)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "File name is too long '%s'", pszName);
    szTmp[0] = '_';
    szTmp[1] = '_';
    szTmp[2] = '_';
    memcpy(&szTmp[3], pszName, cchName);
    szTmp[3 + cchName + 0] = '_';
    szTmp[3 + cchName + 1] = '_';
    szTmp[3 + cchName + 2] = '_';
    szTmp[3 + cchName + 3] = '\0';
    char *psz = &szTmp[3];
    while (*psz)
    {
        if (!RT_C_IS_ALNUM(*psz) && *psz != '_')
            *psz = '_';
        psz++;
    }

    ScmStreamPrintf(pStrm,
                    "/* $Id$ */\n"
                    "/** @file\n"
                    " * Automatically generated from %s. Do NOT edit!\n"
                    " */\n"
                    "\n"
                    "#ifndef %s\n"
                    "#define %s\n"
                    "\n"
                    "#include <iprt/types.h>\n"
                    "\n"
                    "#ifdef _MSC_VER\n"
                    "# define DECL_DATA_SECT(scope, type, name, sect) __declspec(allocate(#sect)) scope type name\n"
                    "#elif defined(__GNUC__)\n"
                    "# ifdef RT_OS_DARWIN\n"
                    "#  define DECL_DATA_SECT(scope, type, name, sect) scope type __attribute__((section(#sect \",\" #sect))) name\n"
                    "# else\n"
                    "#  define DECL_DATA_SECT(scope, type, name, sect) scope type __attribute__((section(#sect))) name\n"
                    "# endif\n"
                    "#else\n"
                    "# error portme\n"
                    "#endif\n"
                    "\n"
                    "typedef struct VBOXTPGPROBELOC\n"
                    "{\n"
                    "    uint32_t    uLine    : 31;\n"
                    "    uint32_t    fEnabled : 1;\n"
                    "    uint32_t    idProbe;\n"
                    "    const char *pszFunction;\n"
                    "    const char *pszFile;\n"
                    "    uint8_t    *pbProbe;\n"
                    "} VBOXTPGPROBELOC;\n"
                    "\n"
                    "RT_C_DECLS_BEGIN\n"
                    ,
                    g_pszScript,
                    szTmp,
                    szTmp);

    /*
     * Declare data, code and macros for each probe.
     */
    PVTGPROVIDER pProv;
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        PVTGPROBE pProbe;
        RTListForEach(&pProv->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            PVTGARG pArg;
            ScmStreamPrintf(pStrm,
                            "extern bool    g_fVTGProbeEnabled_%s_%s;\n"
                            "extern uint8_t g_VTGProbeData_%s_%s;\n"
                            "DECLASM(void)  VTGProbeStub_%s_%s(VBOXTPGPROBELOC *",
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm, ", %s", pArg->pszType);
            }
            generateProbeDefineName(szTmp, sizeof(szTmp), pProv->pszName, pProbe->pszName);
            ScmStreamPrintf(pStrm,
                            ");\n"
                            "#define %s_ENABLED() \\\n"
                            "    (RT_UNLIKELY(g_fVTGProbeEnabled_%s_%s)) \n"
                            "#define %s("
                            , szTmp,
                            pProv->pszName, pProbe->pszName,
                            szTmp);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                if (RTListNodeIsFirst(&pProbe->ArgHead, &pArg->ListEntry))
                    ScmStreamPrintf(pStrm, "%s", pArg->pszName);
                else
                    ScmStreamPrintf(pStrm, ", %s", pArg->pszName);
            }
            ScmStreamPrintf(pStrm,
                            ") \\\n"
                            "    do { \\\n"
                            "        if (RT_UNLIKELY(/*g_fVTGProbeEnabled_%s_%s*/ true)) \\\n"
                            "        { \\\n"
                            "            DECL_DATA_SECT(static, VBOXTPGPROBELOC, s_VTGProbeLoc, VTGPrLc) = \\\n"
                            "            { __LINE__, 0, UINT32_MAX, __PRETTY_FUNCTION__, __FILE__, /*&g_VTGProbeData_%s_%s*/ NULL }; \\\n"
                            "            /*VTGProbeStub_%s_%s(&s_VTGProbeLoc",
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm, ", %s", pArg->pszName);
            }
            ScmStreamPrintf(pStrm,
                            ");*/ RTAssertMsg2(\"%p\", &s_VTGProbeLoc); \\\n"
                            "        } \\\n"
                            "    } while (0)\n"
                            "\n");
        }
    }

    ScmStreamWrite(pStrm, RT_STR_TUPLE("\n"
                                       "RT_C_DECLS_END\n"
                                       "#endif\n"));
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE generateHeader(const char *pszHeader)
{
    return generateFile(pszHeader, "header", generateHeaderInner);
}

/**
 * If the given C word is at off - 1, return @c true and skip beyond it,
 * otherwise return @c false.
 *
 * @retval  true if the given C-word is at the current position minus one char.
 *          The stream position changes.
 * @retval  false if not. The stream position is unchanged.
 *
 * @param   pStream             The stream.
 * @param   cchWord             The length of the word.
 * @param   pszWord             The word.
 */
bool ScmStreamCMatchingWordM1(PSCMSTREAM pStream, const char *pszWord, size_t cchWord)
{
    /* Check stream state. */
    AssertReturn(!pStream->fWriteOrRead, false);
    AssertReturn(RT_SUCCESS(pStream->rc), false);
    AssertReturn(pStream->fFullyLineated, false);

    /* Sufficient chars left on the line? */
    size_t const    iLine   = pStream->iLine;
    AssertReturn(pStream->off > pStream->paLines[iLine].off, false);
    size_t const    cchLeft = pStream->paLines[iLine].cch + pStream->paLines[iLine].off - (pStream->off - 1);
    if (cchWord > cchLeft)
        return false;

    /* Do they match? */
    const char     *psz     = &pStream->pch[pStream->off - 1];
    if (memcmp(psz, pszWord, cchWord))
        return false;

    /* Is it the end of a C word? */
    if (cchWord < cchLeft)
    {
        psz += cchWord;
        if (RT_C_IS_ALNUM(*psz) || *psz == '_')
            return false;
    }

    /* Skip ahead. */
    pStream->off += cchWord - 1;
    return true;
}

/**
 * Get's the C word starting at the current position.
 *
 * @returns Pointer to the word on success and the stream position advanced to
 *          the end of it.
 *          NULL on failure, stream position normally unchanged.
 * @param   pStream             The stream to get the C word from.
 * @param   pcchWord            Where to return the word length.
 */
const char *ScmStreamCGetWord(PSCMSTREAM pStream, size_t *pcchWord)
{
    /* Check stream state. */
    AssertReturn(!pStream->fWriteOrRead, false);
    AssertReturn(RT_SUCCESS(pStream->rc), false);
    AssertReturn(pStream->fFullyLineated, false);

    /* Get the number of chars left on the line and locate the current char. */
    size_t const    iLine   = pStream->iLine;
    size_t const    cchLeft = pStream->paLines[iLine].cch + pStream->paLines[iLine].off - pStream->off;
    const char     *psz     = &pStream->pch[pStream->off];

    /* Is it a leading C character. */
    if (!RT_C_IS_ALPHA(*psz) && !*psz == '_')
        return NULL;

    /* Find the end of the word. */
    char    ch;
    size_t  off = 1;
    while (     off < cchLeft
           &&  (   (ch = psz[off]) == '_'
                || RT_C_IS_ALNUM(ch)))
        off++;

    pStream->off += off;
    *pcchWord = off;
    return psz;
}


/**
 * Get's the C word starting at the current position minus one.
 *
 * @returns Pointer to the word on success and the stream position advanced to
 *          the end of it.
 *          NULL on failure, stream position normally unchanged.
 * @param   pStream             The stream to get the C word from.
 * @param   pcchWord            Where to return the word length.
 */
const char *ScmStreamCGetWordM1(PSCMSTREAM pStream, size_t *pcchWord)
{
    /* Check stream state. */
    AssertReturn(!pStream->fWriteOrRead, false);
    AssertReturn(RT_SUCCESS(pStream->rc), false);
    AssertReturn(pStream->fFullyLineated, false);

    /* Get the number of chars left on the line and locate the current char. */
    size_t const    iLine   = pStream->iLine;
    size_t const    cchLeft = pStream->paLines[iLine].cch + pStream->paLines[iLine].off - (pStream->off - 1);
    const char     *psz     = &pStream->pch[pStream->off - 1];

    /* Is it a leading C character. */
    if (!RT_C_IS_ALPHA(*psz) && !*psz == '_')
        return NULL;

    /* Find the end of the word. */
    char    ch;
    size_t  off = 1;
    while (     off < cchLeft
           &&  (   (ch = psz[off]) == '_'
                || RT_C_IS_ALNUM(ch)))
        off++;

    pStream->off += off - 1;
    *pcchWord = off;
    return psz;
}


/**
 * Parser error with line and position.
 *
 * @returns RTEXITCODE_FAILURE.
 * @param   pStrm               The stream.
 * @param   cb                  The offset from the current position to the
 *                              point of failure.
 * @param   pszMsg              The message to display.
 */
static RTEXITCODE parseError(PSCMSTREAM pStrm, size_t cb, const char *pszMsg)
{
    if (cb)
        ScmStreamSeekRelative(pStrm, -cb);
    size_t const off     = ScmStreamTell(pStrm);
    size_t const iLine   = ScmStreamTellLine(pStrm);
    ScmStreamSeekByLine(pStrm, iLine);
    size_t const offLine = ScmStreamTell(pStrm);

    RTPrintf("%s:%d:%zd: error: %s.\n", g_pszScript, iLine + 1, off - offLine + 1, pszMsg);

    size_t cchLine;
    SCMEOL enmEof;
    const char *pszLine = ScmStreamGetLineByNo(pStrm, iLine, &cchLine, &enmEof);
    if (pszLine)
        RTPrintf("  %.*s\n"
                 "  %*s^\n",
                 cchLine, pszLine, off - offLine, "");
    return RTEXITCODE_FAILURE;
}


/**
 * Parser error with line and position.
 *
 * @returns RTEXITCODE_FAILURE.
 * @param   pStrm               The stream.
 * @param   cb                  The offset from the current position to the
 *                              point of failure.
 * @param   pszMsg              The message to display.
 */
static RTEXITCODE parseErrorAbs(PSCMSTREAM pStrm, size_t off, const char *pszMsg)
{
    ScmStreamSeekAbsolute(pStrm, off);
    return parseError(pStrm, 0, pszMsg);
}

/**
 * Handles a C++ one line comment.
 *
 * @returns Exit code.
 * @param   pStrm               The stream.
 */
static RTEXITCODE parseOneLineComment(PSCMSTREAM pStrm)
{
    ScmStreamSeekByLine(pStrm, ScmStreamTellLine(pStrm) + 1);
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles a multi-line C/C++ comment.
 *
 * @returns Exit code.
 * @param   pStrm               The stream.
 */
static RTEXITCODE parseMultiLineComment(PSCMSTREAM pStrm)
{
    unsigned ch;
    while ((ch = ScmStreamGetCh(pStrm)) != ~(unsigned)0)
    {
        if (ch == '*')
        {
            do
                ch = ScmStreamGetCh(pStrm);
            while (ch == '*');
            if (ch == '/')
                return RTEXITCODE_SUCCESS;
        }
    }

    parseError(pStrm, 1, "Expected end of comment, got end of file");
    return RTEXITCODE_FAILURE;
}


/**
 * Skips spaces and comments.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE.
 * @param   pStrm               The stream..
 */
static RTEXITCODE parseSkipSpacesAndComments(PSCMSTREAM pStrm)
{
    unsigned ch;
    while ((ch = ScmStreamPeekCh(pStrm)) != ~(unsigned)0)
    {
        if (!RT_C_IS_SPACE(ch) && ch != '/')
            return RTEXITCODE_SUCCESS;
        unsigned ch2 = ScmStreamGetCh(pStrm); AssertBreak(ch == ch2); NOREF(ch2);
        if (ch == '/')
        {
            ch = ScmStreamGetCh(pStrm);
            RTEXITCODE rcExit;
            if (ch == '*')
                rcExit = parseMultiLineComment(pStrm);
            else if (ch == '/')
                rcExit = parseOneLineComment(pStrm);
            else
                rcExit = parseError(pStrm, 2, "Unexpected character");
            if (rcExit != RTEXITCODE_SUCCESS)
                return rcExit;
        }
    }

    return parseError(pStrm, 0, "Unexpected end of file");
}


/**
 * Skips spaces and comments, returning the next character.
 *
 * @returns Next non-space-non-comment character. ~(unsigned)0 on EOF or
 *          failure.
 * @param   pStrm               The stream.
 */
static unsigned parseGetNextNonSpaceNonCommentCh(PSCMSTREAM pStrm)
{
    unsigned ch;
    while ((ch = ScmStreamGetCh(pStrm)) != ~(unsigned)0)
    {
        if (!RT_C_IS_SPACE(ch) && ch != '/')
            return ch;
        if (ch == '/')
        {
            ch = ScmStreamGetCh(pStrm);
            RTEXITCODE rcExit;
            if (ch == '*')
                rcExit = parseMultiLineComment(pStrm);
            else if (ch == '/')
                rcExit = parseOneLineComment(pStrm);
            else
                rcExit = parseError(pStrm, 2, "Unexpected character");
            if (rcExit != RTEXITCODE_SUCCESS)
                return ~(unsigned)0;
        }
    }

    parseError(pStrm, 0, "Unexpected end of file");
    return ~(unsigned)0;
}


/**
 * Get the next non-space-non-comment character on a preprocessor line.
 *
 * @returns The next character. On error message and ~(unsigned)0.
 * @param   pStrm               The stream.
 */
static unsigned parseGetNextNonSpaceNonCommentChOnPpLine(PSCMSTREAM pStrm)
{
    size_t   off = ScmStreamTell(pStrm) - 1;
    unsigned ch;
    while ((ch = ScmStreamGetCh(pStrm)) != ~(unsigned)0)
    {
        if (RT_C_IS_SPACE(ch))
        {
            if (ch == '\n' || ch == '\r')
            {
                parseErrorAbs(pStrm, off, "Invalid preprocessor statement");
                break;
            }
        }
        else if (ch == '\\')
        {
            size_t off2 = ScmStreamTell(pStrm) - 1;
            ch = ScmStreamGetCh(pStrm);
            if (ch == '\r')
                ch = ScmStreamGetCh(pStrm);
            if (ch != '\n')
            {
                parseErrorAbs(pStrm, off2, "Expected new line");
                break;
            }
        }
        else
            return ch;
    }
    return ~(unsigned)0;
}



/**
 * Skips spaces and comments.
 *
 * @returns Same as ScmStreamCGetWord
 * @param   pStrm               The stream..
 * @param   pcchWord            Where to return the length.
 */
static const char *parseGetNextCWord(PSCMSTREAM pStrm, size_t *pcchWord)
{
    if (parseSkipSpacesAndComments(pStrm) != RTEXITCODE_SUCCESS)
        return NULL;
    return ScmStreamCGetWord(pStrm, pcchWord);
}



/**
 * Parses interface stability.
 *
 * @returns Interface stability if parsed correctly, otherwise error message and
 *          kVTGStability_Invalid.
 * @param   pStrm               The stream.
 * @param   ch                  The first character in the stability spec.
 */
static kVTGStability parseStability(PSCMSTREAM pStrm, unsigned ch)
{
    switch (ch)
    {
        case 'E':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("External")))
                return kVTGStability_External;
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Evolving")))
                return kVTGStability_Evolving;
            break;
        case 'I':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Internal")))
                return kVTGStability_Internal;
            break;
        case 'O':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Obsolete")))
                return kVTGStability_Obsolete;
            break;
        case 'P':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Private")))
                return kVTGStability_Private;
            break;
        case 'S':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Stable")))
                return kVTGStability_Stable;
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Standard")))
                return kVTGStability_Standard;
            break;
        case 'U':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Unstable")))
                return kVTGStability_Unstable;
            break;
    }
    parseError(pStrm, 1, "Unknown stability specifier");
    return kVTGStability_Invalid;
}


/**
 * Parses data depndency class.
 *
 * @returns Data dependency class if parsed correctly, otherwise error message
 *          and kVTGClass_Invalid.
 * @param   pStrm               The stream.
 * @param   ch                  The first character in the stability spec.
 */
static kVTGClass parseDataDepClass(PSCMSTREAM pStrm, unsigned ch)
{
    switch (ch)
    {
        case 'C':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Common")))
                return kVTGClass_Common;
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Cpu")))
                return kVTGClass_Cpu;
            break;
        case 'G':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Group")))
                return kVTGClass_Group;
            break;
        case 'I':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Isa")))
                return kVTGClass_Isa;
            break;
        case 'P':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Platform")))
                return kVTGClass_Platform;
            break;
        case 'U':
            if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("Unknown")))
                return kVTGClass_Unknown;
            break;
    }
    parseError(pStrm, 1, "Unknown data dependency class specifier");
    return kVTGClass_Invalid;
}

/**
 * Parses a pragma D attributes statement.
 *
 * @returns Suitable exit code, errors message already written on failure.
 * @param   pStrm               The stream.
 */
static RTEXITCODE parsePragmaDAttributes(PSCMSTREAM pStrm)
{
    /*
     * "CodeStability/DataStability/DataDepClass" - no spaces allowed.
     */
    unsigned ch = parseGetNextNonSpaceNonCommentChOnPpLine(pStrm);
    if (ch == ~(unsigned)0)
        return RTEXITCODE_FAILURE;

    kVTGStability enmCode = parseStability(pStrm, ch);
    if (enmCode == kVTGStability_Invalid)
        return RTEXITCODE_FAILURE;
    ch = ScmStreamGetCh(pStrm);
    if (ch != '/')
        return parseError(pStrm, 1, "Expected '/' following the code stability specifier");

    kVTGStability enmData = parseStability(pStrm, ScmStreamGetCh(pStrm));
    if (enmData == kVTGStability_Invalid)
        return RTEXITCODE_FAILURE;
    ch = ScmStreamGetCh(pStrm);
    if (ch != '/')
        return parseError(pStrm, 1, "Expected '/' following the data stability specifier");

    kVTGClass enmDataDep =  parseDataDepClass(pStrm, ScmStreamGetCh(pStrm));
    if (enmDataDep == kVTGClass_Invalid)
        return RTEXITCODE_FAILURE;

    /*
     * Expecting 'provider' followed by the name of an provider defined earlier.
     */
    ch = parseGetNextNonSpaceNonCommentChOnPpLine(pStrm);
    if (ch == ~(unsigned)0)
        return RTEXITCODE_FAILURE;
    if (ch != 'p' || !ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("provider")))
        return parseError(pStrm, 1, "Expected 'provider'");

    size_t      cchName;
    const char *pszName = parseGetNextCWord(pStrm, &cchName);
    if (!pszName)
        return parseError(pStrm, 1, "Expected provider name");

    PVTGPROVIDER pProv;
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        if (   !strncmp(pProv->pszName, pszName, cchName)
            && pProv->pszName[cchName] == '\0')
            break;
    }
    if (!pProv)
        return parseError(pStrm, cchName, "Provider not found");

    /*
     * Which aspect of the provider?
     */
    size_t      cchAspect;
    const char *pszAspect = parseGetNextCWord(pStrm, &cchAspect);
    if (!pszAspect)
        return parseError(pStrm, 1, "Expected provider aspect");

    PVTGATTRS pAttrs;
    if (cchAspect == 8 && !memcmp(pszAspect, "provider", 8))
        pAttrs = &pProv->AttrSelf;
    else if (cchAspect == 8 && !memcmp(pszAspect, "function", 8))
        pAttrs = &pProv->AttrFunctions;
    else if (cchAspect == 6 && !memcmp(pszAspect, "module", 6))
        pAttrs = &pProv->AttrModules;
    else if (cchAspect == 4 && !memcmp(pszAspect, "name", 4))
        pAttrs = &pProv->AttrName;
    else if (cchAspect == 4 && !memcmp(pszAspect, "args", 4))
        pAttrs = &pProv->AttrArguments;
    else
        return parseError(pStrm, cchAspect, "Unknown aspect");

    if (pAttrs->enmCode != kVTGStability_Invalid)
        return parseError(pStrm, cchAspect, "You have already specified these attributes");

    pAttrs->enmCode     = enmCode;
    pAttrs->enmData     = enmData;
    pAttrs->enmDataDep  = enmDataDep;
    return RTEXITCODE_SUCCESS;
}

/**
 * Parses a D pragma statement.
 *
 * @returns Suitable exit code, errors message already written on failure.
 * @param   pStrm               The stream.
 */
static RTEXITCODE parsePragma(PSCMSTREAM pStrm)
{
    RTEXITCODE rcExit;
    unsigned   ch = parseGetNextNonSpaceNonCommentChOnPpLine(pStrm);
    if (ch == ~(unsigned)0)
        rcExit = RTEXITCODE_FAILURE;
    else if (ch == 'D' && ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("D")))
    {
        ch = parseGetNextNonSpaceNonCommentChOnPpLine(pStrm);
        if (ch == ~(unsigned)0)
            rcExit = RTEXITCODE_FAILURE;
        else if (ch == 'a' && ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("attributes")))
            rcExit = parsePragmaDAttributes(pStrm);
        else
            rcExit = parseError(pStrm, 1, "Unknown pragma D");
    }
    else
        rcExit = parseError(pStrm, 1, "Unknown pragma");
    return rcExit;
}


/**
 * Parses a D probe statement.
 *
 * @returns Suitable exit code, errors message already written on failure.
 * @param   pStrm               The stream.
 * @param   pProv               The provider being parsed.
 */
static RTEXITCODE parseProbe(PSCMSTREAM pStrm, PVTGPROVIDER pProv)
{
    /*
     * Next up is a name followed by an opening parenthesis.
     */
    size_t      cchProbe;
    const char *pszProbe = parseGetNextCWord(pStrm, &cchProbe);
    if (!pszProbe)
        return parseError(pStrm, 1, "Expected a probe name starting with an alphabetical character");
    unsigned ch = parseGetNextNonSpaceNonCommentCh(pStrm);
    if (ch != '(')
        return parseError(pStrm, 1, "Expected '(' after the probe name");

    /*
     * Create a probe instance.
     */
    PVTGPROBE pProbe = (PVTGPROBE)RTMemAllocZ(sizeof(*pProbe));
    if (!pProbe)
        return parseError(pStrm, 0, "Out of memory");
    RTListInit(&pProbe->ArgHead);
    RTListAppend(&pProv->ProbeHead, &pProbe->ListEntry);
    pProbe->pszName = strtabInsertN(pszProbe, cchProbe);
    if (!pProbe->pszName)
        return parseError(pStrm, 0, "Out of memory");

    /*
     * Parse loop for the argument.
     */
    PVTGARG pArg    = NULL;
    size_t  cchName = 0;
    size_t  cchArg  = 0;
    char    szArg[4096];
    for (;;)
    {
        ch = parseGetNextNonSpaceNonCommentCh(pStrm);
        switch (ch)
        {
            case ')':
            case ',':
            {
                /* commit the argument */
                if (pArg)
                {
                    if (!cchName)
                        return parseError(pStrm, 1, "Argument has no name");
                    pArg->pszType = strtabInsertN(szArg, cchArg - cchName - 1);
                    pArg->pszName = strtabInsertN(&szArg[cchArg - cchName], cchName);
                    if (!pArg->pszType || !pArg->pszName)
                        return parseError(pStrm, 1, "Out of memory");
                    pArg = NULL;
                    cchName = cchArg = 0;
                }
                if (ch == ')')
                {
                    size_t off = ScmStreamTell(pStrm);
                    ch = parseGetNextNonSpaceNonCommentCh(pStrm);
                    if (ch != ';')
                        return parseErrorAbs(pStrm, off, "Expected ';'");
                    return RTEXITCODE_SUCCESS;
                }
                break;
            }

            default:
            {
                size_t      cchWord;
                const char *pszWord = ScmStreamCGetWordM1(pStrm, &cchWord);
                if (!pszWord)
                    return parseError(pStrm, 0, "Expected argument");
                if (!pArg)
                {
                    pArg = (PVTGARG)RTMemAllocZ(sizeof(*pArg));
                    if (!pArg)
                        return parseError(pStrm, 1, "Out of memory");
                    RTListAppend(&pProbe->ArgHead, &pArg->ListEntry);
                    pProbe->cArgs++;

                    if (cchWord + 1 > sizeof(szArg))
                        return parseError(pStrm, 1, "Too long parameter declaration");
                    memcpy(szArg, pszWord, cchWord);
                    szArg[cchWord] = '\0';
                    cchArg  = cchWord;
                    cchName = 0;
                }
                else
                {
                    if (cchArg + 1 + cchWord + 1 > sizeof(szArg))
                        return parseError(pStrm, 1, "Too long parameter declaration");

                    szArg[cchArg++] = ' ';
                    memcpy(&szArg[cchArg], pszWord, cchWord);
                    cchArg += cchWord;
                    szArg[cchArg] = '\0';
                    cchName = cchWord;
                }
                break;
            }

            case '*':
            {
                if (!pArg)
                    return parseError(pStrm, 1, "A parameter type does not start with an asterix");
                if (cchArg + sizeof(" *") >= sizeof(szArg))
                    return parseError(pStrm, 1, "Too long parameter declaration");
                szArg[cchArg++] = ' ';
                szArg[cchArg++] = '*';
                szArg[cchArg  ] = '\0';
                cchName = 0;
                break;
            }

            case ~(unsigned)0:
                return parseError(pStrm, 0, "Missing closing ')' on probe");
        }
    }
}

/**
 * Parses a D provider statement.
 *
 * @returns Suitable exit code, errors message already written on failure.
 * @param   pStrm               The stream.
 */
static RTEXITCODE parseProvider(PSCMSTREAM pStrm)
{
    /*
     * Next up is a name followed by a curly bracket. Ignore comments.
     */
    RTEXITCODE rcExit = parseSkipSpacesAndComments(pStrm);
    if (rcExit != RTEXITCODE_SUCCESS)
        return parseError(pStrm, 1, "Expected a provider name starting with an alphabetical character");
    size_t      cchName;
    const char *pszName = ScmStreamCGetWord(pStrm, &cchName);
    if (!pszName)
        return parseError(pStrm, 0, "Bad provider name");
    if (RT_C_IS_DIGIT(pszName[cchName - 1]))
        return parseError(pStrm, 1, "A provider name cannot end with digit");

    unsigned ch = parseGetNextNonSpaceNonCommentCh(pStrm);
    if (ch != '{')
        return parseError(pStrm, 1, "Expected '{' after the provider name");

    /*
     * Create a provider instance.
     */
    PVTGPROVIDER pProv = (PVTGPROVIDER)RTMemAllocZ(sizeof(*pProv));
    if (!pProv)
        return parseError(pStrm, 0, "Out of memory");
    RTListInit(&pProv->ProbeHead);
    RTListAppend(&g_ProviderHead, &pProv->ListEntry);
    pProv->pszName = strtabInsertN(pszName, cchName);
    if (!pProv->pszName)
        return parseError(pStrm, 0, "Out of memory");

    /*
     * Parse loop.
     */
    for (;;)
    {
        ch = parseGetNextNonSpaceNonCommentCh(pStrm);
        switch (ch)
        {
            case 'p':
                if (ScmStreamCMatchingWordM1(pStrm, RT_STR_TUPLE("probe")))
                    rcExit = parseProbe(pStrm, pProv);
                else
                    rcExit = parseError(pStrm, 1, "Unexpected character");
                break;

            case '}':
            {
                size_t off = ScmStreamTell(pStrm);
                ch = parseGetNextNonSpaceNonCommentCh(pStrm);
                if (ch == ';')
                    return RTEXITCODE_SUCCESS;
                rcExit = parseErrorAbs(pStrm, off, "Expected ';'");
                break;
            }

            case ~(unsigned)0:
                rcExit = parseError(pStrm, 0, "Missing closing '}' on provider");
                break;

            default:
                rcExit = parseError(pStrm, 1, "Unexpected character");
                break;
        }
        if (rcExit != RTEXITCODE_SUCCESS)
            return rcExit;
    }
}


static RTEXITCODE parseScript(const char *pszScript)
{
    SCMSTREAM Strm;
    int rc = ScmStreamInitForReading(&Strm, pszScript);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to open & read '%s' into memory: %Rrc", pszScript, rc);
    if (g_cVerbosity > 0)
        RTMsgInfo("Parsing '%s'...", pszScript);

    RTEXITCODE  rcExit = RTEXITCODE_SUCCESS;
    unsigned    ch;
    while ((ch = ScmStreamGetCh(&Strm)) != ~(unsigned)0)
    {
        if (RT_C_IS_SPACE(ch))
            continue;
        switch (ch)
        {
            case '/':
                ch = ScmStreamGetCh(&Strm);
                if (ch == '*')
                    rcExit = parseMultiLineComment(&Strm);
                else if (ch == '/')
                    rcExit = parseOneLineComment(&Strm);
                else
                    rcExit = parseError(&Strm, 2, "Unexpected character");
                break;

            case 'p':
                if (ScmStreamCMatchingWordM1(&Strm, RT_STR_TUPLE("provider")))
                    rcExit = parseProvider(&Strm);
                else
                    rcExit = parseError(&Strm, 1, "Unexpected character");
                break;

            case '#':
            {
                ch = parseGetNextNonSpaceNonCommentChOnPpLine(&Strm);
                if (ch == ~(unsigned)0)
                    rcExit != RTEXITCODE_FAILURE;
                else if (ch == 'p' && ScmStreamCMatchingWordM1(&Strm, RT_STR_TUPLE("pragma")))
                    rcExit = parsePragma(&Strm);
                else
                    rcExit = parseError(&Strm, 1, "Unsupported preprocessor directive");
                break;
            }

            default:
                rcExit = parseError(&Strm, 1, "Unexpected character");
                break;
        }
        if (rcExit != RTEXITCODE_SUCCESS)
            return rcExit;
    }

    ScmStreamDelete(&Strm);
    if (g_cVerbosity > 0 && rcExit == RTEXITCODE_SUCCESS)
        RTMsgInfo("Successfully parsed '%s'.", pszScript);
    return rcExit;
}


/**
 * Parses the arguments.
 */
static RTEXITCODE parseArguments(int argc,  char **argv)
{
    enum
    {
        kVBoxTpGOpt_32Bit = 1000,
        kVBoxTpGOpt_64Bit,
        kVBoxTpGOpt_Assembler,
        kVBoxTpGOpt_AssemblerFmtOpt,
        kVBoxTpGOpt_AssemblerFmtVal,
        kVBoxTpGOpt_AssemblerOutputOpt,
        kVBoxTpGOpt_AssemblerOption,
        kVBoxTpGOpt_End
    };

    static RTGETOPTDEF const s_aOpts[] =
    {
        /* dtrace w/ long options */
        { "-32",                                kVBoxTpGOpt_32Bit,                      RTGETOPT_REQ_NOTHING },
        { "-64",                                kVBoxTpGOpt_64Bit,                      RTGETOPT_REQ_NOTHING },
        { "--apply-cpp",                        'C',                                    RTGETOPT_REQ_NOTHING },
        { "--generate-obj",                     'G',                                    RTGETOPT_REQ_NOTHING },
        { "--generate-header",                  'h',                                    RTGETOPT_REQ_NOTHING },
        { "--output",                           'o',                                    RTGETOPT_REQ_STRING  },
        { "--script",                           's',                                    RTGETOPT_REQ_STRING  },
        { "--verbose",                          'v',                                    RTGETOPT_REQ_NOTHING },
        /* out stuff */
        { "--assembler",                        kVBoxTpGOpt_Assembler,                  RTGETOPT_REQ_STRING  },
        { "--assembler-fmt-opt",                kVBoxTpGOpt_AssemblerFmtOpt,            RTGETOPT_REQ_STRING  },
        { "--assembler-fmt-val",                kVBoxTpGOpt_AssemblerFmtVal,            RTGETOPT_REQ_STRING  },
        { "--assembler-output-opt",             kVBoxTpGOpt_AssemblerOutputOpt,         RTGETOPT_REQ_STRING  },
        { "--assembler-option",                 kVBoxTpGOpt_AssemblerOption,            RTGETOPT_REQ_STRING  },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    int rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            /*
             * DTrace compatible options.
             */
            case kVBoxTpGOpt_32Bit:
                g_cBits = 32;
                g_pszAssemblerFmtOpt = g_szAssemblerFmtVal32;
                break;

            case kVBoxTpGOpt_64Bit:
                g_cBits = 64;
                g_pszAssemblerFmtOpt = g_szAssemblerFmtVal64;
                break;

            case 'C':
                g_fApplyCpp = true;
                RTMsgWarning("Ignoring the -C option - no preprocessing of the D script will be performed");
                break;

            case 'G':
                if (   g_enmAction != kVBoxTpGAction_Nothing
                    && g_enmAction != kVBoxTpGAction_GenerateObject)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "-G and -h does not mix");
                g_enmAction = kVBoxTpGAction_GenerateObject;
                break;

            case 'h':
                if (!strcmp(GetOptState.pDef->pszLong, "--generate-header"))
                {
                    if (   g_enmAction != kVBoxTpGAction_Nothing
                        && g_enmAction != kVBoxTpGAction_GenerateHeader)
                        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "-h and -G does not mix");
                    g_enmAction = kVBoxTpGAction_GenerateHeader;
                }
                else
                {
                    /* --help or similar */
                    RTPrintf("VirtualBox Tracepoint Generator\n"
                             "\n"
                             "Usage: %s [options]\n"
                             "\n"
                             "Options:\n", RTProcShortName());
                    for (size_t i = 0; i < RT_ELEMENTS(s_aOpts); i++)
                        if ((unsigned)s_aOpts[i].iShort < 128)
                            RTPrintf("   -%c,%s\n", s_aOpts[i].iShort, s_aOpts[i].pszLong);
                        else
                            RTPrintf("   %s\n", s_aOpts[i].pszLong);
                    return RTEXITCODE_SUCCESS;
                }
                break;

            case 'o':
                if (g_pszOutput)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Output file is already set to '%s'", g_pszOutput);
                g_pszOutput = ValueUnion.psz;
                break;

            case 's':
                if (g_pszScript)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Script file is already set to '%s'", g_pszScript);
                g_pszScript = ValueUnion.psz;
                break;

            case 'v':
                g_cVerbosity++;
                break;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision$";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                return RTEXITCODE_SUCCESS;
            }

            case VINF_GETOPT_NOT_OPTION:
                if (g_enmAction == kVBoxTpGAction_GenerateObject)
                    break; /* object files, ignore them. */
                return RTGetOptPrintError(rc, &ValueUnion);


            /*
             * Out options.
             */
            case kVBoxTpGOpt_Assembler:
                g_pszAssembler = ValueUnion.psz;
                break;

            case kVBoxTpGOpt_AssemblerFmtOpt:
                g_pszAssemblerFmtOpt = ValueUnion.psz;
                break;

            case kVBoxTpGOpt_AssemblerFmtVal:
                g_pszAssemblerFmtVal = ValueUnion.psz;
                break;

            case kVBoxTpGOpt_AssemblerOutputOpt:
                g_pszAssemblerOutputOpt = ValueUnion.psz;
                break;

            case kVBoxTpGOpt_AssemblerOption:
                if (g_cAssemblerOptions >= RT_ELEMENTS(g_apszAssemblerOptions))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many assembly options (max %u)", RT_ELEMENTS(g_apszAssemblerOptions));
                g_apszAssemblerOptions[g_cAssemblerOptions] = ValueUnion.psz;
                g_cAssemblerOptions++;
                break;

            /*
             * Errors and bugs.
             */
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Check that we've got all we need.
     */
    if (g_enmAction == kVBoxTpGAction_Nothing)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No action specified (-h or -G)");
    if (!g_pszScript)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No script file specified (-s)");
    if (!g_pszOutput)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No output file specified (-o)");

    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return 1;

    RTEXITCODE rcExit = parseArguments(argc, argv);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Parse the script.
         */
        RTListInit(&g_ProviderHead);
        rcExit = parseScript(g_pszScript);
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            /*
             * Take action.
             */
            if (g_enmAction == kVBoxTpGAction_GenerateHeader)
                rcExit = generateHeader(g_pszOutput);
            else
                rcExit = generateObject(g_pszOutput, g_pszTempAsm);
        }
    }

    return rcExit;
}

