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


/** The integer value should be dumped as a hex number instead. */
#define RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_HEX             RT_BIT_32(0)
/** The hexdump should be dumped as an inline string (Rhxs). */
#define RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_HEX_DUMP_STR    RT_BIT_32(1)


/**
 * An enum entry for the struct builder.
 */
typedef struct RTTRACELOGDECODERSTRUCTBLDENUM
{
    /** The raw enum value. */
    uint64_t        u64EnumVal;
    /** String version of the enum value. */
    const char      *pszString;
    /** Some opaque user data not used by RTTRACELOGDECODERHLP::pfnStructBldAddEnum. */
    uintptr_t       uPtrUser;
} RTTRACELOGDECODERSTRUCTBLDENUM;
/** Pointer to a struct build enum entry. */
typedef RTTRACELOGDECODERSTRUCTBLDENUM *PRTTRACELOGDECODERSTRUCTBLDENUM;
/** Pointer to a const struct build enum entry. */
typedef const RTTRACELOGDECODERSTRUCTBLDENUM *PCRTTRACELOGDECODERSTRUCTBLDENUM;


/** Initializes a enum entry, extended version. */
#define RT_TRACELOG_DECODER_STRUCT_BLD_ENUM_INIT_EX(a_enmVal, a_uPtrUser) \
    { a_enmVal, #a_enmVal, a_uPtrUser }
/** Initializes a enum entry, extended version. */
#define RT_TRACELOG_DECODER_STRUCT_BLD_ENUM_INIT(a_enmVal) \
    { a_enmVal, #a_enmVal, 0 }
/** Terminates an enum value to descriptor table. */
#define RT_TRACELOG_DECODER_STRUCT_BLD_ENUM_TERM \
    { 0, NULL, 0 }


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


    /** @name Decoder state related callbacks.
     * @{ */

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

    /** @} */

    /** @name Structure builder callbacks.
     * @{ */

    /**
     * Begins a new (sub-)structure with the given name.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the structure.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldBegin,      (PRTTRACELOGDECODERHLP pHlp, const char *pszName));


    /**
     * Ends the current (sub-)structure and returns to the parent.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldEnd,        (PRTTRACELOGDECODERHLP pHlp));


    /**
     * Adds a boolean member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   f           The boolean value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddBool,    (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, bool f));


    /**
     * Adds an unsigned byte (uint8_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   u8          The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddU8,      (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, uint8_t u8));


    /**
     * Adds an unsigned 16-bit (uint16_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   u16         The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddU16,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, uint16_t u16));


    /**
     * Adds an unsigned 32-bit (uint32_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   u32         The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddU32,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, uint32_t u32));


    /**
     * Adds an unsigned 64-bit (uint64_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   u64         The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddU64,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, uint64_t u64));


    /**
     * Adds a signed byte (int8_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   i8          The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddS8,      (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, int8_t i8));


    /**
     * Adds a signed 16-bit (int16_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   i16          The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddS16,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, int16_t i16));


    /**
     * Adds a signed 32-bit (int32_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   i32          The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddS32,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, int32_t i32));


    /**
     * Adds a signed 64-bit (int64_t) member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   i32          The value to record.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddS64,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, int64_t i64));


    /**
     * Adds a string member to the current (sub-)structure with the given name and string.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   pszStr      The string to add.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddStr,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, const char *pszStr));


    /**
     * Adds a data buffer member to the current (sub-)structure with the given name and content.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   pb          The buffer to add.
     * @param   cb          Size of the buffer in bytes.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddBuf,     (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, const uint8_t *pb, size_t cb));


    /**
     * Adds a enum member to the current (sub-)structure with the given name and value.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the member.
     * @param   fFlags      Combination of RTTRACELOG_DECODER_HLP_STRUCT_BLD_F_XXX.
     * @param   cBits       Size of the enum data type in bits.
     * @param   paEnums     Pointer to an erray of enum entries mapping a value to the description.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldAddEnum,    (PRTTRACELOGDECODERHLP pHlp, const char *pszName, uint32_t fFlags, uint8_t cBits,
                                                     PCRTTRACELOGDECODERSTRUCTBLDENUM paEnums, uint64_t u64Val));

    /**
     * Begins a new array member in the current (sub-)structure with the given name.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     * @param   pszName     Name of the array member.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldArrayBegin, (PRTTRACELOGDECODERHLP pHlp, const char *pszName));


    /**
     * Ends the current array member in the current (sub-)structure.
     *
     * @returns IPRT status code.
     * @param   pHlp        Pointer to the callback structure.
     */
    DECLCALLBACKMEMBER(int, pfnStructBldArrayEnd,   (PRTTRACELOGDECODERHLP pHlp));

    /** @} */

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
