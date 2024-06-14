/** @file
 * IPRT: Tracelog decoder plugin API for RTTraceLogTool.
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


/** Pointer to helper functions for decoders. */
typedef struct RTTRACELOGDECODERHLP *PRTTRACELOGDECODERHLP;


/**
 * Decoder state free callback.
 *
 * @param   pHlp        Pointer to the callback structure.
 * @param   pvState     Pointer to the decoder state.
 */
typedef DECLCALLBACKTYPE(void, FNTRACELOGDECODERSTATEFREE,(PRTTRACELOGDECODERHLP pHlp, void *pvState));
/** Pointer to an event decode callback. */
typedef FNTRACELOGDECODERSTATEFREE *PFNTRACELOGDECODERSTATEFREE;


/**
 * Helper functions for decoders.
 */
typedef struct RTTRACELOGDECODERHLP
{
    /** Magic value (RTTRACELOGDECODERHLP_MAGIC). */
    uint32_t                u32Magic;

    /**
     * Helper for writing formatted text to the output.
     *
     * @returns IPRT status.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszFormat   The format string.  This may use all IPRT extensions as
     *                      well as the debugger ones.
     * @param   ...         Arguments specified in the format string.
     */
    DECLCALLBACKMEMBER(int, pfnPrintf, (PRTTRACELOGDECODERHLP pHlp, const char *pszFormat, ...)) RT_IPRT_FORMAT_ATTR(2, 3);


    /**
     * Helper for writing formatted error message to the output.
     *
     * @returns IPRT status.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszFormat   The format string.  This may use all IPRT extensions as
     *                      well as the debugger ones.
     * @param   ...         Arguments specified in the format string.
     */
    DECLCALLBACKMEMBER(int, pfnErrorMsg, (PRTTRACELOGDECODERHLP pHlp, const char *pszFormat, ...)) RT_IPRT_FORMAT_ATTR(2, 3);


    /**
     * Creates a new decoder state and associates it with the given helper structure.
     *
     * @returns IPRT status.
     * @param   pHlp        Pointer to the callback structure.
     * @param   cbState     Size of the state in bytes.
     * @param   pfnFree     Callback which is called before the decoder state is freed to give the decoder
     *                      a chance to do some necessary cleanup, optional.
     * @param   ppvState    Where to return the pointer to the state on success.
     *
     * @note This will destroy and free any previously created decoder state as there can be only one currently for
     *       a decoder.
     */
    DECLCALLBACKMEMBER(int, pfnDecoderStateCreate, (PRTTRACELOGDECODERHLP pHlp, size_t cbState, PFNTRACELOGDECODERSTATEFREE pfnFree,
                                                    void **ppvState));


    /**
     * Destroys any currently attached decoder state.
     *
     * @param   pHlp        Pointer to the callback structure.
     */
    DECLCALLBACKMEMBER(void, pfnDecoderStateDestroy, (PRTTRACELOGDECODERHLP pHlp));


    /**
     * Returns any decoder state created previously with RTTRACELOGDECODERHLP::pfnDecoderStateCreate().
     *
     * @returns Pointer to the decoder state or NULL if none was created yet.
     * @param   pHlp        Pointer to the callback structure.
     */
    DECLCALLBACKMEMBER(void*, pfnDecoderStateGet, (PRTTRACELOGDECODERHLP pHlp));


    /** End marker (DBGCCMDHLP_MAGIC). */
    uint32_t                u32EndMarker;
} RTTRACELOGDECODERHLP;

/** Magic value for RTTRACELOGDECODERHLP::u32Magic and RTTRACELOGDECODERHLP::u32EndMarker. (Bernhard-Viktor Christoph-Carl von Buelow) */
#define DBGCCMDHLP_MAGIC    UINT32_C(0x19231112)


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
 * @param   pHlp                The decoder helper callback table.
 * @param   idDecodeEvt         Event decoder ID given in RTTRACELOGDECODEEVT::idDecodeEvt for the particular event ID.
 * @param   hTraceLogEvt        The tracelog event handle called for decoding.
 * @param   pEvtDesc            The event descriptor.
 * @param   paVals              Pointer to the array of values.
 * @param   cVals               Number of values in the array.
 */
typedef DECLCALLBACKTYPE(int, FNTRACELOGDECODEREVENTDECODE,(PRTTRACELOGDECODERHLP pHlp, uint32_t idDecodeEvt,
                                                            RTTRACELOGRDREVT hTraceLogEvt, PCRTTRACELOGEVTDESC pEvtDesc,
                                                            PRTTRACELOGEVTVAL paVals, uint32_t cVals));
/** Pointer to an event decode callback. */
typedef FNTRACELOGDECODEREVENTDECODE *PFNTRACELOGDECODEREVENTDECODE;


/**
 * Event decoder entry.
 */
typedef struct RTTRACELOGDECODEEVT
{
    /** The event ID name. */
    const char *pszEvtId;
    /** The decoder event ID ordinal to pass to in the decode callback for
     * faster lookup. */
    uint32_t   idDecodeEvt;
} RTTRACELOGDECODEEVT;
/** Pointer to an event decoder entry. */
typedef RTTRACELOGDECODEEVT *PRTTRACELOGDECODEEVT;
/** Pointer to a const event decoder entry. */
typedef const RTTRACELOGDECODEEVT *PCRTTRACELOGDECODEEVT;


/**
 * A decoder registration structure.
 */
typedef struct RTTRACELOGDECODERREG
{
    /** Decoder name. */
    const char                    *pszName;
    /** Decoder description. */
    const char                    *pszDesc;
    /** The event IDs to register the decoder for. */
    PCRTTRACELOGDECODEEVT         paEvtIds;
    /** The decode callback. */
    PFNTRACELOGDECODEREVENTDECODE pfnDecode;
} RTTRACELOGDECODERREG;
/** Pointer to a decoder registration structure. */
typedef RTTRACELOGDECODERREG *PRTTRACELOGDECODERREG;
/** Pointer to a const decoder registration structure. */
typedef const RTTRACELOGDECODERREG *PCRTTRACELOGDECODERREG;


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
    DECLR3CALLBACKMEMBER(int, pfnRegisterDecoders, (void *pvUser, PCRTTRACELOGDECODERREG paDecoders, uint32_t cDecoders));

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
