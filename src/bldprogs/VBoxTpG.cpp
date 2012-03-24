/* $Id$ */
/** @file
 * VBox Build Tool - VBox Tracepoint Generator.
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
#include <VBox/VBoxTpG.h>

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
    uint32_t        offArgList;
    uint32_t        iProbe;
} VTGPROBE;
typedef VTGPROBE *PVTGPROBE;

typedef struct VTGPROVIDER
{
    RTLISTNODE      ListEntry;
    const char     *pszName;

    uint16_t        iFirstProbe;
    uint16_t        cProbes;

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
static const char          *g_pszAssemblerFmtOpt        = "-f";
static const char           g_szAssemblerFmtVal32[]     = "macho32";
static const char           g_szAssemblerFmtVal64[]     = "macho64";
#elif defined(RT_OS_OS2)
static const char          *pszAssembler                = "nasm.exe";
static const char          *pszAssemblerFmtOpt          = "-f";
static const char           g_szAssemblerFmtVal32[]     = "obj";
static const char           g_szAssemblerFmtVal64[]     = "elf64";
#elif defined(RT_OS_WINDOWS)
static const char          *g_pszAssembler              = "yasm.exe";
static const char          *g_pszAssemblerFmtOpt        = "-f";
static const char           g_szAssemblerFmtVal32[]     = "win32";
static const char           g_szAssemblerFmtVal64[]     = "win64";
#else
static const char          *g_pszAssembler              = "yasm";
static const char          *g_pszAssemblerFmtOpt        = "-f";
static const char           g_szAssemblerFmtVal32[]     = "elf32";
static const char           g_szAssemblerFmtVal64[]     = "elf64";
#endif
static const char          *g_pszAssemblerFmtVal        = RT_CONCAT(g_szAssemblerFmtVal, ARCH_BITS);
static const char          *g_pszAssemblerDefOpt        = "-D";
static const char          *g_pszAssemblerIncOpt        = "-I";
static char                 g_szAssemblerIncVal[RTPATH_MAX];
static const char          *g_pszAssemblerIncVal        = __FILE__ "/../../../include/";
static const char          *g_pszAssemblerOutputOpt     = "-o";
static unsigned             g_cAssemblerOptions         = 0;
static const char          *g_apszAssemblerOptions[32];
static const char          *g_pszProbeFnName            = "SUPR0FireProbe";
static bool                 g_fProbeFnImported          = true;
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


/**
 * Retrieves the string table offset of the given string table string.
 *
 * @returns String table offset.
 * @param   pszStrTabString     The string table string.
 */
static uint32_t strtabGetOff(const char *pszStrTabString)
{
    PVTGSTRING pStr = RT_FROM_MEMBER(pszStrTabString, VTGSTRING, szString[0]);
    Assert(pStr->Core.pszString == pszStrTabString);
    return pStr->offStrTab;
}


/**
 * Invokes the assembler.
 *
 * @returns Exit code.
 * @param   pszOutput           The output file.
 * @param   pszTempAsm          The source file.
 */
static RTEXITCODE generateInvokeAssembler(const char *pszOutput, const char *pszTempAsm)
{
    const char     *apszArgs[64];
    unsigned        iArg = 0;

    apszArgs[iArg++] = g_pszAssembler;
    apszArgs[iArg++] = g_pszAssemblerFmtOpt;
    apszArgs[iArg++] = g_pszAssemblerFmtVal;
    apszArgs[iArg++] = g_pszAssemblerDefOpt;
    if (!strcmp(g_pszAssemblerFmtVal, "macho32") || !strcmp(g_pszAssemblerFmtVal, "macho64"))
        apszArgs[iArg++] = "ASM_FORMAT_MACHO";
    else if (!strcmp(g_pszAssemblerFmtVal, "obj") || !strcmp(g_pszAssemblerFmtVal, "omf"))
        apszArgs[iArg++] = "ASM_FORMAT_OMF";
    else if (   !strcmp(g_pszAssemblerFmtVal, "win32")
             || !strcmp(g_pszAssemblerFmtVal, "win64")
             || !strcmp(g_pszAssemblerFmtVal, "pe32")
             || !strcmp(g_pszAssemblerFmtVal, "pe64")
             || !strcmp(g_pszAssemblerFmtVal, "pe") )
        apszArgs[iArg++] = "ASM_FORMAT_PE";
    else if (   !strcmp(g_pszAssemblerFmtVal, "elf32")
             || !strcmp(g_pszAssemblerFmtVal, "elf64")
             || !strcmp(g_pszAssemblerFmtVal, "elf"))
        apszArgs[iArg++] = "ASM_FORMAT_ELF";
    else
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unknown assembler format '%s'", g_pszAssemblerFmtVal);
    apszArgs[iArg++] = g_pszAssemblerDefOpt;
    if (g_cBits == 32)
        apszArgs[iArg++] = "ARCH_BITS=32";
    else
        apszArgs[iArg++] = "ARCH_BITS=64";
    apszArgs[iArg++] = g_pszAssemblerDefOpt;
    if (g_cBits == 32)
        apszArgs[iArg++] = "RT_ARCH_X86";
    else
        apszArgs[iArg++] = "RT_ARCH_AMD64";
    apszArgs[iArg++] = g_pszAssemblerIncOpt;
    apszArgs[iArg++] = g_pszAssemblerIncVal;
    apszArgs[iArg++] = g_pszAssemblerOutputOpt;
    apszArgs[iArg++] = pszOutput;
    for (unsigned i = 0; i < g_cAssemblerOptions; i++)
        apszArgs[iArg++] = g_apszAssemblerOptions[i];
    apszArgs[iArg++] = pszTempAsm;
    apszArgs[iArg]   = NULL;

    if (g_cVerbosity > 1)
    {
        RTMsgInfo("Starting assmbler '%s' with arguments:\n",  g_pszAssembler);
        for (unsigned i = 0; i < iArg; i++)
            RTMsgInfo("  #%02u: '%s'\n",  i, apszArgs[i]);
    }

    RTPROCESS hProc;
    int rc = RTProcCreate(apszArgs[0], apszArgs, RTENV_DEFAULT, RTPROC_FLAGS_SEARCH_PATH, &hProc);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to start '%s' (assmebler): %Rrc", apszArgs[0], rc);

    RTPROCSTATUS Status;
    rc = RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &Status);
    if (RT_FAILURE(rc))
    {
        RTProcTerminate(hProc);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTProcWait failed: %Rrc", rc);
    }
    if (Status.enmReason == RTPROCEXITREASON_SIGNAL)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The assembler failed: signal %d", Status.iStatus);
    if (Status.enmReason != RTPROCEXITREASON_NORMAL)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The assembler failed: abend");
    if (Status.iStatus != 0)
        return RTMsgErrorExit((RTEXITCODE)Status.iStatus, "The assembler failed: exit code %d", Status.iStatus);

    return RTEXITCODE_SUCCESS;
}


/**
 * Worker that does the boring bits when generating a file.
 *
 * @returns Exit code.
 * @param   pszOutput           The name of the output file.
 * @param   pszWhat             What kind of file it is.
 * @param   pfnGenerator        The callback function that provides the contents
 *                              of the file.
 */
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


/**
 * Formats a string and writes it to the SCM stream.
 *
 * @returns The number of bytes written (>= 0). Negative value are IPRT error
 *          status codes.
 * @param   pStream             The stream to write to.
 * @param   pszFormat           The format string.
 * @param   va                  The arguments to format.
 */
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


/**
 * Formats a string and writes it to the SCM stream.
 *
 * @returns The number of bytes written (>= 0). Negative value are IPRT error
 *          status codes.
 * @param   pStream             The stream to write to.
 * @param   pszFormat           The format string.
 * @param   ...                 The arguments to format.
 */
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


/**
 * Generate assembly source that can be turned into an object file.
 *
 * (This is a generateFile callback.)
 *
 * @returns Exit code.
 * @param   pStrm               The output stream.
 */
static RTEXITCODE generateAssembly(PSCMSTREAM pStrm)
{
    PVTGPROVIDER    pProvider;
    PVTGPROBE       pProbe;
    PVTGARG         pArg;


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
                    "%%include \"iprt/asmdefs.mac\"\n"
                    "\n"
                    "\n"
                    ";"
                    "; We put all the data in a dedicated section / segment.\n"
                    ";\n"
                    "; In order to find the probe location specifiers, we do the necessary\n"
                    "; trickery here, ASSUMING that this object comes in first in the link\n"
                    "; editing process.\n"
                    ";\n"
                    "%%ifdef ASM_FORMAT_OMF\n"
                    " %%macro VTG_GLOBAL 2\n"
                    "  global NAME(%%1)\n"
                    "  NAME(%%1):\n"
                    " %%endmacro\n"
                    " segment VTG.Obj public CLASS=DATA align=4096 use32\n"
                    "\n"
                    "%%elifdef ASM_FORMAT_MACHO\n"
                    " %%macro VTG_GLOBAL 2\n"
                    "  global NAME(%%1)\n"
                    "  NAME(%%1):\n"
                    " %%endmacro\n"
                    " ;[section VTG Obj align=4096]\n"
                    " [section .data]\n"
                    "\n"
                    "%%elifdef ASM_FORMAT_PE\n"
                    " %%macro VTG_GLOBAL 2\n"
                    "  global NAME(%%1)\n"
                    "  NAME(%%1):\n"
                    " %%endmacro\n"
                    " [section VTGObj align=4096]\n"
                    "\n"
                    "%%elifdef ASM_FORMAT_ELF\n"
                    " %%macro VTG_GLOBAL 2\n"
                    "  global NAME(%%1):%%2 hidden\n"
                    "  NAME(%%1):\n"
                    " %%endmacro\n"
                    " [section .VTGPrLc.Start progbits alloc noexec write align=1]\n"
                    "VTG_GLOBAL g_aVTGPrLc, data\n"
                    " [section .VTGPrLc progbits alloc noexec write align=1]\n"
                    " [section .VTGPrLc.End progbits alloc noexec write align=1]\n"
                    "VTG_GLOBAL g_aVTGPrLc_End, data\n"
                    " [section .VTGData progbits alloc noexec write align=4096]\n"
                    "\n"
                    "%%else\n"
                    " %%error \"ASM_FORMAT_XXX is not defined\"\n"
                    "%%endif\n"
                    "\n"
                    "\n"
                    "VTG_GLOBAL g_VTGObjHeader, data\n"
                    "                ;0         1         2         3\n"
                    "                ;012345678901234567890123456789012\n"
                    "    db          'VTG Object Header v1.0', 0, 0\n"
                    "    dd          %u\n"
                    "    dd          0\n"
                    "    RTCCPTR_DEF NAME(g_aVTGProviders)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGProviders_End)     - NAME(g_aVTGProviders)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGProbes)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGProbes_End)        - NAME(g_aVTGProbes)\n"
                    "    RTCCPTR_DEF NAME(g_afVTGProbeEnabled)\n"
                    "    RTCCPTR_DEF NAME(g_afVTGProbeEnabled_End) - NAME(g_afVTGProbeEnabled)\n"
                    "    RTCCPTR_DEF NAME(g_achVTGStringTable)\n"
                    "    RTCCPTR_DEF NAME(g_achVTGStringTable_End) - NAME(g_achVTGStringTable)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGArgLists)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGArgLists_End)      - NAME(g_aVTGArgLists)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGPrLc)\n"
                    "    RTCCPTR_DEF NAME(g_aVTGPrLc_End) ; cross section/segment size not possible\n"
                    "    RTCCPTR_DEF 0\n"
                    "    RTCCPTR_DEF 0\n"
                    "    RTCCPTR_DEF 0\n"
                    "    RTCCPTR_DEF 0\n"
                    ,
                    g_pszScript, g_cBits);

    /*
     * Declare the probe enable flags.
     */
    ScmStreamPrintf(pStrm,
                    ";\n"
                    "; Probe enabled flags.  Since these will be accessed all the time\n"
                    "; they are placed together and early in the section to get some more\n"
                    "; cache and TLB hits when the probes are disabled.\n"
                    ";\n"
                    "VTG_GLOBAL g_afVTGProbeEnabled, data\n"
                    );
    uint32_t        cProbes = 0;
    RTListForEach(&g_ProviderHead, pProvider, VTGPROVIDER, ListEntry)
    {
        RTListForEach(&pProvider->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            ScmStreamPrintf(pStrm,
                            "VTG_GLOBAL g_fVTGProbeEnabled_%s_%s, data\n"
                            "    db 0\n",
                            pProvider->pszName, pProbe->pszName);
            cProbes++;
        }
    }
    ScmStreamPrintf(pStrm, "VTG_GLOBAL g_afVTGProbeEnabled_End, data\n");
    if (cProbes >= _32K)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Too many probes: %u (max %u)", cProbes, _32K - 1);

    /*
     * Dump the string table before we start using the strings.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; The string table.\n"
                    ";\n"
                    "VTG_GLOBAL g_achVTGStringTable, data\n");
    g_offStrTab = 0;
    RTStrSpaceEnumerate(&g_StrSpace, generateAssemblyStrTabCallback, pStrm);
    ScmStreamPrintf(pStrm,
                    "VTG_GLOBAL g_achVTGStringTable_End, data\n");

    /*
     * Write out the argument lists before we use them.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; The argument lists.\n"
                    ";\n"
                    "VTG_GLOBAL g_aVTGArgLists, data\n");
    uint32_t off = 0;
    RTListForEach(&g_ProviderHead, pProvider, VTGPROVIDER, ListEntry)
    {
        RTListForEach(&pProvider->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            if (pProbe->offArgList != UINT32_MAX)
                continue;

            /* Write it. */
            pProbe->offArgList = off;
            ScmStreamPrintf(pStrm,
                            "    ; off=%u\n"
                            "    db   %2u     ; Argument count\n"
                            "    db  0, 0, 0 ; Reserved\n"
                            , off, pProbe->cArgs);
            off += 4;
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm,
                                "    dd %6u   ; type '%s'\n"
                                "    dd %6u   ; name '%s'\n",
                                strtabGetOff(pArg->pszType), pArg->pszType,
                                strtabGetOff(pArg->pszName), pArg->pszName);
                off += 8;
            }

            /* Look for matching argument lists (lazy bird walks the whole list). */
            PVTGPROVIDER pProv2;
            RTListForEach(&g_ProviderHead, pProv2, VTGPROVIDER, ListEntry)
            {
                PVTGPROBE pProbe2;
                RTListForEach(&pProvider->ProbeHead, pProbe2, VTGPROBE, ListEntry)
                {
                    if (pProbe2->offArgList != UINT32_MAX)
                        continue;
                    if (pProbe2->cArgs != pProbe->cArgs)
                        continue;

                    PVTGARG pArg2;
                    pArg  = RTListNodeGetNext(&pProbe->ArgHead, VTGARG, ListEntry);
                    pArg2 = RTListNodeGetNext(&pProbe2->ArgHead, VTGARG, ListEntry);
                    int32_t cArgs = pProbe->cArgs;
                    while (   cArgs-- > 0
                           && pArg2->pszName == pArg2->pszName
                           && pArg2->pszType == pArg2->pszType)
                    {
                        pArg  = RTListNodeGetNext(&pArg->ListEntry, VTGARG, ListEntry);
                        pArg2 = RTListNodeGetNext(&pArg2->ListEntry, VTGARG, ListEntry);
                    }
                    if (cArgs >= 0)
                        continue;
                    pProbe2->offArgList = pProbe->offArgList;
                }
            }
        }
    }
    ScmStreamPrintf(pStrm,
                    "VTG_GLOBAL g_aVTGArgLists_End, data\n");


    /*
     * Probe definitions.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; Prob definitions.\n"
                    ";\n"
                    "VTG_GLOBAL g_aVTGProbes, data\n"
                    "\n");
    uint32_t iProvider = 0;
    uint32_t iProbe = 0;
    RTListForEach(&g_ProviderHead, pProvider, VTGPROVIDER, ListEntry)
    {
        pProvider->iFirstProbe = iProbe;
        RTListForEach(&pProvider->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            ScmStreamPrintf(pStrm,
                            "VTG_GLOBAL g_VTGProbeData_%s_%s, data ; idx=#%4u\n"
                            "    dd %6u  ; name\n"
                            "    dd %6u  ; Argument list offset\n"
                            "    dw g_fVTGProbeEnabled_%s_%s - g_afVTGProbeEnabled\n"
                            "    dw %6u  ; provider index\n"
                            "    dd 0       ; for the application\n"
                            ,
                            pProvider->pszName, pProbe->pszName, iProbe,
                            strtabGetOff(pProbe->pszName),
                            pProbe->offArgList,
                            pProvider->pszName, pProbe->pszName,
                            iProvider);
            pProbe->iProbe = iProbe;
            iProbe++;
        }
        pProvider->cProbes = iProbe - pProvider->iFirstProbe;
        iProvider++;
    }
    ScmStreamPrintf(pStrm, "VTG_GLOBAL g_aVTGProbes_End, data\n");

    /*
     * The providers data.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; Provider data.\n"
                    ";\n"
                    "VTG_GLOBAL g_aVTGProviders, data\n");
    iProvider = 0;
    RTListForEach(&g_ProviderHead, pProvider, VTGPROVIDER, ListEntry)
    {
        ScmStreamPrintf(pStrm,
                        "    ; idx=#%4u - %s\n"
                        "    dd %6u  ; name\n"
                        "    dw %6u  ; index of first probe\n"
                        "    dw %6u  ; count of probes\n"
                        "    db %d, %d, %d ; AttrSelf\n"
                        "    db %d, %d, %d ; AttrModules\n"
                        "    db %d, %d, %d ; AttrFunctions\n"
                        "    db %d, %d, %d ; AttrName\n"
                        "    db %d, %d, %d ; AttrArguments\n"
                        "    db 0       ; reserved\n"
                        ,
                        iProvider, pProvider->pszName,
                        strtabGetOff(pProvider->pszName),
                        pProvider->iFirstProbe,
                        pProvider->cProbes,
                        pProvider->AttrSelf.enmCode,        pProvider->AttrSelf.enmData,        pProvider->AttrSelf.enmDataDep,
                        pProvider->AttrModules.enmCode,     pProvider->AttrModules.enmData,     pProvider->AttrModules.enmDataDep,
                        pProvider->AttrFunctions.enmCode,   pProvider->AttrFunctions.enmData,   pProvider->AttrFunctions.enmDataDep,
                        pProvider->AttrName.enmCode,        pProvider->AttrName.enmData,        pProvider->AttrName.enmDataDep,
                        pProvider->AttrArguments.enmCode,   pProvider->AttrArguments.enmData,   pProvider->AttrArguments.enmDataDep);
        iProvider++;
    }
    ScmStreamPrintf(pStrm, "VTG_GLOBAL g_aVTGProviders_End, data\n");

    /*
     * Emit code for the stub functions.
     */
    ScmStreamPrintf(pStrm,
                    "\n"
                    ";\n"
                    "; Prob stubs.\n"
                    ";\n"
                    "BEGINCODE\n"
                    "extern %sNAME(%s)\n", 
                    g_fProbeFnImported ? "IMP" : "",
                    g_pszProbeFnName);
    RTListForEach(&g_ProviderHead, pProvider, VTGPROVIDER, ListEntry)
    {
        RTListForEach(&pProvider->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            ScmStreamPrintf(pStrm,
                            "\n"
                            "VTG_GLOBAL VTGProbeStub_%s_%s, function; (VBOXTPGPROBELOC pVTGProbeLoc",
                            pProvider->pszName, pProbe->pszName);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm, ", %s %s", pArg->pszType, pArg->pszName);
            }
            ScmStreamPrintf(pStrm,
                            ");\n");

            bool const fWin64   = g_cBits == 64 && (!strcmp(g_pszAssemblerFmtVal, "win64") || !strcmp(g_pszAssemblerFmtVal, "pe64"));
            bool const fMachO32 = g_cBits == 32 && !strcmp(g_pszAssemblerFmtVal, "macho32");

            /*
             * Check if the probe in question is enabled.
             */
            if (g_cBits == 32)
                ScmStreamPrintf(pStrm,
                                "        mov     eax, [esp + 4]\n"
                                "        test    byte [eax+3], 0x80 ; fEnabled == true?\n"
                                "        jz      .return            ; jump on false\n");
            else if (fWin64)
                ScmStreamPrintf(pStrm,
                                "        test    byte [rcx+3], 0x80 ; fEnabled == true?\n"
                                "        jz      .return            ; jump on false\n");
            else
                ScmStreamPrintf(pStrm,
                                "        test    byte [rdi+3], 0x80 ; fEnabled == true?\n"
                                "        jz      .return            ; jump on false\n");

            /*
             * Shuffle the arguments around, replacing the location pointer with the probe ID.
             */
            if (fMachO32)
            {
                /* Need to recreate the stack frame entirely here as the probe
                   function differs by taking all uint64_t arguments instead
                   of uintptr_t.  Understandable, but real PITA. */
                ScmStreamPrintf(pStrm, "int3\n");
            }
            else if (g_cBits == 32)
                /* Assumes the size of the arguments are no larger than a
                   pointer.  This is asserted in the header. */
                ScmStreamPrintf(pStrm, g_fProbeFnImported ? 
                                "        mov     edx, [eax + 4]     ; idProbe\n"
                                "        mov     ecx, IMP(%s)\n"
                                "        mov     [esp + 4], edx     ; Replace pVTGProbeLoc with idProbe.\n"
                                "        jmp     ecx\n"
                                :
                                "        mov     edx, [eax + 4]     ; idProbe\n"
                                "        mov     [esp + 4], edx     ; Replace pVTGProbeLoc with idProbe.\n"
                                "        jmp     NAME(%s)\n"
                                , g_pszProbeFnName);
            else if (fWin64)
                ScmStreamPrintf(pStrm, g_fProbeFnImported ? 
                                "        mov     rax, IMP(%s) wrt RIP\n"
                                "        mov     ecx, [rcx + 4]     ; idProbe replaces pVTGProbeLoc.\n"
                                "        jmp     rax\n"
                                :
                                "        mov     ecx, [rcx + 4]     ; idProbe replaces pVTGProbeLoc.\n"
                                "        jmp     NAME(SUPR0FireProbe)\n"
                                , g_pszProbeFnName);
            else
                ScmStreamPrintf(pStrm, g_fProbeFnImported ?
                                "        lea     rax, [IMP(%s) wrt RIP]\n" //??? macho64?
                                "        mov     edi, [rdi + 4]     ; idProbe replaces pVTGProbeLoc.\n"
                                "        jmp     rax\n"
                                :
                                "        mov     edi, [rdi + 4]     ; idProbe replaces pVTGProbeLoc.\n"
                                "        jmp     NAME(SUPR0FireProbe)\n"
                                , g_pszProbeFnName);

            ScmStreamPrintf(pStrm,
                            ".return:\n"
                            "        ret                        ; The probe was disabled, return\n"
                            "\n");
        }
    }

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
                    "#include <VBox/VBoxTpG.h>\n"
                    "\n"
                    "RT_C_DECLS_BEGIN\n"
                    "\n"
                    "#ifdef VBOX_WITH_DTRACE\n"
                    "\n"
                    ,
                    g_pszScript,
                    szTmp,
                    szTmp);

    /*
     * Declare data, code and macros for each probe.
     */
    PVTGPROVIDER pProv;
    PVTGPROBE    pProbe;
    PVTGARG      pArg;
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        RTListForEach(&pProv->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            ScmStreamPrintf(pStrm,
                            "extern bool    g_fVTGProbeEnabled_%s_%s;\n"
                            "extern uint8_t g_VTGProbeData_%s_%s;\n"
                            "DECLASM(void)  VTGProbeStub_%s_%s(PVTGPROBELOC",
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
                            "# define %s_ENABLED() \\\n"
                            "    (RT_UNLIKELY(g_fVTGProbeEnabled_%s_%s)) \n"
                            "# define %s("
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
                            "        if (RT_UNLIKELY(g_fVTGProbeEnabled_%s_%s)) \\\n"
                            "        { \\\n"
                            "            VTG_DECL_VTGPROBELOC(s_VTGProbeLoc) = \\\n"
                            "            { __LINE__, 0, UINT32_MAX, __PRETTY_FUNCTION__, __FILE__, &g_VTGProbeData_%s_%s }; \\\n"
                            "            VTGProbeStub_%s_%s(&s_VTGProbeLoc",
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName,
                            pProv->pszName, pProbe->pszName);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm, ", %s", pArg->pszName);
            }
            ScmStreamPrintf(pStrm,
                            "); \\\n"
                            "        } \\\n");
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                ScmStreamPrintf(pStrm,
                                "        AssertCompile(sizeof(%s) <= sizeof(uintptr_t)); \\\n"
                                "        AssertCompile(sizeof(%s) <= sizeof(uintptr_t)); \\\n",
                                pArg->pszName,
                                pArg->pszType);
            }
            ScmStreamPrintf(pStrm,
                            "    } while (0)\n"
                            "\n");
        }
    }

    ScmStreamPrintf(pStrm,
                    "\n"
                    "#else\n"
                    "\n");
    RTListForEach(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry)
    {
        RTListForEach(&pProv->ProbeHead, pProbe, VTGPROBE, ListEntry)
        {
            generateProbeDefineName(szTmp, sizeof(szTmp), pProv->pszName, pProbe->pszName);
            ScmStreamPrintf(pStrm,
                            "# define %s_ENABLED() (false)\n"
                            "# define %s("
                            , szTmp, szTmp);
            RTListForEach(&pProbe->ArgHead, pArg, VTGARG, ListEntry)
            {
                if (RTListNodeIsFirst(&pProbe->ArgHead, &pArg->ListEntry))
                    ScmStreamPrintf(pStrm, "%s", pArg->pszName);
                else
                    ScmStreamPrintf(pStrm, ", %s", pArg->pszName);
            }
            ScmStreamPrintf(pStrm,
                            ") do { } while (0)\n");
        }
    }

    ScmStreamWrite(pStrm, RT_STR_TUPLE("\n"
                                       "#endif\n"
                                       "\n"
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
    if (RTListNodeIsDummy(&g_ProviderHead, pProv, VTGPROVIDER, ListEntry))
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
    pProbe->offArgList = UINT32_MAX;
    pProbe->pszName    = strtabInsertN(pszProbe, cchProbe);
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
    /*
     * Set / Adjust defaults.
     */
    int rc = RTPathAbs(g_pszAssemblerIncVal, g_szAssemblerIncVal, sizeof(g_szAssemblerIncVal) - 1);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs failed: %Rrc", rc);
    strcat(g_szAssemblerIncVal, "/");
    g_pszAssemblerIncVal = g_szAssemblerIncVal;

    /*
     * Option config.
     */
    enum
    {
        kVBoxTpGOpt_32Bit = 1000,
        kVBoxTpGOpt_64Bit,
        kVBoxTpGOpt_Assembler,
        kVBoxTpGOpt_AssemblerFmtOpt,
        kVBoxTpGOpt_AssemblerFmtVal,
        kVBoxTpGOpt_AssemblerOutputOpt,
        kVBoxTpGOpt_AssemblerOption,
        kVBoxTpGOpt_ProbeFnName,
        kVBoxTpGOpt_ProbeFnImported,
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
        { "--probe-fn-name",                    kVBoxTpGOpt_ProbeFnName,                RTGETOPT_REQ_STRING  },
        { "--probe-fn-imported",                kVBoxTpGOpt_ProbeFnImported,            RTGETOPT_REQ_BOOL    },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process \the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            /*
             * DTrace compatible options.
             */
            case kVBoxTpGOpt_32Bit:
                g_cBits = 32;
                g_pszAssemblerFmtVal = g_szAssemblerFmtVal32;
                break;

            case kVBoxTpGOpt_64Bit:
                g_cBits = 64;
                g_pszAssemblerFmtVal = g_szAssemblerFmtVal64;
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
             * Our options.
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
            
            case kVBoxTpGOpt_ProbeFnName:
                g_pszProbeFnName = ValueUnion.psz;
                break;

            case kVBoxTpGOpt_ProbeFnImported:
                g_pszProbeFnName = ValueUnion.psz;
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

