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
#include <iprt/message.h>
#include <iprt/tracelog-decoder-plugin.h>

#include <iprt/formats/tpm.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 */
typedef DECLCALLBACKTYPE(void, FNDECODETPM2CC, (PRTTRACELOGDECODERHLP pHlp, PCTPMREQHDR pHdr, size_t cb));
/** Pointer to an event decode callback. */
typedef FNDECODETPM2CC *PFNFNDECODETPM2CC;


/*********************************************************************************************************************************
*   Static Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeStartupShutdown(PRTTRACELOGDECODERHLP pHlp, PCTPMREQHDR pHdr, size_t cb)
{
    if (cb >= sizeof(uint16_t))
    {
        uint16_t u16TpmSu = RT_BE2H_U16(*(const uint16_t *)(pHdr + 1));
        if (u16TpmSu == TPM2_SU_CLEAR)
            RTMsgInfo("        TPM2_SU_CLEAR\n");
        else if (u16TpmSu == TPM2_SU_STATE)
            RTMsgInfo("        TPM2_SU_STATE\n");
        else
            RTMsgInfo("        Unknown: %#x\n", u16TpmSu);
        return;
    }

    pHlp->pfnErrorMsg(pHlp, "Malformed TPM2_CC_STARTUP/TPM2_CC_SHUTDOWN command, not enough room for TPM_SU constant\n");
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

static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeGetCapability(PRTTRACELOGDECODERHLP pHlp, PCTPMREQHDR pHdr, size_t cb)
{
    if (cb >= sizeof(TPM2REQGETCAPABILITY))
    {
        PCTPM2REQGETCAPABILITY pReq = (PCTPM2REQGETCAPABILITY)pHdr;
        uint32_t u32Cap      = RT_BE2H_U32(pReq->u32Cap);
        uint32_t u32Property = RT_BE2H_U32(pReq->u32Property);
        uint32_t u32Count    = RT_BE2H_U32(pReq->u32Count);
        if (u32Cap < RT_ELEMENTS(s_aTpm2Caps))
            RTMsgInfo("        u32Cap:      %s\n"
                      "        u32Property: %#x\n"
                      "        u32Count:    %#x\n",
                      s_aTpm2Caps[u32Cap], u32Property, u32Count);
        else
            RTMsgInfo("        u32Cap:      %#x (UNKNOWN)\n"
                      "        u32Property: %#x\n"
                      "        u32Count:    %#x\n",
                      u32Cap, u32Property, u32Count);
        return;
    }

    pHlp->pfnErrorMsg(pHlp, "Malformed TPM2_CC_GET_CAPABILITY command, not enough room for the input\n");
}


static DECLCALLBACK(void) vboxTraceLogDecodeEvtTpmDecodeReadPublic(PRTTRACELOGDECODERHLP pHlp, PCTPMREQHDR pHdr, size_t cb)
{
    if (cb >= sizeof(TPM2REQREADPUBLIC))
    {
        PCTPM2REQREADPUBLIC pReq = (PCTPM2REQREADPUBLIC)pHdr;
        TPMIDHOBJECT hObj = RT_BE2H_U32(pReq->hObj);
        RTMsgInfo("        hObj:      %#x\n", hObj);
        return;
    }

    pHlp->pfnErrorMsg(pHlp, "Malformed TPM2_CC_READ_PUBLIC command, not enough room for the input\n");
}


static struct
{
    uint32_t          u32CmdCode;
    const char        *pszCmdCode;
    PFNFNDECODETPM2CC pfnDecode;
} s_aTpmCmdCodes[] =
{
#define TPM_CMD_CODE_INIT(a_CmdCode, a_Desc) { a_CmdCode, #a_CmdCode, a_Desc }
    TPM_CMD_CODE_INIT(TPM2_CC_NV_UNDEFINE_SPACE_SPECIAL,        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_EVICT_CONTROL,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_HIERARCHY_CONTROL,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_UNDEFINE_SPACE,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CHANGE_EPS,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CHANGE_PPS,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CLEAR,                            NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CLEAR_CONTROL,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CLOCK_SET,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_HIERARCHY_CHANGE_AUTH,            NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_DEFINE_SPACE,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_ALLOCATE,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_SET_AUTH_POLICY,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PP_COMMANDS,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SET_PRIMARY_POLICY,               NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_FIELD_UPGRADE_START,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CLOCK_RATE_ADJUST,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CREATE_PRIMARY,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_GLOBAL_WRITE_LOCK,             NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_COMMAND_AUDIT_DIGEST,         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_INCREMENT,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_SET_BITS,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_EXTEND,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_WRITE,                         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_WRITE_LOCK,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_DICTIONARY_ATTACK_LOCK_RESET,     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_DICTIONARY_ATTACK_PARAMETERS,     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_CHANGE_AUTH,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_EVENT,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_RESET,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SEQUENCE_COMPLETE,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SET_ALGORITHM_SET,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SET_COMMAND_CODE_AUDIT_STATUS,    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_FIELD_UPGRADE_DATA,               NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_INCREMENTAL_SELF_TEST,            NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SELF_TEST,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_STARTUP,                          vboxTraceLogDecodeEvtTpmDecodeStartupShutdown),
    TPM_CMD_CODE_INIT(TPM2_CC_SHUTDOWN,                         vboxTraceLogDecodeEvtTpmDecodeStartupShutdown),
    TPM_CMD_CODE_INIT(TPM2_CC_STIR_RANDOM,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ACTIVATE_CREDENTIAL,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CERTIFY,                          NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_NV,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CERTIFY_CREATION,                 NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_DUPLICATE,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_TIME,                         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_SESSION_AUDIT_DIGEST,         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_READ,                          NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_READ_LOCK,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_OBJECT_CHANGE_AUTH,               NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_SECRET,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_REWRAP,                           NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CREATE,                           NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ECDH_ZGEN,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_HMAC_MAC,                         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_IMPORT,                           NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_LOAD,                             NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_QUOTE,                            NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_RSA_DECRYPT,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_HMAC_MAC_START,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SEQUENCE_UPDATE,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SIGN,                             NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_UNSEAL,                           NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_SIGNED,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CONTEXT_LOAD,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CONTEXT_SAVE,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ECDH_KEY_GEN,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ENCRYPT_DECRYPT,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_FLUSH_CONTEXT,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_LOAD_EXTERNAL,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_MAKE_CREDENTIAL,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_READ_PUBLIC,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_AUTHORIZE,                 NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_AUTH_VALUE,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_COMMAND_CODE,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_COUNTER_TIMER,             NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_CP_HASH,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_LOCALITY,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_NAME_HASH,                 NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_OR,                        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_TICKET,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_READ_PUBLIC,                      vboxTraceLogDecodeEvtTpmDecodeReadPublic),
    TPM_CMD_CODE_INIT(TPM2_CC_RSA_ENCRYPT,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_START_AUTH_SESSION,               NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_VERIFY_SIGNATURE,                 NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ECC_PARAMETERS,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_FIRMWARE_READ,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_CAPABILITY,                   vboxTraceLogDecodeEvtTpmDecodeGetCapability),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_RANDOM,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_TEST_RESULT,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_GET_HASH,                         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_READ,                         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_PCR,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_RESTART,                   NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_READ_CLOCK,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_EXTEND,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_PCR_SET_AUTH_VALUE,               NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_CERTIFY,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_EVENT_SEQUENCE_COMPLETE,          NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_HASH_SEQUENCE_START,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_PHYSICAL_PRESENCE,         NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_DUPLICATION_SELECT,        NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_GET_DIGEST,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_TEST_PARMS,                       NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_COMMIT,                           NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_PASSWORD,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ZGEN_2PHASE,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_EC_EPHEMERAL,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_NV_WRITTEN,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_TEMPLATE,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CREATE_LOADED,                    NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_AUTHORIZE_NV,              NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ENCRYPT_DECRYPT_2,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_AC_GET_CAPABILITY,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_AC_SEND,                          NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_AC_SEND_SELECT,            NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_CERTIFY_X509,                     NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ACT_SET_TIMEOUT,                  NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ECC_ENCRYPT,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_ECC_DECRYPT,                      NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_CAPABILITY,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_POLICY_PARAMETERS,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_DEFINE_SPACE_2,                NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_NV_READ_PUBLIC_2,                 NULL),
    TPM_CMD_CODE_INIT(TPM2_CC_SET_CAPABILITY,                   NULL)
#undef TPM_CMD_CODE_INIT
};

static void vboxTraceLogDecodeEvtTpmDecodeCmdBuffer(PRTTRACELOGDECODERHLP pHlp, const uint8_t *pbCmd, size_t cbCmd)
{
    PCTPMREQHDR pHdr = (PCTPMREQHDR)pbCmd;
    if (cbCmd >= sizeof(*pHdr))
    {
        uint32_t u32CmdCode = RT_BE2H_U32(pHdr->u32Ordinal);
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTpmCmdCodes); i++)
        {
            if (s_aTpmCmdCodes[i].u32CmdCode == u32CmdCode)
            {
                RTMsgInfo("    %s:\n", s_aTpmCmdCodes[i].pszCmdCode);
                if (s_aTpmCmdCodes[i].pfnDecode)
                    s_aTpmCmdCodes[i].pfnDecode(pHlp, pHdr, RT_BE2H_U32(pHdr->cbReq));
                return;
            }
        }
        RTMsgInfo("    <Unknown command code>: %#x\n", u32CmdCode);
    }
    else
        RTMsgError("Command buffer is smaller than the request header (required %u, given %zu\n", sizeof(*pHdr), cbCmd);
}


static void vboxTraceLogDecodeEvtTpmDecodeRespBuffer(PRTTRACELOGDECODERHLP pHlp, const uint8_t *pbResp, size_t cbResp)
{
    RT_NOREF(pHlp);

    PCTPMRESPHDR pHdr = (PCTPMRESPHDR)pbResp;
    if (cbResp >= sizeof(*pHdr))
    {
        RTMsgInfo("    Status code: %#x\n", RT_BE2H_U32(pHdr->u32ErrCode));
    }
    else
        RTMsgError("Response buffer is smaller than the request header (required %u, given %zu\n", sizeof(*pHdr), cbResp);
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

