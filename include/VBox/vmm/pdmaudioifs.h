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

/** Device latency spec in milliseconds (ms). */
typedef uint32_t PDMAUDIODEVLATSPECMS;

/** Device latency spec in seconds (s). */
typedef uint32_t PDMAUDIODEVLATSPECSEC;

/** @name PDMAUDIOHOSTDEV_F_XXX
 * @{  */
/** No flags set. */
#define PDMAUDIOHOSTDEV_F_NONE              UINT32_C(0)
/** The device marks the default device within the host OS. */
#define PDMAUDIOHOSTDEV_F_DEFAULT           RT_BIT_32(0)
/** The device can be removed at any time and we have to deal with it. */
#define PDMAUDIOHOSTDEV_F_HOTPLUG           RT_BIT_32(1)
/** The device is known to be buggy and needs special treatment. */
#define PDMAUDIOHOSTDEV_F_BUGGY             RT_BIT_32(2)
/** Ignore the device, no matter what. */
#define PDMAUDIOHOSTDEV_F_IGNORE            RT_BIT_32(3)
/** The device is present but marked as locked by some other application. */
#define PDMAUDIOHOSTDEV_F_LOCKED            RT_BIT_32(4)
/** The device is present but not in an alive state (dead). */
#define PDMAUDIOHOSTDEV_F_DEAD              RT_BIT_32(5)
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
    /** Reference count indicating how many audio streams currently are relying on this device. */
    uint8_t             cRefCount;
    /** Maximum number of input audio channels the device supports. */
    uint8_t             cMaxInputChannels;
    /** Maximum number of output audio channels the device supports. */
    uint8_t             cMaxOutputChannels;
    uint8_t             bAlignment;
    /** Device type union, based on enmType. */
    union
    {
        /** USB type specifics. */
        struct
        {
            /** Vendor ID. */
            uint16_t    idVendor;
            /** Product ID. */
            uint16_t    idProduct;
        } USB;
        uint64_t        uPadding[ARCH_BITS >= 64 ? 3 : 4];
    } Type;
    /** Friendly name of the device, if any. Could be truncated. */
    char                szName[64];
} PDMAUDIOHOSTDEV;
AssertCompileSizeAlignment(PDMAUDIOHOSTDEV, 16);
/** Pointer to audio device info (enumeration result). */
typedef PDMAUDIOHOSTDEV *PPDMAUDIOHOSTDEV;
/** Pointer to a const audio device info (enumeration result). */
typedef PDMAUDIOHOSTDEV const *PCPDMAUDIOHOSTDEV;

/** Magic value for PDMAUDIOHOSTDEV.  */
#define PDMAUDIOHOSTDEV_MAGIC       PDM_VERSION_MAKE(0xa0d0, 1, 0)


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
    /** Size (in bytes) of the host backend's audio output stream structure. */
    size_t          cbStreamOut;
    /** Size (in bytes) of the host backend's audio input stream structure. */
    size_t          cbStreamIn;
    /** Number of concurrent output (playback) streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t        cMaxStreamsOut;
    /** Number of concurrent input (recording) streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t        cMaxStreamsIn;
} PDMAUDIOBACKENDCFG;
/** Pointer to a static host audio audio configuration. */
typedef PDMAUDIOBACKENDCFG *PPDMAUDIOBACKENDCFG;

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
 * Audio playback destinations.
 */
typedef enum PDMAUDIOPLAYBACKDST
{
    /** Invalid zero value as per usual (guards against using unintialized values). */
    PDMAUDIOPLAYBACKDST_INVALID = 0,
    /** Unknown destination. */
    PDMAUDIOPLAYBACKDST_UNKNOWN,
    /** Front channel. */
    PDMAUDIOPLAYBACKDST_FRONT,
    /** Center / LFE (Subwoofer) channel. */
    PDMAUDIOPLAYBACKDST_CENTER_LFE,
    /** Rear channel. */
    PDMAUDIOPLAYBACKDST_REAR,
    /** End of valid values. */
    PDMAUDIOPLAYBACKDST_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOPLAYBACKDST_32BIT_HACK = 0x7fffffff
} PDMAUDIOPLAYBACKDST;

/**
 * Audio recording sources.
 *
 * @note Because this is almost exclusively used in PDMAUDIODSTSRCUNION where it
 *       overlaps with PDMAUDIOPLAYBACKDST, the values starts at 64 instead of 0.
 */
typedef enum PDMAUDIORECSRC
{
    /** Unknown recording source. */
    PDMAUDIORECSRC_UNKNOWN = 64,
    /** Microphone-In. */
    PDMAUDIORECSRC_MIC,
    /** CD. */
    PDMAUDIORECSRC_CD,
    /** Video-In. */
    PDMAUDIORECSRC_VIDEO,
    /** AUX. */
    PDMAUDIORECSRC_AUX,
    /** Line-In. */
    PDMAUDIORECSRC_LINE,
    /** Phone-In. */
    PDMAUDIORECSRC_PHONE,
    /** End of valid values. */
    PDMAUDIORECSRC_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIORECSRC_32BIT_HACK = 0x7fffffff
} PDMAUDIORECSRC;

/**
 * Union for keeping an audio stream destination or source.
 */
typedef union PDMAUDIODSTSRCUNION
{
    /** Desired playback destination (for an output stream). */
    PDMAUDIOPLAYBACKDST enmDst;
    /** Desired recording source (for an input stream). */
    PDMAUDIORECSRC      enmSrc;
} PDMAUDIODSTSRCUNION;
/** Pointer to an audio stream src/dst union. */
typedef PDMAUDIODSTSRCUNION *PPDMAUDIODSTSRCUNION;

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
    /** Destination / source indicator, depending on enmDir. */
    PDMAUDIODSTSRCUNION     u;
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
    /** Unknown command, do not use. */
    PDMAUDIOSTREAMCMD_UNKNOWN,
    /** Enables the stream. */
    PDMAUDIOSTREAMCMD_ENABLE,
    /** Disables the stream.
     *  For output streams this stops the stream after playing the remaining (buffered) audio data.
     *  For input streams this will deliver the remaining (captured) audio data and not accepting
     *  any new audio input data afterwards. */
    PDMAUDIOSTREAMCMD_DISABLE,
    /** Pauses the stream. */
    PDMAUDIOSTREAMCMD_PAUSE,
    /** Resumes the stream. */
    PDMAUDIOSTREAMCMD_RESUME,
    /** Tells the stream to drain itself.
     *  For output streams this plays all remaining (buffered) audio frames,
     *  for input streams this permits receiving any new audio frames.
     *  No supported by all backends. */
    PDMAUDIOSTREAMCMD_DRAIN,
    /** Tells the stream to drop all (buffered) audio data immediately.
     *  No supported by all backends. */
    PDMAUDIOSTREAMCMD_DROP,
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
typedef PDMAUDIOVOLUME  *PPDMAUDIOVOLUME;

/** Defines the minimum volume allowed. */
#define PDMAUDIO_VOLUME_MIN     (0)
/** Defines the maximum volume allowed. */
#define PDMAUDIO_VOLUME_MAX     (255)


/** @name PDMAUDIOSTREAMSTS_FLAGS_XXX
 * @{ */
/** No flags being set. */
#define PDMAUDIOSTREAMSTS_FLAGS_NONE            UINT32_C(0)
/** Whether this stream has been initialized by the
 *  backend or not. */
#define PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED     RT_BIT_32(0)
/** Whether this stream is enabled or disabled. */
#define PDMAUDIOSTREAMSTS_FLAGS_ENABLED         RT_BIT_32(1)
/** Whether this stream has been paused or not. This also implies
 *  that this is an enabled stream! */
#define PDMAUDIOSTREAMSTS_FLAGS_PAUSED          RT_BIT_32(2)
/** Whether this stream was marked as being disabled
 *  but there are still associated guest output streams
 *  which rely on its data. */
#define PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE RT_BIT_32(3)
/** Whether this stream is in re-initialization phase.
 *  All other bits remain untouched to be able to restore
 *  the stream's state after the re-initialization bas been
 *  finished. */
#define PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT  RT_BIT_32(4)
/** Validation mask. */
#define PDMAUDIOSTREAMSTS_VALID_MASK            UINT32_C(0x0000001F)
/** Stream status flag, PDMAUDIOSTREAMSTS_FLAGS_XXX. */
typedef uint32_t PDMAUDIOSTREAMSTS;
/** @} */

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
    /** Magic value (PDMAUDIOSTREAM_MAGIC). */
    uint32_t                uMagic;
    /** Number of references to this stream.
     *  Only can be destroyed when the reference count reaches 0. */
    uint32_t volatile       cRefs;
    /** Stream status flag. */
    PDMAUDIOSTREAMSTS       fStatus;
    /** Audio direction of this stream. */
    PDMAUDIODIR             enmDir;
    /** Size (in bytes) of the backend-specific stream data. */
    uint32_t                cbBackend;
    /** Warnings shown already in the release log.
     *  See PDMAUDIOSTREAM_WARN_FLAGS_XXX. */
    uint32_t                fWarningsShown;

    /** Name of this stream. */
    char                    szName[64];
} PDMAUDIOSTREAM;
/** Pointer to an audio stream. */
typedef struct PDMAUDIOSTREAM *PPDMAUDIOSTREAM;
/** Pointer to a const audio stream. */
typedef struct PDMAUDIOSTREAM const *PCPDMAUDIOSTREAM;

/** Magic value for PDMAUDIOSTREAM. */
#define PDMAUDIOSTREAM_MAGIC    PDM_VERSION_MAKE(0xa0d3, 2, 0)



/** @todo r=bird: What is this exactly? */
#define PPDMAUDIOBACKENDSTREAM void *


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
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

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
     * Writes PCM audio data to the host (output).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream to read from.
     * @param   pvBuf           Audio data to be written.
     * @param   cbBuf           Number of bytes to be written.
     * @param   pcbWritten      Bytes of audio data written. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamWrite, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                               const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten));

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
     * Returns the status of a specific audio stream.
     *
     * @returns Audio stream status
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOSTREAMSTS, pfnStreamGetStatus, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

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
     * Plays (transfers) available audio frames to the host backend.
     *
     * Only works with output streams.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pStream              Pointer to audio stream.
     * @param   pcFramesPlayed       Number of frames played. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamPlay, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcFramesPlayed));

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
#define PDMIAUDIOCONNECTOR_IID                  "122511ca-deb3-4630-ad31-ade9f3177df4"


/** Pointer to a host audio interface. */
typedef struct PDMIHOSTAUDIO *PPDMIHOSTAUDIO;

/**
 * PDM host audio interface.
 */
typedef struct PDMIHOSTAUDIO
{
    /**
     * Initializes the host backend (driver).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @deprecated Just stub this and do the real work in the driver constructor.
     */
    DECLR3CALLBACKMEMBER(int, pfnInit, (PPDMIHOSTAUDIO pInterface));

    /**
     * Shuts down the host backend (driver).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @deprecated Just stub this and do the real work in the driver destructor.
     */
    DECLR3CALLBACKMEMBER(void, pfnShutdown, (PPDMIHOSTAUDIO pInterface));

    /**
     * Returns the host backend's configuration (backend).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pBackendCfg         Where to store the backend audio configuration to.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConfig, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg));

    /**
     * Returns (enumerates) host audio device information.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pDeviceEnum         Where to return the enumerated audio devices.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetDevices, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum));

    /**
     * Returns the current status from the audio backend.
     *
     * @returns PDMAUDIOBACKENDSTS enum.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   enmDir              Audio direction to get status for. Pass PDMAUDIODIR_DUPLEX for overall status.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOBACKENDSTS, pfnGetStatus, (PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir));

    /**
     * Creates an audio stream using the requested stream configuration.
     *
     * If a backend is not able to create this configuration, it will return its
     * best match in the acquired configuration structure on success.
     *
     * @returns VBox status code.
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
     * Destroys an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

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
     * Returns the amount which is pending (in other words has not yet been processed) by/from the backend yet.
     * Optional.
     *
     * For input streams this is read audio data by the backend which has not been processed by the host yet.
     * For output streams this is written audio data to the backend which has not been processed by the backend yet.
     *
     * @returns For non-raw layout streams: Number of pending bytes.
     *          for raw layout streams    : Number of pending audio frames.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetPending, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Returns the current status of the given backend stream.
     *
     * @returns PDMAUDIOSTREAMSTS
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOSTREAMSTS, pfnStreamGetStatus, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Gives the host backend the chance to do some (necessary) iteration work.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamIterate, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Signals the backend that the host wants to begin playing for this iteration. Optional.
     *
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(void, pfnStreamPlayBegin, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream));

    /**
     * Plays (writes to) an audio (output) stream.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface structure containing the called function pointer.
     * @param   pStream     Pointer to audio stream.
     * @param   pvBuf       Pointer to audio data buffer to play.
     * @param   cbBuf       The number of bytes of audio data to play.
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
     * @param   uBufSize    Size of the audio data buffer (see note below for unit).
     * @param   puRead      Returns number of units read.
     * @note    The @a uBufSize and @a puRead values are in bytes for non-raw
     *          layout streams and in frames for raw layout ones.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCapture, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                 void *pvBuf, uint32_t uBufSize, uint32_t *puRead));

} PDMIHOSTAUDIO;

/** PDMIHOSTAUDIO interface ID. */
#define PDMIHOSTAUDIO_IID                           "cf8dcd5f-0077-499e-9db1-1161b046fded"


/** Pointer to a audio notify from host interface. */
typedef struct PDMIAUDIONOTIFYFROMHOST *PPDMIAUDIONOTIFYFROMHOST;

/**
 * PDM audio notification interface, for use by host audio.
 *
 * @todo better name?
 */
typedef struct PDMIAUDIONOTIFYFROMHOST
{
    /**
     * One or more audio devices have changed in some way.
     *
     * The upstream driver/device should re-evaluate the devices they're using.
     *
     * @param   pInterface          Pointer to this interface.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyDevicesChanged,(PPDMIAUDIONOTIFYFROMHOST pInterface));
} PDMIAUDIONOTIFYFROMHOST;

/** PDMIAUDIONOTIFYFROMHOST interface ID. */
#define PDMIAUDIONOTIFYFROMHOST_IID                 "ec10f36b-ec2d-4b97-9044-2a59fba837ad"

/** @} */

#endif /* !VBOX_INCLUDED_vmm_pdmaudioifs_h */

