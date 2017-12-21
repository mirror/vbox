/* $Id$ */
/** @file
 * IPRT - Module dependency checker.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Import checker options.
 */
typedef struct RTCHECKIMPORTSOPTS
{
    /** Number of paths to search. */
    size_t      cPaths;
    /** Search directories. */
    char      **papszPaths;
    /** The loader architecture. */
    RTLDRARCH   enmLdrArch;
} RTCHECKIMPORTSOPTS;
/** Pointer to the checker options. */
typedef RTCHECKIMPORTSOPTS *PRTCHECKIMPORTSOPTS;
/** Pointer to the const checker options. */
typedef RTCHECKIMPORTSOPTS const *PCRTCHECKIMPORTSOPTS;


/**
 * Import module.
 */
typedef struct RTCHECKIMPORTMODULE
{
    /** The module. If NIL, then we've got a export list (papszExports). */
    RTLDRMOD    hLdrMod;
    /** Number of export in the export list.  (Zero if hLdrMod is valid.) */
    size_t      cExports;
    /** Export list. (NULL if hLdrMod is valid.)   */
    char       *papszExports;
    /** The module name. */
    char        szModule[256];
} RTCHECKIMPORTMODULE;
/** Pointer to an import module. */
typedef RTCHECKIMPORTMODULE *PRTCHECKIMPORTMODULE;


/**
 * Import checker state (for each image being checked).
 */
typedef struct RTCHECKIMPORTSTATE
{
    /** The image we're processing. */
    const char             *pszImage;
    /** The image we're processing. */
    PCRTCHECKIMPORTSOPTS    pOpts;
    /** Status code. */
    int                     iRc;
    /** Import hint.   */
    uint32_t                iHint;
    /** Number modules. */
    uint32_t                cImports;
    /** Import modules. */
    RTCHECKIMPORTMODULE     aImports[RT_FLEXIBLE_ARRAY];
} RTCHECKIMPORTSTATE;
/** Pointer to the import checker state. */
typedef RTCHECKIMPORTSTATE *PRTCHECKIMPORTSTATE;



/**
 * @callback_method_impl{FNRTLDRIMPORT}
 */
static DECLCALLBACK(int) GetImportCallback(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol,
                                           unsigned uSymbol, PRTLDRADDR pValue, void *pvUser)
{
    PRTCHECKIMPORTSTATE pState = (PRTCHECKIMPORTSTATE)pvUser;
    int rc;
    NOREF(hLdrMod);

    /*
     * If a module is given, lookup the symbol/ordinal there.
     */
    if (pszModule)
    {
        uint32_t iModule = pState->iHint;
        if (   iModule > pState->cImports
            || strcmp(pState->aImports[iModule].szModule, pszModule) != 0)
        {
            for (iModule = 0; iModule < pState->cImports; iModule++)
                if (strcmp(pState->aImports[iModule].szModule, pszModule) == 0)
                    break;
            if (iModule >= pState->cImports)
                return RTMsgErrorRc(VERR_MODULE_NOT_FOUND, "%s: Failed to locate import module '%s'", pState->pszImage, pszModule);
            pState->iHint = iModule;
        }

        rc = RTLdrGetSymbolEx(pState->aImports[iModule].hLdrMod, NULL, _128M, uSymbol, pszSymbol, pValue);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else if (rc == VERR_LDR_FORWARDER)
            rc= VINF_SUCCESS;
        else
        {
            if (pszSymbol)
                RTMsgError("%s: Missing import '%s' from '%s'!", pState->pszImage, pszSymbol, pszModule);
            else
                RTMsgError("%s: Missing import #%u from '%s'!", pState->pszImage, uSymbol, pszModule);
            pState->iRc = rc;
            rc = VINF_SUCCESS;
            *pValue = _128M + _4K;
        }
    }
    /*
     * Otherwise we need to scan all modules.
     */
    else
    {
        Assert(pszSymbol);
        uint32_t iModule = pState->iHint;
        if (iModule < pState->cImports)
            rc = RTLdrGetSymbolEx(pState->aImports[iModule].hLdrMod, NULL, _128M, uSymbol, pszSymbol, pValue);
        else
            rc = VERR_SYMBOL_NOT_FOUND;
        if (rc == VERR_SYMBOL_NOT_FOUND)
        {
            for (iModule = 0; iModule < pState->cImports; iModule++)
            {
                rc = RTLdrGetSymbolEx(pState->aImports[iModule].hLdrMod, NULL, _128M, uSymbol, pszSymbol, pValue);
                if (rc != VERR_SYMBOL_NOT_FOUND)
                    break;
            }
        }
        if (RT_FAILURE(rc))
        {
            RTMsgError("%s: Missing import '%s'!", pState->pszImage, pszSymbol);
            pState->iRc = rc;
            rc = VINF_SUCCESS;
            *pValue = _128M + _4K;
        }
    }
    return rc;
}


/**
 * Loads an imported module.
 *
 * @returns IPRT status code.
 * @param   pOpts               The check program options.
 * @param   pModule             The import module.
 * @param   pErrInfo            Error buffer (to avoid wasting stack).
 * @param   pszImage            The image we're processing (for error messages).
 */
static int LoadImportModule(PCRTCHECKIMPORTSOPTS pOpts, PRTCHECKIMPORTMODULE pModule, PRTERRINFO pErrInfo, const char *pszImage)

{
    for (uint32_t iPath = 0; iPath < pOpts->cPaths; iPath++)
    {
        char szPath[RTPATH_MAX];
        int rc = RTPathJoin(szPath, sizeof(szPath), pOpts->papszPaths[iPath], pModule->szModule);
        if (   RT_SUCCESS(rc)
            && RTFileExists(szPath))
        {
            RTLDRMOD hLdrMod;
            rc = RTLdrOpenEx(szPath, RTLDR_O_FOR_DEBUG, pOpts->enmLdrArch, &hLdrMod, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                pModule->hLdrMod = hLdrMod;
                RTMsgInfo("Import '%s' -> '%s'\n", pModule->szModule, szPath);
            }
            else if (RTErrInfoIsSet(pErrInfo))
                RTMsgError("%s: Failed opening import image '%s': %Rrc - %s", pszImage, szPath, rc, pErrInfo->pszMsg);
            else
                RTMsgError("%s: Failed opening import image '%s': %Rrc", pszImage, szPath, rc);
            return rc;
        }
    }

    /** @todo export list. */

    return RTMsgErrorRc(VERR_MODULE_NOT_FOUND, "%s: Import module '%s' was not found!", pszImage, pModule->szModule);
}


/**
 * Checks the imports for the given image.
 *
 * @returns IPRT status code.
 * @param   pOpts               The check program options.
 * @param   pszImage            The image to check.
 */
static int rtCheckImportsForImage(PCRTCHECKIMPORTSOPTS pOpts, const char *pszImage)
{
    /*
     * Open the image.
     */
    RTERRINFOSTATIC ErrInfo;
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpenEx(pszImage, 0 /*fFlags*/, RTLDRARCH_WHATEVER, &hLdrMod, RTErrInfoInitStatic(&ErrInfo));
    if (RT_FAILURE(rc) && RTErrInfoIsSet(&ErrInfo.Core))
        return RTMsgErrorRc(rc, "Failed opening image '%s': %Rrc - %s", pszImage, rc, ErrInfo.Core.pszMsg);
    if (RT_FAILURE(rc))
        return RTMsgErrorRc(rc, "Failed opening image '%s': %Rrc", pszImage, rc);

    /*
     * Do the import modules first.
     */
    uint32_t cImports = 0;
    rc = RTLdrQueryProp(hLdrMod, RTLDRPROP_IMPORT_COUNT, &cImports, sizeof(cImports));
    if (RT_SUCCESS(rc))
    {
        RTCHECKIMPORTSTATE *pState = (RTCHECKIMPORTSTATE *)RTMemAllocZ(RT_UOFFSETOF(RTCHECKIMPORTSTATE, aImports[cImports + 1]));
        if (pState)
        {
            pState->pszImage = pszImage;
            pState->pOpts    = pOpts;
            pState->cImports = cImports;

            for (uint32_t iImport = 0; iImport < cImports; iImport++)
            {
                *(uint32_t *)&pState->aImports[iImport].szModule[0] = iImport;
                rc = RTLdrQueryProp(hLdrMod, RTLDRPROP_IMPORT_MODULE, pState->aImports[iImport].szModule,
                                    sizeof(pState->aImports[iImport].szModule));
                if (RT_FAILURE(rc))
                {
                    RTMsgError("%s: Error querying import #%u: %Rrc", pszImage, iImport, rc);
                    break;
                }
                rc = LoadImportModule(pOpts, &pState->aImports[iImport], &ErrInfo.Core, pszImage);
                if (RT_FAILURE(rc))
                    break;
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Get the image bits, indirectly resolving imports.
                 */
                size_t cbImage = RTLdrSize(hLdrMod);
                void  *pvImage = RTMemAllocZ(cbImage);
                if (pvImage)
                {
                    pState->iRc = VINF_SUCCESS;
                    rc = RTLdrGetBits(hLdrMod, pvImage, _4M, GetImportCallback, pState);
                    if (RT_SUCCESS(rc))
                        rc = pState->iRc;
                    else
                        RTMsgError("%s: RTLdrGetBits failed: %Rrc", pszImage, rc);

                    RTMemFree(pvImage);
                }
                else
                    rc = RTMsgErrorRc(VERR_NO_MEMORY, "%s: out of memory", pszImage);
            }

            AssertCompile(NIL_RTLDRMOD == NULL);
            for (uint32_t iImport = 0; iImport < cImports; iImport++)
                if (pState->aImports[iImport].hLdrMod != NIL_RTLDRMOD)
                    RTLdrClose(pState->aImports[iImport].hLdrMod);
            RTMemFree(pState);
        }
        else
            rc = RTMsgErrorRc(VERR_NO_MEMORY, "%s: out of memory", pszImage);
    }
    else
        RTMsgError("%s: Querying RTLDRPROP_IMPORT_COUNT failed: %Rrc", pszImage, rc);
    RTLdrClose(hLdrMod);
    return rc;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTCHECKIMPORTSOPTS Opts;
    Opts.cPaths     = 0;
    Opts.papszPaths = NULL;
    Opts.enmLdrArch = RTLDRARCH_WHATEVER;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--path", 'p', RTGETOPT_REQ_STRING },
    };
    RTGETOPTSTATE State;
    rc = RTGetOptInit(&State, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    RTEXITCODE    rcExit = RTEXITCODE_SUCCESS;
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'p':
                if ((Opts.cPaths % 16) == 0)
                {
                    void *pvNew = RTMemRealloc(Opts.papszPaths, sizeof(Opts.papszPaths[0]) * (Opts.cPaths + 16));
                    AssertRCReturn(rc, RTEXITCODE_FAILURE);
                    Opts.papszPaths = (char **)pvNew;
                }
                Opts.papszPaths[Opts.cPaths] = RTStrDup(ValueUnion.psz);
                AssertReturn(Opts.papszPaths[Opts.cPaths], RTEXITCODE_FAILURE);
                Opts.cPaths++;
                break;

            case VINF_GETOPT_NOT_OPTION:
                rc = rtCheckImportsForImage(&Opts, ValueUnion.psz);
                if (RT_FAILURE(rc))
                    rcExit = RTEXITCODE_FAILURE;
                break;

            case 'h':
                RTPrintf("Usage: RTCheckImports [-p|--path <dir>] [-h|--help] [-V|--version] <image [..]>\n"
                         "Checks library imports.\n");
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    return rcExit;
}

