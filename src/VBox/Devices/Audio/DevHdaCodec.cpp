/* $Id$ */
/** @file
 * Intel HD Audio Controller Emulation - Codec, Sigmatel/IDT STAC9220.
 *
 * Implemented based on the Intel HD Audio specification and the
 * Sigmatel/IDT STAC9220 datasheet.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HDA_CODEC
#include <VBox/log.h>

#include <VBox/AssertGuest.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/cpp/utils.h>

#include "VBoxDD.h"
#include "AudioMixer.h"
#include "DevHda.h"
#include "DevHdaCodec.h"
#include "DevHdaCommon.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


#define AMPLIFIER_IN    0
#define AMPLIFIER_OUT   1
#define AMPLIFIER_LEFT  1
#define AMPLIFIER_RIGHT 0
#define AMPLIFIER_REGISTER(amp, inout, side, index) ((amp)[30*(inout) + 15*(side) + (index)])


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/* STAC9220 - Nodes IDs / names. */
#define STAC9220_NID_ROOT                                  0x0  /* Root node */
#define STAC9220_NID_AFG                                   0x1  /* Audio Configuration Group */
#define STAC9220_NID_DAC0                                  0x2  /* Out */
#define STAC9220_NID_DAC1                                  0x3  /* Out */
#define STAC9220_NID_DAC2                                  0x4  /* Out */
#define STAC9220_NID_DAC3                                  0x5  /* Out */
#define STAC9220_NID_ADC0                                  0x6  /* In */
#define STAC9220_NID_ADC1                                  0x7  /* In */
#define STAC9220_NID_SPDIF_OUT                             0x8  /* Out */
#define STAC9220_NID_SPDIF_IN                              0x9  /* In */
/** Also known as PIN_A. */
#define STAC9220_NID_PIN_HEADPHONE0                        0xA  /* In, Out */
#define STAC9220_NID_PIN_B                                 0xB  /* In, Out */
#define STAC9220_NID_PIN_C                                 0xC  /* In, Out */
/** Also known as PIN D. */
#define STAC9220_NID_PIN_HEADPHONE1                        0xD  /* In, Out */
#define STAC9220_NID_PIN_E                                 0xE  /* In */
#define STAC9220_NID_PIN_F                                 0xF  /* In, Out */
/** Also known as DIGOUT0. */
#define STAC9220_NID_PIN_SPDIF_OUT                         0x10 /* Out */
/** Also known as DIGIN. */
#define STAC9220_NID_PIN_SPDIF_IN                          0x11 /* In */
#define STAC9220_NID_ADC0_MUX                              0x12 /* In */
#define STAC9220_NID_ADC1_MUX                              0x13 /* In */
#define STAC9220_NID_PCBEEP                                0x14 /* Out */
#define STAC9220_NID_PIN_CD                                0x15 /* In */
#define STAC9220_NID_VOL_KNOB                              0x16
#define STAC9220_NID_AMP_ADC0                              0x17 /* In */
#define STAC9220_NID_AMP_ADC1                              0x18 /* In */
/* Only for STAC9221. */
#define STAC9221_NID_ADAT_OUT                              0x19 /* Out */
#define STAC9221_NID_I2S_OUT                               0x1A /* Out */
#define STAC9221_NID_PIN_I2S_OUT                           0x1B /* Out */

/** Number of total nodes emulated. */
#define STAC9221_NUM_NODES                                 0x1C


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef IN_RING3

/* STAC9220 - Referenced through STAC9220WIDGET in the constructor below. */
static uint8_t const g_abStac9220Ports[]      = { STAC9220_NID_PIN_HEADPHONE0, STAC9220_NID_PIN_B, STAC9220_NID_PIN_C, STAC9220_NID_PIN_HEADPHONE1, STAC9220_NID_PIN_E, STAC9220_NID_PIN_F, 0 };
static uint8_t const g_abStac9220Dacs[]       = { STAC9220_NID_DAC0, STAC9220_NID_DAC1, STAC9220_NID_DAC2, STAC9220_NID_DAC3, 0 };
static uint8_t const g_abStac9220Adcs[]       = { STAC9220_NID_ADC0, STAC9220_NID_ADC1, 0 };
static uint8_t const g_abStac9220SpdifOuts[]  = { STAC9220_NID_SPDIF_OUT, 0 };
static uint8_t const g_abStac9220SpdifIns[]   = { STAC9220_NID_SPDIF_IN, 0 };
static uint8_t const g_abStac9220DigOutPins[] = { STAC9220_NID_PIN_SPDIF_OUT, 0 };
static uint8_t const g_abStac9220DigInPins[]  = { STAC9220_NID_PIN_SPDIF_IN, 0 };
static uint8_t const g_abStac9220AdcVols[]    = { STAC9220_NID_AMP_ADC0, STAC9220_NID_AMP_ADC1, 0 };
static uint8_t const g_abStac9220AdcMuxs[]    = { STAC9220_NID_ADC0_MUX, STAC9220_NID_ADC1_MUX, 0 };
static uint8_t const g_abStac9220Pcbeeps[]    = { STAC9220_NID_PCBEEP, 0 };
static uint8_t const g_abStac9220Cds[]        = { STAC9220_NID_PIN_CD, 0 };
static uint8_t const g_abStac9220VolKnobs[]   = { STAC9220_NID_VOL_KNOB, 0 };
/* STAC 9221. */
/** @todo Is STAC9220_NID_SPDIF_IN really correct for reserved nodes? */
static uint8_t const g_abStac9220Reserveds[]  = { STAC9220_NID_SPDIF_IN, STAC9221_NID_ADAT_OUT, STAC9221_NID_I2S_OUT, STAC9221_NID_PIN_I2S_OUT, 0 };

/** SSM description of CODECCOMMONNODE. */
static SSMFIELD const g_aCodecNodeFields[] =
{
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.uID),
    SSMFIELD_ENTRY_PAD_HC_AUTO(3, 3),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F00_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F02_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, au32Params),
    SSMFIELD_ENTRY_TERM()
};

/** Backward compatibility with v1 of CODECCOMMONNODE. */
static SSMFIELD const g_aCodecNodeFieldsV1[] =
{
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.uID),
    SSMFIELD_ENTRY_PAD_HC_AUTO(3, 7),
    SSMFIELD_ENTRY_OLD_HCPTR(Core.name),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F00_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, Core.au32F02_param),
    SSMFIELD_ENTRY(     CODECSAVEDSTATENODE, au32Params),
    SSMFIELD_ENTRY_TERM()
};

#endif /* IN_RING3 */



#if 0 /* unused */
static DECLCALLBACK(void) stac9220DbgNodes(PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);
    uint8_t const cTotalNodes = RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes));
    for (uint8_t i = 1; i < cTotalNodes; i++)
    {
        PCODECNODE pNode = &pThis->aNodes[i];
        AMPLIFIER *pAmp = &pNode->dac.B_params;

        uint8_t lVol = AMPLIFIER_REGISTER(*pAmp, AMPLIFIER_OUT, AMPLIFIER_LEFT, 0) & 0x7f;
        uint8_t rVol = AMPLIFIER_REGISTER(*pAmp, AMPLIFIER_OUT, AMPLIFIER_RIGHT, 0) & 0x7f;

        pHlp->pfnPrintf(pHlp, "0x%x: lVol=%RU8, rVol=%RU8\n", i, lVol, rVol);
    }
}
#endif


/**
 * Resets a single node of the codec.
 *
 * @param   pThis               HDA codec of node to reset.
 * @param   uNID                Node ID to set node to.
 * @param   pNode               Node to reset.
 */
static void stac9220NodeReset(PHDACODEC pThis, uint8_t uNID, PCODECNODE pNode)
{
    LogFlowFunc(("NID=0x%x (%RU8)\n", uNID, uNID));

    if (   !pThis->fInReset
        && (   uNID != STAC9220_NID_ROOT
            && uNID != STAC9220_NID_AFG)
       )
    {
        RT_ZERO(pNode->node);
    }

    /* Set common parameters across all nodes. */
    pNode->node.uID = uNID;
    pNode->node.uSD = 0;

    switch (uNID)
    {
        /* Root node. */
        case STAC9220_NID_ROOT:
        {
            /* Set the revision ID. */
            pNode->root.node.au32F00_param[0x02] = CODEC_MAKE_F00_02(0x1, 0x0, 0x3, 0x4, 0x0, 0x1);
            break;
        }

        /*
         * AFG (Audio Function Group).
         */
        case STAC9220_NID_AFG:
        {
            pNode->afg.node.au32F00_param[0x08] = CODEC_MAKE_F00_08(1, 0xd, 0xd);
            /* We set the AFG's PCM capabitilies fixed to 16kHz, 22.5kHz + 44.1kHz, 16-bit signed. */
            pNode->afg.node.au32F00_param[0x0A] = CODEC_F00_0A_44_1KHZ          /* 44.1 kHz */
                                                | CODEC_F00_0A_44_1KHZ_1_2X     /* Messed up way of saying 22.05 kHz */
                                                | CODEC_F00_0A_48KHZ_1_3X       /* Messed up way of saying 16 kHz. */
                                                | CODEC_F00_0A_16_BIT;          /* 16-bit signed */
            /* Note! We do not set CODEC_F00_0A_48KHZ here because we end up with
                     S/PDIF output showing up in windows and it trying to configure
                     streams other than 0 and 4 and stuff going sideways in the
                     stream setup/removal area. */
            pNode->afg.node.au32F00_param[0x0B] = CODEC_F00_0B_PCM;
            pNode->afg.node.au32F00_param[0x0C] = CODEC_MAKE_F00_0C(0x17)
                                                | CODEC_F00_0C_CAP_BALANCED_IO
                                                | CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT
                                                | CODEC_F00_0C_CAP_TRIGGER_REQUIRED
                                                | CODEC_F00_0C_CAP_IMPENDANCE_SENSE;

            /* Default input amplifier capabilities. */
            pNode->node.au32F00_param[0x0D] = CODEC_MAKE_F00_0D(CODEC_AMP_CAP_MUTE,
                                                                CODEC_AMP_STEP_SIZE,
                                                                CODEC_AMP_NUM_STEPS,
                                                                CODEC_AMP_OFF_INITIAL);
            /* Default output amplifier capabilities. */
            pNode->node.au32F00_param[0x12] = CODEC_MAKE_F00_12(CODEC_AMP_CAP_MUTE,
                                                                CODEC_AMP_STEP_SIZE,
                                                                CODEC_AMP_NUM_STEPS,
                                                                CODEC_AMP_OFF_INITIAL);

            pNode->afg.node.au32F00_param[0x11] = CODEC_MAKE_F00_11(1, 1, 0, 0, 4);
            pNode->afg.node.au32F00_param[0x0F] = CODEC_F00_0F_D3
                                                | CODEC_F00_0F_D2
                                                | CODEC_F00_0F_D1
                                                | CODEC_F00_0F_D0;

            pNode->afg.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D2, CODEC_F05_D2); /* PS-Act: D2, PS->Set D2. */
            pNode->afg.u32F08_param = 0;
            pNode->afg.u32F17_param = 0;
            break;
        }

        /*
         * DACs.
         */
        case STAC9220_NID_DAC0: /* DAC0: Headphones 0 + 1 */
        case STAC9220_NID_DAC1: /* DAC1: PIN C */
        case STAC9220_NID_DAC2: /* DAC2: PIN B */
        case STAC9220_NID_DAC3: /* DAC3: PIN F */
        {
            pNode->dac.u32A_param = CODEC_MAKE_A(HDA_SDFMT_TYPE_PCM, HDA_SDFMT_BASE_44KHZ,
                                                 HDA_SDFMT_MULT_1X, HDA_SDFMT_DIV_2X, HDA_SDFMT_16_BIT,
                                                 HDA_SDFMT_CHAN_STEREO);

            /* 7.3.4.6: Audio widget capabilities. */
            pNode->dac.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 13, 0)
                                               | CODEC_F00_09_CAP_L_R_SWAP
                                               | CODEC_F00_09_CAP_POWER_CTRL
                                               | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                               | CODEC_F00_09_CAP_STEREO;

            /* Connection list; must be 0 if the only connection for the widget is
             * to the High Definition Audio Link. */
            pNode->dac.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 0 /* Entries */);

            pNode->dac.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3);

            RT_ZERO(pNode->dac.B_params);
            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_LEFT,  0) = 0x7F | RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->dac.B_params, AMPLIFIER_OUT, AMPLIFIER_RIGHT, 0) = 0x7F | RT_BIT(7);
            break;
        }

        /*
         * ADCs.
         */
        case STAC9220_NID_ADC0: /* Analog input. */
        {
            pNode->node.au32F02_param[0] = STAC9220_NID_AMP_ADC0;
            goto adc_init;
        }

        case STAC9220_NID_ADC1: /* Analog input (CD). */
        {
            pNode->node.au32F02_param[0] = STAC9220_NID_AMP_ADC1;

            /* Fall through is intentional. */
        adc_init:

            pNode->adc.u32A_param   = CODEC_MAKE_A(HDA_SDFMT_TYPE_PCM, HDA_SDFMT_BASE_44KHZ,
                                                   HDA_SDFMT_MULT_1X, HDA_SDFMT_DIV_2X, HDA_SDFMT_16_BIT,
                                                   HDA_SDFMT_CHAN_STEREO);

            pNode->adc.u32F03_param = RT_BIT(0);
            pNode->adc.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3); /* PS-Act: D3 Set: D3 */

            pNode->adc.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_INPUT, 0xD, 0)
                                               | CODEC_F00_09_CAP_POWER_CTRL
                                               | CODEC_F00_09_CAP_CONNECTION_LIST
                                               | CODEC_F00_09_CAP_PROC_WIDGET
                                               | CODEC_F00_09_CAP_STEREO;
            /* Connection list entries. */
            pNode->adc.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);
            break;
        }

        /*
         * SP/DIF In/Out.
         */
        case STAC9220_NID_SPDIF_OUT:
        {
            pNode->spdifout.u32A_param   = CODEC_MAKE_A(HDA_SDFMT_TYPE_PCM, HDA_SDFMT_BASE_44KHZ,
                                                        HDA_SDFMT_MULT_1X, HDA_SDFMT_DIV_2X, HDA_SDFMT_16_BIT,
                                                        HDA_SDFMT_CHAN_STEREO);
            pNode->spdifout.u32F06_param = 0;
            pNode->spdifout.u32F0d_param = 0;

            pNode->spdifout.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 4, 0)
                                                    | CODEC_F00_09_CAP_DIGITAL
                                                    | CODEC_F00_09_CAP_FMT_OVERRIDE
                                                    | CODEC_F00_09_CAP_STEREO;

            /* Use a fixed format from AFG. */
            pNode->spdifout.node.au32F00_param[0xA] = pThis->aNodes[STAC9220_NID_AFG].node.au32F00_param[0xA];
            pNode->spdifout.node.au32F00_param[0xB] = CODEC_F00_0B_PCM;
            break;
        }

        case STAC9220_NID_SPDIF_IN:
        {
            pNode->spdifin.u32A_param = CODEC_MAKE_A(HDA_SDFMT_TYPE_PCM, HDA_SDFMT_BASE_44KHZ,
                                                     HDA_SDFMT_MULT_1X, HDA_SDFMT_DIV_2X, HDA_SDFMT_16_BIT,
                                                     HDA_SDFMT_CHAN_STEREO);

            pNode->spdifin.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_INPUT, 4, 0)
                                                   | CODEC_F00_09_CAP_DIGITAL
                                                   | CODEC_F00_09_CAP_CONNECTION_LIST
                                                   | CODEC_F00_09_CAP_FMT_OVERRIDE
                                                   | CODEC_F00_09_CAP_STEREO;

            /* Use a fixed format from AFG. */
            pNode->spdifin.node.au32F00_param[0xA] = pThis->aNodes[STAC9220_NID_AFG].node.au32F00_param[0xA];
            pNode->spdifin.node.au32F00_param[0xB] = CODEC_F00_0B_PCM;

            /* Connection list entries. */
            pNode->spdifin.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);
            pNode->spdifin.node.au32F02_param[0]   = 0x11;
            break;
        }

        /*
         * PINs / Ports.
         */
        case STAC9220_NID_PIN_HEADPHONE0: /* Port A: Headphone in/out (front). */
        {
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(0 /*fPresent*/, CODEC_F09_ANALOG_NA);

            pNode->port.node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                                | CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT
                                                | CODEC_F00_0C_CAP_HEADPHONE_AMP
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT
                                                | CODEC_F00_0C_CAP_TRIGGER_REQUIRED;

            /* Connection list entry 0: Goes to DAC0. */
            pNode->port.node.au32F02_param[0]   = STAC9220_NID_DAC0;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_HP,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_1, 0x0 /* Seq */);
            goto port_init;
        }

        case STAC9220_NID_PIN_B: /* Port B: Rear CLFE (Center / Subwoofer). */
        {
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1 /*fPresent*/, CODEC_F09_ANALOG_NA);

            pNode->port.node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                                | CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT
                                                | CODEC_F00_0C_CAP_TRIGGER_REQUIRED;

            /* Connection list entry 0: Goes to DAC2. */
            pNode->port.node.au32F02_param[0]   = STAC9220_NID_DAC2;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_BLACK,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_0, 0x1 /* Seq */);
            goto port_init;
        }

        case STAC9220_NID_PIN_C: /* Rear Speaker. */
        {
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1 /*fPresent*/, CODEC_F09_ANALOG_NA);

            pNode->port.node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                                | CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT
                                                | CODEC_F00_0C_CAP_TRIGGER_REQUIRED;

            /* Connection list entry 0: Goes to DAC1. */
            pNode->port.node.au32F02_param[0x0] = STAC9220_NID_DAC1;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_0, 0x0 /* Seq */);
            goto port_init;
        }

        case STAC9220_NID_PIN_HEADPHONE1: /* Also known as PIN_D. */
        {
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1 /*fPresent*/, CODEC_F09_ANALOG_NA);

            pNode->port.node.au32F00_param[0xC] = CODEC_MAKE_F00_0C(0x17)
                                                | CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT
                                                | CODEC_F00_0C_CAP_HEADPHONE_AMP
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT
                                                | CODEC_F00_0C_CAP_TRIGGER_REQUIRED;

            /* Connection list entry 0: Goes to DAC1. */
            pNode->port.node.au32F02_param[0x0] = STAC9220_NID_DAC0;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_MIC,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_PINK,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_15, 0x0 /* Ignored */);
            /* Fall through is intentional. */

        port_init:

            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE
                                     | CODEC_F07_OUT_ENABLE;
            pNode->port.u32F08_param = 0;

            pNode->port.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                                | CODEC_F00_09_CAP_CONNECTION_LIST
                                                | CODEC_F00_09_CAP_UNSOL
                                                | CODEC_F00_09_CAP_STEREO;
            /* Connection list entries. */
            pNode->port.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);
            break;
        }

        case STAC9220_NID_PIN_E:
        {
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE;
            pNode->port.u32F08_param = 0;
            /* If Line in is reported as enabled, OS X sees no speakers! Windows does
             * not care either way, although Linux does.
             */
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(0 /* fPresent */, 0);

            pNode->port.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                                | CODEC_F00_09_CAP_UNSOL
                                                | CODEC_F00_09_CAP_STEREO;

            pNode->port.node.au32F00_param[0xC] = CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_PRESENCE_DETECT;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_LINE_IN,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_BLUE,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_4, 0x1 /* Seq */);
            break;
        }

        case STAC9220_NID_PIN_F:
        {
            pNode->port.u32F07_param = CODEC_F07_IN_ENABLE | CODEC_F07_OUT_ENABLE;
            pNode->port.u32F08_param = 0;
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1 /* fPresent */, CODEC_F09_ANALOG_NA);

            pNode->port.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                                | CODEC_F00_09_CAP_CONNECTION_LIST
                                                | CODEC_F00_09_CAP_UNSOL
                                                | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                                | CODEC_F00_09_CAP_STEREO;

            pNode->port.node.au32F00_param[0xC] = CODEC_F00_0C_CAP_INPUT
                                                | CODEC_F00_0C_CAP_OUTPUT;

            /* Connection list entry 0: Goes to DAC3. */
            pNode->port.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);
            pNode->port.node.au32F02_param[0x0] = STAC9220_NID_DAC3;

            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_INTERNAL,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_ORANGE,
                                                          CODEC_F1C_MISC_NONE,
                                                          CODEC_F1C_ASSOCIATION_GROUP_0, 0x2 /* Seq */);
            break;
        }

        case STAC9220_NID_PIN_SPDIF_OUT: /* Rear SPDIF Out. */
        {
            pNode->digout.u32F07_param = CODEC_F07_OUT_ENABLE;
            pNode->digout.u32F09_param = 0;

            pNode->digout.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                                  | CODEC_F00_09_CAP_DIGITAL
                                                  | CODEC_F00_09_CAP_CONNECTION_LIST
                                                  | CODEC_F00_09_CAP_STEREO;
            pNode->digout.node.au32F00_param[0xC] = CODEC_F00_0C_CAP_OUTPUT;

            /* Connection list entries. */
            pNode->digout.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 3 /* Entries */);
            pNode->digout.node.au32F02_param[0x0] = RT_MAKE_U32_FROM_U8(STAC9220_NID_SPDIF_OUT,
                                                                        STAC9220_NID_AMP_ADC0, STAC9221_NID_ADAT_OUT, 0);
            if (!pThis->fInReset)
                pNode->digout.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                            CODEC_F1C_LOCATION_REAR,
                                                            CODEC_F1C_DEVICE_SPDIF_OUT,
                                                            CODEC_F1C_CONNECTION_TYPE_DIN,
                                                            CODEC_F1C_COLOR_BLACK,
                                                            CODEC_F1C_MISC_NONE,
                                                            CODEC_F1C_ASSOCIATION_GROUP_2, 0x0 /* Seq */);
            break;
        }

        case STAC9220_NID_PIN_SPDIF_IN:
        {
            pNode->digin.u32F05_param = CODEC_MAKE_F05(0, 0, 0, CODEC_F05_D3, CODEC_F05_D3); /* PS-Act: D3 -> D3 */
            pNode->digin.u32F07_param = CODEC_F07_IN_ENABLE;
            pNode->digin.u32F08_param = 0;
            pNode->digin.u32F09_param = CODEC_MAKE_F09_DIGITAL(0, 0);
            pNode->digin.u32F0c_param = 0;

            pNode->digin.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 3, 0)
                                                 | CODEC_F00_09_CAP_POWER_CTRL
                                                 | CODEC_F00_09_CAP_DIGITAL
                                                 | CODEC_F00_09_CAP_UNSOL
                                                 | CODEC_F00_09_CAP_STEREO;

            pNode->digin.node.au32F00_param[0xC] = CODEC_F00_0C_CAP_EAPD
                                                 | CODEC_F00_0C_CAP_INPUT
                                                 | CODEC_F00_0C_CAP_PRESENCE_DETECT;
            if (!pThis->fInReset)
                pNode->digin.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                           CODEC_F1C_LOCATION_REAR,
                                                           CODEC_F1C_DEVICE_SPDIF_IN,
                                                           CODEC_F1C_CONNECTION_TYPE_OTHER_DIGITAL,
                                                           CODEC_F1C_COLOR_BLACK,
                                                           CODEC_F1C_MISC_NONE,
                                                           CODEC_F1C_ASSOCIATION_GROUP_5, 0x0 /* Seq */);
            break;
        }

        case STAC9220_NID_ADC0_MUX:
        {
            pNode->adcmux.u32F01_param = 0; /* Connection select control index (STAC9220_NID_PIN_E). */
            goto adcmux_init;
        }

        case STAC9220_NID_ADC1_MUX:
        {
            pNode->adcmux.u32F01_param = 1; /* Connection select control index (STAC9220_NID_PIN_CD). */
            /* Fall through is intentional. */

        adcmux_init:

            pNode->adcmux.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_SELECTOR, 0, 0)
                                                  | CODEC_F00_09_CAP_CONNECTION_LIST
                                                  | CODEC_F00_09_CAP_AMP_FMT_OVERRIDE
                                                  | CODEC_F00_09_CAP_OUT_AMP_PRESENT
                                                  | CODEC_F00_09_CAP_STEREO;

            pNode->adcmux.node.au32F00_param[0xD] = CODEC_MAKE_F00_0D(0, 27, 4, 0);

            /* Connection list entries. */
            pNode->adcmux.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 7 /* Entries */);
            pNode->adcmux.node.au32F02_param[0x0] = RT_MAKE_U32_FROM_U8(STAC9220_NID_PIN_E,
                                                                        STAC9220_NID_PIN_CD,
                                                                        STAC9220_NID_PIN_F,
                                                                        STAC9220_NID_PIN_B);
            pNode->adcmux.node.au32F02_param[0x4] = RT_MAKE_U32_FROM_U8(STAC9220_NID_PIN_C,
                                                                        STAC9220_NID_PIN_HEADPHONE1,
                                                                        STAC9220_NID_PIN_HEADPHONE0,
                                                                        0x0 /* Unused */);

            /* STAC 9220 v10 6.21-22.{4,5} both(left and right) out amplifiers initialized with 0. */
            RT_ZERO(pNode->adcmux.B_params);
            break;
        }

        case STAC9220_NID_PCBEEP:
        {
            pNode->pcbeep.u32F0a_param = 0;

            pNode->pcbeep.node.au32F00_param[0x9]  = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_BEEP_GEN, 0, 0)
                                                   | CODEC_F00_09_CAP_AMP_FMT_OVERRIDE
                                                   | CODEC_F00_09_CAP_OUT_AMP_PRESENT;
            pNode->pcbeep.node.au32F00_param[0xD]  = CODEC_MAKE_F00_0D(0, 17, 3, 3);

            RT_ZERO(pNode->pcbeep.B_params);
            break;
        }

        case STAC9220_NID_PIN_CD:
        {
            pNode->cdnode.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                                  | CODEC_F00_09_CAP_STEREO;
            pNode->cdnode.node.au32F00_param[0xC] = CODEC_F00_0C_CAP_INPUT;

            if (!pThis->fInReset)
                pNode->cdnode.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_FIXED,
                                                            CODEC_F1C_LOCATION_INTERNAL,
                                                            CODEC_F1C_DEVICE_CD,
                                                            CODEC_F1C_CONNECTION_TYPE_ATAPI,
                                                            CODEC_F1C_COLOR_UNKNOWN,
                                                            CODEC_F1C_MISC_NONE,
                                                            CODEC_F1C_ASSOCIATION_GROUP_4, 0x2 /* Seq */);
            break;
        }

        case STAC9220_NID_VOL_KNOB:
        {
            pNode->volumeKnob.u32F08_param = 0;
            pNode->volumeKnob.u32F0f_param = 0x7f;

            pNode->volumeKnob.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_VOLUME_KNOB, 0, 0);
            pNode->volumeKnob.node.au32F00_param[0xD] = RT_BIT(7) | 0x7F;

            /* Connection list entries. */
            pNode->volumeKnob.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 4 /* Entries */);
            pNode->volumeKnob.node.au32F02_param[0x0] = RT_MAKE_U32_FROM_U8(STAC9220_NID_DAC0,
                                                                            STAC9220_NID_DAC1,
                                                                            STAC9220_NID_DAC2,
                                                                            STAC9220_NID_DAC3);
            break;
        }

        case STAC9220_NID_AMP_ADC0: /* ADC0Vol */
        {
            pNode->adcvol.node.au32F02_param[0] = STAC9220_NID_ADC0_MUX;
            goto adcvol_init;
        }

        case STAC9220_NID_AMP_ADC1: /* ADC1Vol */
        {
            pNode->adcvol.node.au32F02_param[0] = STAC9220_NID_ADC1_MUX;
            /* Fall through is intentional. */

        adcvol_init:

            pNode->adcvol.node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_SELECTOR, 0, 0)
                                                  | CODEC_F00_09_CAP_L_R_SWAP
                                                  | CODEC_F00_09_CAP_CONNECTION_LIST
                                                  | CODEC_F00_09_CAP_IN_AMP_PRESENT
                                                  | CODEC_F00_09_CAP_STEREO;


            pNode->adcvol.node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);

            RT_ZERO(pNode->adcvol.B_params);
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_LEFT,  0) = RT_BIT(7);
            AMPLIFIER_REGISTER(pNode->adcvol.B_params, AMPLIFIER_IN, AMPLIFIER_RIGHT, 0) = RT_BIT(7);
            break;
        }

        /*
         * STAC9221 nodes.
         */

        case STAC9221_NID_ADAT_OUT:
        {
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_VENDOR_DEFINED, 3, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_STEREO;
            break;
        }

        case STAC9221_NID_I2S_OUT:
        {
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_AUDIO_OUTPUT, 3, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_STEREO;
            break;
        }

        case STAC9221_NID_PIN_I2S_OUT:
        {
            pNode->node.au32F00_param[0x9] = CODEC_MAKE_F00_09(CODEC_F00_09_TYPE_PIN_COMPLEX, 0, 0)
                                           | CODEC_F00_09_CAP_DIGITAL
                                           | CODEC_F00_09_CAP_CONNECTION_LIST
                                           | CODEC_F00_09_CAP_STEREO;

            pNode->node.au32F00_param[0xC] = CODEC_F00_0C_CAP_OUTPUT;

            /* Connection list entries. */
            pNode->node.au32F00_param[0xE] = CODEC_MAKE_F00_0E(CODEC_F00_0E_LIST_NID_SHORT, 1 /* Entries */);
            pNode->node.au32F02_param[0]   = STAC9221_NID_I2S_OUT;

            if (!pThis->fInReset)
                pNode->reserved.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_NO_PHYS,
                                                              CODEC_F1C_LOCATION_NA,
                                                              CODEC_F1C_DEVICE_LINE_OUT,
                                                              CODEC_F1C_CONNECTION_TYPE_UNKNOWN,
                                                              CODEC_F1C_COLOR_UNKNOWN,
                                                              CODEC_F1C_MISC_NONE,
                                                              CODEC_F1C_ASSOCIATION_GROUP_15, 0x0 /* Ignored */);
            break;
        }

        default:
            AssertMsgFailed(("Node %RU8 not implemented\n", uNID));
            break;
    }
}

/**
 * Resets the codec with all its connected nodes.
 *
 * @param   pThis               HDA codec to reset.
 */
static DECLCALLBACK(void) stac9220Reset(PHDACODEC pThis)
{
    AssertPtrReturnVoid(pThis->aNodes);

    LogRel(("HDA: Codec reset\n"));

    pThis->fInReset = true;

    uint8_t const cTotalNodes = (uint8_t)RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes));
    for (uint8_t i = 0; i < cTotalNodes; i++)
        stac9220NodeReset(pThis, i, &pThis->aNodes[i]);

    pThis->fInReset = false;
}

#ifdef IN_RING3

static int stac9220Construct(PHDACODEC pThis)
{
    pThis->u16VendorId  = 0x8384; /* SigmaTel */
    /*
     * Note: The Linux kernel uses "patch_stac922x" for the fixups,
     *       which in turn uses "ref922x_pin_configs" for the configuration
     *       defaults tweaking in sound/pci/hda/patch_sigmatel.c.
     */
    pThis->u16DeviceId  = 0x7680; /* STAC9221 A1 */
    pThis->u8BSKU       = 0x76;
    pThis->u8AssemblyId = 0x80;

    pThis->fInReset = false;

#define STAC9220WIDGET(type) memcpy(&pThis->au8##type##s, &g_abStac9220##type##s, sizeof(uint8_t) * RT_ELEMENTS(g_abStac9220##type##s));
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

    AssertCompile(STAC9221_NUM_NODES <= RT_ELEMENTS(pThis->aNodes));
    pThis->cTotalNodes = STAC9221_NUM_NODES;

    pThis->u8AdcVolsLineIn = STAC9220_NID_AMP_ADC0;
    pThis->u8DacLineOut    = STAC9220_NID_DAC1;

    /*
     * Initialize all codec nodes.
     * This is specific to the codec, so do this here.
     *
     * Note: Do *not* call stac9220Reset() here, as this would not
     *       initialize the node default configuration values then!
     */
    for (uint8_t i = 0; i < STAC9221_NUM_NODES; i++)
        stac9220NodeReset(pThis, i, &pThis->aNodes[i]);

    /* Common root node initializers. */
    pThis->aNodes[STAC9220_NID_ROOT].root.node.au32F00_param[0] = CODEC_MAKE_F00_00(pThis->u16VendorId, pThis->u16DeviceId);
    pThis->aNodes[STAC9220_NID_ROOT].root.node.au32F00_param[4] = CODEC_MAKE_F00_04(0x1, 0x1);

    /* Common AFG node initializers. */
    pThis->aNodes[STAC9220_NID_AFG].afg.node.au32F00_param[0x4] = CODEC_MAKE_F00_04(0x2, STAC9221_NUM_NODES - 2);
    pThis->aNodes[STAC9220_NID_AFG].afg.node.au32F00_param[0x5] = CODEC_MAKE_F00_05(1, CODEC_F00_05_AFG);
    pThis->aNodes[STAC9220_NID_AFG].afg.node.au32F00_param[0xA] = CODEC_F00_0A_44_1KHZ | CODEC_F00_0A_16_BIT;
    pThis->aNodes[STAC9220_NID_AFG].afg.u32F20_param = CODEC_MAKE_F20(pThis->u16VendorId, pThis->u8BSKU, pThis->u8AssemblyId);

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

/*
 * Some generic predicate functions.
 */

#define DECLISNODEOFTYPE(type)                                              \
    DECLINLINE(bool) hdaCodecIs##type##Node(PHDACODEC pThis, uint8_t cNode) \
    {                                                                       \
        Assert(pThis->au8##type##s);                                        \
        for (int i = 0; pThis->au8##type##s[i] != 0; ++i)                   \
            if (pThis->au8##type##s[i] == cNode)                            \
                return true;                                                \
        return false;                                                       \
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

#ifdef IN_RING3

/*
 * Misc helpers.
 */
static int hdaR3CodecToAudVolume(PHDACODECR3 pThisCC, PCODECNODE pNode, AMPLIFIER *pAmp, PDMAUDIOMIXERCTL enmMixerCtl)
{
    RT_NOREF(pNode);

    uint8_t iDir;
    switch (enmMixerCtl)
    {
        case PDMAUDIOMIXERCTL_VOLUME_MASTER:
        case PDMAUDIOMIXERCTL_FRONT:
            iDir = AMPLIFIER_OUT;
            break;
        case PDMAUDIOMIXERCTL_LINE_IN:
        case PDMAUDIOMIXERCTL_MIC_IN:
            iDir = AMPLIFIER_IN;
            break;
        default:
            AssertMsgFailedReturn(("Invalid mixer control %RU32\n", enmMixerCtl), VERR_INVALID_PARAMETER);
            break;
    }

    int iMute;
    iMute  = AMPLIFIER_REGISTER(*pAmp, iDir, AMPLIFIER_LEFT,  0) & RT_BIT(7);
    iMute |= AMPLIFIER_REGISTER(*pAmp, iDir, AMPLIFIER_RIGHT, 0) & RT_BIT(7);
    iMute >>=7;
    iMute &= 0x1;

    uint8_t bLeft = AMPLIFIER_REGISTER(*pAmp, iDir, AMPLIFIER_LEFT,  0) & 0x7f;
    uint8_t bRight = AMPLIFIER_REGISTER(*pAmp, iDir, AMPLIFIER_RIGHT, 0) & 0x7f;

    /*
     * The STAC9220 volume controls have 0 to -96dB attenuation range in 128 steps.
     * We have 0 to -96dB range in 256 steps. HDA volume setting of 127 must map
     * to 255 internally (0dB), while HDA volume setting of 0 (-96dB) should map
     * to 1 (rather than zero) internally.
     */
    bLeft = (bLeft + 1) * (2 * 255) / 256;
    bRight = (bRight + 1) * (2 * 255) / 256;

    PDMAUDIOVOLUME Vol;
    PDMAudioVolumeInitFromStereo(&Vol, RT_BOOL(iMute), bLeft, bRight);

    LogFunc(("[NID0x%02x] %RU8/%RU8%s\n", pNode->node.uID, bLeft, bRight, Vol.fMuted ? "- Muted!" : ""));
    LogRel2(("HDA: Setting volume for mixer control '%s' to %RU8/%RU8%s\n",
             PDMAudioMixerCtlGetName(enmMixerCtl), bLeft, bRight, Vol.fMuted ? "- Muted!" : ""));

    return pThisCC->pfnCbMixerSetVolume(pThisCC->pDevIns, enmMixerCtl, &Vol);
}

#endif /* IN_RING3 */

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
#if 0 /* unused */

static DECLCALLBACK(int) vrbProcUnimplemented(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThis, pThisCC, cmd);
    LogFlowFunc(("cmd(raw:%x: cad:%x, d:%c, nid:%x, verb:%x)\n", cmd,
                 CODEC_CAD(cmd), CODEC_DIRECT(cmd) ? 'N' : 'Y', CODEC_NID(cmd), CODEC_VERBDATA(cmd)));
    *pResp = 0;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcBreak(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    int rc;
    rc = vrbProcUnimplemented(pThis, pThisCC, cmd, pResp);
    *pResp |= CODEC_RESPONSE_UNSOLICITED;
    return rc;
}

#endif /* unused */


/* B-- */
static DECLCALLBACK(int) vrbProcGetAmplifier(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    /* HDA spec 7.3.3.7 Note A */
    /** @todo If index out of range response should be 0. */
    uint8_t u8Index = CODEC_GET_AMP_DIRECTION(cmd) == AMPLIFIER_OUT ? 0 : CODEC_GET_AMP_INDEX(cmd);

    PCODECNODE pNode = &pThis->aNodes[CODEC_NID(cmd)];
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->dac.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adcvol.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adcmux.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->pcbeep.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->port.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = AMPLIFIER_REGISTER(pNode->adc.B_params,
                            CODEC_GET_AMP_DIRECTION(cmd),
                            CODEC_GET_AMP_SIDE(cmd),
                            u8Index);
    else
        LogRel2(("HDA: Warning: Unhandled get amplifier command: 0x%x (NID=0x%x [%RU8])\n", cmd, CODEC_NID(cmd), CODEC_NID(cmd)));

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetParameter(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F00_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F00_PARAM_LENGTH)
    {
        *pResp = 0;

        LogFlowFunc(("invalid F00 parameter %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }

    *pResp = pThis->aNodes[CODEC_NID(cmd)].node.au32F00_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F01 */
static DECLCALLBACK(int) vrbProcGetConSelectCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    else
        LogRel2(("HDA: Warning: Unhandled get connection select control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 701 */
static DECLCALLBACK(int) vrbProcSetConSelectCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    else
        LogRel2(("HDA: Warning: Unhandled set connection select control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F07 */
static DECLCALLBACK(int) vrbProcGetPinCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    else
        LogRel2(("HDA: Warning: Unhandled get pin control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 707 */
static DECLCALLBACK(int) vrbProcSetPinCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (   hdaCodecIsReservedNode(pThis, CODEC_NID(cmd))
             && CODEC_NID(cmd) == 0x1b)
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    else
        LogRel2(("HDA: Warning: Unhandled set pin control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F08 */
static DECLCALLBACK(int) vrbProcGetUnsolicitedEnabled(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if ((cmd) == STAC9220_NID_AFG)
        *pResp = pThis->aNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else
        LogRel2(("HDA: Warning: Unhandled get unsolicited enabled command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 708 */
static DECLCALLBACK(int) vrbProcSetUnsolicitedEnabled(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else
        LogRel2(("HDA: Warning: Unhandled set unsolicited enabled command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F09 */
static DECLCALLBACK(int) vrbProcGetPinSense(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
    {
        AssertFailed();
        LogRel2(("HDA: Warning: Unhandled get pin sense command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));
    }

    return VINF_SUCCESS;
}

/* 709 */
static DECLCALLBACK(int) vrbProcSetPinSense(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
        LogRel2(("HDA: Warning: Unhandled set pin sense command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetConnectionListEntry(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F02_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F02_PARAM_LENGTH)
    {
        LogFlowFunc(("access to invalid F02 index %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = pThis->aNodes[CODEC_NID(cmd)].node.au32F02_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F03 */
static DECLCALLBACK(int) vrbProcGetProcessingState(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adc.u32F03_param;

    return VINF_SUCCESS;
}

/* 703 */
static DECLCALLBACK(int) vrbProcSetProcessingState(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->aNodes[CODEC_NID(cmd)].adc.u32F03_param, cmd, 0);
    return VINF_SUCCESS;
}

/* F0D */
static DECLCALLBACK(int) vrbProcGetDigitalConverter(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F0d_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F0d_param;

    return VINF_SUCCESS;
}

static int codecSetDigitalConverter(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset, uint64_t *pResp)
{
    *pResp = 0;

    if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F0d_param, cmd, u8Offset);
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F0d_param, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 70D */
static DECLCALLBACK(int) vrbProcSetDigitalConverter1(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    return codecSetDigitalConverter(pThis, cmd, 0, pResp);
}

/* 70E */
static DECLCALLBACK(int) vrbProcSetDigitalConverter2(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    return codecSetDigitalConverter(pThis, cmd, 8, pResp);
}

/* F20 */
static DECLCALLBACK(int) vrbProcGetSubId(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    Assert(CODEC_CAD(cmd) == pThis->id);
    uint8_t const cTotalNodes = (uint8_t)RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes));
    Assert(CODEC_NID(cmd) < cTotalNodes);
    if (CODEC_NID(cmd) >= cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        *pResp = 0;
        return VINF_SUCCESS;
    }
    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        *pResp = pThis->aNodes[CODEC_NID(cmd)].afg.u32F20_param;
    else
        *pResp = 0;
    return VINF_SUCCESS;
}

static int codecSetSubIdX(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    uint8_t const cTotalNodes = (uint8_t)RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes));
    Assert(CODEC_NID(cmd) < cTotalNodes);
    if (CODEC_NID(cmd) >= cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg;
    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].afg.u32F20_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 720 */
static DECLCALLBACK(int) vrbProcSetSubId0(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 0);
}

/* 721 */
static DECLCALLBACK(int) vrbProcSetSubId1(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 8);
}

/* 722 */
static DECLCALLBACK(int) vrbProcSetSubId2(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 16);
}

/* 723 */
static DECLCALLBACK(int) vrbProcSetSubId3(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 24);
}

static DECLCALLBACK(int) vrbProcReset(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    Assert(CODEC_CAD(cmd) == pThis->id);

    if (pThis->enmType == CODEC_TYPE_STAC9220)
    {
        Assert(CODEC_NID(cmd) == STAC9220_NID_AFG);

        if (CODEC_NID(cmd) == STAC9220_NID_AFG)
            stac9220Reset(pThis);
    }
    else
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);

    *pResp = 0;
    return VINF_SUCCESS;
}

/* F05 */
static DECLCALLBACK(int) vrbProcGetPowerState(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        *pResp = pThis->aNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digout.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].reserved.u32F05_param;
    else
        LogRel2(("HDA: Warning: Unhandled get power state command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    LogFunc(("[NID0x%02x]: fReset=%RTbool, fStopOk=%RTbool, Act=D%RU8, Set=D%RU8\n",
             CODEC_NID(cmd), CODEC_F05_IS_RESET(*pResp), CODEC_F05_IS_STOPOK(*pResp), CODEC_F05_ACT(*pResp), CODEC_F05_SET(*pResp)));
    return VINF_SUCCESS;
}

/* 705 */
#if 1
static DECLCALLBACK(int) vrbProcSetPowerState(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digout.u32F05_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].reserved.u32F05_param;
    else
    {
        LogRel2(("HDA: Warning: Unhandled set power state command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));
    }

    if (!pu32Reg)
        return VINF_SUCCESS;

    uint8_t uPwrCmd = CODEC_F05_SET      (cmd);
    bool    fReset  = CODEC_F05_IS_RESET (*pu32Reg);
    bool    fStopOk = CODEC_F05_IS_STOPOK(*pu32Reg);
#ifdef LOG_ENABLED
    bool    fError  = CODEC_F05_IS_ERROR (*pu32Reg);
    uint8_t uPwrAct = CODEC_F05_ACT      (*pu32Reg);
    uint8_t uPwrSet = CODEC_F05_SET      (*pu32Reg);
    LogFunc(("[NID0x%02x] Cmd=D%RU8, fReset=%RTbool, fStopOk=%RTbool, fError=%RTbool, uPwrAct=D%RU8, uPwrSet=D%RU8\n",
             CODEC_NID(cmd), uPwrCmd, fReset, fStopOk, fError, uPwrAct, uPwrSet));
    LogFunc(("AFG: Act=D%RU8, Set=D%RU8\n",
            CODEC_F05_ACT(pThis->aNodes[STAC9220_NID_AFG].afg.u32F05_param),
            CODEC_F05_SET(pThis->aNodes[STAC9220_NID_AFG].afg.u32F05_param)));
#endif

    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0, uPwrCmd /* PS-Act */, uPwrCmd /* PS-Set */);

    const uint8_t uAFGPwrAct = CODEC_F05_ACT(pThis->aNodes[STAC9220_NID_AFG].afg.u32F05_param);
    if (uAFGPwrAct == CODEC_F05_D0) /* Only propagate power state if AFG is on (D0). */
    {
        /* Propagate to all other nodes under this AFG. */
        LogFunc(("Propagating Act=D%RU8 (AFG), Set=D%RU8 to all AFG child nodes ...\n", uAFGPwrAct, uPwrCmd));

#define PROPAGATE_PWR_STATE(_aList, _aMember) \
        { \
            const uint8_t *pu8NodeIndex = &_aList[0]; \
            while (*(++pu8NodeIndex)) \
            { \
                pThis->aNodes[*pu8NodeIndex]._aMember.u32F05_param = \
                    CODEC_MAKE_F05(fReset, fStopOk, 0, uAFGPwrAct, uPwrCmd); \
                LogFunc(("\t[NID0x%02x]: Act=D%RU8, Set=D%RU8\n", *pu8NodeIndex, \
                         CODEC_F05_ACT(pThis->aNodes[*pu8NodeIndex]._aMember.u32F05_param), \
                         CODEC_F05_SET(pThis->aNodes[*pu8NodeIndex]._aMember.u32F05_param))); \
            } \
        }

        PROPAGATE_PWR_STATE(pThis->au8Dacs,       dac);
        PROPAGATE_PWR_STATE(pThis->au8Adcs,       adc);
        PROPAGATE_PWR_STATE(pThis->au8DigInPins,  digin);
        PROPAGATE_PWR_STATE(pThis->au8DigOutPins, digout);
        PROPAGATE_PWR_STATE(pThis->au8SpdifIns,   spdifin);
        PROPAGATE_PWR_STATE(pThis->au8SpdifOuts,  spdifout);
        PROPAGATE_PWR_STATE(pThis->au8Reserveds,  reserved);

#undef PROPAGATE_PWR_STATE
    }
    /*
     * If this node is a reqular node (not the AFG one), adopt PS-Set of the AFG node
     * as PS-Set of this node. PS-Act always is one level under PS-Set here.
     */
    else
    {
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0, uAFGPwrAct, uPwrCmd);
    }

    LogFunc(("[NID0x%02x] fReset=%RTbool, fStopOk=%RTbool, Act=D%RU8, Set=D%RU8\n",
             CODEC_NID(cmd),
             CODEC_F05_IS_RESET(*pu32Reg), CODEC_F05_IS_STOPOK(*pu32Reg), CODEC_F05_ACT(*pu32Reg), CODEC_F05_SET(*pu32Reg)));

    return VINF_SUCCESS;
}
#else
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

static DECLCALLBACK(int) vrbProcSetPowerState(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    Assert(CODEC_CAD(cmd) == pThis->id);
    uint8_t const cTotalNodes = (uint8_t)RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes));
    Assert(CODEC_NID(cmd) < cTotalNodes);
    if (CODEC_NID(cmd) >= cTotalNodes)
    {
        *pResp = 0;
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].reserved.u32F05_param;
    else
        AssertFailedReturn(VINF_SUCCESS);

    bool fReset = CODEC_F05_IS_RESET(*pu32Reg);
    bool fStopOk = CODEC_F05_IS_STOPOK(*pu32Reg);

    if (CODEC_NID(cmd) != 1 /* AFG */)
    {
        /*
         * We shouldn't propogate actual power state, which actual for AFG
         */
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0,
                                  CODEC_F05_ACT(pThis->aNodes[1].afg.u32F05_param),
                                  CODEC_F05_SET(cmd));
    }

    /* Propagate next power state only if AFG is on or verb modifies AFG power state */
    if (   CODEC_NID(cmd) == 1 /* AFG */
        || !CODEC_F05_ACT(pThis->aNodes[1].afg.u32F05_param))
    {
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0, CODEC_F05_SET(cmd), CODEC_F05_SET(cmd));
        if (   CODEC_NID(cmd) == 1 /* AFG */
            && (CODEC_F05_SET(cmd)) == CODEC_F05_D0)
        {
            /* now we're powered on AFG and may propogate power states on nodes */
            const uint8_t *pu8NodeIndex = &pThis->au8Dacs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->aNodes[*pu8NodeIndex].dac.u32F05_param);

            pu8NodeIndex = &pThis->au8Adcs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->aNodes[*pu8NodeIndex].adc.u32F05_param);

            pu8NodeIndex = &pThis->au8DigInPins[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->aNodes[*pu8NodeIndex].digin.u32F05_param);
        }
    }
    return VINF_SUCCESS;
}
#endif

/* F06 */
static DECLCALLBACK(int) vrbProcGetStreamId(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (CODEC_NID(cmd) == STAC9221_NID_I2S_OUT)
        *pResp = pThis->aNodes[CODEC_NID(cmd)].reserved.u32F06_param;
    else
        LogRel2(("HDA: Warning: Unhandled get stream ID command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    LogFlowFunc(("[NID0x%02x] Stream ID=%RU8, channel=%RU8\n",
                 CODEC_NID(cmd), CODEC_F00_06_GET_STREAM_ID(cmd), CODEC_F00_06_GET_CHANNEL_ID(cmd)));

    return VINF_SUCCESS;
}

/* A0 */
static DECLCALLBACK(int) vrbProcGetConverterFormat(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].dac.u32A_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adc.u32A_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifout.u32A_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].spdifin.u32A_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].reserved.u32A_param;
    else
        LogRel2(("HDA: Warning: Unhandled get converter format command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* Also see section 3.7.1. */
static DECLCALLBACK(int) vrbProcSetConverterFormat(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->aNodes[CODEC_NID(cmd)].dac.u32A_param, cmd, 0);
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->aNodes[CODEC_NID(cmd)].adc.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->aNodes[CODEC_NID(cmd)].spdifout.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->aNodes[CODEC_NID(cmd)].spdifin.u32A_param, cmd, 0);
    else
        LogRel2(("HDA: Warning: Unhandled set converter format command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* F0C */
static DECLCALLBACK(int) vrbProcGetEAPD_BTLEnabled(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    else
        LogRel2(("HDA: Warning: Unhandled get EAPD/BTL enabled command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 70C */
static DECLCALLBACK(int) vrbProcSetEAPD_BTLEnabled(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    else
        LogRel2(("HDA: Warning: Unhandled set EAPD/BTL enabled command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F0F */
static DECLCALLBACK(int) vrbProcGetVolumeKnobCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    else
        LogRel2(("HDA: Warning: Unhandled get volume knob control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 70F */
static DECLCALLBACK(int) vrbProcSetVolumeKnobCtrl(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    else
        LogRel2(("HDA: Warning: Unhandled set volume knob control command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F15 */
static DECLCALLBACK(int) vrbProcGetGPIOData(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThis, pThisCC, cmd);
    *pResp = 0;
    return VINF_SUCCESS;
}

/* 715 */
static DECLCALLBACK(int) vrbProcSetGPIOData(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThis, pThisCC, cmd);
    *pResp = 0;
    return VINF_SUCCESS;
}

/* F16 */
static DECLCALLBACK(int) vrbProcGetGPIOEnableMask(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThis, pThisCC, cmd);
    *pResp = 0;
    return VINF_SUCCESS;
}

/* 716 */
static DECLCALLBACK(int) vrbProcSetGPIOEnableMask(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThis, pThisCC, cmd);
    *pResp = 0;
    return VINF_SUCCESS;
}

/* F17 */
static DECLCALLBACK(int) vrbProcGetGPIODirection(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    /* Note: this is true for ALC885. */
    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        *pResp = pThis->aNodes[1].afg.u32F17_param;
    else
        LogRel2(("HDA: Warning: Unhandled get GPIO direction command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 717 */
static DECLCALLBACK(int) vrbProcSetGPIODirection(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (CODEC_NID(cmd) == STAC9220_NID_AFG)
        pu32Reg = &pThis->aNodes[1].afg.u32F17_param;
    else
        LogRel2(("HDA: Warning: Unhandled set GPIO direction command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F1C */
static DECLCALLBACK(int) vrbProcGetConfig(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    else
        LogRel2(("HDA: Warning: Unhandled get config command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

static int codecSetConfigX(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset)
{
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    else
        LogRel2(("HDA: Warning: Unhandled set config command (%RU8) for NID0x%02x: 0x%x\n", u8Offset, CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);

    return VINF_SUCCESS;
}

/* 71C */
static DECLCALLBACK(int) vrbProcSetConfig0(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 0);
}

/* 71D */
static DECLCALLBACK(int) vrbProcSetConfig1(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 8);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig2(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 16);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig3(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 24);
}

/* F04 */
static DECLCALLBACK(int) vrbProcGetSDISelect(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->aNodes[CODEC_NID(cmd)].dac.u32F04_param;
    else
        LogRel2(("HDA: Warning: Unhandled get SDI select command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    return VINF_SUCCESS;
}

/* 704 */
static DECLCALLBACK(int) vrbProcSetSDISelect(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    RT_NOREF(pThisCC);
    *pResp = 0;

    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->aNodes[CODEC_NID(cmd)].dac.u32F04_param;
    else
        LogRel2(("HDA: Warning: Unhandled set SDI select command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));

    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

#ifdef IN_RING3

/* 3-- */
static DECLCALLBACK(int) vrbProcR3SetAmplifier(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;

    PCODECNODE pNode      = &pThis->aNodes[CODEC_NID(cmd)];
    AMPLIFIER *pAmplifier = NULL;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->dac.B_params;
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->adcvol.B_params;
    else if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->adcmux.B_params;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->pcbeep.B_params;
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->port.B_params;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pAmplifier = &pNode->adc.B_params;
    else
        LogRel2(("HDA: Warning: Unhandled set amplifier command: 0x%x (Payload=%RU16, NID=0x%x [%RU8])\n",
                 cmd, CODEC_VERB_PAYLOAD16(cmd), CODEC_NID(cmd), CODEC_NID(cmd)));

    if (!pAmplifier)
        return VINF_SUCCESS;

    bool fIsOut     = CODEC_SET_AMP_IS_OUT_DIRECTION(cmd);
    bool fIsIn      = CODEC_SET_AMP_IS_IN_DIRECTION(cmd);
    bool fIsLeft    = CODEC_SET_AMP_IS_LEFT_SIDE(cmd);
    bool fIsRight   = CODEC_SET_AMP_IS_RIGHT_SIDE(cmd);
    uint8_t u8Index = CODEC_SET_AMP_INDEX(cmd);

    if (   (!fIsLeft && !fIsRight)
        || (!fIsOut && !fIsIn))
        return VINF_SUCCESS;

    LogFunc(("[NID0x%02x] fIsOut=%RTbool, fIsIn=%RTbool, fIsLeft=%RTbool, fIsRight=%RTbool, Idx=%RU8\n",
             CODEC_NID(cmd), fIsOut, fIsIn, fIsLeft, fIsRight, u8Index));

    if (fIsIn)
    {
        if (fIsLeft)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_IN, AMPLIFIER_LEFT, u8Index), cmd, 0);
        if (fIsRight)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_IN, AMPLIFIER_RIGHT, u8Index), cmd, 0);

    //    if (CODEC_NID(cmd) == pThis->u8AdcVolsLineIn)
    //    {
            hdaR3CodecToAudVolume(pThisCC, pNode, pAmplifier, PDMAUDIOMIXERCTL_LINE_IN);
    //    }
    }
    if (fIsOut)
    {
        if (fIsLeft)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_LEFT, u8Index), cmd, 0);
        if (fIsRight)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_RIGHT, u8Index), cmd, 0);

        if (CODEC_NID(cmd) == pThis->u8DacLineOut)
            hdaR3CodecToAudVolume(pThisCC, pNode, pAmplifier, PDMAUDIOMIXERCTL_FRONT);
    }

    return VINF_SUCCESS;
}

/* 706 */
static DECLCALLBACK(int) vrbProcR3SetStreamId(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;

    uint8_t uSD      = CODEC_F00_06_GET_STREAM_ID(cmd);
    uint8_t uChannel = CODEC_F00_06_GET_CHANNEL_ID(cmd);

    LogFlowFunc(("[NID0x%02x] Setting to stream ID=%RU8, channel=%RU8\n",
                 CODEC_NID(cmd), uSD, uChannel));

    ASSERT_GUEST_LOGREL_MSG_RETURN(uSD < HDA_MAX_STREAMS,
                                   ("Setting stream ID #%RU8 is invalid\n", uSD), VERR_INVALID_PARAMETER);

    PDMAUDIODIR enmDir;
    uint32_t   *pu32Addr;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
    {
        pu32Addr = &pThis->aNodes[CODEC_NID(cmd)].dac.u32F06_param;
        enmDir = PDMAUDIODIR_OUT;
    }
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
    {
        pu32Addr = &pThis->aNodes[CODEC_NID(cmd)].adc.u32F06_param;
        enmDir = PDMAUDIODIR_IN;
    }
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
    {
        pu32Addr = &pThis->aNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
        enmDir = PDMAUDIODIR_OUT;
    }
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
    {
        pu32Addr = &pThis->aNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
        enmDir = PDMAUDIODIR_IN;
    }
    else
    {
        enmDir = PDMAUDIODIR_UNKNOWN;
        LogRel2(("HDA: Warning: Unhandled set stream ID command for NID0x%02x: 0x%x\n", CODEC_NID(cmd), cmd));
        return VINF_SUCCESS;
    }

    /* Do we (re-)assign our input/output SDn (SDI/SDO) IDs? */
    pThis->aNodes[CODEC_NID(cmd)].node.uSD      = uSD;
    pThis->aNodes[CODEC_NID(cmd)].node.uChannel = uChannel;

    if (enmDir == PDMAUDIODIR_OUT)
    {
        /** @todo Check if non-interleaved streams need a different channel / SDn? */

        /* Propagate to the controller. */
        pThisCC->pfnCbMixerControl(pThisCC->pDevIns, PDMAUDIOMIXERCTL_FRONT,      uSD, uChannel);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        pThisCC->pfnCbMixerControl(pThisCC->pDevIns, PDMAUDIOMIXERCTL_CENTER_LFE, uSD, uChannel);
        pThisCC->pfnCbMixerControl(pThisCC->pDevIns, PDMAUDIOMIXERCTL_REAR,       uSD, uChannel);
# endif
    }
    else if (enmDir == PDMAUDIODIR_IN)
    {
        pThisCC->pfnCbMixerControl(pThisCC->pDevIns, PDMAUDIOMIXERCTL_LINE_IN,    uSD, uChannel);
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        pThisCC->pfnCbMixerControl(pThisCC->pDevIns, PDMAUDIOMIXERCTL_MIC_IN,     uSD, uChannel);
# endif
    }

    hdaCodecSetRegisterU8(pu32Addr, cmd, 0);

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */


/**
 * HDA codec verb descriptors.
 *
 * @note This must be ordered by uVerb so we can do a binary lookup.
 */
static const CODECVERB g_aCodecVerbs[] =
{
    /* Verb        Verb mask            Callback                                   Name
       ---------- --------------------- ------------------------------------------------------------------- */
    { 0x00020000, CODEC_VERB_16BIT_CMD, vrbProcSetConverterFormat                , "SetConverterFormat    " },
    { 0x00030000, CODEC_VERB_16BIT_CMD, CTX_EXPR(vrbProcR3SetAmplifier,NULL,NULL), "SetAmplifier          " },
    { 0x00070100, CODEC_VERB_8BIT_CMD , vrbProcSetConSelectCtrl                  , "SetConSelectCtrl      " },
    { 0x00070300, CODEC_VERB_8BIT_CMD , vrbProcSetProcessingState                , "SetProcessingState    " },
    { 0x00070400, CODEC_VERB_8BIT_CMD , vrbProcSetSDISelect                      , "SetSDISelect          " },
    { 0x00070500, CODEC_VERB_8BIT_CMD , vrbProcSetPowerState                     , "SetPowerState         " },
    { 0x00070600, CODEC_VERB_8BIT_CMD , CTX_EXPR(vrbProcR3SetStreamId,NULL,NULL) , "SetStreamId           " },
    { 0x00070700, CODEC_VERB_8BIT_CMD , vrbProcSetPinCtrl                        , "SetPinCtrl            " },
    { 0x00070800, CODEC_VERB_8BIT_CMD , vrbProcSetUnsolicitedEnabled             , "SetUnsolicitedEnabled " },
    { 0x00070900, CODEC_VERB_8BIT_CMD , vrbProcSetPinSense                       , "SetPinSense           " },
    { 0x00070C00, CODEC_VERB_8BIT_CMD , vrbProcSetEAPD_BTLEnabled                , "SetEAPD_BTLEnabled    " },
    { 0x00070D00, CODEC_VERB_8BIT_CMD , vrbProcSetDigitalConverter1              , "SetDigitalConverter1  " },
    { 0x00070E00, CODEC_VERB_8BIT_CMD , vrbProcSetDigitalConverter2              , "SetDigitalConverter2  " },
    { 0x00070F00, CODEC_VERB_8BIT_CMD , vrbProcSetVolumeKnobCtrl                 , "SetVolumeKnobCtrl     " },
    { 0x00071500, CODEC_VERB_8BIT_CMD , vrbProcSetGPIOData                       , "SetGPIOData           " },
    { 0x00071600, CODEC_VERB_8BIT_CMD , vrbProcSetGPIOEnableMask                 , "SetGPIOEnableMask     " },
    { 0x00071700, CODEC_VERB_8BIT_CMD , vrbProcSetGPIODirection                  , "SetGPIODirection      " },
    { 0x00071C00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig0                        , "SetConfig0            " },
    { 0x00071D00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig1                        , "SetConfig1            " },
    { 0x00071E00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig2                        , "SetConfig2            " },
    { 0x00071F00, CODEC_VERB_8BIT_CMD , vrbProcSetConfig3                        , "SetConfig3            " },
    { 0x00072000, CODEC_VERB_8BIT_CMD , vrbProcSetSubId0                         , "SetSubId0             " },
    { 0x00072100, CODEC_VERB_8BIT_CMD , vrbProcSetSubId1                         , "SetSubId1             " },
    { 0x00072200, CODEC_VERB_8BIT_CMD , vrbProcSetSubId2                         , "SetSubId2             " },
    { 0x00072300, CODEC_VERB_8BIT_CMD , vrbProcSetSubId3                         , "SetSubId3             " },
    { 0x0007FF00, CODEC_VERB_8BIT_CMD , vrbProcReset                             , "Reset                 " },
    { 0x000A0000, CODEC_VERB_16BIT_CMD, vrbProcGetConverterFormat                , "GetConverterFormat    " },
    { 0x000B0000, CODEC_VERB_16BIT_CMD, vrbProcGetAmplifier                      , "GetAmplifier          " },
    { 0x000F0000, CODEC_VERB_8BIT_CMD , vrbProcGetParameter                      , "GetParameter          " },
    { 0x000F0100, CODEC_VERB_8BIT_CMD , vrbProcGetConSelectCtrl                  , "GetConSelectCtrl      " },
    { 0x000F0200, CODEC_VERB_8BIT_CMD , vrbProcGetConnectionListEntry            , "GetConnectionListEntry" },
    { 0x000F0300, CODEC_VERB_8BIT_CMD , vrbProcGetProcessingState                , "GetProcessingState    " },
    { 0x000F0400, CODEC_VERB_8BIT_CMD , vrbProcGetSDISelect                      , "GetSDISelect          " },
    { 0x000F0500, CODEC_VERB_8BIT_CMD , vrbProcGetPowerState                     , "GetPowerState         " },
    { 0x000F0600, CODEC_VERB_8BIT_CMD , vrbProcGetStreamId                       , "GetStreamId           " },
    { 0x000F0700, CODEC_VERB_8BIT_CMD , vrbProcGetPinCtrl                        , "GetPinCtrl            " },
    { 0x000F0800, CODEC_VERB_8BIT_CMD , vrbProcGetUnsolicitedEnabled             , "GetUnsolicitedEnabled " },
    { 0x000F0900, CODEC_VERB_8BIT_CMD , vrbProcGetPinSense                       , "GetPinSense           " },
    { 0x000F0C00, CODEC_VERB_8BIT_CMD , vrbProcGetEAPD_BTLEnabled                , "GetEAPD_BTLEnabled    " },
    { 0x000F0D00, CODEC_VERB_8BIT_CMD , vrbProcGetDigitalConverter               , "GetDigitalConverter   " },
    { 0x000F0F00, CODEC_VERB_8BIT_CMD , vrbProcGetVolumeKnobCtrl                 , "GetVolumeKnobCtrl     " },
    { 0x000F1500, CODEC_VERB_8BIT_CMD , vrbProcGetGPIOData                       , "GetGPIOData           " },
    { 0x000F1600, CODEC_VERB_8BIT_CMD , vrbProcGetGPIOEnableMask                 , "GetGPIOEnableMask     " },
    { 0x000F1700, CODEC_VERB_8BIT_CMD , vrbProcGetGPIODirection                  , "GetGPIODirection      " },
    { 0x000F1C00, CODEC_VERB_8BIT_CMD , vrbProcGetConfig                         , "GetConfig             " },
    { 0x000F2000, CODEC_VERB_8BIT_CMD , vrbProcGetSubId                          , "GetSubId              " },
    /** @todo Implement 0x7e7: IDT Set GPIO (STAC922x only). */
};


#ifdef IN_RING3

/**
 * CODEC debug info item printing state.
 */
typedef struct CODECDEBUG
{
    /** DBGF info helpers. */
    PCDBGFINFOHLP pHlp;
    /** Current recursion level. */
    uint8_t uLevel;
    /** Pointer to codec state. */
    PHDACODEC pThis;
} CODECDEBUG;
/** Pointer to the debug info item printing state for the codec. */
typedef CODECDEBUG *PCODECDEBUG;

#define CODECDBG_INDENT         pInfo->uLevel++;
#define CODECDBG_UNINDENT       if (pInfo->uLevel) pInfo->uLevel--;

#define CODECDBG_PRINT(...)     pInfo->pHlp->pfnPrintf(pInfo->pHlp, __VA_ARGS__)
#define CODECDBG_PRINTI(...)    codecDbgPrintf(pInfo, __VA_ARGS__)

/** Wrapper around DBGFINFOHLP::pfnPrintf that adds identation. */
static void codecDbgPrintf(PCODECDEBUG pInfo, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pInfo->pHlp->pfnPrintf(pInfo->pHlp, "%*s%N", pInfo->uLevel * 4, "", pszFormat, &va);
    va_end(va);
}

/** Power state */
static void codecDbgPrintNodeRegF05(PCODECDEBUG pInfo, uint32_t u32Reg)
{
    codecDbgPrintf(pInfo, "Power (F05): fReset=%RTbool, fStopOk=%RTbool, Set=%RU8, Act=%RU8\n",
                   CODEC_F05_IS_RESET(u32Reg), CODEC_F05_IS_STOPOK(u32Reg), CODEC_F05_SET(u32Reg), CODEC_F05_ACT(u32Reg));
}

static void codecDbgPrintNodeRegA(PCODECDEBUG pInfo, uint32_t u32Reg)
{
    codecDbgPrintf(pInfo, "RegA: %x\n", u32Reg);
}

static void codecDbgPrintNodeRegF00(PCODECDEBUG pInfo, uint32_t *paReg00)
{
    codecDbgPrintf(pInfo, "Parameters (F00):\n");

    CODECDBG_INDENT
        codecDbgPrintf(pInfo, "Connections: %RU8\n", CODEC_F00_0E_COUNT(paReg00[0xE]));
        codecDbgPrintf(pInfo, "Amplifier Caps:\n");
        uint32_t uReg = paReg00[0xD];
        CODECDBG_INDENT
            codecDbgPrintf(pInfo, "Input Steps=%02RU8, StepSize=%02RU8, StepOff=%02RU8, fCanMute=%RTbool\n",
                           CODEC_F00_0D_NUM_STEPS(uReg),
                           CODEC_F00_0D_STEP_SIZE(uReg),
                           CODEC_F00_0D_OFFSET(uReg),
                           RT_BOOL(CODEC_F00_0D_IS_CAP_MUTE(uReg)));

            uReg = paReg00[0x12];
            codecDbgPrintf(pInfo, "Output Steps=%02RU8, StepSize=%02RU8, StepOff=%02RU8, fCanMute=%RTbool\n",
                           CODEC_F00_12_NUM_STEPS(uReg),
                           CODEC_F00_12_STEP_SIZE(uReg),
                           CODEC_F00_12_OFFSET(uReg),
                           RT_BOOL(CODEC_F00_12_IS_CAP_MUTE(uReg)));
        CODECDBG_UNINDENT
    CODECDBG_UNINDENT
}

static void codecDbgPrintNodeAmp(PCODECDEBUG pInfo, uint32_t *paReg, uint8_t uIdx, uint8_t uDir)
{
# define CODECDBG_AMP(reg, chan) \
        codecDbgPrintf(pInfo, "Amp %RU8 %s %s: In=%RTbool, Out=%RTbool, Left=%RTbool, Right=%RTbool, Idx=%RU8, fMute=%RTbool, uGain=%RU8\n", \
                       uIdx, chan, uDir == AMPLIFIER_IN ? "In" : "Out", \
                       RT_BOOL(CODEC_SET_AMP_IS_IN_DIRECTION(reg)), RT_BOOL(CODEC_SET_AMP_IS_OUT_DIRECTION(reg)), \
                       RT_BOOL(CODEC_SET_AMP_IS_LEFT_SIDE(reg)), RT_BOOL(CODEC_SET_AMP_IS_RIGHT_SIDE(reg)), \
                       CODEC_SET_AMP_INDEX(reg), RT_BOOL(CODEC_SET_AMP_MUTE(reg)), CODEC_SET_AMP_GAIN(reg))

    uint32_t regAmp = AMPLIFIER_REGISTER(paReg, uDir, AMPLIFIER_LEFT, uIdx);
    CODECDBG_AMP(regAmp, "Left");
    regAmp = AMPLIFIER_REGISTER(paReg, uDir, AMPLIFIER_RIGHT, uIdx);
    CODECDBG_AMP(regAmp, "Right");

# undef CODECDBG_AMP
}

# if 0 /* unused */
static void codecDbgPrintNodeConnections(PCODECDEBUG pInfo, PCODECNODE pNode)
{
    if (pNode->node.au32F00_param[0xE] == 0) /* Directly connected to HDA link. */
    {
         codecDbgPrintf(pInfo, "[HDA LINK]\n");
         return;
    }
}
# endif

static void codecDbgPrintNode(PCODECDEBUG pInfo, PCODECNODE pNode, bool fRecursive)
{
    codecDbgPrintf(pInfo, "Node 0x%02x (%02RU8): ", pNode->node.uID, pNode->node.uID);

    if (pNode->node.uID == STAC9220_NID_ROOT)
    {
        CODECDBG_PRINT("ROOT\n");
    }
    else if (pNode->node.uID == STAC9220_NID_AFG)
    {
        CODECDBG_PRINT("AFG\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->afg.u32F05_param);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsPortNode(pInfo->pThis, pNode->node.uID))
    {
        CODECDBG_PRINT("PORT\n");
    }
    else if (hdaCodecIsDacNode(pInfo->pThis, pNode->node.uID))
    {
        CODECDBG_PRINT("DAC\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->dac.u32F05_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->dac.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->dac.B_params, 0, AMPLIFIER_OUT);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcVolNode(pInfo->pThis, pNode->node.uID))
    {
        CODECDBG_PRINT("ADC VOLUME\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adcvol.u32A_params);
            codecDbgPrintNodeAmp   (pInfo, pNode->adcvol.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcNode(pInfo->pThis, pNode->node.uID))
    {
        CODECDBG_PRINT("ADC\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->adc.u32F05_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adc.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->adc.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcMuxNode(pInfo->pThis, pNode->node.uID))
    {
        CODECDBG_PRINT("ADC MUX\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adcmux.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->adcmux.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsPcbeepNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("PC BEEP\n");
    else if (hdaCodecIsSpdifOutNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("SPDIF OUT\n");
    else if (hdaCodecIsSpdifInNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("SPDIF IN\n");
    else if (hdaCodecIsDigInPinNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("DIGITAL IN PIN\n");
    else if (hdaCodecIsDigOutPinNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("DIGITAL OUT PIN\n");
    else if (hdaCodecIsCdNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("CD\n");
    else if (hdaCodecIsVolKnobNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("VOLUME KNOB\n");
    else if (hdaCodecIsReservedNode(pInfo->pThis, pNode->node.uID))
        CODECDBG_PRINT("RESERVED\n");
    else
        CODECDBG_PRINT("UNKNOWN TYPE 0x%x\n", pNode->node.uID);

    if (fRecursive)
    {
# define CODECDBG_PRINT_CONLIST_ENTRY(_aNode, _aEntry) \
            if (cCnt >= _aEntry) \
            { \
                const uint8_t uID = RT_BYTE##_aEntry(_aNode->node.au32F02_param[0x0]); \
                if (pNode->node.uID == uID) \
                    codecDbgPrintNode(pInfo, _aNode, false /* fRecursive */); \
            }

        /* Slow recursion, but this is debug stuff anyway. */
        for (uint8_t i = 0; i < pInfo->pThis->cTotalNodes; i++)
        {
            const PCODECNODE pSubNode = &pInfo->pThis->aNodes[i];
            if (pSubNode->node.uID == pNode->node.uID)
                continue;

            const uint8_t cCnt = CODEC_F00_0E_COUNT(pSubNode->node.au32F00_param[0xE]);
            if (cCnt == 0) /* No connections present? Skip. */
                continue;

            CODECDBG_INDENT
                CODECDBG_PRINT_CONLIST_ENTRY(pSubNode, 1)
                CODECDBG_PRINT_CONLIST_ENTRY(pSubNode, 2)
                CODECDBG_PRINT_CONLIST_ENTRY(pSubNode, 3)
                CODECDBG_PRINT_CONLIST_ENTRY(pSubNode, 4)
            CODECDBG_UNINDENT
        }

# undef CODECDBG_PRINT_CONLIST_ENTRY
   }
}

static DECLCALLBACK(void) codecR3DbgListNodes(PHDACODEC pThis, PHDACODECR3 pThisCC, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pThisCC, pszArgs);

    pHlp->pfnPrintf(pHlp, "HDA LINK / INPUTS\n");

    CODECDEBUG dbgInfo;
    dbgInfo.pHlp   = pHlp;
    dbgInfo.pThis  = pThis;
    dbgInfo.uLevel = 0;

    PCODECDEBUG pInfo = &dbgInfo;

    CODECDBG_INDENT
        for (uint8_t i = 0; i < pThis->cTotalNodes; i++)
        {
            PCODECNODE pNode = &pThis->aNodes[i];

            /* Start with all nodes which have connection entries set. */
            if (CODEC_F00_0E_COUNT(pNode->node.au32F00_param[0xE]))
                codecDbgPrintNode(&dbgInfo, pNode, true /* fRecursive */);
        }
    CODECDBG_UNINDENT
}

static DECLCALLBACK(void) codecR3DbgSelector(PHDACODEC pThis, PHDACODECR3 pThisCC, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pThis, pThisCC, pHlp, pszArgs);
}

#endif /* IN_RING3 */

/**
 * Implements
 */
static DECLCALLBACK(int) codecLookup(PHDACODEC pThis, PHDACODECCC pThisCC, uint32_t uCmd, uint64_t *puResp)
{
    /*
     * Clear the return value and assert some sanity.
     */
    AssertPtr(puResp);
    *puResp = 0;
    AssertPtr(pThis);
    AssertPtr(pThisCC);
    AssertMsgReturn(CODEC_CAD(uCmd) == pThis->id,
                    ("Unknown codec address 0x%x\n", CODEC_CAD(uCmd)),
                    VERR_INVALID_PARAMETER);
    uint32_t const uCmdData = CODEC_VERBDATA(uCmd);
    AssertMsgReturn(   uCmdData != 0
                    && CODEC_NID(uCmd) < RT_MIN(pThis->cTotalNodes, RT_ELEMENTS(pThis->aNodes)),
                    ("[NID0x%02x] Unknown / invalid node or data (0x%x)\n", CODEC_NID(uCmd), uCmdData),
                    VERR_INVALID_PARAMETER);
    STAM_COUNTER_INC(&pThis->CTX_SUFF(StatLookups));

    /*
     * Do a binary lookup of the verb.
     * Note! if we want other verb tables, add a table selector before the loop.
     */
    size_t iFirst = 0;
    size_t iEnd   = RT_ELEMENTS(g_aCodecVerbs);
    for (;;)
    {
        size_t const   iCur  = iFirst + (iEnd - iFirst) / 2;
        uint32_t const uVerb = g_aCodecVerbs[iCur].uVerb;
        if (uCmdData < uVerb)
        {
            if (iCur > iFirst)
                iEnd = iCur;
            else
                break;
        }
        else if ((uCmdData & g_aCodecVerbs[iCur].fMask) != uVerb)
        {
            if (iCur + 1 < iEnd)
                iFirst = iCur + 1;
            else
                break;
        }
        else
        {
            /*
             * Found it!  Run the callback and return.
             */
#ifndef IN_RING3
            if (!g_aCodecVerbs[iCur].pfn)
            {
                Log3Func(("[NID0x%02x] (0x%x) %s: 0x%x -> VERR_INVALID_CONTEXT\n", /* -> ring-3 */
                          CODEC_NID(uCmd), g_aCodecVerbs[iCur].uVerb, g_aCodecVerbs[iCur].pszName, CODEC_VERB_PAYLOAD8(uCmd)));
                return VERR_INVALID_CONTEXT;
            }
#endif
            AssertPtrReturn(g_aCodecVerbs[iCur].pfn, VERR_INTERNAL_ERROR_5); /* Paranoia^2. */

            int rc = g_aCodecVerbs[iCur].pfn(pThis, pThisCC, uCmd, puResp);
            AssertRC(rc);
            Log3Func(("[NID0x%02x] (0x%x) %s: 0x%x -> 0x%x\n",
                      CODEC_NID(uCmd), g_aCodecVerbs[iCur].uVerb, g_aCodecVerbs[iCur].pszName, CODEC_VERB_PAYLOAD8(uCmd), *puResp));
            return rc;
        }
    }

#ifdef VBOX_STRICT
    for (size_t i = 0; i < RT_ELEMENTS(g_aCodecVerbs); i++)
    {
        AssertMsg(i == 0 || g_aCodecVerbs[i - 1].uVerb < g_aCodecVerbs[i].uVerb,
                  ("i=%#x uVerb[-1]=%#x uVerb=%#x - buggy table!\n", i, g_aCodecVerbs[i - 1].uVerb, g_aCodecVerbs[i].uVerb));
        AssertMsg((uCmdData & g_aCodecVerbs[i].fMask) != g_aCodecVerbs[i].uVerb,
                  ("i=%#x uVerb=%#x uCmd=%#x - buggy binary search or table!\n", i, g_aCodecVerbs[i].uVerb, uCmd));
    }
#endif
    LogFunc(("[NID0x%02x] Callback for %x not found\n", CODEC_NID(uCmd), CODEC_VERBDATA(uCmd)));
    return VERR_NOT_FOUND;
}


/*
 * APIs exposed to DevHDA.
 */

#ifdef IN_RING3

int hdaR3CodecAddStream(PHDACODECR3 pThisCC, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThisCC, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    switch (enmMixerCtl)
    {
        case PDMAUDIOMIXERCTL_VOLUME_MASTER:
        case PDMAUDIOMIXERCTL_FRONT:
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        case PDMAUDIOMIXERCTL_CENTER_LFE:
        case PDMAUDIOMIXERCTL_REAR:
#endif
            break;

        case PDMAUDIOMIXERCTL_LINE_IN:
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        case PDMAUDIOMIXERCTL_MIC_IN:
#endif
            break;

        default:
            AssertMsgFailed(("Mixer control %#x not implemented\n", enmMixerCtl));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
        rc = pThisCC->pfnCbMixerAddStream(pThisCC->pDevIns, enmMixerCtl, pCfg);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int hdaR3CodecRemoveStream(PHDACODECR3 pThisCC, PDMAUDIOMIXERCTL enmMixerCtl, bool fImmediate)
{
    AssertPtrReturn(pThisCC, VERR_INVALID_POINTER);

    int rc = pThisCC->pfnCbMixerRemoveStream(pThisCC->pDevIns, enmMixerCtl, fImmediate);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int hdaCodecSaveState(PPDMDEVINS pDevIns, PHDACODEC pThis, PSSMHANDLE pSSM)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;
    AssertLogRelMsgReturn(pThis->cTotalNodes == STAC9221_NUM_NODES, ("cTotalNodes=%#x, should be 0x1c", pThis->cTotalNodes),
                          VERR_INTERNAL_ERROR);
    pHlp->pfnSSMPutU32(pSSM, pThis->cTotalNodes);
    for (unsigned idxNode = 0; idxNode < pThis->cTotalNodes; ++idxNode)
        pHlp->pfnSSMPutStructEx(pSSM, &pThis->aNodes[idxNode].SavedState, sizeof(pThis->aNodes[idxNode].SavedState),
                                0 /*fFlags*/, g_aCodecNodeFields, NULL /*pvUser*/);
    return VINF_SUCCESS;
}

int hdaR3CodecLoadState(PPDMDEVINS pDevIns, PHDACODEC pThis, PHDACODECR3 pThisCC, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;
    PCSSMFIELD pFields = NULL;
    uint32_t   fFlags  = 0;
    if (uVersion >= HDA_SAVED_STATE_VERSION_4)
    {
        /* Since version 4 a flexible node count is supported. */
        uint32_t cNodes;
        int rc2 = pHlp->pfnSSMGetU32(pSSM, &cNodes);
        AssertRCReturn(rc2, rc2);
        AssertReturn(cNodes == 0x1c, VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);

        pFields = g_aCodecNodeFields;
        fFlags  = 0;
    }
    else if (uVersion >= HDA_SAVED_STATE_VERSION_2)
    {
        AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
        pFields = g_aCodecNodeFields;
        fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
    }
    else if (uVersion >= HDA_SAVED_STATE_VERSION_1)
    {
        AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
        pFields = g_aCodecNodeFieldsV1;
        fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
    }
    else
        AssertFailedReturn(VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    for (unsigned idxNode = 0; idxNode < pThis->cTotalNodes; ++idxNode)
    {
        uint8_t idOld = pThis->aNodes[idxNode].SavedState.Core.uID;
        int rc = pHlp->pfnSSMGetStructEx(pSSM, &pThis->aNodes[idxNode].SavedState, sizeof(pThis->aNodes[idxNode].SavedState),
                                         fFlags, pFields, NULL);
        AssertRCReturn(rc, rc);
        AssertLogRelMsgReturn(idOld == pThis->aNodes[idxNode].SavedState.Core.uID,
                              ("loaded %#x, expected %#x\n", pThis->aNodes[idxNode].SavedState.Core.uID, idOld),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    /*
     * Update stuff after changing the state.
     */
    PCODECNODE pNode;
    if (hdaCodecIsDacNode(pThis, pThis->u8DacLineOut))
    {
        pNode = &pThis->aNodes[pThis->u8DacLineOut];
        hdaR3CodecToAudVolume(pThisCC, pNode, &pNode->dac.B_params, PDMAUDIOMIXERCTL_FRONT);
    }
    else if (hdaCodecIsSpdifOutNode(pThis, pThis->u8DacLineOut))
    {
        pNode = &pThis->aNodes[pThis->u8DacLineOut];
        hdaR3CodecToAudVolume(pThisCC, pNode, &pNode->spdifout.B_params, PDMAUDIOMIXERCTL_FRONT);
    }

    pNode = &pThis->aNodes[pThis->u8AdcVolsLineIn];
    hdaR3CodecToAudVolume(pThisCC, pNode, &pNode->adcvol.B_params, PDMAUDIOMIXERCTL_LINE_IN);

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Powers off the codec (ring-3).
 *
 * @param   pThisCC             Context-specific codec data (ring-3) to power off.
 */
void hdaR3CodecPowerOff(PHDACODECR3 pThisCC)
{
    if (!pThisCC)
        return;

    LogFlowFuncEnter();

    LogRel2(("HDA: Powering off codec ...\n"));

    int rc2 = hdaR3CodecRemoveStream(pThisCC, PDMAUDIOMIXERCTL_FRONT, true /*fImmediate*/);
    AssertRC(rc2);
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    rc2 = hdaR3CodecRemoveStream(pThisCC, PDMAUDIOMIXERCTL_CENTER_LFE, true /*fImmediate*/);
    AssertRC(rc2);
    rc2 = hdaR3CodecRemoveStream(pThisCC, PDMAUDIOMIXERCTL_REAR, true /*fImmediate*/);
    AssertRC(rc2);
#endif

#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    rc2 = hdaR3CodecRemoveStream(pThisCC, PDMAUDIOMIXERCTL_MIC_IN, true /*fImmediate*/);
    AssertRC(rc2);
#endif
    rc2 = hdaR3CodecRemoveStream(pThisCC, PDMAUDIOMIXERCTL_LINE_IN, true /*fImmediate*/);
    AssertRC(rc2);
}

/**
 * Constructs a codec (ring-3).
 *
 * @returns VBox status code.
 * @param   pDevIns             Associated device instance.
 * @param   pThis               Shared codec data beteen r0/r3.
 * @param   pThisCC             Context-specific codec data (ring-3).
 * @param   uLUN                Device LUN to assign.
 * @param   pCfg                CFGM node to use for configuration.
 */
int hdaR3CodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis, PHDACODECR3 pThisCC, uint16_t uLUN, PCFGMNODE pCfg)
{
    AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pThisCC, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,    VERR_INVALID_POINTER);

    pThis->id      = uLUN;
    pThis->enmType = CODEC_TYPE_STAC9220; /** @todo Make this dynamic. */

    int rc;

    switch (pThis->enmType)
    {
        case CODEC_TYPE_STAC9220:
        {
            rc = stac9220Construct(pThis);
            AssertRCReturn(rc, rc);
            break;
        }

        default:
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);
            break;
    }

    pThisCC->pfnDbgSelector  = codecR3DbgSelector;
    pThisCC->pfnDbgListNodes = codecR3DbgListNodes;
    pThisCC->pfnLookup       = codecLookup;

    /*
     * Set initial volume.
     */
    PCODECNODE pNode = &pThis->aNodes[pThis->u8DacLineOut];
    rc = hdaR3CodecToAudVolume(pThisCC, pNode, &pNode->dac.B_params, PDMAUDIOMIXERCTL_FRONT);
    AssertRCReturn(rc, rc);

    pNode = &pThis->aNodes[pThis->u8AdcVolsLineIn];
    rc = hdaR3CodecToAudVolume(pThisCC, pNode, &pNode->adcvol.B_params, PDMAUDIOMIXERCTL_LINE_IN);
    AssertRCReturn(rc, rc);

# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
#  error "Implement mic-in support!"
# endif

    /*
     * Statistics
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatLookupsR3, STAMTYPE_COUNTER, "Codec/LookupsR0", STAMUNIT_OCCURENCES, "Number of R0 codecLookup calls");
# if 0 /* Codec is not yet kosher enough for ring-0.  @bugref{9890c64} */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatLookupsR0, STAMTYPE_COUNTER, "Codec/LookupsR3", STAMUNIT_OCCURENCES, "Number of R3 codecLookup calls");
# endif

    return rc;
}

#else /* IN_RING0 */

/**
 * Constructs a codec (ring-0).
 *
 * @returns VBox status code.
 * @param   pDevIns             Associated device instance.
 * @param   pThis               Shared codec data beteen r0/r3.
 * @param   pThisCC             Context-specific codec data (ring-0).
 */
int hdaR0CodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis, PHDACODECR0 pThisCC)
{
    AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pThisCC, VERR_INVALID_POINTER);

    pThisCC->pfnLookup = codecLookup;

    /* Note: Everything else is done in the R3 part. */

    return VINF_SUCCESS;
}

#endif /* IN_RING0 */

/**
 * Destructs a codec.
 *
 * @param   pThis           Codec to destruct.
 */
void hdaCodecDestruct(PHDACODEC pThis)
{
    if (!pThis)
        return;

    /* Nothing to do here atm. */

    LogFlowFuncEnter();
}

/**
 * Resets a codec.
 *
 * @param   pThis           Codec to reset.
 */
void hdaCodecReset(PHDACODEC pThis)
{
    switch (pThis->enmType)
    {
        case CODEC_TYPE_STAC9220:
            stac9220Reset(pThis);
            break;

        default:
            AssertFailed();
            break;
    }
}

