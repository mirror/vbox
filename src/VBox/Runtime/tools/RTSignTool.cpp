/* $Id$ */
/** @file
 * IPRT - Signing Tool.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/message.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/crypto/x509.h>
#include <iprt/crypto/pkcs7.h>
#include <iprt/crypto/store.h>
#ifdef VBOX
# include <VBox/sup.h> /* Certificates */
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Help detail levels. */
typedef enum RTSIGNTOOLHELP
{
    RTSIGNTOOLHELP_USAGE,
    RTSIGNTOOLHELP_FULL
} RTSIGNTOOLHELP;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static RTEXITCODE HandleHelp(int cArgs, char **papszArgs);
static RTEXITCODE HelpHelp(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel);
static RTEXITCODE HandleVersion(int cArgs, char **papszArgs);




/*
 * The 'extract-exe-signer-cert' command.
 */
static RTEXITCODE HelpExtractExeSignerCert(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel)
{
    RTStrmPrintf(pStrm, "extract-exe-signer-cert [--ber|--cer|--der] [--exe|-e] <exe> [--output|-o] <outfile.cer>\n");
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE HandleExtractExeSignerCert(int cArgs, char **papszArgs)
{
    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--ber",    'b', RTGETOPT_REQ_NOTHING },
        { "--cer",    'c', RTGETOPT_REQ_NOTHING },
        { "--der",    'd', RTGETOPT_REQ_NOTHING },
        { "--exe",    'e', RTGETOPT_REQ_STRING },
        { "--output", 'o', RTGETOPT_REQ_STRING },
    };

    const char *pszExe = NULL;
    const char *pszOut = NULL;
    RTLDRARCH   enmLdrArch   = RTLDRARCH_WHATEVER;
    uint32_t    fCursorFlags = RTASN1CURSOR_FLAGS_DER;

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    RTGETOPTUNION ValueUnion;
    int ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'e':   pszExe = ValueUnion.psz; break;
            case 'o':   pszOut = ValueUnion.psz; break;
            case 'b':   fCursorFlags = 0; break;
            case 'c':   fCursorFlags = RTASN1CURSOR_FLAGS_CER; break;
            case 'd':   fCursorFlags = RTASN1CURSOR_FLAGS_DER; break;
            case 'V':   return HandleVersion(cArgs, papszArgs);
            case 'h':   return HelpExtractExeSignerCert(g_pStdOut, RTSIGNTOOLHELP_FULL);

            case VINF_GETOPT_NOT_OPTION:
                if (!pszExe)
                    pszExe = ValueUnion.psz;
                else if (!pszOut)
                    pszOut = ValueUnion.psz;
                else
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Too many file arguments: %s", ValueUnion.psz);
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (!pszExe)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No executable given.");
    if (!pszOut)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No output file given.");
    if (RTPathExists(pszOut))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The output file '%s' exists.", pszOut);

    /*
     * Do it.
     */

    /* Open the executable image and query the PKCS7 info. */
    RTLDRMOD hLdrMod;
    rc = RTLdrOpen(pszExe, RTLDR_O_FOR_VALIDATION, enmLdrArch, &hLdrMod);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening executable image '%s': %Rrc", pszExe, rc);

    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
#ifdef DEBUG
    size_t     cbBuf = 64;
#else
    size_t     cbBuf = _512K;
#endif
    void      *pvBuf = RTMemAlloc(cbBuf);
    size_t     cbRet = 0;
    rc = RTLdrQueryPropEx(hLdrMod, RTLDRPROP_PKCS7_SIGNED_DATA, NULL /*pvBits*/, pvBuf, cbBuf, &cbRet);
    if (rc == VERR_BUFFER_OVERFLOW && cbRet < _4M && cbRet > 0)
    {
        RTMemFree(pvBuf);
        cbBuf = cbRet;
        pvBuf = RTMemAlloc(cbBuf);
        rc = RTLdrQueryPropEx(hLdrMod, RTLDRPROP_PKCS7_SIGNED_DATA, NULL /*pvBits*/, pvBuf, cbBuf, &cbRet);
    }
    if (RT_SUCCESS(rc))
    {
        static RTERRINFOSTATIC s_StaticErrInfo;
        RTErrInfoInitStatic(&s_StaticErrInfo);

        /*
         * Decode the output.
         */
        RTASN1CURSORPRIMARY PrimaryCursor;
        RTAsn1CursorInitPrimary(&PrimaryCursor, pvBuf, (uint32_t)cbRet, &s_StaticErrInfo.Core,
                                &g_RTAsn1DefaultAllocator, fCursorFlags, "exe");
        RTCRPKCS7CONTENTINFO Pkcs7Ci;
        rc = RTCrPkcs7ContentInfo_DecodeAsn1(&PrimaryCursor.Cursor, 0, &Pkcs7Ci, "pkcs7");
        if (RT_SUCCESS(rc))
        {
            if (RTCrPkcs7ContentInfo_IsSignedData(&Pkcs7Ci))
            {
                PCRTCRPKCS7SIGNEDDATA pSd = Pkcs7Ci.u.pSignedData;
                if (pSd->SignerInfos.cItems == 1)
                {
                    PCRTCRPKCS7ISSUERANDSERIALNUMBER pISN = &pSd->SignerInfos.paItems[0].IssuerAndSerialNumber;
                    PCRTCRX509CERTIFICATE pCert;
                    pCert = RTCrPkcs7SetOfCerts_FindX509ByIssuerAndSerialNumber(&pSd->Certificates,
                                                                                &pISN->Name, &pISN->SerialNumber);
                    if (pCert)
                    {
                        /*
                         * Write it out.
                         */
                        RTFILE hFile;
                        rc = RTFileOpen(&hFile, pszOut, RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE);
                        if (RT_SUCCESS(rc))
                        {
                            uint32_t cbCert = pCert->SeqCore.Asn1Core.cbHdr + pCert->SeqCore.Asn1Core.cb;
                            rc = RTFileWrite(hFile, pCert->SeqCore.Asn1Core.uData.pu8 - pCert->SeqCore.Asn1Core.cbHdr,
                                             cbCert, NULL);
                            if (RT_SUCCESS(rc))
                            {
                                rc = RTFileClose(hFile);
                                if (RT_SUCCESS(rc))
                                {
                                    hFile  = NIL_RTFILE;
                                    rcExit = RTEXITCODE_SUCCESS;
                                    RTMsgInfo("Successfully wrote %u bytes to '%s'", cbCert, pszOut);
                                }
                                else
                                    RTMsgError("RTFileClose failed: %Rrc", rc);
                            }
                            else
                                RTMsgError("RTFileWrite failed: %Rrc", rc);
                            RTFileClose(hFile);
                        }
                        else
                            RTMsgError("Error opening '%s': %Rrc", pszOut, rc);
                    }
                    else
                        RTMsgError("Certificate not found.");
                }
                else
                    RTMsgError("SignerInfo count: %u", pSd->SignerInfos.cItems);
            }
            else
                RTMsgError("No PKCS7 content: ContentType=%s", Pkcs7Ci.ContentType.szObjId);
            RTAsn1VtDelete(&Pkcs7Ci.SeqCore.Asn1Core);
        }
        else
            RTMsgError("RTPkcs7ContentInfoDecodeAsn1 failed: %Rrc - %s", rc, s_StaticErrInfo.szMsg);
    }
    else
        RTMsgError("RTLDRPROP_PKCS7_SIGNED_DATA failed on '%s': %Rrc", pszExe, rc);
    RTMemFree(pvBuf);
    rc = RTLdrClose(hLdrMod);
    if (RT_FAILURE(rc))
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTLdrClose failed: %Rrc\n", rc);
    return rcExit;
}

#ifndef IPRT_IN_BUILD_TOOL

/*
 * The 'verify-exe' command.
 */
static RTEXITCODE HelpVerifyExe(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel)
{
    RTStrmPrintf(pStrm,
                 "verify-exe [--verbose|--quiet] [--kernel] [--root <root-cert.der>] [--additional <supp-cert.der>]\n"
                 "        [--type <win|osx>] <exe1> [exe2 [..]]\n");
    return RTEXITCODE_SUCCESS;
}

typedef struct VERIFYEXESTATE
{
    RTCRSTORE   hRootStore;
    RTCRSTORE   hKernelRootStore;
    RTCRSTORE   hAdditionalStore;
    bool        fKernel;
    int         cVerbose;
    enum { kSignType_Windows, kSignType_OSX } enmSignType;
    uint64_t    uTimestamp;
    RTLDRARCH   enmLdrArch;
} VERIFYEXESTATE;

#ifdef VBOX
/** Certificate store load set.
 * Declared outside HandleVerifyExe because of braindead gcc visibility crap. */
struct STSTORESET
{
    RTCRSTORE       hStore;
    PCSUPTAENTRY    paTAs;
    unsigned        cTAs;
};
#endif

/**
 * @callback_method_impl{FNRTCRPKCS7VERIFYCERTCALLBACK,
 * Standard code signing.  Use this for Microsoft SPC.}
 */
static DECLCALLBACK(int) VerifyExecCertVerifyCallback(PCRTCRX509CERTIFICATE pCert, RTCRX509CERTPATHS hCertPaths, uint32_t fFlags,
                                                      void *pvUser, PRTERRINFO pErrInfo)
{
    VERIFYEXESTATE *pState = (VERIFYEXESTATE *)pvUser;
    uint32_t        cPaths = hCertPaths != NIL_RTCRX509CERTPATHS ? RTCrX509CertPathsGetPathCount(hCertPaths) : 0;

    /*
     * Dump all the paths.
     */
    if (pState->cVerbose > 0)
    {
        for (uint32_t iPath = 0; iPath < cPaths; iPath++)
        {
            RTPrintf("---\n");
            RTCrX509CertPathsDumpOne(hCertPaths, iPath, pState->cVerbose, RTStrmDumpPrintfV, g_pStdOut);
            *pErrInfo->pszMsg = '\0';
        }
        RTPrintf("---\n");
    }

    /*
     * Test signing certificates normally doesn't have all the necessary
     * features required below.  So, treat them as special cases.
     */
    if (   hCertPaths == NIL_RTCRX509CERTPATHS
        && RTCrX509Name_Compare(&pCert->TbsCertificate.Issuer, &pCert->TbsCertificate.Subject) == 0)
    {
        RTMsgInfo("Test signed.\n");
        return VINF_SUCCESS;
    }

    if (hCertPaths == NIL_RTCRX509CERTPATHS)
        RTMsgInfo("Signed by trusted certificate.\n");

    /*
     * Standard code signing capabilites required.
     */
    int rc = RTCrPkcs7VerifyCertCallbackCodeSigning(pCert, hCertPaths, fFlags, NULL, pErrInfo);
    if (   RT_SUCCESS(rc)
        && (fFlags & RTCRPKCS7VCC_F_SIGNED_DATA))
    {
        /*
         * If kernel signing, a valid certificate path must be anchored by the
         * microsoft kernel signing root certificate.  The only alternative is
         * test signing.
         */
        if (pState->fKernel && hCertPaths != NIL_RTCRX509CERTPATHS)
        {
            uint32_t cFound = 0;
            uint32_t cValid = 0;
            for (uint32_t iPath = 0; iPath < cPaths; iPath++)
            {
                bool                            fTrusted;
                PCRTCRX509NAME                  pSubject;
                PCRTCRX509SUBJECTPUBLICKEYINFO  pPublicKeyInfo;
                int                             rcVerify;
                rc = RTCrX509CertPathsQueryPathInfo(hCertPaths, iPath, &fTrusted, NULL /*pcNodes*/, &pSubject, &pPublicKeyInfo,
                                                    NULL, NULL /*pCertCtx*/, &rcVerify);
                AssertRCBreak(rc);

                if (RT_SUCCESS(rcVerify))
                {
                    Assert(fTrusted);
                    cValid++;

                    /* Search the kernel signing root store for a matching anchor. */
                    RTCRSTORECERTSEARCH Search;
                    rc = RTCrStoreCertFindBySubjectOrAltSubjectByRfc5280(pState->hKernelRootStore, pSubject, &Search);
                    AssertRCBreak(rc);
                    PCRTCRCERTCTX pCertCtx;
                    while ((pCertCtx = RTCrStoreCertSearchNext(pState->hKernelRootStore, &Search)) != NULL)
                    {
                        PCRTCRX509SUBJECTPUBLICKEYINFO pPubKeyInfo;
                        if (pCertCtx->pCert)
                            pPubKeyInfo = &pCertCtx->pCert->TbsCertificate.SubjectPublicKeyInfo;
                        else if (pCertCtx->pTaInfo)
                            pPubKeyInfo = &pCertCtx->pTaInfo->PubKey;
                        else
                            pPubKeyInfo = NULL;
                        if (RTCrX509SubjectPublicKeyInfo_Compare(pPubKeyInfo, pPublicKeyInfo) == 0)
                            cFound++;
                        RTCrCertCtxRelease(pCertCtx);
                    }

                    int rc2 = RTCrStoreCertSearchDestroy(pState->hKernelRootStore, &Search); AssertRC(rc2);
                }
            }
            if (RT_SUCCESS(rc) && cFound == 0)
                rc = RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE, "Not valid kernel code signature.");
            if (RT_SUCCESS(rc) && cValid != 2)
                RTMsgWarning("%u valid paths, expected 2", cValid);
        }
    }

    return rc;
}


/** @callback_method_impl{FNRTLDRVALIDATESIGNEDDATA}  */
static DECLCALLBACK(int) VerifyExeCallback(RTLDRMOD hLdrMod, RTLDRSIGNATURETYPE enmSignature,
                                           void const *pvSignature, size_t cbSignature,
                                           PRTERRINFO pErrInfo, void *pvUser)
{
    VERIFYEXESTATE *pState = (VERIFYEXESTATE *)pvUser;
    switch (enmSignature)
    {
        case RTLDRSIGNATURETYPE_PKCS7_SIGNED_DATA:
        {
            PCRTCRPKCS7CONTENTINFO pContentInfo = (PCRTCRPKCS7CONTENTINFO)pvSignature;

            RTTIMESPEC ValidationTime;
            RTTimeSpecSetSeconds(&ValidationTime, pState->uTimestamp);

            /*
             * Dump the signed data if so requested.
             */
            if (pState->cVerbose)
                RTAsn1Dump(&pContentInfo->SeqCore.Asn1Core, 0, 0, RTStrmDumpPrintfV, g_pStdOut);


            /*
             * Do the actual verification.  Will have to modify this so it takes
             * the authenticode policies into account.
             */
            return RTCrPkcs7VerifySignedData(pContentInfo,
                                             RTCRPKCS7VERIFY_SD_F_COUNTER_SIGNATURE_SIGNING_TIME_ONLY
                                             | RTCRPKCS7VERIFY_SD_F_ALWAYS_USE_SIGNING_TIME_IF_PRESENT
                                             | RTCRPKCS7VERIFY_SD_F_ALWAYS_USE_MS_TIMESTAMP_IF_PRESENT,
                                             pState->hAdditionalStore, pState->hRootStore, &ValidationTime,
                                             VerifyExecCertVerifyCallback, pState, pErrInfo);
        }

        default:
            return RTErrInfoSetF(pErrInfo, VERR_NOT_SUPPORTED, "Unsupported signature type: %d", enmSignature);
    }
    return VINF_SUCCESS;
}

/** Worker for HandleVerifyExe. */
static RTEXITCODE HandleVerifyExeWorker(VERIFYEXESTATE *pState, const char *pszFilename, PRTERRINFOSTATIC pStaticErrInfo)
{
    /*
     * Open the executable image and verify it.
     */
    RTLDRMOD hLdrMod;
    int rc = RTLdrOpen(pszFilename, RTLDR_O_FOR_VALIDATION, pState->enmLdrArch, &hLdrMod);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error opening executable image '%s': %Rrc", pszFilename, rc);


    rc = RTLdrQueryProp(hLdrMod, RTLDRPROP_TIMESTAMP_SECONDS, &pState->uTimestamp, sizeof(pState->uTimestamp));
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrVerifySignature(hLdrMod, VerifyExeCallback, pState, RTErrInfoInitStatic(pStaticErrInfo));
        if (RT_SUCCESS(rc))
            RTMsgInfo("'%s' is valid.\n", pszFilename);
        else
            RTMsgError("RTLdrVerifySignature failed on '%s': %Rrc - %s\n", pszFilename, rc, pStaticErrInfo->szMsg);
    }
    else
        RTMsgError("RTLdrQueryProp/RTLDRPROP_TIMESTAMP_SECONDS failed on '%s': %Rrc\n", pszFilename, rc);

    int rc2 = RTLdrClose(hLdrMod);
    if (RT_FAILURE(rc2))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTLdrClose failed: %Rrc\n", rc2);
    if (RT_FAILURE(rc))
        return rc != VERR_LDRVI_NOT_SIGNED ? RTEXITCODE_FAILURE : RTEXITCODE_SKIPPED;

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE HandleVerifyExe(int cArgs, char **papszArgs)
{
    RTERRINFOSTATIC StaticErrInfo;

    /* Note! This code does not try to clean up the crypto stores on failure.
             This is intentional as the code is only expected to be used in a
             one-command-per-process environment where we do exit() upon
             returning from this function. */

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--kernel",       'k', RTGETOPT_REQ_NOTHING },
        { "--root",         'r', RTGETOPT_REQ_STRING },
        { "--additional",   'a', RTGETOPT_REQ_STRING },
        { "--add",          'a', RTGETOPT_REQ_STRING },
        { "--type",         't', RTGETOPT_REQ_STRING },
        { "--verbose",      'v', RTGETOPT_REQ_NOTHING },
        { "--quiet",        'q', RTGETOPT_REQ_NOTHING },
    };

    VERIFYEXESTATE State =
    {
        NIL_RTCRSTORE, NIL_RTCRSTORE, NIL_RTCRSTORE, false, false,
        VERIFYEXESTATE::kSignType_Windows, 0, RTLDRARCH_WHATEVER
    };
    int rc = RTCrStoreCreateInMem(&State.hRootStore, 0);
    if (RT_SUCCESS(rc))
        rc = RTCrStoreCreateInMem(&State.hKernelRootStore, 0);
    if (RT_SUCCESS(rc))
        rc = RTCrStoreCreateInMem(&State.hAdditionalStore, 0);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error creating in-memory certificate store: %Rrc", rc);

    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    RTGETOPTUNION ValueUnion;
    int ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) && ch != VINF_GETOPT_NOT_OPTION)
    {
        switch (ch)
        {
            case 'r': case 'a':
                rc = RTCrStoreCertAddFromFile(ch == 'r' ? State.hRootStore : State.hAdditionalStore,
                                              RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                              ValueUnion.psz, RTErrInfoInitStatic(&StaticErrInfo));
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error loading certificate '%s': %Rrc - %s",
                                          ValueUnion.psz, rc, StaticErrInfo.szMsg);
                if (RTErrInfoIsSet(&StaticErrInfo.Core))
                    RTMsgWarning("Warnings loading certificate '%s': %s", ValueUnion.psz, StaticErrInfo.szMsg);
                break;

            case 't':
                if (!strcmp(ValueUnion.psz, "win") || !strcmp(ValueUnion.psz, "windows"))
                    State.enmSignType = VERIFYEXESTATE::kSignType_Windows;
                else if (!strcmp(ValueUnion.psz, "osx") || !strcmp(ValueUnion.psz, "apple"))
                    State.enmSignType = VERIFYEXESTATE::kSignType_OSX;
                else
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Unknown signing type: '%s'", ValueUnion.psz);
                break;

            case 'k': State.fKernel = true; break;
            case 'v': State.cVerbose++; break;
            case 'q': State.cVerbose = 0; break;
            case 'V': return HandleVersion(cArgs, papszArgs);
            case 'h': return HelpVerifyExe(g_pStdOut, RTSIGNTOOLHELP_FULL);
            default:  return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (ch != VINF_GETOPT_NOT_OPTION)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No executable given.");

    /*
     * Populate the certificate stores according to the signing type.
     */
#ifdef VBOX
    unsigned          cSets = 0;
    struct STSTORESET aSets[6];
#endif

    switch (State.enmSignType)
    {
        case VERIFYEXESTATE::kSignType_Windows:
#ifdef VBOX
            aSets[cSets].hStore  = State.hRootStore;
            aSets[cSets].paTAs   = g_aSUPTimestampTAs;
            aSets[cSets].cTAs    = g_cSUPTimestampTAs;
            cSets++;
            aSets[cSets].hStore  = State.hRootStore;
            aSets[cSets].paTAs   = g_aSUPSpcRootTAs;
            aSets[cSets].cTAs    = g_cSUPSpcRootTAs;
            cSets++;
            aSets[cSets].hStore  = State.hRootStore;
            aSets[cSets].paTAs   = g_aSUPNtKernelRootTAs;
            aSets[cSets].cTAs    = g_cSUPNtKernelRootTAs;
            cSets++;
            aSets[cSets].hStore  = State.hKernelRootStore;
            aSets[cSets].paTAs   = g_aSUPNtKernelRootTAs;
            aSets[cSets].cTAs    = g_cSUPNtKernelRootTAs;
            cSets++;
#endif
            break;

        case VERIFYEXESTATE::kSignType_OSX:
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Mac OS X executable signing is not implemented.");
    }

#ifdef VBOX
    for (unsigned i = 0; i < cSets; i++)
        for (unsigned j = 0; j < aSets[i].cTAs; j++)
        {
            rc = RTCrStoreCertAddEncoded(aSets[i].hStore, RTCRCERTCTX_F_ENC_TAF_DER, aSets[i].paTAs[j].pch,
                                         aSets[i].paTAs[j].cb, RTErrInfoInitStatic(&StaticErrInfo));
            if (RT_FAILURE(rc))
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTCrStoreCertAddEncoded failed (%u/%u): %s",
                                      i, j, StaticErrInfo.szMsg);
        }
#endif

    /*
     * Do it.
     */
    RTEXITCODE rcExit;
    for (;;)
    {
        rcExit = HandleVerifyExeWorker(&State, ValueUnion.psz, &StaticErrInfo);
        if (rcExit != RTEXITCODE_SUCCESS)
            break;

        /*
         * Next file
         */
        ch = RTGetOpt(&GetState, &ValueUnion);
        if (ch == 0)
            break;
        if (ch != VINF_GETOPT_NOT_OPTION)
        {
            rcExit = RTGetOptPrintError(ch, &ValueUnion);
            break;
        }
    }

    /*
     * Clean up.
     */
    uint32_t cRefs;
    cRefs = RTCrStoreRelease(State.hRootStore);       Assert(cRefs == 0);
    cRefs = RTCrStoreRelease(State.hKernelRootStore); Assert(cRefs == 0);
    cRefs = RTCrStoreRelease(State.hAdditionalStore); Assert(cRefs == 0);

    return rcExit;
}

#endif /* !IPRT_IN_BUILD_TOOL */

/*
 * The 'make-tainfo' command.
 */
static RTEXITCODE HelpMakeTaInfo(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel)
{
    RTStrmPrintf(pStrm,
                 "make-tainfo [--verbose|--quiet] [--cert <cert.der>]  [-o|--output] <tainfo.der>\n");
    return RTEXITCODE_SUCCESS;
}


typedef struct MAKETAINFOSTATE
{
    int         cVerbose;
    const char *pszCert;
    const char *pszOutput;
} MAKETAINFOSTATE;


/** @callback_method_impl{FNRTASN1ENCODEWRITER}  */
static DECLCALLBACK(int) handleMakeTaInfoWriter(const void *pvBuf, size_t cbToWrite, void *pvUser, PRTERRINFO pErrInfo)
{
    return RTStrmWrite((PRTSTREAM)pvUser, pvBuf, cbToWrite);
}


static RTEXITCODE HandleMakeTaInfo(int cArgs, char **papszArgs)
{
    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--cert",         'c', RTGETOPT_REQ_STRING },
        { "--output",       'o', RTGETOPT_REQ_STRING },
        { "--verbose",      'v', RTGETOPT_REQ_NOTHING },
        { "--quiet",        'q', RTGETOPT_REQ_NOTHING },
    };

    RTLDRARCH       enmLdrArch = RTLDRARCH_WHATEVER;
    MAKETAINFOSTATE State = { 0, NULL, NULL };

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    RTGETOPTUNION ValueUnion;
    int ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            case 'c':
                if (State.pszCert)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "The --cert option can only be used once.");
                State.pszCert = ValueUnion.psz;
                break;

            case 'o':
            case VINF_GETOPT_NOT_OPTION:
                if (State.pszOutput)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Multiple output files specified.");
                State.pszOutput = ValueUnion.psz;
                break;

            case 'v': State.cVerbose++; break;
            case 'q': State.cVerbose = 0; break;
            case 'V': return HandleVersion(cArgs, papszArgs);
            case 'h': return HelpMakeTaInfo(g_pStdOut, RTSIGNTOOLHELP_FULL);
            default:  return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (!State.pszCert)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No input certificate was specified.");
    if (!State.pszOutput)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No output file was specified.");

    /*
     * Read the certificate.
     */
    RTERRINFOSTATIC         StaticErrInfo;
    RTCRX509CERTIFICATE     Certificate;
    rc = RTCrX509Certificate_ReadFromFile(&Certificate, State.pszCert, 0, &g_RTAsn1DefaultAllocator,
                                          RTErrInfoInitStatic(&StaticErrInfo));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading certificate from %s: %Rrc - %s",
                              State.pszCert, rc, StaticErrInfo.szMsg);
    /*
     * Construct the trust anchor information.
     */
    RTCRTAFTRUSTANCHORINFO TrustAnchor;
    rc = RTCrTafTrustAnchorInfo_Init(&TrustAnchor, &g_RTAsn1DefaultAllocator);
    if (RT_SUCCESS(rc))
    {
        /* Public key. */
        Assert(RTCrX509SubjectPublicKeyInfo_IsPresent(&TrustAnchor.PubKey));
        RTCrX509SubjectPublicKeyInfo_Delete(&TrustAnchor.PubKey);
        rc = RTCrX509SubjectPublicKeyInfo_Clone(&TrustAnchor.PubKey, &Certificate.TbsCertificate.SubjectPublicKeyInfo,
                                                &g_RTAsn1DefaultAllocator);
        if (RT_FAILURE(rc))
            RTMsgError("RTCrX509SubjectPublicKeyInfo_Clone failed: %Rrc", rc);
        RTAsn1Core_ResetImplict(RTCrX509SubjectPublicKeyInfo_GetAsn1Core(&TrustAnchor.PubKey)); /* temporary hack. */

        /* Key Identifier. */
        PCRTASN1OCTETSTRING pKeyIdentifier = NULL;
        if (Certificate.TbsCertificate.T3.fFlags & RTCRX509TBSCERTIFICATE_F_PRESENT_SUBJECT_KEY_IDENTIFIER)
            pKeyIdentifier = Certificate.TbsCertificate.T3.pSubjectKeyIdentifier;
        else if (   (Certificate.TbsCertificate.T3.fFlags & RTCRX509TBSCERTIFICATE_F_PRESENT_AUTHORITY_KEY_IDENTIFIER)
                 && RTCrX509Certificate_IsSelfSigned(&Certificate)
                 && RTAsn1OctetString_IsPresent(&Certificate.TbsCertificate.T3.pAuthorityKeyIdentifier->KeyIdentifier) )
            pKeyIdentifier = &Certificate.TbsCertificate.T3.pAuthorityKeyIdentifier->KeyIdentifier;
        else if (   (Certificate.TbsCertificate.T3.fFlags & RTCRX509TBSCERTIFICATE_F_PRESENT_OLD_AUTHORITY_KEY_IDENTIFIER)
                 && RTCrX509Certificate_IsSelfSigned(&Certificate)
                 && RTAsn1OctetString_IsPresent(&Certificate.TbsCertificate.T3.pOldAuthorityKeyIdentifier->KeyIdentifier) )
            pKeyIdentifier = &Certificate.TbsCertificate.T3.pOldAuthorityKeyIdentifier->KeyIdentifier;
        if (pKeyIdentifier && pKeyIdentifier->Asn1Core.cb > 0)
        {
            Assert(RTAsn1OctetString_IsPresent(&TrustAnchor.KeyIdentifier));
            RTAsn1OctetString_Delete(&TrustAnchor.KeyIdentifier);
            rc = RTAsn1OctetString_Clone(&TrustAnchor.KeyIdentifier, pKeyIdentifier, &g_RTAsn1DefaultAllocator);
            if (RT_FAILURE(rc))
                RTMsgError("RTAsn1OctetString_Clone failed: %Rrc", rc);
            RTAsn1Core_ResetImplict(RTAsn1OctetString_GetAsn1Core(&TrustAnchor.KeyIdentifier)); /* temporary hack. */
        }
        else
            RTMsgWarning("No key identifier found or has zero length.");

        /* Subject */
        if (RT_SUCCESS(rc))
        {
            Assert(!RTCrTafCertPathControls_IsPresent(&TrustAnchor.CertPath));
            rc = RTCrTafCertPathControls_Init(&TrustAnchor.CertPath, &g_RTAsn1DefaultAllocator);
            if (RT_SUCCESS(rc))
            {
                Assert(RTCrX509Name_IsPresent(&TrustAnchor.CertPath.TaName));
                RTCrX509Name_Delete(&TrustAnchor.CertPath.TaName);
                rc = RTCrX509Name_Clone(&TrustAnchor.CertPath.TaName, &Certificate.TbsCertificate.Subject,
                                        &g_RTAsn1DefaultAllocator);
                if (RT_SUCCESS(rc))
                {
                    RTAsn1Core_ResetImplict(RTCrX509Name_GetAsn1Core(&TrustAnchor.CertPath.TaName)); /* temporary hack. */
                    rc = RTCrX509Name_RecodeAsUtf8(&TrustAnchor.CertPath.TaName, &g_RTAsn1DefaultAllocator);
                    if (RT_FAILURE(rc))
                        RTMsgError("RTCrX509Name_RecodeAsUtf8 failed: %Rrc", rc);
                }
                else
                    RTMsgError("RTCrX509Name_Clone failed: %Rrc", rc);
            }
            else
                RTMsgError("RTCrTafCertPathControls_Init failed: %Rrc", rc);
        }

        /* Check that what we've constructed makes some sense. */
        if (RT_SUCCESS(rc))
        {
            rc = RTCrTafTrustAnchorInfo_CheckSanity(&TrustAnchor, 0, RTErrInfoInitStatic(&StaticErrInfo), "TAI");
            if (RT_FAILURE(rc))
                RTMsgError("RTCrTafTrustAnchorInfo_CheckSanity failed: %Rrc - %s", rc, StaticErrInfo.szMsg);
        }

        if (RT_SUCCESS(rc))
        {
            /*
             * Encode it and write it to the output file.
             */
            uint32_t cbEncoded;
            rc = RTAsn1EncodePrepare(RTCrTafTrustAnchorInfo_GetAsn1Core(&TrustAnchor), RTASN1ENCODE_F_DER, &cbEncoded,
                                     RTErrInfoInitStatic(&StaticErrInfo));
            if (RT_SUCCESS(rc))
            {
                if (State.cVerbose >= 1)
                    RTAsn1Dump(RTCrTafTrustAnchorInfo_GetAsn1Core(&TrustAnchor), 0, 0, RTStrmDumpPrintfV, g_pStdOut);

                PRTSTREAM pStrm;
                rc = RTStrmOpen(State.pszOutput, "wb", &pStrm);
                if (RT_SUCCESS(rc))
                {
                    rc = RTAsn1EncodeWrite(RTCrTafTrustAnchorInfo_GetAsn1Core(&TrustAnchor), RTASN1ENCODE_F_DER,
                                           handleMakeTaInfoWriter, pStrm, RTErrInfoInitStatic(&StaticErrInfo));
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTStrmClose(pStrm);
                        if (RT_SUCCESS(rc))
                            RTMsgInfo("Successfully wrote TrustedAnchorInfo to '%s'.", State.pszOutput);
                        else
                            RTMsgError("RTStrmClose failed: %Rrc", rc);
                    }
                    else
                    {
                        RTMsgError("RTAsn1EncodeWrite failed: %Rrc - %s", rc, StaticErrInfo.szMsg);
                        RTStrmClose(pStrm);
                    }
                }
                else
                    RTMsgError("Error opening '%s' for writing: %Rrcs", State.pszOutput, rc);
            }
            else
                RTMsgError("RTAsn1EncodePrepare failed: %Rrc - %s", rc, StaticErrInfo.szMsg);
        }

        RTCrTafTrustAnchorInfo_Delete(&TrustAnchor);
    }
    else
        RTMsgError("RTCrTafTrustAnchorInfo_Init failed: %Rrc", rc);

    RTCrX509Certificate_Delete(&Certificate);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}



/*
 * The 'version' command.
 */
static RTEXITCODE HelpVersion(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel)
{
    RTStrmPrintf(pStrm, "version\n");
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE HandleVersion(int cArgs, char **papszArgs)
{
#ifndef IN_BLD_PROG  /* RTBldCfgVersion or RTBldCfgRevision in build time IPRT lib. */
    RTPrintf("%s\n", RTBldCfgVersion());
    return RTEXITCODE_SUCCESS;
#else
    return RTEXITCODE_FAILURE;
#endif
}



/**
 * Command mapping.
 */
static struct
{
    /** The command. */
    const char *pszCmd;
    /**
     * Handle the command.
     * @returns Program exit code.
     * @param   cArgs       Number of arguments.
     * @param   papszArgs   The argument vector, starting with the command name.
     */
    RTEXITCODE (*pfnHandler)(int cArgs, char **papszArgs);
    /**
     * Produce help.
     * @returns RTEXITCODE_SUCCESS to simplify handling '--help' in the handler.
     * @param   pStrm       Where to send help text.
     * @param   enmLevel    The level of the help information.
     */
    RTEXITCODE (*pfnHelp)(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel);
}
/** Mapping commands to handler and helper functions. */
const g_aCommands[] =
{
    { "extract-exe-signer-cert",        HandleExtractExeSignerCert,         HelpExtractExeSignerCert },
#ifndef IPRT_IN_BUILD_TOOL
    { "verify-exe",                     HandleVerifyExe,                    HelpVerifyExe },
#endif
    { "make-tainfo",                    HandleMakeTaInfo,                   HelpMakeTaInfo },
    { "help",                           HandleHelp,                         HelpHelp },
    { "--help",                         HandleHelp,                         NULL },
    { "-h",                             HandleHelp,                         NULL },
    { "version",                        HandleVersion,                      HelpVersion },
    { "--version",                      HandleVersion,                      NULL },
    { "-V",                             HandleVersion,                      NULL },
};


/*
 * The 'help' command.
 */
static RTEXITCODE HelpHelp(PRTSTREAM pStrm, RTSIGNTOOLHELP enmLevel)
{
    RTStrmPrintf(pStrm, "help [cmd-patterns]\n");
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE HandleHelp(int cArgs, char **papszArgs)
{
    RTSIGNTOOLHELP  enmLevel = cArgs <= 1 ? RTSIGNTOOLHELP_USAGE : RTSIGNTOOLHELP_FULL;
    uint32_t        cShowed  = 0;
    for (uint32_t iCmd = 0; iCmd < RT_ELEMENTS(g_aCommands); iCmd++)
    {
        if (g_aCommands[iCmd].pfnHelp)
        {
            bool fShow;
            if (cArgs <= 1)
                fShow = true;
            else
            {
                for (int iArg = 1; iArg < cArgs; iArg++)
                    if (RTStrSimplePatternMultiMatch(papszArgs[iArg], RTSTR_MAX, g_aCommands[iCmd].pszCmd, RTSTR_MAX, NULL))
                    {
                        fShow = true;
                        break;
                    }
            }
            if (fShow)
            {
                g_aCommands[iCmd].pfnHelp(g_pStdOut, enmLevel);
                cShowed++;
            }
        }
    }
    return cShowed ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}



int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse global arguments.
     */
    int iArg = 1;
    /* none presently. */

    /*
     * Command dispatcher.
     */
    if (iArg < argc)
    {
        const char *pszCmd = argv[iArg];
        uint32_t i = RT_ELEMENTS(g_aCommands);
        while (i-- > 0)
            if (!strcmp(g_aCommands[i].pszCmd, pszCmd))
                return g_aCommands[i].pfnHandler(argc - iArg, &argv[iArg]);
        RTMsgError("Unknown command '%s'.", pszCmd);
    }
    else
        RTMsgError("No command given. (try --help)");

    return RTEXITCODE_SYNTAX;
}


