/* $Id$ */
/** @file
 * DevCodec - VBox ICH Intel HD Audio Codec.
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

#include "../Builtins.h"
extern "C" {
#include "audio.h"
}
#include "DevCodec.h"

#define CODEC_CAD_MASK              0xF0000000
#define CODEC_CAD_SHIFT             28
#define CODEC_DIRECT_MASK           RT_BIT(27)
#define CODEC_NID_MASK              0x07F00000
#define CODEC_NID_SHIFT             20
#define CODEC_VERBDATA_MASK         0x000FFFFF
#define CODEC_VERB_4BIT_CMD         0x000FFFF0
#define CODEC_VERB_4BIT_DATA        0x0000000F
#define CODEC_VERB_8BIT_CMD         0x000FFF00
#define CODEC_VERB_8BIT_DATA        0x000000FF
#define CODEC_VERB_16BIT_CMD        0x000F0000
#define CODEC_VERB_16BIT_DATA       0x0000FFFF

#define CODEC_CAD(cmd) ((cmd) & CODEC_CAD_MASK)
#define CODEC_DIRECT(cmd) ((cmd) & CODEC_DIRECT_MASK)
#define CODEC_NID(cmd) ((((cmd) & CODEC_NID_MASK)) >> CODEC_NID_SHIFT)
#define CODEC_VERBDATA(cmd) ((cmd) & CODEC_VERBDATA_MASK)
#define CODEC_VERB_CMD(cmd, mask, x) (((cmd) & (mask)) >> (x))
#define CODEC_VERB_CMD4(cmd) (CODEC_VERB_CMD((cmd), CODEC_VERB_4BIT_CMD, 4))
#define CODEC_VERB_CMD8(cmd) (CODEC_VERB_CMD((cmd), CODEC_VERB_8BIT_CMD, 8))
#define CODEC_VERB_CMD16(cmd) (CODEC_VERB_CMD((cmd), CODEC_VERB_16BIT_CMD, 16))

#define CODEC_VERB_B_DIRECTION  RT_BIT(15)
#define CODEC_VERB_B_SIDE       RT_BIT(13)
#define CODEC_VERB_B_INDEX      0x7

#define CODEC_B_DIRECTION(cmd)  (((cmd) & CODEC_VERB_B_DIRECTION) >> 15)
#define CODEC_B_SIDE(cmd)       (((cmd) & CODEC_VERB_B_SIDE) >> 13)
#define CODEC_B_INDEX(cmd)      ((cmd) & CODEC_VERB_B_INDEX)


#define STAC9220_NODE_COUNT 0x1C

#define STAC9220_IS_AFG_CMD(cmd) (  \
    CODEC_NID(cmd) == 0x1)

#define STAC9220_IS_PORT_CMD(cmd) (  \
       CODEC_NID(cmd) == 0xA    \
    || CODEC_NID(cmd) == 0xB    \
    || CODEC_NID(cmd) == 0xC    \
    || CODEC_NID(cmd) == 0xD    \
    || CODEC_NID(cmd) == 0xE    \
    || CODEC_NID(cmd) == 0xF)

#define STAC9220_IS_DAC_CMD(cmd) (  \
       CODEC_NID(cmd) == 0x2    \
    || CODEC_NID(cmd) == 0x3    \
    || CODEC_NID(cmd) == 0x4    \
    || CODEC_NID(cmd) == 0x5)

#define STAC9220_IS_ADCVOL_CMD(cmd) (   \
       CODEC_NID(cmd) == 0x17           \
    || CODEC_NID(cmd) == 0x18)

#define STAC9220_IS_ADC_CMD(cmd) (     \
       CODEC_NID(cmd) == 0x6           \
    || CODEC_NID(cmd) == 0x7)

#define STAC9220_IS_ADCMUX_CMD(cmd) (   \
       CODEC_NID(cmd) == 0x12           \
    || CODEC_NID(cmd) == 0x13)

#define STAC9220_IS_PCBEEP_CMD(cmd) (CODEC_NID((cmd)) == 0x14)
#define STAC9220_IS_SPDIFOUT_CMD(cmd) (CODEC_NID((cmd)) == 0x8)
#define STAC9220_IS_SPDIFIN_CMD(cmd) (CODEC_NID((cmd)) == 0x9)

#define STAC9220_IS_DIGINPIN_CMD(cmd) (CODEC_NID((cmd)) == 0x11)
#define STAC9220_IS_DIGOUTPIN_CMD(cmd) (CODEC_NID((cmd)) == 0x10)

#define STAC9220_IS_CD_CMD(cmd) (CODEC_NID((cmd)) == 0x15)

#define STAC9220_IS_VOLKNOB_CMD(cmd) (CODEC_NID((cmd)) == 0x16)

/* STAC9220 6.2 & 6.12 */
#define STAC9220_IS_RESERVED_CMD(cmd) ( \
    CODEC_NID((cmd)) == 0x19            \
    || CODEC_NID((cmd)) == 0x1A         \
    || CODEC_NID((cmd)) == 0x1B)

static int stac9220ResetNode(struct CODECState *pState, uint8_t nodenum, PCODECNODE pNode);


static int codecUnimplemented(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Log(("codecUnimplemented: cmd(raw:%x: cad:%x, d:%c, nid:%x, verb:%x)\n", cmd,
        CODEC_CAD(cmd), CODEC_DIRECT(cmd) ? 'N' : 'Y', CODEC_NID(cmd), CODEC_VERBDATA(cmd)));
    *pResp = 0;
    return VINF_SUCCESS;
}

static int codecBreak(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    int rc;
    rc = codecUnimplemented(pState, cmd, pResp);
    *pResp |= CODEC_RESPONSE_UNSOLICITED;
    return rc;
}
/* B-- */
static int codecGetAmplifier(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    PCODECNODE pNode = &pState->pNodes[CODEC_NID(cmd)];
    if (STAC9220_IS_DAC_CMD(cmd))
        *pResp = AMPLIFIER_REGISTER(pNode->dac.B_params,
                            CODEC_B_DIRECTION(cmd),
                            CODEC_B_SIDE(cmd),
                            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_ADCVOL_CMD(cmd))
        *pResp = AMPLIFIER_REGISTER(pNode->adcvol.B_params,
                            CODEC_B_DIRECTION(cmd),
                            CODEC_B_SIDE(cmd),
                            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_ADCMUX_CMD(cmd))
        *pResp = AMPLIFIER_REGISTER(pNode->adcmux.B_params,
                            CODEC_B_DIRECTION(cmd),
                            CODEC_B_SIDE(cmd),
                            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_PCBEEP_CMD(cmd))
        *pResp = AMPLIFIER_REGISTER(pNode->pcbeep.B_params,
                            CODEC_B_DIRECTION(cmd),
                            CODEC_B_SIDE(cmd),
                            CODEC_B_INDEX(cmd));
    else{
        AssertMsgReturn(0, ("access to fields of %x need to be implemented\n", CODEC_NID(cmd)), VINF_SUCCESS);
    }
    return VINF_SUCCESS;
}
/* 3-- */
static int codecSetAmplifier(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t *pu32Bparam = NULL;
    Assert((CODEC_CAD(cmd) == pState->id));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    PCODECNODE pNode = &pState->pNodes[CODEC_NID(cmd)];
    if (STAC9220_IS_DAC_CMD(cmd))
        pu32Bparam = &AMPLIFIER_REGISTER(pNode->dac.B_params,
            CODEC_B_DIRECTION(cmd),
            CODEC_B_SIDE(cmd),
            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_ADCVOL_CMD(cmd))
        pu32Bparam = &AMPLIFIER_REGISTER(pNode->adcvol.B_params,
            CODEC_B_DIRECTION(cmd),
            CODEC_B_SIDE(cmd),
            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_ADCMUX_CMD(cmd))
        pu32Bparam = &AMPLIFIER_REGISTER(pNode->adcmux.B_params,
            CODEC_B_DIRECTION(cmd),
            CODEC_B_SIDE(cmd),
            CODEC_B_INDEX(cmd));
    else if (STAC9220_IS_PCBEEP_CMD(cmd))
        pu32Bparam = &AMPLIFIER_REGISTER(pNode->pcbeep.B_params,
            CODEC_B_DIRECTION(cmd),
            CODEC_B_SIDE(cmd),
            CODEC_B_INDEX(cmd));
    Assert(pu32Bparam);
    if (pu32Bparam)
    {
        *pu32Bparam = (*pu32Bparam) & ~CODEC_VERB_8BIT_DATA;
        *pu32Bparam = (*pu32Bparam) | (cmd & CODEC_VERB_8BIT_DATA);
    }
    return VINF_SUCCESS;
}

static int codecGetParameter(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert(((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F0_PARAM_LENGTH));
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F0_PARAM_LENGTH)
    {
        Log(("HDAcodec: invalid F00 parameter %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    *pResp = pState->pNodes[CODEC_NID(cmd)].node.au32F00_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F01 */
static int codecGetConSelectCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_ADCMUX_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digout.u32F01_param;
    return VINF_SUCCESS;
}

/* 701 */
static int codecSetConSelectCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t *pu32Reg = NULL;
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_ADCMUX_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digout.u32F01_param;
    Assert((pu32Reg));
    if (!pu32Reg)
        return VINF_SUCCESS;
    *pu32Reg = (*pu32Reg) & ~CODEC_VERB_8BIT_DATA;
    *pu32Reg = (*pu32Reg) | (cmd & CODEC_VERB_8BIT_DATA);
    return VINF_SUCCESS;
}

/* F07 */
static int codecGetPinCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_PORT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (STAC9220_IS_CD_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else
        AssertMsgFailed(("Unsupported"));
    return VINF_SUCCESS;
}

/* 707 */
static int codecSetPinCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (STAC9220_IS_PORT_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (STAC9220_IS_CD_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    Assert((pu32Reg));
    if (!pu32Reg)
        return VINF_SUCCESS;
    *pu32Reg = (*pu32Reg) & ~CODEC_VERB_8BIT_DATA;
    *pu32Reg = (*pu32Reg) | (cmd & CODEC_VERB_8BIT_DATA);
    return VINF_SUCCESS;
}

/* F08 */
static int codecGetUnsolicitedEnabled(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_PORT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (STAC9220_IS_AFG_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (STAC9220_IS_VOLKNOB_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 708 */
static int codecSetUnsolicitedEnabled(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (STAC9220_IS_PORT_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (STAC9220_IS_AFG_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (STAC9220_IS_VOLKNOB_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    Assert(pu32Reg);
    if(!pu32Reg)
        return VINF_SUCCESS;
    *pu32Reg &= ~CODEC_VERB_8BIT_DATA;
    *pu32Reg |= cmd & CODEC_VERB_8BIT_DATA;
    return VINF_SUCCESS;
}

/* F09 */
static int codecGetPinSense(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_PORT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 709 */
static int codecSetPinSense(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (STAC9220_IS_PORT_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F08_param;
    Assert(pu32Reg);
    if(!pu32Reg)
        return VINF_SUCCESS;
    *pu32Reg &= ~CODEC_VERB_8BIT_DATA;
    *pu32Reg |= cmd & CODEC_VERB_8BIT_DATA;
    return VINF_SUCCESS;
}

static int codecGetConnectionListEntry(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert((cmd & CODEC_VERB_8BIT_DATA) < 16);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= 16)
    {
        Log(("HDAcodec: access to invalid F02 index %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
    }
    *pResp = *(uint32_t *)&pState->pNodes[CODEC_NID(cmd)].node.au8F02_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}
/* F03 */
static int codecGetProcessingState(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_ADC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adc.u32F03_param;
    return VINF_SUCCESS;
}
/* 703 */
static int codecSetProcessingState(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_ADC_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].adc.u32F03_param &= ~CODEC_VERB_8BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].adc.u32F03_param |= cmd & CODEC_VERB_8BIT_DATA;
    }
    return VINF_SUCCESS;
}
/* F0D */
static int codecGetDigitalConverter(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_SPDIFOUT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifout.u32F0d_param;
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifin.u32F0d_param;
    return VINF_SUCCESS;
}
/* 70D */
static int codecSetDigitalConverter1(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_SPDIFOUT_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32F0d_param &= ~CODEC_VERB_8BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32F0d_param |= cmd & CODEC_VERB_8BIT_DATA;
    }
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32F0d_param &= ~CODEC_VERB_8BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32F0d_param |= cmd & CODEC_VERB_8BIT_DATA;
    }
    return VINF_SUCCESS;
}
/* 70E */
static int codecSetDigitalConverter2(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_SPDIFOUT_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32F0d_param &= ~(CODEC_VERB_8BIT_DATA << 8);
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32F0d_param |= cmd & (CODEC_VERB_8BIT_DATA << 8);
    }
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32F0d_param &= ~(CODEC_VERB_8BIT_DATA << 8);
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32F0d_param |= cmd & (CODEC_VERB_8BIT_DATA << 8);
    }
    return VINF_SUCCESS;
}

static int codecGetSubId(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_AFG_CMD(cmd))
    {
        *pResp = pState->pNodes[CODEC_NID(cmd)].afg.u32F20_param;
    }
    return VINF_SUCCESS;
}

static int codecReset(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert(STAC9220_IS_AFG_CMD(cmd));
    if(STAC9220_IS_AFG_CMD(cmd))
    {
        uint8_t i;
        Log(("HDAcodec: enters reset\n"));
        for (i = 0; i < STAC9220_NODE_COUNT; ++i)
        {
            stac9220ResetNode(pState, i, &pState->pNodes[i]);
        }
        pState->pfnReset(pState);
        Log(("HDAcodec: exits reset\n"));
    }
    *pResp = 0;
    return VINF_SUCCESS;
}

/* F05 */
static int codecGetPowerState(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_AFG_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (STAC9220_IS_DAC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (STAC9220_IS_ADC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adc.u32F05_param;
    return VINF_SUCCESS;
}

/* 705 */
static int codecSetPowerState(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (STAC9220_IS_AFG_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (STAC9220_IS_DAC_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (STAC9220_IS_ADC_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].adc.u32F05_param;
    Assert((pu32Reg));
    if (!pu32Reg)
        return VINF_SUCCESS;
    *pu32Reg &= ~CODEC_VERB_8BIT_DATA;
    *pu32Reg |= cmd & CODEC_VERB_4BIT_DATA;
    *pu32Reg |= (cmd & CODEC_VERB_4BIT_DATA) << 4;
    return VINF_SUCCESS;
}

static int codecGetStreamId(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_DAC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (STAC9220_IS_ADC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (STAC9220_IS_SPDIFOUT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    return VINF_SUCCESS;
}
static int codecSetStreamId(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32addr = NULL;
    *pResp = 0;
    if (STAC9220_IS_DAC_CMD(cmd))
        pu32addr = &pState->pNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (STAC9220_IS_ADC_CMD(cmd))
        pu32addr = &pState->pNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (STAC9220_IS_SPDIFOUT_CMD(cmd))
        pu32addr = &pState->pNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
        pu32addr = &pState->pNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    Assert((pu32addr));
    if (pu32addr)
    {
        *pu32addr &= ~CODEC_VERB_8BIT_DATA;
        *pu32addr |= cmd & CODEC_VERB_8BIT_DATA;
    }
    return VINF_SUCCESS;
}
static int codecGetConverterFormat(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_DAC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].dac.u32A_param;
    else if (STAC9220_IS_ADC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adc.u32A_param;
    else if (STAC9220_IS_SPDIFOUT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifout.u32A_param;
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].spdifin.u32A_param;
    return VINF_SUCCESS;
}

static int codecSetConverterFormat(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_DAC_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].dac.u32A_param &= ~CODEC_VERB_16BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].dac.u32A_param |= cmd & CODEC_VERB_16BIT_DATA;
    }
    else if (STAC9220_IS_ADC_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].adc.u32A_param &= ~CODEC_VERB_16BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].adc.u32A_param |= cmd & CODEC_VERB_16BIT_DATA;
    }
    else if (STAC9220_IS_SPDIFOUT_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32A_param &= ~CODEC_VERB_16BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].spdifout.u32A_param |= cmd & CODEC_VERB_16BIT_DATA;
    }
    else if (STAC9220_IS_SPDIFIN_CMD(cmd))
    {
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32A_param &= ~CODEC_VERB_16BIT_DATA;
        pState->pNodes[CODEC_NID(cmd)].spdifin.u32A_param |= cmd & CODEC_VERB_16BIT_DATA;
    }
    return VINF_SUCCESS;
}

/* F0C */
static int codecGetEAPD_BTLEnabled(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_ADCVOL_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (STAC9220_IS_DAC_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    return VINF_SUCCESS;
}

/* 70C */
static int codecSetEAPD_BTLEnabled(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (STAC9220_IS_ADCVOL_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (STAC9220_IS_DAC_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    *pResp = 0;
    Assert((pu32Reg));
    if (pu32Reg)
    {
        *pu32Reg &= ~CODEC_VERB_8BIT_DATA;
        *pu32Reg |= cmd & CODEC_VERB_8BIT_DATA;
    }
    return VINF_SUCCESS;
}

/* F0F */
static int codecGetVolumeKnobCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_VOLKNOB_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    return VINF_SUCCESS;
}

/* 70F */
static int codecSetVolumeKnobCtrl(struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (STAC9220_IS_VOLKNOB_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    Assert((pu32Reg));
    if (pu32Reg)
    {
        *pu32Reg &= ~CODEC_VERB_8BIT_DATA;
        *pu32Reg |= cmd & CODEC_VERB_8BIT_DATA;
    }
    return VINF_SUCCESS;
}

/* F1C */
static int codecGetConfig (struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (STAC9220_IS_PORT_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (STAC9220_IS_CD_CMD(cmd))
        *pResp = pState->pNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    return VINF_SUCCESS;
}
static int codecSetConfigX(struct CODECState *pState, uint32_t cmd, uint32_t mask)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < STAC9220_NODE_COUNT));
    if (CODEC_NID(cmd) >= STAC9220_NODE_COUNT)
    {
        Log(("HDAcodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    if (STAC9220_IS_PORT_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (STAC9220_IS_DIGINPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (STAC9220_IS_DIGOUTPIN_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (STAC9220_IS_CD_CMD(cmd))
        pu32Reg = &pState->pNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    Assert((pu32Reg));
    if (pu32Reg)
    {
        *pu32Reg &= ~mask;
        *pu32Reg |= cmd & mask;
    }
    return VINF_SUCCESS;
}
/* 71C */
static int codecSetConfig0 (struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t mask = CODEC_VERB_8BIT_DATA;
    *pResp = 0;
    return codecSetConfigX(pState, cmd, mask);
}
/* 71D */
static int codecSetConfig1 (struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t mask = CODEC_VERB_8BIT_DATA << 8;
    *pResp = 0;
    return codecSetConfigX(pState, cmd, mask);
}
/* 71E */
static int codecSetConfig2 (struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t mask = CODEC_VERB_8BIT_DATA << 16;
    *pResp = 0;
    return codecSetConfigX(pState, cmd, mask);
}
/* 71E */
static int codecSetConfig3 (struct CODECState *pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t mask = CODEC_VERB_8BIT_DATA << 24;
    *pResp = 0;
    return codecSetConfigX(pState, cmd, mask);
}

static int stac9220ResetNode(struct CODECState *pState, uint8_t nodenum, PCODECNODE pNode)
{
    pNode->node.id = nodenum;
    pNode->node.au32F00_param[0xF] = RT_BIT(3)|RT_BIT(0); /* Power statest Supported: D0-yes, D1, D2, D3-no*/
    switch (nodenum)
    {
        /* Root Node*/
        case 0:
            pNode->root.node.name = "Root";
            //** @todo r=michaln: I fear the use of RT_MAKE_U32_FROM_U8() here makes the
            // code much harder to read, not easier.
            pNode->node.au32F00_param[0] = RT_MAKE_U32_FROM_U8(0x80, 0x76, 0x84, 0x83); /* VendorID = STAC9220/ DevId = 0x7680 */
            pNode->node.au32F00_param[2] = RT_MAKE_U32_FROM_U8(0x1, 0x34, 0x10, 0x00); /* rev id */
            pNode->node.au32F00_param[4] = RT_MAKE_U32_FROM_U8(0x1, 0x00, 0x01, 0x00); /* node info (start node: 1, start id = 1) */
            break;
        case 1:
            pNode->afg.node.name = "AFG";
            pNode->node.au32F00_param[4] = 2 << 16 | 0x17; /* starting node - 2; total numbers of nodes  0x17 */
            pNode->node.au32F00_param[5] = RT_BIT(8)|RT_BIT(0);
            pNode->node.au32F00_param[8] = RT_MAKE_U32_FROM_U8(0x0d, 0x0d, 0x01, 0x0); /* Capabilities */
            //pNode->node.au32F00_param[0xa] = RT_BIT(19)|RT_BIT(18)|RT_BIT(17)|RT_BIT(10)|RT_BIT(9)|RT_BIT(8)|RT_BIT(7)|RT_BIT(6)|RT_BIT(5);
            pNode->node.au32F00_param[0xa] = RT_BIT(17)|RT_BIT(5);
            pNode->node.au32F00_param[0xc] = (17 << 8)|RT_BIT(6)|RT_BIT(5)|RT_BIT(2)|RT_BIT(1)|RT_BIT(0);
            pNode->node.au32F00_param[0xb] = RT_BIT(0);
            pNode->node.au32F00_param[0xd] = RT_BIT(31)|(0x5 << 16)|(0xE)<<8;
            pNode->node.au32F00_param[0x12] = RT_BIT(31)|(0x2 << 16)|(0x7f << 8)|0x7f;
            pNode->node.au32F00_param[0x11] = 0;
            pNode->node.au32F00_param[0xF] = RT_BIT(30)|RT_BIT(3)|RT_BIT(0); /* Power statest Supported: D0-yes, D1, D2, D3-no*/
            pNode->afg.u32F05_param = 0x3 << 4| 0x3; /* PS-Act: D3, PS->Set D3  */
            pNode->afg.u32F20_param = 0x83847882;
            pNode->afg.u32F08_param = 0;
            break;
        case 2:
            pNode->dac.node.name = "DAC0";
            goto dac_init;
        case 3:
            pNode->dac.node.name = "DAC1";
            goto dac_init;
        case 4:
            pNode->dac.node.name = "DAC2";
            goto dac_init;
        case 5:
            pNode->dac.node.name = "DAC3";
        dac_init:
            memset(pNode->dac.B_params, 0, AMPLIFIER_SIZE);
            pNode->dac.u32A_param = RT_BIT(14)|(0x1 << 4)|0x2; /* 441000Hz/16bit/2ch */

            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_LEFT, 0) = 0x7F | RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_RIGHT, 0) = 0x7F | RT_BIT(7);

            pNode->dac.node.au32F00_param[9] = (0xd << 16) | RT_BIT(11) |  RT_BIT(10) | RT_BIT(2) | RT_BIT(0);
            pNode->dac.node.au32F00_param[5] = (0x3 << 4) | 0x3;
            pNode->dac.u32F0c_param = 0;
            pNode->dac.u32F05_param = 0x3 << 4 | 0x3; /* PS-Act: D3, Set: D3  */
        break;
        case 6:
            pNode->adc.node.name = "ADC0";
            pNode->node.au8F02_param[0] = 0x17;
            goto adc_init;
        case 7:
            pNode->adc.node.name = "ADC1";
            pNode->node.au8F02_param[0] = 0x18;
        adc_init:
            pNode->adc.u32A_param = RT_BIT(14)|(0x1 << 3)|0x2; /* 441000Hz/16bit/2ch */
            pNode->adc.node.au32F00_param[0xE] = RT_BIT(0);
            pNode->adc.u32F03_param = RT_BIT(0);
            pNode->adc.u32F05_param = 0x3 << 4 | 0x3; /* PS-Act: D3 Set: D3 */
            pNode->adc.u32F06_param = 0;
            pNode->adc.node.au32F00_param[9] = RT_BIT(20)| (0xd << 16) |  RT_BIT(10) | RT_BIT(8) | RT_BIT(6)| RT_BIT(0);
            break;
        case 8:
            pNode->spdifout.node.name = "SPDIFOut";
            pNode->spdifout.u32A_param = (1<<14)|(0x1<<4) | 0x1;
            pNode->spdifout.node.au32F00_param[9] = (4 << 16) | RT_BIT(9)|RT_BIT(4)|0x1;
            pNode->node.au32F00_param[0xa] = RT_BIT(17)|RT_BIT(5);
            pNode->spdifout.node.au32F00_param[0xB] = RT_BIT(2)|RT_BIT(0);
            pNode->spdifout.u32F06_param = 0;
            pNode->spdifout.u32F0d_param = 0;
            //pNode->spdifout.node.au32F00_param[0xA] = RT_BIT(19)|RT_BIT(18)|RT_BIT(17)|RT_BIT(10)|RT_BIT(9)|RT_BIT(8)|RT_BIT(7)|RT_BIT(6);
        break;
        case 9:
            pNode->node.name = "Reserved_0";
            pNode->spdifin.u32A_param = (0x1<<4) | 0x1;
            pNode->spdifin.node.au32F00_param[9] = (0x1 << 20)|(4 << 16) | RT_BIT(9)|RT_BIT(4)|0x1;
            pNode->node.au32F00_param[0xa] = RT_BIT(17)|RT_BIT(5);
            pNode->spdifin.node.au32F00_param[0xB] = RT_BIT(2)|RT_BIT(0);
            pNode->spdifin.u32F06_param = 0;
            pNode->spdifin.u32F0d_param = 0;
        break;
        case 0xA:
            pNode->node.name = "PortA";
            pNode->node.au32F00_param[0xC] = 0x173f;
            *(uint32_t *)pNode->node.au8F02_param = 0x2;
            pNode->port.u32F07_param = RT_BIT(6);
            pNode->port.u32F08_param = 0;
            pNode->port.u32F09_param = RT_BIT(31)|RT_BIT(30); /* 39.2 kOm */
            pNode->port.u32F1c_param = RT_MAKE_U32_FROM_U8(0x20, 0x40, 0x21, 0x02);
            goto port_init;
        case 0xB:
            pNode->node.name = "PortB";
            pNode->node.au32F00_param[0xC] = 0x1737;
            *(uint32_t *)pNode->node.au8F02_param = 0x4;
            pNode->port.u32F09_param = 0;
            pNode->port.u32F07_param = RT_BIT(5);
            pNode->port.u32F1c_param = RT_MAKE_U32_FROM_U8(0x11, 0x60, 0x11, 0x01);
            goto port_init;
        case 0xC:
            pNode->node.name = "PortC";
            *(uint32_t *)pNode->node.au8F02_param = 0x3;
            pNode->node.au32F00_param[0xC] = 0x1737;
            pNode->port.u32F09_param = 0;
            pNode->port.u32F07_param = RT_BIT(5);
            pNode->port.u32F1c_param = RT_MAKE_U32_FROM_U8(0x10, 0x40, 0x11, 0x01);
            goto port_init;
        case 0xD:
            pNode->node.name = "PortD";
            pNode->port.u32F09_param = 0;
            pNode->node.au32F00_param[0xC] = 0x173f;
            *(uint32_t *)pNode->node.au8F02_param = 0x2;
        port_init:
            pNode->port.u32F08_param = 0;
            pNode->node.au32F00_param[9] = (4 << 20)|RT_BIT(8)|RT_BIT(7)|RT_BIT(0);
            pNode->node.au32F00_param[0xE] = 0x1;
        break;
        case 0xE:
            pNode->node.name = "PortE";
            pNode->node.au32F00_param[9] = (4 << 20)|RT_BIT(7)|RT_BIT(0);
            pNode->port.u32F08_param = 0;
            pNode->node.au32F00_param[0xC] = RT_BIT(5)|RT_BIT(2);
            pNode->port.u32F07_param = RT_BIT(5);
            pNode->port.u32F09_param = 0;
            pNode->port.u32F1c_param = RT_MAKE_U32_FROM_U8(0x51, 0x30, 0x81, 0x01);
            break;
        case 0xF:
            pNode->node.name = "PortF";
            pNode->node.au32F00_param[9] = (4 << 20)|RT_BIT(8)|RT_BIT(7)|RT_BIT(0);
            pNode->node.au32F00_param[0xC] = 0x37;
            pNode->node.au32F00_param[0xE] = 0x1;
            pNode->port.u32F08_param = 0;
            pNode->port.u32F07_param = 0;
            pNode->port.u32F1c_param = RT_MAKE_U32_FROM_U8(0x12, 0x60, 0x11, 0x01);
            pNode->node.au8F02_param[0] = 0x5;
            pNode->port.u32F09_param = 0;
        break;
        case 0x10:
            pNode->node.name = "DigOut_0";
            pNode->node.au32F00_param[9] = (4<<20)|RT_BIT(9)|RT_BIT(8)|RT_BIT(0);
            pNode->node.au32F00_param[0xC] = RT_BIT(4);
            pNode->node.au32F00_param[0xE] = 0x2;
            pNode->digout.u32F01_param = 0;
            /* STAC9220 spec defines default connection list containing reserved nodes, that confuses some drivers. */
            *(uint32_t *)pNode->node.au8F02_param = RT_MAKE_U32_FROM_U8(0x08, 0x17, 0x0, 0);
            pNode->digout.u32F07_param = 0;
            pNode->digout.u32F1c_param = RT_MAKE_U32_FROM_U8(0x30, 0x10, 0x45, 0x01);
        break;
        case 0x11:
            pNode->node.name = "DigIn_0";
            pNode->node.au32F00_param[9] = (4 << 20)|(3<<16)|RT_BIT(10)|RT_BIT(9)|RT_BIT(7)|RT_BIT(0);
            pNode->node.au32F00_param[0xC] = /* RT_BIT(16)|*/ RT_BIT(5)|RT_BIT(2);
            pNode->digin.u32F05_param = 0x3 << 4 | 0x3; /* PS-Act: D3 -> D3 */
            pNode->digin.u32F07_param = 0;
            pNode->digin.u32F08_param = 0;
            pNode->digin.u32F09_param = 0;
            pNode->digin.u32F0c_param = 0;
            pNode->digin.u32F1c_param = (0x1 << 24) | (0xc5 << 16) | (0x10 << 8) | 0x60;
        break;
        case 0x12:
            pNode->node.name = "ADCMux_0";
            pNode->adcmux.u32F01_param = 0;
            goto adcmux_init;
        case 0x13:
            pNode->node.name = "ADCMux_1";
            pNode->adcmux.u32F01_param = 1;
            adcmux_init:
            pNode->node.au32F00_param[9] = (3<<20)|RT_BIT(8)|RT_BIT(3)|RT_BIT(2)|RT_BIT(0);
            pNode->node.au32F00_param[0xe] = 0x7;
            pNode->node.au32F00_param[0x12] = (0x27 << 16)|(0x4 << 8);
            /* STAC 9220 v10 6.21-22.{4,5} both(left and right) out amplefiers inited with 0*/
            memset(pNode->adcmux.B_params, 0, AMPLIFIER_SIZE);
            *(uint32_t *)&pNode->node.au8F02_param[0] = RT_MAKE_U32_FROM_U8(0xe, 0x15, 0xf, 0xb);
            *(uint32_t *)&pNode->node.au8F02_param[4] = RT_MAKE_U32_FROM_U8(0xc, 0xd, 0xa, 0x0);
        break;
        case 0x14:
            pNode->node.name = "PCBEEP";
            pNode->node.au32F00_param[9] = (7 << 20) | RT_BIT(3) | RT_BIT(2);
            pNode->node.au32F00_param[0x12] = (0x17 << 16)|(0x3 << 8)| 0x3;
            pNode->pcbeep.u32F0a_param = 0;
            memset(pNode->pcbeep.B_params, 0, AMPLIFIER_SIZE);
        break;
        case 0x15:
            pNode->node.name = "CD";
            pNode->node.au32F00_param[0x9] = (4 << 20)|RT_BIT(0);
            pNode->node.au32F00_param[0xc] = RT_BIT(5);
            pNode->cdnode.u32F07_param = 0;
            pNode->cdnode.u32F1c_param = RT_MAKE_U32_FROM_U8(0x52, 0x0, 0x33, 0x90);
        break;
        case 0x16:
            pNode->node.name = "VolumeKnob";
            pNode->node.au32F00_param[0x9] = (0x6 << 20);
            pNode->node.au32F00_param[0x13] = RT_BIT(7)| 0x7F;
            pNode->node.au32F00_param[0xe] = 0x4;
            *(uint32_t *)pNode->node.au8F02_param = RT_MAKE_U32_FROM_U8(0x2, 0x3, 0x4, 0x5);
            pNode->volumeKnob.u32F08_param = 0;
            pNode->volumeKnob.u32F0f_param = 0x7f;
        break;
        case 0x17:
            pNode->node.name = "ADC0Vol";
            *(uint32_t *)pNode->node.au8F02_param = 0x12;
            goto adcvol_init;
        case 0x18:
            pNode->node.name = "ADC1Vol";
            *(uint32_t *)pNode->node.au8F02_param = 0x13;
        adcvol_init:
            memset(pNode->adcvol.B_params, 0, AMPLIFIER_SIZE);

            pNode->node.au32F00_param[0x9] = (0x3 << 20)|RT_BIT(11)|RT_BIT(8)|RT_BIT(1)|RT_BIT(0);
            pNode->node.au32F00_param[0xe] = 0x1;
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_LEFT, 0) = RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_RIGHT, 0) = RT_BIT(7);
            pNode->adcvol.u32F0c_param = 0;
        default:
        break;
    }
    return VINF_SUCCESS;
}

static CODECVERB STAC9220VERB[] =
{
/*    verb     | verb mask              | callback               */
/*  -----------  --------------------   -----------------------  */
    {0x000F0000, CODEC_VERB_8BIT_CMD , codecGetParameter           },
    {0x000F0100, CODEC_VERB_8BIT_CMD , codecGetConSelectCtrl       },
    {0x00070100, CODEC_VERB_8BIT_CMD , codecSetConSelectCtrl       },
    {0x000F0600, CODEC_VERB_8BIT_CMD , codecGetStreamId            },
    {0x00070600, CODEC_VERB_8BIT_CMD , codecSetStreamId            },
    {0x000F0700, CODEC_VERB_8BIT_CMD , codecGetPinCtrl             },
    {0x00070700, CODEC_VERB_8BIT_CMD , codecSetPinCtrl             },
    {0x000F0800, CODEC_VERB_8BIT_CMD , codecGetUnsolicitedEnabled  },
    {0x00070800, CODEC_VERB_8BIT_CMD , codecSetUnsolicitedEnabled  },
    {0x000F0900, CODEC_VERB_8BIT_CMD , codecGetPinSense            },
    {0x00070900, CODEC_VERB_8BIT_CMD , codecSetPinSense            },
    {0x000F0200, CODEC_VERB_8BIT_CMD , codecGetConnectionListEntry },
    {0x000F0300, CODEC_VERB_8BIT_CMD , codecGetProcessingState     },
    {0x00070300, CODEC_VERB_8BIT_CMD , codecSetProcessingState     },
    {0x000F0D00, CODEC_VERB_8BIT_CMD , codecGetDigitalConverter    },
    {0x00070D00, CODEC_VERB_8BIT_CMD , codecSetDigitalConverter1   },
    {0x00070E00, CODEC_VERB_8BIT_CMD , codecSetDigitalConverter2   },
    {0x000F2000, CODEC_VERB_8BIT_CMD , codecGetSubId               },
    {0x0007FF00, CODEC_VERB_8BIT_CMD , codecReset                  },
    {0x000F0500, CODEC_VERB_8BIT_CMD , codecGetPowerState          },
    {0x00070500, CODEC_VERB_8BIT_CMD , codecSetPowerState          },
    {0x000F0C00, CODEC_VERB_8BIT_CMD , codecGetEAPD_BTLEnabled     },
    {0x00070C00, CODEC_VERB_8BIT_CMD , codecSetEAPD_BTLEnabled     },
    {0x000F0F00, CODEC_VERB_8BIT_CMD , codecGetVolumeKnobCtrl      },
    {0x00070F00, CODEC_VERB_8BIT_CMD , codecSetVolumeKnobCtrl      },
    {0x000F1C00, CODEC_VERB_8BIT_CMD , codecGetConfig              },
    {0x00071C00, CODEC_VERB_8BIT_CMD , codecSetConfig0             },
    {0x00071D00, CODEC_VERB_8BIT_CMD , codecSetConfig1             },
    {0x00071E00, CODEC_VERB_8BIT_CMD , codecSetConfig2             },
    {0x00071F00, CODEC_VERB_8BIT_CMD , codecSetConfig3             },
    {0x000A0000, CODEC_VERB_16BIT_CMD, codecGetConverterFormat     },
    {0x00020000, CODEC_VERB_16BIT_CMD, codecSetConverterFormat     },
    {0x000B0000, CODEC_VERB_16BIT_CMD, codecGetAmplifier           },
    {0x00030000, CODEC_VERB_16BIT_CMD, codecSetAmplifier           },
};

static int codecLookup(CODECState *pState, uint32_t cmd, PPFNCODECVERBPROCESSOR pfn)
{
    int rc = VINF_SUCCESS;
    Assert(CODEC_CAD(cmd) == pState->id);
    if (   CODEC_VERBDATA(cmd) == 0
        || CODEC_NID(cmd) >= STAC9220_NODE_COUNT
        || STAC9220_IS_RESERVED_CMD(cmd))
    {
        *pfn = codecUnimplemented;
        //** @todo r=michaln: There needs to be a counter to avoid log flooding (see e.g. DevRTC.cpp)
        LogRel(("HDAcodec: cmd %x was ignored\n", cmd));
        return VINF_SUCCESS;
    }
    for (int i = 0; i < pState->cVerbs; ++i)
    {
        if ((CODEC_VERBDATA(cmd) & pState->pVerbs[i].mask) == pState->pVerbs[i].verb)
        {
            *pfn = pState->pVerbs[i].pfn;
            return VINF_SUCCESS;
        }
    }
    *pfn = codecUnimplemented;
    LogRel(("HDAcodec: callback for %x wasn't found\n", CODEC_VERBDATA(cmd)));
    return rc;
}
#define CODEC_FMT_BASE_FRQ_SHIFT (14)
#define CODEC_FMT_BASE_FRQ_MASK (RT_BIT(CODEC_FMT_BASE_FRQ_SHIFT))
#define CODEC_FMT_DIV_FRQ_SHIFT 8
#define CODEC_FMT_DIV_FRQ_MASK ((0x7) << CODEC_FMT_DIV_FRQ_SHIFT)
#define CODEC_FMT_MUL_FRQ_SHIFT 11
#define CODEC_FMT_MUL_FRQ_MASK ((0x7) << CODEC_FMT_MUL_FRQ_SHIFT)
#define CODEC_FMT_BASE_FRQ(fmt) ((fmt & CODEC_FMT_BASE_FRQ_MASK) >> CODEC_FMT_BASE_FRQ_SHIFT)
#define CODEC_FMT_DIV_FRQ(fmt) ((fmt & CODEC_FMT_DIV_FRQ_MASK) >> CODEC_FMT_DIV_FRQ_SHIFT)
#define CODEC_FMT_MUL_FRQ(fmt) ((fmt & CODEC_FMT_MUL_FRQ_MASK) >> CODEC_FMT_MUL_FRQ_SHIFT)
#define CODEC_DAC_CHANELS(reg) (1 << ((reg) & 0x3))
static int codecFrequencyCalculate(uint32_t dacFmt)
{
    uint32_t baseFrq = CODEC_FMT_BASE_FRQ(dacFmt);
    uint32_t divFrq = CODEC_FMT_DIV_FRQ(dacFmt);
    uint32_t multFrq = CODEC_FMT_MUL_FRQ(dacFmt);
    switch (baseFrq)
    {
        case 0: baseFrq = 48000; break;
        case 0x1: baseFrq = 44100; break;
        default:
            AssertMsgFailed(("Unsupported Freq."));
    }
    switch(multFrq)
    {
        case 0: multFrq = 1; break;
        case 0x1: multFrq = 2; break;
        case 0x3: multFrq = 4; break;
        default:
            AssertMsgFailed(("Unsupported Freq. multiplier"));
    }
    switch(divFrq)
    {
        case 0: divFrq = 1; break;
        case 0x1: divFrq = 2; break;
        case 0x2: divFrq = 3; break;
        case 0x3: divFrq = 4; break;
        case 0x4: divFrq = 5; break;
        case 0x5: divFrq = 6; break;
        case 0x6: divFrq = 7; break;
        case 0x7: divFrq = 8; break;
    }
    return baseFrq * multFrq / divFrq;
}
static int codec_dac_to_aud(CODECState *pState, int dacnum, audsettings_t *paud)
{
    uint32_t dacfmt = pState->pNodes[dacnum].dac.u32A_param;
    paud->freq = 44100;//codecFrequencyCalculate(dacfmt);
    paud->nchannels = 2;//CODEC_DAC_CHANELS(dacfmt);
    paud->fmt = AUD_FMT_U16;

    paud->endianness = 0;
    return VINF_SUCCESS;
}

static void pi_callback (void *opaque, int avail)
{
    CODECState *pState = (CODECState *)opaque;
    pState->pfnTransfer(pState, PI_INDEX, avail);
}

static void po_callback (void *opaque, int avail)
{
    CODECState *pState = (CODECState *)opaque;
    pState->pfnTransfer(pState, PO_INDEX, avail);
}

static void mc_callback (void *opaque, int avail)
{
    CODECState *pState = (CODECState *)opaque;
    pState->pfnTransfer(pState, MC_INDEX, avail);
}
#define STAC9220_DAC_PI (0x2)
#define STAC9220_DAC_MC (0x3)
#define STAC9220_DAC_PO (0x4)
int stac9220Construct(CODECState *pState)
{
    audsettings_t as;
    pState->pVerbs = (CODECVERB *)&STAC9220VERB;
    pState->cVerbs = sizeof(STAC9220VERB)/sizeof(CODECVERB);
    pState->pfnLookup = codecLookup;
    pState->pNodes = (PCODECNODE)RTMemAllocZ(sizeof(CODECNODE) * STAC9220_NODE_COUNT);
    uint8_t i;
    for (i = 0; i < STAC9220_NODE_COUNT; ++i)
    {
        stac9220ResetNode(pState, i, &pState->pNodes[i]);
    }
    //** @todo r=michaln: Was this meant to be 'HDA' or something like that? (AC'97 was on ICH0)
    AUD_register_card ("ICH0", &pState->card);


    codec_dac_to_aud(pState, STAC9220_DAC_PI, &as);
    pState->voice_pi = AUD_open_in(&pState->card, pState->voice_pi, "hda.in", pState, pi_callback, &as);
    codec_dac_to_aud(pState, STAC9220_DAC_PO, &as);
    pState->voice_po = AUD_open_out(&pState->card, pState->voice_po, "hda.out", pState, po_callback, &as);
    codec_dac_to_aud(pState, STAC9220_DAC_MC, &as);
    pState->voice_mc = AUD_open_in(&pState->card, pState->voice_mc, "hda.mc", pState, mc_callback, &as);
    if (!pState->voice_pi)
        LogRel (("HDAcodec: WARNING: Unable to open PCM IN!\n"));
    if (!pState->voice_mc)
        LogRel (("HDAcodec: WARNING: Unable to open PCM MC!\n"));
    if (!pState->voice_po)
        LogRel (("HDAcodec: WARNING: Unable to open PCM OUT!\n"));
    int mute = 0;
    uint8_t lvol = 0x7f;
    uint8_t rvol = 0x7f;
    AUD_set_volume_out(pState->voice_po, mute, lvol, rvol);
    return VINF_SUCCESS;
}
int stac9220Destruct(CODECState *pCodecState)
{
    RTMemFree(pCodecState->pNodes);
    return VINF_SUCCESS;
}
