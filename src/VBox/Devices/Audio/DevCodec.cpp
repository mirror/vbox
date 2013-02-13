/* $Id$ */
/** @file
 * DevCodec - VBox ICH Intel HD Audio Codec.
 *
 * Implemented against "Intel I/O Controller Hub 6 (ICH6) High Definition
 * Audio / AC '97 - Programmer's Reference Manual (PRM)", document number
 * 302349-003.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/vmm/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/cpp/utils.h>

#include "VBoxDD.h"
extern "C" {
#include "audio.h"
}
#include "DevCodec.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** The F00 parameter length (in dwords). */
#define CODECNODE_F00_PARAM_LENGTH  20
/** The F02 parameter length (in dwords). */
#define CODECNODE_F02_PARAM_LENGTH  16

/**
 * Common (or core) codec node structure.
 */
typedef struct CODECCOMMONNODE
{
    /** Node id - 7 bit format */
    uint8_t         id;
    /** The node name. */
    char const     *pszName;
    /* PRM 5.3.6 */
    uint32_t au32F00_param[CODECNODE_F00_PARAM_LENGTH];
    uint32_t au32F02_param[CODECNODE_F02_PARAM_LENGTH];
} CODECCOMMONNODE;
typedef CODECCOMMONNODE *PCODECCOMMONNODE;
AssertCompile(CODECNODE_F00_PARAM_LENGTH == 20);  /* saved state */
AssertCompile(CODECNODE_F02_PARAM_LENGTH == 16); /* saved state */

/**
 * Compile time assertion on the expected node size.
 */
#define AssertNodeSize(a_Node, a_cParams) \
    AssertCompile((a_cParams) <= (60 + 6)); /* the max size - saved state */ \
    AssertCompile(   sizeof(a_Node) - sizeof(CODECCOMMONNODE)  \
                  == (((a_cParams) * sizeof(uint32_t) + sizeof(void *) - 1) & ~(sizeof(void *) - 1)) )

typedef struct ROOTCODECNODE
{
    CODECCOMMONNODE node;
} ROOTCODECNODE, *PROOTCODECNODE;
AssertNodeSize(ROOTCODECNODE, 0);

#define AMPLIFIER_SIZE 60
typedef uint32_t AMPLIFIER[AMPLIFIER_SIZE];
#define AMPLIFIER_IN    0
#define AMPLIFIER_OUT   1
#define AMPLIFIER_LEFT  1
#define AMPLIFIER_RIGHT 0
#define AMPLIFIER_REGISTER(amp, inout, side, index) ((amp)[30*(inout) + 15*(side) + (index)])
typedef struct DACNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F0d_param;
    uint32_t    u32F04_param;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F0c_param;

    uint32_t    u32A_param;
    AMPLIFIER B_params;

} DACNODE, *PDACNODE;
AssertNodeSize(DACNODE, 6 + 60);

typedef struct ADCNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F03_param;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;

    uint32_t    u32A_param;
    uint32_t    u32F01_param;
    AMPLIFIER   B_params;
} ADCNODE, *PADCNODE;
AssertNodeSize(DACNODE, 6 + 60);

typedef struct SPDIFOUTNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;
    uint32_t    u32F0d_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} SPDIFOUTNODE, *PSPDIFOUTNODE;
AssertNodeSize(SPDIFOUTNODE, 5 + 60);

typedef struct SPDIFINNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;
    uint32_t    u32F0d_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} SPDIFINNODE, *PSPDIFINNODE;
AssertNodeSize(SPDIFINNODE, 5 + 60);

typedef struct AFGCODECNODE
{
    CODECCOMMONNODE node;
    uint32_t  u32F05_param;
    uint32_t  u32F08_param;
    uint32_t  u32F20_param;
    uint32_t  u32F17_param;
} AFGCODECNODE, *PAFGCODECNODE;
AssertNodeSize(AFGCODECNODE, 4);

typedef struct PORTNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F07_param;
    uint32_t u32F08_param;
    uint32_t u32F09_param;
    uint32_t u32F01_param;
    uint32_t u32F1c_param;
    AMPLIFIER   B_params;
} PORTNODE, *PPORTNODE;
AssertNodeSize(PORTNODE, 5 + 60);

typedef struct DIGOUTNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F01_param;
    uint32_t u32F08_param;
    uint32_t u32F07_param;
    uint32_t u32F09_param;
    uint32_t u32F1c_param;
} DIGOUTNODE, *PDIGOUTNODE;
AssertNodeSize(DIGOUTNODE, 5);

typedef struct DIGINNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F05_param;
    uint32_t u32F07_param;
    uint32_t u32F08_param;
    uint32_t u32F09_param;
    uint32_t u32F0c_param;
    uint32_t u32F1c_param;
    uint32_t u32F1e_param;
} DIGINNODE, *PDIGINNODE;
AssertNodeSize(DIGINNODE, 7);

typedef struct ADCMUXNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F01_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} ADCMUXNODE, *PADCMUXNODE;
AssertNodeSize(ADCMUXNODE, 2 + 60);

typedef struct PCBEEPNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F07_param;
    uint32_t    u32F0a_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
    uint32_t    u32F1c_param;
} PCBEEPNODE, *PPCBEEPNODE;
AssertNodeSize(PCBEEPNODE, 3 + 60 + 1);

typedef struct CDNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F07_param;
    uint32_t u32F1c_param;
} CDNODE, *PCDNODE;
AssertNodeSize(CDNODE, 2);

typedef struct VOLUMEKNOBNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F08_param;
    uint32_t    u32F0f_param;
} VOLUMEKNOBNODE, *PVOLUMEKNOBNODE;
AssertNodeSize(VOLUMEKNOBNODE, 2);

typedef struct ADCVOLNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F0c_param;
    uint32_t    u32F01_param;
    uint32_t    u32A_params;
    AMPLIFIER   B_params;
} ADCVOLNODE, *PADCVOLNODE;
AssertNodeSize(ADCVOLNODE, 3 + 60);

typedef struct RESNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F07_param;
    uint32_t    u32F1c_param;
} RESNODE, *PRESNODE;
AssertNodeSize(RESNODE, 4);

/**
 * Used for the saved state.
 */
typedef struct CODECSAVEDSTATENODE
{
    CODECCOMMONNODE Core;
    uint32_t        au32Params[60 + 6];
} CODECSAVEDSTATENODE;
AssertNodeSize(CODECSAVEDSTATENODE, 60 + 6);

typedef union CODECNODE
{
    CODECCOMMONNODE node;
    ROOTCODECNODE   root;
    AFGCODECNODE    afg;
    DACNODE         dac;
    ADCNODE         adc;
    SPDIFOUTNODE    spdifout;
    SPDIFINNODE     spdifin;
    PORTNODE        port;
    DIGOUTNODE      digout;
    DIGINNODE       digin;
    ADCMUXNODE      adcmux;
    PCBEEPNODE      pcbeep;
    CDNODE          cdnode;
    VOLUMEKNOBNODE  volumeKnob;
    ADCVOLNODE      adcvol;
    RESNODE         reserved;
    CODECSAVEDSTATENODE SavedState;
} CODECNODE, *PCODECNODE;
AssertNodeSize(CODECNODE, 60 + 6);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* STAC9220 - Referenced thru STAC9220WIDGET in the constructor below. */
static uint8_t const g_abStac9220Ports[]      = { 0x0A, 0xB, 0xC, 0xD, 0xE, 0xF, 0};
static uint8_t const g_abStac9220Dacs[]       = { 0x02, 0x3, 0x4, 0x5, 0};
static uint8_t const g_abStac9220Adcs[]       = { 0x06, 0x7, 0};
static uint8_t const g_abStac9220SpdifOuts[]  = { 0x08, 0 };
static uint8_t const g_abStac9220SpdifIns[]   = { 0x09, 0 };
static uint8_t const g_abStac9220DigOutPins[] = { 0x10, 0 };
static uint8_t const g_abStac9220DigInPins[]  = { 0x11, 0 };
static uint8_t const g_abStac9220AdcVols[]    = { 0x17, 0x18, 0};
static uint8_t const g_abStac9220AdcMuxs[]    = { 0x12, 0x13, 0};
static uint8_t const g_abStac9220Pcbeeps[]    = { 0x14, 0 };
static uint8_t const g_abStac9220Cds[]        = { 0x15, 0 };
static uint8_t const g_abStac9220VolKnobs[]   = { 0x16, 0 };
static uint8_t const g_abStac9220Reserveds[]  = { 0x09, 0x19, 0x1a, 0x1b, 0 };


/** SSM description of a CODECNODE. */
static SSMFIELD const g_aCodecNodeFields[] =
{
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.id),
    SSMFIELD_ENTRY_PAD_HC_AUTO(3, 3),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F00_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F02_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, au32Params),
    SSMFIELD_ENTRY_TERM()
};

/** Backward compatibility with v1 of the CODECNODE. */
static SSMFIELD const g_aCodecNodeFieldsV1[] =
{
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.id),
    SSMFIELD_ENTRY_PAD_HC_AUTO(3, 7),
    SSMFIELD_ENTRY_OLD_HCPTR(Core.name),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F00_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F02_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, au32Params),
    SSMFIELD_ENTRY_TERM()
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int stac9220ResetNode(PHDACODEC pState, uint8_t nodenum, PCODECNODE pNode);



static int stac9220Construct(PHDACODEC pState)
{
    unconst(pState->cTotalNodes) = 0x1C;
    pState->pfnCodecNodeReset = stac9220ResetNode;
    pState->u16VendorId = 0x8384;
    pState->u16DeviceId = 0x7680;
    pState->u8BSKU = 0x76;
    pState->u8AssemblyId = 0x80;
    pState->paNodes = (PCODECNODE)RTMemAllocZ(sizeof(CODECNODE) * pState->cTotalNodes);
    pState->fInReset = false;
#define STAC9220WIDGET(type) pState->au8##type##s = g_abStac9220##type##s
    STAC9220WIDGET(Port);
    STAC9220WIDGET(Dac);
    STAC9220WIDGET(Adc);
    STAC9220WIDGET(AdcVol);
    STAC9220WIDGET(AdcMux);
    STAC9220WIDGET(Pcbeep);
    STAC9220WIDGET(SpdifIn);
    STAC9220WIDGET(SpdifOut);
    STAC9220WIDGET(DigInPin);
    STAC9220WIDGET(DigOutPin);
    STAC9220WIDGET(Cd);
    STAC9220WIDGET(VolKnob);
    STAC9220WIDGET(Reserved);
#undef STAC9220WIDGET
    unconst(pState->u8AdcVolsLineIn) = 0x17;
    unconst(pState->u8DacLineOut) = 0x2;

    return VINF_SUCCESS;
}

static int stac9220ResetNode(PHDACODEC pState, uint8_t nodenum, PCODECNODE pNode)
{
    pNode->node.id = nodenum;
    pNode->node.au32F00_param[0xF] = 0; /* Power statest Supported: are the same as AFG reports */
    switch (nodenum)
    {
        /* Root Node*/
        case 0:
            pNode->node.au32F00_param[0x02] = CODEC_MAKE_F00_02(0x1, 0x0, 0x34, 0x1); /* rev id */
            break;
        case 1:
            pNode->node.au32F00_param[0x08] = CODEC_MAKE_F00_08(1, 0xd, 0xd);
            pNode->node.au32F00_param[0x0C] = CODEC_MAKE_F00_0C(0x17)
                                            | CODEC_F00_0C_CAP_BALANCED_IO
                                            | CODEC_F00_0C_CAP_INPUT
                                            | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                            | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                            | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;//(17 << 8)|RT_BIT(6)|RT_BIT(5)|RT_BIT(2)|RT_BIT(1)|RT_BIT(0);
            pNode->node.au32F00_param[0x0B] = CODEC_F00_0B_PCM;
            pNode->node.au32F00_param[0x0D] = CODEC_MAKE_F00_0D(1, 0x5, 0xE, 0);//RT_BIT(31)|(0x5 << 16)|(0xE)<<8;
            pNode->node.au32F00_param[0x12] = RT_BIT(31)|(0x2 << 16)|(0x7f << 8)|0x7f;
            pNode->node.au32F00_param[0x11] = CODEC_MAKE_F00_11(1, 1, 0, 0, 4);//0xc0000004;
            pNode->node.au32F00_param[0x0F] = CODEC_F00_0F_D3|CODEC_F00_0F_D2|CODEC_F00_0F_D1|CODEC_F00_0F_D0;
            pNode->afg.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D2, CODEC_F05_D2);//0x2 << 4| 0x2; /* PS-Act: D3, PS->Set D3  */
            pNode->afg.u32F08_param = 0;
            pNode->afg.u32F17_param = 0;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            memset(pNode->dac.B_params, 0, AMPLIFIER_SIZE);
            pNode->dac.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//RT_BIT(14)|(0x1 << 4)|0x1; /* 441000Hz/16bit/2ch */

            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_LEFT, 0) = 0x7F | RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_RIGHT, 0) = 0x7F | RT_BIT(7);

            pNode->dac.node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 0xD, 0)
                                             | CODEC_F00_09_CAP_L_R_SWAP
                                             | CODEC_F00_09_CAP_POWER_CTRL
                                             | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                             | CODEC_F00_09_CAP_LSB;//(0xD << 16) | RT_BIT(11) |  RT_BIT(10) | RT_BIT(2) | RT_BIT(0);
            pNode->dac.u32F0c_param = 0;
            pNode->dac.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3);//0x3 << 4 | 0x3; /* PS-Act: D3, Set: D3  */
            break;
        case 6:
            pNode->node.au32F02_param[0] = 0x17;
            goto adc_init;
        case 7:
            pNode->node.au32F02_param[0] = 0x18;
        adc_init:
            pNode->adc.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//RT_BIT(14)|(0x1 << 3)|0x1; /* 441000Hz/16bit/2ch */
            pNode->adc.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 1);//RT_BIT(0);
            pNode->adc.u32F03_param = RT_BIT(0);
            pNode->adc.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3);//0x3 << 4 | 0x3; /* PS-Act: D3 Set: D3 */
            pNode->adc.u32F06_param = 0;
            pNode->adc.node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_INPUT, 0xD, 0)
                                             | CODEC_F00_09_CAP_POWER_CTRL
                                             | CODEC_F00_09_CAP_CONNECTION_LIST
                                             | CODEC_F00_09_CAP_PROC_WIDGET
                                             | CODEC_F00_09_CAP_LSB;//RT_BIT(20)| (0xd << 16) |  RT_BIT(10) | RT_BIT(8) | RT_BIT(6)| RT_BIT(0);
            break;
        case 8:
            pNode->spdifout.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//(1<<14)|(0x1<<4) | 0x1;
            pNode->spdifout.node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 0x4, 0)
                                                  | CODEC_F00_09_CAP_DIGITAL
                                                  | CODEC_F00_09_CAP_FMT_OVERRIDE
                                                  | CODEC_F00_09_CAP_LSB;//(4 << 16) | RT_BIT(9)|RT_BIT(4)|0x1;
            pNode->node.au32F00_param[0xa] = pState->paNodes[1].node.au32F00_param[0xA];
            pNode->spdifout.node.au32F00_param[0xB] = CODEC_F00_0B_PCM;
            pNode->spdifout.u32F06_param = 0;
            pNode->spdifout.u32F0d_param = 0;
            break;
        case 9:
            pNode->spdifin.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//(0x1<<4) | 0x1;
            pNode->spdifin.node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_INPUT, 0x4, 0)
                                                 | CODEC_F00_09_CAP_DIGITAL
                                                 | CODEC_F00_09_CAP_CONNECTION_LIST
                                                 | CODEC_F00_09_CAP_FMT_OVERRIDE
                                                 | CODEC_F00_09_CAP_LSB;//(0x1 << 20)|(4 << 16) | RT_BIT(9)| RT_BIT(8)|RT_BIT(4)|0x1;
            pNode->node.au32F00_param[0xA] = pState->paNodes[1].node.au32F00_param[0xA];
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 1);//RT_BIT(0);
            pNode->node.au32F02_param[0] = 0x11;
            pNode->spdifin.node.au32F00_param[0xB] = CODEC_F00_0B_PCM;
            pNode->spdifin.u32F06_param = 0;
            pNode->spdifin.u32F0d_param = 0;
            break;
        case 0xA:
            pNode->node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                           | CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_HP
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                           | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                           | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;//0x173f;
            pNode->node.au32F02_param[0] = 0x2;
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE
                                     | CODEC_F07_OUT_ENABLE;
            pNode->port.u32F08_param = 0;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_HP,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          CODEC_F1C_MISC_JACK_DETECT,
                                                          0x2, 0);//RT_MAKE_U32_FROM_U8(0x20, 0x40, 0x21, 0x02);
            goto port_init;
        case 0xB:
            pNode->node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                           | CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                           | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                           | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;//0x1737;
            pNode->node.au32F02_param[0] = 0x4;
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_INTERNAL|CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_BLACK,
                                                          CODEC_F1C_MISC_JACK_DETECT,
                                                          0x1, 0x1);//RT_MAKE_U32_FROM_U8(0x11, 0x60, 0x11, 0x01);
            goto port_init;
        case 0xC:
            pNode->node.au32F02_param[0] = 0x3;
            pNode->node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                           | CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                           | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                           | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;//0x1737;
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          0x0, 0x1, 0x0);//RT_MAKE_U32_FROM_U8(0x10, 0x40, 0x11, 0x01);
            goto port_init;
        case 0xD:
            pNode->node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                           | CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                           | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                           | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;//0x1737;
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE;
            pNode->node.au32F02_param[0] = 0x2;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_MIC,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_PINK,
                                                          0x0, 0x5, 0x0);//RT_MAKE_U32_FROM_U8(0x50, 0x90, 0xA1, 0x02); /* Microphone */
        port_init:
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1, CODEC_F09_ANALOG_NA);//RT_BIT(31)|0x7fffffff;
            pNode->port.u32F08_param = 0;
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0x0, 0)
                                         | CODEC_F00_09_CAP_CONNECTION_LIST
                                         | CODEC_F00_09_CAP_UNSOL
                                         | CODEC_F00_09_CAP_LSB;//(4 << 20)|RT_BIT(8)|RT_BIT(7)|RT_BIT(0);
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 1);//0x1;
            break;
        case 0xE:
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0x0, 0)
                                         | CODEC_F00_09_CAP_UNSOL
                                         | CODEC_F00_09_CAP_LSB;//(4 << 20)|RT_BIT(7)|RT_BIT(0);
            pNode->port.u32F08_param = 0;
            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT;//0x34;
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE;
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(0, CODEC_F09_ANALOG_NA);//0x7fffffff;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_LINE_OUT,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_BLUE,
                                                          0x0, 0x4, 0x0);//0x01013040;  /* Line Out */
            break;
        case 0xF:
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0x0, 0x0)
                                         | CODEC_F00_09_CAP_CONNECTION_LIST
                                         | CODEC_F00_09_CAP_UNSOL
                                         | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                         | CODEC_F00_09_CAP_LSB;//(4 << 20)|RT_BIT(8)|RT_BIT(7)|RT_BIT(2)|RT_BIT(0);
            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_OUTPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT
                                           /* | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                           | CODEC_F00_0C_CAP_IMPENDANCE_SENSE */;//0x37;
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 1);//0x1;
            pNode->port.u32F08_param = 0;
            pNode->port.u32F07_param = CODEC_F07_OUT_ENABLE
                                     | CODEC_F07_IN_ENABLE;
            if (!pState->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_INTERNAL,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_ORANGE,
                                                          0x0, 0x1, 0x2);//RT_MAKE_U32_FROM_U8(0x12, 0x60, 0x11, 0x01);
            pNode->node.au32F02_param[0] = 0x5;
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(0, CODEC_F09_ANALOG_NA);//0x7fffffff;
            break;
        case 0x10:
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0x0, 0x0)
                                         | CODEC_F00_09_CAP_DIGITAL
                                         | CODEC_F00_09_CAP_CONNECTION_LIST
                                         | CODEC_F00_09_CAP_LSB;//(4<<20)|RT_BIT(9)|RT_BIT(8)|RT_BIT(0);
            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_OUTPUT;//RT_BIT(4);
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 0x3);
            pNode->node.au32F02_param[0] = RT_MAKE_U32_FROM_U8(0x08, 0x17, 0x19, 0);
            if (!pState->fInReset)
                pNode->digout.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                            CODEC_F1C_LOCATION_REAR,
                                                            CODEC_F1C_DEVICE_SPDIF_OUT,
                                                            CODEC_F1C_CONNECTION_TYPE_DIN,
                                                            CODEC_F1C_COLOR_BLACK,
                                                            0x0, 0x3, 0x0);//RT_MAKE_U32_FROM_U8(0x30, 0x10, 0x45, 0x01);
            break;
        case 0x11:
            pNode->node.au32F00_param[9] = (4 << 20) | (3 << 16) | RT_BIT(10) | RT_BIT(9) | RT_BIT(7) | RT_BIT(0);
            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_EAPD
                                           | CODEC_F00_0C_CAP_INPUT
                                           | CODEC_F00_0C_CAP_PRESENSE_DETECT;//RT_BIT(16)| RT_BIT(5)|RT_BIT(2);
            pNode->digin.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3);//0x3 << 4 | 0x3; /* PS-Act: D3 -> D3 */
            pNode->digin.u32F07_param = 0;
            pNode->digin.u32F08_param = 0;
            pNode->digin.u32F09_param = 0;
            pNode->digin.u32F0c_param = 0;
            if (!pState->fInReset)
                pNode->digin.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                           CODEC_F1C_LOCATION_REAR,
                                                           CODEC_F1C_DEVICE_SPDIF_IN,
                                                           CODEC_F1C_CONNECTION_TYPE_OTHER_DIGITAL,
                                                           CODEC_F1C_COLOR_BLACK,
                                                           0x0, 0x6, 0x0);//(0x1 << 24) | (0xc5 << 16) | (0x10 << 8) | 0x60;
            break;
        case 0x12:
            pNode->adcmux.u32F01_param = 0;
            goto adcmux_init;
        case 0x13:
            pNode->adcmux.u32F01_param = 1;
            adcmux_init:
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_SELECTOR, 0x0, 0)
                                         | CODEC_F00_09_CAP_CONNECTION_LIST
                                         | CODEC_F00_09_CAP_AMP_FMT_OVERRIDE
                                         | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                         | CODEC_F00_09_CAP_LSB;//(3<<20)|RT_BIT(8)|RT_BIT(3)|RT_BIT(2)|RT_BIT(0);
            pNode->node.au32F00_param[0xe] = CODEC_MAKE_F00_0E(0, 0x7);
            pNode->node.au32F00_param[0x12] = (0x27 << 16)|(0x4 << 8);
            /* STAC 9220 v10 6.21-22.{4,5} both(left and right) out amplefiers inited with 0*/
            memset(pNode->adcmux.B_params, 0, AMPLIFIER_SIZE);
            pNode->node.au32F02_param[0] = RT_MAKE_U32_FROM_U8(0xe, 0x15, 0xf, 0xb);
            pNode->node.au32F02_param[4] = RT_MAKE_U32_FROM_U8(0xc, 0xd, 0xa, 0x0);
            break;
        case 0x14:
            pNode->node.au32F00_param[9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_BEEP_GEN, 0, 0)
                                         | CODEC_F00_09_CAP_AMP_FMT_OVERRIDE
                                         | CODEC_F00_09_CAP_OUT_AMP_PRESENT;//(7 << 20) | RT_BIT(3) | RT_BIT(2);
            pNode->node.au32F00_param[0x12] = (0x17 << 16)|(0x3 << 8)| 0x3;
            pNode->pcbeep.u32F0a_param = 0;
            memset(pNode->pcbeep.B_params, 0, AMPLIFIER_SIZE);
            break;
        case 0x15:
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                           | CODEC_F00_09_CAP_LSB;//(4 << 20)|RT_BIT(0);
            pNode->node.au32F00_param[0xc] = CODEC_F00_0C_CAP_INPUT;//RT_BIT(5);
            if (!pState->fInReset)
                pNode->cdnode.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_FIXED,
                                                            CODEC_F1C_LOCATION_INTERNAL,
                                                            CODEC_F1C_DEVICE_CD,
                                                            CODEC_F1C_CONNECTION_TYPE_ATAPI,
                                                            CODEC_F1C_COLOR_UNKNOWN,
                                                            0x0, 0x7, 0x0);//RT_MAKE_U32_FROM_U8(0x70, 0x0, 0x33, 0x90);
            break;
        case 0x16:
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_VOLUME_KNOB, 0x0, 0x0);//(0x6 << 20);
            pNode->node.au32F00_param[0x13] = RT_BIT(7)| 0x7F;
            pNode->node.au32F00_param[0xe] = CODEC_MAKE_F00_0E(0, 0x4);
            pNode->node.au32F02_param[0] = RT_MAKE_U32_FROM_U8(0x2, 0x3, 0x4, 0x5);
            pNode->volumeKnob.u32F08_param = 0;
            pNode->volumeKnob.u32F0f_param = 0x7f;
            break;
        case 0x17:
            pNode->node.au32F02_param[0] = 0x12;
            goto adcvol_init;
        case 0x18:
            pNode->node.au32F02_param[0] = 0x13;
        adcvol_init:
            memset(pNode->adcvol.B_params, 0, AMPLIFIER_SIZE);

            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_SELECTOR, 0, 0)
                                           | CODEC_F00_09_CAP_L_R_SWAP
                                           | CODEC_F00_09_CAP_CONNECTION_LIST
                                           | CODEC_F00_09_CAP_IN_AMP_PRESENT
                                           | CODEC_F00_09_CAP_LSB;//(0x3 << 20)|RT_BIT(11)|RT_BIT(8)|RT_BIT(1)|RT_BIT(0);
            pNode->node.au32F00_param[0xe] = CODEC_MAKE_F00_0E(0, 0x1);
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_LEFT, 0) = RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_RIGHT, 0) = RT_BIT(7);
            pNode->adcvol.u32F0c_param = 0;
            break;
        case 0x19:
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_VENDOR_DEFINED, 0x3, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_LSB;//(0xF << 20)|(0x3 << 16)|RT_BIT(9)|RT_BIT(0);
            break;
        case 0x1A:
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 0x3, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_LSB;//(0x3 << 16)|RT_BIT(9)|RT_BIT(0);
            break;
        case 0x1B:
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_CONNECTION_LIST
                                           | CODEC_F00_09_CAP_LSB;//(0x4 << 20)|RT_BIT(9)|RT_BIT(8)|RT_BIT(0);
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(0, 0x1);
            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_OUTPUT;//0x10;
            pNode->node.au32F02_param[0] = 0x1a;
            pNode->reserved.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_NO_PHYS,
                                                          CODEC_F1C_LOCATION_NA,
                                                          CODEC_F1C_DEVICE_LINE_OUT,
                                                          CODEC_F1C_CONNECTION_TYPE_UNKNOWN,
                                                          CODEC_F1C_COLOR_UNKNOWN,
                                                          0x0, 0x0, 0xf);//0x4000000f;
            break;
        default:
        break;
    }
    return VINF_SUCCESS;
}

/* generic */

#define DECLISNODEOFTYPE(type)                                                                  \
    DECLINLINE(int) hdaCodecIs##type##Node(PHDACODEC pState, uint8_t cNode)                     \
    {                                                                                           \
        Assert(pState->au8##type##s);                                                           \
        for (int i = 0; pState->au8##type##s[i] != 0; ++i)                                      \
            if (pState->au8##type##s[i] == cNode)                                               \
                return 1;                                                                       \
        return 0;                                                                               \
    }
/* hdaCodecIsPortNode */
DECLISNODEOFTYPE(Port)
/* hdaCodecIsDacNode */
DECLISNODEOFTYPE(Dac)
/* hdaCodecIsAdcVolNode */
DECLISNODEOFTYPE(AdcVol)
/* hdaCodecIsAdcNode */
DECLISNODEOFTYPE(Adc)
/* hdaCodecIsAdcMuxNode */
DECLISNODEOFTYPE(AdcMux)
/* hdaCodecIsPcbeepNode */
DECLISNODEOFTYPE(Pcbeep)
/* hdaCodecIsSpdifOutNode */
DECLISNODEOFTYPE(SpdifOut)
/* hdaCodecIsSpdifInNode */
DECLISNODEOFTYPE(SpdifIn)
/* hdaCodecIsDigInPinNode */
DECLISNODEOFTYPE(DigInPin)
/* hdaCodecIsDigOutPinNode */
DECLISNODEOFTYPE(DigOutPin)
/* hdaCodecIsCdNode */
DECLISNODEOFTYPE(Cd)
/* hdaCodecIsVolKnobNode */
DECLISNODEOFTYPE(VolKnob)
/* hdaCodecIsReservedNode */
DECLISNODEOFTYPE(Reserved)

static int hdaCodecToAudVolume(AMPLIFIER *pAmp, audmixerctl_t mt);

DECLINLINE(void) hdaCodecSetRegister(uint32_t *pu32Reg, uint32_t u32Cmd, uint8_t u8Offset, uint32_t mask)
{
    Assert((pu32Reg && u8Offset < 32));
    *pu32Reg &= ~(mask << u8Offset);
    *pu32Reg |= (u32Cmd & mask) << u8Offset;
}
DECLINLINE(void) hdaCodecSetRegisterU8(uint32_t *pu32Reg, uint32_t u32Cmd, uint8_t u8Offset)
{
    hdaCodecSetRegister(pu32Reg, u32Cmd, u8Offset, CODEC_VERB_8BIT_DATA);
}

DECLINLINE(void) hdaCodecSetRegisterU16(uint32_t *pu32Reg, uint32_t u32Cmd, uint8_t u8Offset)
{
    hdaCodecSetRegister(pu32Reg, u32Cmd, u8Offset, CODEC_VERB_16BIT_DATA);
}


/*
 * Verb processor functions.
 */

static DECLCALLBACK(int) vrbProcUnimplemented(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Log(("vrbProcUnimplemented: cmd(raw:%x: cad:%x, d:%c, nid:%x, verb:%x)\n", cmd,
        CODEC_CAD(cmd), CODEC_DIRECT(cmd) ? 'N' : 'Y', CODEC_NID(cmd), CODEC_VERBDATA(cmd)));
    *pResp = 0;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcBreak(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    int rc;
    rc = vrbProcUnimplemented(pState, cmd, pResp);
    *pResp |= CODEC_RESPONSE_UNSOLICITED;
    return rc;
}

/* B-- */
static DECLCALLBACK(int) vrbProcGetAmplifier(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    /* HDA spec 7.3.3.7 Note A */
    /* @todo: if index out of range response should be 0 */
    uint8_t u8Index = CODEC_GET_AMP_DIRECTION(cmd) == AMPLIFIER_OUT? 0 : CODEC_GET_AMP_INDEX(cmd);

    PCODECNODE pNode = &pState->paNodes[CODEC_NID(cmd)];
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->dac.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adcvol.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcMuxNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adcmux.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->pcbeep.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->port.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adc.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else{
        AssertMsgReturn(0, ("access to fields of %x need to be implemented\n", CODEC_NID(cmd)), VINF_SUCCESS);
    }
    return VINF_SUCCESS;
}

/* 3-- */
static DECLCALLBACK(int) vrbProcSetAmplifier(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    AMPLIFIER *pAmplifier = NULL;
    bool fIsLeft = false;
    bool fIsRight = false;
    bool fIsOut = false;
    bool fIsIn = false;
    uint8_t u8Index = 0;
    Assert((CODEC_CAD(cmd) == pState->id));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    PCODECNODE pNode = &pState->paNodes[CODEC_NID(cmd)];
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->dac.B_params;
    else if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->adcvol.B_params;
    else if (hdaCodecIsAdcMuxNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->adcmux.B_params;
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->pcbeep.B_params;
    else if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->port.B_params;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        pAmplifier = &pNode->adc.B_params;
    Assert(pAmplifier);
    if (pAmplifier)
    {
        fIsOut = CODEC_SET_AMP_IS_OUT_DIRECTION(cmd);
        fIsIn = CODEC_SET_AMP_IS_IN_DIRECTION(cmd);
        fIsRight = CODEC_SET_AMP_IS_RIGHT_SIDE(cmd);
        fIsLeft = CODEC_SET_AMP_IS_LEFT_SIDE(cmd);
        u8Index = CODEC_SET_AMP_INDEX(cmd);
        if (   (!fIsLeft && !fIsRight)
            || (!fIsOut && !fIsIn))
            return VINF_SUCCESS;
        if (fIsIn)
        {
            if (fIsLeft)
                hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_IN, AMPLIFIER_LEFT, u8Index), cmd, 0);
            if (fIsRight)
                hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_IN, AMPLIFIER_RIGHT, u8Index), cmd, 0);
        }
        if (fIsOut)
        {
            if (fIsLeft)
                hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_LEFT, u8Index), cmd, 0);
            if (fIsRight)
                hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_RIGHT, u8Index), cmd, 0);
        }
        if (CODEC_NID(cmd) == pState->u8DacLineOut)
            hdaCodecToAudVolume(pAmplifier, AUD_MIXER_VOLUME);
        if (CODEC_NID(cmd) == pState->u8AdcVolsLineIn) /* Microphone */
            hdaCodecToAudVolume(pAmplifier, AUD_MIXER_LINE_IN);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetParameter(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pState->id);
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F00_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F00_PARAM_LENGTH)
    {
        Log(("HdaCodec: invalid F00 parameter %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    *pResp = pState->paNodes[CODEC_NID(cmd)].node.au32F00_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F01 */
static DECLCALLBACK(int) vrbProcGetConSelectCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pState->id);
    Assert(CODEC_NID(cmd) < pState->cTotalNodes);
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcMuxNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    return VINF_SUCCESS;
}

/* 701 */
static DECLCALLBACK(int) vrbProcSetConSelectCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    uint32_t *pu32Reg = NULL;
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcMuxNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F07 */
static DECLCALLBACK(int) vrbProcGetPinCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsCdNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    else
        AssertMsgFailed(("Unsupported"));
    return VINF_SUCCESS;
}

/* 707 */
static DECLCALLBACK(int) vrbProcSetPinCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsCdNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (   hdaCodecIsReservedNode(pState, CODEC_NID(cmd))
             && CODEC_NID(cmd) == 0x1b)
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F08 */
static DECLCALLBACK(int) vrbProcGetUnsolicitedEnabled(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if ((cmd) == 1 /* AFG */)
        *pResp = pState->paNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 708 */
static DECLCALLBACK(int) vrbProcSetUnsolicitedEnabled(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    Assert(pu32Reg);
    if(pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F09 */
static DECLCALLBACK(int) vrbProcGetPinSense(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 709 */
static DECLCALLBACK(int) vrbProcSetPinSense(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F09_param;
    Assert(pu32Reg);
    if(pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetConnectionListEntry(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    *pResp = 0;
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F02_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F02_PARAM_LENGTH)
    {
        Log(("HdaCodec: access to invalid F02 index %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = pState->paNodes[CODEC_NID(cmd)].node.au32F02_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F03 */
static DECLCALLBACK(int) vrbProcGetProcessingState(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adc.u32F03_param;
    return VINF_SUCCESS;
}

/* 703 */
static DECLCALLBACK(int) vrbProcSetProcessingState(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
    {
        hdaCodecSetRegisterU8(&pState->paNodes[CODEC_NID(cmd)].adc.u32F03_param, cmd, 0);
    }
    return VINF_SUCCESS;
}

/* F0D */
static DECLCALLBACK(int) vrbProcGetDigitalConverter(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifout.u32F0d_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifin.u32F0d_param;
    return VINF_SUCCESS;
}

static int codecSetDigitalConverter(PHDACODEC pState, uint32_t cmd, uint8_t u8Offset, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pState->paNodes[CODEC_NID(cmd)].spdifout.u32F0d_param, cmd, u8Offset);
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pState->paNodes[CODEC_NID(cmd)].spdifin.u32F0d_param, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 70D */
static DECLCALLBACK(int) vrbProcSetDigitalConverter1(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    return codecSetDigitalConverter(pState, cmd, 0, pResp);
}

/* 70E */
static DECLCALLBACK(int) vrbProcSetDigitalConverter2(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    return codecSetDigitalConverter(pState, cmd, 8, pResp);
}

/* F20 */
static DECLCALLBACK(int) vrbProcGetSubId(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
    {
        *pResp = pState->paNodes[CODEC_NID(cmd)].afg.u32F20_param;
    }
    return VINF_SUCCESS;
}

static int codecSetSubIdX(PHDACODEC pState, uint32_t cmd, uint8_t u8Offset)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    if (CODEC_NID(cmd) == 0x1 /* AFG */)
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].afg.u32F20_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 720 */
static DECLCALLBACK(int) vrbProcSetSubId0(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pState, cmd, 0);
}

/* 721 */
static DECLCALLBACK(int) vrbProcSetSubId1(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pState, cmd, 8);
}

/* 722 */
static DECLCALLBACK(int) vrbProcSetSubId2(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pState, cmd, 16);
}

/* 723 */
static DECLCALLBACK(int) vrbProcSetSubId3(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pState, cmd, 24);
}

static DECLCALLBACK(int) vrbProcReset(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert(CODEC_NID(cmd) == 1 /* AFG */);
    if(   CODEC_NID(cmd) == 1 /* AFG */
       && pState->pfnCodecNodeReset)
    {
        uint8_t i;
        Log(("HdaCodec: enters reset\n"));
        Assert(pState->pfnCodecNodeReset);
        for (i = 0; i < pState->cTotalNodes; ++i)
        {
            pState->pfnCodecNodeReset(pState, i, &pState->paNodes[i]);
        }
        pState->fInReset = false;
        Log(("HdaCodec: exits reset\n"));
    }
    *pResp = 0;
    return VINF_SUCCESS;
}

/* F05 */
static DECLCALLBACK(int) vrbProcGetPowerState(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        *pResp = pState->paNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].reserved.u32F05_param;
    return VINF_SUCCESS;
}

/* 705 */

DECLINLINE(void) codecPropogatePowerState(uint32_t *pu32F05_param)
{
    Assert(pu32F05_param);
    if (!pu32F05_param)
        return;
    bool fReset = CODEC_F05_IS_RESET(*pu32F05_param);
    bool fStopOk = CODEC_F05_IS_STOPOK(*pu32F05_param);
    uint8_t u8SetPowerState = CODEC_F05_SET(*pu32F05_param);
    *pu32F05_param = CODEC_MAKE_F05(fReset, fStopOk, 0, u8SetPowerState, u8SetPowerState);
}

static DECLCALLBACK(int) vrbProcSetPowerState(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].reserved.u32F05_param;
    Assert((pu32Reg));
    if (!pu32Reg)
        return VINF_SUCCESS;

    bool fReset = CODEC_F05_IS_RESET(*pu32Reg);
    bool fStopOk = CODEC_F05_IS_STOPOK(*pu32Reg);

    if (CODEC_NID(cmd) != 1 /* AFG */)
    {
        /*
         * We shouldn't propogate actual power state, which actual for AFG
         */
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0,
                                  CODEC_F05_ACT(pState->paNodes[1].afg.u32F05_param),
                                  CODEC_F05_SET(cmd));
    }

    /* Propagate next power state only if AFG is on or verb modifies AFG power state */
    if (   CODEC_NID(cmd) == 1 /* AFG */
        || !CODEC_F05_ACT(pState->paNodes[1].afg.u32F05_param))
    {
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0, CODEC_F05_SET(cmd), CODEC_F05_SET(cmd));
        if (   CODEC_NID(cmd) == 1 /* AFG */
            && (CODEC_F05_SET(cmd)) == CODEC_F05_D0)
        {
            /* now we're powered on AFG and may propogate power states on nodes */
            const uint8_t *pu8NodeIndex = &pState->au8Dacs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pState->paNodes[*pu8NodeIndex].dac.u32F05_param);

            pu8NodeIndex = &pState->au8Adcs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pState->paNodes[*pu8NodeIndex].adc.u32F05_param);

            pu8NodeIndex = &pState->au8DigInPins[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pState->paNodes[*pu8NodeIndex].digin.u32F05_param);
        }
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetStreamId(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (CODEC_NID(cmd) == 0x1A)
        *pResp = pState->paNodes[CODEC_NID(cmd)].reserved.u32F06_param;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcSetStreamId(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32addr = NULL;
    *pResp = 0;
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        pu32addr = &pState->paNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        pu32addr = &pState->paNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        pu32addr = &pState->paNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        pu32addr = &pState->paNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        pu32addr = &pState->paNodes[CODEC_NID(cmd)].reserved.u32F06_param;
    Assert((pu32addr));
    if (pu32addr)
        hdaCodecSetRegisterU8(pu32addr, cmd, 0);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetConverterFormat(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].dac.u32A_param;
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adc.u32A_param;
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifout.u32A_param;
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].spdifin.u32A_param;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcSetConverterFormat(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pState->paNodes[CODEC_NID(cmd)].dac.u32A_param, cmd, 0);
    else if (hdaCodecIsAdcNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pState->paNodes[CODEC_NID(cmd)].adc.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifOutNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pState->paNodes[CODEC_NID(cmd)].spdifout.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifInNode(pState, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pState->paNodes[CODEC_NID(cmd)].spdifin.u32A_param, cmd, 0);
    return VINF_SUCCESS;
}

/* F0C */
static DECLCALLBACK(int) vrbProcGetEAPD_BTLEnabled(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    return VINF_SUCCESS;
}

/* 70C */
static DECLCALLBACK(int) vrbProcSetEAPD_BTLEnabled(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsAdcVolNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    *pResp = 0;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F0F */
static DECLCALLBACK(int) vrbProcGetVolumeKnobCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsVolKnobNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    return VINF_SUCCESS;
}

/* 70F */
static DECLCALLBACK(int) vrbProcSetVolumeKnobCtrl(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (hdaCodecIsVolKnobNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F17 */
static DECLCALLBACK(int) vrbProcGetGPIOUnsolisted(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    /* note: this is true for ALC885 */
    if (CODEC_NID(cmd) == 0x1 /* AFG */)
        *pResp = pState->paNodes[1].afg.u32F17_param;
    return VINF_SUCCESS;
}

/* 717 */
static DECLCALLBACK(int) vrbProcSetGPIOUnsolisted(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pState->paNodes[1].afg.u32F17_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F1C */
static DECLCALLBACK(int) vrbProcGetConfig(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsCdNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        *pResp = pState->paNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    return VINF_SUCCESS;
}

static int codecSetConfigX(PHDACODEC pState, uint32_t cmd, uint8_t u8Offset)
{
    Assert((CODEC_CAD(cmd) == pState->id));
    Assert((CODEC_NID(cmd) < pState->cTotalNodes));
    if (CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        Log(("HdaCodec: invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsCdNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        pu32Reg = &pState->paNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    Assert((pu32Reg));
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 71C */
static DECLCALLBACK(int) vrbProcSetConfig0(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pState, cmd, 0);
}

/* 71D */
static DECLCALLBACK(int) vrbProcSetConfig1(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pState, cmd, 8);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig2(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pState, cmd, 16);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig3(PHDACODEC pState, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pState, cmd, 24);
}


static int hdaCodecToAudVolume(AMPLIFIER *pAmp, audmixerctl_t mt)
{
    uint32_t dir = AMPLIFIER_OUT;
    switch (mt)
    {
        case AUD_MIXER_VOLUME:
        case AUD_MIXER_PCM:
            dir = AMPLIFIER_OUT;
            break;
        case AUD_MIXER_LINE_IN:
            dir = AMPLIFIER_IN;
            break;
    }
    int mute = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_LEFT, 0) & RT_BIT(7);
    mute |= AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_RIGHT, 0) & RT_BIT(7);
    mute >>=7;
    mute &= 0x1;
    uint8_t lVol = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_LEFT, 0) & 0x7f;
    uint8_t rVol = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_RIGHT, 0) & 0x7f;
    AUD_set_volume(mt, &mute, &lVol, &rVol);
    return VINF_SUCCESS;
}

/**
 * HDA codec verb map.
 * @todo Any reason not to use binary search here?
 */
static const CODECVERB g_aCodecVerbs[] =
{
/*     verb     | verb mask              | callback               */
/*   -----------  --------------------   -----------------------  */
    { 0x000F0000, CODEC_VERB_8BIT_CMD , vrbProcGetParameter           },
    { 0x000F0100, CODEC_VERB_8BIT_CMD , vrbProcGetConSelectCtrl       },
    { 0x00070100, CODEC_VERB_8BIT_CMD , vrbProcSetConSelectCtrl       },
    { 0x000F0600, CODEC_VERB_8BIT_CMD , vrbProcGetStreamId            },
    { 0x00070600, CODEC_VERB_8BIT_CMD , vrbProcSetStreamId            },
    { 0x000F0700, CODEC_VERB_8BIT_CMD , vrbProcGetPinCtrl             },
    { 0x00070700, CODEC_VERB_8BIT_CMD , vrbProcSetPinCtrl             },
    { 0x000F0800, CODEC_VERB_8BIT_CMD , vrbProcGetUnsolicitedEnabled  },
    { 0x00070800, CODEC_VERB_8BIT_CMD , vrbProcSetUnsolicitedEnabled  },
    { 0x000F0900, CODEC_VERB_8BIT_CMD , vrbProcGetPinSense            },
    { 0x00070900, CODEC_VERB_8BIT_CMD , vrbProcSetPinSense            },
    { 0x000F0200, CODEC_VERB_8BIT_CMD , vrbProcGetConnectionListEntry },
    { 0x000F0300, CODEC_VERB_8BIT_CMD , vrbProcGetProcessingState     },
    { 0x00070300, CODEC_VERB_8BIT_CMD , vrbProcSetProcessingState     },
    { 0x000F0D00, CODEC_VERB_8BIT_CMD , vrbProcGetDigitalConverter    },
    { 0x00070D00, CODEC_VERB_8BIT_CMD , vrbProcSetDigitalConverter1   },
    { 0x00070E00, CODEC_VERB_8BIT_CMD , vrbProcSetDigitalConverter2   },
    { 0x000F2000, CODEC_VERB_8BIT_CMD , vrbProcGetSubId               },
    { 0x00072000, CODEC_VERB_8BIT_CMD , vrbProcSetSubId0              },
    { 0x00072100, CODEC_VERB_8BIT_CMD , vrbProcSetSubId1              },
    { 0x00072200, CODEC_VERB_8BIT_CMD , vrbProcSetSubId2              },
    { 0x00072300, CODEC_VERB_8BIT_CMD , vrbProcSetSubId3              },
    { 0x0007FF00, CODEC_VERB_8BIT_CMD , vrbProcReset                  },
    { 0x000F0500, CODEC_VERB_8BIT_CMD , vrbProcGetPowerState          },
    { 0x00070500, CODEC_VERB_8BIT_CMD , vrbProcSetPowerState          },
    { 0x000F0C00, CODEC_VERB_8BIT_CMD , vrbProcGetEAPD_BTLEnabled     },
    { 0x00070C00, CODEC_VERB_8BIT_CMD , vrbProcSetEAPD_BTLEnabled     },
    { 0x000F0F00, CODEC_VERB_8BIT_CMD , vrbProcGetVolumeKnobCtrl      },
    { 0x00070F00, CODEC_VERB_8BIT_CMD , vrbProcSetVolumeKnobCtrl      },
    { 0x000F1700, CODEC_VERB_8BIT_CMD , vrbProcGetGPIOUnsolisted      },
    { 0x00071700, CODEC_VERB_8BIT_CMD , vrbProcSetGPIOUnsolisted      },
    { 0x000F1C00, CODEC_VERB_8BIT_CMD , vrbProcGetConfig              },
    { 0x00071C00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig0             },
    { 0x00071D00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig1             },
    { 0x00071E00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig2             },
    { 0x00071F00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig3             },
    { 0x000A0000, CODEC_VERB_16BIT_CMD, vrbProcGetConverterFormat     },
    { 0x00020000, CODEC_VERB_16BIT_CMD, vrbProcSetConverterFormat     },
    { 0x000B0000, CODEC_VERB_16BIT_CMD, vrbProcGetAmplifier           },
    { 0x00030000, CODEC_VERB_16BIT_CMD, vrbProcSetAmplifier           },
};

static int codecLookup(PHDACODEC pState, uint32_t cmd, PPFNCODECVERBPROCESSOR pfn)
{
    int rc = VINF_SUCCESS;
    Assert(CODEC_CAD(cmd) == pState->id);
    if (hdaCodecIsReservedNode(pState, CODEC_NID(cmd)))
        Log(("HdaCodec: cmd %x was addressed to reserved node\n", cmd));

    if (   CODEC_VERBDATA(cmd) == 0
        || CODEC_NID(cmd) >= pState->cTotalNodes)
    {
        *pfn = vrbProcUnimplemented;
        /// @todo r=michaln: There needs to be a counter to avoid log flooding (see e.g. DevRTC.cpp)
        Log(("HdaCodec: cmd %x was ignored\n", cmd));
        return VINF_SUCCESS;
    }

    for (int i = 0; i < pState->cVerbs; ++i)
    {
        if ((CODEC_VERBDATA(cmd) & pState->paVerbs[i].mask) == pState->paVerbs[i].verb)
        {
            *pfn = pState->paVerbs[i].pfn;
            return VINF_SUCCESS;
        }
    }

    *pfn = vrbProcUnimplemented;
    Log(("HdaCodec: callback for %x wasn't found\n", CODEC_VERBDATA(cmd)));
    return rc;
}

static void pi_callback(void *opaque, int avail)
{
    PHDACODEC pState = (PHDACODEC)opaque;
    pState->pfnTransfer(pState, PI_INDEX, avail);
}

static void po_callback(void *opaque, int avail)
{
    PHDACODEC pState = (PHDACODEC)opaque;
    pState->pfnTransfer(pState, PO_INDEX, avail);
}



/*
 * APIs exposed to DevHDA.
 */


/**
 *
 * routines open one of the voices (IN, OUT) with corresponding parameters.
 * this routine could be called from HDA on setting/resseting sound format.
 *
 * @todo Probably passed settings should be verified (if AFG's declared proposed
 *       format) before enabling.
 */
int hdaCodecOpenVoice(PHDACODEC pState, ENMSOUNDSOURCE enmSoundSource, audsettings_t *pAudioSettings)
{
    int rc = 0;
    Assert((pState && pAudioSettings));
    if (   !pState
        || !pAudioSettings)
        return -1;
    switch (enmSoundSource)
    {
        case PI_INDEX:
            pState->SwVoiceIn = AUD_open_in(&pState->card, pState->SwVoiceIn, "hda.in", pState, pi_callback, pAudioSettings);
            rc = pState->SwVoiceIn ? 0 : 1;
            break;
        case PO_INDEX:
            pState->SwVoiceOut = AUD_open_out(&pState->card, pState->SwVoiceOut, "hda.out", pState, po_callback, pAudioSettings);
            rc = pState->SwVoiceOut ? 0 : 1;
            break;
        default:
            return -1;
    }
    if (!rc)
        LogRel(("HdaCodec: can't open %s fmt(freq: %d)\n", enmSoundSource == PI_INDEX? "in" : "out", pAudioSettings->freq));
    return rc;
}


int hdaCodecSaveState(CODECState *pCodecState, PSSMHANDLE pSSM)
{
    AssertLogRelMsgReturn(pCodecState->cTotalNodes == 0x1c, ("cTotalNodes=%#x, should be 0x1c", pCodecState->cTotalNodes),
                          VERR_INTERNAL_ERROR);
    SSMR3PutU32(pSSM, pCodecState->cTotalNodes);
    for (unsigned idxNode = 0; idxNode < pCodecState->cTotalNodes; ++idxNode)
        SSMR3PutStructEx(pSSM, &pCodecState->paNodes[idxNode].SavedState, sizeof(pCodecState->paNodes[idxNode].SavedState),
                         0 /*fFlags*/, g_aCodecNodeFields, NULL /*pvUser*/);
    return VINF_SUCCESS;
}


int hdaCodecLoadState(CODECState *pCodecState, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PCSSMFIELD pFields;
    uint32_t   fFlags;
    switch (uVersion)
    {
        case HDA_SSM_VERSION_1:
            AssertReturn(pCodecState->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
            pFields = g_aCodecNodeFieldsV1;
            fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
            break;

        case HDA_SSM_VERSION_2:
        case HDA_SSM_VERSION_3:
            AssertReturn(pCodecState->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
            pFields = g_aCodecNodeFields;
            fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
            break;

        case HDA_SSM_VERSION:
        {
            uint32_t cNodes;
            int rc2 = SSMR3GetU32(pSSM, &cNodes);
            AssertRCReturn(rc2, rc2);
            if (cNodes != 0x1c)
                return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
            AssertReturn(pCodecState->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);

            pFields = g_aCodecNodeFields;
            fFlags  = 0;
            break;
        }

        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    for (unsigned idxNode = 0; idxNode < pCodecState->cTotalNodes; ++idxNode)
    {
        uint8_t idOld = pCodecState->paNodes[idxNode].SavedState.Core.id;
        int rc = SSMR3GetStructEx(pSSM, &pCodecState->paNodes[idxNode].SavedState,
                                  sizeof(pCodecState->paNodes[idxNode].SavedState),
                                  fFlags, pFields, NULL);
        if (RT_FAILURE(rc))
            return rc;
        AssertLogRelMsgReturn(idOld == pCodecState->paNodes[idxNode].SavedState.Core.id,
                              ("loaded %#x, expected \n", pCodecState->paNodes[idxNode].SavedState.Core.id, idOld),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    /*
     * Update stuff after changing the state.
     */
    if (hdaCodecIsDacNode(pCodecState, pCodecState->u8DacLineOut))
        hdaCodecToAudVolume(&pCodecState->paNodes[pCodecState->u8DacLineOut].dac.B_params, AUD_MIXER_VOLUME);
    else if (hdaCodecIsSpdifOutNode(pCodecState, pCodecState->u8DacLineOut))
        hdaCodecToAudVolume(&pCodecState->paNodes[pCodecState->u8DacLineOut].spdifout.B_params, AUD_MIXER_VOLUME);
    hdaCodecToAudVolume(&pCodecState->paNodes[pCodecState->u8AdcVolsLineIn].adcvol.B_params, AUD_MIXER_LINE_IN);

    return VINF_SUCCESS;
}


int hdaCodecDestruct(CODECState *pCodecState)
{
    RTMemFree(pCodecState->paNodes);
    pCodecState->paNodes = NULL;
    return VINF_SUCCESS;
}


int hdaCodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pState, PCFGMNODE pCfg)
{
    pState->paVerbs   = &g_aCodecVerbs[0];
    pState->cVerbs    = RT_ELEMENTS(g_aCodecVerbs);
    pState->pfnLookup = codecLookup;
    int rc = stac9220Construct(pState);
    AssertRC(rc);

    /* common root node initializers */
    pState->paNodes[0].node.au32F00_param[0] = CODEC_MAKE_F00_00(pState->u16VendorId, pState->u16DeviceId);
    pState->paNodes[0].node.au32F00_param[4] = CODEC_MAKE_F00_04(0x1, 0x1);
    /* common AFG node initializers */
    pState->paNodes[1].node.au32F00_param[4] = CODEC_MAKE_F00_04(0x2, pState->cTotalNodes - 2);
    pState->paNodes[1].node.au32F00_param[5] = CODEC_MAKE_F00_05(1, CODEC_F00_05_AFG);
    pState->paNodes[1].afg.u32F20_param = CODEC_MAKE_F20(pState->u16VendorId, pState->u8BSKU, pState->u8AssemblyId);

    //** @todo r=michaln: Was this meant to be 'HDA' or something like that? (AC'97 was on ICH0)
    AUD_register_card ("ICH0", &pState->card);

    /* 44.1 kHz */
    audsettings_t as;
    as.freq = 44100;
    as.nchannels = 2;
    as.fmt = AUD_FMT_S16;
    as.endianness = 0;

    pState->paNodes[1].node.au32F00_param[0xA] = CODEC_F00_0A_16_BIT;
    hdaCodecOpenVoice(pState, PI_INDEX, &as);
    hdaCodecOpenVoice(pState, PO_INDEX, &as);
    pState->paNodes[1].node.au32F00_param[0xA] |= CODEC_F00_0A_44_1KHZ;

    uint8_t i;
    Assert(pState->paNodes);
    Assert(pState->pfnCodecNodeReset);
    for (i = 0; i < pState->cTotalNodes; ++i)
    {
        pState->pfnCodecNodeReset(pState, i, &pState->paNodes[i]);
    }

    hdaCodecToAudVolume(&pState->paNodes[pState->u8DacLineOut].dac.B_params, AUD_MIXER_VOLUME);
    hdaCodecToAudVolume(&pState->paNodes[pState->u8AdcVolsLineIn].adcvol.B_params, AUD_MIXER_LINE_IN);

    /* If no host voices were created, then fallback to nul audio. */
    if (!AUD_is_host_voice_in_ok(pState->SwVoiceIn))
        LogRel (("HDA: WARNING: Unable to open PCM IN!\n"));
    if (!AUD_is_host_voice_out_ok(pState->SwVoiceOut))
        LogRel (("HDA: WARNING: Unable to open PCM OUT!\n"));

    if (   !AUD_is_host_voice_in_ok(pState->SwVoiceIn)
        && !AUD_is_host_voice_out_ok(pState->SwVoiceOut))
    {
        /* Was not able initialize *any* voice. Select the NULL audio driver instead */
        AUD_close_in  (&pState->card, pState->SwVoiceIn);
        AUD_close_out (&pState->card, pState->SwVoiceOut);
        pState->SwVoiceOut = NULL;
        pState->SwVoiceIn = NULL;
        AUD_init_null ();

        PDMDevHlpVMSetRuntimeError (pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
            N_ ("No audio devices could be opened. Selecting the NULL audio backend "
                "with the consequence that no sound is audible"));
    }
    else if (   !AUD_is_host_voice_in_ok(pState->SwVoiceIn)
             || !AUD_is_host_voice_out_ok(pState->SwVoiceOut))
    {
        char   szMissingVoices[128];
        size_t len = 0;
        if (!AUD_is_host_voice_in_ok(pState->SwVoiceIn))
            len = RTStrPrintf (szMissingVoices, sizeof(szMissingVoices), "PCM_in");
        if (!AUD_is_host_voice_out_ok(pState->SwVoiceOut))
            len += RTStrPrintf (szMissingVoices + len, sizeof(szMissingVoices) - len, len ? ", PCM_out" : "PCM_out");

        PDMDevHlpVMSetRuntimeError (pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
            N_ ("Some audio devices (%s) could not be opened. Guest applications generating audio "
                "output or depending on audio input may hang. Make sure your host audio device "
                "is working properly. Check the logfile for error messages of the audio "
                "subsystem"), szMissingVoices);
    }

    return VINF_SUCCESS;
}

