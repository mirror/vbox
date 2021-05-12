/* $Id$ */
/** @file
 * Host audio driver - Mac OS X CoreAudio.
 *
 * For relevant Apple documentation, here are some starters:
 *     - Core Audio Essentials
 *       https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/CoreAudioEssentials/CoreAudioEssentials.html
 *     - TN2097: Playing a sound file using the Default Output Audio Unit
 *       https://developer.apple.com/library/archive/technotes/tn2097/
 *     - TN2091: Device input using the HAL Output Audio Unit
 *       https://developer.apple.com/library/archive/technotes/tn2091/
 *     - Audio Component Services
 *       https://developer.apple.com/documentation/audiounit/audio_component_services?language=objc
 *     - QA1533: How to handle kAudioUnitProperty_MaximumFramesPerSlice
 *       https://developer.apple.com/library/archive/qa/qa1533/
 *     - QA1317: Signaling the end of data when using AudioConverterFillComplexBuffer
 *       https://developer.apple.com/library/archive/qa/qa1317/
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#ifdef VBOX_AUDIO_VKAT
# include "VBoxDDVKAT.h"
#else
# include "VBoxDD.h"
#endif

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>
#include <iprt/mem.h>

#include <iprt/uuid.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioToolbox.h>



/* Enables utilizing the Core Audio converter unit for converting
 * input / output from/to our requested formats. That might be more
 * performant than using our own routines later down the road. */
/** @todo Needs more investigation and testing first before enabling. */
//# define VBOX_WITH_AUDIO_CA_CONVERTER

/** @todo
 * - Maybe make sure the threads are immediately stopped if playing/recording stops.
 */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the instance data for a Core Audio driver instance.  */
typedef struct DRVHOSTCOREAUDIO *PDRVHOSTCOREAUDIO;
/** Pointer to the Core Audio specific backend data for an audio stream. */
typedef struct COREAUDIOSTREAM *PCOREAUDIOSTREAM;

/**
 * Core Audio device entry (enumeration).
 *
 * @note This is definitely not safe to just copy!
 */
typedef struct COREAUDIODEVICEDATA
{
    /** The core PDM structure. */
    PDMAUDIOHOSTDEV     Core;

    /** Pointer to driver instance this device is bound to. */
    PDRVHOSTCOREAUDIO   pDrv;
    /** The audio device ID of the currently used device (UInt32 typedef). */
    AudioDeviceID       deviceID;
    /** The device' "UUID".
     * @todo r=bird: We leak this.  Header say we must CFRelease it. */
    CFStringRef         UUID;
    /** List of attached (native) Core Audio streams attached to this device. */
    RTLISTANCHOR        lstStreams;
} COREAUDIODEVICEDATA;
/** Pointer to a Core Audio device entry (enumeration). */
typedef COREAUDIODEVICEDATA *PCOREAUDIODEVICEDATA;


/**
 * Core Audio stream state.
 */
typedef enum COREAUDIOINITSTATE
{
    /** The device is uninitialized. */
    COREAUDIOINITSTATE_UNINIT = 0,
    /** The device is currently initializing. */
    COREAUDIOINITSTATE_IN_INIT,
    /** The device is initialized. */
    COREAUDIOINITSTATE_INIT,
    /** The device is currently uninitializing. */
    COREAUDIOINITSTATE_IN_UNINIT,
    /** The usual 32-bit hack. */
    COREAUDIOINITSTATE_32BIT_HACK = 0x7fffffff
} COREAUDIOINITSTATE;


#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
/**
 * Context data for the audio format converter.
 */
typedef struct COREAUDIOCONVCBCTX
{
    /** Pointer to the stream this context is bound to. */
    PCOREAUDIOSTREAM             pStream; /**< @todo r=bird: It's part of the COREAUDIOSTREAM structure! You don't need this. */
    /** Source stream description. */
    AudioStreamBasicDescription  asbdSrc;
    /** Destination stream description. */
    AudioStreamBasicDescription  asbdDst;
    /** Pointer to native buffer list used for rendering the source audio data into. */
    AudioBufferList             *pBufLstSrc;
    /** Total packet conversion count. */
    UInt32                       uPacketCnt;
    /** Current packet conversion index. */
    UInt32                       uPacketIdx;
    /** Error count, for limiting the logging. */
    UInt32                       cErrors;
} COREAUDIOCONVCBCTX;
/** Pointer to the context of a conversion callback. */
typedef COREAUDIOCONVCBCTX *PCOREAUDIOCONVCBCTX;
#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */


/**
 * Core Audio specific data for an audio stream.
 */
typedef struct COREAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM       Core;

    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG           Cfg;
    /** Direction specific data. */
    union
    {
        struct
        {
#if 0 /* Unused */
            /** The ratio between the device & the stream sample rate. */
            Float64             sampleRatio;
#endif
#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
            /** The audio converter if necessary. NULL if no converter is being used. */
            AudioConverterRef   ConverterRef;
            /** Callback context for the audio converter. */
            COREAUDIOCONVCBCTX  convCbCtx;
#endif
        } In;
        //struct {}      Out;
    };
    /** List node for the device's stream list. */
    RTLISTNODE                  Node;
    /** The stream's thread handle for maintaining the audio queue. */
    RTTHREAD                    hThread;
    /** The runloop of the queue thread. */
    CFRunLoopRef                hRunLoop;
    /** Flag indicating to start a stream's data processing. */
    bool                        fRun;
    /** Whether the stream is in a running (active) state or not.
     *  For playback streams this means that audio data can be (or is being) played,
     *  for capturing streams this means that audio data is being captured (if available). */
    bool                        fIsRunning;
    /** Thread shutdown indicator. */
    bool volatile               fShutdown;
    /** The actual audio queue being used. */
    AudioQueueRef               hAudioQueue;
    /** The audio buffers which are used with the above audio queue. */
    AudioQueueBufferRef         apAudioBuffers[2];
    /** The acquired (final) audio format for this stream. */
    AudioStreamBasicDescription asbdStream;
    /** The audio unit for this stream. */
    struct
    {
        /** Pointer to the device this audio unit is bound to.
         *  Can be NULL if not bound to a device (anymore). */
        PCOREAUDIODEVICEDATA    pDevice;
#if 0 /* not used */
        /** The actual audio unit object. */
        AudioUnit                   hAudioUnit;
        /** Stream description for using with VBox:
         *  - When using this audio unit for input (capturing), this format states
         *    the unit's output format.
         *  - When using this audio unit for output (playback), this format states
         *    the unit's input format. */
        AudioStreamBasicDescription StreamFmt;
#endif
    } Unit;
    /** Initialization status tracker, actually COREAUDIOINITSTATE.
     * Used when some of the device parameters or the device itself is changed
     * during the runtime. */
    volatile uint32_t           enmInitState;
    /** An internal ring buffer for transferring data from/to the rendering callbacks. */
    PRTCIRCBUF                  pCircBuf;
    /** Critical section for serializing access between thread + callbacks. */
    RTCRITSECT                  CritSect;
} COREAUDIOSTREAM;


/**
 * Instance data for a Core Audio host audio driver.
 *
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTCOREAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO           IHostAudio;
    /** Critical section to serialize access. */
    RTCRITSECT              CritSect;
    /** Current (last reported) device enumeration. */
    PDMAUDIOHOSTENUM        Devices;
    /** Pointer to the currently used input device in the device enumeration.
     *  Can be NULL if none assigned. */
    PCOREAUDIODEVICEDATA    pDefaultDevIn;
    /** Pointer to the currently used output device in the device enumeration.
     *  Can be NULL if none assigned. */
    PCOREAUDIODEVICEDATA    pDefaultDevOut;
    /** Upwards notification interface. */
    PPDMIHOSTAUDIOPORT      pIHostAudioPort;
    /** Indicates whether we've registered default input device change listener. */
    bool                    fRegisteredDefaultInputListener;
    /** Indicates whether we've registered default output device change listener. */
    bool                    fRegisteredDefaultOutputListener;
} DRVHOSTCOREAUDIO;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
/** Error code which indicates "End of data" */
static const OSStatus g_rcCoreAudioConverterEOFDErr = 0x656F6664; /* 'eofd' */
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
DECLHIDDEN(int) coreAudioInputPermissionCheck(void); /* DrvHostAudioCoreAudioAuth.mm */
static int drvHostCoreAudioStreamControlInternal(PCOREAUDIOSTREAM pStreamCA, PDMAUDIOSTREAMCMD enmStreamCmd);
static DECLCALLBACK(void) coreAudioInputQueueCb(void *pvUser, AudioQueueRef hAudioQueue, AudioQueueBufferRef audioBuffer,
                                                const AudioTimeStamp *pAudioTS, UInt32 cPacketDesc,
                                                const AudioStreamPacketDescription *paPacketDesc);
static DECLCALLBACK(void) coreAudioOutputQueueCb(void *pvUser, AudioQueueRef hAudioQueue, AudioQueueBufferRef audioBuffer);




static void coreAudioPrintASBD(const char *pszDesc, const AudioStreamBasicDescription *pASBD)
{
    LogRel2(("CoreAudio: %s description:\n", pszDesc));
    LogRel2(("CoreAudio:  Format ID: %RU32 (%c%c%c%c)\n", pASBD->mFormatID,
             RT_BYTE4(pASBD->mFormatID), RT_BYTE3(pASBD->mFormatID),
             RT_BYTE2(pASBD->mFormatID), RT_BYTE1(pASBD->mFormatID)));
    LogRel2(("CoreAudio:  Flags: %RU32", pASBD->mFormatFlags));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsFloat)
        LogRel2((" Float"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsBigEndian)
        LogRel2((" BigEndian"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsSignedInteger)
        LogRel2((" SignedInteger"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsPacked)
        LogRel2((" Packed"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsAlignedHigh)
        LogRel2((" AlignedHigh"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
        LogRel2((" NonInterleaved"));
    if (pASBD->mFormatFlags & kAudioFormatFlagIsNonMixable)
        LogRel2((" NonMixable"));
    if (pASBD->mFormatFlags & kAudioFormatFlagsAreAllClear)
        LogRel2((" AllClear"));
    LogRel2(("\n"));
    LogRel2(("CoreAudio:  SampleRate      : %RU64.%02u Hz\n",
             (uint64_t)pASBD->mSampleRate, (unsigned)(pASBD->mSampleRate * 100) % 100));
    LogRel2(("CoreAudio:  ChannelsPerFrame: %RU32\n", pASBD->mChannelsPerFrame));
    LogRel2(("CoreAudio:  FramesPerPacket : %RU32\n", pASBD->mFramesPerPacket));
    LogRel2(("CoreAudio:  BitsPerChannel  : %RU32\n", pASBD->mBitsPerChannel));
    LogRel2(("CoreAudio:  BytesPerFrame   : %RU32\n", pASBD->mBytesPerFrame));
    LogRel2(("CoreAudio:  BytesPerPacket  : %RU32\n", pASBD->mBytesPerPacket));
}


static void coreAudioPCMPropsToASBD(PCPDMAUDIOPCMPROPS pProps, AudioStreamBasicDescription *pASBD)
{
    AssertPtrReturnVoid(pProps);
    AssertPtrReturnVoid(pASBD);

    RT_BZERO(pASBD, sizeof(AudioStreamBasicDescription));

    pASBD->mFormatID         = kAudioFormatLinearPCM;
    pASBD->mFormatFlags      = kAudioFormatFlagIsPacked;
    if (pProps->fSigned)
        pASBD->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    if (PDMAudioPropsIsBigEndian(pProps))
        pASBD->mFormatFlags |= kAudioFormatFlagIsBigEndian;
    pASBD->mSampleRate       = PDMAudioPropsHz(pProps);
    pASBD->mChannelsPerFrame = PDMAudioPropsChannels(pProps);
    pASBD->mBitsPerChannel   = PDMAudioPropsSampleBits(pProps);
    pASBD->mBytesPerFrame    = PDMAudioPropsFrameSize(pProps);
    pASBD->mFramesPerPacket  = 1; /* For uncompressed audio, set this to 1. */
    pASBD->mBytesPerPacket   = PDMAudioPropsFrameSize(pProps) * pASBD->mFramesPerPacket;
}


#if 0 /* unused */
static int coreAudioCFStringToCString(const CFStringRef pCFString, char **ppszString)
{
    CFIndex cLen = CFStringGetLength(pCFString) + 1;
    char *pszResult = (char *)RTMemAllocZ(cLen * sizeof(char));
    if (!CFStringGetCString(pCFString, pszResult, cLen, kCFStringEncodingUTF8))
    {
        RTMemFree(pszResult);
        return VERR_NOT_FOUND;
    }

    *ppszString = pszResult;
    return VINF_SUCCESS;
}

static AudioDeviceID coreAudioDeviceUIDtoID(const char* pszUID)
{
    /* Create a CFString out of our CString. */
    CFStringRef strUID = CFStringCreateWithCString(NULL, pszUID, kCFStringEncodingMacRoman);

    /* Fill the translation structure. */
    AudioDeviceID deviceID;

    AudioValueTranslation translation;
    translation.mInputData      = &strUID;
    translation.mInputDataSize  = sizeof(CFStringRef);
    translation.mOutputData     = &deviceID;
    translation.mOutputDataSize = sizeof(AudioDeviceID);

    /* Fetch the translation from the UID to the device ID. */
    AudioObjectPropertyAddress PropAddr =
    {
        kAudioHardwarePropertyDeviceForUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    UInt32 uSize = sizeof(AudioValueTranslation);
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &PropAddr, 0, NULL, &uSize, &translation);

    /* Release the temporary CFString */
    CFRelease(strUID);

    if (RT_LIKELY(err == noErr))
        return deviceID;

    /* Return the unknown device on error. */
    return kAudioDeviceUnknown;
}
#endif /* unused */


#ifdef VBOX_WITH_AUDIO_CA_CONVERTER

/**
 * Initializes a conversion callback context.
 *
 * @return  IPRT status code.
 * @param   pConvCbCtx          Conversion callback context to initialize.
 * @param   pStream             Pointer to stream to use.
 * @param   pASBDSrc            Input (source) stream description to use.
 * @param   pASBDDst            Output (destination) stream description to use.
 */
static int coreAudioInitConvCbCtx(PCOREAUDIOCONVCBCTX pConvCbCtx, PCOREAUDIOSTREAM pStream,
                                  AudioStreamBasicDescription *pASBDSrc, AudioStreamBasicDescription *pASBDDst)
{
    AssertPtrReturn(pConvCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pASBDSrc,   VERR_INVALID_POINTER);
    AssertPtrReturn(pASBDDst,   VERR_INVALID_POINTER);

# ifdef DEBUG
    coreAudioPrintASBD("CbCtx: Src", pASBDSrc);
    coreAudioPrintASBD("CbCtx: Dst", pASBDDst);
# endif

    pConvCbCtx->pStream = pStream;

    memcpy(&pConvCbCtx->asbdSrc, pASBDSrc, sizeof(AudioStreamBasicDescription));
    memcpy(&pConvCbCtx->asbdDst, pASBDDst, sizeof(AudioStreamBasicDescription));

    pConvCbCtx->pBufLstSrc = NULL;
    pConvCbCtx->cErrors    = 0;

    return VINF_SUCCESS;
}


/**
 * Uninitializes a conversion callback context.
 *
 * @return  IPRT status code.
 * @param   pConvCbCtx          Conversion callback context to uninitialize.
 */
static void coreAudioUninitConvCbCtx(PCOREAUDIOCONVCBCTX pConvCbCtx)
{
    AssertPtrReturnVoid(pConvCbCtx);

    pConvCbCtx->pStream = NULL;

    RT_ZERO(pConvCbCtx->asbdSrc);
    RT_ZERO(pConvCbCtx->asbdDst);

    pConvCbCtx->pBufLstSrc = NULL;
    pConvCbCtx->cErrors    = 0;
}

#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */


/**
 * Propagates an audio device status to all its connected Core Audio streams.
 *
 * @return IPRT status code.
 * @param  pDev                 Audio device to propagate status for.
 * @param  enmSts               Status to propagate.
 */
static int coreAudioDevicePropagateStatus(PCOREAUDIODEVICEDATA pDev, COREAUDIOINITSTATE enmSts)
{
    AssertPtrReturn(pDev, VERR_INVALID_POINTER);


    /* Sanity. */
    AssertPtr(pDev->pDrv);

    LogFlowFunc(("pDev=%p enmSts=%RU32\n", pDev, enmSts));

    PCOREAUDIOSTREAM pStreamCA;
    RTListForEach(&pDev->lstStreams, pStreamCA, COREAUDIOSTREAM, Node)
    {
        LogFlowFunc(("pStreamCA=%p\n", pStreamCA));

        /* We move the reinitialization to the next output event.
         * This make sure this thread isn't blocked and the
         * reinitialization is done when necessary only. */
        ASMAtomicWriteU32(&pStreamCA->enmInitState, enmSts);
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(OSStatus) coreAudioDeviceStateChangedCb(AudioObjectID propertyID,
                                                            UInt32 nAddresses,
                                                            const AudioObjectPropertyAddress properties[],
                                                            void *pvUser)
{
    RT_NOREF(propertyID, nAddresses, properties);

    LogFlowFunc(("propertyID=%u, nAddresses=%u, pvUser=%p\n", propertyID, nAddresses, pvUser));

    PCOREAUDIODEVICEDATA pDev = (PCOREAUDIODEVICEDATA)pvUser;
    AssertPtr(pDev);

    PDRVHOSTCOREAUDIO pThis = pDev->pDrv;
    AssertPtr(pThis);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    UInt32 uAlive = 1;
    UInt32 uSize  = sizeof(UInt32);

    AudioObjectPropertyAddress PropAddr =
    {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    AudioDeviceID deviceID = pDev->deviceID;

    OSStatus err = AudioObjectGetPropertyData(deviceID, &PropAddr, 0, NULL, &uSize, &uAlive);

    bool fIsDead = false;

    if (err == kAudioHardwareBadDeviceError)
        fIsDead = true; /* Unplugged. */
    else if ((err == kAudioHardwareNoError) && (!RT_BOOL(uAlive)))
        fIsDead = true; /* Something else happened. */

    if (fIsDead)
    {
        LogRel2(("CoreAudio: Device '%s' stopped functioning\n", pDev->Core.szName));

        /* Mark device as dead. */
        rc2 = coreAudioDevicePropagateStatus(pDev, COREAUDIOINITSTATE_UNINIT);
        AssertRC(rc2);
    }

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    return noErr;
}

/* Callback for getting notified when the default recording/playback device has been changed. */
/** @todo r=bird: Why DECLCALLBACK? */
static DECLCALLBACK(OSStatus) coreAudioDefaultDeviceChangedCb(AudioObjectID propertyID,
                                                              UInt32 nAddresses,
                                                              const AudioObjectPropertyAddress properties[],
                                                              void *pvUser)
{
    RT_NOREF(propertyID, nAddresses);

    LogFlowFunc(("propertyID=%u, nAddresses=%u, pvUser=%p\n", propertyID, nAddresses, pvUser));

    PDRVHOSTCOREAUDIO pThis = (PDRVHOSTCOREAUDIO)pvUser;
    AssertPtr(pThis);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    for (UInt32 idxAddress = 0; idxAddress < nAddresses; idxAddress++)
    {
        PCOREAUDIODEVICEDATA pDev = NULL;

        /*
         * Check if the default input / output device has been changed.
         */
        const AudioObjectPropertyAddress *pProperty = &properties[idxAddress];
        switch (pProperty->mSelector)
        {
            case kAudioHardwarePropertyDefaultInputDevice:
                LogFlowFunc(("kAudioHardwarePropertyDefaultInputDevice\n"));
                pDev = pThis->pDefaultDevIn;
                break;

            case kAudioHardwarePropertyDefaultOutputDevice:
                LogFlowFunc(("kAudioHardwarePropertyDefaultOutputDevice\n"));
                pDev = pThis->pDefaultDevOut;
                break;

            default:
                /* Skip others. */
                break;
        }

        LogFlowFunc(("pDev=%p\n", pDev));
    }

    /* Make sure to leave the critical section before notify higher drivers/devices. */
    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    /* Notify the driver/device above us about possible changes in devices. */
    if (pThis->pIHostAudioPort)
        pThis->pIHostAudioPort->pfnNotifyDevicesChanged(pThis->pIHostAudioPort);

    return noErr;
}


#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
/* Callback to convert audio input data from one format to another. */
static DECLCALLBACK(OSStatus) coreAudioConverterCb(AudioConverterRef              inAudioConverter,
                                                   UInt32                        *ioNumberDataPackets,
                                                   AudioBufferList               *ioData,
                                                   AudioStreamPacketDescription **ppASPD,
                                                   void                          *pvUser)
{
    RT_NOREF(inAudioConverter);

    AssertPtrReturn(ioNumberDataPackets, g_rcCoreAudioConverterEOFDErr);
    AssertPtrReturn(ioData,              g_rcCoreAudioConverterEOFDErr);

    PCOREAUDIOCONVCBCTX pConvCbCtx = (PCOREAUDIOCONVCBCTX)pvUser;
    AssertPtr(pConvCbCtx);

    /* Initialize values. */
    ioData->mBuffers[0].mNumberChannels = 0;
    ioData->mBuffers[0].mDataByteSize   = 0;
    ioData->mBuffers[0].mData           = NULL;

    if (ppASPD)
    {
        Log3Func(("Handling packet description not implemented\n"));
    }
    else
    {
        /** @todo Check converter ID? */

        /** @todo Handled non-interleaved data by going through the full buffer list,
         *        not only through the first buffer like we do now. */
        Log3Func(("ioNumberDataPackets=%RU32\n", *ioNumberDataPackets));

        UInt32 cNumberDataPackets = *ioNumberDataPackets;
        Assert(pConvCbCtx->uPacketIdx + cNumberDataPackets <= pConvCbCtx->uPacketCnt);

        if (cNumberDataPackets)
        {
            AssertPtr(pConvCbCtx->pBufLstSrc);
            Assert(pConvCbCtx->pBufLstSrc->mNumberBuffers == 1); /* Only one buffer for the source supported atm. */

            AudioStreamBasicDescription *pSrcASBD = &pConvCbCtx->asbdSrc;
            AudioBuffer                 *pSrcBuf  = &pConvCbCtx->pBufLstSrc->mBuffers[0];

            size_t cbOff   = pConvCbCtx->uPacketIdx * pSrcASBD->mBytesPerPacket;

            cNumberDataPackets = RT_MIN((pSrcBuf->mDataByteSize - cbOff) / pSrcASBD->mBytesPerPacket,
                                        cNumberDataPackets);

            void  *pvAvail = (uint8_t *)pSrcBuf->mData + cbOff;
            size_t cbAvail = RT_MIN(pSrcBuf->mDataByteSize - cbOff, cNumberDataPackets * pSrcASBD->mBytesPerPacket);

            Log3Func(("cNumberDataPackets=%RU32, cbOff=%zu, cbAvail=%zu\n", cNumberDataPackets, cbOff, cbAvail));

            /* Set input data for the converter to use.
             * Note: For VBR (Variable Bit Rates) or interleaved data handling we need multiple buffers here. */
            ioData->mNumberBuffers = 1;

            ioData->mBuffers[0].mNumberChannels = pSrcBuf->mNumberChannels;
            ioData->mBuffers[0].mDataByteSize   = cbAvail;
            ioData->mBuffers[0].mData           = pvAvail;

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            int rc = RTFileOpen(&fh,VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "caConverterCbInput.pcm",
                                RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                RTFileWrite(fh, pvAvail, cbAvail, NULL);
                RTFileClose(fh);
            }
            else
                AssertFailed();
#endif
            pConvCbCtx->uPacketIdx += cNumberDataPackets;
            Assert(pConvCbCtx->uPacketIdx <= pConvCbCtx->uPacketCnt);

            *ioNumberDataPackets = cNumberDataPackets;
        }
    }

    Log3Func(("%RU32 / %RU32 -> ioNumberDataPackets=%RU32\n",
              pConvCbCtx->uPacketIdx, pConvCbCtx->uPacketCnt, *ioNumberDataPackets));

    return noErr;
}
#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */


/**
 * @callback_method_impl{FNRTTHREAD,
 * Thread for a Core Audio stream's audio queue handling.}
 *
 * This thread is required per audio queue to pump data to/from the Core Audio
 * stream and handling its callbacks.
 */
static DECLCALLBACK(int) coreAudioQueueThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PCOREAUDIOSTREAM           pStreamCA = (PCOREAUDIOSTREAM)pvUser;
    AssertPtr(pStreamCA);
    const bool                 fIn       = pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN;
    PCOREAUDIODEVICEDATA const pDev      = (PCOREAUDIODEVICEDATA)pStreamCA->Unit.pDevice;
    CFRunLoopRef const         hRunLoop  = CFRunLoopGetCurrent();
    AssertPtr(pDev);

    LogFunc(("Thread started for pStreamCA=%p fIn=%RTbool\n", pStreamCA, fIn));

    /*
     * Create audio queue.
     */
    int rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    OSStatus orc;
    if (fIn)
        orc = AudioQueueNewInput(&pStreamCA->asbdStream, coreAudioInputQueueCb, pStreamCA /* pvData */,
                                 CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0, &pStreamCA->hAudioQueue);
    else
        orc = AudioQueueNewOutput(&pStreamCA->asbdStream, coreAudioOutputQueueCb, pStreamCA /* pvData */,
                                  CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0, &pStreamCA->hAudioQueue);
    if (orc == noErr)
    {
        /*
         * Assign device to the queue.
         */
        UInt32 uSize = sizeof(pDev->UUID);
        orc = AudioQueueSetProperty(pStreamCA->hAudioQueue, kAudioQueueProperty_CurrentDevice, &pDev->UUID, uSize);
        if (orc == noErr)
        {
            /*
             * Allocate audio buffers.
             */
            const size_t cbBuf = PDMAudioPropsFramesToBytes(&pStreamCA->Cfg.Props, pStreamCA->Cfg.Backend.cFramesPeriod);
            size_t iBuf;
            for (iBuf = 0; orc == noErr && iBuf < RT_ELEMENTS(pStreamCA->apAudioBuffers); iBuf++)
                orc = AudioQueueAllocateBuffer(pStreamCA->hAudioQueue, cbBuf, &pStreamCA->apAudioBuffers[iBuf]);
            if (orc == noErr)
            {
                /*
                 * Get a reference to our runloop so it can be stopped then signal
                 * our creator to say that we're done.  The runloop reference is the
                 * success indicator.
                 */
                pStreamCA->hRunLoop = hRunLoop;
                CFRetain(hRunLoop);
                RTThreadUserSignal(hThreadSelf);

                /*
                 * The main loop.
                 */
                while (!ASMAtomicReadBool(&pStreamCA->fShutdown))
                {
                    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 30.0 /*sec*/, 1);
                }

                AudioQueueStop(pStreamCA->hAudioQueue, fIn ? 1 : 0);
                rc = VINF_SUCCESS;
            }
            else
                LogRel(("CoreAudio: Failed to allocate %#x byte queue buffer #%u: %#x (%d)\n", cbBuf, iBuf, orc, orc));

            while (iBuf-- > 0)
            {
                AudioQueueFreeBuffer(pStreamCA->hAudioQueue, pStreamCA->apAudioBuffers[iBuf]);
                pStreamCA->apAudioBuffers[iBuf] = NULL;
            }
        }
        else
            LogRel(("CoreAudio: Failed to associate device with queue: %#x (%d)\n", orc, orc));

        AudioQueueDispose(pStreamCA->hAudioQueue, 1);
    }
    else
        LogRel(("CoreAudio: Failed to create audio queue: %#x (%d)\n", orc, orc));


    RTThreadUserSignal(hThreadSelf);
    LogFunc(("Thread ended for pStreamCA=%p fIn=%RTbool: rc=%Rrc (orc=%#x/%d)\n", pStreamCA, fIn, rc, orc, orc));
    return rc;
}

/**
 * Processes input data of an audio queue buffer and stores it into a Core Audio stream.
 *
 * @returns IPRT status code.
 * @param   pStreamCA           Core Audio stream to store input data into.
 * @param   audioBuffer         Audio buffer to process input data from.
 */
static int coreAudioInputQueueProcBuffer(PCOREAUDIOSTREAM pStreamCA, AudioQueueBufferRef audioBuffer)
{
    PRTCIRCBUF pCircBuf = pStreamCA->pCircBuf;
    AssertPtr(pCircBuf);

    UInt8 *pvSrc = (UInt8 *)audioBuffer->mAudioData;
    UInt8 *pvDst = NULL;

    size_t cbWritten = 0;

    size_t cbToWrite = audioBuffer->mAudioDataByteSize;
    size_t cbLeft    = RT_MIN(cbToWrite, RTCircBufFree(pCircBuf));

    while (cbLeft)
    {
        /* Try to acquire the necessary block from the ring buffer. */
        RTCircBufAcquireWriteBlock(pCircBuf, cbLeft, (void **)&pvDst, &cbToWrite);

        if (!cbToWrite)
            break;

        /* Copy the data from our ring buffer to the core audio buffer. */
        /** @todo r=bird: WTF is the (UInt8 *) cast for? Despite the 'pv' prefix, pvDst
         * is a UInt8 pointer.  Whoever wrote this crap needs to check his/her
         * medication.  I shouldn't have to wast my time fixing crap like this! */
        memcpy((UInt8 *)pvDst, pvSrc + cbWritten, cbToWrite);

        /* Release the read buffer, so it could be used for new data. */
        RTCircBufReleaseWriteBlock(pCircBuf, cbToWrite);

        cbWritten += cbToWrite;

        Assert(cbLeft >= cbToWrite);
        cbLeft -= cbToWrite;
    }

    Log3Func(("pStreamCA=%p, cbBuffer=%RU32/%zu, cbWritten=%zu\n",
              pStreamCA, audioBuffer->mAudioDataByteSize, audioBuffer->mAudioDataBytesCapacity, cbWritten));

    return VINF_SUCCESS;
}

/**
 * Input audio queue callback. Called whenever input data from the audio queue becomes available.
 *
 * @param   pvUser              User argument.
 * @param   hAudioQueue         Audio queue to process input data from.
 * @param   audioBuffer         Audio buffer to process input data from. Must be part of audio queue.
 * @param   pAudioTS            Audio timestamp.
 * @param   cPacketDesc         Number of packet descriptors.
 * @param   paPacketDesc        Array of packet descriptors.
 */
static DECLCALLBACK(void) coreAudioInputQueueCb(void *pvUser, AudioQueueRef hAudioQueue, AudioQueueBufferRef audioBuffer,
                                                const AudioTimeStamp *pAudioTS,
                                                UInt32 cPacketDesc, const AudioStreamPacketDescription *paPacketDesc)
{
    RT_NOREF(pAudioTS, cPacketDesc, paPacketDesc);

    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pvUser;
    AssertPtr(pStreamCA);

    int rc = RTCritSectEnter(&pStreamCA->CritSect);
    AssertRC(rc);

    rc = coreAudioInputQueueProcBuffer(pStreamCA, audioBuffer);
    if (RT_SUCCESS(rc))
        AudioQueueEnqueueBuffer(hAudioQueue, audioBuffer, 0, NULL);

    rc = RTCritSectLeave(&pStreamCA->CritSect);
    AssertRC(rc);
}

/**
 * Processes output data of a Core Audio stream into an audio queue buffer.
 *
 * @returns IPRT status code.
 * @param   pStreamCA           Core Audio stream to process output data for.
 * @param   audioBuffer         Audio buffer to store data into.
 */
int coreAudioOutputQueueProcBuffer(PCOREAUDIOSTREAM pStreamCA, AudioQueueBufferRef audioBuffer)
{
    AssertPtr(pStreamCA);

    PRTCIRCBUF pCircBuf = pStreamCA->pCircBuf;
    AssertPtr(pCircBuf);

    size_t cbRead = 0;

    UInt8 *pvSrc = NULL;
    UInt8 *pvDst = (UInt8 *)audioBuffer->mAudioData;

    size_t cbToRead = RT_MIN(RTCircBufUsed(pCircBuf), audioBuffer->mAudioDataBytesCapacity);
    size_t cbLeft   = cbToRead;

    while (cbLeft)
    {
        /* Try to acquire the necessary block from the ring buffer. */
        RTCircBufAcquireReadBlock(pCircBuf, cbLeft, (void **)&pvSrc, &cbToRead);

        if (cbToRead)
        {
            /* Copy the data from our ring buffer to the core audio buffer. */
            memcpy((UInt8 *)pvDst + cbRead, pvSrc, cbToRead);
        }

        /* Release the read buffer, so it could be used for new data. */
        RTCircBufReleaseReadBlock(pCircBuf, cbToRead);

        if (!cbToRead)
            break;

        /* Move offset. */
        cbRead += cbToRead;
        Assert(cbRead <= audioBuffer->mAudioDataBytesCapacity);

        Assert(cbToRead <= cbLeft);
        cbLeft -= cbToRead;
    }

    audioBuffer->mAudioDataByteSize = cbRead;

    if (audioBuffer->mAudioDataByteSize < audioBuffer->mAudioDataBytesCapacity)
    {
        RT_BZERO((UInt8 *)audioBuffer->mAudioData + audioBuffer->mAudioDataByteSize,
                 audioBuffer->mAudioDataBytesCapacity - audioBuffer->mAudioDataByteSize);

        audioBuffer->mAudioDataByteSize = audioBuffer->mAudioDataBytesCapacity;
    }

    Log3Func(("pStreamCA=%p, cbCapacity=%RU32, cbRead=%zu\n",
              pStreamCA, audioBuffer->mAudioDataBytesCapacity, cbRead));

    return VINF_SUCCESS;
}

/**
 * Output audio queue callback. Called whenever an audio queue is ready to process more output data.
 *
 * @param   pvUser              User argument.
 * @param   hAudioQueue         Audio queue to process output data for.
 * @param   audioBuffer         Audio buffer to store output data in. Must be part of audio queue.
 */
static DECLCALLBACK(void) coreAudioOutputQueueCb(void *pvUser, AudioQueueRef hAudioQueue, AudioQueueBufferRef audioBuffer)
{
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pvUser;
    AssertPtr(pStreamCA);

    int rc = RTCritSectEnter(&pStreamCA->CritSect);
    AssertRC(rc);

    rc = coreAudioOutputQueueProcBuffer(pStreamCA, audioBuffer);
    if (RT_SUCCESS(rc))
        AudioQueueEnqueueBuffer(hAudioQueue, audioBuffer, 0, NULL);

    rc = RTCritSectLeave(&pStreamCA->CritSect);
    AssertRC(rc);
}

/**
 * Invalidates a Core Audio stream's audio queue.
 *
 * @returns IPRT status code.
 * @param   pStreamCA           Core Audio stream to invalidate its queue for.
 *
 * @todo r=bird: Which use of the word 'invalidate' is this?
 */
static int coreAudioStreamInvalidateQueue(PCOREAUDIOSTREAM pStreamCA)
{
    int rc = VINF_SUCCESS;

    Log3Func(("pStreamCA=%p\n", pStreamCA));

    for (size_t i = 0; i < RT_ELEMENTS(pStreamCA->apAudioBuffers); i++)
    {
        AudioQueueBufferRef pBuf = pStreamCA->apAudioBuffers[i];
        if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN)
        {
            int rc2 = coreAudioInputQueueProcBuffer(pStreamCA, pBuf);
            if (RT_SUCCESS(rc2))
                AudioQueueEnqueueBuffer(pStreamCA->hAudioQueue, pBuf, 0 /*inNumPacketDescs*/, NULL /*inPacketDescs*/);
        }
        else
        {
            Assert(pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT);
            int rc2 = coreAudioOutputQueueProcBuffer(pStreamCA, pBuf);
            if (   RT_SUCCESS(rc2)
                && pBuf->mAudioDataByteSize)
                AudioQueueEnqueueBuffer(pStreamCA->hAudioQueue, pBuf, 0 /*inNumPacketDescs*/, NULL /*inPacketDescs*/);

            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


/* Callback for getting notified when some of the properties of an audio device have changed. */
static DECLCALLBACK(OSStatus) coreAudioDevPropChgCb(AudioObjectID                     propertyID,
                                                    UInt32                            cAddresses,
                                                    const AudioObjectPropertyAddress  properties[],
                                                    void                             *pvUser)
{
    RT_NOREF(cAddresses, properties, pvUser);

    PCOREAUDIODEVICEDATA pDev = (PCOREAUDIODEVICEDATA)pvUser;
    AssertPtr(pDev);

    LogFlowFunc(("propertyID=%u, nAddresses=%u, pDev=%p\n", propertyID, cAddresses, pDev));

    switch (propertyID)
    {
#ifdef DEBUG
       case kAudioDeviceProcessorOverload:
        {
            LogFunc(("Processor overload detected!\n"));
            break;
        }
#endif /* DEBUG */
        case kAudioDevicePropertyNominalSampleRate:
        {
            RT_NOREF(pDev);
            break;
        }

        default:
            /* Just skip. */
            break;
    }

    return noErr;
}


/*********************************************************************************************************************************
*   PDMIHOSTAUDIO                                                                                                                *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    PDRVHOSTCOREAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "Core Audio");
    pBackendCfg->cbStream       = sizeof(COREAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    /* For Core Audio we provide one stream per device for now. */
    pBackendCfg->cMaxStreamsIn  = PDMAudioHostEnumCountMatching(&pThis->Devices, PDMAUDIODIR_IN);
    pBackendCfg->cMaxStreamsOut = PDMAudioHostEnumCountMatching(&pThis->Devices, PDMAUDIODIR_OUT);

    LogFlowFunc(("Returning %Rrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * Initializes a Core Audio-specific device data structure.
 *
 * @returns IPRT status code.
 * @param   pDevData            Device data structure to initialize.
 * @param   deviceID            Core Audio device ID to assign this structure to.
 * @param   fIsInput            Whether this is an input device or not.
 * @param   pDrv                Driver instance to use.
 */
static void coreAudioDeviceDataInit(PCOREAUDIODEVICEDATA pDevData, AudioDeviceID deviceID, bool fIsInput, PDRVHOSTCOREAUDIO pDrv)
{
    AssertPtrReturnVoid(pDevData);
    AssertPtrReturnVoid(pDrv);

    pDevData->deviceID = deviceID;
    pDevData->pDrv     = pDrv;

    /* Get the device UUID. */
    AudioObjectPropertyAddress PropAddrDevUUID =
    {
        kAudioDevicePropertyDeviceUID,
        fIsInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    UInt32 uSize = sizeof(pDevData->UUID);
    OSStatus err = AudioObjectGetPropertyData(pDevData->deviceID, &PropAddrDevUUID, 0, NULL, &uSize, &pDevData->UUID);
    if (err != noErr)
        LogRel(("CoreAudio: Failed to retrieve device UUID for device %RU32 (%RI32)\n", deviceID, err));

    RTListInit(&pDevData->lstStreams);
}


/**
 * Does a (re-)enumeration of the host's playback + recording devices.
 *
 * @todo No, it doesn't do playback & recording, it does only what @a enmUsage
 *       says.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   enmUsage            Which devices to enumerate.
 * @param   pDevEnm             Where to store the enumerated devices.
 */
static int coreAudioDevicesEnumerate(PDRVHOSTCOREAUDIO pThis, PDMAUDIODIR enmUsage, PPDMAUDIOHOSTENUM pDevEnm)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pDevEnm, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    do /* (this is not a loop, just a device for avoid gotos while trying not to shoot oneself in the foot too badly.) */
    {
        AudioDeviceID defaultDeviceID = kAudioDeviceUnknown;

        /* Fetch the default audio device currently in use. */
        AudioObjectPropertyAddress PropAddrDefaultDev =
        {
            enmUsage == PDMAUDIODIR_IN ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        UInt32 uSize = sizeof(defaultDeviceID);
        OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &PropAddrDefaultDev, 0, NULL, &uSize, &defaultDeviceID);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Unable to determine default %s device (%RI32)\n",
                    enmUsage == PDMAUDIODIR_IN ? "capturing" : "playback", err));
            return VERR_NOT_FOUND;
        }

        if (defaultDeviceID == kAudioDeviceUnknown)
        {
            LogFunc(("No default %s device found\n", enmUsage == PDMAUDIODIR_IN ? "capturing" : "playback"));
            /* Keep going. */
        }

        AudioObjectPropertyAddress PropAddrDevList =
        {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &PropAddrDevList, 0, NULL, &uSize);
        if (err != kAudioHardwareNoError)
            break;

        AudioDeviceID *pDevIDs = (AudioDeviceID *)alloca(uSize);
        if (pDevIDs == NULL)
            break;

        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &PropAddrDevList, 0, NULL, &uSize, pDevIDs);
        if (err != kAudioHardwareNoError)
            break;

        PDMAudioHostEnumInit(pDevEnm);

        UInt16 cDevices = uSize / sizeof(AudioDeviceID);

        PCOREAUDIODEVICEDATA pDev = NULL;
        for (UInt16 i = 0; i < cDevices; i++)
        {
            if (pDev) /* Some (skipped) device to clean up first? */
                PDMAudioHostDevFree(&pDev->Core);

            pDev = (PCOREAUDIODEVICEDATA)PDMAudioHostDevAlloc(sizeof(*pDev));
            if (!pDev)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            /* Set usage. */
            pDev->Core.enmUsage = enmUsage;

            /* Init backend-specific device data. */
            coreAudioDeviceDataInit(pDev, pDevIDs[i], enmUsage == PDMAUDIODIR_IN, pThis);

            /* Check if the device is valid. */
            AudioDeviceID curDevID = pDev->deviceID;

            /* Is the device the default device? */
            if (curDevID == defaultDeviceID)
                pDev->Core.fFlags |= PDMAUDIOHOSTDEV_F_DEFAULT;

            AudioObjectPropertyAddress PropAddrCfg =
            {
                kAudioDevicePropertyStreamConfiguration,
                enmUsage == PDMAUDIODIR_IN ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMaster
            };
            err = AudioObjectGetPropertyDataSize(curDevID, &PropAddrCfg, 0, NULL, &uSize);
            if (err != noErr)
                continue;

            AudioBufferList *pBufList = (AudioBufferList *)RTMemAlloc(uSize);
            if (!pBufList)
                continue;

            err = AudioObjectGetPropertyData(curDevID, &PropAddrCfg, 0, NULL, &uSize, pBufList);
            if (err == noErr)
            {
                for (UInt32 a = 0; a < pBufList->mNumberBuffers; a++)
                {
                    if (enmUsage == PDMAUDIODIR_IN)
                        pDev->Core.cMaxInputChannels  += pBufList->mBuffers[a].mNumberChannels;
                    else if (enmUsage == PDMAUDIODIR_OUT)
                        pDev->Core.cMaxOutputChannels += pBufList->mBuffers[a].mNumberChannels;
                }
            }

            RTMemFree(pBufList);
            pBufList = NULL;

            /* Check if the device is valid, e.g. has any input/output channels according to its usage. */
            if (   enmUsage == PDMAUDIODIR_IN
                && !pDev->Core.cMaxInputChannels)
                continue;
            if (   enmUsage == PDMAUDIODIR_OUT
                && !pDev->Core.cMaxOutputChannels)
                continue;

            /* Resolve the device's name. */
            AudioObjectPropertyAddress PropAddrName =
            {
                kAudioObjectPropertyName,
                enmUsage == PDMAUDIODIR_IN ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMaster
            };
            uSize = sizeof(CFStringRef);
            CFStringRef pcfstrName = NULL;

            err = AudioObjectGetPropertyData(curDevID, &PropAddrName, 0, NULL, &uSize, &pcfstrName);
            if (err != kAudioHardwareNoError)
                continue;

            CFIndex cbName = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pcfstrName), kCFStringEncodingUTF8) + 1;
            if (cbName)
            {
                char *pszName = (char *)RTStrAlloc(cbName);
                if (   pszName
                    && CFStringGetCString(pcfstrName, pszName, cbName, kCFStringEncodingUTF8))
                    RTStrCopy(pDev->Core.szName, sizeof(pDev->Core.szName), pszName);

                LogFunc(("Device '%s': %RU32\n", pszName, curDevID));

                if (pszName)
                {
                    RTStrFree(pszName);
                    pszName = NULL;
                }
            }

            CFRelease(pcfstrName);

            /* Check if the device is alive for the intended usage. */
            AudioObjectPropertyAddress PropAddrAlive =
            {
                kAudioDevicePropertyDeviceIsAlive,
                enmUsage == PDMAUDIODIR_IN ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMaster
            };

            UInt32 uAlive = 0;
            uSize = sizeof(uAlive);

            err = AudioObjectGetPropertyData(curDevID, &PropAddrAlive, 0, NULL, &uSize, &uAlive);
            if (   (err == noErr)
                && !uAlive)
            {
                pDev->Core.fFlags |= PDMAUDIOHOSTDEV_F_DEAD;
            }

            /* Check if the device is being hogged by someone else. */
            AudioObjectPropertyAddress PropAddrHogged =
            {
                kAudioDevicePropertyHogMode,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMaster
            };

            pid_t pid = 0;
            uSize = sizeof(pid);

            err = AudioObjectGetPropertyData(curDevID, &PropAddrHogged, 0, NULL, &uSize, &pid);
            if (   (err == noErr)
                && (pid != -1))
            {
                pDev->Core.fFlags |= PDMAUDIOHOSTDEV_F_LOCKED;
            }

            /* Add the device to the enumeration. */
            PDMAudioHostEnumAppend(pDevEnm, &pDev->Core);

            /* NULL device pointer because it's now part of the device enumeration. */
            pDev = NULL;
        }

        if (RT_FAILURE(rc))
        {
            PDMAudioHostDevFree(&pDev->Core);
            pDev = NULL;
        }

    } while (0);

    if (RT_SUCCESS(rc))
    {
#ifdef LOG_ENABLED
        LogFunc(("Devices for pDevEnm=%p, enmUsage=%RU32:\n", pDevEnm, enmUsage));
        PDMAudioHostEnumLog(pDevEnm, "Core Audio");
#endif
    }
    else
        PDMAudioHostEnumDelete(pDevEnm);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Checks if an audio device with a specific device ID is in the given device
 * enumeration or not.
 *
 * @retval  true if the node is the last element in the list.
 * @retval  false otherwise.
 *
 * @param   pEnmSrc               Device enumeration to search device ID in.
 * @param   deviceID              Device ID to search.
 */
bool coreAudioDevicesHasDevice(PPDMAUDIOHOSTENUM pEnmSrc, AudioDeviceID deviceID)
{
    PCOREAUDIODEVICEDATA pDevSrc;
    RTListForEach(&pEnmSrc->LstDevices, pDevSrc, COREAUDIODEVICEDATA, Core.ListEntry)
    {
        if (pDevSrc->deviceID == deviceID)
            return true;
    }

    return false;
}


/**
 * Enumerates all host devices and builds a final device enumeration list, consisting
 * of (duplex) input and output devices.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pEnmDst             Where to store the device enumeration list.
 */
int coreAudioDevicesEnumerateAll(PDRVHOSTCOREAUDIO pThis, PPDMAUDIOHOSTENUM pEnmDst)
{
    PDMAUDIOHOSTENUM devEnmIn;
    int rc = coreAudioDevicesEnumerate(pThis, PDMAUDIODIR_IN, &devEnmIn);
    if (RT_SUCCESS(rc))
    {
        PDMAUDIOHOSTENUM devEnmOut;
        rc = coreAudioDevicesEnumerate(pThis, PDMAUDIODIR_OUT, &devEnmOut);
        if (RT_SUCCESS(rc))
        {
            /*
             * Build up the final device enumeration, based on the input and output device lists
             * just enumerated.
             *
             * Also make sure to handle duplex devices, that is, devices which act as input and output
             * at the same time.
             */
            PDMAudioHostEnumInit(pEnmDst);
            PCOREAUDIODEVICEDATA pDevSrcIn;
            RTListForEach(&devEnmIn.LstDevices, pDevSrcIn, COREAUDIODEVICEDATA, Core.ListEntry)
            {
                PCOREAUDIODEVICEDATA pDevDst = (PCOREAUDIODEVICEDATA)PDMAudioHostDevAlloc(sizeof(*pDevDst));
                if (!pDevDst)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }

                coreAudioDeviceDataInit(pDevDst, pDevSrcIn->deviceID, true /* fIsInput */, pThis);

                RTStrCopy(pDevDst->Core.szName, sizeof(pDevDst->Core.szName), pDevSrcIn->Core.szName);

                pDevDst->Core.enmUsage          = PDMAUDIODIR_IN; /* Input device by default (simplex). */
                pDevDst->Core.cMaxInputChannels = pDevSrcIn->Core.cMaxInputChannels;

                /* Handle flags. */
                if (pDevSrcIn->Core.fFlags & PDMAUDIOHOSTDEV_F_DEFAULT)
                    pDevDst->Core.fFlags |= PDMAUDIOHOSTDEV_F_DEFAULT;
                /** @todo Handle hot plugging? */

                /*
                 * Now search through the list of all found output devices and check if we found
                 * an output device with the same device ID as the currently handled input device.
                 *
                 * If found, this means we have to treat that device as a duplex device then.
                 */
                PCOREAUDIODEVICEDATA pDevSrcOut;
                RTListForEach(&devEnmOut.LstDevices, pDevSrcOut, COREAUDIODEVICEDATA, Core.ListEntry)
                {
                    if (pDevSrcIn->deviceID == pDevSrcOut->deviceID)
                    {
                        pDevDst->Core.enmUsage           = PDMAUDIODIR_DUPLEX;
                        pDevDst->Core.cMaxOutputChannels = pDevSrcOut->Core.cMaxOutputChannels;

                        if (pDevSrcOut->Core.fFlags & PDMAUDIOHOSTDEV_F_DEFAULT)
                            pDevDst->Core.fFlags |= PDMAUDIOHOSTDEV_F_DEFAULT;
                        break;
                    }
                }

                if (RT_SUCCESS(rc))
                    PDMAudioHostEnumAppend(pEnmDst, &pDevDst->Core);
                else
                {
                    PDMAudioHostDevFree(&pDevDst->Core);
                    pDevDst = NULL;
                }
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * As a last step, add all remaining output devices which have not been handled in the loop above,
                 * that is, all output devices which operate in simplex mode.
                 */
                PCOREAUDIODEVICEDATA pDevSrcOut;
                RTListForEach(&devEnmOut.LstDevices, pDevSrcOut, COREAUDIODEVICEDATA, Core.ListEntry)
                {
                    if (coreAudioDevicesHasDevice(pEnmDst, pDevSrcOut->deviceID))
                        continue; /* Already in our list, skip. */

                    PCOREAUDIODEVICEDATA pDevDst = (PCOREAUDIODEVICEDATA)PDMAudioHostDevAlloc(sizeof(*pDevDst));
                    if (!pDevDst)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    coreAudioDeviceDataInit(pDevDst, pDevSrcOut->deviceID, false /* fIsInput */, pThis);

                    RTStrCopy(pDevDst->Core.szName, sizeof(pDevDst->Core.szName), pDevSrcOut->Core.szName);

                    pDevDst->Core.enmUsage           = PDMAUDIODIR_OUT;
                    pDevDst->Core.cMaxOutputChannels = pDevSrcOut->Core.cMaxOutputChannels;

                    pDevDst->deviceID       = pDevSrcOut->deviceID;

                    /* Handle flags. */
                    if (pDevSrcOut->Core.fFlags & PDMAUDIOHOSTDEV_F_DEFAULT)
                        pDevDst->Core.fFlags |= PDMAUDIOHOSTDEV_F_DEFAULT;
                    /** @todo Handle hot plugging? */

                    PDMAudioHostEnumAppend(pEnmDst, &pDevDst->Core);
                }
            }

            if (RT_FAILURE(rc))
                PDMAudioHostEnumDelete(pEnmDst);

            PDMAudioHostEnumDelete(&devEnmOut);
        }

        PDMAudioHostEnumDelete(&devEnmIn);
    }

#ifdef LOG_ENABLED
    if (RT_SUCCESS(rc))
        PDMAudioHostEnumLog(pEnmDst, "Core Audio (Final)");
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Registers callbacks for a specific Core Audio device.
 *
 * @return IPRT status code.
 * @param  pThis                Host audio driver instance.
 * @param  pDev                 Audio device to use for the registered callbacks.
 */
static int coreAudioDeviceRegisterCallbacks(PDRVHOSTCOREAUDIO pThis, PCOREAUDIODEVICEDATA pDev)
{
    RT_NOREF(pThis);

    AudioDeviceID deviceID = kAudioDeviceUnknown;
    Assert(pDev && pDev->Core.cbSelf == sizeof(*pDev));
    if (pDev && pDev->Core.cbSelf == sizeof(*pDev)) /* paranoia or actually needed? */
        deviceID = pDev->deviceID;

    if (deviceID != kAudioDeviceUnknown)
    {
        LogFunc(("deviceID=%RU32\n", deviceID));

        /*
         * Register device callbacks.
         */
        AudioObjectPropertyAddress PropAddr =
        {
            kAudioDevicePropertyDeviceIsAlive,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        OSStatus err = AudioObjectAddPropertyListener(deviceID, &PropAddr, coreAudioDeviceStateChangedCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareIllegalOperationError)
            LogRel(("CoreAudio: Failed to add the recording device state changed listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDeviceProcessorOverload;
        PropAddr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &PropAddr, coreAudioDevPropChgCb, pDev /* pvUser */);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register processor overload listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyNominalSampleRate;
        PropAddr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &PropAddr, coreAudioDevPropChgCb, pDev /* pvUser */);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register sample rate changed listener (%RI32)\n", err));
    }

    return VINF_SUCCESS;
}


/**
 * Unregisters all formerly registered callbacks of a Core Audio device again.
 *
 * @return IPRT status code.
 * @param  pThis                Host audio driver instance.
 * @param  pDev                 Audio device to use for the registered callbacks.
 */
static int coreAudioDeviceUnregisterCallbacks(PDRVHOSTCOREAUDIO pThis, PCOREAUDIODEVICEDATA pDev)
{
    RT_NOREF(pThis);

    AudioDeviceID deviceID = kAudioDeviceUnknown;
    Assert(pDev && pDev->Core.cbSelf == sizeof(*pDev));
    if (pDev && pDev->Core.cbSelf == sizeof(*pDev)) /* paranoia or actually needed? */
        deviceID = pDev->deviceID;

    if (deviceID != kAudioDeviceUnknown)
    {
        LogFunc(("deviceID=%RU32\n", deviceID));

        /*
         * Unregister per-device callbacks.
         */
        AudioObjectPropertyAddress PropAddr =
        {
            kAudioDeviceProcessorOverload,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        OSStatus err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, coreAudioDevPropChgCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the recording processor overload listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyNominalSampleRate;
        err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, coreAudioDevPropChgCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the sample rate changed listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyDeviceIsAlive;
        err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, coreAudioDeviceStateChangedCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the device alive listener (%RI32)\n", err));
    }

    return VINF_SUCCESS;
}


/**
 * Enumerates all available host audio devices internally.
 *
 * @returns IPRT status code.
 * @param   pThis               Host audio driver instance.
 */
static int coreAudioEnumerateDevices(PDRVHOSTCOREAUDIO pThis)
{
    LogFlowFuncEnter();

    /*
     * Unregister old default devices, if any.
     */
    if (pThis->pDefaultDevIn)
    {
        coreAudioDeviceUnregisterCallbacks(pThis, pThis->pDefaultDevIn);
        pThis->pDefaultDevIn = NULL;
    }

    if (pThis->pDefaultDevOut)
    {
        coreAudioDeviceUnregisterCallbacks(pThis, pThis->pDefaultDevOut);
        pThis->pDefaultDevOut = NULL;
    }

    /* Remove old / stale device entries. */
    PDMAudioHostEnumDelete(&pThis->Devices);

    /* Enumerate all devices internally. */
    int rc = coreAudioDevicesEnumerateAll(pThis, &pThis->Devices);
    if (RT_SUCCESS(rc))
    {
        /*
         * Default input device.
         */
        pThis->pDefaultDevIn = (PCOREAUDIODEVICEDATA)PDMAudioHostEnumGetDefault(&pThis->Devices, PDMAUDIODIR_IN);
        if (pThis->pDefaultDevIn)
        {
            LogRel2(("CoreAudio: Default capturing device is '%s'\n", pThis->pDefaultDevIn->Core.szName));
            LogFunc(("pDefaultDevIn=%p, ID=%RU32\n", pThis->pDefaultDevIn, pThis->pDefaultDevIn->deviceID));
            rc = coreAudioDeviceRegisterCallbacks(pThis, pThis->pDefaultDevIn);
        }
        else
            LogRel2(("CoreAudio: No default capturing device found\n"));

        /*
         * Default output device.
         */
        pThis->pDefaultDevOut = (PCOREAUDIODEVICEDATA)PDMAudioHostEnumGetDefault(&pThis->Devices, PDMAUDIODIR_OUT);
        if (pThis->pDefaultDevOut)
        {
            LogRel2(("CoreAudio: Default playback device is '%s'\n", pThis->pDefaultDevOut->Core.szName));
            LogFunc(("pDefaultDevOut=%p, ID=%RU32\n", pThis->pDefaultDevOut, pThis->pDefaultDevOut->deviceID));
            rc = coreAudioDeviceRegisterCallbacks(pThis, pThis->pDefaultDevOut);
        }
        else
            LogRel2(("CoreAudio: No default playback device found\n"));
    }

    LogFunc(("Returning %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetDevices}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_GetDevices(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum)
{
    PDRVHOSTCOREAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    AssertPtrReturn(pDeviceEnum, VERR_INVALID_POINTER);

    PDMAudioHostEnumInit(pDeviceEnum);

    /*
     * We update the enumeration associated with pThis.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        rc = coreAudioEnumerateDevices(pThis);
        if (RT_SUCCESS(rc))
        {
            /*
             * Return a copy with only PDMAUDIOHOSTDEV and none of the extra
             * bits in COREAUDIODEVICEDATA.
             */
            rc = PDMAudioHostEnumCopy(pDeviceEnum, &pThis->Devices, PDMAUDIODIR_INVALID /*all*/, true /*fOnlyCoreData*/);
            if (RT_FAILURE(rc))
                PDMAudioHostEnumDelete(pDeviceEnum);
        }

        RTCritSectLeave(&pThis->CritSect);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostCoreAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(pInterface, enmDir);
    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTCOREAUDIO pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    PCOREAUDIOSTREAM  pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);
    AssertReturn(pCfgReq->enmDir == PDMAUDIODIR_IN || pCfgReq->enmDir == PDMAUDIODIR_OUT, VERR_INVALID_PARAMETER);
    int rc;

    /*
     * Permission check for input devices before we start.
     */
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
    {
        rc = coreAudioInputPermissionCheck();
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Do we have a device for the requested stream direction?
     */
    PCOREAUDIODEVICEDATA pDev = pCfgReq->enmDir == PDMAUDIODIR_IN ? pThis->pDefaultDevIn : pThis->pDefaultDevOut;
#ifdef LOG_ENABLED
    char szTmp[PDMAUDIOSTRMCFGTOSTRING_MAX];
#endif
    LogFunc(("pDev=%p *pCfgReq: %s\n", pDev, PDMAudioStrmCfgToString(pCfgReq, szTmp, sizeof(szTmp)) ));
    if (pDev)
    {
        Assert(pDev->Core.cbSelf == sizeof(*pDev));

        /*
         * Basic structure init.
         */
        pStreamCA->hThread      = NIL_RTTHREAD;
        pStreamCA->fRun         = false;
        pStreamCA->fIsRunning   = false;
        pStreamCA->fShutdown    = false;
        pStreamCA->Unit.pDevice = pDev;   /** @todo r=bird: How do we protect this against enumeration releasing pDefaultDevOut/In. */
        pStreamCA->enmInitState = COREAUDIOINITSTATE_IN_INIT;

        rc = RTCritSectInit(&pStreamCA->CritSect);
        if (RT_SUCCESS(rc))
        {
            /*
             * Do format conversion and create the circular buffer we use to shuffle
             * data to/from the queue thread.
             */
            PDMAudioStrmCfgCopy(&pStreamCA->Cfg, pCfgReq);
            coreAudioPCMPropsToASBD(&pCfgReq->Props, &pStreamCA->asbdStream);
            /** @todo Do some validation? */
            coreAudioPrintASBD(  pCfgReq->enmDir == PDMAUDIODIR_IN
                               ? "Capturing queue format"
                               : "Playback queue format", &pStreamCA->asbdStream);

            rc = RTCircBufCreate(&pStreamCA->pCircBuf,
                                 PDMAudioPropsFramesToBytes(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Start the thread.
                 */
                static uint32_t volatile s_idxThread = 0;
                uint32_t idxThread = ASMAtomicIncU32(&s_idxThread);

                rc = RTThreadCreateF(&pStreamCA->hThread, coreAudioQueueThread, pStreamCA, 0 /*cbStack*/,
                                     RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "CaQue%u", idxThread);
                if (RT_SUCCESS(rc))
                {
                    rc = RTThreadUserWait(pStreamCA->hThread, RT_MS_10SEC);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc) && pStreamCA->hRunLoop != NULL)
                    {
                        ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_INIT);

                        LogFunc(("returns VINF_SUCCESS\n"));
                        return VINF_SUCCESS;
                    }

                    /*
                     * Failed, clean up.
                     */
                    LogRel(("CoreAudio: Thread failed to initialize in a timely manner (%Rrc).\n", rc));

                    ASMAtomicWriteBool(&pStreamCA->fShutdown, true);
                    RTThreadPoke(pStreamCA->hThread);
                    int rcThread = 0;
                    rc = RTThreadWait(pStreamCA->hThread, RT_MS_15SEC, NULL);
                    AssertLogRelRC(rc);
                    LogRel(("CoreAudio: Thread exit code: %Rrc / %Rrc.\n", rc, rcThread));
                    pStreamCA->hThread = NIL_RTTHREAD;
                }
                else
                    LogRel(("CoreAudio: Failed to create queue thread for stream: %Rrc\n", rc));
                RTCircBufDestroy(pStreamCA->pCircBuf);
                pStreamCA->pCircBuf = NULL;
            }
            else
                LogRel(("CoreAudio: Failed to allocate stream buffer: %Rrc\n", rc));
            RTCritSectDelete(&pStreamCA->CritSect);
        }
        else
            LogRel(("CoreAudio: Failed to initialize critical section for stream: %Rrc\n", rc));
    }
    else
    {
        LogFunc(("No device for stream.\n"));
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    }

    LogFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);

    /*
     * Never mind if the status isn't INIT (it should always be, though).
     */
    COREAUDIOINITSTATE const enmInitState = (COREAUDIOINITSTATE)ASMAtomicReadU32(&pStreamCA->enmInitState);
    AssertMsg(enmInitState == COREAUDIOINITSTATE_INIT, ("%d\n", enmInitState));
    if (enmInitState == COREAUDIOINITSTATE_INIT)
    {
        Assert(RTCritSectIsInitialized(&pStreamCA->CritSect));

        /*
         * Disable (stop) the stream just in case it's running.
         */
        /** @todo this isn't paranoid enough, the pStreamCA->hAudioQueue is
         *        owned+released by the queue thread. */
        drvHostCoreAudioStreamControlInternal(pStreamCA, PDMAUDIOSTREAMCMD_DISABLE);

        /*
         * Change the state (cannot do before the stop).
         * Enter and leave the critsect afterwards for paranoid reasons.
         */
        ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_IN_UNINIT);
        RTCritSectEnter(&pStreamCA->CritSect);
        RTCritSectLeave(&pStreamCA->CritSect);

        /*
         * Bring down the queue thread.
         */
        if (pStreamCA->hThread != NIL_RTTHREAD)
        {
            LogFunc(("Waiting for thread ...\n"));
            ASMAtomicXchgBool(&pStreamCA->fShutdown, true);
            int rcThread = VERR_IPE_UNINITIALIZED_STATUS;
            int rc       = VERR_TIMEOUT;
            for (uint32_t iWait = 0; rc == VERR_TIMEOUT && iWait < 60; iWait++)
            {
                LogFunc(("%u ...\n", iWait));
                if (pStreamCA->hRunLoop != NULL)
                    CFRunLoopStop(pStreamCA->hRunLoop);
                if (iWait >= 10)
                    RTThreadPoke(pStreamCA->hThread);

                rcThread = VERR_IPE_UNINITIALIZED_STATUS;
                rc = RTThreadWait(pStreamCA->hThread, RT_MS_1SEC / 2, &rcThread);
            }
            AssertLogRelRC(rc);
            LogFunc(("Thread stopped with: %Rrc/%Rrc\n", rc, rcThread));
            pStreamCA->hThread = NIL_RTTHREAD;
        }

        if (pStreamCA->hRunLoop != NULL)
        {
            CFRelease(pStreamCA->hRunLoop);
            pStreamCA->hRunLoop = NULL;
        }

        /*
         * Kill the circular buffer and NULL essential variable.
         */
        if (pStreamCA->pCircBuf)
        {
            RTCircBufDestroy(pStreamCA->pCircBuf);
            pStreamCA->pCircBuf = NULL;
        }

        pStreamCA->Unit.pDevice = NULL;

        RTCritSectDelete(&pStreamCA->CritSect);

        /*
         * Done.
         */
        ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_UNINIT);
    }

    LogFunc(("returns\n"));
    return VINF_SUCCESS;
}


static int drvHostCoreAudioStreamControlInternal(PCOREAUDIOSTREAM pStreamCA, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    uint32_t enmInitState = ASMAtomicReadU32(&pStreamCA->enmInitState);

    LogFlowFunc(("enmStreamCmd=%RU32, enmInitState=%RU32\n", enmStreamCmd, enmInitState));

    if (enmInitState != COREAUDIOINITSTATE_INIT)
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            LogFunc(("Queue enable\n"));
            if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN)
            {
                rc = coreAudioStreamInvalidateQueue(pStreamCA);
                if (RT_SUCCESS(rc))
                {
                    /* Start the audio queue immediately. */
                    AudioQueueStart(pStreamCA->hAudioQueue, NULL);
                }
            }
            else if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT)
            {
                /* Touch the run flag to start the audio queue as soon as
                 * we have anough data to actually play something. */
                ASMAtomicXchgBool(&pStreamCA->fRun, true);
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            LogFunc(("Queue disable\n"));
            AudioQueueStop(pStreamCA->hAudioQueue, 1 /* Immediately */);
            ASMAtomicXchgBool(&pStreamCA->fRun,       false);
            ASMAtomicXchgBool(&pStreamCA->fIsRunning, false);
            break;
        }
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            LogFunc(("Queue pause\n"));
            AudioQueuePause(pStreamCA->hAudioQueue);
            ASMAtomicXchgBool(&pStreamCA->fIsRunning, false);
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);

    return drvHostCoreAudioStreamControlInternal(pStreamCA, enmStreamCmd);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostCoreAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);

    if (ASMAtomicReadU32(&pStreamCA->enmInitState) != COREAUDIOINITSTATE_INIT)
        return 0;

    if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        AssertPtr(pStreamCA->pCircBuf);
        return (uint32_t)RTCircBufUsed(pStreamCA->pCircBuf);
    }
    AssertFailed();
    return 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostCoreAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);


    uint32_t cbWritable = 0;
    if (ASMAtomicReadU32(&pStreamCA->enmInitState) == COREAUDIOINITSTATE_INIT)
    {
        AssertPtr(pStreamCA->pCircBuf);

        if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT)
            cbWritable = (uint32_t)RTCircBufFree(pStreamCA->pCircBuf);
    }

    LogFlowFunc(("cbWritable=%RU32\n", cbWritable));
    return cbWritable;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostCoreAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                               PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCa = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCa, PDMHOSTAUDIOSTREAMSTATE_INVALID);

    if (ASMAtomicReadU32(&pStreamCa->enmInitState) == COREAUDIOINITSTATE_INIT)
        return PDMHOSTAUDIOSTREAMSTATE_OKAY;
    return PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING; /** @todo ?? */
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVHOSTCOREAUDIO pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    PCOREAUDIOSTREAM  pStreamCA = (PCOREAUDIOSTREAM)pStream;

    RT_NOREF(pThis);

    if (ASMAtomicReadU32(&pStreamCA->enmInitState) != COREAUDIOINITSTATE_INIT)
    {
        *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    int rc = RTCritSectEnter(&pStreamCA->CritSect);
    AssertRCReturn(rc, rc);

    size_t cbToWrite = RT_MIN(cbBuf, RTCircBufFree(pStreamCA->pCircBuf));
    Log3Func(("cbToWrite=%zu\n", cbToWrite));

    uint32_t cbWrittenTotal = 0;
    while (cbToWrite > 0)
    {
        /* Try to acquire the necessary space from the ring buffer. */
        void    *pvChunk = NULL;
        size_t   cbChunk = 0;
        RTCircBufAcquireWriteBlock(pStreamCA->pCircBuf, cbToWrite, &pvChunk, &cbChunk);
        AssertBreakStmt(cbChunk > 0, RTCircBufReleaseWriteBlock(pStreamCA->pCircBuf, cbChunk));

        Assert(cbChunk <= cbToWrite);
        Assert(cbWrittenTotal + cbChunk <= cbBuf);

        memcpy(pvChunk, (uint8_t *)pvBuf + cbWrittenTotal, cbChunk);

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        rc = RTFileOpen(&fh,VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "caPlayback.pcm",
                        RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            RTFileWrite(fh, pvChunk, cbChunk, NULL);
            RTFileClose(fh);
        }
        else
            AssertFailed();
#endif

        /* Release the ring buffer, so the read thread could start reading this data. */
        RTCircBufReleaseWriteBlock(pStreamCA->pCircBuf, cbChunk);

        if (RT_FAILURE(rc))
            break;

        Assert(cbToWrite >= cbChunk);
        cbToWrite      -= cbChunk;

        cbWrittenTotal += cbChunk;
    }

    if (    RT_SUCCESS(rc)
        &&  pStreamCA->fRun
        && !pStreamCA->fIsRunning)
    {
        rc = coreAudioStreamInvalidateQueue(pStreamCA);
        if (RT_SUCCESS(rc))
        {
            AudioQueueStart(pStreamCA->hAudioQueue, NULL);
            pStreamCA->fRun       = false;
            pStreamCA->fIsRunning = true;
        }
    }

    int rc2 = RTCritSectLeave(&pStreamCA->CritSect);
    AssertRC(rc2);

    *pcbWritten = cbWrittenTotal;
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostCoreAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM  pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);


    if (ASMAtomicReadU32(&pStreamCA->enmInitState) != COREAUDIOINITSTATE_INIT)
    {
        *pcbRead = 0;
        return VINF_SUCCESS;
    }

    int rc = RTCritSectEnter(&pStreamCA->CritSect);
    AssertRCReturn(rc, rc);

    size_t cbToWrite = RT_MIN(cbBuf, RTCircBufUsed(pStreamCA->pCircBuf));
    Log3Func(("cbToWrite=%zu/%zu\n", cbToWrite, RTCircBufSize(pStreamCA->pCircBuf)));

    uint32_t cbReadTotal = 0;
    while (cbToWrite > 0)
    {
        void    *pvChunk = NULL;
        size_t   cbChunk = 0;
        RTCircBufAcquireReadBlock(pStreamCA->pCircBuf, cbToWrite, &pvChunk, &cbChunk);

        AssertStmt(cbChunk <= cbToWrite, cbChunk = cbToWrite);
        memcpy((uint8_t *)pvBuf + cbReadTotal, pvChunk, cbChunk);

        RTCircBufReleaseReadBlock(pStreamCA->pCircBuf, cbChunk);

        cbToWrite      -= cbChunk;
        cbReadTotal    += cbChunk;
    }

    *pcbRead = cbReadTotal;

    RTCritSectLeave(&pStreamCA->CritSect);
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   PDMIBASE                                                                                                                     *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostCoreAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,      &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}


/*********************************************************************************************************************************
*   PDMDRVREG                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Worker for the power off and destructor callbacks.
 */
static void drvHostCoreAudioRemoveDefaultDeviceListners(PDRVHOSTCOREAUDIO pThis)
{
    /*
     * Unregister system callbacks.
     */
    AudioObjectPropertyAddress PropAddr =
    {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    OSStatus orc;
    if (pThis->fRegisteredDefaultInputListener)
    {
        orc = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &PropAddr, coreAudioDefaultDeviceChangedCb, pThis);
        if (   orc != noErr
            && orc != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the default input device changed listener: %d (%#x))\n", orc, orc));
        pThis->fRegisteredDefaultInputListener = false;
    }

    if (pThis->fRegisteredDefaultOutputListener)
    {

        PropAddr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        orc = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &PropAddr, coreAudioDefaultDeviceChangedCb, pThis);
        if (   orc != noErr
            && orc != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the default output device changed listener: %d (%#x))\n", orc, orc));
        pThis->fRegisteredDefaultOutputListener = false;
    }

    LogFlowFuncEnter();
}


/**
 * @interface_method_impl{PDMDRVREG,pfnPowerOff}
 */
static DECLCALLBACK(void) drvHostCoreAudioPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);
    drvHostCoreAudioRemoveDefaultDeviceListners(pThis);
}


/**
 * @callback_method_impl{FNPDMDRVDESTRUCT}
 */
static DECLCALLBACK(void) drvHostCoreAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    drvHostCoreAudioRemoveDefaultDeviceListners(pThis);

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc2);
}


/**
 * @callback_method_impl{FNPDMDRVCONSTRUCT,
 *      Construct a Core Audio driver instance.}
 */
static DECLCALLBACK(int) drvHostCoreAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);
    LogRel(("Audio: Initializing Core Audio driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    PDMAudioHostEnumInit(&pThis->Devices);
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostCoreAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig                  = drvHostCoreAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = drvHostCoreAudioHA_GetDevices;
    pThis->IHostAudio.pfnGetStatus                  = drvHostCoreAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostCoreAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostCoreAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostCoreAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostCoreAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostCoreAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostCoreAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostCoreAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostCoreAudioHA_StreamCapture;

    int rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * Enumerate audio devices.
     */
    rc = coreAudioEnumerateDevices(pThis);
    AssertRCReturn(rc, rc);

    /*
     * Register callbacks for default device input and output changes.
     * We just ignore errors here it seems.
     */
    AudioObjectPropertyAddress PropAddr =
    {
        /* .mSelector = */  kAudioHardwarePropertyDefaultInputDevice,
        /* .mScope = */     kAudioObjectPropertyScopeGlobal,
        /* .mElement = */   kAudioObjectPropertyElementMaster
    };

    OSStatus orc = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &PropAddr, coreAudioDefaultDeviceChangedCb, pThis);
    pThis->fRegisteredDefaultInputListener = orc == noErr;
    if (   orc != noErr
        && orc != kAudioHardwareIllegalOperationError)
        LogRel(("CoreAudio: Failed to add the input default device changed listener: %d (%#x)\n", orc, orc));

    PropAddr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    orc = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &PropAddr, coreAudioDefaultDeviceChangedCb, pThis);
    pThis->fRegisteredDefaultOutputListener = orc == noErr;
    if (   orc != noErr
        && orc != kAudioHardwareIllegalOperationError)
        LogRel(("CoreAudio: Failed to add the output default device changed listener: %d (%#x)\n", orc, orc));

    /*
     * Query the notification interface from the driver/device above us.
     */
    pThis->pIHostAudioPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIHOSTAUDIOPORT);
    AssertReturn(pThis->pIHostAudioPort, VERR_PDM_MISSING_INTERFACE_ABOVE);

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
    RTFileDelete(VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "caConverterCbInput.pcm");
    RTFileDelete(VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "caPlayback.pcm");
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostCoreAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "CoreAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Core Audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTCOREAUDIO),
    /* pfnConstruct */
    drvHostCoreAudioConstruct,
    /* pfnDestruct */
    drvHostCoreAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvHostCoreAudioPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

