/* $Id$ */
/** @file
 * VBox disassembler - Testcase for ARMv8 A64
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/dis.h>
#include <iprt/test.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/script.h>
#include <iprt/stream.h>

#include "tstDisasmArmv8-1-tests.h"

typedef struct TESTRDR
{
    const char *pb;
    unsigned   cb;
} TESTRDR;
typedef TESTRDR *PTESTRDR;

DECLASM(int) TestProcA64(void);
DECLASM(int) TestProcA64_EndProc(void);


static DECLCALLBACK(int) rtScriptLexParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser)
{
    RT_NOREF(ch, pvUser);
    return RTScriptLexScanNumber(hScriptLex, 0 /*uBase*/, false /*fAllowReal*/, pToken);
}


static const char *s_aszSingleStart[] =
{
    ";",
    NULL
};


static const RTSCRIPTLEXTOKMATCH s_aMatches[] =
{
    /* Begin of stuff which will get ignored in the semantic matching. */
    { RT_STR_TUPLE(".private_extern"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("_testproca64"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("_testproca64_endproc"),     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE(":"),                        RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    /* End of stuff which will get ignored in the semantic matching. */

    { RT_STR_TUPLE(","),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("."),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("["),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("]"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { NULL, 0,                                  RTSCRIPTLEXTOKTYPE_INVALID,    false, 0 }
};


static const RTSCRIPTLEXRULE s_aRules[] =
{
    { '#',  '#',   RTSCRIPT_LEX_RULE_CONSUME, rtScriptLexParseNumber,             NULL},
    { '0',  '9',   RTSCRIPT_LEX_RULE_CONSUME, rtScriptLexParseNumber,             NULL},
    { 'a',  'z',   RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanIdentifier,          NULL},
    { 'A',  'Z',   RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanIdentifier,          NULL},
    { '_',  '_',   RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanIdentifier,          NULL},
    { '\0', '\0',  RTSCRIPT_LEX_RULE_DEFAULT, NULL,                               NULL}
};


static const RTSCRIPTLEXCFG s_LexCfg =
{
    /** pszName */
    "ARMv8Dis",
    /** pszDesc */
    "ARMv8 disassembler lexer",
    /** fFlags */
    RTSCRIPT_LEX_CFG_F_CASE_INSENSITIVE,
    /** pszWhitespace */
    NULL,
    /** pszNewline */
    NULL,
    /** papszCommentMultiStart */
    NULL,
    /** papszCommentMultiEnd */
    NULL,
    /** papszCommentSingleStart */
    s_aszSingleStart,
    /** paTokMatches */
    s_aMatches,
    /** paRules */
    s_aRules,
    /** pfnProdDef */
    NULL,
    /** pfnProdDefUser */
    NULL
};


static DECLCALLBACK(int) testDisasmLexerRead(RTSCRIPTLEX hScriptLex, size_t offBuf, char *pchCur,
                                             size_t cchBuf, size_t *pcchRead, void *pvUser)
{
    RT_NOREF(hScriptLex);

    PTESTRDR pRdr = (PTESTRDR)pvUser;
    size_t cbCopy = RT_MIN(cchBuf / sizeof(char), pRdr->cb - offBuf);
    int rc = VINF_SUCCESS;

    *pcchRead = cbCopy * sizeof(char);

    if (cbCopy)
        memcpy(pchCur, &pRdr->pb[offBuf], cbCopy);
    else
        rc = VINF_EOF;

    return rc;
}


static void testDisas(const char *pszSub, uint8_t const *pabInstrs, uintptr_t uEndPtr, DISCPUMODE enmDisCpuMode,
                      const unsigned char *pbSrc, unsigned cbSrc)
{
    RTTestISub(pszSub);

    RTSCRIPTLEX hLexSource = NULL;
    TESTRDR Rdr;

    Rdr.pb = (const char *)pbSrc;
    Rdr.cb = cbSrc;
    int rc = RTScriptLexCreateFromReader(&hLexSource, testDisasmLexerRead, 
                                         NULL /*pfnDtor*/, &Rdr /*pvUser*/, cbSrc,
                                         NULL /*phStrCacheId*/, NULL /*phStrCacheStringLit*/,
                                         &s_LexCfg);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return; /* Can't do our work if this fails. */

    PCRTSCRIPTLEXTOKEN pTokSource;
    rc = RTScriptLexQueryToken(hLexSource, &pTokSource);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

    size_t const cbInstrs = uEndPtr - (uintptr_t)pabInstrs;
    for (size_t off = 0; off < cbInstrs;)
    {
        DISSTATE        Dis;
        uint32_t        cb = 1;
#ifndef DIS_CORE_ONLY
        uint32_t const  cErrBefore = RTTestIErrorCount();
        char            szOutput[256] = {0};

        /*
         * Can't use DISInstrToStr() here as it would add addresses and opcode bytes
         * which would trip the semantic matching later on.
         */
        rc = DISInstrEx((uintptr_t)&pabInstrs[off], enmDisCpuMode, DISOPTYPE_ALL,
                        NULL /*pfnReadBytes*/, NULL /*pvUser*/, &Dis, &cb);
        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        RTTESTI_CHECK(cb == Dis.cbInstr);
        RTTESTI_CHECK(cb == sizeof(uint32_t));

        if (RT_SUCCESS(rc))
        {
            size_t cch = 0;

            switch (enmDisCpuMode)
            {
                case DISCPUMODE_ARMV8_A64:
                case DISCPUMODE_ARMV8_A32:
                case DISCPUMODE_ARMV8_T32:
                    cch = DISFormatArmV8Ex(&Dis, &szOutput[0], sizeof(szOutput),
                                           DIS_FMT_FLAGS_RELATIVE_BRANCH,
                                           NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
                    break;
                default:
                    AssertReleaseFailed(); /* Testcase error. */
                    break;
            }

            szOutput[cch] = '\0';
            RTStrStripR(szOutput);
            RTTESTI_CHECK(szOutput[0]);
            if (szOutput[0])
            {
                /* Build the lexer and compare that it semantically is equal to the source input. */
                RTSCRIPTLEX hLexDis = NULL;
                rc = RTScriptLexCreateFromString(&hLexDis, szOutput, NULL /*phStrCacheId*/,
                                                 NULL /*phStrCacheStringLit*/, &s_LexCfg);
                RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
                if (RT_SUCCESS(rc))
                {
                    PCRTSCRIPTLEXTOKEN pTokDis;
                    rc = RTScriptLexQueryToken(hLexDis, &pTokDis);
                    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                    /*
                     * Skip over any keyword tokens in the source lexer because these
                     * are not part of the disassembly.
                     */
                    while (pTokSource->enmType == RTSCRIPTLEXTOKTYPE_KEYWORD)
                        pTokSource = RTScriptLexConsumeToken(hLexSource);

                    /* Now compare the token streams until we hit EOS in the disassembly lexer. */
                    do
                    {
                        RTTESTI_CHECK(pTokSource->enmType == pTokDis->enmType);
                        if (pTokSource->enmType == pTokDis->enmType)
                        {
                            switch (pTokSource->enmType)
                            {
                                case RTSCRIPTLEXTOKTYPE_IDENTIFIER:
                                {
                                    int iCmp = strcmp(pTokSource->Type.Id.pszIde, pTokDis->Type.Id.pszIde);
                                    RTTESTI_CHECK(!iCmp);
                                    if (iCmp)
                                        RTTestIFailureDetails("<IDE{%u.%u, %u.%u}, %s != %s>\n",
                                                              pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                              pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                              pTokSource->Type.Id.pszIde, pTokDis->Type.Id.pszIde);
                                    break;
                                }
                                case RTSCRIPTLEXTOKTYPE_NUMBER:
                                    RTTESTI_CHECK(pTokSource->Type.Number.enmType == pTokDis->Type.Number.enmType);
                                    if (pTokSource->Type.Number.enmType == pTokDis->Type.Number.enmType)
                                    {
                                        switch (pTokSource->Type.Number.enmType)
                                        {
                                            case RTSCRIPTLEXTOKNUMTYPE_NATURAL:
                                            {
                                                RTTESTI_CHECK(pTokSource->Type.Number.Type.u64 == pTokDis->Type.Number.Type.u64);
                                                if (pTokSource->Type.Number.Type.u64 != pTokDis->Type.Number.Type.u64)
                                                    RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %RU64 != %RU64>\n",
                                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                                          pTokSource->Type.Number.Type.u64, pTokDis->Type.Number.Type.u64);
                                                break;
                                            }
                                            case RTSCRIPTLEXTOKNUMTYPE_INTEGER:
                                            {
                                                RTTESTI_CHECK(pTokSource->Type.Number.Type.i64 == pTokDis->Type.Number.Type.i64);
                                                if (pTokSource->Type.Number.Type.i64 != pTokDis->Type.Number.Type.i64)
                                                    RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %RI64 != %RI64>\n",
                                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                                          pTokSource->Type.Number.Type.i64, pTokDis->Type.Number.Type.i64);
                                                break;
                                            }
                                        case RTSCRIPTLEXTOKNUMTYPE_REAL:
                                            default:
                                                AssertReleaseFailed();
                                        }
                                    }
                                    else
                                        RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %u != %u>\n",
                                                              pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                              pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                              pTokSource->Type.Number.enmType, pTokDis->Type.Number.enmType);
                                    break;
                                case RTSCRIPTLEXTOKTYPE_PUNCTUATOR:
                                {
                                    int iCmp = strcmp(pTokSource->Type.Punctuator.pPunctuator->pszMatch,
                                                      pTokDis->Type.Punctuator.pPunctuator->pszMatch);
                                    RTTESTI_CHECK(!iCmp);
                                    if (iCmp)
                                        RTTestIFailureDetails("<PUNCTUATOR{%u.%u, %u.%u}, %s != %s>\n",
                                                              pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                              pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                              pTokSource->Type.Punctuator.pPunctuator->pszMatch,
                                                              pTokDis->Type.Punctuator.pPunctuator->pszMatch);
                                    break;
                                }

                                /* These should never occur and indicate an issue in the lexer. */
                                case RTSCRIPTLEXTOKTYPE_KEYWORD:
                                    RTTestIFailureDetails("<KEYWORD{%u.%u, %u.%u}, %s>\n",
                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                          pTokSource->Type.Keyword.pKeyword->pszMatch);
                                    break;
                                case RTSCRIPTLEXTOKTYPE_STRINGLIT:
                                    RTTestIFailureDetails("<STRINGLIT{%u.%u, %u.%u}, %s>\n",
                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                          pTokSource->Type.StringLit.pszString);
                                    break;
                                case RTSCRIPTLEXTOKTYPE_OPERATOR:
                                    RTTestIFailureDetails("<OPERATOR{%u.%u, %u.%u}, %s>\n",
                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                          pTokSource->Type.Operator.pOp->pszMatch);
                                    break;
                                case RTSCRIPTLEXTOKTYPE_INVALID:
                                    RTTestIFailureDetails("<INVALID>\n");
                                    break;
                                case RTSCRIPTLEXTOKTYPE_ERROR:
                                    RTTestIFailureDetails("<ERROR{%u.%u, %u.%u}> %s\n",
                                                          pTokSource->PosStart.iLine, pTokSource->PosStart.iCh,
                                                          pTokSource->PosEnd.iLine, pTokSource->PosEnd.iCh,
                                                          pTokSource->Type.Error.pErr->pszMsg);
                                    break;
                                case RTSCRIPTLEXTOKTYPE_EOS:
                                    RTTestIFailureDetails("<EOS>\n");
                                    break;
                                default:
                                    AssertFailed();
                            }
                        }
                        else
                            RTTestIFailureDetails("pTokSource->enmType=%u pTokDis->enmType=%u\n",
                                                  pTokSource->enmType, pTokDis->enmType);

                        /*
                         * Abort on error as the streams are now out of sync and matching will not work
                         * anymore producing lots of noise.
                         */
                        if (cErrBefore != RTTestIErrorCount())
                            break;

                        /* Advance to the next token. */
                        pTokDis = RTScriptLexConsumeToken(hLexDis);
                        Assert(pTokDis);

                        pTokSource = RTScriptLexConsumeToken(hLexSource);
                        Assert(pTokSource);
                    } while (pTokDis->enmType != RTSCRIPTLEXTOKTYPE_EOS);

                    RTScriptLexDestroy(hLexDis);
                }
            }
            if (cErrBefore != RTTestIErrorCount())
            {
                RTTestIFailureDetails("rc=%Rrc, off=%#x (%u) cbInstr=%u enmDisCpuMode=%d\n",
                                      rc, off, off, Dis.cbInstr, enmDisCpuMode);
                RTTestIPrintf(RTTESTLVL_ALWAYS, "%s\n", szOutput);
                break;
            }

            /* Do the output formatting again, now with all the addresses and opcode bytes. */
            DISFormatArmV8Ex(&Dis, szOutput, sizeof(szOutput),
                               DIS_FMT_FLAGS_BYTES_LEFT | DIS_FMT_FLAGS_BYTES_BRACKETS | DIS_FMT_FLAGS_BYTES_SPACED
                             | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_ADDR_LEFT,
                             NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
            RTStrStripR(szOutput);
            RTTESTI_CHECK(szOutput[0]);
            RTTestIPrintf(RTTESTLVL_ALWAYS, "%s\n", szOutput);
        }

        /* Check with size-only. */
        uint32_t        cbOnly = 1;
        DISSTATE        DisOnly;
        rc = DISInstrWithPrefetchedBytes((uintptr_t)&pabInstrs[off], enmDisCpuMode,  0 /*fFilter - none */,
                                         Dis.Instr.ab, Dis.cbCachedInstr, NULL, NULL, &DisOnly, &cbOnly);

        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        RTTESTI_CHECK(cbOnly == DisOnly.cbInstr);
        RTTESTI_CHECK_MSG(cbOnly == cb, ("%#x vs %#x\n", cbOnly, cb));

#else  /* DIS_CORE_ONLY */
        rc = DISInstr(&pabInstrs[off], enmDisCpuMode,  &Dis, &cb);
        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        RTTESTI_CHECK(cb == Dis.cbInstr);
#endif /* DIS_CORE_ONLY */

        off += cb;
    }

    RTScriptLexDestroy(hLexSource);
}


int main(int argc, char **argv)
{
    RT_NOREF2(argc, argv);
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstDisasm", &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);

    static const struct
    {
        const char              *pszDesc;
        uint8_t const           *pbStart;
        uintptr_t               uEndPtr;
        DISCPUMODE              enmCpuMode;
        const unsigned char     *pbSrc;
        unsigned                cbSrc;
    } aSnippets[] =
    {
#ifndef RT_OS_OS2
        { "64-bit",     (uint8_t const *)(uintptr_t)TestProcA64, (uintptr_t)&TestProcA64_EndProc, DISCPUMODE_ARMV8_A64,
          g_abtstDisasmArmv8_1, g_cbtstDisasmArmv8_1 },
#endif
    };

    for (unsigned i = 0; i < RT_ELEMENTS(aSnippets); i++)
        testDisas(aSnippets[i].pszDesc, aSnippets[i].pbStart, aSnippets[i].uEndPtr, aSnippets[i].enmCpuMode,
                  aSnippets[i].pbSrc, aSnippets[i].cbSrc);

    return RTTestSummaryAndDestroy(hTest);
}

