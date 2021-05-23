/** @file
 * PDM - Pluggable Device Manager, Audio interfaces.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/** @page pg_pdm_audio  PDM Audio
 *
 * @section sec_pdm_audio_overview  Audio architecture overview
 *
 * The audio architecture mainly consists of two PDM interfaces,
 * PDMIAUDIOCONNECTOR and PDMIHOSTAUDIO.
 *
 * The PDMIAUDIOCONNECTOR interface is responsible of connecting a device
 * emulation, such as SB16, AC'97 and HDA to one or multiple audio backend(s).
 * Its API abstracts audio stream handling and I/O functions, device enumeration
 * and so on.
 *
 * The PDMIHOSTAUDIO interface must be implemented by all audio backends to
 * provide an abstract and common way of accessing needed functions, such as
 * transferring output audio data for playing audio or recording input from the
 * host.
 *
 * A device emulation can have one or more LUNs attached to it, whereas these
 * LUNs in turn then all have their own PDMIAUDIOCONNECTOR, making it possible
 * to connect multiple backends to a certain device emulation stream
 * (multiplexing).
 *
 * An audio backend's job is to record and/or play audio data (depending on its
 * capabilities). It highly depends on the host it's running on and needs very
 * specific (host-OS-dependent) code. The backend itself only has very limited
 * ways of accessing and/or communicating with the PDMIAUDIOCONNECTOR interface
 * via callbacks, but never directly with the device emulation or other parts of
 * the audio sub system.
 *
 *
 * @section sec_pdm_audio_mixing    Mixing
 *
 * The AUDIOMIXER API is optionally available to create and manage virtual audio
 * mixers. Such an audio mixer in turn then can be used by the device emulation
 * code to manage all the multiplexing to/from the connected LUN audio streams.
 *
 * Currently only input and output stream are supported. Duplex stream are not
 * supported yet.
 *
 * This also is handy if certain LUN audio streams should be added or removed
 * during runtime.
 *
 * To create a group of either input or output streams the AUDMIXSINK API can be
 * used.
 *
 * For example: The device emulation has one hardware output stream (HW0), and
 * that output stream shall be available to all connected LUN backends. For that
 * to happen, an AUDMIXSINK sink has to be created and attached to the device's
 * AUDIOMIXER object.
 *
 * As every LUN has its own AUDMIXSTREAM object, adding all those
 * objects to the just created audio mixer sink will do the job.
 *
 * @note The AUDIOMIXER API is purely optional and is not used by all currently
 *       implemented device emulations (e.g. SB16).
 *
 *
 * @section sec_pdm_audio_data_processing   Data processing
 *
 * Audio input / output data gets handed off to/from the device emulation in an
 * unmodified (raw) way. The actual audio frame / sample conversion is done via
 * the AUDIOMIXBUF API.
 *
 * This concentrates the audio data processing in one place and makes it easier
 * to test / benchmark such code.
 *
 * A PDMAUDIOFRAME is the internal representation of a single audio frame, which
 * consists of a single left and right audio sample in time. Only mono (1) and
 * stereo (2) channel(s) currently are supported.
 *
 *
 * @section sec_pdm_audio_timing    Timing
 *
 * Handling audio data in a virtual environment is hard, as the human perception
 * is very sensitive to the slightest cracks and stutters in the audible data.
 * This can happen if the VM's timing is lagging behind or not within the
 * expected time frame.
 *
 * The two main components which unfortunately contradict each other is a) the
 * audio device emulation and b) the audio backend(s) on the host. Those need to
 * be served in a timely manner to function correctly. To make e.g. the device
 * emulation rely on the pace the host backend(s) set - or vice versa - will not
 * work, as the guest's audio system / drivers then will not be able to
 * compensate this accordingly.
 *
 * So each component, the device emulation, the audio connector(s) and the
 * backend(s) must do its thing *when* it needs to do it, independently of the
 * others. For that we use various (small) ring buffers to (hopefully) serve all
 * components with the amount of data *when* they need it.
 *
 * Additionally, the device emulation can run with a different audio frame size,
 * while the backends(s) may require a different frame size (16 bit stereo
 * -> 8 bit mono, for example).
 *
 * The device emulation can give the audio connector(s) a scheduling hint
 * (optional), e.g. in which interval it expects any data processing.
 *
 * A data transfer for playing audio data from the guest on the host looks like
 * this: (RB = Ring Buffer, MB = Mixing Buffer)
 *
 * (A) Device DMA -> (B) Device RB -> (C) Audio Connector %Guest MB -> (D) Audio
 * Connector %Host MB -> (E) Backend RB (optional, up to the backend) -> (F)
 * Backend audio framework.
 *
 * When capturing audio data the chain is similar to the above one, just in a
 * different direction, of course.
 *
 * The audio connector hereby plays a key role when it comes to (pre-)buffering
 * data to minimize any audio stutters and/or cracks. The following values,
 * which also can be tweaked via CFGM / extra-data are available:
 *
 * - The pre-buffering time (in ms): Audio data which needs to be buffered
 *   before any playback (or capturing) can happen.
 * - The actual buffer size (in ms): How big the mixing buffer (for C and D)
 *   will be.
 * - The period size (in ms): How big a chunk of audio (often called period or
 *   fragment) for F must be to get handled correctly.
 *
 * The above values can be set on a per-driver level, whereas input and output
 * streams for a driver also can be handled set independently. The verbose audio
 * (release) log will tell about the (final) state of each audio stream.
 *
 *
 * @section sec_pdm_audio_diagram Diagram
 *
 * @todo r=bird: Not quite able to make sense of this, esp. the
 *       AUDMIXSINK/AUDIOMIXER bits crossing the LUN connections.
 *
 * @verbatim
               +----------------------------------+
               |Device (SB16 / AC'97 / HDA)       |
               |----------------------------------|
               |AUDIOMIXER (Optional)             |
               |AUDMIXSINK0 (Optional)            |
               |AUDMIXSINK1 (Optional)            |
               |AUDMIXSINKn (Optional)            |
               |                                  |
               |     L    L    L                  |
               |     U    U    U                  |
               |     N    N    N                  |
               |     0    1    n                  |
               +-----+----+----+------------------+
                     |    |    |
                     |    |    |
   +--------------+  |    |    |   +-------------+
   |AUDMIXSINK    |  |    |    |   |AUDIOMIXER   |
   |--------------|  |    |    |   |-------------|
   |AUDMIXSTREAM0 |+-|----|----|-->|AUDMIXSINK0  |
   |AUDMIXSTREAM1 |+-|----|----|-->|AUDMIXSINK1  |
   |AUDMIXSTREAMn |+-|----|----|-->|AUDMIXSINKn  |
   +--------------+  |    |    |   +-------------+
                     |    |    |
                     |    |    |
                +----+----+----+----+
                |LUN                |
                |-------------------|
                |PDMIAUDIOCONNECTOR |
                |AUDMIXSTREAM       |
                |                   +------+
                |                   |      |
                |                   |      |
                |                   |      |
                +-------------------+      |
                                           |
   +-------------------------+             |
   +-------------------------+        +----+--------------------+
   |PDMAUDIOSTREAM           |        |PDMIAUDIOCONNECTOR       |
   |-------------------------|        |-------------------------|
   |AUDIOMIXBUF              |+------>|PDMAUDIOSTREAM Host      |
   |PDMAUDIOSTREAMCFG        |+------>|PDMAUDIOSTREAM Guest     |
   |                         |        |Device capabilities      |
   |                         |        |Device configuration     |
   |                         |        |                         |
   |                         |    +--+|PDMIHOSTAUDIO            |
   |                         |    |   |+-----------------------+|
   +-------------------------+    |   ||Backend storage space  ||
                                  |   |+-----------------------+|
                                  |   +-------------------------+
                                  |
   +---------------------+        |
   |PDMIHOSTAUDIO        |        |
   |+--------------+     |        |
   ||DirectSound   |     |        |
   |+--------------+     |        |
   |                     |        |
   |+--------------+     |        |
   ||PulseAudio    |     |        |
   |+--------------+     |+-------+
   |                     |
   |+--------------+     |
   ||Core Audio    |     |
   |+--------------+     |
   |                     |
   |                     |
   |                     |
   |                     |
   +---------------------+
   @endverbatim
 */

#ifndef VBOX_INCLUDED_vmm_pdmaudioifs_h
#define VBOX_INCLUDED_vmm_pdmaudioifs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assertcompile.h>
#include <iprt/critsect.h>
#include <iprt/circbuf.h>
#include <iprt/list.h>
#include <iprt/path.h>

#include <VBox/types.h>
#include <VBox/vmm/pdmcommon.h>
#include <VBox/vmm/stam.h>

/** @defgroup grp_pdm_ifs_audio     PDM Audio Interfaces
 * @ingroup grp_pdm_interfaces
 * @{
 */

#ifndef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH
# if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
#  define VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "c:\\temp\\"
# else
#  define VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "/tmp/"
# endif
#endif

/** PDM audio driver instance flags. */
typedef uint32_t PDMAUDIODRVFLAGS;

/** No flags set. */
#define PDMAUDIODRVFLAGS_NONE       0
/** Marks a primary audio driver which is critical
 *  when running the VM. */
#define PDMAUDIODRVFLAGS_PRIMARY    RT_BIT(0)

/**
 * Audio format in signed or unsigned variants.
 */
typedef enum PDMAUDIOFMT
{
    /** Invalid format, do not use. */
    PDMAUDIOFMT_INVALID = 0,
    /** 8-bit, unsigned. */
    PDMAUDIOFMT_U8,
    /** 8-bit, signed. */
    PDMAUDIOFMT_S8,
    /** 16-bit, unsigned. */
    PDMAUDIOFMT_U16,
    /** 16-bit, signed. */
    PDMAUDIOFMT_S16,
    /** 32-bit, unsigned. */
    PDMAUDIOFMT_U32,
    /** 32-bit, signed. */
    PDMAUDIOFMT_S32,
    /** End of valid values. */
    PDMAUDIOFMT_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOFMT_32BIT_HACK = 0x7fffffff
} PDMAUDIOFMT;

/**
 * Audio direction.
 */
typedef enum PDMAUDIODIR
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIODIR_INVALID = 0,
    /** Unknown direction. */
    PDMAUDIODIR_UNKNOWN,
    /** Input. */
    PDMAUDIODIR_IN,
    /** Output. */
    PDMAUDIODIR_OUT,
    /** Duplex handling. */
    PDMAUDIODIR_DUPLEX,
    /** End of valid values. */
    PDMAUDIODIR_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIODIR_32BIT_HACK = 0x7fffffff
} PDMAUDIODIR;


/** @name PDMAUDIOHOSTDEV_F_XXX
 * @{  */
/** No flags set. */
#define PDMAUDIOHOSTDEV_F_NONE              UINT32_C(0)
/** The default input (capture/recording) device (for the user). */
#define PDMAUDIOHOSTDEV_F_DEFAULT_IN        RT_BIT_32(0)
/** The default output (playback) device (for the user). */
#define PDMAUDIOHOSTDEV_F_DEFAULT_OUT       RT_BIT_32(1)
/** The device can be removed at any time and we have to deal with it. */
#define PDMAUDIOHOSTDEV_F_HOTPLUG           RT_BIT_32(2)
/** The device is known to be buggy and needs special treatment. */
#define PDMAUDIOHOSTDEV_F_BUGGY             RT_BIT_32(3)
/** Ignore the device, no matter what. */
#define PDMAUDIOHOSTDEV_F_IGNORE            RT_BIT_32(4)
/** The device is present but marked as locked by some other application. */
#define PDMAUDIOHOSTDEV_F_LOCKED            RT_BIT_32(5)
/** The device is present but not in an alive state (dead). */
#define PDMAUDIOHOSTDEV_F_DEAD              RT_BIT_32(6)
/** Set if the PDMAUDIOHOSTDEV::pszId is allocated. */
#define PDMAUDIOHOSTDEV_F_ID_ALLOC          RT_BIT_32(30)
/** Set if the extra backend specific data cannot be duplicated. */
#define PDMAUDIOHOSTDEV_F_NO_DUP            RT_BIT_32(31)
/** @} */

/**
 * Audio device type.
 */
typedef enum PDMAUDIODEVICETYPE
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIODEVICETYPE_INVALID = 0,
    /** Unknown device type. This is the default. */
    PDMAUDIODEVICETYPE_UNKNOWN,
    /** Dummy device; for backends which are not able to report
     *  actual device information (yet). */
    PDMAUDIODEVICETYPE_DUMMY,
    /** The device is built into the host (non-removable). */
    PDMAUDIODEVICETYPE_BUILTIN,
    /** The device is an (external) USB device. */
    PDMAUDIODEVICETYPE_USB,
    /** End of valid values. */
    PDMAUDIODEVICETYPE_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIODEVICETYPE_32BIT_HACK = 0x7fffffff
} PDMAUDIODEVICETYPE;

/**
 * Host audio device info, part of enumeration result.
 *
 * @sa PDMAUDIOHOSTENUM, PDMIHOSTAUDIO::pfnGetDevices
 */
typedef struct PDMAUDIOHOSTDEV
{
    /** List entry (like PDMAUDIOHOSTENUM::LstDevices). */
    RTLISTNODE          ListEntry;
    /** Magic value (PDMAUDIOHOSTDEV_MAGIC). */
    uint32_t            uMagic;
    /** Size of this structure and whatever backend specific data that follows it. */
    uint32_t            cbSelf;
    /** The device type. */
    PDMAUDIODEVICETYPE  enmType;
    /** Usage of the device. */
    PDMAUDIODIR         enmUsage;
    /** Device flags, PDMAUDIOHOSTDEV_F_XXX. */
    uint32_t            fFlags;
    /** Maximum number of input audio channels the device supports. */
    uint8_t             cMaxInputChannels;
    /** Maximum number of output audio channels the device supports. */
    uint8_t             cMaxOutputChannels;
    uint8_t             abAlignment[ARCH_BITS == 32 ? 2 + 12 : 2];
    /** Device identifier, OS specific, can be NULL.
     * This can either point into some non-public part of this structure or to a
     * RTStrAlloc allocation.  PDMAUDIOHOSTDEV_F_ID_ALLOC is set in the latter
     * case. */
    char               *pszId;
    /** Friendly name of the device, if any. Could be truncated. */
    char                szName[64];
} PDMAUDIOHOSTDEV;
AssertCompileSizeAlignment(PDMAUDIOHOSTDEV, 16);
/** Pointer to audio device info (enumeration result). */
typedef PDMAUDIOHOSTDEV *PPDMAUDIOHOSTDEV;
/** Pointer to a const audio device info (enumeration result). */
typedef PDMAUDIOHOSTDEV const *PCPDMAUDIOHOSTDEV;

/** Magic value for PDMAUDIOHOSTDEV.  */
#define PDMAUDIOHOSTDEV_MAGIC       PDM_VERSION_MAKE(0xa0d0, 2, 0)


/**
 * A host audio device enumeration result.
 *
 * @sa PDMIHOSTAUDIO::pfnGetDevices
 */
typedef struct PDMAUDIOHOSTENUM
{
    /** Magic value (PDMAUDIOHOSTENUM_MAGIC). */
    uint32_t        uMagic;
    /** Number of audio devices in the list. */
    uint32_t        cDevices;
    /** List of audio devices (PDMAUDIOHOSTDEV). */
    RTLISTANCHOR    LstDevices;
} PDMAUDIOHOSTENUM;
/** Pointer to an audio device enumeration result. */
typedef PDMAUDIOHOSTENUM *PPDMAUDIOHOSTENUM;
/** Pointer to a const audio device enumeration result. */
typedef PDMAUDIOHOSTENUM const *PCPDMAUDIOHOSTENUM;

/** Magic for the host audio device enumeration. */
#define PDMAUDIOHOSTENUM_MAGIC      PDM_VERSION_MAKE(0xa0d1, 1, 0)


/**
 * Audio configuration (static) of an audio host backend.
 */
typedef struct PDMAUDIOBACKENDCFG
{
    /** The backend's friendly name. */
    char            szName[32];
    /** The size of the backend specific stream data (in bytes). */
    uint32_t        cbStream;
    /** PDMAUDIOBACKEND_F_XXX. */
    uint32_t        fFlags;
    /** Number of concurrent output (playback) streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t        cMaxStreamsOut;
    /** Number of concurrent input (recording) streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t        cMaxStreamsIn;
} PDMAUDIOBACKENDCFG;
/** Pointer to a static host audio audio configuration. */
typedef PDMAUDIOBACKENDCFG *PPDMAUDIOBACKENDCFG;

/** @name PDMAUDIOBACKEND_F_XXX - PDMAUDIOBACKENDCFG::fFlags
 * @{ */
/** PDMIHOSTAUDIO::pfnStreamConfigHint should preferably be called on a
 *  worker thread rather than EMT as it may take a good while. */
#define PDMAUDIOBACKEND_F_ASYNC_HINT            RT_BIT_32(0)
/** PDMIHOSTAUDIO::pfnStreamDestroy and any preceeding
 *  PDMIHOSTAUDIO::pfnStreamControl/DISABLE should be preferably be called on a
 *  worker thread rather than EMT as it may take a good while. */
#define PDMAUDIOBACKEND_F_ASYNC_STREAM_DESTROY  RT_BIT_32(1)
/** @} */


/**
 * A single audio frame.
 *
 * Currently only two (2) channels, left and right, are supported.
 *
 * @note When changing this structure, make sure to also handle
 *       VRDP's input / output processing in DrvAudioVRDE, as VRDP
 *       expects audio data in st_sample_t format (historical reasons)
 *       which happens to be the same as PDMAUDIOFRAME for now.
 *
 * @todo r=bird: This is an internal AudioMixBuffer structure which should not
 *       be exposed here, I think.  Only used to some sizeof statements in VRDE.
 *       (The problem with exposing it, is that we would like to move away from
 *       stereo and instead to anything from 1 to 16 channels.  That means
 *       removing this structure entirely.)
 */
typedef struct PDMAUDIOFRAME
{
    /** Left channel. */
    int64_t         i64LSample;
    /** Right channel. */
    int64_t         i64RSample;
} PDMAUDIOFRAME;
/** Pointer to a single (stereo) audio frame. */
typedef PDMAUDIOFRAME *PPDMAUDIOFRAME;
/** Pointer to a const single (stereo) audio frame. */
typedef PDMAUDIOFRAME const *PCPDMAUDIOFRAME;


/**
 * Audio path: input sources and playback destinations.
 *
 * Think of this as the name of the socket you plug the virtual audio stream
 * jack into.
 *
 * @note Not quite sure what the purpose of this type is.  It used to be two
 * separate enums (PDMAUDIOPLAYBACKDST & PDMAUDIORECSRC) without overlapping
 * values and most commonly used in a union (PDMAUDIODSTSRCUNION).  The output
 * values were designated "channel" (e.g. "Front channel"), whereas this was not
 * done to the input ones.  So, I'm (bird) a little confused what the actual
 * meaning was.
 */
typedef enum PDMAUDIOPATH
{
    /** Customary invalid zero value. */
    PDMAUDIOPATH_INVALID = 0,

    /** Unknown path / Doesn't care. */
    PDMAUDIOPATH_UNKNOWN,

    /** First output value. */
    PDMAUDIOPATH_OUT_FIRST,
    /** Output: Front. */
    PDMAUDIOPATH_OUT_FRONT = PDMAUDIOPATH_OUT_FIRST,
    /** Output: Center / LFE (Subwoofer). */
    PDMAUDIOPATH_OUT_CENTER_LFE,
    /** Output: Rear. */
    PDMAUDIOPATH_OUT_REAR,
    /** Last output value (inclusive)   */
    PDMAUDIOPATH_OUT_END = PDMAUDIOPATH_OUT_REAR,

    /** First input value. */
    PDMAUDIOPATH_IN_FIRST,
    /** Input: Microphone. */
    PDMAUDIOPATH_IN_MIC = PDMAUDIOPATH_IN_FIRST,
    /** Input: CD. */
    PDMAUDIOPATH_IN_CD,
    /** Input: Video-In. */
    PDMAUDIOPATH_IN_VIDEO,
    /** Input: AUX. */
    PDMAUDIOPATH_IN_AUX,
    /** Input: Line-In. */
    PDMAUDIOPATH_IN_LINE,
    /** Input: Phone-In. */
    PDMAUDIOPATH_IN_PHONE,
    /** Last intput value (inclusive). */
    PDMAUDIOPATH_IN_LAST = PDMAUDIOPATH_IN_PHONE,

    /** End of valid values. */
    PDMAUDIOPATH_END,
    /** Hack to blow the typ up to 32 bits. */
    PDMAUDIOPATH_32BIT_HACK = 0x7fffffff
} PDMAUDIOPATH;

/**
 * Audio stream (data) layout.
 */
typedef enum PDMAUDIOSTREAMLAYOUT
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIOSTREAMLAYOUT_INVALID = 0,
    /** Unknown access type; do not use (hdaR3StreamMapReset uses it). */
    PDMAUDIOSTREAMLAYOUT_UNKNOWN,
    /** Non-interleaved access, that is, consecutive access to the data.
     * @todo r=bird: For plain stereo this is actually interleaves left/right.  What
     *       I guess non-interleaved means, is that there are no additional
     *       information interleaved next to the interleaved stereo.
     *       https://stackoverflow.com/questions/17879933/whats-the-interleaved-audio */
    PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED,
    /** Interleaved access, where the data can be mixed together with data of other audio streams. */
    PDMAUDIOSTREAMLAYOUT_INTERLEAVED,
    /** Complex layout, which does not fit into the interleaved / non-interleaved layouts. */
    PDMAUDIOSTREAMLAYOUT_COMPLEX,
    /** Raw (pass through) data, with no data layout processing done.
     *
     *  This means that this stream will operate on PDMAUDIOFRAME data
     *  directly. Don't use this if you don't have to.
     *
     * @deprecated Replaced by S64 (signed, 64-bit sample size).  */
    PDMAUDIOSTREAMLAYOUT_RAW,
    /** End of valid values. */
    PDMAUDIOSTREAMLAYOUT_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMLAYOUT_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMLAYOUT;

/**
 * Stream channel data block.
 */
typedef struct PDMAUDIOSTREAMCHANNELDATA
{
    /** Circular buffer for the channel data. */
    PRTCIRCBUF          pCircBuf;
    /** Amount of audio data (in bytes) acquired for reading. */
    size_t              cbAcq;
    /** Channel data flags, PDMAUDIOSTREAMCHANNELDATA_FLAGS_XXX. */
    uint32_t            fFlags;
} PDMAUDIOSTREAMCHANNELDATA;
/** Pointer to audio stream channel data buffer. */
typedef PDMAUDIOSTREAMCHANNELDATA  *PPDMAUDIOSTREAMCHANNELDATA;

/** @name PDMAUDIOSTREAMCHANNELDATA_FLAGS_XXX
 *  @{ */
/** No stream channel data flags defined. */
#define PDMAUDIOSTREAMCHANNELDATA_FLAGS_NONE      UINT32_C(0)
/** @} */

/**
 * Standard speaker channel IDs.
 *
 * This can cover up to 11.0 surround sound.
 *
 * @note Any of those channels can be marked / used as the LFE channel (played
 *       through the subwoofer).
 */
typedef enum PDMAUDIOSTREAMCHANNELID
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIOSTREAMCHANNELID_INVALID = 0,
    /** Unknown / not set channel ID. */
    PDMAUDIOSTREAMCHANNELID_UNKNOWN,
    /** Front left channel. */
    PDMAUDIOSTREAMCHANNELID_FRONT_LEFT,
    /** Front right channel. */
    PDMAUDIOSTREAMCHANNELID_FRONT_RIGHT,
    /** Front center channel. */
    PDMAUDIOSTREAMCHANNELID_FRONT_CENTER,
    /** Low frequency effects (subwoofer) channel. */
    PDMAUDIOSTREAMCHANNELID_LFE,
    /** Rear left channel. */
    PDMAUDIOSTREAMCHANNELID_REAR_LEFT,
    /** Rear right channel. */
    PDMAUDIOSTREAMCHANNELID_REAR_RIGHT,
    /** Front left of center channel. */
    PDMAUDIOSTREAMCHANNELID_FRONT_LEFT_OF_CENTER,
    /** Front right of center channel. */
    PDMAUDIOSTREAMCHANNELID_FRONT_RIGHT_OF_CENTER,
    /** Rear center channel. */
    PDMAUDIOSTREAMCHANNELID_REAR_CENTER,
    /** Side left channel. */
    PDMAUDIOSTREAMCHANNELID_SIDE_LEFT,
    /** Side right channel. */
    PDMAUDIOSTREAMCHANNELID_SIDE_RIGHT,
    /** Left height channel. */
    PDMAUDIOSTREAMCHANNELID_LEFT_HEIGHT,
    /** Right height channel. */
    PDMAUDIOSTREAMCHANNELID_RIGHT_HEIGHT,
    /** End of valid values. */
    PDMAUDIOSTREAMCHANNELID_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMCHANNELID_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMCHANNELID;

/**
 * Mappings channels onto an audio stream.
 *
 * The mappings are either for a single (mono) or dual (stereo) channels onto an
 * audio stream (aka stream profile).  An audio stream consists of one or
 * multiple channels (e.g. 1 for mono, 2 for stereo), depending on the
 * configuration.
 */
typedef struct PDMAUDIOSTREAMMAP
{
    /** Array of channel IDs being handled.
     * @note The first (zero-based) index specifies the leftmost channel. */
    PDMAUDIOSTREAMCHANNELID     aenmIDs[2];
    /** Step size (in bytes) to the channel's next frame. */
    uint32_t                    cbStep;
    /** Frame size (in bytes) of this channel. */
    uint32_t                    cbFrame;
    /** Byte offset to the first frame in the data block. */
    uint32_t                    offFirst;
    /** Byte offset to the next frame in the data block. */
    uint32_t                    offNext;
    /** Associated data buffer. */
    PDMAUDIOSTREAMCHANNELDATA   Data;

    /** @todo r=bird: I'd structure this very differently.
     * I would've had an array of channel descriptors like this:
     *
     * struct PDMAUDIOCHANNELDESC
     * {
     *     uint8_t      off;    //< Stream offset in bytes.
     *     uint8_t      id;     //< PDMAUDIOSTREAMCHANNELID
     * };
     *
     * And I'd baked it into PDMAUDIOPCMPROPS as a fixed sized array with 16 entries
     * (max HDA channel count IIRC).  */
} PDMAUDIOSTREAMMAP;
/** Pointer to an audio stream channel mapping. */
typedef PDMAUDIOSTREAMMAP *PPDMAUDIOSTREAMMAP;

/**
 * Properties of audio streams for host/guest for in or out directions.
 */
typedef struct PDMAUDIOPCMPROPS
{
    /** The frame size. */
    uint8_t     cbFrame;
    /** Shift count used with PDMAUDIOPCMPROPS_F2B and PDMAUDIOPCMPROPS_B2F.
     * Depends on number of stream channels and the stream format being used, calc
     * value using PDMAUDIOPCMPROPS_MAKE_SHIFT.
     * @sa   PDMAUDIOSTREAMCFG_B2F, PDMAUDIOSTREAMCFG_F2B */
    uint8_t     cShiftX;
    /** Sample width (in bytes). */
    RT_GCC_EXTENSION
    uint8_t     cbSampleX : 4;
    /** Number of audio channels. */
    RT_GCC_EXTENSION
    uint8_t     cChannelsX : 4;
    /** Signed or unsigned sample. */
    bool        fSigned : 1;
    /** Whether the endianness is swapped or not. */
    bool        fSwapEndian : 1;
    /** Raw mixer frames, only applicable for signed 64-bit samples. */
    bool        fRaw : 1;
    /** Sample frequency in Hertz (Hz). */
    uint32_t    uHz;
} PDMAUDIOPCMPROPS;
AssertCompileSize(PDMAUDIOPCMPROPS, 8);
AssertCompileSizeAlignment(PDMAUDIOPCMPROPS, 8);
/** Pointer to audio stream properties. */
typedef PDMAUDIOPCMPROPS *PPDMAUDIOPCMPROPS;
/** Pointer to const audio stream properties. */
typedef PDMAUDIOPCMPROPS const *PCPDMAUDIOPCMPROPS;

/** @name Macros for use with PDMAUDIOPCMPROPS
 * @{ */
/** Initializer for PDMAUDIOPCMPROPS. */
#define PDMAUDIOPCMPROPS_INITIALIZER(a_cbSample, a_fSigned, a_cChannels, a_uHz, a_fSwapEndian) \
    { (a_cbSample) * (a_cChannels), PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(a_cbSample, a_cChannels), a_cbSample, a_cChannels, \
      a_fSigned, a_fSwapEndian, false /*fRaw*/, a_uHz }
/** Calculates the cShift value of given sample bits and audio channels.
 * @note Does only support mono/stereo channels for now, for non-stereo/mono we
 *       returns a special value which the two conversion functions detect
 *       and make them fall back on cbSample * cChannels. */
#define PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(cbSample, cChannels) \
    ( RT_IS_POWER_OF_TWO((unsigned)((cChannels) * (cbSample))) \
      ? (uint8_t)(ASMBitFirstSetU32((unsigned)((cChannels) * (cbSample))) - 1) : (uint8_t)UINT8_MAX )
/** Calculates the cShift value of a PDMAUDIOPCMPROPS structure. */
#define PDMAUDIOPCMPROPS_MAKE_SHIFT(pProps) \
    PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS((pProps)->cbSampleX, (pProps)->cChannelsX)
/** Converts (audio) frames to bytes.
 * @note Requires properly initialized properties, i.e. cbFrames correctly calculated
 *       and cShift set using PDMAUDIOPCMPROPS_MAKE_SHIFT. */
#define PDMAUDIOPCMPROPS_F2B(pProps, cFrames) \
    ( (pProps)->cShiftX != UINT8_MAX ? (cFrames) << (pProps)->cShiftX : (cFrames) * (pProps)->cbFrame )
/** Converts bytes to (audio) frames.
 * @note Requires properly initialized properties, i.e. cbFrames correctly calculated
 *       and cShift set using PDMAUDIOPCMPROPS_MAKE_SHIFT. */
#define PDMAUDIOPCMPROPS_B2F(pProps, cb) \
    ( (pProps)->cShiftX != UINT8_MAX ?      (cb) >> (pProps)->cShiftX :      (cb) / (pProps)->cbFrame )
/** @}   */

/**
 * An audio stream configuration.
 */
typedef struct PDMAUDIOSTREAMCFG
{
    /** Direction of the stream. */
    PDMAUDIODIR             enmDir;
    /** Destination / source path. */
    PDMAUDIOPATH            enmPath;
    /** The stream's PCM properties. */
    PDMAUDIOPCMPROPS        Props;
    /** The stream's audio data layout.
     *  This indicates how the audio data buffers to/from the backend is being layouted.
     *
     *  Currently, the following layouts are supported by the audio connector:
     *
     *  PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED:
     *      One stream at once. The consecutive audio data is exactly in the format and frame width
     *      like defined in the PCM properties. This is the default.
     *
     *  PDMAUDIOSTREAMLAYOUT_RAW:
     *      Can be one or many streams at once, depending on the stream's mixing buffer setup.
     *      The audio data will get handled as PDMAUDIOFRAME frames without any modification done.
     *
     * @todo r=bird: See PDMAUDIOSTREAMLAYOUT comments. */
    PDMAUDIOSTREAMLAYOUT    enmLayout;
    /** Device emulation-specific data needed for the audio connector. */
    struct
    {
        /** Scheduling hint set by the device emulation about when this stream is being served on average (in ms).
         *  Can be 0 if not hint given or some other mechanism (e.g. callbacks) is being used. */
        uint32_t            cMsSchedulingHint;
    } Device;
    /**
     * Backend-specific data for the stream.
     * On input (requested configuration) those values are set by the audio connector to let the backend know what we expect.
     * On output (acquired configuration) those values reflect the values set and used by the backend.
     * Set by the backend on return. Not all backends support all values / features.
     */
    struct
    {
        /** Period size of the stream (in audio frames).
         *  This value reflects the number of audio frames in between each hardware interrupt on the
         *  backend (host) side. 0 if not set / available by the backend. */
        uint32_t            cFramesPeriod;
        /** (Ring) buffer size (in audio frames). Often is a multiple of cFramesPeriod.
         *  0 if not set / available by the backend. */
        uint32_t            cFramesBufferSize;
        /** Pre-buffering size (in audio frames). Frames needed in buffer before the stream becomes active (pre buffering).
         *  The bigger this value is, the more latency for the stream will occur.
         *  0 if not set / available by the backend. UINT32_MAX if not defined (yet). */
        uint32_t            cFramesPreBuffering;
    } Backend;
    uint32_t                u32Padding;
    /** Friendly name of the stream. */
    char                    szName[64];
} PDMAUDIOSTREAMCFG;
AssertCompileSizeAlignment(PDMAUDIOSTREAMCFG, 8);
/** Pointer to audio stream configuration keeper. */
typedef PDMAUDIOSTREAMCFG *PPDMAUDIOSTREAMCFG;
/** Pointer to a const audio stream configuration keeper. */
typedef PDMAUDIOSTREAMCFG const *PCPDMAUDIOSTREAMCFG;

/** Converts (audio) frames to bytes. */
#define PDMAUDIOSTREAMCFG_F2B(pCfg, frames)     PDMAUDIOPCMPROPS_F2B(&(pCfg)->Props, (frames))
/** Converts bytes to (audio) frames. */
#define PDMAUDIOSTREAMCFG_B2F(pCfg, cb)         PDMAUDIOPCMPROPS_B2F(&(pCfg)->Props, (cb))

/**
 * Audio mixer controls.
 */
typedef enum PDMAUDIOMIXERCTL
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIOMIXERCTL_INVALID = 0,
    /** Unknown mixer control. */
    PDMAUDIOMIXERCTL_UNKNOWN,
    /** Master volume. */
    PDMAUDIOMIXERCTL_VOLUME_MASTER,
    /** Front. */
    PDMAUDIOMIXERCTL_FRONT,
    /** Center / LFE (Subwoofer). */
    PDMAUDIOMIXERCTL_CENTER_LFE,
    /** Rear. */
    PDMAUDIOMIXERCTL_REAR,
    /** Line-In. */
    PDMAUDIOMIXERCTL_LINE_IN,
    /** Microphone-In. */
    PDMAUDIOMIXERCTL_MIC_IN,
    /** End of valid values. */
    PDMAUDIOMIXERCTL_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOMIXERCTL_32BIT_HACK = 0x7fffffff
} PDMAUDIOMIXERCTL;

/**
 * Audio stream commands.
 *
 * Used in the audio connector as well as in the actual host backends.
 */
typedef enum PDMAUDIOSTREAMCMD
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIOSTREAMCMD_INVALID = 0,
    /** Enables the stream. */
    PDMAUDIOSTREAMCMD_ENABLE,
    /** Pauses the stream.
     * This is currently only issued when the VM is suspended (paused).
     * @remarks This is issued by DrvAudio, never by the mixer or devices. */
    PDMAUDIOSTREAMCMD_PAUSE,
    /** Resumes the stream.
     * This is currently only issued when the VM is resumed.
     * @remarks This is issued by DrvAudio, never by the mixer or devices. */
    PDMAUDIOSTREAMCMD_RESUME,
    /** Drain the stream, that is, play what's in the buffers and then stop.
     *
     * There will be no more samples written after this command is issued.
     * PDMIAUDIOCONNECTOR::pfnStreamIterate will drive progress for DrvAudio and
     * calls to PDMIHOSTAUDIO::pfnStreamPlay with a zero sized buffer will provide
     * the backend with a way to drive it forwards.  These calls will come at a
     * frequency set by the device and be on an asynchronous I/O thread.
     *
     * A DISABLE command maybe submitted if the device/mixer wants to re-enable the
     * stream while it's still draining or if it gets impatient and thinks the
     * draining has been going on too long, in which case the stream should stop
     * immediately.
     *
     * @note    This should not wait for the stream to finish draining, just change
     *          the state.  (The caller could be an EMT and it must not block for
     *          hundreds of milliseconds of buffer to finish draining.)
     *
     * @note    Does not apply to input streams. Backends should refuse such requests. */
    PDMAUDIOSTREAMCMD_DRAIN,
    /** Stops the stream immediately w/o any draining. */
    PDMAUDIOSTREAMCMD_DISABLE,
    /** End of valid values. */
    PDMAUDIOSTREAMCMD_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMCMD_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMCMD;

/**
 * Audio volume parameters.
 */
typedef struct PDMAUDIOVOLUME
{
    /** Set to @c true if this stream is muted, @c false if not. */
    bool    fMuted;
    /** Left channel volume.
     *  Range is from [0 ... 255], whereas 0 specifies
     *  the most silent and 255 the loudest value. */
    uint8_t uLeft;
    /** Right channel volume.
     *  Range is from [0 ... 255], whereas 0 specifies
     *  the most silent and 255 the loudest value. */
    uint8_t uRight;
} PDMAUDIOVOLUME;
/** Pointer to audio volume settings. */
typedef PDMAUDIOVOLUME *PPDMAUDIOVOLUME;
/** Pointer to const audio volume settings. */
typedef PDMAUDIOVOLUME const *PCPDMAUDIOVOLUME;

/** Defines the minimum volume allowed. */
#define PDMAUDIO_VOLUME_MIN     (0)
/** Defines the maximum volume allowed. */
#define PDMAUDIO_VOLUME_MAX     (255)


/**
 * Backend status.
 */
typedef enum PDMAUDIOBACKENDSTS
{
    /** Unknown/invalid status. */
    PDMAUDIOBACKENDSTS_UNKNOWN = 0,
    /** No backend attached. */
    PDMAUDIOBACKENDSTS_NOT_ATTACHED,
    /** The backend is in its initialization phase.
     *  Not all backends support this status. */
    PDMAUDIOBACKENDSTS_INITIALIZING,
    /** The backend has stopped its operation. */
    PDMAUDIOBACKENDSTS_STOPPED,
    /** The backend is up and running. */
    PDMAUDIOBACKENDSTS_RUNNING,
    /** The backend ran into an error and is unable to recover.
     *  A manual re-initialization might help. */
    PDMAUDIOBACKENDSTS_ERROR,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOBACKENDSTS_32BIT_HACK = 0x7fffffff
} PDMAUDIOBACKENDSTS;

/**
 * PDM audio stream state.
 *
 * This is all the mixer/device needs.  The PDMAUDIOSTREAM_STS_XXX stuff will
 * become DrvAudio internal state once the backend stuff is destilled out of it.
 *
 * @note    The value order is significant, don't change it willy-nilly.
 */
typedef enum PDMAUDIOSTREAMSTATE
{
    /** Invalid state value. */
    PDMAUDIOSTREAMSTATE_INVALID = 0,
    /** The stream is not operative and cannot be enabled. */
    PDMAUDIOSTREAMSTATE_NOT_WORKING,
    /** The stream needs to be re-initialized by the device/mixer
     * (i.e. call PDMIAUDIOCONNECTOR::pfnStreamReInit). */
    PDMAUDIOSTREAMSTATE_NEED_REINIT,
    /** The stream is inactive (not enabled). */
    PDMAUDIOSTREAMSTATE_INACTIVE,
    /** The stream is enabled but nothing to read/write.
     *  @todo not sure if we need this variant... */
    PDMAUDIOSTREAMSTATE_ENABLED,
    /** The stream is enabled and captured samples can be read. */
    PDMAUDIOSTREAMSTATE_ENABLED_READABLE,
    /** The stream is enabled and samples can be written for playback. */
    PDMAUDIOSTREAMSTATE_ENABLED_WRITABLE,
    /** End of valid states.   */
    PDMAUDIOSTREAMSTATE_END,
    /** Make sure the type is 32-bit wide. */
    PDMAUDIOSTREAMSTATE_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMSTATE;

/** @name PDMAUDIOSTREAM_CREATE_F_XXX
 * @{ */
/** Does not need any mixing buffers, the device takes care of all conversion. */
#define PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF       RT_BIT_32(0)
/** @} */

/** @name PDMAUDIOSTREAM_WARN_FLAGS_XXX
 * @{ */
/** No stream warning flags set. */
#define PDMAUDIOSTREAM_WARN_FLAGS_NONE          0
/** Warned about a disabled stream. */
#define PDMAUDIOSTREAM_WARN_FLAGS_DISABLED      RT_BIT(0)
/** @} */

/**
 * An input or output audio stream.
 */
typedef struct PDMAUDIOSTREAM
{
    /** Critical section protecting the stream.
     *
     * When not otherwise stated, DrvAudio will enter this before calling the
     * backend.   The backend and device/mixer can normally safely enter it prior to
     * a DrvAudio call, however not to pfnStreamDestroy, pfnStreamRelease or
     * anything that may access the stream list.
     *
     * @note Lock ordering:
     *          - After DRVAUDIO::CritSectGlobals.
     *          - Before DRVAUDIO::CritSectHotPlug. */
    RTCRITSECT              CritSect;
    /** Magic value (PDMAUDIOSTREAM_MAGIC). */
    uint32_t                uMagic;
    /** Audio direction of this stream. */
    PDMAUDIODIR             enmDir;
    /** Size (in bytes) of the backend-specific stream data. */
    uint32_t                cbBackend;
    /** Warnings shown already in the release log.
     *  See PDMAUDIOSTREAM_WARN_FLAGS_XXX. */
    uint32_t                fWarningsShown;
    /** The stream properties (both sides when PDMAUDIOSTREAM_CREATE_F_NO_MIXBUF
     * is used, otherwise the guest side). */
    PDMAUDIOPCMPROPS        Props;

    /** Name of this stream. */
    char                    szName[64];
} PDMAUDIOSTREAM;
/** Pointer to an audio stream. */
typedef struct PDMAUDIOSTREAM *PPDMAUDIOSTREAM;
/** Pointer to a const audio stream. */
typedef struct PDMAUDIOSTREAM const *PCPDMAUDIOSTREAM;

/** Magic value for PDMAUDIOSTREAM. */
#define PDMAUDIOSTREAM_MAGIC    PDM_VERSION_MAKE(0xa0d3, 5, 0)



/** Pointer to a audio connector interface. */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;

/**
 * Audio connector interface (up).
 */
typedef struct PDMIAUDIOCONNECTOR
{
    /**
     * Enables or disables the given audio direction for this driver.
     *
     * When disabled, assiociated output streams consume written audio without passing them further down to the backends.
     * Associated input streams then return silence when read from those.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmDir          Audio direction to enable or disable driver for.
     * @param   fEnable         Whether to enable or disable the specified audio direction.
     *
     * @note    Be very careful when using this function, as this could
     *          violate / run against the (global) VM settings.  See @bugref{9882}.
     */
    DECLR3CALLBACKMEMBER(int, pfnEnable, (PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir, bool fEnable));

    /**
     * Returns whether the given audio direction for this driver is enabled or not.
     *
     * @returns True if audio is enabled for the given direction, false if not.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmDir          Audio direction to retrieve enabled status for.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsEnabled, (PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir));

    /**
     * Retrieves the current configuration of the host audio backend.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pCfg            Where to store the host audio backend configuration data.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConfig, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOBACKENDCFG pCfg));

    /**
     * Retrieves the current status of the host audio backend.
     *
     * @returns Status of the host audio backend.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmDir          Audio direction to check host audio backend for. Specify PDMAUDIODIR_DUPLEX for the overall
     *                          backend status.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOBACKENDSTS, pfnGetStatus, (PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir));

    /**
     * Gives the audio drivers a hint about a typical configuration.
     *
     * This is a little hack for windows (and maybe other hosts) where stream
     * creation can take a relatively long time, making it very unsuitable for EMT.
     * The audio backend can use this hint to cache pre-configured stream setups,
     * so that when the guest actually wants to play something EMT won't be blocked
     * configuring host audio.
     *
     * @param   pInterface      Pointer to this interface.
     * @param   pCfg            The typical configuration.  Can be modified by the
     *                          drivers in unspecified ways.
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamConfigHint, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAMCFG pCfg));

    /**
     * Creates an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fFlags          PDMAUDIOSTREAM_CREATE_F_XXX.
     * @param   pCfgHost        Stream configuration for host side.
     * @param   pCfgGuest       Stream configuration for guest side.
     * @param   ppStream        Pointer where to return the created audio stream on success.
     * @todo r=bird: It is not documented how pCfgHost and pCfgGuest can be
     *       modified the DrvAudio...
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCreate, (PPDMIAUDIOCONNECTOR pInterface, uint32_t fFlags, PPDMAUDIOSTREAMCFG pCfgHost,
                                                PPDMAUDIOSTREAMCFG pCfgGuest, PPDMAUDIOSTREAM *ppStream));


    /**
     * Destroys an audio stream.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     * @param   fImmediate      Whether to immdiately stop and destroy a draining
     *                          stream (@c true), or to allow it to complete
     *                          draining first (@c false) if that's feasable.
     *                          The latter depends on the draining stage and what
     *                          the backend is capable of.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, bool fImmediate));

    /**
     * Re-initializes the stream in response to PDMAUDIOSTREAM_STS_NEED_REINIT.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pStream         The audio stream needing re-initialization.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamReInit, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Adds a reference to the specified audio stream.
     *
     * @returns New reference count. UINT32_MAX on error.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream adding the reference to.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamRetain, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Releases a reference from the specified stream.
     *
     * @returns New reference count. UINT32_MAX on error.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream releasing a reference from.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamRelease, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Controls a specific audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     * @param   enmStreamCmd    The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamControl, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                                 PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Processes stream data.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamIterate, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the number of readable data (in bytes) of a specific audio input stream.
     *
     * @returns Number of bytes of readable data.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetReadable, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the number of writable data (in bytes) of a specific audio output stream.
     *
     * @returns Number of bytes writable data.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetWritable, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the state of a specific audio stream (destilled status).
     *
     * @returns PDMAUDIOSTREAMSTATE value.
     * @retval  PDMAUDIOSTREAMSTATE_INVALID if the input isn't valid (w/ assertion).
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOSTREAMSTATE, pfnStreamGetState, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Sets the audio volume of a specific audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     * @param   pVol            Pointer to audio volume structure to set the stream's audio volume to.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamSetVolume, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOVOLUME pVol));

    /**
     * Plays (writes to) an audio (output) stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream to read from.
     * @param   pvBuf           Audio data to be written.
     * @param   cbBuf           Number of bytes to be written.
     * @param   pcbWritten      Bytes of audio data written. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamPlay, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                              const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten));

    /**
     * Reads PCM audio data from the host (input).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream to write to.
     * @param   pvBuf           Where to store the read data.
     * @param   cbBuf           Number of bytes to read.
     * @param   pcbRead         Bytes of audio data read. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamRead, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                              void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead));

    /**
     * Captures (transfers) available audio frames from the host backend.
     *
     * Only works with input streams.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pStream              Pointer to audio stream.
     * @param   pcFramesCaptured     Number of frames captured. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCapture, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                                 uint32_t *pcFramesCaptured));

} PDMIAUDIOCONNECTOR;

/** PDMIAUDIOCONNECTOR interface ID. */
#define PDMIAUDIOCONNECTOR_IID                  "ff9cabf0-4138-4c3a-aa99-28bf7a6feae7"


/**
 * Host audio backend specific stream data.
 *
 * The backend will put this as the first member of it's own data structure.
 */
typedef struct PDMAUDIOBACKENDSTREAM
{
    /** Magic value (PDMAUDIOBACKENDSTREAM_MAGIC). */
    uint32_t            uMagic;
    /** Explicit zero padding - do not touch! */
    uint32_t            uReserved;
    /** Pointer to the stream this backend data is associated with. */
    PPDMAUDIOSTREAM     pStream;
    /** Reserved for future use (zeroed) - do not touch. */
    void               *apvReserved[2];
} PDMAUDIOBACKENDSTREAM;
/** Pointer to host audio specific stream data! */
typedef PDMAUDIOBACKENDSTREAM *PPDMAUDIOBACKENDSTREAM;

/** Magic value for PDMAUDIOBACKENDSTREAM. */
#define PDMAUDIOBACKENDSTREAM_MAGIC PDM_VERSION_MAKE(0xa0d4, 1, 0)

/**
 * Host audio (backend) stream state returned by PDMIHOSTAUDIO::pfnStreamGetState.
 */
typedef enum PDMHOSTAUDIOSTREAMSTATE
{
    /** Invalid zero value, as per usual.   */
    PDMHOSTAUDIOSTREAMSTATE_INVALID = 0,
    /** The stream is being initialized.
     * This should also be used when switching to a new device and the stream
     * stops to work with the old device while the new one being configured.  */
    PDMHOSTAUDIOSTREAMSTATE_INITIALIZING,
    /** The stream does not work (async init failed, audio subsystem gone
     *  fishing, or similar). */
    PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING,
    /** Backend is working okay. */
    PDMHOSTAUDIOSTREAMSTATE_OKAY,
    /** Backend is working okay, but currently draining the stream. */
    PDMHOSTAUDIOSTREAMSTATE_DRAINING,
    /** Backend is working but doesn't want any commands or data reads/writes. */
    PDMHOSTAUDIOSTREAMSTATE_INACTIVE,
    /** End of valid values. */
    PDMHOSTAUDIOSTREAMSTATE_END,
    /** Blow the type up to 32 bits. */
    PDMHOSTAUDIOSTREAMSTATE_32BIT_HACK = 0x7fffffff
} PDMHOSTAUDIOSTREAMSTATE;


/** Pointer to a host audio interface. */
typedef struct PDMIHOSTAUDIO *PPDMIHOSTAUDIO;

/**
 * PDM host audio interface.
 */
typedef struct PDMIHOSTAUDIO
{
    /**
     * Returns the host backend's configuration (backend).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pBackendCfg         Where to store the backend audio configuration to.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConfig, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg));

    /**
     * Returns (enumerates) host audio device information (optional).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pDeviceEnum         Where to return the enumerated audio devices.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetDevices, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum));

    /**
     * Returns the current status from the audio backend (optional).
     *
     * @returns PDMAUDIOBACKENDSTS enum.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   enmDir              Audio direction to get status for. Pass PDMAUDIODIR_DUPLEX for overall status.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOBACKENDSTS, pfnGetStatus, (PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir));

    /**
     * Callback for genric on-worker-thread requests initiated by the backend itself.
     *
     * This is the counterpart to PDMIHOSTAUDIOPORT::pfnDoOnWorkerThread that will
     * be invoked on a worker thread when the backend requests it - optional.
     *
     * This does not return a value, so the backend must keep track of
     * failure/success on its own.
     *
     * This method is optional.  A non-NULL will, together with pfnStreamInitAsync
     * and PDMAUDIOBACKEND_F_ASYNC_HINT, force DrvAudio to create the thread pool.
     *
     * @param   pInterface  Pointer to this interface.
     * @param   pStream     Optionally a backend stream if specified in the
     *                      PDMIHOSTAUDIOPORT::pfnDoOnWorkerThread() call.
     * @param   uUser       User specific value as specified in the
     *                      PDMIHOSTAUDIOPORT::pfnDoOnWorkerThread() call.
     * @param   pvUser      User specific pointer as specified in the
     *                      PDMIHOSTAUDIOPORT::pfnDoOnWorkerThread() call.
     */
    DECLR3CALLBACKMEMBER(void, pfnDoOnWorkerThread,(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                    uintptr_t uUser, void *pvUser));

    /**
     * Gives the audio backend a hint about a typical configuration (optional).
     *
     * This is a little hack for windows (and maybe other hosts) where stream
     * creation can take a relatively long time, making it very unsuitable for EMT.
     * The audio backend can use this hint to cache pre-configured stream setups,
     * so that when the guest actually wants to play something EMT won't be blocked
     * configuring host audio.
     *
     * The backend can return PDMAUDIOBACKEND_F_ASYNC_HINT in
     * PDMIHOSTAUDIO::pfnGetConfig to avoid having EMT making this call and thereby
     * speeding up VM construction.
     *
     * @param   pInterface      Pointer to this interface.
     * @param   pCfg            The typical configuration.  (Feel free to change it
     *                          to the actual stream config that would be used,
     *                          however caller will probably ignore this.)
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamConfigHint, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAMCFG pCfg));

    /**
     * Creates an audio stream using the requested stream configuration.
     *
     * If a backend is not able to create this configuration, it will return its
     * best match in the acquired configuration structure on success.
     *
     * @returns VBox status code.
     * @retval  VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED if
     *          PDMIHOSTAUDIO::pfnStreamInitAsync should be called.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   pCfgReq             Pointer to requested stream configuration.
     * @param   pCfgAcq             Pointer to acquired stream configuration.
     * @todo    r=bird: Implementation (at least Alsa) seems to make undocumented
     *          assumptions about the content of @a pCfgAcq.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCreate, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq));

    /**
     * Asynchronous stream initialization step, optional.
     *
     * This is called on a worker thread iff the PDMIHOSTAUDIO::pfnStreamCreate
     * method returns VINF_AUDIO_STREAM_ASYNC_INIT_NEEDED.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pStream             Pointer to audio stream to continue
     *                              initialization of.
     * @param   fDestroyed          Set to @c true if the stream has been destroyed
     *                              before the worker thread got to making this
     *                              call.  The backend should just ready the stream
     *                              for destruction in that case.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamInitAsync, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream, bool fDestroyed));

    /**
     * Destroys an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface containing the called function.
     * @param   pStream     Pointer to audio stream.
     * @param   fImmediate  Whether to immdiately stop and destroy a draining
     *                      stream (@c true), or to allow it to complete
     *                      draining first (@c false) if that's feasable.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream, bool fImmediate));

    /**
     * Called from PDMIHOSTAUDIOPORT::pfnNotifyDeviceChanged so the backend can start
     * the device change for a stream.
     *
     * This is mainly to avoid the need for a list of streams in the backend.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pStream             Pointer to audio stream (locked).
     * @param   pvUser              Backend specific parameter from the call to
     *                              PDMIHOSTAUDIOPORT::pfnNotifyDeviceChanged.
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamNotifyDeviceChanged,(PPDMIHOSTAUDIO pInterface,
                                                             PPDMAUDIOBACKENDSTREAM pStream, void *pvUser));

    /**
     * Controls an audio stream.
     *
     * @returns VBox status code.
     * @retval  VERR_AUDIO_STREAM_NOT_READY if stream is not ready for required operation (yet).
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   enmStreamCmd        The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamControl, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                 PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Returns the amount which is readable from the audio (input) stream.
     *
     * @returns For non-raw layout streams: Number of readable bytes.
     *          for raw layout streams    : Number of readable audio frames.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetReadable, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Returns the amount which is writable to the audio (output) stream.
     *
     * @returns Number of writable bytes.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetWritable, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Returns the number of buffered bytes that hasn't been played yet (optional).
     *
     * Is not valid on an input stream, implementions shall assert and return zero.
     *
     * @returns Number of pending bytes.
     * @param   pInterface          Pointer to this interface.
     * @param   pStream             Pointer to audio stream.
     *
     * @todo This is no longer not used by DrvAudio and can probably be removed.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetPending, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Returns the current state of the given backend stream.
     *
     * @returns PDMHOSTAUDIOSTREAMSTATE value.
     * @retval  PDMHOSTAUDIOSTREAMSTATE_INVALID if invalid stream.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMHOSTAUDIOSTREAMSTATE, pfnStreamGetState, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Plays (writes to) an audio (output) stream.
     *
     * This is always called with data in the buffer, except after
     * PDMAUDIOSTREAMCMD_DRAIN is issued when it's called every so often to assist
     * the backend with moving the draining operation forward (kind of like
     * PDMIAUDIOCONNECTOR::pfnStreamIterate).
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface structure containing the called function pointer.
     * @param   pStream     Pointer to audio stream.
     * @param   pvBuf       Pointer to audio data buffer to play.  This will be NULL
     *                      when called to assist draining the stream.
     * @param   cbBuf       The number of bytes of audio data to play.  This will be
     *                      zero when called to assist draining the stream.
     * @param   pcbWritten  Where to return the actual number of bytes played.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamPlay, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                              const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten));

    /**
     * Captures (reads from) an audio (input) stream.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface structure containing the called function pointer.
     * @param   pStream     Pointer to audio stream.
     * @param   pvBuf       Buffer where to store read audio data.
     * @param   cbBuf       Size of the audio data buffer in bytes.
     * @param   pcbRead     Where to return the number of bytes actually captured.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCapture, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                 void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead));
} PDMIHOSTAUDIO;

/** PDMIHOSTAUDIO interface ID. */
#define PDMIHOSTAUDIO_IID                           "a6b33abc-1393-4548-92ab-a308d54de1e8"


/** Pointer to a audio notify from host interface. */
typedef struct PDMIHOSTAUDIOPORT *PPDMIHOSTAUDIOPORT;

/**
 * PDM host audio port interface, upwards sibling of PDMIHOSTAUDIO.
 */
typedef struct PDMIHOSTAUDIOPORT
{
    /**
     * Ask DrvAudio to call PDMIHOSTAUDIO::pfnDoOnWorkerThread on a worker thread.
     *
     * Generic method for doing asynchronous work using the DrvAudio thread pool.
     *
     * This function will not wait for PDMIHOSTAUDIO::pfnDoOnWorkerThread to
     * complete, but returns immediately after submitting the request to the thread
     * pool.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to this interface.
     * @param   pStream     Optional backend stream structure to pass along.  The
     *                      reference count will be increased till the call
     *                      completes to make sure the stream stays valid.
     * @param   uUser       User specific value.
     * @param   pvUser      User specific pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnDoOnWorkerThread,(PPDMIHOSTAUDIOPORT pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                   uintptr_t uUser, void *pvUser));

    /**
     * The device for the given direction changed.
     *
     * The driver above backend (DrvAudio) will call the backend back
     * (PDMIHOSTAUDIO::pfnStreamNotifyDeviceChanged) for all open streams in the
     * given direction. (This ASSUMES the backend uses one output device and one
     * input devices for all streams.)
     *
     * @param   pInterface  Pointer to this interface.
     * @param   enmDir      The audio direction.
     * @param   pvUser      Backend specific parameter for
     *                      PDMIHOSTAUDIO::pfnStreamNotifyDeviceChanged.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyDeviceChanged,(PPDMIHOSTAUDIOPORT pInterface, PDMAUDIODIR enmDir, void *pvUser));

    /**
     * Notification that the stream is about to change device in a bit.
     *
     * This will assume PDMAUDIOSTREAM_STS_PREPARING_SWITCH will be set when
     * PDMIHOSTAUDIO::pfnStreamGetStatus is next called and change the stream state
     * accordingly.
     *
     * @param   pInterface  Pointer to this interface.
     * @param   pStream     The stream that changed device (backend variant).
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamNotifyPreparingDeviceSwitch,(PPDMIHOSTAUDIOPORT pInterface,
                                                                     PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * The stream has changed its device and left the
     * PDMAUDIOSTREAM_STS_PREPARING_SWITCH state (if it entered it at all).
     *
     * @param   pInterface  Pointer to this interface.
     * @param   pStream     The stream that changed device (backend variant).
     * @param   fReInit     Set if a re-init is required, clear if not.
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamNotifyDeviceChanged,(PPDMIHOSTAUDIOPORT pInterface,
                                                             PPDMAUDIOBACKENDSTREAM pStream, bool fReInit));

    /**
     * One or more audio devices have changed in some way.
     *
     * The upstream driver/device should re-evaluate the devices they're using.
     *
     * @todo r=bird: The upstream driver/device does not know which host audio
     *       devices they are using.  This is mainly for triggering enumeration and
     *       logging of the audio devices.
     *
     * @param   pInterface  Pointer to this interface.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyDevicesChanged,(PPDMIHOSTAUDIOPORT pInterface));
} PDMIHOSTAUDIOPORT;

/** PDMIHOSTAUDIOPORT interface ID. */
#define PDMIHOSTAUDIOPORT_IID                    "9f91ec59-95ba-4925-92dc-e75be1c63352"

/** @} */

#endif /* !VBOX_INCLUDED_vmm_pdmaudioifs_h */

