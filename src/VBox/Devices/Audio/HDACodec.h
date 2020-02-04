/* $Id$ */
/** @file
 * HDACodec - VBox HD Audio Codec.
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

#ifndef VBOX_INCLUDED_SRC_Audio_HDACodec_h
#define VBOX_INCLUDED_SRC_Audio_HDACodec_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/list.h>

#include "AudioMixer.h"

/** Pointer to a shared HDA device state.  */
typedef struct HDASTATE *PHDASTATE;
/** Pointer to a ring-3 HDA device state.  */
typedef struct HDASTATER3 *PHDASTATER3;
/** The ICH HDA (Intel) codec state. */
typedef struct HDACODEC *PHDACODEC;
/** The HDA host driver backend. */
typedef struct HDADRIVER *PHDADRIVER;

/**
 * Verb processor method.
 */
typedef DECLCALLBACK(int) FNHDACODECVERBPROCESSOR(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp);
typedef FNHDACODECVERBPROCESSOR *PFNHDACODECVERBPROCESSOR;

/* PRM 5.3.1 */
#define CODEC_RESPONSE_UNSOLICITED RT_BIT_64(34)

typedef struct CODECVERB
{
    /** Verb. */
    uint32_t                 verb;
    /** Verb mask. */
    uint32_t                 mask;
    /** Function pointer for implementation callback. */
    PFNHDACODECVERBPROCESSOR pfn;
    /** Friendly name, for debugging. */
    const char              *pszName;
} CODECVERB;

union CODECNODE;
typedef union CODECNODE CODECNODE, *PCODECNODE;

/**
 * HDA codec state.
 */
typedef struct HDACODEC
{
    uint16_t                id;
    uint16_t                u16VendorId;
    uint16_t                u16DeviceId;
    uint8_t                 u8BSKU;
    uint8_t                 u8AssemblyId;

    /** List of assigned HDA drivers to this codec.
     * A driver only can be assigned to one codec at a time. */
    RTLISTANCHOR            lstDrv;

    CODECVERB const        *paVerbs;
    size_t                  cVerbs;

    PCODECNODE              paNodes;

    bool                    fInReset;
    uint8_t                 abPadding1[3];

    const uint8_t           cTotalNodes;
    const uint8_t           u8AdcVolsLineIn;
    const uint8_t           u8DacLineOut;
    uint8_t                 bPadding2;
    const uint8_t          *au8Ports;
    const uint8_t          *au8Dacs;
    const uint8_t          *au8AdcVols;
    const uint8_t          *au8Adcs;
    const uint8_t          *au8AdcMuxs;
    const uint8_t          *au8Pcbeeps;
    const uint8_t          *au8SpdifIns;
    const uint8_t          *au8SpdifOuts;
    const uint8_t          *au8DigInPins;
    const uint8_t          *au8DigOutPins;
    const uint8_t          *au8Cds;
    const uint8_t          *au8VolKnobs;
    const uint8_t          *au8Reserveds;

    /** @name Public codec functions.
     *  @{  */
    DECLR3CALLBACKMEMBER(int,  pfnLookup, (PHDACODEC pThis, uint32_t uVerb, uint64_t *puResp));
    DECLR3CALLBACKMEMBER(void, pfnReset, (PHDACODEC pThis));
    DECLR3CALLBACKMEMBER(int,  pfnNodeReset, (PHDACODEC pThis, uint8_t, PCODECNODE));
    DECLR3CALLBACKMEMBER(void, pfnDbgListNodes, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
    DECLR3CALLBACKMEMBER(void, pfnDbgSelector, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
    /** @} */

    /** The parent device instance. */
    PPDMDEVINS              pDevIns;

    /** @name Callbacks to the HDA controller, mostly used for multiplexing to the
     *        various host backends.
     * @{ */
    /**
     *
     * Adds a new audio stream to a specific mixer control.
     *
     * Depending on the mixer control the stream then gets assigned to one of the
     * internal mixer sinks, which in turn then handle the mixing of all connected
     * streams to that sink.
     *
     * @return  VBox status code.
     * @param   pDevIns             The device instance.
     * @param   enmMixerCtl         Mixer control to assign new stream to.
     * @param   pCfg                Stream configuration for the new stream.
     */
    DECLR3CALLBACKMEMBER(int,  pfnCbMixerAddStream, (PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOSTREAMCFG pCfg));
    /**
     * Removes a specified mixer control from the HDA's mixer.
     *
     * @return  VBox status code.
     * @param   pDevIns             The device instance.
     * @param   enmMixerCtl         Mixer control to remove.
     */
    DECLR3CALLBACKMEMBER(int,  pfnCbMixerRemoveStream, (PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl));
    /**
     * Controls an input / output converter widget, that is, which converter is
     * connected to which stream (and channel).
     *
     * @return  VBox status code.
     * @param   pDevIns             The device instance.
     * @param   enmMixerCtl         Mixer control to set SD stream number and channel for.
     * @param   uSD                 SD stream number (number + 1) to set. Set to 0 for unassign.
     * @param   uChannel            Channel to set. Only valid if a valid SD stream number is specified.
     */
    DECLR3CALLBACKMEMBER(int,  pfnCbMixerControl, (PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, uint8_t uSD, uint8_t uChannel));
    /**
     * Sets the volume of a specified mixer control.
     *
     * @return  IPRT status code.
     * @param   pDevIns             The device instance.
     * @param   enmMixerCtl         Mixer control to set volume for.
     * @param   pVol                Pointer to volume data to set.
     */
    DECLR3CALLBACKMEMBER(int,  pfnCbMixerSetVolume, (PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOVOLUME pVol));
    /** @} */

#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             StatLookups;
#endif
} HDACODEC;

int hdaCodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis, uint16_t uLUN, PCFGMNODE pCfg);
void hdaCodecDestruct(PHDACODEC pThis);
void hdaCodecPowerOff(PHDACODEC pThis);
int hdaCodecSaveState(PPDMDEVINS pDevIns, PHDACODEC pThis, PSSMHANDLE pSSM);
int hdaCodecLoadState(PPDMDEVINS pDevIns, PHDACODEC pThis, PSSMHANDLE pSSM, uint32_t uVersion);
int hdaCodecAddStream(PHDACODEC pThis, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOSTREAMCFG pCfg);
int hdaCodecRemoveStream(PHDACODEC pThis, PDMAUDIOMIXERCTL enmMixerCtl);

/** @name DevHDA saved state versions
 * @{ */
/** Added (Controller):              Current wall clock value (this independent from WALCLK register value).
  * Added (Controller):              Current IRQ level.
  * Added (Per stream):              Ring buffer. This is optional and can be skipped if (not) needed.
  * Added (Per stream):              Struct g_aSSMStreamStateFields7.
  * Added (Per stream):              Struct g_aSSMStreamPeriodFields7.
  * Added (Current BDLE per stream): Struct g_aSSMBDLEDescFields7.
  * Added (Current BDLE per stream): Struct g_aSSMBDLEStateFields7. */
#define HDA_SAVED_STATE_VERSION   7
/** Saves the current BDLE state.
 * @since 5.0.14 (r104839) */
#define HDA_SAVED_STATE_VERSION_6 6
/** Introduced dynamic number of streams + stream identifiers for serialization.
 *  Bug: Did not save the BDLE states correctly.
 *  Those will be skipped on load then.
 * @since 5.0.12 (r104520)  */
#define HDA_SAVED_STATE_VERSION_5 5
/** Since this version the number of MMIO registers can be flexible. */
#define HDA_SAVED_STATE_VERSION_4 4
#define HDA_SAVED_STATE_VERSION_3 3
#define HDA_SAVED_STATE_VERSION_2 2
#define HDA_SAVED_STATE_VERSION_1 1
/** @} */

#endif /* !VBOX_INCLUDED_SRC_Audio_HDACodec_h */

