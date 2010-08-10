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
#ifndef DEV_CODEC_H
#define DEV_CODEC_H
struct CODECState;
struct INTELHDLinkState;

typedef DECLCALLBACK(int) FNCODECVERBPROCESSOR(struct CODECState *pState, uint32_t cmd, uint64_t *pResp);
typedef FNCODECVERBPROCESSOR *PFNCODECVERBPROCESSOR;
typedef FNCODECVERBPROCESSOR **PPFNCODECVERBPROCESSOR;

/* RPM 5.3.1 */
#define CODEC_RESPONSE_UNSOLICITED RT_BIT_64(34)

typedef struct CODECVERB
{
    uint32_t verb;
    /* operation bitness mask */
    uint32_t mask;
    PFNCODECVERBPROCESSOR pfn;
} CODECVERB;

#define CODECNODE_F0_PARAM_LENGTH 0x14
typedef struct CODECCOMMONNODE
{
    uint8_t id; /* 7 - bit format */
    const char    *name;
    /* RPM 5.3.6 */
    uint32_t au32F00_param[CODECNODE_F0_PARAM_LENGTH];
    uint8_t    au8F02_param[16];
} CODECCOMMONNODE, *PCODECCOMMONNODE;

typedef struct ROOTCODECNODE
{
    CODECCOMMONNODE node;
}ROOTCODECNODE, *PROOTCODECNODE;

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

typedef struct ADCNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F03_param;
    uint32_t    u32F05_param;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} ADCNODE, *PADCNODE;

typedef struct SPDIFOUTNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;
    uint32_t    u32F0d_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} SPDIFOUTNODE, *PSPDIFOUTNODE;

typedef struct SPDIFINNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F06_param;
    uint32_t    u32F09_param;
    uint32_t    u32F0d_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} SPDIFINNODE, *PSPDIFINNODE;

typedef struct AFGCODECNODE
{
    CODECCOMMONNODE node;
    uint32_t  u32F05_param;
    uint32_t  u32F08_param;
    uint32_t  u32F20_param;
} AFGCODECNODE, *PAFGCODECNODE;

typedef struct PORTNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F07_param;
    uint32_t u32F08_param;
    uint32_t u32F09_param;
    uint32_t u32F1c_param;
} PORTNODE, *PPORTNODE;

typedef struct DIGOUTNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F01_param;
    uint32_t u32F07_param;
    uint32_t u32F09_param;
    uint32_t u32F1c_param;
} DIGOUTNODE, *PDIGOUTNODE;

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

typedef struct ADCMUXNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F01_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} ADCMUXNODE, *PADCMUXNODE;

typedef struct PCBEEPNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F0a_param;

    uint32_t    u32A_param;
    AMPLIFIER   B_params;
} PCBEEPNODE, *PPCBEEPNODE;

typedef struct CDNODE
{
    CODECCOMMONNODE node;
    uint32_t u32F07_param;
    uint32_t u32F1c_param;
} CDNODE, *PCDNODE;

typedef struct VOLUMEKNOBNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F08_param;
    uint32_t    u32F0f_param;
} VOLUMEKNOBNODE, *PVOLUMEKNOBNODE;

typedef struct ADCVOLNODE
{
    CODECCOMMONNODE node;
    uint32_t    u32F0c_param;
    uint32_t    u32A_params;
    AMPLIFIER   B_params;
} ADCVOLNODE, *PADCVOLNODE;

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
} CODECNODE, *PCODECNODE;

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
    CODECVERB               *pVerbs;
    int                     cVerbs;
    PCODECNODE               pNodes;
    QEMUSoundCard           card;
    /** PCM in */
    SWVoiceIn               *voice_pi;
    /** PCM out */
    SWVoiceOut              *voice_po;
    /** Mic in */
    SWVoiceIn               *voice_mc;
    void *pHDAState;
    bool fFirstResetDetected;
    DECLR3CALLBACKMEMBER(int, pfnProcess, (struct CODECState *));
    DECLR3CALLBACKMEMBER(int, pfnLookup, (struct CODECState *pState, uint32_t verb, PPFNCODECVERBPROCESSOR));
    DECLR3CALLBACKMEMBER(int, pfnReset, (struct CODECState *pState));
    DECLR3CALLBACKMEMBER(void, pfnTransfer, (struct CODECState *pState, ENMSOUNDSOURCE, int avail));
} CODECState;

int stac9220Construct(CODECState *pCodecState);
int stac9220Destruct(CODECState *pCodecState);

#endif
