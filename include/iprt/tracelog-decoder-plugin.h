/** @file
 * VD: Plugin support API.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_tracelog_decoder_plugin_h
#define IPRT_INCLUDED_tracelog_decoder_plugin_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/tracelog.h>
#include <iprt/types.h>


/** Makes a IPRT tracelog decoder structure version out of an unique magic value and major &
 * minor version numbers.
 *
 * @returns 32-bit structure version number.
 *
 * @param   uMagic      16-bit magic value.  This must be unique.
 * @param   uMajor      12-bit major version number.  Structures with different
 *                      major numbers are not compatible.
 * @param   uMinor      4-bit minor version number.  When only the minor version
 *                      differs, the structures will be 100% backwards
 *                      compatible.
 */
#define RT_TRACELOG_DECODER_VERSION_MAKE(uMagic, uMajor, uMinor) \
    ( ((uint32_t)(uMagic) << 16) | ((uint32_t)((uMajor) & 0xff) << 4) | ((uint32_t)((uMinor) & 0xf) << 0) )

/** Checks if @a uVerMagic1 is compatible with @a uVerMagic2.
 *
 * @returns true / false.
 * @param   uVerMagic1  Typically the runtime version of the struct.  This must
 *                      have the same magic and major version as @a uVerMagic2
 *                      and the minor version must be greater or equal to that
 *                      of @a uVerMagic2.
 * @param   uVerMagic2  Typically the version the code was compiled against.
 *
 * @remarks The parameters will be referenced more than once.
 */
#define RT_TRACELOG_DECODER_VERSION_ARE_COMPATIBLE(uVerMagic1, uVerMagic2) \
    (    (uVerMagic1) == (uVerMagic2) \
      || (   (uVerMagic1) >= (uVerMagic2) \
          && ((uVerMagic1) & UINT32_C(0xfffffff0)) == ((uVerMagic2) & UINT32_C(0xfffffff0)) ) \
    )


/**
 * Decoder callback for an event.
 *
 * @returns IPRT status code.
 * @param   hTraceLogEvt        The tracelog event handle called for decoding.
 * @param   pEvtDesc            The event descriptor.
 * @param   paVals              Pointer to the array of values.
 * @param   cVals               Number of values in the array.
 */
typedef DECLCALLBACKTYPE(int, FNTRACELOGDECODEREVENTDECODE,(RTTRACELOGRDREVT hTraceLogEvt, PCRTTRACELOGEVTDESC pEvtDesc,
                                                            PRTTRACELOGEVTVAL paVals, uint32_t cVals));
/** Pointer to an event decode callback. */
typedef FNTRACELOGDECODEREVENTDECODE *PFNTRACELOGDECODEREVENTDECODE;


typedef struct RTTRACELOGDECODERDECODEEVENT
{
    /** The event ID to register the decoder for. */
    const char                    *pszId;
    /** The decode callback. */
    PFNTRACELOGDECODEREVENTDECODE pfnDecode;
} RTTRACELOGDECODERDECODEEVENT;
typedef RTTRACELOGDECODERDECODEEVENT *PRTTRACELOGDECODERDECODEEVENT;
typedef const RTTRACELOGDECODERDECODEEVENT *PCRTTRACELOGDECODERDECODEEVENT;


/**
 * Decoder register callbacks structure.
 */
typedef struct RTTRACELOGDECODERREGISTER
{
    /** Interface version.
     * This is set to RT_TRACELOG_DECODERREG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a new decoders.
     *
     * @returns VBox status code.
     * @param   pvUser      Opaque user data given in the plugin load callback.
     * @param   paDecoders  Pointer to an array of decoders to register.
     * @param   cDecoders   Number of entries in the array.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegisterDecoders, (void *pvUser, PCRTTRACELOGDECODERDECODEEVENT paDecoders, uint32_t cDecoders));

} RTTRACELOGDECODERREGISTER;
/** Pointer to a backend register callbacks structure. */
typedef RTTRACELOGDECODERREGISTER *PRTTRACELOGDECODERREGISTER;

/** Current version of the RTTRACELOGDECODERREGISTER structure.  */
#define RT_TRACELOG_DECODERREG_CB_VERSION       RT_TRACELOG_DECODER_VERSION_MAKE(0xfeed, 1, 0)

/**
 * Initialization entry point called by the tracelog layer when
 * a plugin is loaded.
 *
 * @returns IPRT status code.
 * @param   pvUser             Opaque user data passed in the register callbacks.
 * @param   pRegisterCallbacks Pointer to the register callbacks structure.
 */
typedef DECLCALLBACKTYPE(int, FNTRACELOGDECODERPLUGINLOAD,(void *pvUser, PRTTRACELOGDECODERREGISTER pRegisterCallbacks));
typedef FNTRACELOGDECODERPLUGINLOAD *PFNTRACELOGDECODERPLUGINLOAD;
#define RT_TRACELOG_DECODER_PLUGIN_LOAD "RTTraceLogDecoderLoad"

#endif /* !IPRT_INCLUDED_tracelog_decoder_plugin_h */
