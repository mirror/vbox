/* $Id$ */
/** @file
 * RTTraceLogDecoders - Implement decoders for the tracing driver.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/log.h>
#include <iprt/tracelog-decoder-plugin.h>

#include <iprt/formats/tpm.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Algorithm ID to string mapping.
 */
typedef struct TPMALGID2STR
{
    uint16_t    u16AlgId;
    const char *pszAlgId;
} TPMALGID2STR;
typedef const TPMALGID2STR *PCTPMALGID2STR;


/**
 * The TPM state.
 */
typedef struct TPMSTATE
{
    /** Command code. */
    uint32_t                            u32CmdCode;
    /** Command code dependent state. */
    union
    {
        /** TPM2_CC_GET_CAPABILITY related state. */
        struct
        {
            /** The capability group to query. */
            uint32_t                    u32Cap;
            /** Property to query. */
            uint32_t                    u32Property;
            /** Number of values to return. */
            uint32_t                    u32Count;
        } GetCapability;
        /** TPM2_GET_RANDOM related state. */
        struct
        {
            /** Number of bytes of random data to produce. */
            uint16_t                    cbRnd;
        } GetRandom;
    } u;
} TPMSTATE;
typedef TPMSTATE *PTPMSTATE;


/**
 */
typedef DECLCALLBACKTYPE(void, FNDECODETPM2CCREQ, (PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMREQHDR pHdr, size_t cb));
/** Pointer to an event decode request callback. */
typedef FNDECODETPM2CCREQ *PFNFNDECODETPM2CCREQ;


/**
 */
typedef DECLCALLBACKTYPE(void, FNDECODETPM2CCRESP, (PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMRESPHDR pHdr, size_t cb));
/** Pointer to an event decode request callback. */
typedef FNDECODETPM2CCRESP *PFNFNDECODETPM2CCRESP;


/*********************************************************************************************************************************
*   Static Variables                                                                                                             *
*********************************************************************************************************************************/

/**
 * Algorithm ID to string mapping array.
 */
static const TPMALGID2STR g_aAlgId2Str[] =
{
#define TPM_ALGID_2_STR(a_AlgId) { a_AlgId, #a_AlgId }
    TPM_ALGID_2_STR(TPM2_ALG_ERROR),
    TPM_ALGID_2_STR(TPM2_ALG_RSA),
    TPM_ALGID_2_STR(TPM2_ALG_TDES),
    TPM_ALGID_2_STR(TPM2_ALG_SHA1),
    TPM_ALGID_2_STR(TPM2_ALG_HMAC),
    TPM_ALGID_2_STR(TPM2_ALG_AES),
    TPM_ALGID_2_STR(TPM2_ALG_MGF1),
    TPM_ALGID_2_STR(TPM2_ALG_KEYEDHASH),
    TPM_ALGID_2_STR(TPM2_ALG_XOR),
    TPM_ALGID_2_STR(TPM2_ALG_SHA256),
    TPM_ALGID_2_STR(TPM2_ALG_SHA384),
    TPM_ALGID_2_STR(TPM2_ALG_SHA512),
    TPM_ALGID_2_STR(TPM2_ALG_SHA256_192),
    TPM_ALGID_2_STR(TPM2_ALG_NULL),
    TPM_ALGID_2_STR(TPM2_ALG_SM3_256),
    TPM_ALGID_2_STR(TPM2_ALG_SM4),
    TPM_ALGID_2_STR(TPM2_ALG_RSASSA),
    TPM_ALGID_2_STR(TPM2_ALG_RSAES),
    TPM_ALGID_2_STR(TPM2_ALG_RSAPSS),
    TPM_ALGID_2_STR(TPM2_ALG_OAEP),
    TPM_ALGID_2_STR(TPM2_ALG_ECDSA),
    TPM_ALGID_2_STR(TPM2_ALG_ECDH),
    TPM_ALGID_2_STR(TPM2_ALG_ECDAA),
    TPM_ALGID_2_STR(TPM2_ALG_SM2),
    TPM_ALGID_2_STR(TPM2_ALG_ECSCHNORR),
    TPM_ALGID_2_STR(TPM2_ALG_ECMQV),
    TPM_ALGID_2_STR(TPM2_ALG_KDF1_SP800_56A),
    TPM_ALGID_2_STR(TPM2_ALG_KDF2),
    TPM_ALGID_2_STR(TPM2_ALG_KDF1_SP800_108),
    TPM_ALGID_2_STR(TPM2_ALG_ECC),
    TPM_ALGID_2_STR(TPM2_ALG_SYMCIPHER),
    TPM_ALGID_2_STR(TPM2_ALG_CAMELLIA),
    TPM_ALGID_2_STR(TPM2_ALG_SHA3_256),
    TPM_ALGID_2_STR(TPM2_ALG_SHA3_384),
    TPM_ALGID_2_STR(TPM2_ALG_SHA3_512),
    TPM_ALGID_2_STR(TPM2_ALG_SHAKE128),
    TPM_ALGID_2_STR(TPM2_ALG_SHAKE256),
    TPM_ALGID_2_STR(TPM2_ALG_SHAKE256_192),
    TPM_ALGID_2_STR(TPM2_ALG_SHAKE256_256),
    TPM_ALGID_2_STR(TPM2_ALG_SHAKE256_512),
    TPM_ALGID_2_STR(TPM2_ALG_CMAC),
    TPM_ALGID_2_STR(TPM2_ALG_CTR),
    TPM_ALGID_2_STR(TPM2_ALG_OFB),
    TPM_ALGID_2_STR(TPM2_ALG_CBC),
    TPM_ALGID_2_STR(TPM2_ALG_CFB),
    TPM_ALGID_2_STR(TPM2_ALG_ECB),
    TPM_ALGID_2_STR(TPM2_ALG_CCM),
    TPM_ALGID_2_STR(TPM2_ALG_GCM),
    TPM_ALGID_2_STR(TPM2_ALG_KW),
    TPM_ALGID_2_STR(TPM2_ALG_KWP),
    TPM_ALGID_2_STR(TPM2_ALG_EAX),
    TPM_ALGID_2_STR(TPM2_ALG_EDDSA),
    TPM_ALGID_2_STR(TPM2_ALG_EDDSA_PH),
    TPM_ALGID_2_STR(TPM2_ALG_LMS),
    TPM_ALGID_2_STR(TPM2_ALG_XMSS),
    TPM_ALGID_2_STR(TPM2_ALG_KEYEDXOF),
    TPM_ALGID_2_STR(TPM2_ALG_KMACXOF128),
    TPM_ALGID_2_STR(TPM2_ALG_KMACXOF256),
    TPM_ALGID_2_STR(TPM2_ALG_KMAC128),
    TPM_ALGID_2_STR(TPM2_ALG_KMAC256)
#undef TPM_ALGID_2_STR
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static const char *vboxTraceLogDecodeEvtTpmAlgId2Str(uint16_t u16AlgId)
{
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aAlgId2Str); i++)
        if (g_aAlgId2Str[i].u16AlgId == u16AlgId)
            return g_aAlgId2Str[i].pszAlgId;

    return "<UNKNOWN>";
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeStartupShutdownReq(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMREQHDR pHdr, size_t cb)
{
    RT_NOREF(pThis, cb);

    uint16_t u16TpmSu = RT_BE2H_U16(*(const uint16_t *)(pHdr + 1));
    if (u16TpmSu == TPM2_SU_CLEAR)
        pHlp->pfnPrintf(pHlp, "        TPM2_SU_CLEAR\n");
    else if (u16TpmSu == TPM2_SU_STATE)
        pHlp->pfnPrintf(pHlp, "        TPM2_SU_STATE\n");
    else
        pHlp->pfnPrintf(pHlp, "        Unknown: %#x\n", u16TpmSu);
}


static struct
{
    const char     *pszCap;
    const uint32_t *paProperties;
} s_aTpm2Caps[] =
{
    { RT_STR(TPM2_CAP_ALGS),            NULL },
    { RT_STR(TPM2_CAP_HANDLES),         NULL },
    { RT_STR(TPM2_CAP_COMMANDS),        NULL },
    { RT_STR(TPM2_CAP_PP_COMMANDS),     NULL },
    { RT_STR(TPM2_CAP_AUDIT_COMMANDS),  NULL },
    { RT_STR(TPM2_CAP_PCRS),            NULL },
    { RT_STR(TPM2_CAP_TPM_PROPERTIES),  NULL },
    { RT_STR(TPM2_CAP_PCR_PROPERTIES),  NULL },
    { RT_STR(TPM2_CAP_ECC_CURVES),      NULL },
    { RT_STR(TPM2_CAP_AUTH_POLICIES),   NULL },
    { RT_STR(TPM2_CAP_ACT),             NULL },
};

static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeGetCapabilityReq(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMREQHDR pHdr, size_t cb)
{
    RT_NOREF(cb);

    PCTPM2REQGETCAPABILITY pReq = (PCTPM2REQGETCAPABILITY)pHdr;
    uint32_t u32Cap      = RT_BE2H_U32(pReq->u32Cap);
    uint32_t u32Property = RT_BE2H_U32(pReq->u32Property);
    uint32_t u32Count    = RT_BE2H_U32(pReq->u32Count);

    pThis->u.GetCapability.u32Cap      = u32Cap;
    pThis->u.GetCapability.u32Property = u32Property;
    pThis->u.GetCapability.u32Count    = u32Count;

    if (u32Cap < RT_ELEMENTS(s_aTpm2Caps))
        pHlp->pfnPrintf(pHlp, "        u32Cap:      %s\n"
                              "        u32Property: %#x\n"
                              "        u32Count:    %#x\n",
                        s_aTpm2Caps[u32Cap], u32Property, u32Count);
    else
        pHlp->pfnPrintf(pHlp, "        u32Cap:      %#x (UNKNOWN)\n"
                              "        u32Property: %#x\n"
                              "        u32Count:    %#x\n",
                        u32Cap, u32Property, u32Count);
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeGetCapabilityResp(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMRESPHDR pHdr, size_t cb)
{
    RT_NOREF(pThis, cb);

    PCTPM2RESPGETCAPABILITY pResp  = (PCTPM2RESPGETCAPABILITY)pHdr;
    TPMCAP                  u32Cap = RT_BE2H_U32(pResp->u32Cap);
    pHlp->pfnPrintf(pHlp, "        fMoreData: %RTbool\n", pResp->fMoreData);
    if (u32Cap < RT_ELEMENTS(s_aTpm2Caps))
    {
        pHlp->pfnPrintf(pHlp, "        u32Cap:    %s\n", s_aTpm2Caps[u32Cap]);
        switch (u32Cap)
        {
            case TPM2_CAP_PCRS:
            {
                uint32_t u32Count = RT_BE2H_U32(*(uint32_t *)&pResp->abCap[0]);
                pHlp->pfnPrintf(pHlp, "        u32Count:  %u\n", u32Count);

                /* Walk the list of PCR selection entries. */
                const uint8_t *pbPcrSelection = &pResp->abCap[sizeof(u32Count)];
                for (uint32_t i = 0; i < u32Count; i++)
                {
                    uint16_t u16AlgId       = RT_BE2H_U16(*(const uint16_t *)pbPcrSelection);
                    uint8_t  cbPcrSelection = *(pbPcrSelection + 2);
                    const char *pszAlgId = vboxTraceLogDecodeEvtTpmAlgId2Str(u16AlgId);

                    pbPcrSelection += 3;
                    pHlp->pfnPrintf(pHlp, "            u16AlgId:      %s\n", pszAlgId);
                    pHlp->pfnPrintf(pHlp, "            u8SizeOfSlect: %u\n", cbPcrSelection);
                    pHlp->pfnPrintf(pHlp, "            PCRs:          ");
                    for (uint8_t idxPcr = 0; idxPcr < cbPcrSelection * 8; idxPcr++)
                        if (RT_BOOL(*(pbPcrSelection + (idxPcr / 8)) & RT_BIT(idxPcr % 8)))
                            pHlp->pfnPrintf(pHlp, "%u ", idxPcr);
                    pbPcrSelection += cbPcrSelection;
                    pHlp->pfnPrintf(pHlp, "\n");
                }
                break;
            }
            default:
                pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cb - sizeof(TPMYESNO) - sizeof(TPMCAP), &pResp->abCap[0]);
                break;
        }
    }
    else
    {
        pHlp->pfnPrintf(pHlp, "        u32Cap:    %#x (UNKNOWN)\n", u32Cap);
        pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cb - sizeof(TPMYESNO) - sizeof(TPMCAP), &pResp->abCap[0]);
    }
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeReadPublicReq(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMREQHDR pHdr, size_t cb)
{
    RT_NOREF(pThis, cb);

    PCTPM2REQREADPUBLIC pReq = (PCTPM2REQREADPUBLIC)pHdr;
    TPMIDHOBJECT hObj = RT_BE2H_U32(pReq->hObj);
    pHlp->pfnPrintf(pHlp, "        hObj:      %#x\n", hObj);
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeGetRandomReq(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMREQHDR pHdr, size_t cb)
{
    RT_NOREF(pThis, cb);

    PCTPM2REQGETRANDOM pReq = (PCTPM2REQGETRANDOM)pHdr;
    pThis->u.GetRandom.cbRnd = RT_BE2H_U16(pReq->u16RandomBytes);
    pHlp->pfnPrintf(pHlp, "        u16RandomBytes: %u\n", pThis->u.GetRandom.cbRnd);
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeGetRandomResp(PRTTRACELOGDECODERHLP pHlp, PTPMSTATE pThis, PCTPMRESPHDR pHdr, size_t cb)
{
    RT_NOREF(pHlp, pThis, cb);
    PCTPM2RESPGETRANDOM pResp = (PCTPM2RESPGETRANDOM)pHdr;
    uint16_t cbBuf = RT_BE2H_U16(pResp->Buf.u16Size);
    if (pThis->u.GetRandom.cbRnd != cbBuf)
    {
        pHlp->pfnErrorMsg(pHlp, "Requested random data size doesn't match returned data size (requested %u, returned %u), using smaller value\n",
                          pThis->u.GetRandom.cbRnd, cbBuf);
        cbBuf = RT_MIN(cbBuf, pThis->u.GetRandom.cbRnd);
    }

    /** @todo Validate against cb. */

    pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cbBuf, pResp->Buf.abBuf);
}


static struct
{
    uint32_t              u32CmdCode;
    const char            *pszCmdCode;
    uint32_t              cbReqMin;
    uint32_t              cbRespMin;
    PFNFNDECODETPM2CCREQ  pfnDecodeReq;
    PFNFNDECODETPM2CCRESP pfnDecodeResp;
} s_aTpmCmdCodes[] =
{
#define TPM_CMD_CODE_INIT_NOT_IMPL(a_CmdCode) { a_CmdCode, #a_CmdCode, 0, 0, NULL, NULL }
#define TPM_CMD_CODE_INIT(a_CmdCode, a_ReqStruct, a_RespStruct, a_pfnReq, a_pfnResp) { a_CmdCode, #a_CmdCode, sizeof(a_ReqStruct), \
                                                                                       sizeof(a_RespStruct), a_pfnReq, a_pfnResp }
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_UNDEFINE_SPACE_SPECIAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_EVICT_CONTROL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_HIERARCHY_CONTROL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_UNDEFINE_SPACE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CHANGE_EPS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CHANGE_PPS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CLEAR),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CLEAR_CONTROL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CLOCK_SET),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_HIERARCHY_CHANGE_AUTH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_DEFINE_SPACE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_ALLOCATE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_SET_AUTH_POLICY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PP_COMMANDS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SET_PRIMARY_POLICY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_FIELD_UPGRADE_START),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CLOCK_RATE_ADJUST),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CREATE_PRIMARY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_GLOBAL_WRITE_LOCK),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_GET_COMMAND_AUDIT_DIGEST),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_INCREMENT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_SET_BITS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_EXTEND),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_WRITE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_WRITE_LOCK),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_DICTIONARY_ATTACK_LOCK_RESET),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_DICTIONARY_ATTACK_PARAMETERS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_CHANGE_AUTH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_EVENT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_RESET),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SEQUENCE_COMPLETE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SET_ALGORITHM_SET),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SET_COMMAND_CODE_AUDIT_STATUS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_FIELD_UPGRADE_DATA),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_INCREMENTAL_SELF_TEST),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SELF_TEST),
    TPM_CMD_CODE_INIT(         TPM2_CC_STARTUP,                          TPMREQHDR,             TPMRESPHDR,             vboxTraceLogDecodeEvtTpmDecodeStartupShutdownReq,   NULL),
    TPM_CMD_CODE_INIT(         TPM2_CC_SHUTDOWN,                         TPMREQHDR,             TPMRESPHDR,             vboxTraceLogDecodeEvtTpmDecodeStartupShutdownReq,   NULL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_STIR_RANDOM),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ACTIVATE_CREDENTIAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CERTIFY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_NV),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CERTIFY_CREATION),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_DUPLICATE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_GET_TIME),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_GET_SESSION_AUDIT_DIGEST),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_READ),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_READ_LOCK),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_OBJECT_CHANGE_AUTH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_SECRET),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_REWRAP),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CREATE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ECDH_ZGEN),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_HMAC_MAC),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_IMPORT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_LOAD),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_QUOTE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_RSA_DECRYPT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_HMAC_MAC_START),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SEQUENCE_UPDATE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SIGN),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_UNSEAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_SIGNED),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CONTEXT_LOAD),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CONTEXT_SAVE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ECDH_KEY_GEN),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ENCRYPT_DECRYPT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_FLUSH_CONTEXT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_LOAD_EXTERNAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_MAKE_CREDENTIAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_READ_PUBLIC),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_AUTHORIZE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_AUTH_VALUE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_COMMAND_CODE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_COUNTER_TIMER),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_CP_HASH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_LOCALITY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_NAME_HASH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_OR),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_TICKET),
    TPM_CMD_CODE_INIT(         TPM2_CC_READ_PUBLIC,                      TPM2REQREADPUBLIC,     TPMRESPHDR,             vboxTraceLogDecodeEvtTpmDecodeReadPublicReq,        NULL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_RSA_ENCRYPT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_START_AUTH_SESSION),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_VERIFY_SIGNATURE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ECC_PARAMETERS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_FIRMWARE_READ),
    TPM_CMD_CODE_INIT(         TPM2_CC_GET_CAPABILITY,                   TPM2REQGETCAPABILITY,  TPM2RESPGETCAPABILITY,  vboxTraceLogDecodeEvtTpmDecodeGetCapabilityReq,     vboxTraceLogDecodeEvtTpmDecodeGetCapabilityResp),
    TPM_CMD_CODE_INIT(         TPM2_CC_GET_RANDOM,                       TPM2REQGETRANDOM,      TPM2RESPGETRANDOM,      vboxTraceLogDecodeEvtTpmDecodeGetRandomReq,         vboxTraceLogDecodeEvtTpmDecodeGetRandomResp),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_GET_TEST_RESULT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_GET_HASH),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_READ),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_PCR),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_RESTART),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_READ_CLOCK),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_EXTEND),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_PCR_SET_AUTH_VALUE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_CERTIFY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_EVENT_SEQUENCE_COMPLETE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_HASH_SEQUENCE_START),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_PHYSICAL_PRESENCE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_DUPLICATION_SELECT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_GET_DIGEST),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_TEST_PARMS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_COMMIT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_PASSWORD),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ZGEN_2PHASE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_EC_EPHEMERAL),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_NV_WRITTEN),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_TEMPLATE),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CREATE_LOADED),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_AUTHORIZE_NV),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ENCRYPT_DECRYPT_2),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_AC_GET_CAPABILITY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_AC_SEND),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_AC_SEND_SELECT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_CERTIFY_X509),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ACT_SET_TIMEOUT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ECC_ENCRYPT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_ECC_DECRYPT),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_CAPABILITY),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_POLICY_PARAMETERS),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_DEFINE_SPACE_2),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_NV_READ_PUBLIC_2),
    TPM_CMD_CODE_INIT_NOT_IMPL(TPM2_CC_SET_CAPABILITY)
#undef TPM_CMD_CODE_INIT
};

static void vboxTraceLogDecodeEvtTpmDecodeCmdBuffer(PRTTRACELOGDECODERHLP pHlp, const uint8_t *pbCmd, size_t cbCmd)
{
    PCTPMREQHDR pHdr = (PCTPMREQHDR)pbCmd;
    if (cbCmd >= sizeof(*pHdr))
    {
        uint32_t  u32CmdCode   = RT_BE2H_U32(pHdr->u32Ordinal);
        uint32_t  cbReqPayload = RT_BE2H_U32(pHdr->cbReq) - sizeof(*pHdr);
        PTPMSTATE pTpmState    = (PTPMSTATE)pHlp->pfnDecoderStateGet(pHlp);

        if (!pTpmState)
        {
            int rc = pHlp->pfnDecoderStateCreate(pHlp, sizeof(*pTpmState), NULL, (void **)&pTpmState);
            if (RT_SUCCESS(rc))
                pTpmState->u32CmdCode = u32CmdCode;
            else
                pHlp->pfnErrorMsg(pHlp, "Failed to allocate TPM decoder state: %Rrc\n", rc);
        }
        else
            pTpmState->u32CmdCode = u32CmdCode;

        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTpmCmdCodes); i++)
        {
            if (s_aTpmCmdCodes[i].u32CmdCode == u32CmdCode)
            {
                pHlp->pfnPrintf(pHlp, "    %s (%u bytes):\n", s_aTpmCmdCodes[i].pszCmdCode, cbReqPayload);
                if (s_aTpmCmdCodes[i].pfnDecodeReq)
                {
                    if (cbCmd >= s_aTpmCmdCodes[i].cbReqMin)
                        s_aTpmCmdCodes[i].pfnDecodeReq(pHlp, pTpmState, pHdr, cbReqPayload);
                    else
                        pHlp->pfnErrorMsg(pHlp, "Malformed %s command, not enough room for the input\n", s_aTpmCmdCodes[i].pszCmdCode);
                }
                else if (cbReqPayload)
                    pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cbReqPayload, pHdr + 1);
                return;
            }
        }
        pHlp->pfnPrintf(pHlp, "    <Unknown command code>: %#x\n", u32CmdCode);

        if (cbReqPayload)
            pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cbReqPayload, pHdr + 1);
    }
    else
        pHlp->pfnErrorMsg(pHlp, "Command buffer is smaller than the request header (required %u, given %zu\n", sizeof(*pHdr), cbCmd);
}


static void vboxTraceLogDecodeEvtTpmDecodeRespBuffer(PRTTRACELOGDECODERHLP pHlp, const uint8_t *pbResp, size_t cbResp)
{
    RT_NOREF(pHlp);

    PCTPMRESPHDR pHdr = (PCTPMRESPHDR)pbResp;
    if (cbResp >= sizeof(*pHdr))
    {
        uint32_t cbRespPayload = RT_BE2H_U32(pHdr->cbResp) - sizeof(*pHdr);

        pHlp->pfnPrintf(pHlp, "    Status code: %#x (%u bytes)\n", RT_BE2H_U32(pHdr->u32ErrCode), cbRespPayload);
        PTPMSTATE pTpmState = (PTPMSTATE)pHlp->pfnDecoderStateGet(pHlp);

        /* Can only decode the response buffer if we know the command code. */
        if (pTpmState)
        {
            for (uint32_t i = 0; i < RT_ELEMENTS(s_aTpmCmdCodes); i++)
            {
                if (s_aTpmCmdCodes[i].u32CmdCode == pTpmState->u32CmdCode)
                {
                    if (s_aTpmCmdCodes[i].pfnDecodeResp)
                    {
                        s_aTpmCmdCodes[i].pfnDecodeResp(pHlp, pTpmState, pHdr, cbRespPayload);
                        return;
                    }
                    break;
                }
            }
        }

        if (cbRespPayload)
            pHlp->pfnPrintf(pHlp, "%.*Rhxd\n", cbRespPayload, pHdr + 1);
    }
    else
        pHlp->pfnErrorMsg(pHlp, "Response buffer is smaller than the request header (required %u, given %zu\n", sizeof(*pHdr), cbResp);
}


static DECLCALLBACK(int) vboxTraceLogDecodeEvtTpm(PRTTRACELOGDECODERHLP pHlp, uint32_t idDecodeEvt, RTTRACELOGRDREVT hTraceLogEvt,
                                                  PCRTTRACELOGEVTDESC pEvtDesc, PRTTRACELOGEVTVAL paVals, uint32_t cVals)
{
    RT_NOREF(hTraceLogEvt, pEvtDesc);
    if (idDecodeEvt == 0)
    {
        for (uint32_t i = 0; i < cVals; i++)
        {
            /* Look for the pvCmd item which stores the command buffer. */
            if (   !strcmp(paVals[i].pItemDesc->pszName, "pvCmd")
                && paVals[i].pItemDesc->enmType == RTTRACELOGTYPE_RAWDATA)
            {
                vboxTraceLogDecodeEvtTpmDecodeCmdBuffer(pHlp, paVals[i].u.RawData.pb, paVals[i].u.RawData.cb);
                return VINF_SUCCESS;
            }
        }

        pHlp->pfnErrorMsg(pHlp, "Failed to find the TPM command data buffer for the given event\n");
    }
    else if (idDecodeEvt == 1)
    {
        for (uint32_t i = 0; i < cVals; i++)
        {
            /* Look for the pvCmd item which stores the response buffer. */
            if (   !strcmp(paVals[i].pItemDesc->pszName, "pvResp")
                && paVals[i].pItemDesc->enmType == RTTRACELOGTYPE_RAWDATA)
            {
                vboxTraceLogDecodeEvtTpmDecodeRespBuffer(pHlp, paVals[i].u.RawData.pb, paVals[i].u.RawData.cb);
                return VINF_SUCCESS;
            }
        }
        pHlp->pfnErrorMsg(pHlp, "Failed to find the TPM command response buffer for the given event\n");
    }

    pHlp->pfnErrorMsg(pHlp, "Decode event ID %u is not known to this decoder\n", idDecodeEvt);
    return VERR_NOT_FOUND;
}


/**
 * TPM decoder event IDs.
 */
static const RTTRACELOGDECODEEVT s_aDecodeEvtTpm[] =
{
    { "ITpmConnector.CmdExecReq",  0          },
    { "ITpmConnector.CmdExecResp", 1          },
    { NULL,                        UINT32_MAX }
};


/**
 * Decoder plugin interface.
 */
static const RTTRACELOGDECODERREG g_TraceLogDecoderTpm =
{
    /** pszName */
    "TPM",
    /** pszDesc */
    "Decodes events from the ITpmConnector interface generated with the IfTrace driver.",
    /** paEvtIds */
    s_aDecodeEvtTpm,
    /** pfnDecode */
    vboxTraceLogDecodeEvtTpm,
};


/**
 * Shared object initialization callback.
 */
extern "C" DECLCALLBACK(DECLEXPORT(int)) RTTraceLogDecoderLoad(void *pvUser, PRTTRACELOGDECODERREGISTER pRegisterCallbacks)
{
    AssertLogRelMsgReturn(pRegisterCallbacks->u32Version == RT_TRACELOG_DECODERREG_CB_VERSION,
                          ("pRegisterCallbacks->u32Version=%#x RT_TRACELOG_DECODERREG_CB_VERSION=%#x\n",
                          pRegisterCallbacks->u32Version, RT_TRACELOG_DECODERREG_CB_VERSION),
                          VERR_VERSION_MISMATCH);

    return pRegisterCallbacks->pfnRegisterDecoders(pvUser, &g_TraceLogDecoderTpm, 1);
}

