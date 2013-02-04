/* $Id$ */
/** @file
 * DevCodec - VBox ICH Intel HD Audio Codec.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef DEV_CODEC_H
#define DEV_CODEC_H
struct CODECState;
struct INTELHDLinkState;

typedef DECLCALLBACK(int) FNCODECVERBPROCESSOR(struct CODECState *pState, uint32_t cmd, uint64_t *pResp);
typedef FNCODECVERBPROCESSOR *PFNCODECVERBPROCESSOR;
typedef FNCODECVERBPROCESSOR **PPFNCODECVERBPROCESSOR;

/* RPM 5.3.1 */
#define CODEC_RESPONSE_UNSOLICITED RT_BIT_64(34)

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
#define CODEC_VERB_PAYLOAD4(cmd) ((cmd) & CODEC_VERB_4BIT_DATA)
#define CODEC_VERB_PAYLOAD8(cmd) ((cmd) & CODEC_VERB_8BIT_DATA)
#define CODEC_VERB_PAYLOAD16(cmd) ((cmd) & CODEC_VERB_16BIT_DATA)

#define CODEC_VERB_GET_AMP_DIRECTION  RT_BIT(15)
#define CODEC_VERB_GET_AMP_SIDE       RT_BIT(13)
#define CODEC_VERB_GET_AMP_INDEX      0x7

/* HDA spec 7.3.3.7 NoteA */
#define CODEC_GET_AMP_DIRECTION(cmd)  (((cmd) & CODEC_VERB_GET_AMP_DIRECTION) >> 15)
#define CODEC_GET_AMP_SIDE(cmd)       (((cmd) & CODEC_VERB_GET_AMP_SIDE) >> 13)
#define CODEC_GET_AMP_INDEX(cmd)      (CODEC_GET_AMP_DIRECTION(cmd) ? 0 : ((cmd) & CODEC_VERB_GET_AMP_INDEX))

/* HDA spec 7.3.3.7 NoteC */
#define CODEC_VERB_SET_AMP_OUT_DIRECTION  RT_BIT(15)
#define CODEC_VERB_SET_AMP_IN_DIRECTION   RT_BIT(14)
#define CODEC_VERB_SET_AMP_LEFT_SIDE      RT_BIT(13)
#define CODEC_VERB_SET_AMP_RIGHT_SIDE     RT_BIT(12)
#define CODEC_VERB_SET_AMP_INDEX          (0x7 << 8)

#define CODEC_SET_AMP_IS_OUT_DIRECTION(cmd)  (((cmd) & CODEC_VERB_SET_AMP_OUT_DIRECTION) != 0)
#define CODEC_SET_AMP_IS_IN_DIRECTION(cmd)   (((cmd) & CODEC_VERB_SET_AMP_IN_DIRECTION) != 0)
#define CODEC_SET_AMP_IS_LEFT_SIDE(cmd)      (((cmd) & CODEC_VERB_SET_AMP_LEFT_SIDE) != 0)
#define CODEC_SET_AMP_IS_RIGHT_SIDE(cmd)     (((cmd) & CODEC_VERB_SET_AMP_RIGHT_SIDE) != 0)
#define CODEC_SET_AMP_INDEX(cmd)             (((cmd) & CODEC_VERB_SET_AMP_INDEX) >> 7)

/* HDA spec 7.3.3.1 defines layout of configuration registers/verbs (0xF00) */
/* VendorID (7.3.4.1) */
#define CODEC_MAKE_F00_00(vendorID, deviceID) (((vendorID) << 16) | (deviceID))
#define CODEC_F00_00_VENDORID(f00_00) (((f00_00) >> 16) & 0xFFFF)
#define CODEC_F00_00_DEVICEID(f00_00) ((f00_00) & 0xFFFF)
/* RevisionID (7.3.4.2)*/
#define CODEC_MAKE_F00_02(MajRev, MinRev, RevisionID, SteppingID) (((MajRev) << 20)|((MinRev) << 16)|((RevisionID) << 8)|(SteppingID))
/* Subordinate node count (7.3.4.3)*/
#define CODEC_MAKE_F00_04(startNodeNumber, totalNodeNumber) ((((startNodeNumber) & 0xFF) << 16)|((totalNodeNumber) & 0xFF))
#define CODEC_F00_04_TO_START_NODE_NUMBER(f00_04) (((f00_04) >> 16) & 0xFF)
#define CODEC_F00_04_TO_NODE_COUNT(f00_04) ((f00_04) & 0xFF)
/*
 * Function Group Type  (7.3.4.4)
 * 0 & [0x3-0x7f] are reserved types
 * [0x80 - 0xff] are vendor defined function groups
 */
#define CODEC_MAKE_F00_05(UnSol, NodeType) (((UnSol) << 8)|(NodeType))
#define CODEC_F00_05_UNSOL  RT_BIT(8)
#define CODEC_F00_05_AFG    (0x1)
#define CODEC_F00_05_MFG    (0x2)
#define CODEC_F00_05_IS_UNSOL(f00_05) RT_BOOL((f00_05) & RT_BIT(8))
#define CODEC_F00_05_GROUP(f00_05) ((f00_05) & 0xff)
/*  Audio Function Group capabilities (7.3.4.5) */
#define CODEC_MAKE_F00_08(BeepGen, InputDelay, OutputDelay) ((((BeepGen) & 0x1) << 16)| (((InputDelay) & 0xF) << 8) | ((OutputDelay) & 0xF))
#define CODEC_F00_08_BEEP_GEN(f00_08) ((f00_08) & RT_BIT(16)

/* Widget Capabilities (7.3.4.6) */
#define CODEC_MAKE_F00_09(type, delay, chanel_count) \
    ( (((type) & 0xF) << 20)            \
    | (((delay) & 0xF) << 16)           \
    | (((chanel_count) & 0xF) << 13))
/* note: types 0x8-0xe are reserved */
#define CODEC_F00_09_TYPE_AUDIO_OUTPUT      (0x0)
#define CODEC_F00_09_TYPE_AUDIO_INPUT       (0x1)
#define CODEC_F00_09_TYPE_AUDIO_MIXER       (0x2)
#define CODEC_F00_09_TYPE_AUDIO_SELECTOR    (0x3)
#define CODEC_F00_09_TYPE_PIN_COMPLEX       (0x4)
#define CODEC_F00_09_TYPE_POWER_WIDGET      (0x5)
#define CODEC_F00_09_TYPE_VOLUME_KNOB       (0x6)
#define CODEC_F00_09_TYPE_BEEP_GEN          (0x7)
#define CODEC_F00_09_TYPE_VENDOR_DEFINED    (0xF)

#define CODEC_F00_09_CAP_CP                 RT_BIT(12)
#define CODEC_F00_09_CAP_L_R_SWAP           RT_BIT(11)
#define CODEC_F00_09_CAP_POWER_CTRL         RT_BIT(10)
#define CODEC_F00_09_CAP_DIGITAL            RT_BIT(9)
#define CODEC_F00_09_CAP_CONNECTION_LIST    RT_BIT(8)
#define CODEC_F00_09_CAP_UNSOL              RT_BIT(7)
#define CODEC_F00_09_CAP_PROC_WIDGET        RT_BIT(6)
#define CODEC_F00_09_CAP_STRIPE             RT_BIT(5)
#define CODEC_F00_09_CAP_FMT_OVERRIDE       RT_BIT(4)
#define CODEC_F00_09_CAP_AMP_FMT_OVERRIDE   RT_BIT(3)
#define CODEC_F00_09_CAP_OUT_AMP_PRESENT    RT_BIT(2)
#define CODEC_F00_09_CAP_IN_AMP_PRESENT     RT_BIT(1)
#define CODEC_F00_09_CAP_LSB                RT_BIT(0)

#define CODEC_F00_09_TYPE(f00_09) (((f00_09) >> 20) & 0xF)

#define CODEC_F00_09_IS_CAP_CP(f00_09)              RT_BOOL((f00_09) & RT_BIT(12))
#define CODEC_F00_09_IS_CAP_L_R_SWAP(f00_09)        RT_BOOL((f00_09) & RT_BIT(11))
#define CODEC_F00_09_IS_CAP_POWER_CTRL(f00_09)      RT_BOOL((f00_09) & RT_BIT(10))
#define CODEC_F00_09_IS_CAP_DIGITAL(f00_09)         RT_BOOL((f00_09) & RT_BIT(9))
#define CODEC_F00_09_IS_CAP_CONNECTION_LIST(f00_09) RT_BOOL((f00_09) & RT_BIT(8))
#define CODEC_F00_09_IS_CAP_UNSOL(f00_09)           RT_BOOL((f00_09) & RT_BIT(7))
#define CODEC_F00_09_IS_CAP_PROC_WIDGET(f00_09)     RT_BOOL((f00_09) & RT_BIT(6))
#define CODEC_F00_09_IS_CAP_STRIPE(f00_09)          RT_BOOL((f00_09) & RT_BIT(5))
#define CODEC_F00_09_IS_CAP_FMT_OVERRIDE(f00_09)    RT_BOOL((f00_09) & RT_BIT(4))
#define CODEC_F00_09_IS_CAP_AMP_OVERRIDE(f00_09)    RT_BOOL((f00_09) & RT_BIT(3))
#define CODEC_F00_09_IS_CAP_OUT_AMP_PRESENT(f00_09) RT_BOOL((f00_09) & RT_BIT(2))
#define CODEC_F00_09_IS_CAP_IN_AMP_PRESENT(f00_09)  RT_BOOL((f00_09) & RT_BIT(1))
#define CODEC_F00_09_IS_CAP_LSB(f00_09)             RT_BOOL((f00_09) & RT_BIT(0))

/* Supported PCM size, rates (7.3.4.7) */
#define CODEC_F00_0A_32_BIT             RT_BIT(19)
#define CODEC_F00_0A_24_BIT             RT_BIT(18)
#define CODEC_F00_0A_16_BIT             RT_BIT(17)
#define CODEC_F00_0A_8_BIT              RT_BIT(16)

#define CODEC_F00_0A_48KHZ_MULT_8X      RT_BIT(11)
#define CODEC_F00_0A_48KHZ_MULT_4X      RT_BIT(10)
#define CODEC_F00_0A_44_1KHZ_MULT_4X    RT_BIT(9)
#define CODEC_F00_0A_48KHZ_MULT_2X      RT_BIT(8)
#define CODEC_F00_0A_44_1KHZ_MULT_2X    RT_BIT(7)
#define CODEC_F00_0A_48KHZ              RT_BIT(6)
#define CODEC_F00_0A_44_1KHZ            RT_BIT(5)
/* 2/3 * 48kHz */
#define CODEC_F00_0A_48KHZ_2_3X         RT_BIT(4)
/* 1/2 * 44.1kHz */
#define CODEC_F00_0A_44_1KHZ_1_2X       RT_BIT(3)
/* 1/3 * 48kHz */
#define CODEC_F00_0A_48KHZ_1_3X         RT_BIT(2)
/* 1/4 * 44.1kHz */
#define CODEC_F00_0A_44_1KHZ_1_4X       RT_BIT(1)
/* 1/6 * 48kHz */
#define CODEC_F00_0A_48KHZ_1_6X         RT_BIT(0)

/* Supported streams formats (7.3.4.8) */
#define CODEC_F00_0B_AC3                RT_BIT(2)
#define CODEC_F00_0B_FLOAT32            RT_BIT(1)
#define CODEC_F00_0B_PCM                RT_BIT(0)

/* Pin Capabilities (7.3.4.9)*/
#define CODEC_MAKE_F00_0C(vref_ctrl) (((vref_ctrl) & 0xFF) << 8)
#define CODEC_F00_0C_CAP_HBR                    RT_BIT(27)
#define CODEC_F00_0C_CAP_DP                     RT_BIT(24)
#define CODEC_F00_0C_CAP_EAPD                   RT_BIT(16)
#define CODEC_F00_0C_CAP_HDMI                   RT_BIT(7)
#define CODEC_F00_0C_CAP_BALANCED_IO            RT_BIT(6)
#define CODEC_F00_0C_CAP_INPUT                  RT_BIT(5)
#define CODEC_F00_0C_CAP_OUTPUT                 RT_BIT(4)
#define CODEC_F00_0C_CAP_HP                     RT_BIT(3)
#define CODEC_F00_0C_CAP_PRESENSE_DETECT        RT_BIT(2)
#define CODEC_F00_0C_CAP_TRIGGER_REQUIRED       RT_BIT(1)
#define CODEC_F00_0C_CAP_IMPENDANCE_SENSE       RT_BIT(0)

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

/* Output Amplifier capabilities (7.3.4.10) */
#define CODEC_MAKE_F00_12 CODEC_MAKE_F00_0D

/* Connection list lenght (7.3.4.11) */
#define CODEC_MAKE_F00_0E(long_form, length)    \
    (  (((long_form) & 0x1) << 7)               \
     | ((length) & 0x7F))
#define CODEC_F00_0E_IS_LONG(f00_0e) RT_BOOL((f00_0e) & RT_BIT(7))
#define CODEC_F00_0E_COUNT(f00_0e) ((f00_0e) & 0x7F)
/* Supported Power States (7.3.4.12) */
#define CODEC_F00_0F_EPSS       RT_BIT(31)
#define CODEC_F00_0F_CLKSTOP    RT_BIT(30)
#define CODEC_F00_0F_S3D3       RT_BIT(29)
#define CODEC_F00_0F_D3COLD     RT_BIT(4)
#define CODEC_F00_0F_D3         RT_BIT(3)
#define CODEC_F00_0F_D2         RT_BIT(2)
#define CODEC_F00_0F_D1         RT_BIT(1)
#define CODEC_F00_0F_D0         RT_BIT(0)

/* Processing capabilities 7.3.4.13 */
#define CODEC_MAKE_F00_10(num, benign) ((((num) & 0xFF) << 8) | ((benign) & 0x1))
#define CODEC_F00_10_NUM(f00_10) (((f00_10) & (0xFF << 8)) >> 8)
#define CODEC_F00_10_BENING(f00_10) ((f00_10) & 0x1)

/* CP/IO Count (7.3.4.14) */
#define CODEC_MAKE_F00_11(wake, unsol, numgpi, numgpo, numgpio) \
    (  (((wake) & 0x1) << 31)                                   \
     | (((unsol) & 0x1) << 30)                                  \
     | (((numgpi) & 0xFF) << 16)                                \
     | (((numgpo) & 0xFF) << 8)                                 \
     | ((numgpio) & 0xFF))

/* Processing States (7.3.3.4) */
#define CODEC_F03_OFF    (0)
#define CODEC_F03_ON     RT_BIT(0)
#define CODEC_F03_BENING RT_BIT(1)
/* Power States (7.3.3.10) */
#define CODEC_MAKE_F05(reset, stopok, error, act, set)          \
    (   (((reset) & 0x1) << 10)                                 \
      | (((stopok) & 0x1) << 9)                                 \
      | (((error) & 0x1) << 8)                                  \
      | (((act) & 0x7) << 4)                                    \
      | ((set) & 0x7))
#define CODEC_F05_D3COLD    (4)
#define CODEC_F05_D3        (3)
#define CODEC_F05_D2        (2)
#define CODEC_F05_D1        (1)
#define CODEC_F05_D0        (0)

#define CODEC_F05_IS_RESET(value)   (((value) & RT_BIT(10)) != 0)
#define CODEC_F05_IS_STOPOK(value)  (((value) & RT_BIT(9)) != 0)
#define CODEC_F05_IS_ERROR(value)   (((value) & RT_BIT(8)) != 0)
#define CODEC_F05_ACT(value)        (((value) & 0x7) >> 4)
#define CODEC_F05_SET(value)        (((value) & 0x7))

#define CODEC_F05_GE(p0, p1) ((p0) <= (p1))
#define CODEC_F05_LE(p0, p1) ((p0) >= (p1))

/* Pin Widged Control (7.3.3.13) */
#define CODEC_F07_VREF_HIZ      (0)
#define CODEC_F07_VREF_50       (0x1)
#define CODEC_F07_VREF_GROUND   (0x2)
#define CODEC_F07_VREF_80       (0x4)
#define CODEC_F07_VREF_100      (0x5)
#define CODEC_F07_IN_ENABLE     RT_BIT(5)
#define CODEC_F07_OUT_ENABLE    RT_BIT(6)
#define CODEC_F07_OUT_H_ENABLE  RT_BIT(7)

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

#define CODEC_A_MULT_1X     (0)
#define CODEC_A_MULT_2X     (1)
#define CODEC_A_MULT_3X     (2)
#define CODEC_A_MULT_4X     (3)

#define CODEC_A_DIV_1X      (0)
#define CODEC_A_DIV_2X      (1)
#define CODEC_A_DIV_3X      (2)
#define CODEC_A_DIV_4X      (3)
#define CODEC_A_DIV_5X      (4)
#define CODEC_A_DIV_6X      (5)
#define CODEC_A_DIV_7X      (6)
#define CODEC_A_DIV_8X      (7)

#define CODEC_A_8_BIT       (0)
#define CODEC_A_16_BIT      (1)
#define CODEC_A_20_BIT      (2)
#define CODEC_A_24_BIT      (3)
#define CODEC_A_32_BIT      (4)

/* Pin Sense (7.3.3.15) */
#define CODEC_MAKE_F09_ANALOG(fPresent, impedance)  \
(  (((fPresent) & 0x1) << 31)                       \
 | (((impedance) & 0x7FFFFFFF)))
#define CODEC_F09_ANALOG_NA    0x7FFFFFFF
#define CODEC_MAKE_F09_DIGITAL(fPresent, fELDValid) \
(   (((fPresent) & 0x1) << 31)                      \
  | (((fELDValid) & 0x1) << 30))

#define CODEC_MAKE_F0C(lrswap, eapd, btl) ((((lrswap) & 1) << 2) | (((eapd) & 1) << 1) | ((btl) & 1))
#define CODEC_FOC_IS_LRSWAP(f0c)    RT_BOOL((f0c) & RT_BIT(2))
#define CODEC_FOC_IS_EAPD(f0c)      RT_BOOL((f0c) & RT_BIT(1))
#define CODEC_FOC_IS_BTL(f0c)       RT_BOOL((f0c) & RT_BIT(0))
/* HDA spec 7.3.3.31 defines layout of configuration registers/verbs (0xF1C) */
/* Configuration's port connection */
#define CODEC_F1C_PORT_MASK    (0x3)
#define CODEC_F1C_PORT_SHIFT   (30)

#define CODEC_F1C_PORT_COMPLEX (0x0)
#define CODEC_F1C_PORT_NO_PHYS (0x1)
#define CODEC_F1C_PORT_FIXED   (0x2)
#define CODEC_F1C_BOTH         (0x3)

/* Configuration's location */
#define CODEC_F1C_LOCATION_MASK  (0x3F)
#define CODEC_F1C_LOCATION_SHIFT (24)
/* [4:5] bits of location region means chassis attachment */
#define CODEC_F1C_LOCATION_PRIMARY_CHASSIS     (0)
#define CODEC_F1C_LOCATION_INTERNAL            RT_BIT(4)
#define CODEC_F1C_LOCATION_SECONDRARY_CHASSIS  RT_BIT(5)
#define CODEC_F1C_LOCATION_OTHER               (RT_BIT(5))

/* [0:3] bits of location region means geometry location attachment */
#define CODEC_F1C_LOCATION_NA                  (0)
#define CODEC_F1C_LOCATION_REAR                (0x1)
#define CODEC_F1C_LOCATION_FRONT               (0x2)
#define CODEC_F1C_LOCATION_LEFT                (0x3)
#define CODEC_F1C_LOCATION_RIGTH               (0x4)
#define CODEC_F1C_LOCATION_TOP                 (0x5)
#define CODEC_F1C_LOCATION_BOTTOM              (0x6)
#define CODEC_F1C_LOCATION_SPECIAL_0           (0x7)
#define CODEC_F1C_LOCATION_SPECIAL_1           (0x8)
#define CODEC_F1C_LOCATION_SPECIAL_2           (0x9)

/* Configuration's devices */
#define CODEC_F1C_DEVICE_MASK                  (0xF)
#define CODEC_F1C_DEVICE_SHIFT                 (20)
#define CODEC_F1C_DEVICE_LINE_OUT              (0)
#define CODEC_F1C_DEVICE_SPEAKER               (0x1)
#define CODEC_F1C_DEVICE_HP                    (0x2)
#define CODEC_F1C_DEVICE_CD                    (0x3)
#define CODEC_F1C_DEVICE_SPDIF_OUT             (0x4)
#define CODEC_F1C_DEVICE_DIGITAL_OTHER_OUT     (0x5)
#define CODEC_F1C_DEVICE_MODEM_LINE_SIDE       (0x6)
#define CODEC_F1C_DEVICE_MODEM_HANDSET_SIDE    (0x7)
#define CODEC_F1C_DEVICE_LINE_IN               (0x8)
#define CODEC_F1C_DEVICE_AUX                   (0x9)
#define CODEC_F1C_DEVICE_MIC                   (0xA)
#define CODEC_F1C_DEVICE_PHONE                 (0xB)
#define CODEC_F1C_DEVICE_SPDIF_IN              (0xC)
#define CODEC_F1C_DEVICE_RESERVED              (0xE)
#define CODEC_F1C_DEVICE_OTHER                 (0xF)

/* Configuration's Connection type */
#define CODEC_F1C_CONNECTION_TYPE_MASK         (0xF)
#define CODEC_F1C_CONNECTION_TYPE_SHIFT        (16)

#define CODEC_F1C_CONNECTION_TYPE_UNKNOWN               (0)
#define CODEC_F1C_CONNECTION_TYPE_1_8INCHES             (0x1)
#define CODEC_F1C_CONNECTION_TYPE_1_4INCHES             (0x2)
#define CODEC_F1C_CONNECTION_TYPE_ATAPI                 (0x3)
#define CODEC_F1C_CONNECTION_TYPE_RCA                   (0x4)
#define CODEC_F1C_CONNECTION_TYPE_OPTICAL               (0x5)
#define CODEC_F1C_CONNECTION_TYPE_OTHER_DIGITAL         (0x6)
#define CODEC_F1C_CONNECTION_TYPE_ANALOG                (0x7)
#define CODEC_F1C_CONNECTION_TYPE_DIN                   (0x8)
#define CODEC_F1C_CONNECTION_TYPE_XLR                   (0x9)
#define CODEC_F1C_CONNECTION_TYPE_RJ_11                 (0xA)
#define CODEC_F1C_CONNECTION_TYPE_COMBO                 (0xB)
#define CODEC_F1C_CONNECTION_TYPE_OTHER                 (0xF)

/* Configuration's color */
#define CODEC_F1C_COLOR_MASK                  (0xF)
#define CODEC_F1C_COLOR_SHIFT                 (12)
#define CODEC_F1C_COLOR_UNKNOWN               (0)
#define CODEC_F1C_COLOR_BLACK                 (0x1)
#define CODEC_F1C_COLOR_GREY                  (0x2)
#define CODEC_F1C_COLOR_BLUE                  (0x3)
#define CODEC_F1C_COLOR_GREEN                 (0x4)
#define CODEC_F1C_COLOR_RED                   (0x5)
#define CODEC_F1C_COLOR_ORANGE                (0x6)
#define CODEC_F1C_COLOR_YELLOW                (0x7)
#define CODEC_F1C_COLOR_PURPLE                (0x8)
#define CODEC_F1C_COLOR_PINK                  (0x9)
#define CODEC_F1C_COLOR_RESERVED_0            (0xA)
#define CODEC_F1C_COLOR_RESERVED_1            (0xB)
#define CODEC_F1C_COLOR_RESERVED_2            (0xC)
#define CODEC_F1C_COLOR_RESERVED_3            (0xD)
#define CODEC_F1C_COLOR_WHITE                 (0xE)
#define CODEC_F1C_COLOR_OTHER                 (0xF)

/* Configuration's misc */
#define CODEC_F1C_MISC_MASK                  (0xF)
#define CODEC_F1C_MISC_SHIFT                 (8)
#define CODEC_F1C_MISC_JACK_DETECT           (0)
#define CODEC_F1C_MISC_RESERVED_0            (1)
#define CODEC_F1C_MISC_RESERVED_1            (2)
#define CODEC_F1C_MISC_RESERVED_2            (3)

/* Configuration's association */
#define CODEC_F1C_ASSOCIATION_MASK                  (0xF)
#define CODEC_F1C_ASSOCIATION_SHIFT                 (4)
/* Connection's sequence */
#define CODEC_F1C_SEQ_MASK                  (0xF)
#define CODEC_F1C_SEQ_SHIFT                 (0)

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


#ifndef VBOX_WITH_HDA_CODEC_EMU
typedef struct CODECVERB
{
    uint32_t verb;
    /* operation bitness mask */
    uint32_t mask;
    PFNCODECVERBPROCESSOR pfn;
} CODECVERB;
#endif

#ifndef VBOX_WITH_HDA_CODEC_EMU
# define TYPE union
#else
# define TYPE struct
typedef struct CODECEMU CODECEMU;
typedef CODECEMU *PCODECEMU;
#endif
TYPE CODECNODE;
typedef TYPE CODECNODE CODECNODE;
typedef TYPE CODECNODE *PCODECNODE;


typedef enum
{
    PI_INDEX = 0,    /* PCM in */
    PO_INDEX,        /* PCM out */
    MC_INDEX,        /* Mic in */
    LAST_INDEX
} ENMSOUNDSOURCE;


typedef struct CODECState
{
    uint16_t                id;
    uint16_t                u16VendorId;
    uint16_t                u16DeviceId;
    uint8_t                 u8BSKU;
    uint8_t                 u8AssemblyId;
#ifndef VBOX_WITH_HDA_CODEC_EMU
    CODECVERB               *pVerbs;
    int                     cVerbs;
#else
    PCODECEMU               pCodecBackend;
#endif
    PCODECNODE               pNodes;
    QEMUSoundCard           card;
    /** PCM in */
    SWVoiceIn               *SwVoiceIn;
    /** PCM out */
    SWVoiceOut              *SwVoiceOut;
    void                    *pHDAState;
    bool                    fInReset;
#ifndef VBOX_WITH_HDA_CODEC_EMU
    const uint8_t           cTotalNodes;
    const uint8_t           *au8Ports;
    const uint8_t           *au8Dacs;
    const uint8_t           *au8AdcVols;
    const uint8_t           *au8Adcs;
    const uint8_t           *au8AdcMuxs;
    const uint8_t           *au8Pcbeeps;
    const uint8_t           *au8SpdifIns;
    const uint8_t           *au8SpdifOuts;
    const uint8_t           *au8DigInPins;
    const uint8_t           *au8DigOutPins;
    const uint8_t           *au8Cds;
    const uint8_t           *au8VolKnobs;
    const uint8_t           *au8Reserveds;
    const uint8_t           u8AdcVolsLineIn;
    const uint8_t           u8DacLineOut;
#endif
    DECLR3CALLBACKMEMBER(int, pfnProcess, (struct CODECState *));
    DECLR3CALLBACKMEMBER(void, pfnTransfer, (struct CODECState *pState, ENMSOUNDSOURCE, int avail));
    /* These callbacks are set by Codec implementation */
    DECLR3CALLBACKMEMBER(int, pfnLookup, (struct CODECState *pState, uint32_t verb, PPFNCODECVERBPROCESSOR));
    DECLR3CALLBACKMEMBER(int, pfnReset, (struct CODECState *pState));
    DECLR3CALLBACKMEMBER(int, pfnCodecNodeReset, (struct CODECState *pState, uint8_t, PCODECNODE));
    /* These callbacks are set by codec implementation to answer debugger requests */
    DECLR3CALLBACKMEMBER(void, pfnCodecDbgListNodes, (CODECState *pState, PCDBGFINFOHLP pHlp, const char *pszArgs));
    DECLR3CALLBACKMEMBER(void, pfnCodecDbgSelector, (CODECState *pState, PCDBGFINFOHLP pHlp, const char *pszArgs));
} CODECState, *PCODECState;

int codecConstruct(PPDMDEVINS pDevIns, CODECState *pCodecState, PCFGMNODE pCfgHandle);
int codecDestruct(CODECState *pCodecState);
int codecSaveState(CODECState *pCodecState, PSSMHANDLE pSSMHandle);
int codecLoadState(CODECState *pCodecState, PSSMHANDLE pSSMHandle, uint32_t uVersion);
int codecOpenVoice(CODECState *pCodecState, ENMSOUNDSOURCE enmSoundSource, audsettings_t *pAudioSettings);

#define HDA_SSM_VERSION   4
#define HDA_SSM_VERSION_1 1
#define HDA_SSM_VERSION_2 2
#define HDA_SSM_VERSION_3 3

# ifdef VBOX_WITH_HDA_CODEC_EMU
/* */
struct CODECEMU
{
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuConstruct, (PCODECState pState));
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuDestruct, (PCODECState pState));
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuReset, (PCODECState pState, bool fInit));
};
# endif
#endif
