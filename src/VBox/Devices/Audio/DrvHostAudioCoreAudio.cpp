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

#include "VBoxDD.h"

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>
#include <iprt/mem.h>
#include <iprt/uuid.h>
#include <iprt/timer.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioUnit/AudioUnit.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The max number of queue buffers we'll use. */
#define COREAUDIO_MAX_BUFFERS       1024
/** The minimum number of queue buffers. */
#define COREAUDIO_MIN_BUFFERS       4

/** Enables the worker thread.
 * This saves CoreAudio from creating an additional thread upon queue
 * creation.  (It does not help with the slow AudioQueueDispose fun.)  */
#define CORE_AUDIO_WITH_WORKER_THREAD
#if 0
/** Enables the AudioQueueDispose breakpoint timer (debugging help). */
# define CORE_AUDIO_WITH_BREAKPOINT_TIMER
#endif


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


/**
 * Core audio buffer tracker.
 *
 * For output buffer we'll be using AudioQueueBuffer::mAudioDataByteSize to
 * track how much we've written.  When a buffer is full, or if we run low on
 * queued bufferes, it will be queued.
 *
 * For input buffer we'll be using offRead to track how much we've read.
 *
 * The queued/not-queued state is stored in the first bit of
 * AudioQueueBuffer::mUserData.  While bits 8 and up holds the index into
 * COREAUDIOSTREAM::paBuffers.
 */
typedef struct COREAUDIOBUF
{
    /** The buffer. */
    AudioQueueBufferRef     pBuf;
    /** The buffer read offset (input only). */
    uint32_t                offRead;
} COREAUDIOBUF;
/** Pointer to a core audio buffer tracker. */
typedef COREAUDIOBUF *PCOREAUDIOBUF;


/**
 * Core Audio specific data for an audio stream.
 */
typedef struct COREAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM       Core;

    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG           Cfg;
    /** List node for the device's stream list. */
    RTLISTNODE                  Node;
    /** The acquired (final) audio format for this stream.
     * @note This what the device requests, we don't alter anything. */
    AudioStreamBasicDescription BasicStreamDesc;
    /** The actual audio queue being used. */
    AudioQueueRef               hAudioQueue;

    /** Number of buffers. */
    uint32_t                    cBuffers;
    /** The array of buffer. */
    PCOREAUDIOBUF               paBuffers;

    /** Initialization status tracker, actually COREAUDIOINITSTATE.
     * Used when some of the device parameters or the device itself is changed
     * during the runtime. */
    volatile uint32_t           enmInitState;
    /** The current buffer being written to / read from. */
    uint32_t                    idxBuffer;
    /** Set if the stream is enabled. */
    bool                        fEnabled;
    /** Set if the stream is started (playing/capturing). */
    bool                        fStarted;
    /** Set if the stream is draining (output only). */
    bool                        fDraining;
    /** Set if we should restart the stream on resume (saved pause state). */
    bool                        fRestartOnResume;
//    /** Set if we're switching to a new output/input device. */
//    bool                        fSwitchingDevice;
    /** Internal stream offset (bytes). */
    uint64_t                    offInternal;
    /** The RTTimeMilliTS() at the end of the last transfer. */
    uint64_t                    msLastTransfer;

    /** Critical section for serializing access between thread + callbacks. */
    RTCRITSECT                  CritSect;
    /** Buffer that drvHostAudioCaStreamStatusString uses. */
    char                        szStatus[64];
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

#ifdef CORE_AUDIO_WITH_WORKER_THREAD
    /** @name Worker Thread For Queue callbacks and stuff.
     * @{ */
    /** The worker thread. */
    RTTHREAD                hThread;
    /** The runloop of the worker thread. */
    CFRunLoopRef            hThreadRunLoop;
    /** The message port we use to talk to the thread.
     * @note While we don't currently use the port, it is necessary to prevent
     *       the thread from spinning or stopping prematurely because of
     *       CFRunLoopRunInMode returning kCFRunLoopRunFinished. */
    CFMachPortRef           hThreadPort;
    /** Runloop source for hThreadPort. */
    CFRunLoopSourceRef      hThreadPortSrc;
    /** @} */
#endif

    /** Critical section to serialize access. */
    RTCRITSECT              CritSect;
#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
    /** Timder for debugging AudioQueueDispose slowness. */
    RTTIMERLR               hBreakpointTimer;
#endif
} DRVHOSTCOREAUDIO;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/* DrvHostAudioCoreAudioAuth.mm: */
DECLHIDDEN(int) coreAudioInputPermissionCheck(void);


#ifdef LOG_ENABLED
/**
 * Gets the stream status.
 *
 * @returns Pointer to stream status string.
 * @param   pStreamCA   The stream to get the status for.
 */
static const char *drvHostAudioCaStreamStatusString(PCOREAUDIOSTREAM pStreamCA)
{
    static RTSTRTUPLE const s_aInitState[5] =
    {
        { RT_STR_TUPLE("UNINIT")    },
        { RT_STR_TUPLE("IN_INIT")   },
        { RT_STR_TUPLE("INIT")      },
        { RT_STR_TUPLE("IN_UNINIT") },
        { RT_STR_TUPLE("BAD")       },
    };
    uint32_t enmInitState = pStreamCA->enmInitState;
    PCRTSTRTUPLE pTuple = &s_aInitState[RT_MIN(enmInitState, RT_ELEMENTS(s_aInitState) - 1)];
    memcpy(pStreamCA->szStatus, pTuple->psz, pTuple->cch);
    size_t off = pTuple->cch;

    static RTSTRTUPLE const s_aEnable[2] =
    {
        { RT_STR_TUPLE("DISABLED") },
        { RT_STR_TUPLE("ENABLED ") },
    };
    pTuple = &s_aEnable[pStreamCA->fEnabled];
    memcpy(pStreamCA->szStatus, pTuple->psz, pTuple->cch);
    off += pTuple->cch;

    static RTSTRTUPLE const s_aStarted[2] =
    {
        { RT_STR_TUPLE(" STOPPED") },
        { RT_STR_TUPLE(" STARTED") },
    };
    pTuple = &s_aStarted[pStreamCA->fStarted];
    memcpy(&pStreamCA->szStatus[off], pTuple->psz, pTuple->cch);
    off += pTuple->cch;

    static RTSTRTUPLE const s_aDraining[2] =
    {
        { RT_STR_TUPLE("")          },
        { RT_STR_TUPLE(" DRAINING") },
    };
    pTuple = &s_aDraining[pStreamCA->fDraining];
    memcpy(&pStreamCA->szStatus[off], pTuple->psz, pTuple->cch);
    off += pTuple->cch;

    Assert(off < sizeof(pStreamCA->szStatus));
    pStreamCA->szStatus[off] = '\0';
    return pStreamCA->szStatus;
}
#endif /*LOG_ENABLED*/


static void drvHostAudioCaPrintASBD(const char *pszDesc, const AudioStreamBasicDescription *pASBD)
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


static void drvHostAudioCaPCMPropsToASBD(PCPDMAUDIOPCMPROPS pProps, AudioStreamBasicDescription *pASBD)
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
static int drvHostAudioCaCFStringToCString(const CFStringRef pCFString, char **ppszString)
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

static AudioDeviceID drvHostAudioCaDeviceUIDtoID(const char* pszUID)
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


/*********************************************************************************************************************************
*   Device Change Notification Callbacks                                                                                         *
*********************************************************************************************************************************/

/**
 * Called when the kAudioDevicePropertyNominalSampleRate or
 * kAudioDeviceProcessorOverload properties changes on a default device.
 *
 * Registered on default devices after device enumeration.
 * Not sure on which thread/runloop this runs.
 *
 * (See AudioObjectPropertyListenerProc in the SDK headers.)
 */
static OSStatus drvHostAudioCaDevicePropertyChangedCallback(AudioObjectID idObject, UInt32 cAddresses,
                                                            const AudioObjectPropertyAddress paAddresses[], void *pvUser)
{
    PCOREAUDIODEVICEDATA pDev = (PCOREAUDIODEVICEDATA)pvUser;
    AssertPtr(pDev);
    RT_NOREF(pDev, cAddresses, paAddresses);

    LogFlowFunc(("idObject=%#x (%u) cAddresses=%u pDev=%p\n", idObject, idObject, cAddresses, pDev));
    for (UInt32 idx = 0; idx < cAddresses; idx++)
        LogFlowFunc(("  #%u: sel=%#x scope=%#x element=%#x\n",
                     idx, paAddresses[idx].mSelector, paAddresses[idx].mScope, paAddresses[idx].mElement));

/** @todo r=bird: What's the plan here exactly?   */
    switch (idObject)
    {
        case kAudioDeviceProcessorOverload:
            LogFunc(("Processor overload detected!\n"));
            break;
        case kAudioDevicePropertyNominalSampleRate:
            LogFunc(("kAudioDevicePropertyNominalSampleRate!\n"));
            break;
        default:
            /* Just skip. */
            break;
    }

    return noErr;
}


/**
 * Propagates an audio device status to all its Core Audio streams.
 *
 * @param  pDev     Audio device to propagate status for.
 * @param  enmSts   Status to propagate.
 */
static void drvHostAudioCaDevicePropagateStatus(PCOREAUDIODEVICEDATA pDev, COREAUDIOINITSTATE enmSts)
{
    /* Sanity. */
    AssertPtr(pDev);
    AssertPtr(pDev->pDrv);

    LogFlowFunc(("pDev=%p enmSts=%RU32\n", pDev, enmSts));

    PCOREAUDIOSTREAM pStreamCA;
    RTListForEach(&pDev->lstStreams, pStreamCA, COREAUDIOSTREAM, Node)
    {
        LogFlowFunc(("pStreamCA=%p\n", pStreamCA));

        /* We move the reinitialization to the next output event.
         * This make sure this thread isn't blocked and the
         * reinitialization is done when necessary only. */
/** @todo r=bird: This is now extremely bogus, see comment in caller. */
        ASMAtomicWriteU32(&pStreamCA->enmInitState, enmSts);
    }
}


/**
 * Called when the kAudioDevicePropertyDeviceIsAlive property changes on a
 * default device.
 *
 * Registered on default devices after device enumeration.
 * Not sure on which thread/runloop this runs.
 *
 * (See AudioObjectPropertyListenerProc in the SDK headers.)
 */
static OSStatus drvHostAudioCaDeviceIsAliveChangedCallback(AudioObjectID idObject, UInt32 cAddresses,
                                                           const AudioObjectPropertyAddress paAddresses[], void *pvUser)
{
    PCOREAUDIODEVICEDATA pDev = (PCOREAUDIODEVICEDATA)pvUser;
    AssertPtr(pDev);
    PDRVHOSTCOREAUDIO    pThis = pDev->pDrv;
    AssertPtr(pThis);
    RT_NOREF(idObject, cAddresses, paAddresses);

    LogFlowFunc(("idObject=%#x (%u) cAddresses=%u pDev=%p\n", idObject, idObject, cAddresses, pDev));
    for (UInt32 idx = 0; idx < cAddresses; idx++)
        LogFlowFunc(("  #%u: sel=%#x scope=%#x element=%#x\n",
                     idx, paAddresses[idx].mSelector, paAddresses[idx].mScope, paAddresses[idx].mElement));

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc);

    UInt32 uAlive = 1;
    UInt32 uSize  = sizeof(UInt32);

    AudioObjectPropertyAddress PropAddr =
    {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    OSStatus err = AudioObjectGetPropertyData(pDev->deviceID, &PropAddr, 0, NULL, &uSize, &uAlive);

    bool fIsDead = false;
    if (err == kAudioHardwareBadDeviceError)
        fIsDead = true; /* Unplugged. */
    else if (err == kAudioHardwareNoError && !RT_BOOL(uAlive))
        fIsDead = true; /* Something else happened. */

    if (fIsDead)
    {
        LogRel2(("CoreAudio: Device '%s' stopped functioning\n", pDev->Core.szName));

        /* Mark device as dead. */
/** @todo r=bird: This is certifiably insane given how StreamDestroy does absolutely _nothing_ unless the init state is INIT.
 * The queue thread will be running and trashing random heap if it tries to modify anything in the stream structure. */
        drvHostAudioCaDevicePropagateStatus(pDev, COREAUDIOINITSTATE_UNINIT);
    }

    RTCritSectLeave(&pThis->CritSect);
    return noErr;
}


/**
 * Called when the default recording or playback device has changed.
 *
 * Registered by the constructor.  Not sure on which thread/runloop this runs.
 *
 * (See AudioObjectPropertyListenerProc in the SDK headers.)
 */
static OSStatus drvHostAudioCaDefaultDeviceChangedCallback(AudioObjectID idObject, UInt32 cAddresses,
                                                           const AudioObjectPropertyAddress *paAddresses, void *pvUser)

{
    PDRVHOSTCOREAUDIO pThis = (PDRVHOSTCOREAUDIO)pvUser;
    AssertPtr(pThis);
    LogFunc(("idObject=%#x (%u) cAddresses=%u\n", idObject, idObject, cAddresses));
    RT_NOREF(idObject);

    //int rc2 = RTCritSectEnter(&pThis->CritSect);
    //AssertRC(rc2);

    for (UInt32 idxAddress = 0; idxAddress < cAddresses; idxAddress++)
    {
        /// @todo r=bird: what's the plan here? PCOREAUDIODEVICEDATA pDev = NULL;

        /*
         * Check if the default input / output device has been changed.
         */
        const AudioObjectPropertyAddress *pProperty = &paAddresses[idxAddress];
        switch (pProperty->mSelector)
        {
            case kAudioHardwarePropertyDefaultInputDevice:
                LogFlowFunc(("#%u: sel=kAudioHardwarePropertyDefaultInputDevice scope=%#x element=%#x\n",
                             idxAddress, pProperty->mScope, pProperty->mElement));
                //pDev = pThis->pDefaultDevIn;
                break;

            case kAudioHardwarePropertyDefaultOutputDevice:
                LogFlowFunc(("#%u: sel=kAudioHardwarePropertyDefaultOutputDevice scope=%#x element=%#x\n",
                             idxAddress, pProperty->mScope, pProperty->mElement));
                //pDev = pThis->pDefaultDevOut;
                break;

            default:
                LogFlowFunc(("#%u: sel=%#x scope=%#x element=%#x\n",
                             idxAddress, pProperty->mSelector, pProperty->mScope, pProperty->mElement));
                break;
        }
    }

    /* Make sure to leave the critical section before notify higher drivers/devices. */
    //rc2 = RTCritSectLeave(&pThis->CritSect);
    //AssertRC(rc2);

    /*
     * Notify the driver/device above us about possible changes in devices.
     */
    if (pThis->pIHostAudioPort)
        pThis->pIHostAudioPort->pfnNotifyDevicesChanged(pThis->pIHostAudioPort);

    return noErr;
}


/*********************************************************************************************************************************
*   Worker Thread                                                                                                                *
*********************************************************************************************************************************/
#ifdef CORE_AUDIO_WITH_WORKER_THREAD

/**
 * Message handling callback for CFMachPort.
 */
static void drvHostAudioCaThreadPortCallback(CFMachPortRef hPort, void *pvMsg, CFIndex cbMsg, void *pvUser)
{
    RT_NOREF(hPort, pvMsg, cbMsg, pvUser);
    LogFunc(("hPort=%p pvMsg=%p cbMsg=%#x pvUser=%p\n", hPort, pvMsg, cbMsg, pvUser));
}


/**
 * @callback_method_impl{FNRTTHREAD, Worker thread for buffer callbacks.}
 */
static DECLCALLBACK(int) drvHostAudioCaThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PDRVHOSTCOREAUDIO pThis = (PDRVHOSTCOREAUDIO)pvUser;

    /*
     * Get the runloop, add the mach port to it and signal the constructor thread that we're ready.
     */
    pThis->hThreadRunLoop = CFRunLoopGetCurrent();
    CFRetain(pThis->hThreadRunLoop);

    CFRunLoopAddSource(pThis->hThreadRunLoop, pThis->hThreadPortSrc, kCFRunLoopDefaultMode);

    int rc = RTThreadUserSignal(hThreadSelf);
    AssertRCReturn(rc, rc);

    /*
     * Do work.
     */
    for (;;)
    {
        SInt32 rcRunLoop = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 30.0, TRUE);
        Log8Func(("CFRunLoopRunInMode -> %d\n", rcRunLoop));
        Assert(rcRunLoop != kCFRunLoopRunFinished);
        if (rcRunLoop != kCFRunLoopRunStopped && rcRunLoop != kCFRunLoopRunFinished)
        { /* likely */ }
        else
            break;
    }

    /*
     * Clean up.
     */
    CFRunLoopRemoveSource(pThis->hThreadRunLoop, pThis->hThreadPortSrc, kCFRunLoopDefaultMode);
    LogFunc(("The thread quits!\n"));
    return VINF_SUCCESS;
}

#endif /* CORE_AUDIO_WITH_WORKER_THREAD */



/*********************************************************************************************************************************
*   PDMIHOSTAUDIO                                                                                                                *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    PDRVHOSTCOREAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "Core Audio");
    pBackendCfg->cbStream       = sizeof(COREAUDIOSTREAM);
    pBackendCfg->fFlags         = PDMAUDIOBACKEND_F_ASYNC_STREAM_DESTROY;
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
static void drvHostAudioCaDeviceDataInit(PCOREAUDIODEVICEDATA pDevData, AudioDeviceID deviceID,
                                         bool fIsInput, PDRVHOSTCOREAUDIO pDrv)
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
static int drvHostAudioCaDevicesEnumerate(PDRVHOSTCOREAUDIO pThis, PDMAUDIODIR enmUsage, PPDMAUDIOHOSTENUM pDevEnm)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pDevEnm, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    do /* (this is not a loop, just a device for avoid gotos while trying not to shoot oneself in the foot too badly.) */
    {
        /*
         * First get the device ID of the default device.
         */
        AudioDeviceID defaultDeviceID = kAudioDeviceUnknown;
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

        /*
         * Get a list of all audio devices.
         */
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

        /*
         * Try get details on each device and try add them to the enumeration result.
         */
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
            drvHostAudioCaDeviceDataInit(pDev, pDevIDs[i], enmUsage == PDMAUDIODIR_IN, pThis);

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
static bool drvHostAudioCaDevicesHasDevice(PPDMAUDIOHOSTENUM pEnmSrc, AudioDeviceID deviceID)
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
static int drvHostAudioCaDevicesEnumerateAll(PDRVHOSTCOREAUDIO pThis, PPDMAUDIOHOSTENUM pEnmDst)
{
    PDMAUDIOHOSTENUM devEnmIn;
    int rc = drvHostAudioCaDevicesEnumerate(pThis, PDMAUDIODIR_IN, &devEnmIn);
    if (RT_SUCCESS(rc))
    {
        PDMAUDIOHOSTENUM devEnmOut;
        rc = drvHostAudioCaDevicesEnumerate(pThis, PDMAUDIODIR_OUT, &devEnmOut);
        if (RT_SUCCESS(rc))
        {

/** @todo r=bird: This is an awfully complicated and inefficient way of doing
 * it.   Here you could just merge the two list (walk one, remove duplicates
 * from the other one) and skip all that duplication.
 *
 * Howerver, drvHostAudioCaDevicesEnumerate gets the device list twice, which is
 * a complete waste of time.  You could easily do all the work in
 * drvHostAudioCaDevicesEnumerate by just querying the IDs of both default
 * devices we're interested in, saving the merging extra allocations and
 * extra allocation.  */

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

                drvHostAudioCaDeviceDataInit(pDevDst, pDevSrcIn->deviceID, true /* fIsInput */, pThis);

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
                    if (drvHostAudioCaDevicesHasDevice(pEnmDst, pDevSrcOut->deviceID))
                        continue; /* Already in our list, skip. */

                    PCOREAUDIODEVICEDATA pDevDst = (PCOREAUDIODEVICEDATA)PDMAudioHostDevAlloc(sizeof(*pDevDst));
                    if (!pDevDst)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    drvHostAudioCaDeviceDataInit(pDevDst, pDevSrcOut->deviceID, false /* fIsInput */, pThis);

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
static int drvHostAudioCaDeviceRegisterCallbacks(PDRVHOSTCOREAUDIO pThis, PCOREAUDIODEVICEDATA pDev)
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
        OSStatus err = AudioObjectAddPropertyListener(deviceID, &PropAddr,
                                                      drvHostAudioCaDeviceIsAliveChangedCallback, pDev /*pvUser*/);
        if (   err != noErr
            && err != kAudioHardwareIllegalOperationError)
            LogRel(("CoreAudio: Failed to add the recording device state changed listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDeviceProcessorOverload;
        PropAddr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &PropAddr, drvHostAudioCaDevicePropertyChangedCallback, pDev /* pvUser */);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register processor overload listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyNominalSampleRate;
        PropAddr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &PropAddr, drvHostAudioCaDevicePropertyChangedCallback, pDev /* pvUser */);
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
static int drvHostAudioCaDeviceUnregisterCallbacks(PDRVHOSTCOREAUDIO pThis, PCOREAUDIODEVICEDATA pDev)
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
        OSStatus err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, drvHostAudioCaDevicePropertyChangedCallback, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the recording processor overload listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyNominalSampleRate;
        err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, drvHostAudioCaDevicePropertyChangedCallback, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the sample rate changed listener (%RI32)\n", err));

        PropAddr.mSelector = kAudioDevicePropertyDeviceIsAlive;
        err = AudioObjectRemovePropertyListener(deviceID, &PropAddr, drvHostAudioCaDeviceIsAliveChangedCallback, pDev /* pvUser */);
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
static int drvHostAudioCaEnumerateDevices(PDRVHOSTCOREAUDIO pThis)
{
    LogFlowFuncEnter();

    /*
     * Unregister old default devices, if any.
     */
    if (pThis->pDefaultDevIn)
    {
        drvHostAudioCaDeviceUnregisterCallbacks(pThis, pThis->pDefaultDevIn);
        pThis->pDefaultDevIn = NULL;
    }

    if (pThis->pDefaultDevOut)
    {
        drvHostAudioCaDeviceUnregisterCallbacks(pThis, pThis->pDefaultDevOut);
        pThis->pDefaultDevOut = NULL;
    }

    /* Remove old / stale device entries. */
    PDMAudioHostEnumDelete(&pThis->Devices);

    /* Enumerate all devices internally. */
    int rc = drvHostAudioCaDevicesEnumerateAll(pThis, &pThis->Devices);
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
            rc = drvHostAudioCaDeviceRegisterCallbacks(pThis, pThis->pDefaultDevIn);
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
            rc = drvHostAudioCaDeviceRegisterCallbacks(pThis, pThis->pDefaultDevOut);
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
static DECLCALLBACK(int) drvHostAudioCaHA_GetDevices(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum)
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
        rc = drvHostAudioCaEnumerateDevices(pThis);
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
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostAudioCaHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(pInterface, enmDir);
    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * Marks the given buffer as queued or not-queued.
 *
 * @returns Old queued value.
 * @param   pAudioBuffer    The buffer.
 * @param   fQueued         The new queued state.
 */
DECLINLINE(bool) drvHostAudioCaSetBufferQueued(AudioQueueBufferRef pAudioBuffer, bool fQueued)
{
    if (fQueued)
        return ASMAtomicBitTestAndSet(&pAudioBuffer->mUserData, 0);
    return ASMAtomicBitTestAndClear(&pAudioBuffer->mUserData, 0);
}


/**
 * Gets the queued state of the buffer.
 * @returns true if queued, false if not.
 * @param   pAudioBuffer    The buffer.
 */
DECLINLINE(bool) drvHostAudioCaIsBufferQueued(AudioQueueBufferRef pAudioBuffer)
{
    return ((uintptr_t)pAudioBuffer->mUserData & 1) == 1;
}


/**
 * Output audio queue buffer callback.
 *
 * Called whenever an audio queue is done processing a buffer.  This routine
 * will set the data fill size to zero and mark it as unqueued so that
 * drvHostAudioCaHA_StreamPlay knowns it can use it.
 *
 * @param   pvUser          User argument.
 * @param   hAudioQueue     Audio queue to process output data for.
 * @param   pAudioBuffer    Audio buffer to store output data in.
 *
 * @thread  queue thread.
 */
static void drvHostAudioCaOutputQueueBufferCallback(void *pvUser, AudioQueueRef hAudioQueue, AudioQueueBufferRef pAudioBuffer)
{
#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pvUser;
    AssertPtr(pStreamCA);
    Assert(pStreamCA->hAudioQueue == hAudioQueue);

    uintptr_t idxBuf = (uintptr_t)pAudioBuffer->mUserData >> 8;
    Log4Func(("Got back buffer #%zu (%p)\n", idxBuf, pAudioBuffer));
    AssertReturnVoid(   idxBuf < pStreamCA->cBuffers
                     && pStreamCA->paBuffers[idxBuf].pBuf == pAudioBuffer);
#endif

    pAudioBuffer->mAudioDataByteSize = 0;
    bool fWasQueued = drvHostAudioCaSetBufferQueued(pAudioBuffer, false /*fQueued*/);
    Assert(!drvHostAudioCaIsBufferQueued(pAudioBuffer));
    Assert(fWasQueued); RT_NOREF(fWasQueued);

    RT_NOREF(pvUser, hAudioQueue);
}


/**
 * Input audio queue buffer callback.
 *
 * Called whenever input data from the audio queue becomes available.  This
 * routine will mark the buffer unqueued so that drvHostAudioCaHA_StreamCapture
 * can read the data from it.
 *
 * @param   pvUser          User argument.
 * @param   hAudioQueue     Audio queue to process input data from.
 * @param   pAudioBuffer    Audio buffer to process input data from.
 * @param   pAudioTS        Audio timestamp.
 * @param   cPacketDesc     Number of packet descriptors.
 * @param   paPacketDesc    Array of packet descriptors.
 */
static void drvHostAudioCaInputQueueBufferCallback(void *pvUser, AudioQueueRef hAudioQueue,
                                                   AudioQueueBufferRef pAudioBuffer, const AudioTimeStamp *pAudioTS,
                                                   UInt32 cPacketDesc, const AudioStreamPacketDescription *paPacketDesc)
{
#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pvUser;
    AssertPtr(pStreamCA);
    Assert(pStreamCA->hAudioQueue == hAudioQueue);

    uintptr_t idxBuf = (uintptr_t)pAudioBuffer->mUserData >> 8;
    Log4Func(("Got back buffer #%zu (%p) with %#x bytes\n", idxBuf, pAudioBuffer, pAudioBuffer->mAudioDataByteSize));
    AssertReturnVoid(   idxBuf < pStreamCA->cBuffers
                     && pStreamCA->paBuffers[idxBuf].pBuf == pAudioBuffer);
#endif

    bool fWasQueued = drvHostAudioCaSetBufferQueued(pAudioBuffer, false /*fQueued*/);
    Assert(!drvHostAudioCaIsBufferQueued(pAudioBuffer));
    Assert(fWasQueued); RT_NOREF(fWasQueued);

    RT_NOREF(pvUser, hAudioQueue, pAudioTS, cPacketDesc, paPacketDesc);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTCOREAUDIO pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
    PCOREAUDIOSTREAM  pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);
    AssertReturn(pCfgReq->enmDir == PDMAUDIODIR_IN || pCfgReq->enmDir == PDMAUDIODIR_OUT, VERR_INVALID_PARAMETER);
    int rc;

    /** @todo This takes too long.  Stats indicates it may take up to 200 ms.
     *        Knoppix guest resets the stream and we hear nada because the
     *        draining is aborted when the stream is destroyed.  Should try use
     *        async init for parts (much) of this. */

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
    RTCritSectEnter(&pThis->CritSect);
    CFStringRef hDevUidStr = NULL;
    {
        PCOREAUDIODEVICEDATA pDev = pCfgReq->enmDir == PDMAUDIODIR_IN ? pThis->pDefaultDevIn : pThis->pDefaultDevOut;
        if (pDev)
        {
            Assert(pDev->Core.cbSelf == sizeof(*pDev));
            hDevUidStr = pDev->UUID;
            CFRetain(pDev->UUID);
        }
    }
    RTCritSectLeave(&pThis->CritSect);

#ifdef LOG_ENABLED
    char szTmp[PDMAUDIOSTRMCFGTOSTRING_MAX];
#endif
    LogFunc(("hDevUidStr=%p *pCfgReq: %s\n", hDevUidStr, PDMAudioStrmCfgToString(pCfgReq, szTmp, sizeof(szTmp)) ));
    if (hDevUidStr)
    {
        /*
         * Basic structure init.
         */
        pStreamCA->fEnabled         = false;
        pStreamCA->fStarted         = false;
        pStreamCA->fDraining        = false;
        pStreamCA->fRestartOnResume = false;
        pStreamCA->offInternal      = 0;
        pStreamCA->idxBuffer        = 0;
        pStreamCA->enmInitState     = COREAUDIOINITSTATE_IN_INIT;

        rc = RTCritSectInit(&pStreamCA->CritSect);
        if (RT_SUCCESS(rc))
        {
            /*
             * Do format conversion and create the circular buffer we use to shuffle
             * data to/from the queue thread.
             */
            PDMAudioStrmCfgCopy(&pStreamCA->Cfg, pCfgReq);
            drvHostAudioCaPCMPropsToASBD(&pCfgReq->Props, &pStreamCA->BasicStreamDesc);
            /** @todo Do some validation? */
            drvHostAudioCaPrintASBD(  pCfgReq->enmDir == PDMAUDIODIR_IN
                                    ? "Capturing queue format"
                                    : "Playback queue format", &pStreamCA->BasicStreamDesc);
            /*
             * Create audio queue.
             *
             * Documentation says the callbacks will be run on some core audio
             * related thread if we don't specify a runloop here.  That's simpler.
             */
#ifdef CORE_AUDIO_WITH_WORKER_THREAD
            CFRunLoopRef const hRunLoop     = pThis->hThreadRunLoop;
            CFStringRef  const hRunLoopMode = kCFRunLoopDefaultMode;
#else
            CFRunLoopRef const hRunLoop     = NULL;
            CFStringRef  const hRunLoopMode = NULL;
#endif
            OSStatus orc;
            if (pCfgReq->enmDir == PDMAUDIODIR_OUT)
                orc = AudioQueueNewOutput(&pStreamCA->BasicStreamDesc, drvHostAudioCaOutputQueueBufferCallback, pStreamCA,
                                          hRunLoop, hRunLoopMode, 0 /*fFlags - MBZ*/, &pStreamCA->hAudioQueue);
            else
                orc = AudioQueueNewInput(&pStreamCA->BasicStreamDesc, drvHostAudioCaInputQueueBufferCallback, pStreamCA,
                                         hRunLoop, hRunLoopMode, 0 /*fFlags - MBZ*/, &pStreamCA->hAudioQueue);
            LogFlowFunc(("AudioQueueNew%s -> %#x\n", pCfgReq->enmDir == PDMAUDIODIR_OUT ? "Output" : "Input", orc));
            if (orc == noErr)
            {
                /*
                 * Assign device to the queue.
                 */
                UInt32 uSize = sizeof(hDevUidStr);
                orc = AudioQueueSetProperty(pStreamCA->hAudioQueue, kAudioQueueProperty_CurrentDevice, &hDevUidStr, uSize);
                LogFlowFunc(("AudioQueueSetProperty -> %#x\n", orc));
                if (orc == noErr)
                {
                    /*
                     * Sanity-adjust the requested buffer size.
                     */
                    uint32_t cFramesBufferSizeMax = PDMAudioPropsMilliToFrames(&pStreamCA->Cfg.Props, 2 * RT_MS_1SEC);
                    uint32_t cFramesBufferSize    = PDMAudioPropsMilliToFrames(&pStreamCA->Cfg.Props, 32 /*ms*/);
                    cFramesBufferSize = RT_MAX(cFramesBufferSize, pCfgReq->Backend.cFramesBufferSize);
                    cFramesBufferSize = RT_MIN(cFramesBufferSize, cFramesBufferSizeMax);

                    /*
                     * The queue buffers size is based on cMsSchedulingHint so that we're likely to
                     * have a new one ready/done after each guest DMA transfer.  We must however
                     * make sure we don't end up with too may or too few.
                     */
                    Assert(pCfgReq->Device.cMsSchedulingHint > 0);
                    uint32_t cFramesQueueBuffer = PDMAudioPropsMilliToFrames(&pStreamCA->Cfg.Props,
                                                                             pCfgReq->Device.cMsSchedulingHint > 0
                                                                             ? pCfgReq->Device.cMsSchedulingHint : 10);
                    uint32_t cQueueBuffers;
                    if (cFramesQueueBuffer * COREAUDIO_MIN_BUFFERS <= cFramesBufferSize)
                    {
                        cQueueBuffers      = cFramesBufferSize / cFramesQueueBuffer;
                        if (cQueueBuffers > COREAUDIO_MAX_BUFFERS)
                        {
                            cQueueBuffers = COREAUDIO_MAX_BUFFERS;
                            cFramesQueueBuffer = cFramesBufferSize / COREAUDIO_MAX_BUFFERS;
                        }
                    }
                    else
                    {
                        cQueueBuffers      = COREAUDIO_MIN_BUFFERS;
                        cFramesQueueBuffer = cFramesBufferSize / COREAUDIO_MIN_BUFFERS;
                    }

                    cFramesBufferSize = cQueueBuffers * cFramesBufferSize;

                    /*
                     * Allocate the audio queue buffers.
                     */
                    pStreamCA->paBuffers = (PCOREAUDIOBUF)RTMemAllocZ(sizeof(pStreamCA->paBuffers[0]) * cQueueBuffers);
                    if (pStreamCA->paBuffers != NULL)
                    {
                        pStreamCA->cBuffers = cQueueBuffers;

                        const size_t cbQueueBuffer = PDMAudioPropsFramesToBytes(&pStreamCA->Cfg.Props, cFramesQueueBuffer);
                        LogFlowFunc(("Allocating %u, each %#x bytes / %u frames\n", cQueueBuffers, cbQueueBuffer, cFramesQueueBuffer));
                        cFramesBufferSize = 0;
                        for (uint32_t iBuf = 0; iBuf < cQueueBuffers; iBuf++)
                        {
                            AudioQueueBufferRef pBuf = NULL;
                            orc = AudioQueueAllocateBuffer(pStreamCA->hAudioQueue, cbQueueBuffer, &pBuf);
                            if (RT_LIKELY(orc == noErr))
                            {
                                pBuf->mUserData = (void *)(uintptr_t)(iBuf << 8); /* bit zero is the queued-indicator. */
                                pStreamCA->paBuffers[iBuf].pBuf = pBuf;
                                cFramesBufferSize += PDMAudioPropsBytesToFrames(&pStreamCA->Cfg.Props,
                                                                                pBuf->mAudioDataBytesCapacity);
                                Assert(PDMAudioPropsIsSizeAligned(&pStreamCA->Cfg.Props, pBuf->mAudioDataBytesCapacity));
                            }
                            else
                            {
                                LogRel(("CoreAudio: Out of memory (buffer %#x out of %#x, %#x bytes)\n",
                                        iBuf, cQueueBuffers, cbQueueBuffer));
                                while (iBuf-- > 0)
                                {
                                    AudioQueueFreeBuffer(pStreamCA->hAudioQueue, pStreamCA->paBuffers[iBuf].pBuf);
                                    pStreamCA->paBuffers[iBuf].pBuf = NULL;
                                }
                                break;
                            }
                        }
                        if (orc == noErr)
                        {
                            /*
                             * Update the stream config.
                             */
                            pStreamCA->Cfg.Backend.cFramesBufferSize   = cFramesBufferSize;
                            pStreamCA->Cfg.Backend.cFramesPeriod       = cFramesQueueBuffer; /* whatever */
                            pStreamCA->Cfg.Backend.cFramesPreBuffering =   pStreamCA->Cfg.Backend.cFramesPreBuffering
                                                                         * pStreamCA->Cfg.Backend.cFramesBufferSize
                                                                       / RT_MAX(pCfgReq->Backend.cFramesBufferSize, 1);

                            PDMAudioStrmCfgCopy(pCfgAcq, &pStreamCA->Cfg);

                            ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_INIT);

                            LogFunc(("returns VINF_SUCCESS\n"));
                            CFRelease(hDevUidStr);
                            return VINF_SUCCESS;
                        }

                        RTMemFree(pStreamCA->paBuffers);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
                else
                    LogRelMax(64, ("CoreAudio: Failed to associate device with queue: %#x (%d)\n", orc, orc));
                AudioQueueDispose(pStreamCA->hAudioQueue, TRUE /*inImmediate*/);
            }
            else
                LogRelMax(64, ("CoreAudio: Failed to create audio queue: %#x (%d)\n", orc, orc));
            RTCritSectDelete(&pStreamCA->CritSect);
        }
        else
            LogRel(("CoreAudio: Failed to initialize critical section for stream: %Rrc\n", rc));
        CFRelease(hDevUidStr);
    }
    else
    {
        LogRelMax(64, ("CoreAudio: No device for %s stream.\n", PDMAudioDirGetName(pCfgReq->enmDir)));
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    }

    LogFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                        bool fImmediate)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    LogFunc(("%p: %s fImmediate=%RTbool\n", pStreamCA, pStreamCA->Cfg.szName, fImmediate));
#ifdef LOG_ENABLED
    uint64_t const nsStart = RTTimeNanoTS();
#endif

    /*
     * Never mind if the status isn't INIT (it should always be, though).
     */
    COREAUDIOINITSTATE const enmInitState = (COREAUDIOINITSTATE)ASMAtomicReadU32(&pStreamCA->enmInitState);
    AssertMsg(enmInitState == COREAUDIOINITSTATE_INIT, ("%d\n", enmInitState));
    if (enmInitState == COREAUDIOINITSTATE_INIT)
    {
        Assert(RTCritSectIsInitialized(&pStreamCA->CritSect));

        /*
         * Change the stream state and stop the stream (just to be sure).
         */
        OSStatus orc;
        ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_IN_UNINIT);
        if (pStreamCA->hAudioQueue)
        {
            orc = AudioQueueStop(pStreamCA->hAudioQueue, fImmediate ? TRUE : FALSE /*inImmediate/synchronously*/);
            LogFlowFunc(("AudioQueueStop -> %#x\n", orc));
        }

        /*
         * Enter and leave the critsect afterwards for paranoid reasons.
         */
        RTCritSectEnter(&pStreamCA->CritSect);
        RTCritSectLeave(&pStreamCA->CritSect);

        /*
         * Free the queue buffers and the queue.
         *
         * This may take a while.  The AudioQueueReset call seems to helps
         * reducing time stuck in AudioQueueDispose.
         */
#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
        LogRel(("Queue-destruction timer starting...\n"));
        PDRVHOSTCOREAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio);
        RTTimerLRStart(pThis->hBreakpointTimer, RT_NS_100MS);
        uint64_t nsStart = RTTimeNanoTS();
#endif

#if 0 /* This seems to work even when doing a non-immediate stop&dispose. However, it doesn't make sense conceptually. */
        if (pStreamCA->hAudioQueue /*&& fImmediate*/)
        {
            LogFlowFunc(("Calling AudioQueueReset ...\n"));
            orc = AudioQueueReset(pStreamCA->hAudioQueue);
            LogFlowFunc(("AudioQueueReset -> %#x\n", orc));
        }
#endif

        if (pStreamCA->paBuffers && fImmediate)
        {
            LogFlowFunc(("Freeing %u buffers ...\n", pStreamCA->cBuffers));
            for (uint32_t iBuf = 0; iBuf < pStreamCA->cBuffers; iBuf++)
            {
                orc = AudioQueueFreeBuffer(pStreamCA->hAudioQueue, pStreamCA->paBuffers[iBuf].pBuf);
                AssertMsg(orc == noErr, ("AudioQueueFreeBuffer(#%u) -> orc=%#x\n", iBuf, orc));
                pStreamCA->paBuffers[iBuf].pBuf = NULL;
            }
        }

        if (pStreamCA->hAudioQueue)
        {
            LogFlowFunc(("Disposing of the queue ...\n"));
            orc = AudioQueueDispose(pStreamCA->hAudioQueue, fImmediate ? TRUE : FALSE /*inImmediate/synchronously*/); /* may take some time */
            LogFlowFunc(("AudioQueueDispose -> %#x (%d)\n", orc, orc));
            AssertMsg(orc == noErr, ("AudioQueueDispose -> orc=%#x\n", orc));
            pStreamCA->hAudioQueue = NULL;
        }

        /* We should get no further buffer callbacks at this point according to the docs. */
        if (pStreamCA->paBuffers)
        {
            RTMemFree(pStreamCA->paBuffers);
            pStreamCA->paBuffers = NULL;
        }
        pStreamCA->cBuffers = 0;

#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
        RTTimerLRStop(pThis->hBreakpointTimer);
        LogRel(("Queue-destruction: %'RU64\n", RTTimeNanoTS() - nsStart));
#endif

        /*
         * Delete the critsect and we're done.
         */
        RTCritSectDelete(&pStreamCA->CritSect);

        ASMAtomicWriteU32(&pStreamCA->enmInitState, COREAUDIOINITSTATE_UNINIT);
    }
    else
        LogFunc(("Wrong stream init state for %p: %d - leaking it\n", pStream, enmInitState));

    LogFunc(("returns (took %'RU64 ns)\n", RTTimeNanoTS() - nsStart));
    return VINF_SUCCESS;
}


#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
/** @callback_method_impl{FNRTTIMERLR, For debugging things that takes too long.} */
static DECLCALLBACK(void) drvHostAudioCaBreakpointTimer(RTTIMERLR hTimer, void *pvUser, uint64_t iTick)
{
    LogFlowFunc(("Queue-destruction timeout! iTick=%RU64\n", iTick));
    RT_NOREF(hTimer, pvUser, iTick);
    RTLogFlush(NULL);
    RT_BREAKPOINT();
}
#endif


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    LogFlowFunc(("Stream '%s' {%s}\n", pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA)));
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, VERR_AUDIO_STREAM_NOT_READY);
    RTCritSectEnter(&pStreamCA->CritSect);

    Assert(!pStreamCA->fEnabled);
    Assert(!pStreamCA->fStarted);

    /*
     * We always reset the buffer before enabling the stream (normally never necessary).
     */
    OSStatus orc = AudioQueueReset(pStreamCA->hAudioQueue);
    if (orc != noErr)
        LogRelMax(64, ("CoreAudio: Stream reset failed when enabling '%s': %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
    Assert(orc == noErr);
    for (uint32_t iBuf = 0; iBuf < pStreamCA->cBuffers; iBuf++)
        Assert(!drvHostAudioCaIsBufferQueued(pStreamCA->paBuffers[iBuf].pBuf));

    pStreamCA->offInternal      = 0;
    pStreamCA->fDraining        = false;
    pStreamCA->fEnabled         = true;
    pStreamCA->fRestartOnResume = false;
    pStreamCA->idxBuffer        = 0;

    /*
     * Input streams will start capturing, while output streams will only start
     * playing once we get some audio data to play (see drvHostAudioCaHA_StreamPlay).
     */
    int rc = VINF_SUCCESS;
    if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        /* Zero (probably not needed) and submit all the buffers first. */
        for (uint32_t iBuf = 0; iBuf < pStreamCA->cBuffers; iBuf++)
        {
            AudioQueueBufferRef pBuf = pStreamCA->paBuffers[iBuf].pBuf;

            RT_BZERO(pBuf->mAudioData, pBuf->mAudioDataBytesCapacity);
            pBuf->mAudioDataByteSize = 0;
            drvHostAudioCaSetBufferQueued(pBuf, true /*fQueued*/);

            orc = AudioQueueEnqueueBuffer(pStreamCA->hAudioQueue, pBuf, 0 /*inNumPacketDescs*/, NULL /*inPacketDescs*/);
            AssertLogRelMsgBreakStmt(orc == noErr, ("CoreAudio: AudioQueueEnqueueBuffer(#%u) -> %#x (%d) - stream '%s'\n",
                                                    iBuf, orc, orc, pStreamCA->Cfg.szName),
                                     drvHostAudioCaSetBufferQueued(pBuf, false /*fQueued*/));
        }

        /* Start the stream. */
        if (orc == noErr)
        {
            LogFlowFunc(("Start input stream '%s'...\n", pStreamCA->Cfg.szName));
            orc = AudioQueueStart(pStreamCA->hAudioQueue, NULL /*inStartTime*/);
            AssertLogRelMsgStmt(orc == noErr, ("CoreAudio: AudioQueueStart(%s) -> %#x (%d) \n", pStreamCA->Cfg.szName, orc, orc),
                                rc = VERR_AUDIO_STREAM_NOT_READY);
            pStreamCA->fStarted = orc == noErr;
        }
        else
            rc = VERR_AUDIO_STREAM_NOT_READY;
    }
    else
        Assert(pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT);

    RTCritSectLeave(&pStreamCA->CritSect);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    LogFlowFunc(("cMsLastTransfer=%RI64 ms, stream '%s' {%s} \n",
                 pStreamCA->msLastTransfer ? RTTimeMilliTS() - pStreamCA->msLastTransfer : -1,
                 pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA) ));
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, VERR_AUDIO_STREAM_NOT_READY);
    RTCritSectEnter(&pStreamCA->CritSect);

    /*
     * Always stop it (draining or no).
     */
    pStreamCA->fEnabled         = false;
    pStreamCA->fRestartOnResume = false;
    Assert(!pStreamCA->fDraining || pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT);

    int rc = VINF_SUCCESS;
    if (pStreamCA->fStarted)
    {
#if 0
        OSStatus orc2 = AudioQueueReset(pStreamCA->hAudioQueue);
        LogFlowFunc(("AudioQueueReset(%s) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc2, orc2)); RT_NOREF(orc2);
        orc2 = AudioQueueFlush(pStreamCA->hAudioQueue);
        LogFlowFunc(("AudioQueueFlush(%s) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc2, orc2)); RT_NOREF(orc2);
#endif

        OSStatus orc = AudioQueueStop(pStreamCA->hAudioQueue, TRUE /*inImmediate*/);
        LogFlowFunc(("AudioQueueStop(%s,TRUE) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
        if (orc != noErr)
        {
            LogRelMax(64, ("CoreAudio: Stopping '%s' failed (disable): %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
            rc = VERR_GENERAL_FAILURE;
        }
        pStreamCA->fStarted  = false;
        pStreamCA->fDraining = false;
    }

    RTCritSectLeave(&pStreamCA->CritSect);
    LogFlowFunc(("returns %Rrc {%s}\n", rc, drvHostAudioCaStreamStatusString(pStreamCA)));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    LogFlowFunc(("cMsLastTransfer=%RI64 ms, stream '%s' {%s} \n",
                 pStreamCA->msLastTransfer ? RTTimeMilliTS() - pStreamCA->msLastTransfer : -1,
                 pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA) ));
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, VERR_AUDIO_STREAM_NOT_READY);
    RTCritSectEnter(&pStreamCA->CritSect);

    /*
     * Unless we're draining the stream, pause it if it has started.
     */
    int rc = VINF_SUCCESS;
    if (pStreamCA->fStarted && !pStreamCA->fDraining)
    {
        pStreamCA->fRestartOnResume = true;

        OSStatus orc = AudioQueuePause(pStreamCA->hAudioQueue);
        LogFlowFunc(("AudioQueuePause(%s) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
        if (orc != noErr)
        {
            LogRelMax(64, ("CoreAudio: Pausing '%s' failed: %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
            rc = VERR_GENERAL_FAILURE;
        }
        pStreamCA->fStarted = false;
    }
    else
    {
        pStreamCA->fRestartOnResume = false;
        if (pStreamCA->fDraining)
        {
            LogFunc(("Stream '%s' is draining\n", pStreamCA->Cfg.szName));
            Assert(pStreamCA->fStarted);
        }
    }

    RTCritSectLeave(&pStreamCA->CritSect);
    LogFlowFunc(("returns %Rrc {%s}\n", rc, drvHostAudioCaStreamStatusString(pStreamCA)));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    LogFlowFunc(("Stream '%s' {%s}\n", pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA)));
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, VERR_AUDIO_STREAM_NOT_READY);
    RTCritSectEnter(&pStreamCA->CritSect);

    /*
     * Resume according to state saved by drvHostAudioCaHA_StreamPause.
     */
    int rc = VINF_SUCCESS;
    if (pStreamCA->fRestartOnResume)
    {
        OSStatus orc = AudioQueueStart(pStreamCA->hAudioQueue, NULL /*inStartTime*/);
        LogFlowFunc(("AudioQueueStart(%s, NULL) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
        if (orc != noErr)
        {
            LogRelMax(64, ("CoreAudio: Pausing '%s' failed: %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
            rc = VERR_AUDIO_STREAM_NOT_READY;
        }
    }
    pStreamCA->fRestartOnResume = false;

    RTCritSectLeave(&pStreamCA->CritSect);
    LogFlowFunc(("returns %Rrc {%s}\n", rc, drvHostAudioCaStreamStatusString(pStreamCA)));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDrain}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertReturn(pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT, VERR_INVALID_PARAMETER);
    LogFlowFunc(("cMsLastTransfer=%RI64 ms, stream '%s' {%s} \n",
                 pStreamCA->msLastTransfer ? RTTimeMilliTS() - pStreamCA->msLastTransfer : -1,
                 pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA) ));
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, VERR_AUDIO_STREAM_NOT_READY);
    RTCritSectEnter(&pStreamCA->CritSect);

    /*
     * The AudioQueueStop function has both an immediate and a drain mode,
     * so we'll obviously use the latter here.  For checking draining progress,
     * we will just check if all buffers have been returned or not.
     */
    int rc = VINF_SUCCESS;
    if (pStreamCA->fStarted)
    {
        if (!pStreamCA->fDraining)
        {
            OSStatus orc = AudioQueueStop(pStreamCA->hAudioQueue, FALSE /*inImmediate*/);
            LogFlowFunc(("AudioQueueStop(%s, FALSE) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
            if (orc == noErr)
                pStreamCA->fDraining = true;
            else
            {
                LogRelMax(64, ("CoreAudio: Stopping '%s' failed (drain): %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
                rc = VERR_GENERAL_FAILURE;
            }
        }
        else
            LogFlowFunc(("Already draining '%s' ...\n", pStreamCA->Cfg.szName));
    }
    else
    {
        LogFlowFunc(("Drain requested for '%s', but not started playback...\n", pStreamCA->Cfg.szName));
        AssertStmt(!pStreamCA->fDraining, pStreamCA->fDraining = false);
    }

    RTCritSectLeave(&pStreamCA->CritSect);
    LogFlowFunc(("returns %Rrc {%s}\n", rc, drvHostAudioCaStreamStatusString(pStreamCA)));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostAudioCaHA_StreamEnable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostAudioCaHA_StreamDisable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostAudioCaHA_StreamPause(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostAudioCaHA_StreamResume(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostAudioCaHA_StreamDrain(pInterface, pStream);

        case PDMAUDIOSTREAMCMD_END:
        case PDMAUDIOSTREAMCMD_32BIT_HACK:
        case PDMAUDIOSTREAMCMD_INVALID:
            /* no default*/
            break;
    }
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostAudioCaHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, 0);

    uint32_t cbReadable = 0;
    if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        RTCritSectEnter(&pStreamCA->CritSect);
        PCOREAUDIOBUF const paBuffers = pStreamCA->paBuffers;
        uint32_t const      cBuffers  = pStreamCA->cBuffers;
        uint32_t const      idxStart  = pStreamCA->idxBuffer;
        uint32_t            idxBuffer = idxStart;
        AudioQueueBufferRef pBuf;

        if (   cBuffers > 0
            && !drvHostAudioCaIsBufferQueued(pBuf = paBuffers[idxBuffer].pBuf))
        {
            do
            {
                uint32_t const  cbTotal = pBuf->mAudioDataBytesCapacity;
                uint32_t        cbFill  = pBuf->mAudioDataByteSize;
                AssertStmt(cbFill <= cbTotal, cbFill = cbTotal);
                uint32_t        off     = paBuffers[idxBuffer].offRead;
                AssertStmt(off < cbFill, off = cbFill);

                cbReadable += cbFill - off;

                /* Advance. */
                idxBuffer++;
                if (idxBuffer < cBuffers)
                { /* likely */ }
                else
                    idxBuffer = 0;
            } while (idxBuffer != idxStart && !drvHostAudioCaIsBufferQueued(pBuf = paBuffers[idxBuffer].pBuf));
        }

        RTCritSectLeave(&pStreamCA->CritSect);
    }
    Log2Func(("returns %#x for '%s'\n", cbReadable, pStreamCA->Cfg.szName));
    return cbReadable;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostAudioCaHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertReturn(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, 0);

    uint32_t cbWritable = 0;
    if (pStreamCA->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        RTCritSectEnter(&pStreamCA->CritSect);
        PCOREAUDIOBUF const paBuffers = pStreamCA->paBuffers;
        uint32_t const      cBuffers  = pStreamCA->cBuffers;
        uint32_t const      idxStart  = pStreamCA->idxBuffer;
        uint32_t            idxBuffer = idxStart;
        AudioQueueBufferRef pBuf;

        if (   cBuffers > 0
            && !drvHostAudioCaIsBufferQueued(pBuf = paBuffers[idxBuffer].pBuf))
        {
            do
            {
                uint32_t const  cbTotal = pBuf->mAudioDataBytesCapacity;
                uint32_t        cbUsed  = pBuf->mAudioDataByteSize;
                AssertStmt(cbUsed <= cbTotal, paBuffers[idxBuffer].pBuf->mAudioDataByteSize = cbUsed = cbTotal);

                cbWritable += cbTotal - cbUsed;

                /* Advance. */
                idxBuffer++;
                if (idxBuffer < cBuffers)
                { /* likely */ }
                else
                    idxBuffer = 0;
            } while (idxBuffer != idxStart && !drvHostAudioCaIsBufferQueued(pBuf = paBuffers[idxBuffer].pBuf));
        }

        RTCritSectLeave(&pStreamCA->CritSect);
    }
    Log2Func(("returns %#x for '%s'\n", cbWritable, pStreamCA->Cfg.szName));
    return cbWritable;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostAudioCaHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                             PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, PDMHOSTAUDIOSTREAMSTATE_INVALID);

    if (ASMAtomicReadU32(&pStreamCA->enmInitState) == COREAUDIOINITSTATE_INIT)
    {
        if (!pStreamCA->fDraining)
        { /* likely */ }
        else
        {
            /*
             * If we're draining, we're done when we've got all the buffers back.
             */
            RTCritSectEnter(&pStreamCA->CritSect);
            PCOREAUDIOBUF const paBuffers = pStreamCA->paBuffers;
            uintptr_t           idxBuffer = pStreamCA->cBuffers;
            while (idxBuffer-- > 0)
                if (!drvHostAudioCaIsBufferQueued(paBuffers[idxBuffer].pBuf))
                { /* likely */ }
                else
                {
#ifdef LOG_ENABLED
                    uint32_t cQueued = 1;
                    while (idxBuffer-- > 0)
                        cQueued += drvHostAudioCaIsBufferQueued(paBuffers[idxBuffer].pBuf);
                    LogFunc(("Still done draining '%s': %u queued buffers\n", pStreamCA->Cfg.szName, cQueued));
#endif
                    RTCritSectLeave(&pStreamCA->CritSect);
                    return PDMHOSTAUDIOSTREAMSTATE_DRAINING;
                }

            LogFunc(("Done draining '%s'\n", pStreamCA->Cfg.szName));
            pStreamCA->fDraining = false;
            pStreamCA->fEnabled  = false;
            pStreamCA->fStarted  = false;
            RTCritSectLeave(&pStreamCA->CritSect);
        }

        return PDMHOSTAUDIOSTREAMSTATE_OKAY;
    }
    return PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING; /** @todo ?? */
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                     const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);
    if (cbBuf)
        AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    Assert(PDMAudioPropsIsSizeAligned(&pStreamCA->Cfg.Props, cbBuf));
    AssertReturnStmt(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, *pcbWritten = 0, VERR_AUDIO_STREAM_NOT_READY);

    RTCritSectEnter(&pStreamCA->CritSect);
    if (pStreamCA->fEnabled)
    { /* likely */ }
    else
    {
        RTCritSectLeave(&pStreamCA->CritSect);
        *pcbWritten = 0;
        LogFunc(("Skipping %#x byte write to disabled stream {%s}\n", cbBuf, drvHostAudioCaStreamStatusString(pStreamCA) ));
        return VINF_SUCCESS;
    }
    Log4Func(("cbBuf=%#x stream '%s' {%s}\n", cbBuf, pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA) ));

    /*
     * Transfer loop.
     */
    PCOREAUDIOBUF const paBuffers = pStreamCA->paBuffers;
    uint32_t const      cBuffers  = pStreamCA->cBuffers;
    AssertMsgReturnStmt(cBuffers >= COREAUDIO_MIN_BUFFERS && cBuffers < COREAUDIO_MAX_BUFFERS, ("%u\n", cBuffers),
                        RTCritSectLeave(&pStreamCA->CritSect), VERR_AUDIO_STREAM_NOT_READY);

    uint32_t            idxBuffer = pStreamCA->idxBuffer;
    AssertStmt(idxBuffer < cBuffers, idxBuffer %= cBuffers);

    int                 rc        = VINF_SUCCESS;
    uint32_t            cbWritten = 0;
    while (cbBuf > 0)
    {
        AssertBreakStmt(pStreamCA->hAudioQueue, rc = VERR_AUDIO_STREAM_NOT_READY);

        /*
         * Check out how much we can put into the current buffer.
         */
        AudioQueueBufferRef const pBuf = paBuffers[idxBuffer].pBuf;
        if (!drvHostAudioCaIsBufferQueued(pBuf))
        { /* likely */ }
        else
        {
            LogFunc(("@%#RX64: Warning! Out of buffer space! (%#x bytes unwritten)\n", pStreamCA->offInternal, cbBuf));
            /** @todo stats   */
            break;
        }

        AssertPtrBreakStmt(pBuf, rc = VERR_INTERNAL_ERROR_2);
        uint32_t const  cbTotal = pBuf->mAudioDataBytesCapacity;
        uint32_t        cbUsed  = pBuf->mAudioDataByteSize;
        AssertStmt(cbUsed < cbTotal, cbUsed = cbTotal);
        uint32_t const  cbAvail = cbTotal - cbUsed;

        /*
         * Copy over the data.
         */
        if (cbBuf < cbAvail)
        {
            Log3Func(("@%#RX64: buffer #%u/%u: %#x bytes, have %#x only - leaving unqueued {%s}\n",
                      pStreamCA->offInternal, idxBuffer, cBuffers, cbAvail, cbBuf, drvHostAudioCaStreamStatusString(pStreamCA) ));
            memcpy((uint8_t *)pBuf->mAudioData + cbUsed, pvBuf, cbBuf);
            pBuf->mAudioDataByteSize = cbUsed + cbBuf;
            cbWritten               += cbBuf;
            pStreamCA->offInternal  += cbBuf;
            /** @todo Maybe queue it anyway if it's almost full or we haven't got a lot of
             *        buffers queued. */
            break;
        }

        Log3Func(("@%#RX64: buffer #%u/%u: %#x bytes, have %#x - will queue {%s}\n",
                  pStreamCA->offInternal, idxBuffer, cBuffers, cbAvail, cbBuf, drvHostAudioCaStreamStatusString(pStreamCA) ));
        memcpy((uint8_t *)pBuf->mAudioData + cbUsed, pvBuf, cbAvail);
        pBuf->mAudioDataByteSize = cbTotal;
        cbWritten               += cbAvail;
        pStreamCA->offInternal  += cbAvail;
        drvHostAudioCaSetBufferQueued(pBuf, true /*fQueued*/);

        OSStatus orc = AudioQueueEnqueueBuffer(pStreamCA->hAudioQueue, pBuf, 0 /*inNumPacketDesc*/, NULL /*inPacketDescs*/);
        if (orc == noErr)
        { /* likely */ }
        else
        {
            LogRelMax(256, ("CoreAudio: AudioQueueEnqueueBuffer('%s', #%u) failed: %#x (%d)\n",
                            pStreamCA->Cfg.szName, idxBuffer, orc, orc));
            drvHostAudioCaSetBufferQueued(pBuf, false /*fQueued*/);
            pBuf->mAudioDataByteSize -= PDMAudioPropsFramesToBytes(&pStreamCA->Cfg.Props, 1); /* avoid assertions above */
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        /*
         * Advance.
         */
        idxBuffer += 1;
        if (idxBuffer < cBuffers)
        { /* likely */ }
        else
            idxBuffer = 0;
        pStreamCA->idxBuffer = idxBuffer;

        pvBuf  = (const uint8_t *)pvBuf + cbAvail;
        cbBuf -= cbAvail;
    }

    /*
     * Start the stream if we haven't do so yet.
     */
    if (   pStreamCA->fStarted
        || cbWritten == 0
        || RT_FAILURE_NP(rc))
    { /* likely */ }
    else
    {
        UInt32   cFramesPrepared = 0;
#if 0 /* taking too long? */
        OSStatus orc = AudioQueuePrime(pStreamCA->hAudioQueue, 0 /*inNumberOfFramesToPrepare*/, &cFramesPrepared);
        LogFlowFunc(("AudioQueuePrime(%s, 0,) returns %#x (%d) and cFramesPrepared=%u (offInternal=%#RX64)\n",
                     pStreamCA->Cfg.szName, orc, orc, cFramesPrepared, pStreamCA->offInternal));
        AssertMsg(orc == noErr, ("%#x (%d)\n", orc, orc));
#else
        OSStatus orc;
#endif
        orc = AudioQueueStart(pStreamCA->hAudioQueue, NULL /*inStartTime*/);
        LogFunc(("AudioQueueStart(%s, NULL) returns %#x (%d)\n", pStreamCA->Cfg.szName, orc, orc));
        if (orc == noErr)
            pStreamCA->fStarted = true;
        else
        {
            LogRelMax(128, ("CoreAudio: Starting '%s' failed: %#x (%d) - %u frames primed, %#x bytes queued\n",
                            pStreamCA->Cfg.szName, orc, orc, cFramesPrepared, pStreamCA->offInternal));
            rc = VERR_AUDIO_STREAM_NOT_READY;
        }
    }

    /*
     * Done.
     */
#ifdef LOG_ENABLED
    uint64_t const msPrev = pStreamCA->msLastTransfer;
#endif
    uint64_t const msNow  = RTTimeMilliTS();
    if (cbWritten)
        pStreamCA->msLastTransfer = msNow;

    RTCritSectLeave(&pStreamCA->CritSect);

    *pcbWritten = cbWritten;
    if (RT_SUCCESS(rc) || !cbWritten)
    { }
    else
    {
        LogFlowFunc(("Suppressing %Rrc to report %#x bytes written\n", rc, cbWritten));
        rc = VINF_SUCCESS;
    }
    LogFlowFunc(("@%#RX64: rc=%Rrc cbWritten=%RU32 cMsDelta=%RU64 (%RU64 -> %RU64) {%s}\n", pStreamCA->offInternal, rc, cbWritten,
                 msPrev ? msNow - msPrev : 0, msPrev, pStreamCA->msLastTransfer, drvHostAudioCaStreamStatusString(pStreamCA) ));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostAudioCaHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                        void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface);
    PCOREAUDIOSTREAM pStreamCA = (PCOREAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamCA, 0);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);
    Assert(PDMAudioPropsIsSizeAligned(&pStreamCA->Cfg.Props, cbBuf));
    AssertReturnStmt(pStreamCA->enmInitState == COREAUDIOINITSTATE_INIT, *pcbRead = 0, VERR_AUDIO_STREAM_NOT_READY);

    RTCritSectEnter(&pStreamCA->CritSect);
    if (pStreamCA->fEnabled)
    { /* likely */ }
    else
    {
        RTCritSectLeave(&pStreamCA->CritSect);
        *pcbRead = 0;
        LogFunc(("Skipping %#x byte read from disabled stream {%s}\n", cbBuf, drvHostAudioCaStreamStatusString(pStreamCA)));
        return VINF_SUCCESS;
    }
    Log4Func(("cbBuf=%#x stream '%s' {%s}\n", cbBuf, pStreamCA->Cfg.szName, drvHostAudioCaStreamStatusString(pStreamCA) ));


    /*
     * Transfer loop.
     */
    uint32_t const      cbFrame   = PDMAudioPropsFrameSize(&pStreamCA->Cfg.Props);
    PCOREAUDIOBUF const paBuffers = pStreamCA->paBuffers;
    uint32_t const      cBuffers  = pStreamCA->cBuffers;
    AssertMsgReturnStmt(cBuffers >= COREAUDIO_MIN_BUFFERS && cBuffers < COREAUDIO_MAX_BUFFERS, ("%u\n", cBuffers),
                        RTCritSectLeave(&pStreamCA->CritSect), VERR_AUDIO_STREAM_NOT_READY);

    uint32_t            idxBuffer = pStreamCA->idxBuffer;
    AssertStmt(idxBuffer < cBuffers, idxBuffer %= cBuffers);

    int                 rc        = VINF_SUCCESS;
    uint32_t            cbRead    = 0;
    while (cbBuf > cbFrame)
    {
        AssertBreakStmt(pStreamCA->hAudioQueue, rc = VERR_AUDIO_STREAM_NOT_READY);

        /*
         * Check out how much we can read from the current buffer (if anything at all).
         */
        AudioQueueBufferRef const pBuf = paBuffers[idxBuffer].pBuf;
        if (!drvHostAudioCaIsBufferQueued(pBuf))
        { /* likely */ }
        else
        {
            LogFunc(("@%#RX64: Warning! Underrun! (%#x bytes unread)\n", pStreamCA->offInternal, cbBuf));
            /** @todo stats   */
            break;
        }

        AssertPtrBreakStmt(pBuf, rc = VERR_INTERNAL_ERROR_2);
        uint32_t const  cbTotal = pBuf->mAudioDataBytesCapacity;
        uint32_t        cbValid = pBuf->mAudioDataByteSize;
        AssertStmt(cbValid < cbTotal, cbValid = cbTotal);
        uint32_t        offRead = paBuffers[idxBuffer].offRead;
        uint32_t const  cbLeft  = cbValid - offRead;

        /*
         * Copy over the data.
         */
        if (cbBuf < cbLeft)
        {
            Log3Func(("@%#RX64: buffer #%u/%u: %#x bytes, want %#x - leaving unqueued {%s}\n",
                      pStreamCA->offInternal, idxBuffer, cBuffers, cbLeft, cbBuf, drvHostAudioCaStreamStatusString(pStreamCA) ));
            memcpy(pvBuf, (uint8_t const *)pBuf->mAudioData + offRead, cbBuf);
            paBuffers[idxBuffer].offRead = offRead + cbBuf;
            cbRead                      += cbBuf;
            pStreamCA->offInternal      += cbBuf;
            break;
        }

        Log3Func(("@%#RX64: buffer #%u/%u: %#x bytes, want all (%#x) - will queue {%s}\n",
                  pStreamCA->offInternal, idxBuffer, cBuffers, cbLeft, cbBuf, drvHostAudioCaStreamStatusString(pStreamCA) ));
        memcpy(pvBuf, (uint8_t const *)pBuf->mAudioData + offRead, cbLeft);
        cbRead                  += cbLeft;
        pStreamCA->offInternal  += cbLeft;

        RT_BZERO(pBuf->mAudioData, cbTotal); /* paranoia */
        paBuffers[idxBuffer].offRead = 0;
        pBuf->mAudioDataByteSize     = 0;
        drvHostAudioCaSetBufferQueued(pBuf, true /*fQueued*/);

        OSStatus orc = AudioQueueEnqueueBuffer(pStreamCA->hAudioQueue, pBuf, 0 /*inNumPacketDesc*/, NULL /*inPacketDescs*/);
        if (orc == noErr)
        { /* likely */ }
        else
        {
            LogRelMax(256, ("CoreAudio: AudioQueueEnqueueBuffer('%s', #%u) failed: %#x (%d)\n",
                            pStreamCA->Cfg.szName, idxBuffer, orc, orc));
            drvHostAudioCaSetBufferQueued(pBuf, false /*fQueued*/);
            rc = VERR_AUDIO_STREAM_NOT_READY;
            break;
        }

        /*
         * Advance.
         */
        idxBuffer += 1;
        if (idxBuffer < cBuffers)
        { /* likely */ }
        else
            idxBuffer = 0;
        pStreamCA->idxBuffer = idxBuffer;

        pvBuf  = (uint8_t *)pvBuf + cbLeft;
        cbBuf -= cbLeft;
    }

    /*
     * Done.
     */
#ifdef LOG_ENABLED
    uint64_t const msPrev = pStreamCA->msLastTransfer;
#endif
    uint64_t const msNow  = RTTimeMilliTS();
    if (cbRead)
        pStreamCA->msLastTransfer = msNow;

    RTCritSectLeave(&pStreamCA->CritSect);

    *pcbRead = cbRead;
    if (RT_SUCCESS(rc) || !cbRead)
    { }
    else
    {
        LogFlowFunc(("Suppressing %Rrc to report %#x bytes read\n", rc, cbRead));
        rc = VINF_SUCCESS;
    }
    LogFlowFunc(("@%#RX64: rc=%Rrc cbRead=%RU32 cMsDelta=%RU64 (%RU64 -> %RU64) {%s}\n", pStreamCA->offInternal, rc, cbRead,
                 msPrev ? msNow - msPrev : 0, msPrev, pStreamCA->msLastTransfer, drvHostAudioCaStreamStatusString(pStreamCA) ));
    return rc;
}


/*********************************************************************************************************************************
*   PDMIBASE                                                                                                                     *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostAudioCaQueryInterface(PPDMIBASE pInterface, const char *pszIID)
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
static void drvHostAudioCaRemoveDefaultDeviceListners(PDRVHOSTCOREAUDIO pThis)
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
        orc = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &PropAddr, drvHostAudioCaDefaultDeviceChangedCallback, pThis);
        if (   orc != noErr
            && orc != kAudioHardwareBadObjectError)
            LogRel(("CoreAudio: Failed to remove the default input device changed listener: %d (%#x))\n", orc, orc));
        pThis->fRegisteredDefaultInputListener = false;
    }

    if (pThis->fRegisteredDefaultOutputListener)
    {

        PropAddr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        orc = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &PropAddr, drvHostAudioCaDefaultDeviceChangedCallback, pThis);
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
static DECLCALLBACK(void) drvHostAudioCaPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);
    drvHostAudioCaRemoveDefaultDeviceListners(pThis);
}


/**
 * @callback_method_impl{FNPDMDRVDESTRUCT}
 */
static DECLCALLBACK(void) drvHostAudioCaDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    drvHostAudioCaRemoveDefaultDeviceListners(pThis);

#ifdef CORE_AUDIO_WITH_WORKER_THREAD
    if (pThis->hThread != NIL_RTTHREAD)
    {
        for (unsigned iLoop = 0; iLoop < 60; iLoop++)
        {
            if (pThis->hThreadRunLoop)
                CFRunLoopStop(pThis->hThreadRunLoop);
            if (iLoop > 10)
                RTThreadPoke(pThis->hThread);
            int rc = RTThreadWait(pThis->hThread, 500 /*ms*/, NULL /*prcThread*/);
            if (RT_SUCCESS(rc))
                break;
            AssertMsgBreak(rc == VERR_TIMEOUT, ("RTThreadWait -> %Rrc\n",rc));
        }
        pThis->hThread = NIL_RTTHREAD;
    }
    if (pThis->hThreadPortSrc)
    {
        CFRelease(pThis->hThreadPortSrc);
        pThis->hThreadPortSrc = NULL;
    }
    if (pThis->hThreadPort)
    {
        CFMachPortInvalidate(pThis->hThreadPort);
        CFRelease(pThis->hThreadPort);
        pThis->hThreadPort = NULL;
    }
    if (pThis->hThreadRunLoop)
    {
        CFRelease(pThis->hThreadRunLoop);
        pThis->hThreadRunLoop = NULL;
    }
#endif

#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
    if (pThis->hBreakpointTimer != NIL_RTTIMERLR)
    {
        RTTimerLRDestroy(pThis->hBreakpointTimer);
        pThis->hBreakpointTimer = NIL_RTTIMERLR;
    }
#endif

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc2);
}


/**
 * @callback_method_impl{FNPDMDRVCONSTRUCT,
 *      Construct a Core Audio driver instance.}
 */
static DECLCALLBACK(int) drvHostAudioCaConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);
    LogRel(("Audio: Initializing Core Audio driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
#ifdef CORE_AUDIO_WITH_WORKER_THREAD
    pThis->hThread                   = NIL_RTTHREAD;
#endif
#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
    pThis->hBreakpointTimer          = NIL_RTTIMERLR;
#endif
    PDMAudioHostEnumInit(&pThis->Devices);
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostAudioCaQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig                  = drvHostAudioCaHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = drvHostAudioCaHA_GetDevices;
    pThis->IHostAudio.pfnGetStatus                  = drvHostAudioCaHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostAudioCaHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostAudioCaHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostAudioCaHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostAudioCaHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostAudioCaHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostAudioCaHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostAudioCaHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostAudioCaHA_StreamCapture;

    int rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);

#ifdef CORE_AUDIO_WITH_WORKER_THREAD
    /*
     * Create worker thread for running callbacks on.
     */
    CFMachPortContext PortCtx;
    PortCtx.version         = 0;
    PortCtx.info            = pThis;
    PortCtx.retain          = NULL;
    PortCtx.release         = NULL;
    PortCtx.copyDescription = NULL;
    pThis->hThreadPort = CFMachPortCreate(NULL /*allocator*/, drvHostAudioCaThreadPortCallback, &PortCtx, NULL);
    AssertLogRelReturn(pThis->hThreadPort != NULL, VERR_NO_MEMORY);

    pThis->hThreadPortSrc = CFMachPortCreateRunLoopSource(NULL, pThis->hThreadPort, 0 /*order*/);
    AssertLogRelReturn(pThis->hThreadPortSrc != NULL, VERR_NO_MEMORY);

    rc = RTThreadCreateF(&pThis->hThread, drvHostAudioCaThread, pThis, 0, RTTHREADTYPE_IO,
                         RTTHREADFLAGS_WAITABLE, "CaAud-%u", pDrvIns->iInstance);
    AssertLogRelMsgReturn(RT_SUCCESS(rc), ("RTThreadCreateF failed: %Rrc\n", rc), rc);

    RTThreadUserWait(pThis->hThread, RT_MS_10SEC);
    AssertLogRel(pThis->hThreadRunLoop);
#endif

#ifdef CORE_AUDIO_WITH_BREAKPOINT_TIMER
    /*
     * Create a IPRT timer.  The TM timers won't necessarily work as EMT is probably busy.
     */
    rc = RTTimerLRCreateEx(&pThis->hBreakpointTimer, 0 /*no interval*/, 0, drvHostAudioCaBreakpointTimer, pThis);
    AssertRCReturn(rc, rc);
#endif

    /*
     * Enumerate audio devices.
     */
    rc = drvHostAudioCaEnumerateDevices(pThis);
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

    OSStatus orc = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &PropAddr, drvHostAudioCaDefaultDeviceChangedCallback, pThis);
    pThis->fRegisteredDefaultInputListener = orc == noErr;
    if (   orc != noErr
        && orc != kAudioHardwareIllegalOperationError)
        LogRel(("CoreAudio: Failed to add the input default device changed listener: %d (%#x)\n", orc, orc));

    PropAddr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    orc = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &PropAddr, drvHostAudioCaDefaultDeviceChangedCallback, pThis);
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
    drvHostAudioCaConstruct,
    /* pfnDestruct */
    drvHostAudioCaDestruct,
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
    drvHostAudioCaPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

