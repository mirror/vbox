/* $Id$ */
/** @file
 * DevIchHdaCodec - VBox ICH Intel HD Audio Codec.
 *
 * Implemented against "Intel I/O Controller Hub 6 (ICH6) High Definition
 * Audio / AC '97 - Programmer's Reference Manual (PRM)", document number
 * 302349-003.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/cpp/utils.h>

#include "VBoxDD.h"
#include "DevIchHdaCodec.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* PRM 5.3.1 */
/** Codec address mask. */
#define CODEC_CAD_MASK                                     0xF0000000
/** Codec address shift. */
#define CODEC_CAD_SHIFT                                    28
#define CODEC_DIRECT_MASK                                  RT_BIT(27)
/** Node ID mask. */
#define CODEC_NID_MASK                                     0x07F00000
/** Node ID shift. */
#define CODEC_NID_SHIFT                                    20
#define CODEC_VERBDATA_MASK                                0x000FFFFF
#define CODEC_VERB_4BIT_CMD                                0x000FFFF0
#define CODEC_VERB_4BIT_DATA                               0x0000000F
#define CODEC_VERB_8BIT_CMD                                0x000FFF00
#define CODEC_VERB_8BIT_DATA                               0x000000FF
#define CODEC_VERB_16BIT_CMD                               0x000F0000
#define CODEC_VERB_16BIT_DATA                              0x0000FFFF

#define CODEC_CAD(cmd)                                     (((cmd) & CODEC_CAD_MASK) >> CODEC_CAD_SHIFT)
#define CODEC_DIRECT(cmd)                                  ((cmd) & CODEC_DIRECT_MASK)
#define CODEC_NID(cmd)                                     ((((cmd) & CODEC_NID_MASK)) >> CODEC_NID_SHIFT)
#define CODEC_VERBDATA(cmd)                                ((cmd) & CODEC_VERBDATA_MASK)
#define CODEC_VERB_CMD(cmd, mask, x)                       (((cmd) & (mask)) >> (x))
#define CODEC_VERB_CMD4(cmd)                               (CODEC_VERB_CMD((cmd), CODEC_VERB_4BIT_CMD, 4))
#define CODEC_VERB_CMD8(cmd)                               (CODEC_VERB_CMD((cmd), CODEC_VERB_8BIT_CMD, 8))
#define CODEC_VERB_CMD16(cmd)                              (CODEC_VERB_CMD((cmd), CODEC_VERB_16BIT_CMD, 16))
#define CODEC_VERB_PAYLOAD4(cmd)                           ((cmd) & CODEC_VERB_4BIT_DATA)
#define CODEC_VERB_PAYLOAD8(cmd)                           ((cmd) & CODEC_VERB_8BIT_DATA)
#define CODEC_VERB_PAYLOAD16(cmd)                          ((cmd) & CODEC_VERB_16BIT_DATA)

#define CODEC_VERB_GET_AMP_DIRECTION                       RT_BIT(15)
#define CODEC_VERB_GET_AMP_SIDE                            RT_BIT(13)
#define CODEC_VERB_GET_AMP_INDEX                           0x7

/* HDA spec 7.3.3.7 NoteA */
#define CODEC_GET_AMP_DIRECTION(cmd)                       (((cmd) & CODEC_VERB_GET_AMP_DIRECTION) >> 15)
#define CODEC_GET_AMP_SIDE(cmd)                            (((cmd) & CODEC_VERB_GET_AMP_SIDE) >> 13)
#define CODEC_GET_AMP_INDEX(cmd)                           (CODEC_GET_AMP_DIRECTION(cmd) ? 0 : ((cmd) & CODEC_VERB_GET_AMP_INDEX))

/* HDA spec 7.3.3.7 NoteC */
#define CODEC_VERB_SET_AMP_OUT_DIRECTION                   RT_BIT(15)
#define CODEC_VERB_SET_AMP_IN_DIRECTION                    RT_BIT(14)
#define CODEC_VERB_SET_AMP_LEFT_SIDE                       RT_BIT(13)
#define CODEC_VERB_SET_AMP_RIGHT_SIDE                      RT_BIT(12)
#define CODEC_VERB_SET_AMP_INDEX                           (0x7 << 8)
#define CODEC_VERB_SET_AMP_MUTE                            RT_BIT(7)
/** Note: 7-bit value [6:0]. */
#define CODEC_VERB_SET_AMP_GAIN                            0x7F

#define CODEC_SET_AMP_IS_OUT_DIRECTION(cmd)                (((cmd) & CODEC_VERB_SET_AMP_OUT_DIRECTION) != 0)
#define CODEC_SET_AMP_IS_IN_DIRECTION(cmd)                 (((cmd) & CODEC_VERB_SET_AMP_IN_DIRECTION) != 0)
#define CODEC_SET_AMP_IS_LEFT_SIDE(cmd)                    (((cmd) & CODEC_VERB_SET_AMP_LEFT_SIDE) != 0)
#define CODEC_SET_AMP_IS_RIGHT_SIDE(cmd)                   (((cmd) & CODEC_VERB_SET_AMP_RIGHT_SIDE) != 0)
#define CODEC_SET_AMP_INDEX(cmd)                           (((cmd) & CODEC_VERB_SET_AMP_INDEX) >> 7)
#define CODEC_SET_AMP_MUTE(cmd)                            ((cmd) & CODEC_VERB_SET_AMP_MUTE)
#define CODEC_SET_AMP_GAIN(cmd)                            ((cmd) & CODEC_VERB_SET_AMP_GAIN)

/* HDA spec 7.3.3.1 defines layout of configuration registers/verbs (0xF00) */
/* VendorID (7.3.4.1) */
#define CODEC_MAKE_F00_00(vendorID, deviceID)              (((vendorID) << 16) | (deviceID))
#define CODEC_F00_00_VENDORID(f00_00)                      (((f00_00) >> 16) & 0xFFFF)
#define CODEC_F00_00_DEVICEID(f00_00)                      ((f00_00) & 0xFFFF)
/* RevisionID (7.3.4.2)*/
#define CODEC_MAKE_F00_02(MajRev, MinRev, RevisionID, SteppingID) (((MajRev) << 20)|((MinRev) << 16)|((RevisionID) << 8)|(SteppingID))
/* Subordinate node count (7.3.4.3)*/
#define CODEC_MAKE_F00_04(startNodeNumber, totalNodeNumber) ((((startNodeNumber) & 0xFF) << 16)|((totalNodeNumber) & 0xFF))
#define CODEC_F00_04_TO_START_NODE_NUMBER(f00_04)          (((f00_04) >> 16) & 0xFF)
#define CODEC_F00_04_TO_NODE_COUNT(f00_04)                 ((f00_04) & 0xFF)
/*
 * Function Group Type  (7.3.4.4)
 * 0 & [0x3-0x7f] are reserved types
 * [0x80 - 0xff] are vendor defined function groups
 */
#define CODEC_MAKE_F00_05(UnSol, NodeType)                 (((UnSol) << 8)|(NodeType))
#define CODEC_F00_05_UNSOL                                 RT_BIT(8)
#define CODEC_F00_05_AFG                                   (0x1)
#define CODEC_F00_05_MFG                                   (0x2)
#define CODEC_F00_05_IS_UNSOL(f00_05)                      RT_BOOL((f00_05) & RT_BIT(8))
#define CODEC_F00_05_GROUP(f00_05)                         ((f00_05) & 0xff)
/*  Audio Function Group capabilities (7.3.4.5) */
#define CODEC_MAKE_F00_08(BeepGen, InputDelay, OutputDelay) ((((BeepGen) & 0x1) << 16)| (((InputDelay) & 0xF) << 8) | ((OutputDelay) & 0xF))
#define CODEC_F00_08_BEEP_GEN(f00_08)                      ((f00_08) & RT_BIT(16)

/* Widget Capabilities (7.3.4.6) */
#define CODEC_MAKE_F00_09(type, delay, chanel_count) \
    ( (((type) & 0xF) << 20)            \
    | (((delay) & 0xF) << 16)           \
    | (((chanel_count) & 0xF) << 13))
/* note: types 0x8-0xe are reserved */
#define CODEC_F00_09_TYPE_AUDIO_OUTPUT                     (0x0)
#define CODEC_F00_09_TYPE_AUDIO_INPUT                      (0x1)
#define CODEC_F00_09_TYPE_AUDIO_MIXER                      (0x2)
#define CODEC_F00_09_TYPE_AUDIO_SELECTOR                   (0x3)
#define CODEC_F00_09_TYPE_PIN_COMPLEX                      (0x4)
#define CODEC_F00_09_TYPE_POWER_WIDGET                     (0x5)
#define CODEC_F00_09_TYPE_VOLUME_KNOB                      (0x6)
#define CODEC_F00_09_TYPE_BEEP_GEN                         (0x7)
#define CODEC_F00_09_TYPE_VENDOR_DEFINED                   (0xF)

#define CODEC_F00_09_CAP_CP                                RT_BIT(12)
#define CODEC_F00_09_CAP_L_R_SWAP                          RT_BIT(11)
#define CODEC_F00_09_CAP_POWER_CTRL                        RT_BIT(10)
#define CODEC_F00_09_CAP_DIGITAL                           RT_BIT(9)
#define CODEC_F00_09_CAP_CONNECTION_LIST                   RT_BIT(8)
#define CODEC_F00_09_CAP_UNSOL                             RT_BIT(7)
#define CODEC_F00_09_CAP_PROC_WIDGET                       RT_BIT(6)
#define CODEC_F00_09_CAP_STRIPE                            RT_BIT(5)
#define CODEC_F00_09_CAP_FMT_OVERRIDE                      RT_BIT(4)
#define CODEC_F00_09_CAP_AMP_FMT_OVERRIDE                  RT_BIT(3)
#define CODEC_F00_09_CAP_OUT_AMP_PRESENT                   RT_BIT(2)
#define CODEC_F00_09_CAP_IN_AMP_PRESENT                    RT_BIT(1)
#define CODEC_F00_09_CAP_LSB                               RT_BIT(0)

#define CODEC_F00_09_TYPE(f00_09)                          (((f00_09) >> 20) & 0xF)

#define CODEC_F00_09_IS_CAP_CP(f00_09)                     RT_BOOL((f00_09) & RT_BIT(12))
#define CODEC_F00_09_IS_CAP_L_R_SWAP(f00_09)               RT_BOOL((f00_09) & RT_BIT(11))
#define CODEC_F00_09_IS_CAP_POWER_CTRL(f00_09)             RT_BOOL((f00_09) & RT_BIT(10))
#define CODEC_F00_09_IS_CAP_DIGITAL(f00_09)                RT_BOOL((f00_09) & RT_BIT(9))
#define CODEC_F00_09_IS_CAP_CONNECTION_LIST(f00_09)        RT_BOOL((f00_09) & RT_BIT(8))
#define CODEC_F00_09_IS_CAP_UNSOL(f00_09)                  RT_BOOL((f00_09) & RT_BIT(7))
#define CODEC_F00_09_IS_CAP_PROC_WIDGET(f00_09)            RT_BOOL((f00_09) & RT_BIT(6))
#define CODEC_F00_09_IS_CAP_STRIPE(f00_09)                 RT_BOOL((f00_09) & RT_BIT(5))
#define CODEC_F00_09_IS_CAP_FMT_OVERRIDE(f00_09)           RT_BOOL((f00_09) & RT_BIT(4))
#define CODEC_F00_09_IS_CAP_AMP_OVERRIDE(f00_09)           RT_BOOL((f00_09) & RT_BIT(3))
#define CODEC_F00_09_IS_CAP_OUT_AMP_PRESENT(f00_09)        RT_BOOL((f00_09) & RT_BIT(2))
#define CODEC_F00_09_IS_CAP_IN_AMP_PRESENT(f00_09)         RT_BOOL((f00_09) & RT_BIT(1))
#define CODEC_F00_09_IS_CAP_LSB(f00_09)                    RT_BOOL((f00_09) & RT_BIT(0))

/* Supported PCM size, rates (7.3.4.7) */
#define CODEC_F00_0A_32_BIT                                RT_BIT(19)
#define CODEC_F00_0A_24_BIT                                RT_BIT(18)
#define CODEC_F00_0A_16_BIT                                RT_BIT(17)
#define CODEC_F00_0A_8_BIT                                 RT_BIT(16)

#define CODEC_F00_0A_48KHZ_MULT_8X                         RT_BIT(11)
#define CODEC_F00_0A_48KHZ_MULT_4X                         RT_BIT(10)
#define CODEC_F00_0A_44_1KHZ_MULT_4X                       RT_BIT(9)
#define CODEC_F00_0A_48KHZ_MULT_2X                         RT_BIT(8)
#define CODEC_F00_0A_44_1KHZ_MULT_2X                       RT_BIT(7)
#define CODEC_F00_0A_48KHZ                                 RT_BIT(6)
#define CODEC_F00_0A_44_1KHZ                               RT_BIT(5)
/* 2/3 * 48kHz */
#define CODEC_F00_0A_48KHZ_2_3X                            RT_BIT(4)
/* 1/2 * 44.1kHz */
#define CODEC_F00_0A_44_1KHZ_1_2X                          RT_BIT(3)
/* 1/3 * 48kHz */
#define CODEC_F00_0A_48KHZ_1_3X                            RT_BIT(2)
/* 1/4 * 44.1kHz */
#define CODEC_F00_0A_44_1KHZ_1_4X                          RT_BIT(1)
/* 1/6 * 48kHz */
#define CODEC_F00_0A_48KHZ_1_6X                            RT_BIT(0)

/* Supported streams formats (7.3.4.8) */
#define CODEC_F00_0B_AC3                                   RT_BIT(2)
#define CODEC_F00_0B_FLOAT32                               RT_BIT(1)
#define CODEC_F00_0B_PCM                                   RT_BIT(0)

/* Pin Capabilities (7.3.4.9)*/
#define CODEC_MAKE_F00_0C(vref_ctrl) (((vref_ctrl) & 0xFF) << 8)
#define CODEC_F00_0C_CAP_HBR                               RT_BIT(27)
#define CODEC_F00_0C_CAP_DP                                RT_BIT(24)
#define CODEC_F00_0C_CAP_EAPD                              RT_BIT(16)
#define CODEC_F00_0C_CAP_HDMI                              RT_BIT(7)
#define CODEC_F00_0C_CAP_BALANCED_IO                       RT_BIT(6)
#define CODEC_F00_0C_CAP_INPUT                             RT_BIT(5)
#define CODEC_F00_0C_CAP_OUTPUT                            RT_BIT(4)
#define CODEC_F00_0C_CAP_HP                                RT_BIT(3)
#define CODEC_F00_0C_CAP_PRESENSE_DETECT                   RT_BIT(2)
#define CODEC_F00_0C_CAP_TRIGGER_REQUIRED                  RT_BIT(1)
#define CODEC_F00_0C_CAP_IMPENDANCE_SENSE                  RT_BIT(0)

#define CODEC_F00_0C_IS_CAP_HBR(f00_0c)                    ((f00_0c) & RT_BIT(27))
#define CODEC_F00_0C_IS_CAP_DP(f00_0c)                     ((f00_0c) & RT_BIT(24))
#define CODEC_F00_0C_IS_CAP_EAPD(f00_0c)                   ((f00_0c) & RT_BIT(16))
#define CODEC_F00_0C_IS_CAP_HDMI(f00_0c)                   ((f00_0c) & RT_BIT(7))
#define CODEC_F00_0C_IS_CAP_BALANCED_IO(f00_0c)            ((f00_0c) & RT_BIT(6))
#define CODEC_F00_0C_IS_CAP_INPUT(f00_0c)                  ((f00_0c) & RT_BIT(5))
#define CODEC_F00_0C_IS_CAP_OUTPUT(f00_0c)                 ((f00_0c) & RT_BIT(4))
#define CODEC_F00_0C_IS_CAP_HP(f00_0c)                     ((f00_0c) & RT_BIT(3))
#define CODEC_F00_0C_IS_CAP_PRESENSE_DETECT(f00_0c)        ((f00_0c) & RT_BIT(2))
#define CODEC_F00_0C_IS_CAP_TRIGGER_REQUIRED(f00_0c)       ((f00_0c) & RT_BIT(1))
#define CODEC_F00_0C_IS_CAP_IMPENDANCE_SENSE(f00_0c)       ((f00_0c) & RT_BIT(0))

/* Input Amplifier capabilities (7.3.4.10) */
#define CODEC_MAKE_F00_0D(mute_cap, step_size, num_steps, offset) \
        (  (((mute_cap) & 0x1) << 31)                             \
         | (((step_size) & 0xFF) << 16)                           \
         | (((num_steps) & 0xFF) << 8)                            \
         | ((offset) & 0xFF))

#define CODEC_F00_0D_CAP_MUTE                              RT_BIT(7)

#define CODEC_F00_0D_IS_CAP_MUTE(f00_0d)                   ( ( f00_0d) & RT_BIT(31))
#define CODEC_F00_0D_STEP_SIZE(f00_0d)                     ((( f00_0d) & (0x7F << 16)) >> 16)
#define CODEC_F00_0D_NUM_STEPS(f00_0d)                     ((((f00_0d) & (0x7F << 8)) >> 8) + 1)
#define CODEC_F00_0D_OFFSET(f00_0d)                        (  (f00_0d) & 0x7F)

/* Output Amplifier capabilities (7.3.4.10) */
#define CODEC_MAKE_F00_12                                  CODEC_MAKE_F00_0D

#define CODEC_F00_12_IS_CAP_MUTE(f00_12)                   CODEC_F00_0D_IS_CAP_MUTE(f00_12)
#define CODEC_F00_12_STEP_SIZE(f00_12)                     CODEC_F00_0D_STEP_SIZE(f00_12)
#define CODEC_F00_12_NUM_STEPS(f00_12)                     CODEC_F00_0D_NUM_STEPS(f00_12)
#define CODEC_F00_12_OFFSET(f00_12)                        CODEC_F00_0D_OFFSET(f00_12)

/* Connection list lenght (7.3.4.11) */
#define CODEC_MAKE_F00_0E(long_form, length)    \
    (  (((long_form) & 0x1) << 7)               \
     | ((length) & 0x7F))
/* Indicates short-form NIDs. */
#define CODEC_F00_0E_LIST_NID_SHORT                        0
/* Indicates long-form NIDs. */
#define CODEC_F00_0E_LIST_NID_LONG                         1
#define CODEC_F00_0E_IS_LONG(f00_0e)                       RT_BOOL((f00_0e) & RT_BIT(7))
#define CODEC_F00_0E_COUNT(f00_0e)                         ((f00_0e) & 0x7F)
/* Supported Power States (7.3.4.12) */
#define CODEC_F00_0F_EPSS                                  RT_BIT(31)
#define CODEC_F00_0F_CLKSTOP                               RT_BIT(30)
#define CODEC_F00_0F_S3D3                                  RT_BIT(29)
#define CODEC_F00_0F_D3COLD                                RT_BIT(4)
#define CODEC_F00_0F_D3                                    RT_BIT(3)
#define CODEC_F00_0F_D2                                    RT_BIT(2)
#define CODEC_F00_0F_D1                                    RT_BIT(1)
#define CODEC_F00_0F_D0                                    RT_BIT(0)

/* Processing capabilities 7.3.4.13 */
#define CODEC_MAKE_F00_10(num, benign)                     ((((num) & 0xFF) << 8) | ((benign) & 0x1))
#define CODEC_F00_10_NUM(f00_10)                           (((f00_10) & (0xFF << 8)) >> 8)
#define CODEC_F00_10_BENING(f00_10)                        ((f00_10) & 0x1)

/* CP/IO Count (7.3.4.14) */
#define CODEC_MAKE_F00_11(wake, unsol, numgpi, numgpo, numgpio) \
    (  (((wake) & 0x1) << 31)                                   \
     | (((unsol) & 0x1) << 30)                                  \
     | (((numgpi) & 0xFF) << 16)                                \
     | (((numgpo) & 0xFF) << 8)                                 \
     | ((numgpio) & 0xFF))

/* Processing States (7.3.3.4) */
#define CODEC_F03_OFF                                      (0)
#define CODEC_F03_ON                                       RT_BIT(0)
#define CODEC_F03_BENING                                   RT_BIT(1)
/* Power States (7.3.3.10) */
#define CODEC_MAKE_F05(reset, stopok, error, act, set)          \
    (   (((reset) & 0x1) << 10)                                 \
      | (((stopok) & 0x1) << 9)                                 \
      | (((error) & 0x1) << 8)                                  \
      | (((act) & 0x7) << 4)                                    \
      | ((set) & 0x7))
#define CODEC_F05_D3COLD                                   (4)
#define CODEC_F05_D3                                       (3)
#define CODEC_F05_D2                                       (2)
#define CODEC_F05_D1                                       (1)
#define CODEC_F05_D0                                       (0)

#define CODEC_F05_IS_RESET(value)                          (((value) & RT_BIT(10)) != 0)
#define CODEC_F05_IS_STOPOK(value)                         (((value) & RT_BIT(9)) != 0)
#define CODEC_F05_IS_ERROR(value)                          (((value) & RT_BIT(8)) != 0)
#define CODEC_F05_ACT(value)                               (((value) & 0x7) >> 4)
#define CODEC_F05_SET(value)                               (((value) & 0x7))

#define CODEC_F05_GE(p0, p1)                               ((p0) <= (p1))
#define CODEC_F05_LE(p0, p1)                               ((p0) >= (p1))

/* Pin Widged Control (7.3.3.13) */
#define CODEC_F07_VREF_HIZ                                 (0)
#define CODEC_F07_VREF_50                                  (0x1)
#define CODEC_F07_VREF_GROUND                              (0x2)
#define CODEC_F07_VREF_80                                  (0x4)
#define CODEC_F07_VREF_100                                 (0x5)
#define CODEC_F07_IN_ENABLE                                RT_BIT(5)
#define CODEC_F07_OUT_ENABLE                               RT_BIT(6)
#define CODEC_F07_OUT_H_ENABLE                             RT_BIT(7)

/* Unsolicited enabled (7.3.3.14) */
#define CODEC_MAKE_F08(enable, tag) ((((enable) & 1) << 7) | ((tag) & 0x3F))

/* Converter formats (7.3.3.8) and (3.7.1) */
#define CODEC_MAKE_A(fNonPCM, f44_1BaseRate, mult, div, bits, chan) \
    (  (((fNonPCM) & 0x1) << 15)                                    \
     | (((f44_1BaseRate) & 0x1) << 14)                              \
     | (((mult) & 0x7) << 11)                                       \
     | (((div) & 0x7) << 8)                                         \
     | (((bits) & 0x7) << 4)                                        \
     | ((chan) & 0xF))

#define CODEC_A_TYPE                                       RT_BIT(15)
#define CODEC_A_TYPE_PCM                                   (0)
#define CODEC_A_TYPE_NON_PCM                               (1)

#define CODEC_A_BASE                                       RT_BIT(14)
#define CODEC_A_BASE_48KHZ                                 (0)
#define CODEC_A_BASE_44KHZ                                 (1)

#define CODEC_A_MULT_1X                                    (0)
#define CODEC_A_MULT_2X                                    (1)
#define CODEC_A_MULT_3X                                    (2)
#define CODEC_A_MULT_4X                                    (3)

#define CODEC_A_DIV_1X                                     (0)
#define CODEC_A_DIV_2X                                     (1)
#define CODEC_A_DIV_3X                                     (2)
#define CODEC_A_DIV_4X                                     (3)
#define CODEC_A_DIV_5X                                     (4)
#define CODEC_A_DIV_6X                                     (5)
#define CODEC_A_DIV_7X                                     (6)
#define CODEC_A_DIV_8X                                     (7)

#define CODEC_A_8_BIT                                      (0)
#define CODEC_A_16_BIT                                     (1)
#define CODEC_A_20_BIT                                     (2)
#define CODEC_A_24_BIT                                     (3)
#define CODEC_A_32_BIT                                     (4)

#define CODEC_A_CHAN_MONO                                  (0)
#define CODEC_A_CHAN_STEREO                                (1)

/* Pin Sense (7.3.3.15) */
#define CODEC_MAKE_F09_ANALOG(fPresent, impedance)  \
(  (((fPresent) & 0x1) << 31)                       \
 | (((impedance) & 0x7FFFFFFF)))
#define CODEC_F09_ANALOG_NA    0x7FFFFFFF
#define CODEC_MAKE_F09_DIGITAL(fPresent, fELDValid) \
(   (((fPresent) & 0x1) << 31)                      \
  | (((fELDValid) & 0x1) << 30))

#define CODEC_MAKE_F0C(lrswap, eapd, btl) ((((lrswap) & 1) << 2) | (((eapd) & 1) << 1) | ((btl) & 1))
#define CODEC_FOC_IS_LRSWAP(f0c)                           RT_BOOL((f0c) & RT_BIT(2))
#define CODEC_FOC_IS_EAPD(f0c)                             RT_BOOL((f0c) & RT_BIT(1))
#define CODEC_FOC_IS_BTL(f0c)                              RT_BOOL((f0c) & RT_BIT(0))
/* HDA spec 7.3.3.31 defines layout of configuration registers/verbs (0xF1C) */
/* Configuration's port connection */
#define CODEC_F1C_PORT_MASK                                (0x3)
#define CODEC_F1C_PORT_SHIFT                               (30)

#define CODEC_F1C_PORT_COMPLEX                             (0x0)
#define CODEC_F1C_PORT_NO_PHYS                             (0x1)
#define CODEC_F1C_PORT_FIXED                               (0x2)
#define CODEC_F1C_BOTH                                     (0x3)

/* Configuration default: connection */
#define CODEC_F1C_PORT_MASK                                (0x3)
#define CODEC_F1C_PORT_SHIFT                               (30)

/* Connected to a jack (1/8", ATAPI, ...). */
#define CODEC_F1C_PORT_COMPLEX                             (0x0)
/* No physical connection. */
#define CODEC_F1C_PORT_NO_PHYS                             (0x1)
/* Fixed function device (integrated speaker, integrated mic, ...). */
#define CODEC_F1C_PORT_FIXED                               (0x2)
/* Both, a jack and an internal device are attached. */
#define CODEC_F1C_BOTH                                     (0x3)

/* Configuration default: Location */
#define CODEC_F1C_LOCATION_MASK                            (0x3F)
#define CODEC_F1C_LOCATION_SHIFT                           (24)

/* [4:5] bits of location region means chassis attachment */
#define CODEC_F1C_LOCATION_PRIMARY_CHASSIS                 (0)
#define CODEC_F1C_LOCATION_INTERNAL                        RT_BIT(4)
#define CODEC_F1C_LOCATION_SECONDRARY_CHASSIS              RT_BIT(5)
#define CODEC_F1C_LOCATION_OTHER                           RT_BIT(5)

/* [0:3] bits of location region means geometry location attachment */
#define CODEC_F1C_LOCATION_NA                              (0)
#define CODEC_F1C_LOCATION_REAR                            (0x1)
#define CODEC_F1C_LOCATION_FRONT                           (0x2)
#define CODEC_F1C_LOCATION_LEFT                            (0x3)
#define CODEC_F1C_LOCATION_RIGTH                           (0x4)
#define CODEC_F1C_LOCATION_TOP                             (0x5)
#define CODEC_F1C_LOCATION_BOTTOM                          (0x6)
#define CODEC_F1C_LOCATION_SPECIAL_0                       (0x7)
#define CODEC_F1C_LOCATION_SPECIAL_1                       (0x8)
#define CODEC_F1C_LOCATION_SPECIAL_2                       (0x9)

/* Configuration default: Device type */
#define CODEC_F1C_DEVICE_MASK                              (0xF)
#define CODEC_F1C_DEVICE_SHIFT                             (20)
#define CODEC_F1C_DEVICE_LINE_OUT                          (0)
#define CODEC_F1C_DEVICE_SPEAKER                           (0x1)
#define CODEC_F1C_DEVICE_HP                                (0x2)
#define CODEC_F1C_DEVICE_CD                                (0x3)
#define CODEC_F1C_DEVICE_SPDIF_OUT                         (0x4)
#define CODEC_F1C_DEVICE_DIGITAL_OTHER_OUT                 (0x5)
#define CODEC_F1C_DEVICE_MODEM_LINE_SIDE                   (0x6)
#define CODEC_F1C_DEVICE_MODEM_HANDSET_SIDE                (0x7)
#define CODEC_F1C_DEVICE_LINE_IN                           (0x8)
#define CODEC_F1C_DEVICE_AUX                               (0x9)
#define CODEC_F1C_DEVICE_MIC                               (0xA)
#define CODEC_F1C_DEVICE_PHONE                             (0xB)
#define CODEC_F1C_DEVICE_SPDIF_IN                          (0xC)
#define CODEC_F1C_DEVICE_RESERVED                          (0xE)
#define CODEC_F1C_DEVICE_OTHER                             (0xF)

/* Configuration default: Connection type */
#define CODEC_F1C_CONNECTION_TYPE_MASK                     (0xF)
#define CODEC_F1C_CONNECTION_TYPE_SHIFT                    (16)

#define CODEC_F1C_CONNECTION_TYPE_UNKNOWN                  (0)
#define CODEC_F1C_CONNECTION_TYPE_1_8INCHES                (0x1)
#define CODEC_F1C_CONNECTION_TYPE_1_4INCHES                (0x2)
#define CODEC_F1C_CONNECTION_TYPE_ATAPI                    (0x3)
#define CODEC_F1C_CONNECTION_TYPE_RCA                      (0x4)
#define CODEC_F1C_CONNECTION_TYPE_OPTICAL                  (0x5)
#define CODEC_F1C_CONNECTION_TYPE_OTHER_DIGITAL            (0x6)
#define CODEC_F1C_CONNECTION_TYPE_ANALOG                   (0x7)
#define CODEC_F1C_CONNECTION_TYPE_DIN                      (0x8)
#define CODEC_F1C_CONNECTION_TYPE_XLR                      (0x9)
#define CODEC_F1C_CONNECTION_TYPE_RJ_11                    (0xA)
#define CODEC_F1C_CONNECTION_TYPE_COMBO                    (0xB)
#define CODEC_F1C_CONNECTION_TYPE_OTHER                    (0xF)

/* Configuration's color */
#define CODEC_F1C_COLOR_MASK                               (0xF)
#define CODEC_F1C_COLOR_SHIFT                              (12)
#define CODEC_F1C_COLOR_UNKNOWN                            (0)
#define CODEC_F1C_COLOR_BLACK                              (0x1)
#define CODEC_F1C_COLOR_GREY                               (0x2)
#define CODEC_F1C_COLOR_BLUE                               (0x3)
#define CODEC_F1C_COLOR_GREEN                              (0x4)
#define CODEC_F1C_COLOR_RED                                (0x5)
#define CODEC_F1C_COLOR_ORANGE                             (0x6)
#define CODEC_F1C_COLOR_YELLOW                             (0x7)
#define CODEC_F1C_COLOR_PURPLE                             (0x8)
#define CODEC_F1C_COLOR_PINK                               (0x9)
#define CODEC_F1C_COLOR_RESERVED_0                         (0xA)
#define CODEC_F1C_COLOR_RESERVED_1                         (0xB)
#define CODEC_F1C_COLOR_RESERVED_2                         (0xC)
#define CODEC_F1C_COLOR_RESERVED_3                         (0xD)
#define CODEC_F1C_COLOR_WHITE                              (0xE)
#define CODEC_F1C_COLOR_OTHER                              (0xF)

/* Configuration's misc */
#define CODEC_F1C_MISC_MASK                                (0xF)
#define CODEC_F1C_MISC_SHIFT                               (8)
#define CODEC_F1C_MISC_JACK_DETECT                         (0)
#define CODEC_F1C_MISC_RESERVED_0                          (1)
#define CODEC_F1C_MISC_RESERVED_1                          (2)
#define CODEC_F1C_MISC_RESERVED_2                          (3)

/* Configuration default: Association */
#define CODEC_F1C_ASSOCIATION_MASK                         (0xF)
#define CODEC_F1C_ASSOCIATION_SHIFT                        (4)

/* Reserved; don't use. */
#define CODEC_F1C_ASSOCIATION_INVALID                      0x0
#define CODEC_F1C_ASSOCIATION_GROUP_0                      0x1
#define CODEC_F1C_ASSOCIATION_GROUP_1                      0x2
#define CODEC_F1C_ASSOCIATION_GROUP_2                      0x3
#define CODEC_F1C_ASSOCIATION_GROUP_3                      0x4
#define CODEC_F1C_ASSOCIATION_GROUP_4                      0x5
#define CODEC_F1C_ASSOCIATION_GROUP_5                      0x6
#define CODEC_F1C_ASSOCIATION_GROUP_6                      0x7
#define CODEC_F1C_ASSOCIATION_GROUP_7                      0x8
#define CODEC_F1C_ASSOCIATION_GROUP_15                     0xF

/* Configuration default: Association Sequence */
#define CODEC_F1C_SEQ_MASK                                 (0xF)
#define CODEC_F1C_SEQ_SHIFT                                (0)

/* Implementation identification (7.3.3.30) */
#define CODEC_MAKE_F20(bmid, bsku, aid)     \
    (  (((bmid) & 0xFFFF) << 16)            \
     | (((bsku) & 0xFF) << 8)               \
     | (((aid) & 0xFF))                     \
    )

/* macro definition helping in filling the configuration registers. */
#define CODEC_MAKE_F1C(port_connectivity, location, device, connection_type, color, misc, association, sequence)    \
    (  ((port_connectivity) << CODEC_F1C_PORT_SHIFT)          \
     | ((location) << CODEC_F1C_LOCATION_SHIFT)               \
     | ((device) << CODEC_F1C_DEVICE_SHIFT)                   \
     | ((connection_type) << CODEC_F1C_CONNECTION_TYPE_SHIFT) \
     | ((color) << CODEC_F1C_COLOR_SHIFT)                     \
     | ((misc) << CODEC_F1C_MISC_SHIFT)                       \
     | ((association) << CODEC_F1C_ASSOCIATION_SHIFT)         \
     | ((sequence)))


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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
#define STAC9220_NID_PIN_HEADPHONE0                        0xA  /* In, Out */
#define STAC9220_NID_PIN_B                                 0xB  /* In, Out */
#define STAC9220_NID_PIN_C                                 0xC  /* In, Out */
#define STAC9220_NID_PIN_HEADPHONE1                        0xD  /* In, Out */
#define STAC9220_NID_PIN_E                                 0xE  /* In */
#define STAC9220_NID_PIN_F                                 0xF  /* In, Out */
#define STAC9220_NID_PIN_SPDIF_OUT                         0x10 /* Out */
#define STAC9220_NID_PIN_SPDIF_IN                          0x11 /* In */
#define STAC9220_NID_ADC0_MUX                              0x12 /* In */
#define STAC9220_NID_ADC1_MUX                              0x13 /* In */
#define STAC9220_NID_PCBEEP                                0x14 /* Out */
#define STAC9220_NID_PIN_CD                                0x15 /* In */
#define STAC9220_NID_VOL_KNOB                              0x16
#define STAC9220_NID_AMP_ADC0                              0x17 /* In */
#define STAC9220_NID_AMP_ADC1                              0x18 /* In */
/* STAC9221. */
#define STAC9221_NID_ADAT_OUT                              0x19 /* Out */
#define STAC9221_NID_I2S_OUT                               0x1A /* Out */
#define STAC9221_NID_PIN_I2S_OUT                           0x1B /* Out */

#if 1
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
#else /** @todo Enable this after 5.0 -- needs more testing first. */
static uint8_t const g_abStac9220Ports[]      = { STAC9220_NID_PIN_HEADPHONE0, STAC9220_NID_PIN_B, STAC9220_NID_PIN_C, STAC9220_NID_PIN_HEADPHONE1, STAC9220_NID_PIN_E, STAC9220_NID_PIN_F, 0};
static uint8_t const g_abStac9220Dacs[]       = { STAC9220_NID_DAC0, STAC9220_NID_DAC1, STAC9220_NID_DAC2, STAC9220_NID_DAC3, 0};
static uint8_t const g_abStac9220Adcs[]       = { STAC9220_NID_ADC0, STAC9220_NID_ADC1, 0};
static uint8_t const g_abStac9220SpdifOuts[]  = { STAC9220_NID_SPDIF_OUT, 0 };
static uint8_t const g_abStac9220SpdifIns[]   = { STAC9220_NID_SPDIF_IN, 0 };
static uint8_t const g_abStac9220DigOutPins[] = { STAC9220_NID_PIN_SPDIF_OUT, 0 };
static uint8_t const g_abStac9220DigInPins[]  = { STAC9220_NID_PIN_SPDIF_IN, 0 };
static uint8_t const g_abStac9220AdcVols[]    = { STAC9220_NID_AMP_ADC0, STAC9220_NID_AMP_ADC1, 0};
static uint8_t const g_abStac9220AdcMuxs[]    = { STAC9220_NID_ADC0_MUX, STAC9220_NID_ADC1_MUX, 0};
static uint8_t const g_abStac9220Pcbeeps[]    = { STAC9220_NID_PCBEEP, 0 };
static uint8_t const g_abStac9220Cds[]        = { STAC9220_NID_PIN_CD, 0 };
static uint8_t const g_abStac9220VolKnobs[]   = { STAC9220_NID_VOL_KNOB, 0 };
/* STAC 9221. */
/** @todo Is STAC9220_NID_SPDIF_IN really correct for reserved nodes? */
static uint8_t const g_abStac9220Reserveds[]  = { STAC9220_NID_SPDIF_IN, STAC9221_NID_ADAT_OUT, STAC9221_NID_I2S_OUT, STAC9221_NID_PIN_I2S_OUT, 0 };
#endif

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




static DECLCALLBACK(void) stac9220DbgNodes(PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    for (int i = 1; i < 12; i++)
    {
        PCODECNODE pNode = &pThis->paNodes[i];
        AMPLIFIER *pAmp = &pNode->dac.B_params;

        uint8_t lVol = AMPLIFIER_REGISTER(*pAmp, AMPLIFIER_OUT, AMPLIFIER_LEFT, 0) & 0x7f;
        uint8_t rVol = AMPLIFIER_REGISTER(*pAmp, AMPLIFIER_OUT, AMPLIFIER_RIGHT, 0) & 0x7f;

        pHlp->pfnPrintf(pHlp, "0x%x: lVol=%RU8, rVol=%RU8\n", i, lVol, rVol);
    }
}


static DECLCALLBACK(int) stac9220ResetNode(PHDACODEC pThis, uint8_t nodenum, PCODECNODE pNode)
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
            pNode->dac.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//RT_BIT(14)|(0x1 << 4)|0x1; /* 44100Hz/16bit/2ch */

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
            pNode->adc.u32A_param = CODEC_MAKE_A(0, 1, CODEC_A_MULT_1X, CODEC_A_DIV_1X, CODEC_A_16_BIT, 1);//RT_BIT(14)|(0x1 << 3)|0x1; /* 44100Hz/16bit/2ch */
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
            pNode->node.au32F00_param[0xa] = pThis->paNodes[1].node.au32F00_param[0xA];
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
            pNode->node.au32F00_param[0xA] = pThis->paNodes[1].node.au32F00_param[0xA];
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
            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_HP,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          CODEC_F1C_MISC_JACK_DETECT,
                                                          0x2, 0);//RT_MAKE_U32_FROM_U8(0x20, 0x40, 0x21, 0x02);
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(0, CODEC_F09_ANALOG_NA);//0x7fffffff;
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
            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_INTERNAL|CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_BLACK,
                                                          CODEC_F1C_MISC_JACK_DETECT,
                                                          0x1, 0x1);//RT_MAKE_U32_FROM_U8(0x11, 0x60, 0x11, 0x01);
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1, CODEC_F09_ANALOG_NA);//RT_BIT(31)|0x7fffffff;
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
            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_REAR,
                                                          CODEC_F1C_DEVICE_SPEAKER,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_GREEN,
                                                          0x0, 0x1, 0x0);//RT_MAKE_U32_FROM_U8(0x10, 0x40, 0x11, 0x01);
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1, CODEC_F09_ANALOG_NA);//RT_BIT(31)|0x7fffffff;
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
            if (!pThis->fInReset)
                pNode->port.u32F1c_param = CODEC_MAKE_F1C(CODEC_F1C_PORT_COMPLEX,
                                                          CODEC_F1C_LOCATION_FRONT,
                                                          CODEC_F1C_DEVICE_MIC,
                                                          CODEC_F1C_CONNECTION_TYPE_1_8INCHES,
                                                          CODEC_F1C_COLOR_PINK,
                                                          0x0, 0x5, 0x0);//RT_MAKE_U32_FROM_U8(0x50, 0x90, 0xA1, 0x02); /* Microphone */
            pNode->port.u32F09_param = CODEC_MAKE_F09_ANALOG(1, CODEC_F09_ANALOG_NA);//RT_BIT(31)|0x7fffffff;
        port_init:
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
            if (!pThis->fInReset)
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
            if (!pThis->fInReset)
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
            if (!pThis->fInReset)
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
            pNode->digin.u32F09_param = CODEC_MAKE_F09_DIGITAL(0, 0);
            pNode->digin.u32F0c_param = 0;
            if (!pThis->fInReset)
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
            if (!pThis->fInReset)
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


static int stac9220Construct(PHDACODEC pThis)
{
    unconst(pThis->cTotalNodes) = 0x1C;
    pThis->pfnCodecNodeReset = stac9220ResetNode;
    pThis->pfnDbgListNodes   = stac9220DbgNodes;
    pThis->u16VendorId = 0x8384;
    pThis->u16DeviceId = 0x7680;
    pThis->u8BSKU = 0x76;
    pThis->u8AssemblyId = 0x80;
    pThis->paNodes = (PCODECNODE)RTMemAllocZ(sizeof(CODECNODE) * pThis->cTotalNodes);
    if (!pThis->paNodes)
        return VERR_NO_MEMORY;
    pThis->fInReset = false;
#define STAC9220WIDGET(type) pThis->au8##type##s = g_abStac9220##type##s
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
    unconst(pThis->u8AdcVolsLineIn) = 0x17;
    unconst(pThis->u8DacLineOut) = 0x3;

    return VINF_SUCCESS;
}


/*
 * Some generic predicate functions.
 */

#define DECLISNODEOFTYPE(type)                                                                  \
    DECLINLINE(int) hdaCodecIs##type##Node(PHDACODEC pThis, uint8_t cNode)                     \
    {                                                                                           \
        Assert(pThis->au8##type##s);                                                           \
        for (int i = 0; pThis->au8##type##s[i] != 0; ++i)                                      \
            if (pThis->au8##type##s[i] == cNode)                                               \
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


/*
 * Misc helpers.
 */
static int hdaCodecToAudVolume(PHDACODEC pThis, AMPLIFIER *pAmp, PDMAUDIOMIXERCTL mt)
{
    uint32_t dir = AMPLIFIER_OUT;
    ENMSOUNDSOURCE enmSrc;
    switch (mt)
    {
        case PDMAUDIOMIXERCTL_PCM:
            enmSrc = PO_INDEX;
            dir = AMPLIFIER_OUT;
            break;
        case PDMAUDIOMIXERCTL_LINE_IN:
            enmSrc = PI_INDEX;
            dir = AMPLIFIER_IN;
            break;
        default:
            AssertMsgFailedReturn(("Invalid mixer control %ld\n", mt), VERR_INVALID_PARAMETER);
            break;
    }

    int mute = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_LEFT, 0) & RT_BIT(7);
    mute |= AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_RIGHT, 0) & RT_BIT(7);
    mute >>=7;
    mute &= 0x1;
    uint8_t lVol = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_LEFT, 0) & 0x7f;
    uint8_t rVol = AMPLIFIER_REGISTER(*pAmp, dir, AMPLIFIER_RIGHT, 0) & 0x7f;

    /* The STAC9220 volume controls have 0 to -96dB attenuation range in 128 steps.
     * We have 0 to -96dB range in 256 steps. HDA volume setting of 127 must map
     * to 255 internally (0dB), while HDA volume setting of 0 (-96dB) should map
     * to 1 (rather than zero) internally.
     */
    lVol = (lVol + 1) * (2 * 255) / 256;
    rVol = (rVol + 1) * (2 * 255) / 256;

    return pThis->pfnSetVolume(pThis->pHDAState, enmSrc, RT_BOOL(mute), lVol, rVol);
}

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

static DECLCALLBACK(int) vrbProcUnimplemented(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    LogFlowFunc(("cmd(raw:%x: cad:%x, d:%c, nid:%x, verb:%x)\n", cmd,
        CODEC_CAD(cmd), CODEC_DIRECT(cmd) ? 'N' : 'Y', CODEC_NID(cmd), CODEC_VERBDATA(cmd)));
    *pResp = 0;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcBreak(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    int rc;
    rc = vrbProcUnimplemented(pThis, cmd, pResp);
    *pResp |= CODEC_RESPONSE_UNSOLICITED;
    return rc;
}

/* B-- */
static DECLCALLBACK(int) vrbProcGetAmplifier(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    /* HDA spec 7.3.3.7 Note A */
    /** @todo: if index out of range response should be 0 */
    uint8_t u8Index = CODEC_GET_AMP_DIRECTION(cmd) == AMPLIFIER_OUT? 0 : CODEC_GET_AMP_INDEX(cmd);

    PCODECNODE pNode = &pThis->paNodes[CODEC_NID(cmd)];
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
        AssertMsgFailedReturn(("access to fields of %x need to be implemented\n", CODEC_NID(cmd)), VINF_SUCCESS);
    return VINF_SUCCESS;
}

/* 3-- */
static DECLCALLBACK(int) vrbProcSetAmplifier(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    bool fIsLeft = false;
    bool fIsRight = false;
    bool fIsOut = false;
    bool fIsIn = false;
    uint8_t u8Index = 0;
    Assert(CODEC_CAD(cmd) == pThis->id);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    PCODECNODE pNode = &pThis->paNodes[CODEC_NID(cmd)];
    AMPLIFIER *pAmplifier;
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
        AssertFailedReturn(VINF_SUCCESS);

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

        /** @todo Fix ID of u8AdcVolsLineIn! */
        hdaCodecToAudVolume(pThis, pAmplifier, PDMAUDIOMIXERCTL_LINE_IN);
    }
    if (fIsOut)
    {
        if (fIsLeft)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_LEFT, u8Index), cmd, 0);
        if (fIsRight)
            hdaCodecSetRegisterU8(&AMPLIFIER_REGISTER(*pAmplifier, AMPLIFIER_OUT, AMPLIFIER_RIGHT, u8Index), cmd, 0);

        if (CODEC_NID(cmd) == pThis->u8DacLineOut)
            hdaCodecToAudVolume(pThis, pAmplifier, PDMAUDIOMIXERCTL_PCM);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetParameter(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F00_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F00_PARAM_LENGTH)
    {
        LogFlowFunc(("invalid F00 parameter %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = pThis->paNodes[CODEC_NID(cmd)].node.au32F00_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F01 */
static DECLCALLBACK(int) vrbProcGetConSelectCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    return VINF_SUCCESS;
}

/* 701 */
static DECLCALLBACK(int) vrbProcSetConSelectCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (hdaCodecIsAdcMuxNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].adcmux.u32F01_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digout.u32F01_param;
    else if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].port.u32F01_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].adc.u32F01_param;
    else if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].adcvol.u32F01_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F07 */
static DECLCALLBACK(int) vrbProcGetPinCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    else
        AssertMsgFailed(("Unsupported"));
    return VINF_SUCCESS;
}

/* 707 */
static DECLCALLBACK(int) vrbProcSetPinCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].port.u32F07_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F07_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digout.u32F07_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].cdnode.u32F07_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].pcbeep.u32F07_param;
    else if (   hdaCodecIsReservedNode(pThis, CODEC_NID(cmd))
             && CODEC_NID(cmd) == 0x1b)
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].reserved.u32F07_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F08 */
static DECLCALLBACK(int) vrbProcGetUnsolicitedEnabled(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if ((cmd) == 1 /* AFG */)
        *pResp = pThis->paNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 708 */
static DECLCALLBACK(int) vrbProcSetUnsolicitedEnabled(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].port.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].afg.u32F08_param;
    else if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].volumeKnob.u32F08_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F08_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digout.u32F08_param;
    else
        AssertMsgFailedReturn(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)), VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F09 */
static DECLCALLBACK(int) vrbProcGetPinSense(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
        AssertMsgFailed(("unsupported operation %x on node: %x\n", CODEC_VERB_CMD8(cmd), CODEC_NID(cmd)));
    return VINF_SUCCESS;
}

/* 709 */
static DECLCALLBACK(int) vrbProcSetPinSense(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].port.u32F09_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F09_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetConnectionListEntry(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    *pResp = 0;
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    Assert((cmd & CODEC_VERB_8BIT_DATA) < CODECNODE_F02_PARAM_LENGTH);
    if ((cmd & CODEC_VERB_8BIT_DATA) >= CODECNODE_F02_PARAM_LENGTH)
    {
        LogFlowFunc(("access to invalid F02 index %d\n", (cmd & CODEC_VERB_8BIT_DATA)));
        return VINF_SUCCESS;
    }
    *pResp = pThis->paNodes[CODEC_NID(cmd)].node.au32F02_param[cmd & CODEC_VERB_8BIT_DATA];
    return VINF_SUCCESS;
}

/* F03 */
static DECLCALLBACK(int) vrbProcGetProcessingState(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adc.u32F03_param;
    return VINF_SUCCESS;
}

/* 703 */
static DECLCALLBACK(int) vrbProcSetProcessingState(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->paNodes[CODEC_NID(cmd)].adc.u32F03_param, cmd, 0);
    return VINF_SUCCESS;
}

/* F0D */
static DECLCALLBACK(int) vrbProcGetDigitalConverter(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F0d_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F0d_param;
    return VINF_SUCCESS;
}

static int codecSetDigitalConverter(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F0d_param, cmd, u8Offset);
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU8(&pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F0d_param, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 70D */
static DECLCALLBACK(int) vrbProcSetDigitalConverter1(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    return codecSetDigitalConverter(pThis, cmd, 0, pResp);
}

/* 70E */
static DECLCALLBACK(int) vrbProcSetDigitalConverter2(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    return codecSetDigitalConverter(pThis, cmd, 8, pResp);
}

/* F20 */
static DECLCALLBACK(int) vrbProcGetSubId(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    if (CODEC_NID(cmd) == 1 /* AFG */)
        *pResp = pThis->paNodes[CODEC_NID(cmd)].afg.u32F20_param;
    else
        *pResp = 0;
    return VINF_SUCCESS;
}

static int codecSetSubIdX(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg;
    if (CODEC_NID(cmd) == 0x1 /* AFG */)
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].afg.u32F20_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 720 */
static DECLCALLBACK(int) vrbProcSetSubId0(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 0);
}

/* 721 */
static DECLCALLBACK(int) vrbProcSetSubId1(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 8);
}

/* 722 */
static DECLCALLBACK(int) vrbProcSetSubId2(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 16);
}

/* 723 */
static DECLCALLBACK(int) vrbProcSetSubId3(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetSubIdX(pThis, cmd, 24);
}

static DECLCALLBACK(int) vrbProcReset(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) == 1 /* AFG */);
    if (   CODEC_NID(cmd) == 1 /* AFG */
        && pThis->pfnCodecNodeReset)
    {
        uint8_t i;
        LogFlowFunc(("enters reset\n"));
        Assert(pThis->pfnCodecNodeReset);
        for (i = 0; i < pThis->cTotalNodes; ++i)
        {
            pThis->pfnCodecNodeReset(pThis, i, &pThis->paNodes[i]);
        }
        pThis->fInReset = false;
        LogFlowFunc(("exits reset\n"));
    }
    *pResp = 0;
    return VINF_SUCCESS;
}

/* F05 */
static DECLCALLBACK(int) vrbProcGetPowerState(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        *pResp = pThis->paNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].reserved.u32F05_param;
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

static DECLCALLBACK(int) vrbProcSetPowerState(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32Reg;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].afg.u32F05_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].dac.u32F05_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F05_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].adc.u32F05_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F05_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F05_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].reserved.u32F05_param;
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
                                  CODEC_F05_ACT(pThis->paNodes[1].afg.u32F05_param),
                                  CODEC_F05_SET(cmd));
    }

    /* Propagate next power state only if AFG is on or verb modifies AFG power state */
    if (   CODEC_NID(cmd) == 1 /* AFG */
        || !CODEC_F05_ACT(pThis->paNodes[1].afg.u32F05_param))
    {
        *pu32Reg = CODEC_MAKE_F05(fReset, fStopOk, 0, CODEC_F05_SET(cmd), CODEC_F05_SET(cmd));
        if (   CODEC_NID(cmd) == 1 /* AFG */
            && (CODEC_F05_SET(cmd)) == CODEC_F05_D0)
        {
            /* now we're powered on AFG and may propogate power states on nodes */
            const uint8_t *pu8NodeIndex = &pThis->au8Dacs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->paNodes[*pu8NodeIndex].dac.u32F05_param);

            pu8NodeIndex = &pThis->au8Adcs[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->paNodes[*pu8NodeIndex].adc.u32F05_param);

            pu8NodeIndex = &pThis->au8DigInPins[0];
            while (*(++pu8NodeIndex))
                codecPropogatePowerState(&pThis->paNodes[*pu8NodeIndex].digin.u32F05_param);
        }
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetStreamId(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (CODEC_NID(cmd) == 0x1A)
        *pResp = pThis->paNodes[CODEC_NID(cmd)].reserved.u32F06_param;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcSetStreamId(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    uint32_t *pu32addr;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32addr = &pThis->paNodes[CODEC_NID(cmd)].dac.u32F06_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        pu32addr = &pThis->paNodes[CODEC_NID(cmd)].adc.u32F06_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        pu32addr = &pThis->paNodes[CODEC_NID(cmd)].spdifout.u32F06_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        pu32addr = &pThis->paNodes[CODEC_NID(cmd)].spdifin.u32F06_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32addr = &pThis->paNodes[CODEC_NID(cmd)].reserved.u32F06_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32addr, cmd, 0);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcGetConverterFormat(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].dac.u32A_param;
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adc.u32A_param;
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifout.u32A_param;
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].spdifin.u32A_param;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vrbProcSetConverterFormat(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->paNodes[CODEC_NID(cmd)].dac.u32A_param, cmd, 0);
    else if (hdaCodecIsAdcNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->paNodes[CODEC_NID(cmd)].adc.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifOutNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->paNodes[CODEC_NID(cmd)].spdifout.u32A_param, cmd, 0);
    else if (hdaCodecIsSpdifInNode(pThis, CODEC_NID(cmd)))
        hdaCodecSetRegisterU16(&pThis->paNodes[CODEC_NID(cmd)].spdifin.u32A_param, cmd, 0);
    return VINF_SUCCESS;
}

/* F0C */
static DECLCALLBACK(int) vrbProcGetEAPD_BTLEnabled(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    return VINF_SUCCESS;
}

/* 70C */
static DECLCALLBACK(int) vrbProcSetEAPD_BTLEnabled(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }

    *pResp = 0;
    uint32_t *pu32Reg;
    if (hdaCodecIsAdcVolNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].adcvol.u32F0c_param;
    else if (hdaCodecIsDacNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].dac.u32F0c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F0c_param;
    else
        AssertFailedReturn(VINF_SUCCESS);
    hdaCodecSetRegisterU8(pu32Reg, cmd, 0);

    return VINF_SUCCESS;
}

/* F0F */
static DECLCALLBACK(int) vrbProcGetVolumeKnobCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    return VINF_SUCCESS;
}

/* 70F */
static DECLCALLBACK(int) vrbProcSetVolumeKnobCtrl(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (hdaCodecIsVolKnobNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].volumeKnob.u32F0f_param;
    Assert(pu32Reg);
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F17 */
static DECLCALLBACK(int) vrbProcGetGPIOUnsolisted(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    /* note: this is true for ALC885 */
    if (CODEC_NID(cmd) == 0x1 /* AFG */)
        *pResp = pThis->paNodes[1].afg.u32F17_param;
    return VINF_SUCCESS;
}

/* 717 */
static DECLCALLBACK(int) vrbProcSetGPIOUnsolisted(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    *pResp = 0;
    if (CODEC_NID(cmd) == 1 /* AFG */)
        pu32Reg = &pThis->paNodes[1].afg.u32F17_param;
    Assert(pu32Reg);
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, 0);
    return VINF_SUCCESS;
}

/* F1C */
static DECLCALLBACK(int) vrbProcGetConfig(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    *pResp = 0;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        *pResp = pThis->paNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    return VINF_SUCCESS;
}

static int codecSetConfigX(PHDACODEC pThis, uint32_t cmd, uint8_t u8Offset)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    Assert(CODEC_NID(cmd) < pThis->cTotalNodes);
    if (CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        LogFlowFunc(("invalid node address %d\n", CODEC_NID(cmd)));
        return VINF_SUCCESS;
    }
    uint32_t *pu32Reg = NULL;
    if (hdaCodecIsPortNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].port.u32F1c_param;
    else if (hdaCodecIsDigInPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digin.u32F1c_param;
    else if (hdaCodecIsDigOutPinNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].digout.u32F1c_param;
    else if (hdaCodecIsCdNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].cdnode.u32F1c_param;
    else if (hdaCodecIsPcbeepNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].pcbeep.u32F1c_param;
    else if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        pu32Reg = &pThis->paNodes[CODEC_NID(cmd)].reserved.u32F1c_param;
    Assert(pu32Reg);
    if (pu32Reg)
        hdaCodecSetRegisterU8(pu32Reg, cmd, u8Offset);
    return VINF_SUCCESS;
}

/* 71C */
static DECLCALLBACK(int) vrbProcSetConfig0(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 0);
}

/* 71D */
static DECLCALLBACK(int) vrbProcSetConfig1(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 8);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig2(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 16);
}

/* 71E */
static DECLCALLBACK(int) vrbProcSetConfig3(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp)
{
    *pResp = 0;
    return codecSetConfigX(pThis, cmd, 24);
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

#ifdef DEBUG
typedef struct CODECDBGINFO
{
    /** DBGF info helpers. */
    PCDBGFINFOHLP pHlp;
    /** Current recursion level. */
    uint8_t uLevel;
    /** Pointer to codec state. */
    PHDACODEC pThis;

} CODECDBGINFO, *PCODECDBGINFO;

#define CODECDBG_INDENT   pInfo->uLevel++;
#define CODECDBG_UNINDENT if (pInfo->uLevel) pInfo->uLevel--;

#define CODECDBG_PRINT(...)  pInfo->pHlp->pfnPrintf(pInfo->pHlp, __VA_ARGS__)
#define CODECDBG_PRINTI(...) codecDbgPrintf(pInfo, __VA_ARGS__)

static void codecDbgPrintfIndentV(PCODECDBGINFO pInfo, uint16_t uIndent, const char *pszFormat, va_list va)
{
    char *pszValueFormat;
    if (RTStrAPrintfV(&pszValueFormat, pszFormat, va))
    {
        pInfo->pHlp->pfnPrintf(pInfo->pHlp, "%*s%s", uIndent, "", pszValueFormat);
        RTStrFree(pszValueFormat);
    }
}

static void codecDbgPrintf(PCODECDBGINFO pInfo, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    codecDbgPrintfIndentV(pInfo, pInfo->uLevel * 4, pszFormat, va);
    va_end(va);
}

/* Power state */
static void codecDbgPrintNodeRegF05(PCODECDBGINFO pInfo, uint32_t u32Reg)
{
    codecDbgPrintf(pInfo, "Power (F05): fReset=%RTbool, fStopOk=%RTbool, Set=%RU8, Act=%RU8\n",
                   CODEC_F05_IS_RESET(u32Reg), CODEC_F05_IS_STOPOK(u32Reg), CODEC_F05_SET(u32Reg), CODEC_F05_ACT(u32Reg));
}

static void codecDbgPrintNodeRegA(PCODECDBGINFO pInfo, uint32_t u32Reg)
{
    codecDbgPrintf(pInfo, "RegA: %x\n", u32Reg);
}

static void codecDbgPrintNodeRegF00(PCODECDBGINFO pInfo, uint32_t *paReg00)
{
    codecDbgPrintf(pInfo, "Parameters (F00):\n");

    CODECDBG_INDENT
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
                           RT_BOOL(CODEC_F00_0D_IS_CAP_MUTE(uReg)));
        CODECDBG_UNINDENT
    CODECDBG_UNINDENT
}

static void codecDbgPrintNodeAmp(PCODECDBGINFO pInfo, uint32_t *paReg, uint8_t uIdx, uint8_t uDir)
{
#define CODECDBG_AMP(reg, chan) \
    codecDbgPrintf(pInfo, "Amp %RU8 %s %s: In=%RTbool, Out=%RTbool, Left=%RTbool, Right=%RTbool, Idx=%RU8, fMute=%RTbool, uGain=%RU8\n", \
                   uIdx, chan, uDir == AMPLIFIER_IN ? "In" : "Out", \
                   RT_BOOL(CODEC_SET_AMP_IS_IN_DIRECTION(reg)), RT_BOOL(CODEC_SET_AMP_IS_OUT_DIRECTION(reg)), \
                   RT_BOOL(CODEC_SET_AMP_IS_LEFT_SIDE(reg)), RT_BOOL(CODEC_SET_AMP_IS_RIGHT_SIDE(reg)), \
                   CODEC_SET_AMP_INDEX(reg), RT_BOOL(CODEC_SET_AMP_MUTE(reg)), CODEC_SET_AMP_GAIN(reg));

    uint32_t regAmp = AMPLIFIER_REGISTER(paReg, uDir, AMPLIFIER_LEFT, uIdx);
    CODECDBG_AMP(regAmp, "Left");
    regAmp = AMPLIFIER_REGISTER(paReg, uDir, AMPLIFIER_RIGHT, uIdx);
    CODECDBG_AMP(regAmp, "Right");

#undef CODECDBG_AMP
}

static void codecDbgPrintNodeConnections(PCODECDBGINFO pInfo, PCODECNODE pNode)
{
    if (pNode->node.au32F00_param[0xE] == 0) /* Directly connected to HDA link. */
    {
         codecDbgPrintf(pInfo, "[HDA LINK]\n");
         return;
    }
}

static void codecDbgPrintNode(PCODECDBGINFO pInfo, PCODECNODE pNode)
{
    codecDbgPrintf(pInfo, "Node 0x%02x (%02RU8): ", pNode->node.id, pNode->node.id);

    if (pNode->node.id == STAC9220_NID_ROOT)
    {
        CODECDBG_PRINT("ROOT\n");
    }
    else if (pNode->node.id == STAC9220_NID_AFG)
    {
        CODECDBG_PRINT("AFG\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->afg.u32F05_param);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsPortNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("PORT\n");
    }
    else if (hdaCodecIsDacNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("DAC\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->dac.u32F05_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->dac.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->dac.B_params, 0, AMPLIFIER_OUT);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcVolNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("ADC VOLUME\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adcvol.u32A_params);
            codecDbgPrintNodeAmp   (pInfo, pNode->adcvol.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("ADC\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegF05(pInfo, pNode->adc.u32F05_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adc.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->adc.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsAdcMuxNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("ADC MUX\n");
        CODECDBG_INDENT
            codecDbgPrintNodeRegF00(pInfo, pNode->node.au32F00_param);
            codecDbgPrintNodeRegA  (pInfo, pNode->adcmux.u32A_param);
            codecDbgPrintNodeAmp   (pInfo, pNode->adcmux.B_params, 0, AMPLIFIER_IN);
        CODECDBG_UNINDENT
    }
    else if (hdaCodecIsPcbeepNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("PC BEEP\n");
    }
    else if (hdaCodecIsSpdifOutNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("SPDIF OUT\n");
    }
    else if (hdaCodecIsSpdifInNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("SPDIF IN\n");
    }
    else if (hdaCodecIsDigInPinNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("DIGITAL IN PIN\n");
    }
    else if (hdaCodecIsDigOutPinNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("DIGITAL OUT PIN\n");
    }
    else if (hdaCodecIsCdNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("CD\n");
    }
    else if (hdaCodecIsVolKnobNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("VOLUME KNOB\n");
    }
    else if (hdaCodecIsReservedNode(pInfo->pThis, pNode->node.id))
    {
        CODECDBG_PRINT("RESERVED\n");
    }
    else
        CODECDBG_PRINT("UNKNOWN TYPE 0x%x\n", pNode->node.id);
}

static DECLCALLBACK(void) codecDbgListNodes(PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    pHlp->pfnPrintf(pHlp, "HDA LINK\n");

    CODECDBGINFO dbgInfo;
    dbgInfo.pHlp   = pHlp;
    dbgInfo.pThis  = pThis;
    dbgInfo.uLevel = 0;

    PCODECDBGINFO pInfo = &dbgInfo;

    CODECDBG_INDENT
        for (uint8_t i = 0; i < pThis->cTotalNodes; i++)
        {
            PCODECNODE pNode = &pThis->paNodes[i];
            if (pNode->node.au32F00_param[0xE] == 0) /* Start with all nodes connected directly to the HDA (Azalia) link. */
                codecDbgPrintNode(&dbgInfo, pNode);
        }
    CODECDBG_UNINDENT
}

static DECLCALLBACK(void) codecDbgSelector(PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs)
{

}
#endif

static DECLCALLBACK(int) codecLookup(PHDACODEC pThis, uint32_t cmd, PPFNHDACODECVERBPROCESSOR pfn)
{
    Assert(CODEC_CAD(cmd) == pThis->id);
    if (hdaCodecIsReservedNode(pThis, CODEC_NID(cmd)))
        LogFlowFunc(("cmd %x was addressed to reserved node\n", cmd));

    if (   CODEC_VERBDATA(cmd) == 0
        || CODEC_NID(cmd) >= pThis->cTotalNodes)
    {
        *pfn = vrbProcUnimplemented;
        /// @todo r=michaln: There needs to be a counter to avoid log flooding (see e.g. DevRTC.cpp)
        LogFlowFunc(("cmd %x was ignored\n", cmd));
        return VINF_SUCCESS;
    }

    for (int i = 0; i < pThis->cVerbs; ++i)
    {
        if ((CODEC_VERBDATA(cmd) & pThis->paVerbs[i].mask) == pThis->paVerbs[i].verb)
        {
            *pfn = pThis->paVerbs[i].pfn;
            return VINF_SUCCESS;
        }
    }

    *pfn = vrbProcUnimplemented;
    LogFlowFunc(("callback for %x wasn't found\n", CODEC_VERBDATA(cmd)));
    return VINF_SUCCESS;
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
int hdaCodecOpenStream(PHDACODEC pThis, ENMSOUNDSOURCE enmSoundSource, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    int rc;
    switch (enmSoundSource)
    {
        case PI_INDEX:
            rc = pThis->pfnOpenIn(pThis->pHDAState,  "hda.in", PDMAUDIORECSOURCE_LINE_IN, pCfg);
            break;
#ifdef VBOX_WITH_HDA_MIC_IN
        case MC_INDEX:
            rc = pThis->pfnOpenIn(pThis->pHDAState,  "hda.mc", PDMAUDIORECSOURCE_MIC, pCfg);
            break;
#endif
        case PO_INDEX:
            rc = pThis->pfnOpenOut(pThis->pHDAState, "hda.out", pCfg);
            break;

        default:
            AssertMsgFailed(("Index %ld not implemented\n", enmSoundSource));
            rc = VERR_NOT_IMPLEMENTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int hdaCodecSaveState(PHDACODEC pThis, PSSMHANDLE pSSM)
{
    AssertLogRelMsgReturn(pThis->cTotalNodes == 0x1c, ("cTotalNodes=%#x, should be 0x1c", pThis->cTotalNodes),
                          VERR_INTERNAL_ERROR);
    SSMR3PutU32(pSSM, pThis->cTotalNodes);
    for (unsigned idxNode = 0; idxNode < pThis->cTotalNodes; ++idxNode)
        SSMR3PutStructEx(pSSM, &pThis->paNodes[idxNode].SavedState, sizeof(pThis->paNodes[idxNode].SavedState),
                         0 /*fFlags*/, g_aCodecNodeFields, NULL /*pvUser*/);
    return VINF_SUCCESS;
}

int hdaCodecLoadState(PHDACODEC pThis, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PCSSMFIELD pFields;
    uint32_t   fFlags;
    switch (uVersion)
    {
        case HDA_SSM_VERSION_1:
            AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
            pFields = g_aCodecNodeFieldsV1;
            fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
            break;

        case HDA_SSM_VERSION_2:
        case HDA_SSM_VERSION_3:
            AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);
            pFields = g_aCodecNodeFields;
            fFlags  = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
            break;

        /* Since version 4 a flexible node count is supported. */
        case HDA_SSM_VERSION_4:
        case HDA_SSM_VERSION_5:
        case HDA_SSM_VERSION:
        {
            uint32_t cNodes;
            int rc2 = SSMR3GetU32(pSSM, &cNodes);
            AssertRCReturn(rc2, rc2);
            if (cNodes != 0x1c)
                return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
            AssertReturn(pThis->cTotalNodes == 0x1c, VERR_INTERNAL_ERROR);

            pFields = g_aCodecNodeFields;
            fFlags  = 0;
            break;
        }

        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    for (unsigned idxNode = 0; idxNode < pThis->cTotalNodes; ++idxNode)
    {
        uint8_t idOld = pThis->paNodes[idxNode].SavedState.Core.id;
        int rc = SSMR3GetStructEx(pSSM, &pThis->paNodes[idxNode].SavedState,
                                  sizeof(pThis->paNodes[idxNode].SavedState),
                                  fFlags, pFields, NULL);
        if (RT_FAILURE(rc))
            return rc;
        AssertLogRelMsgReturn(idOld == pThis->paNodes[idxNode].SavedState.Core.id,
                              ("loaded %#x, expected %#x\n", pThis->paNodes[idxNode].SavedState.Core.id, idOld),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    /*
     * Update stuff after changing the state.
     */
    if (hdaCodecIsDacNode(pThis, pThis->u8DacLineOut))
        hdaCodecToAudVolume(pThis, &pThis->paNodes[pThis->u8DacLineOut].dac.B_params, PDMAUDIOMIXERCTL_PCM);
    else if (hdaCodecIsSpdifOutNode(pThis, pThis->u8DacLineOut))
        hdaCodecToAudVolume(pThis, &pThis->paNodes[pThis->u8DacLineOut].spdifout.B_params, PDMAUDIOMIXERCTL_PCM);
    hdaCodecToAudVolume(pThis, &pThis->paNodes[pThis->u8AdcVolsLineIn].adcvol.B_params, PDMAUDIOMIXERCTL_LINE_IN);

    return VINF_SUCCESS;
}

int hdaCodecDestruct(PHDACODEC pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (pThis->paNodes)
    {
        RTMemFree(pThis->paNodes);
        pThis->paNodes = NULL;
    }

    return VINF_SUCCESS;
}

int hdaCodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis,
                      uint16_t uLUN, PCFGMNODE pCfg)
{
    AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    pThis->id        = uLUN;
    pThis->paVerbs   = &g_aCodecVerbs[0];
    pThis->cVerbs    = RT_ELEMENTS(g_aCodecVerbs);

    pThis->pfnLookup       = codecLookup;
#ifdef DEBUG
    pThis->pfnDbgSelector  = codecDbgSelector;
    pThis->pfnDbgListNodes = codecDbgListNodes;
#endif
    int rc = stac9220Construct(pThis);
    AssertRC(rc);

    /* common root node initializers */
    pThis->paNodes[0].node.au32F00_param[0] = CODEC_MAKE_F00_00(pThis->u16VendorId, pThis->u16DeviceId);
    pThis->paNodes[0].node.au32F00_param[4] = CODEC_MAKE_F00_04(0x1, 0x1);
    /* common AFG node initializers */
    pThis->paNodes[1].node.au32F00_param[4] = CODEC_MAKE_F00_04(0x2, pThis->cTotalNodes - 2);
    pThis->paNodes[1].node.au32F00_param[5] = CODEC_MAKE_F00_05(1, CODEC_F00_05_AFG);
    pThis->paNodes[1].afg.u32F20_param = CODEC_MAKE_F20(pThis->u16VendorId, pThis->u8BSKU, pThis->u8AssemblyId);

    /* This codec uses a fixed setting (44.1 kHz, 16-bit signed, 2 channels). */
    pThis->strmCfg.uHz           = 44100;
    pThis->strmCfg.cChannels     = 2;
    pThis->strmCfg.enmFormat     = AUD_FMT_S16;
    pThis->strmCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

    hdaCodecOpenStream(pThis, PI_INDEX, &pThis->strmCfg);
#ifdef VBOX_WITH_HDA_MIC_IN
    hdaCodecOpenStream(pThis, MC_INDEX, &pThis->strmCfg);
#endif
    hdaCodecOpenStream(pThis, PO_INDEX, &pThis->strmCfg);

    /* Initialize the AFG node with the fixed setting. */
    pThis->paNodes[1].node.au32F00_param[0xA] = CODEC_F00_0A_44_1KHZ | CODEC_F00_0A_16_BIT;

    AssertPtr(pThis->paNodes);
    AssertPtr(pThis->pfnCodecNodeReset);

    for (uint8_t i = 0; i < pThis->cTotalNodes; i++)
        pThis->pfnCodecNodeReset(pThis, i, &pThis->paNodes[i]);

    hdaCodecToAudVolume(pThis, &pThis->paNodes[pThis->u8DacLineOut].dac.B_params,       PDMAUDIOMIXERCTL_PCM);
    hdaCodecToAudVolume(pThis, &pThis->paNodes[pThis->u8AdcVolsLineIn].adcvol.B_params, PDMAUDIOMIXERCTL_LINE_IN);

    return VINF_SUCCESS;
}

