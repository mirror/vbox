/* $Id$ */
/** @file
 * HDAStreamMap.h - Stream map functions for HD Audio.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_DevHdaStreamMap_h
#define VBOX_INCLUDED_SRC_Audio_DevHdaStreamMap_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/**
 * Audio stream data mapping.
 */
typedef struct HDASTREAMMAP
{
    /** The guest stream properties that is being mapped from/to.
     * The host properties are found in HDASTREAMSTATE::Cfg::Props.  */
    PDMAUDIOPCMPROPS                GuestProps;
    /** The stream's layout. */
    PDMAUDIOSTREAMLAYOUT            enmLayout;
    /** The guest side frame size in bytes. */
    uint8_t                         cbGuestFrame;
    /** Set if mapping is needed. */
    bool                            fMappingNeeded;
    /** Number of mappings in paMappings. */
    uint8_t                         cMappings;
    uint8_t                         aPadding[1];
    /** Array of stream mappings.
     *  Note: The mappings *must* be layed out in an increasing order, e.g.
     *        how the data appears in the given data block. */
    R3PTRTYPE(PPDMAUDIOSTREAMMAP)   paMappings;
#if HC_ARCH_BITS == 32
    RTR3PTR                         Padding1;
#endif
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** Circular buffer holding for holding audio data for this mapping. */
    R3PTRTYPE(PRTCIRCBUF)           pCircBuf;
#endif
    /**
     * Converts guest data to host data.
     *
     * @param   pvDst       The destination (host) buffer.
     * @param   pvSrc       The source (guest) data.  Does not overlap @a pvDst.
     * @param   cFrames     Number of frames to convert.
     * @param   pMapping    Pointer to this structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnGuestToHost,(void *pvDst, void const *pvSrc, uint32_t cFrames,
                                               struct HDASTREAMMAP const *pMapping));
    /**
     * Converts host data to guest data.
     *
     * @param   pvDst       The destination (guest) buffer.
     * @param   pvSrc       The source (host) data.  Does not overlap @a pvDst.
     * @param   cFrames     Number of frames to convert.
     * @param   pMapping    Pointer to this structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnHostToGuest,(void *pvDst, void const *pvSrc, uint32_t cFrames,
                                               struct HDASTREAMMAP const *pMapping));
} HDASTREAMMAP;
AssertCompileSizeAlignment(HDASTREAMMAP, 8);
/** Pointer to an audio stream data mapping. */
typedef HDASTREAMMAP *PHDASTREAMMAP;

/** @name Stream mapping functions.
 * @{
 */
#ifdef IN_RING3
int  hdaR3StreamMapInit(PHDASTREAMMAP pMapping, uint8_t cHostChannels, PPDMAUDIOPCMPROPS pProps);
void hdaR3StreamMapDestroy(PHDASTREAMMAP pMapping);
void hdaR3StreamMapReset(PHDASTREAMMAP pMapping);
#endif
/** @} */

#endif /* !VBOX_INCLUDED_SRC_Audio_DevHdaStreamMap_h */

