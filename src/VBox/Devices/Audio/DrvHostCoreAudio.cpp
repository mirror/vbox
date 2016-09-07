/* $Id$ */
/** @file
 * VBox audio devices - Mac OS X CoreAudio audio driver.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>
#include <iprt/mem.h>

#include <iprt/uuid.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioConverter.h>

#if 0
# include <iprt/file.h>
# define DEBUG_DUMP_PCM_DATA
# ifdef RT_OS_WINDOWS
#  define DEBUG_DUMP_PCM_DATA_PATH "c:\\temp\\"
# else
#  define DEBUG_DUMP_PCM_DATA_PATH "/tmp/"
# endif
#endif

/* Enables utilizing the Core Audio converter unit for converting
 * input / output from/to our requested formats. That might be more
 * performant than using our own routines later down the road. */
/** @todo Needs more investigation and testing first before enabling. */
//# define VBOX_WITH_AUDIO_CA_CONVERTER

#ifdef DEBUG_andy
# undef  DEBUG_DUMP_PCM_DATA_PATH
# define DEBUG_DUMP_PCM_DATA_PATH "/Users/anloeffl/Documents/"
# undef  VBOX_WITH_AUDIO_CA_CONVERTER
#endif

/** @todo
 * - Maybe make sure the threads are immediately stopped if playing/recording stops.
 */

/*
 * Most of this is based on:
 * http://developer.apple.com/mac/library/technotes/tn2004/tn2097.html
 * http://developer.apple.com/mac/library/technotes/tn2002/tn2091.html
 * http://developer.apple.com/mac/library/qa/qa2007/qa1533.html
 * http://developer.apple.com/mac/library/qa/qa2001/qa1317.html
 * http://developer.apple.com/mac/library/documentation/AudioUnit/Reference/AUComponentServicesReference/Reference/reference.html
 */

/* Prototypes needed for COREAUDIODEVICE. */
struct DRVHOSTCOREAUDIO;
typedef struct DRVHOSTCOREAUDIO *PDRVHOSTCOREAUDIO;

/**
 * Structure for holding Core Audio-specific device data.
 * This data then lives in the pvData part of the PDMAUDIODEVICE struct.
 */
typedef struct COREAUDIODEVICEDATA
{
    /** Pointer to driver instance this device is bound to. */
    PDRVHOSTCOREAUDIO pDrv;
    /** The audio device ID of the currently used device (UInt32 typedef). */
    AudioDeviceID     deviceID;
    /** List of attached (native) Core Audio streams attached to this device. */
    RTLISTANCHOR      lstStreams;
} COREAUDIODEVICEDATA, *PCOREAUDIODEVICEDATA;

/**
 * Host Coreaudio driver instance data.
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
    PDMAUDIODEVICEENUM      Devices;
    /** Pointer to the currently used input device in the device enumeration.
     *  Can be NULL if none assigned. */
    PPDMAUDIODEVICE         pDefaultDevIn;
    /** Pointer to the currently used output device in the device enumeration.
     *  Can be NULL if none assigned. */
    PPDMAUDIODEVICE         pDefaultDevOut;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /** Callback function to the upper driver.
     *  Can be NULL if not being used / registered. */
    PFNPDMHOSTAUDIOCALLBACK pfnCallback;
#endif
} DRVHOSTCOREAUDIO, *PDRVHOSTCOREAUDIO;

/** Converts a pointer to DRVHOSTCOREAUDIO::IHostAudio to a PDRVHOSTCOREAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface) RT_FROM_MEMBER(pInterface, DRVHOSTCOREAUDIO, IHostAudio)

/**
 * Structure for holding a Core Audio unit
 * and its data.
 */
typedef struct COREAUDIOUNIT
{
    /** Pointer to the device this audio unit is bound to.
     *  Can be NULL if not bound to a device (anymore). */
    PPDMAUDIODEVICE             pDevice;
    /** The actual audio unit object. */
    AudioUnit                   audioUnit;
    /** Stream description for using with VBox:
     *  - When using this audio unit for input (capturing), this format states
     *    the unit's output format.
     *  - When using this audio unit for output (playback), this format states
     *    the unit's input format. */
    AudioStreamBasicDescription streamFmt;
} COREAUDIOUNIT, *PCOREAUDIOUNIT;

/*******************************************************************************
 *
 * Helper function section
 *
 ******************************************************************************/

/* Move these down below the internal function prototypes... */

static void coreAudioPrintASBD(const char *pszDesc, const AudioStreamBasicDescription *pASBD)
{
    char pszSampleRate[32];
    LogRel2(("CoreAudio: %s description:\n", pszDesc));
    LogRel2(("CoreAudio:\tFormat ID: %RU32 (%c%c%c%c)\n", pASBD->mFormatID,
             RT_BYTE4(pASBD->mFormatID), RT_BYTE3(pASBD->mFormatID),
             RT_BYTE2(pASBD->mFormatID), RT_BYTE1(pASBD->mFormatID)));
    LogRel2(("CoreAudio:\tFlags: %RU32", pASBD->mFormatFlags));
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
    snprintf(pszSampleRate, 32, "%.2f", (float)pASBD->mSampleRate); /** @todo r=andy Use RTStrPrint*. */
    LogRel2(("CoreAudio:\tSampleRate      : %s\n", pszSampleRate));
    LogRel2(("CoreAudio:\tChannelsPerFrame: %RU32\n", pASBD->mChannelsPerFrame));
    LogRel2(("CoreAudio:\tFramesPerPacket : %RU32\n", pASBD->mFramesPerPacket));
    LogRel2(("CoreAudio:\tBitsPerChannel  : %RU32\n", pASBD->mBitsPerChannel));
    LogRel2(("CoreAudio:\tBytesPerFrame   : %RU32\n", pASBD->mBytesPerFrame));
    LogRel2(("CoreAudio:\tBytesPerPacket  : %RU32\n", pASBD->mBytesPerPacket));
}

static void coreAudioPCMPropsToASBD(PDMAUDIOPCMPROPS *pPCMProps, AudioStreamBasicDescription *pASBD)
{
    AssertPtrReturnVoid(pPCMProps);
    AssertPtrReturnVoid(pASBD);

    RT_BZERO(pASBD, sizeof(AudioStreamBasicDescription));

    pASBD->mFormatID         = kAudioFormatLinearPCM;
    pASBD->mFormatFlags      = kAudioFormatFlagIsPacked;
    pASBD->mFramesPerPacket  = 1; /* For uncompressed audio, set this to 1. */
    pASBD->mSampleRate       = (Float64)pPCMProps->uHz;
    pASBD->mChannelsPerFrame = pPCMProps->cChannels;
    pASBD->mBitsPerChannel   = pPCMProps->cBits;
    if (pPCMProps->fSigned)
        pASBD->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    pASBD->mBytesPerFrame    = pASBD->mChannelsPerFrame * (pASBD->mBitsPerChannel / 8);
    pASBD->mBytesPerPacket   = pASBD->mFramesPerPacket * pASBD->mBytesPerFrame;
}

static int coreAudioStreamCfgToASBD(PPDMAUDIOSTREAMCFG pCfg, AudioStreamBasicDescription *pASBD)
{
    AssertPtrReturn(pCfg,  VERR_INVALID_PARAMETER);
    AssertPtrReturn(pASBD, VERR_INVALID_PARAMETER);

    PDMAUDIOPCMPROPS Props;
    int rc = DrvAudioHlpStreamCfgToProps(pCfg, &Props);
    if (RT_SUCCESS(rc))
        coreAudioPCMPropsToASBD(&Props, pASBD);

    return rc;
}

static int coreAudioASBDToStreamCfg(AudioStreamBasicDescription *pASBD, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pASBD, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pCfg,  VERR_INVALID_PARAMETER);

    pCfg->cChannels     = pASBD->mChannelsPerFrame;
    pCfg->uHz           = (uint32_t)pASBD->mSampleRate;
    pCfg->enmEndianness = PDMAUDIOENDIANNESS_LITTLE;

    int rc = VINF_SUCCESS;

    if (pASBD->mFormatFlags & kAudioFormatFlagIsSignedInteger)
    {
        switch (pASBD->mBitsPerChannel)
        {
            case 8:  pCfg->enmFormat = PDMAUDIOFMT_S8;  break;
            case 16: pCfg->enmFormat = PDMAUDIOFMT_S16; break;
            case 32: pCfg->enmFormat = PDMAUDIOFMT_S32; break;
            default: rc = VERR_NOT_SUPPORTED;           break;
        }
    }
    else
    {
        switch (pASBD->mBitsPerChannel)
        {
            case 8:  pCfg->enmFormat = PDMAUDIOFMT_U8;  break;
            case 16: pCfg->enmFormat = PDMAUDIOFMT_U16; break;
            case 32: pCfg->enmFormat = PDMAUDIOFMT_U32; break;
            default: rc = VERR_NOT_SUPPORTED;           break;
        }
    }

    AssertRC(rc);
    return rc;
}

static OSStatus coreAudioSetFrameBufferSize(AudioDeviceID deviceID, bool fInput, UInt32 cReqSize, UInt32 *pcActSize)
{
    AudioObjectPropertyScope propScope = fInput
                                       ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    AudioObjectPropertyAddress propAdr = { kAudioDevicePropertyBufferFrameSize, propScope,
                                           kAudioObjectPropertyElementMaster };

    /* First try to set the new frame buffer size. */
    OSStatus err = AudioObjectSetPropertyData(deviceID, &propAdr, 0, NULL, sizeof(cReqSize), &cReqSize);

    /* Check if it really was set. */
    UInt32 cSize = sizeof(*pcActSize);
    err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &cSize, pcActSize);
    if (RT_UNLIKELY(err != noErr))
        return err;

    /* If both sizes are the same, we are done. */
    if (cReqSize == *pcActSize)
        return noErr;

    /* If not we have to check the limits of the device. First get the size of
       the buffer size range property. */
    propAdr.mSelector = kAudioDevicePropertyBufferSizeRange;
    err = AudioObjectGetPropertyDataSize(deviceID, &propAdr, 0, NULL, &cSize);
    if (RT_UNLIKELY(err != noErr))
        return err;

    Assert(cSize);
    AudioValueRange *pRange = (AudioValueRange *)RTMemAllocZ(cSize);
    if (pRange)
    {
        err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &cSize, pRange);
        if (err == noErr)
        {
            Float64 cMin = -1;
            Float64 cMax = -1;
            for (size_t a = 0; a < cSize / sizeof(AudioValueRange); a++)
            {
                /* Search for the absolute minimum. */
                if (   pRange[a].mMinimum < cMin
                    || cMin == -1)
                    cMin = pRange[a].mMinimum;

                /* Search for the best maximum which isn't bigger than cReqSize. */
                if (pRange[a].mMaximum < cReqSize)
                {
                    if (pRange[a].mMaximum > cMax)
                        cMax = pRange[a].mMaximum;
                }
            }
            if (cMax == -1)
                cMax = cMin;
            cReqSize = cMax;

            /* First try to set the new frame buffer size. */
            propAdr.mSelector = kAudioDevicePropertyBufferFrameSize;
            err = AudioObjectSetPropertyData(deviceID, &propAdr, 0, NULL, sizeof(cReqSize), &cReqSize);
            if (err == noErr)
            {
                /* Check if it really was set. */
                cSize = sizeof(*pcActSize);
                err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &cSize, pcActSize);
            }
        }

        RTMemFree(pRange);
    }
    else
        err = notEnoughMemoryErr;

    return err;
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
    AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDeviceForUID, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };

    UInt32 uSize = sizeof(AudioValueTranslation);
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdr, 0, NULL, &uSize, &translation);

    /* Release the temporary CFString */
    CFRelease(strUID);

    if (RT_LIKELY(err == noErr))
        return deviceID;

    /* Return the unknown device on error. */
    return kAudioDeviceUnknown;
}
#endif /* unused */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** @name Initialization status indicator used for the recreation of the AudioUnits.
 *
 * Global structures section
 *
 ******************************************************************************/

/**
 * Enumeration for a Core Audio stream status.
 */
typedef enum COREAUDIOSTATUS
{
    /** The device is uninitialized. */
    COREAUDIOSTATUS_UNINIT  = 0,
    /** The device is currently initializing. */
    COREAUDIOSTATUS_IN_INIT,
    /** The device is initialized. */
    COREAUDIOSTATUS_INIT,
    /** The device is currently uninitializing. */
    COREAUDIOSTATUS_IN_UNINIT,
#ifndef VBOX_WITH_AUDIO_CALLBACKS
    /** The device has to be reinitialized.
     *  Note: Only needed if VBOX_WITH_AUDIO_CALLBACKS is not defined, as otherwise
     *        the Audio Connector will take care of this as soon as this backend
     *        tells it to do so via the provided audio callback. */
    COREAUDIOSTATUS_REINIT,
#endif
    /** The usual 32-bit hack. */
    COREAUDIOSTATUS_32BIT_HACK = 0x7fffffff
} COREAUDIOSTATUS, *PCOREAUDIOSTATUS;

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
 /* Error code which indicates "End of data" */
 static const OSStatus caConverterEOFDErr = 0x656F6664; /* 'eofd' */
#endif

/* Prototypes needed for COREAUDIOSTREAMCBCTX. */
struct COREAUDIOSTREAM;
typedef struct COREAUDIOSTREAM *PCOREAUDIOSTREAM;

/**
 * Structure for keeping a conversion callback context.
 * This is needed when using an audio converter during input/output processing.
 */
typedef struct COREAUDIOCONVCBCTX
{
    /** Pointer to the stream this context is bound to. */
    PCOREAUDIOSTREAM             pStream;
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
} COREAUDIOCONVCBCTX, *PCOREAUDIOCONVCBCTX;

/**
 * Structure for keeping the input stream specifics.
 */
typedef struct COREAUDIOSTREAMIN
{
#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
    /** The audio converter if necessary. NULL if no converter is being used. */
    AudioConverterRef           ConverterRef;
    /** Callback context for the audio converter. */
    COREAUDIOCONVCBCTX          convCbCtx;
#endif
    /** The ratio between the device & the stream sample rate. */
    Float64                     sampleRatio;
} COREAUDIOSTREAMIN, *PCOREAUDIOSTREAMIN;

/**
 * Structure for keeping the output stream specifics.
 */
typedef struct COREAUDIOSTREAMOUT
{
    /** Nothing here yet. */
} COREAUDIOSTREAMOUT, *PCOREAUDIOSTREAMOUT;

/**
 * Structure for maintaining a Core Audio stream.
 */
typedef struct COREAUDIOSTREAM
{
    /** Host input stream.
     *  Note: Always must come first in this structure! */
    PDMAUDIOSTREAM          Stream;
    /** Note: This *always* must come first! */
    union
    {
        COREAUDIOSTREAMIN   In;
        COREAUDIOSTREAMOUT  Out;
    };
    /** List node for the device's stream list. */
    RTLISTNODE              Node;
    /** Pointer to driver instance this stream is bound to. */
    PDRVHOSTCOREAUDIO       pDrv;
    /** The stream's direction. */
    PDMAUDIODIR             enmDir;
    /** The audio unit for this stream. */
    COREAUDIOUNIT           Unit;
    /** Initialization status tracker. Used when some of the device parameters
     *  or the device itself is changed during the runtime. */
    volatile uint32_t       enmStatus;
    /** An internal ring buffer for transferring data from/to the rendering callbacks. */
    PRTCIRCBUF              pCircBuf;
} COREAUDIOSTREAM, *PCOREAUDIOSTREAM;

static int coreAudioStreamInit(PCOREAUDIOSTREAM pCAStream, PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev);
#ifndef VBOX_WITH_AUDIO_CALLBACKS
static int coreAudioStreamReinit(PDRVHOSTCOREAUDIO pThis, PCOREAUDIOSTREAM pCAStream, PPDMAUDIODEVICE pDev);
#endif
static int coreAudioStreamUninit(PCOREAUDIOSTREAM pCAStream);

static int coreAudioStreamInitIn(PCOREAUDIOSTREAM pCAStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq);
static int coreAudioStreamInitOut(PCOREAUDIOSTREAM pCAStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq);

static int coreAudioStreamControl(PDRVHOSTCOREAUDIO pThis, PCOREAUDIOSTREAM pCAStream, PDMAUDIOSTREAMCMD enmStreamCmd);

static int coreAudioDeviceRegisterCallbacks(PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev);
static int coreAudioDeviceUnregisterCallbacks(PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev);
static void coreAudioDeviceDataInit(PCOREAUDIODEVICEDATA pDevData, AudioDeviceID deviceID, PDRVHOSTCOREAUDIO pDrv);

static OSStatus coreAudioDevPropChgCb(AudioObjectID propertyID, UInt32 nAddresses, const AudioObjectPropertyAddress properties[], void *pvUser);
static OSStatus coreAudioPlaybackCb(void *pvUser, AudioUnitRenderActionFlags *pActionFlags, const AudioTimeStamp *pAudioTS, UInt32 uBusID, UInt32 cFrames, AudioBufferList* pBufData);


/**
 * Returns whether an assigned audio unit to a given stream is running or not.
 *
 * @return  True if audio unit is running, false if not.
 * @param   pStream             Audio stream to check.
 */
static bool coreAudioUnitIsRunning(PCOREAUDIOSTREAM pStream)
{
    AssertPtrReturn(pStream, false);

    UInt32 uFlag = 0;
    UInt32 uSize = sizeof(uFlag);
    OSStatus err = AudioUnitGetProperty(pStream->Unit.audioUnit, kAudioOutputUnitProperty_IsRunning, kAudioUnitScope_Global,
                                        0, &uFlag, &uSize);
    if (err != kAudioHardwareNoError)
        LogRel(("CoreAudio: Could not determine whether the audio unit is running (%RI32)\n", err));

    Log3Func(("%s -> %RU32\n", pStream->enmDir == PDMAUDIODIR_IN ? "Input" : "Output", uFlag));

    return (uFlag >= 1);
}


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

#ifdef DEBUG
    coreAudioPrintASBD("CbCtx: Src", pASBDSrc);
    coreAudioPrintASBD("CbCtx: Dst", pASBDDst);
#endif

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
 * Does a (re-)enumeration of the host's playback + recording devices.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   enmUsage            Which devices to enumerate.
 * @param   pDevEnm             Where to store the enumerated devices.
 */
static int coreAudioDevicesEnumerate(PDRVHOSTCOREAUDIO pThis, PDMAUDIODIR enmUsage, PPDMAUDIODEVICEENUM pDevEnm)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pDevEnm, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    do
    {
        AudioDeviceID defaultDeviceID = kAudioDeviceUnknown;

        /* Fetch the default audio device currently in use. */
        AudioObjectPropertyAddress propAdrDefaultDev = {   enmUsage == PDMAUDIODIR_IN
                                                         ? kAudioHardwarePropertyDefaultInputDevice
                                                         : kAudioHardwarePropertyDefaultOutputDevice,
                                                         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        UInt32 uSize = sizeof(defaultDeviceID);
        OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdrDefaultDev, 0, NULL, &uSize, &defaultDeviceID);
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

        AudioObjectPropertyAddress propAdrDevList = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                                      kAudioObjectPropertyElementMaster };

        err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propAdrDevList, 0, NULL, &uSize);
        if (err != kAudioHardwareNoError)
            break;

        AudioDeviceID *pDevIDs = (AudioDeviceID *)alloca(uSize);
        if (pDevIDs == NULL)
            break;

        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdrDevList, 0, NULL, &uSize, pDevIDs);
        if (err != kAudioHardwareNoError)
            break;

        rc = DrvAudioHlpDeviceEnumInit(pDevEnm);
        if (RT_FAILURE(rc))
            break;

        UInt16 cDevices = uSize / sizeof(AudioDeviceID);

        PPDMAUDIODEVICE pDev = NULL;
        for (UInt16 i = 0; i < cDevices; i++)
        {
            if (pDev) /* Some (skipped) device to clean up first? */
                DrvAudioHlpDeviceFree(pDev);

            pDev = DrvAudioHlpDeviceAlloc(sizeof(COREAUDIODEVICEDATA));
            if (!pDev)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            /* Set usage. */
            pDev->enmUsage = enmUsage;

            /* Init backend-specific device data. */
            PCOREAUDIODEVICEDATA pDevData = (PCOREAUDIODEVICEDATA)pDev->pvData;
            AssertPtr(pDevData);
            coreAudioDeviceDataInit(pDevData, pDevIDs[i], pThis);

            /* Check if the device is valid. */
            AudioDeviceID curDevID = pDevData->deviceID;

            AudioObjectPropertyAddress propAddrCfg = { kAudioDevicePropertyStreamConfiguration,
                                                         enmUsage == PDMAUDIODIR_IN
                                                       ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                                                       kAudioObjectPropertyElementMaster };

            err = AudioObjectGetPropertyDataSize(curDevID, &propAddrCfg, 0, NULL, &uSize);
            if (err != noErr)
                continue;

            AudioBufferList *pBufList = (AudioBufferList *)RTMemAlloc(uSize);
            if (!pBufList)
                continue;

            err = AudioObjectGetPropertyData(curDevID, &propAddrCfg, 0, NULL, &uSize, pBufList);
            if (err == noErr)
            {
                for (UInt32 a = 0; a < pBufList->mNumberBuffers; a++)
                {
                    if (enmUsage == PDMAUDIODIR_IN)
                        pDev->cMaxInputChannels  += pBufList->mBuffers[a].mNumberChannels;
                    else if (enmUsage == PDMAUDIODIR_OUT)
                        pDev->cMaxOutputChannels += pBufList->mBuffers[a].mNumberChannels;
                }
            }

            if (pBufList)
            {
                RTMemFree(pBufList);
                pBufList = NULL;
            }

            /* Check if the device is valid, e.g. has any input/output channels according to its usage. */
            if (   enmUsage == PDMAUDIODIR_IN
                && !pDev->cMaxInputChannels)
            {
                continue;
            }
            else if (   enmUsage == PDMAUDIODIR_OUT
                     && !pDev->cMaxOutputChannels)
            {
                continue;
            }

            /* Resolve the device's name. */
            AudioObjectPropertyAddress propAddrName = { kAudioObjectPropertyName,
                                                          enmUsage == PDMAUDIODIR_IN
                                                        ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                                                        kAudioObjectPropertyElementMaster };
            uSize = sizeof(CFStringRef);
            CFStringRef pcfstrName = NULL;

            err = AudioObjectGetPropertyData(curDevID, &propAddrName, 0, NULL, &uSize, &pcfstrName);
            if (err != kAudioHardwareNoError)
                continue;

            CFIndex uMax = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pcfstrName), kCFStringEncodingUTF8) + 1;
            if (uMax)
            {
                char *pszName = (char *)RTStrAlloc(uMax);
                if (   pszName
                    && CFStringGetCString(pcfstrName, pszName, uMax, kCFStringEncodingUTF8))
                {
                    RTStrPrintf(pDev->szName, sizeof(pDev->szName), "%s", pszName);
                }

                LogFunc(("Device '%s': %RU32\n", pszName, curDevID));

                if (pszName)
                {
                    RTStrFree(pszName);
                    pszName = NULL;
                }
            }

            CFRelease(pcfstrName);

            /* Adjust flags. */
            if (curDevID == defaultDeviceID) /* Is the device the default device? */
                pDev->fFlags |= PDMAUDIODEV_FLAGS_DEFAULT;

            /* Add the device to the enumeration. */
            rc = DrvAudioHlpDeviceEnumAdd(pDevEnm, pDev);
            if (RT_FAILURE(rc))
                break;

            /* NULL device pointer because it's now part of the device enumeration. */
            pDev = NULL;
        }

        if (RT_FAILURE(rc))
        {
            DrvAudioHlpDeviceFree(pDev);
            pDev = NULL;
        }

    } while (0);

    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG
        LogFunc(("Devices for pDevEnm=%p, enmUsage=%RU32:\n", pDevEnm, enmUsage));
        DrvAudioHlpDeviceEnumPrint("Core Audio", pDevEnm);
#endif
    }
    else
        DrvAudioHlpDeviceEnumFree(pDevEnm);

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
bool coreAudioDevicesHasDevice(PPDMAUDIODEVICEENUM pEnmSrc, AudioDeviceID deviceID)
{
    PPDMAUDIODEVICE pDevSrc;
    RTListForEach(&pEnmSrc->lstDevices, pDevSrc, PDMAUDIODEVICE, Node)
    {
        PCOREAUDIODEVICEDATA pDevSrcData = (PCOREAUDIODEVICEDATA)pDevSrc->pvData;
        AssertPtr(pDevSrcData);

        if (pDevSrcData->deviceID == deviceID)
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
int coreAudioDevicesEnumerateAll(PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICEENUM pEnmDst)
{
    PDMAUDIODEVICEENUM devEnmIn;
    int rc = coreAudioDevicesEnumerate(pThis, PDMAUDIODIR_IN, &devEnmIn);
    if (RT_SUCCESS(rc))
    {
        PDMAUDIODEVICEENUM devEnmOut;
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

            rc = DrvAudioHlpDeviceEnumInit(pEnmDst);
            if (RT_SUCCESS(rc))
            {
                PPDMAUDIODEVICE pDevSrcIn;
                RTListForEach(&devEnmIn.lstDevices, pDevSrcIn, PDMAUDIODEVICE, Node)
                {
                    PCOREAUDIODEVICEDATA pDevSrcInData = (PCOREAUDIODEVICEDATA)pDevSrcIn->pvData;
                    AssertPtr(pDevSrcInData);

                    PPDMAUDIODEVICE pDevDst = DrvAudioHlpDeviceAlloc(sizeof(COREAUDIODEVICEDATA));
                    if (!pDevDst)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    PCOREAUDIODEVICEDATA pDevDstData = (PCOREAUDIODEVICEDATA)pDevDst->pvData;
                    AssertPtr(pDevDstData);
                    coreAudioDeviceDataInit(pDevDstData, pDevSrcInData->deviceID, pThis);

                    RTStrCopy(pDevDst->szName, sizeof(pDevDst->szName), pDevSrcIn->szName);

                    pDevDst->enmUsage          = PDMAUDIODIR_IN; /* Input device by default (simplex). */
                    pDevDst->cMaxInputChannels = pDevSrcIn->cMaxInputChannels;

                    /* Handle flags. */
                    if (pDevSrcIn->fFlags & PDMAUDIODEV_FLAGS_DEFAULT)
                        pDevDst->fFlags |= PDMAUDIODEV_FLAGS_DEFAULT;
                    /** @todo Handle hot plugging? */

                    /*
                     * Now search through the list of all found output devices and check if we found
                     * an output device with the same device ID as the currently handled input device.
                     *
                     * If found, this means we have to treat that device as a duplex device then.
                     */
                    PPDMAUDIODEVICE pDevSrcOut;
                    RTListForEach(&devEnmOut.lstDevices, pDevSrcOut, PDMAUDIODEVICE, Node)
                    {
                        PCOREAUDIODEVICEDATA pDevSrcOutData = (PCOREAUDIODEVICEDATA)pDevSrcOut->pvData;
                        AssertPtr(pDevSrcOutData);

                        if (pDevSrcInData->deviceID == pDevSrcOutData->deviceID)
                        {
                            pDevDst->enmUsage           = PDMAUDIODIR_ANY;
                            pDevDst->cMaxOutputChannels = pDevSrcOut->cMaxOutputChannels;
                            break;
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        rc = DrvAudioHlpDeviceEnumAdd(pEnmDst, pDevDst);
                    }
                    else
                    {
                        DrvAudioHlpDeviceFree(pDevDst);
                        pDevDst = NULL;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    /*
                     * As a last step, add all remaining output devices which have not been handled in the loop above,
                     * that is, all output devices which operate in simplex mode.
                     */
                    PPDMAUDIODEVICE pDevSrcOut;
                    RTListForEach(&devEnmOut.lstDevices, pDevSrcOut, PDMAUDIODEVICE, Node)
                    {
                        PCOREAUDIODEVICEDATA pDevSrcOutData = (PCOREAUDIODEVICEDATA)pDevSrcOut->pvData;
                        AssertPtr(pDevSrcOutData);

                        if (coreAudioDevicesHasDevice(pEnmDst, pDevSrcOutData->deviceID))
                            continue; /* Already in our list, skip. */

                        PPDMAUDIODEVICE pDevDst = DrvAudioHlpDeviceAlloc(sizeof(COREAUDIODEVICEDATA));
                        if (!pDevDst)
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }

                        PCOREAUDIODEVICEDATA pDevDstData = (PCOREAUDIODEVICEDATA)pDevDst->pvData;
                        AssertPtr(pDevDstData);
                        coreAudioDeviceDataInit(pDevDstData, pDevSrcOutData->deviceID, pThis);

                        RTStrCopy(pDevDst->szName, sizeof(pDevDst->szName), pDevSrcOut->szName);

                        pDevDst->enmUsage           = PDMAUDIODIR_OUT;
                        pDevDst->cMaxOutputChannels = pDevSrcOut->cMaxOutputChannels;

                        pDevDstData->deviceID       = pDevSrcOutData->deviceID;

                        /* Handle flags. */
                        if (pDevSrcOut->fFlags & PDMAUDIODEV_FLAGS_DEFAULT)
                            pDevDst->fFlags |= PDMAUDIODEV_FLAGS_DEFAULT;
                        /** @todo Handle hot plugging? */

                        rc = DrvAudioHlpDeviceEnumAdd(pEnmDst, pDevDst);
                        if (RT_FAILURE(rc))
                        {
                            DrvAudioHlpDeviceFree(pDevDst);
                            break;
                        }
                    }
                }

                if (RT_FAILURE(rc))
                    DrvAudioHlpDeviceEnumFree(pEnmDst);
            }

            DrvAudioHlpDeviceEnumFree(&devEnmOut);
        }

        DrvAudioHlpDeviceEnumFree(&devEnmIn);
    }

#ifdef DEBUG
    if (RT_SUCCESS(rc))
        DrvAudioHlpDeviceEnumPrint("Core Audio (Final)", pEnmDst);
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}


#if 0
static int coreAudioDeviceInit(PPDMAUDIODEVICE pDev, PDRVHOSTCOREAUDIO pDrv, PDMAUDIODIR enmUsage, AudioDeviceID deviceID)
{
    AssertPtrReturn(pDev, VERR_INVALID_POINTER);

    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    AssertPtrReturn(pData, VERR_INVALID_POINTER);

    /* Init common parameters. */
    pDev->enmUsage   = enmUsage;

    /* Init Core Audio-specifics. */
    pData->deviceID  = deviceID;
    pData->pDrv      = pDrv;

    RTListInit(&pData->lstStreams);

    return VINF_SUCCESS;
}
#endif


/**
 * Initializes a Core Audio-specific device data structure.
 *
 * @returns IPRT status code.
 * @param   pDevData            Device data structure to initialize.
 * @param   deviceID            Core Audio device ID to assign this structure to.
 * @param   pDrv                Driver instance to use.
 */
static void coreAudioDeviceDataInit(PCOREAUDIODEVICEDATA pDevData, AudioDeviceID deviceID, PDRVHOSTCOREAUDIO pDrv)
{
    AssertPtrReturnVoid(pDevData);
    AssertPtrReturnVoid(pDrv);

    pDevData->deviceID = deviceID;
    pDevData->pDrv     = pDrv;

    RTListInit(&pDevData->lstStreams);
}


/**
 * Propagates an audio device status to all its connected Core Audio streams.
 *
 * @return IPRT status code.
 * @param  pDev                 Audio device to propagate status for.
 * @param  enmSts               Status to propagate.
 */
static int coreAudioDevicePropagateStatus(PPDMAUDIODEVICE pDev, COREAUDIOSTATUS enmSts)
{
    AssertPtrReturn(pDev, VERR_INVALID_POINTER);

    PCOREAUDIODEVICEDATA pDevData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    AssertPtrReturn(pDevData, VERR_INVALID_POINTER);

    /* Sanity. */
    AssertPtr(pDevData->pDrv);

    LogFlowFunc(("pDev=%p, pDevData=%p, enmSts=%RU32\n", pDev, pDevData, enmSts));

    PCOREAUDIOSTREAM pCAStream;
    RTListForEach(&pDevData->lstStreams, pCAStream, COREAUDIOSTREAM, Node)
    {
        LogFlowFunc(("pCAStream=%p\n", pCAStream));

        /* We move the reinitialization to the next output event.
         * This make sure this thread isn't blocked and the
         * reinitialization is done when necessary only. */
        ASMAtomicXchgU32(&pCAStream->enmStatus, enmSts);
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

    PPDMAUDIODEVICE pDev = (PPDMAUDIODEVICE)pvUser;
    AssertPtr(pDev);

    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    AssertPtrReturn(pData, VERR_INVALID_POINTER);

    PDRVHOSTCOREAUDIO pThis = pData->pDrv;
    AssertPtr(pThis);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    UInt32 uAlive = 1;
    UInt32 uSize  = sizeof(UInt32);

    AudioObjectPropertyAddress propAdr = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };

    AudioDeviceID deviceID = pData->deviceID;

    OSStatus err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &uSize, &uAlive);

    bool fIsDead = false;

    if (err == kAudioHardwareBadDeviceError)
        fIsDead = true; /* Unplugged. */
    else if ((err == kAudioHardwareNoError) && (!RT_BOOL(uAlive)))
        fIsDead = true; /* Something else happened. */

    if (fIsDead)
    {
        LogRel2(("CoreAudio: Device '%s' stopped functioning\n", pDev->szName));

        /* Mark device as dead. */
        rc2 = coreAudioDevicePropagateStatus(pDev, COREAUDIOSTATUS_UNINIT);
        AssertRC(rc2);
    }

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    return noErr;
}

/* Callback for getting notified when the default recording/playback device has been changed. */
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
        PPDMAUDIODEVICE pDev = NULL;

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

#ifndef VBOX_WITH_AUDIO_CALLBACKS
        if (pDev)
        {
            PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
            AssertPtr(pData);

            /* This listener is called on every change of the hardware
             * device. So check if the default device has really changed. */
            UInt32 uSize = sizeof(AudioDeviceID);
            UInt32 uResp = 0;

            OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, pProperty, 0, NULL, &uSize, &uResp);
            if (err == noErr)
            {
                if (pData->deviceID != uResp) /* Has the device ID changed? */
                {
                    rc2 = coreAudioDevicePropagateStatus(pDev, COREAUDIOSTATUS_REINIT);
                    AssertRC(rc2);
                }
            }
        }
#endif /* VBOX_WITH_AUDIO_CALLBACKS */
    }

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    PFNPDMHOSTAUDIOCALLBACK pfnCallback = pThis->pfnCallback;
#endif

    /* Make sure to leave the critical section before calling the callback. */
    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    if (pfnCallback)
        /* Ignore rc */ pfnCallback(pThis->pDrvIns, PDMAUDIOCBTYPE_DEVICES_CHANGED, NULL, 0);
#endif

    return noErr;
}

#ifndef VBOX_WITH_AUDIO_CALLBACKS
/**
 * Re-initializes a Core Audio stream with a specific audio device and stream configuration.
 *
 * @return IPRT status code.
 * @param  pThis                Driver instance.
 * @param  pCAStream            Audio stream to re-initialize.
 * @param  pDev                 Audio device to use for re-initialization.
 * @param  pCfg                 Stream configuration to use for re-initialization.
 */
static int coreAudioStreamReinitEx(PDRVHOSTCOREAUDIO pThis,
                                   PCOREAUDIOSTREAM pCAStream, PPDMAUDIODEVICE pDev, PPDMAUDIOSTREAMCFG pCfg)
{
    int rc = coreAudioStreamUninit(pCAStream);
    if (RT_SUCCESS(rc))
    {
        rc = coreAudioStreamInit(pCAStream, pThis, pDev);
        if (RT_SUCCESS(rc))
        {
            if (pCAStream->enmDir == PDMAUDIODIR_IN)
                rc = coreAudioStreamInitIn (pCAStream, pCfg /* pCfgReq */, NULL /* pCfgAcq */);
            else
                rc = coreAudioStreamInitOut(pCAStream, pCfg /* pCfgReq */, NULL /* pCfgAcq */);

            if (RT_SUCCESS(rc))
                rc = coreAudioStreamControl(pCAStream->pDrv, pCAStream, PDMAUDIOSTREAMCMD_ENABLE);

            if (RT_FAILURE(rc))
            {
                int rc2 = coreAudioStreamUninit(pCAStream);
                AssertRC(rc2);
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("CoreAudio: Unable to re-init stream: %Rrc\n", rc));

    return rc;
}

/**
 * Re-initializes a Core Audio stream with a specific audio device.
 *
 * @return IPRT status code.
 * @param  pThis                Driver instance.
 * @param  pCAStream            Audio stream to re-initialize.
 * @param  pDev                 Audio device to use for re-initialization.
 */
static int coreAudioStreamReinit(PDRVHOSTCOREAUDIO pThis, PCOREAUDIOSTREAM pCAStream, PPDMAUDIODEVICE pDev)
{
    int rc = coreAudioStreamUninit(pCAStream);
    if (RT_SUCCESS(rc))
    {
        /* Use the acquired stream configuration from the former initialization to
         * re-initialize the stream. */
        PDMAUDIOSTREAMCFG CfgAcq;
        rc = coreAudioASBDToStreamCfg(&pCAStream->Unit.streamFmt, &CfgAcq);
        if (RT_SUCCESS(rc))
            rc = coreAudioStreamReinitEx(pThis, pCAStream, pDev, &CfgAcq);
    }

    return rc;
}
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
/* Callback to convert audio input data from one format to another. */
static DECLCALLBACK(OSStatus) coreAudioConverterCb(AudioConverterRef              inAudioConverter,
                                                   UInt32                        *ioNumberDataPackets,
                                                   AudioBufferList               *ioData,
                                                   AudioStreamPacketDescription **ppASPD,
                                                   void                          *pvUser)
{
    RT_NOREF(inAudioConverter);

    AssertPtrReturn(ioNumberDataPackets, caConverterEOFDErr);
    AssertPtrReturn(ioData,              caConverterEOFDErr);

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

#ifdef DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            int rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-converter-cb-input.pcm",
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

/* Callback to feed audio input buffer. */
static DECLCALLBACK(OSStatus) coreAudioCaptureCb(void                       *pvUser,
                                                 AudioUnitRenderActionFlags *pActionFlags,
                                                 const AudioTimeStamp       *pAudioTS,
                                                 UInt32                      uBusID,
                                                 UInt32                      cFrames,
                                                 AudioBufferList            *pBufData)
{
    RT_NOREF(uBusID, pBufData);

    /* If nothing is pending return immediately. */
    if (cFrames == 0)
        return noErr;

    PCOREAUDIOSTREAM pStream = (PCOREAUDIOSTREAM)pvUser;

    /* Sanity. */
    AssertPtr(pStream);
    AssertPtr(pStream->pDrv);
    Assert   (pStream->enmDir == PDMAUDIODIR_IN);
    AssertPtr(pStream->Unit.pDevice);

    if (ASMAtomicReadU32(&pStream->enmStatus) != COREAUDIOSTATUS_INIT)
        return noErr;

    OSStatus err = noErr;
    int rc = VINF_SUCCESS;

    AudioBufferList srcBufLst;
    RT_ZERO(srcBufLst);

    do
    {
#ifdef DEBUG
        AudioStreamBasicDescription asbdIn;
        UInt32 uSize = sizeof(asbdIn);
        AudioUnitGetProperty(pStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                             1, &asbdIn, &uSize);
        coreAudioPrintASBD("DevIn",  &asbdIn);

        AudioStreamBasicDescription asbdOut;
        uSize = sizeof(asbdOut);
        AudioUnitGetProperty(pStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                             1, &asbdOut, &uSize);
        coreAudioPrintASBD("DevOut", &asbdOut);
#endif

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
        /* Are we using a converter? */
        if (pStreamIn->ConverterRef)
        {
            PCOREAUDIOCONVCBCTX pConvCbCtx = &pStreamIn->convCbCtx;

            AudioStreamBasicDescription *pSrcASBD =  &pConvCbCtx->asbdSrc;
            AudioStreamBasicDescription *pDstASBD =  &pConvCbCtx->asbdDst;
# ifdef DEBUG
            coreAudioPrintASBD("Src", pSrcASBD);
            coreAudioPrintASBD("Dst", pDstASBD);
# endif
            /* Initialize source list buffer. */
            srcBufLst.mNumberBuffers = 1;

            /* Initialize the first buffer. */
            srcBufLst.mBuffers[0].mNumberChannels = pSrcASBD->mChannelsPerFrame;
            srcBufLst.mBuffers[0].mDataByteSize   = pSrcASBD->mBytesPerFrame * cFrames;
            srcBufLst.mBuffers[0].mData           = RTMemAllocZ(srcBufLst.mBuffers[0].mDataByteSize);
            if (!srcBufLst.mBuffers[0].mData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

        #if 0
            /* Initialize the second buffer. */
            srcBufLst.mBuffers[1].mNumberChannels = 1; //pSrcASBD->mChannelsPerFrame;
            srcBufLst.mBuffers[1].mDataByteSize   = pSrcASBD->mBytesPerFrame * cFrames;
            srcBufLst.mBuffers[1].mData           = RTMemAllocZ(srcBufLst.mBuffers[1].mDataByteSize);
            if (!srcBufLst.mBuffers[1].mData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
        #endif

            /* Set the buffer list for our callback context. */
            pConvCbCtx->pBufLstSrc = &srcBufLst;

            /* Sanity. */
            AssertPtr(pConvCbCtx->pBufLstSrc);
            Assert(pConvCbCtx->pBufLstSrc->mNumberBuffers >= 1);

            /* Get the first buffer. */
            AudioBuffer *pSrcBuf   = &srcBufLst.mBuffers[0];

            Log3Func(("pSrcBuf->mDataByteSize1=%RU32\n", pSrcBuf->mDataByteSize));

            /* First, render the source data as usual. */
            err = AudioUnitRender(pStreamIn->audioUnit, pActionFlags, pAudioTS, uBusID, cFrames,
                                  &srcBufLst);
            if (err != noErr)
            {
                if (pConvCbCtx->cErrors < 32) /** @todo Make this configurable. */
                {
                    LogRel2(("CoreAudio: Failed rendering converted audio input data (%RI32:%c%c%c%c)\n", err,
                             RT_BYTE4(err), RT_BYTE3(err), RT_BYTE2(err), RT_BYTE1(err)));
                    pConvCbCtx->cErrors++;
                }

                rc = VERR_IO_GEN_FAILURE;
                break;
            }

            /* Note: pSrcBuf->mDataByteSize can have changed after calling AudioUnitRender above! */
            Log3Func(("pSrcBuf->mDataByteSize2=%RU32\n", pSrcBuf->mDataByteSize));

# ifdef DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-recording-cb-src.pcm",
                            RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                RTFileWrite(fh, pSrcBuf->mData, pSrcBuf->mDataByteSize, NULL);
                RTFileClose(fh);
            }
            else
                AssertFailed();
# endif
            AudioBufferList dstBufLst;
            RT_ZERO(dstBufLst);

            dstBufLst.mNumberBuffers = 1; /* We only use one buffer at once. */

            AudioBuffer *pDstBuf = &dstBufLst.mBuffers[0];

            UInt32 cbDst = pDstASBD->mBytesPerFrame * cFrames;
            void  *pvDst = RTMemAlloc(cbDst);
            if (!pvDst)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            pDstBuf->mDataByteSize = cbDst;
            pDstBuf->mData         = pvDst;

            AudioConverterReset(pStreamIn->ConverterRef);

            Log3Func(("cbSrcBufSize=%RU32 (BPF=%RU32), cbDstBufSize=%RU32 (BPF=%RU32)\n",
                      pSrcBuf->mDataByteSize, pSrcASBD->mBytesPerFrame,
                      pDstBuf->mDataByteSize, pDstASBD->mBytesPerFrame));

            if (pSrcASBD->mSampleRate == pDstASBD->mSampleRate)
            {
                err = AudioConverterConvertBuffer(pStreamIn->ConverterRef,
                                                #if 0
                                                  cbT1, pvT1, &cbT2, pvT2);
                                                #else
                                                  pSrcBuf->mDataByteSize, pSrcBuf->mData /* Input */,
                                                  &cbDst, pvDst                          /* Output */);
                                                #endif
                if (err != noErr)
                {
                    if (pConvCbCtx->cErrors < 32) /** @todo Make this configurable. */
                    {
                        LogRel2(("CoreAudio: Failed to convert audio input data (%RI32:%c%c%c%c)\n", err,
                                 RT_BYTE4(err), RT_BYTE3(err), RT_BYTE2(err), RT_BYTE1(err)));
                        pConvCbCtx->cErrors++;
                    }

                    rc = VERR_IO_GEN_FAILURE;
                    break;
                }

# ifdef DEBUG_DUMP_PCM_DATA
                rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-recording-cb-conv-dst.pcm",
                                RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
                if (RT_SUCCESS(rc))
                {
                    RTFileWrite(fh, pvDst, cbDst, NULL);
                    RTFileClose(fh);
                }
                else
                    AssertFailed();
# endif
            }
            else /* Invoke FillComplexBuffer because the sample rate is different. */
            {
                pConvCbCtx->uPacketCnt = pSrcBuf->mDataByteSize / pSrcASBD->mBytesPerPacket;
                pConvCbCtx->uPacketIdx = 0;

                Log3Func(("cFrames=%RU32 (%RU32 dest frames per packet) -> %RU32 input frames\n",
                          cFrames, pDstASBD->mFramesPerPacket, pConvCbCtx->uPacketCnt));

                UInt32 cPacketsToWrite = pDstBuf->mDataByteSize / pDstASBD->mBytesPerPacket;
                Assert(cPacketsToWrite);

                UInt32 cPacketsWritten = 0;

                Log3Func(("cPacketsToWrite=%RU32\n", cPacketsToWrite));

                while (cPacketsToWrite)
                {
                    UInt32 cPacketsIO = cPacketsToWrite;

                    Log3Func(("cPacketsIO=%RU32 (In)\n", cPacketsIO));

                    err = AudioConverterFillComplexBuffer(pStreamIn->ConverterRef,
                                                          coreAudioConverterCb, pConvCbCtx /* pvData */,
                                                          &cPacketsIO, &dstBufLst, NULL);
                    if (err != noErr)
                    {
                        if (pConvCbCtx->cErrors < 32) /** @todo Make this configurable. */
                        {
                            LogRel2(("CoreAudio: Failed to convert complex audio data (%RI32:%c%c%c%c)\n", err,
                                     RT_BYTE4(err), RT_BYTE3(err), RT_BYTE2(err), RT_BYTE1(err)));
                            pConvCbCtx->cErrors++;
                        }

                        rc = VERR_IO_GEN_FAILURE;
                        break;
                    }

                    Log3Func(("cPacketsIO=%RU32 (Out)\n", cPacketsIO));

                    cPacketsWritten = cPacketsIO;

                    Assert(cPacketsToWrite >= cPacketsWritten);
                    cPacketsToWrite -= cPacketsWritten;

                    size_t cbPacketsWritten = cPacketsWritten * pDstASBD->mBytesPerPacket;
                    Log3Func(("%RU32 packets written (%zu bytes), err=%RI32\n", cPacketsWritten, cbPacketsWritten, err));

                    if (!cPacketsWritten)
                        break;

# ifdef DEBUG_DUMP_PCM_DATA
                    rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-recording-cb-complex-dst.pcm",
                                    RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
                    if (RT_SUCCESS(rc))
                    {
                        RTFileWrite(fh, pvDst, cbDst, NULL);
                        RTFileClose(fh);
                    }
                    else
                        AssertFailed();
# endif
                    size_t cbFree = RTCircBufFree(pStreamIn->pCircBuf);
                    if (cbFree < cbDst)
                    {
                        LogRel2(("CoreAudio: Recording is lagging behind (%zu bytes available but only %zu bytes free)\n",
                                 cbDst, cbFree));
                        break;
                    }

                    size_t cbDstChunk;
                    void  *puDst;
                    RTCircBufAcquireWriteBlock(pStreamIn->pCircBuf, cbDst, (void **)&puDst, &cbDstChunk);

                    if (cbDstChunk)
                        memcpy(puDst, pvDst, cbDstChunk);

                    RTCircBufReleaseWriteBlock(pStreamIn->pCircBuf, cbDstChunk);
                }
            }

            if (pvDst)
            {
                RTMemFree(pvDst);
                pvDst = NULL;
            }
        }
        else /* No converter being used. */
        {
#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */

            AudioStreamBasicDescription *pStreamFmt = &pStream->Unit.streamFmt;

            AssertBreakStmt(pStreamFmt->mChannelsPerFrame >= 1, rc = VERR_INVALID_PARAMETER);
            AssertBreakStmt(pStreamFmt->mBytesPerFrame >= 1,    rc = VERR_INVALID_PARAMETER);

            srcBufLst.mNumberBuffers = 1;

            AudioBuffer *pSrcBuf = &srcBufLst.mBuffers[0];

            pSrcBuf->mNumberChannels = pStreamFmt->mChannelsPerFrame;
            pSrcBuf->mDataByteSize   = pStreamFmt->mBytesPerFrame * cFrames;
            pSrcBuf->mData           = RTMemAlloc(pSrcBuf->mDataByteSize);
            if (!pSrcBuf->mData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            err = AudioUnitRender(pStream->Unit.audioUnit, pActionFlags, pAudioTS, 1 /* Input bus */, cFrames, &srcBufLst);
            if (err != noErr)
            {
                LogRel2(("CoreAudio: Failed rendering non-converted audio input data (%RI32)\n", err));
                rc = VERR_IO_GEN_FAILURE; /** @todo Improve this. */
                break;
            }

#ifdef DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-recording-cb-src.pcm",
                            RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                RTFileWrite(fh, pSrcBuf->mData, pSrcBuf->mDataByteSize, NULL);
                RTFileClose(fh);
            }
            else
                AssertFailed();
#endif
            PRTCIRCBUF pCircBuf = pStream->pCircBuf;

            const uint32_t cbDataSize = pSrcBuf->mDataByteSize;
            const size_t   cbBufFree  = RTCircBufFree(pCircBuf);
                  size_t   cbAvail    = RT_MIN(cbDataSize, cbBufFree);

            Log3Func(("cbDataSize=%RU32, cbBufFree=%zu, cbAvail=%zu\n", cbDataSize, cbBufFree, cbAvail));

            /* Iterate as long as data is available. */
            uint8_t *puDst = NULL;
            uint32_t cbWrittenTotal = 0;
            while (cbAvail)
            {
                /* Try to acquire the necessary space from the ring buffer. */
                size_t cbToWrite = 0;
                RTCircBufAcquireWriteBlock(pCircBuf, cbAvail, (void **)&puDst, &cbToWrite);
                if (!cbToWrite)
                    break;

                /* Copy the data from the Core Audio buffer to the ring buffer. */
                memcpy(puDst, (uint8_t *)pSrcBuf->mData + cbWrittenTotal, cbToWrite);

#ifdef DEBUG_DUMP_PCM_DATA
                rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-recording-cb-dst.pcm",
                                RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
                if (RT_SUCCESS(rc))
                {
                    RTFileWrite(fh, (uint8_t *)pSrcBuf->mData + cbWrittenTotal, cbToWrite, NULL);
                    RTFileClose(fh);
                }
                else
                    AssertFailed();
#endif
                /* Release the ring buffer, so the main thread could start reading this data. */
                RTCircBufReleaseWriteBlock(pCircBuf, cbToWrite);

                cbWrittenTotal += cbToWrite;

                Assert(cbAvail >= cbToWrite);
                cbAvail -= cbToWrite;
            }

            Log3Func(("cbWrittenTotal=%RU32, cbLeft=%zu\n", cbWrittenTotal, cbAvail));

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
        }
#endif

    } while (0);

    for (UInt32 i = 0; i < srcBufLst.mNumberBuffers; i++)
    {
        if (srcBufLst.mBuffers[i].mData)
        {
            RTMemFree(srcBufLst.mBuffers[i].mData);
            srcBufLst.mBuffers[i].mData = NULL;
        }
    }

    return err;
}


/**
 * Initializes a Core Audio stream.
 *
 * @return IPRT status code.
 * @param  pThis                Driver instance.
 * @param  pCAStream            Stream to initialize.
 * @param  pDev                 Audio device to use for this stream.
 */
static int coreAudioStreamInit(PCOREAUDIOSTREAM pCAStream, PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev)
{
    AssertPtrReturn(pCAStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pDev,      VERR_INVALID_POINTER);

    Assert(pCAStream->Unit.pDevice == NULL); /* Make sure no device is assigned yet. */

    AssertPtr(pDev->pvData);
    Assert(pDev->cbData == sizeof(COREAUDIODEVICEDATA));

#ifdef DEBUG
    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    LogFunc(("pCAStream=%p, pDev=%p ('%s', ID=%RU32)\n", pCAStream, pDev, pDev->szName, pData->deviceID));
#endif

    pCAStream->Unit.pDevice = pDev;
    pCAStream->pDrv = pThis;

    return VINF_SUCCESS;
}

# define CA_BREAK_STMT(stmt) \
    stmt; \
    break;

/** @todo Eventually split up this function, as this already is huge! */
static int coreAudioStreamInitIn(PCOREAUDIOSTREAM pCAStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc = VINF_SUCCESS;

    UInt32 cSamples = 0;

    OSStatus err = noErr;

    LogFunc(("pCAStream=%p, pCfgReq=%p, pCfgAcq=%p\n", pCAStream, pCfgReq, pCfgAcq));

    PPDMAUDIODEVICE pDev = pCAStream->Unit.pDevice;
    AssertPtr(pDev);

    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    AssertPtr(pData);

    AudioDeviceID deviceID = pData->deviceID;
    LogFunc(("deviceID=%RU32\n", deviceID));
    Assert(deviceID != kAudioDeviceUnknown);

    do
    {
        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_IN_INIT);

        /* Get the default frames buffer size, so that we can setup our internal buffers. */
        UInt32 cFrames;
        UInt32 uSize = sizeof(cFrames);

        AudioObjectPropertyAddress propAdr;
        propAdr.mSelector = kAudioDevicePropertyBufferFrameSize;
        propAdr.mScope    = kAudioDevicePropertyScopeInput;
        propAdr.mElement  = kAudioObjectPropertyElementMaster;
        err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &uSize, &cFrames);
        if (err != noErr)
        {
            /* Can happen if no recording device is available by default. Happens on some Macs,
             * so don't log this by default to not scare people. */
            LogRel2(("CoreAudio: Failed to determine frame buffer size of the audio recording device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Set the frame buffer size and honor any minimum/maximum restrictions on the device. */
        err = coreAudioSetFrameBufferSize(deviceID, true /* fInput */, cFrames, &cFrames);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set frame buffer size for the audio recording device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        LogFlowFunc(("cFrames=%RU32\n", cFrames));

        /* Try to find the default HAL output component. */
        AudioComponentDescription cd;

        RT_ZERO(cd);
        cd.componentType         = kAudioUnitType_Output;
        cd.componentSubType      = kAudioUnitSubType_HALOutput;
        cd.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent cp = AudioComponentFindNext(NULL, &cd);
        if (cp == 0)
        {
            LogRel(("CoreAudio: Failed to find HAL output component\n")); /** @todo Return error value? */
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Open the default HAL output component. */
        err = AudioComponentInstanceNew(cp, &pCAStream->Unit.audioUnit);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to open output component (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Switch the I/O mode for input to on. */
        UInt32 uFlag = 1;
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
                                   1, &uFlag, sizeof(uFlag));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to disable input I/O mode for input stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Switch the I/O mode for output to off. This is important, as this is a pure input stream. */
        uFlag = 0;
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                                   0, &uFlag, sizeof(uFlag));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to disable output I/O mode for input stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Set the default audio recording device as the device for the new AudioUnit. */
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
                                   0, &deviceID, sizeof(deviceID));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set current input device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * CoreAudio will inform us on a second thread for new incoming audio data.
         * Therefore register a callback function which will process the new data.
         */
        AURenderCallbackStruct cb;
        RT_ZERO(cb);
        cb.inputProc       = coreAudioCaptureCb;
        cb.inputProcRefCon = pCAStream;

        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global,
                                   0, &cb, sizeof(cb));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to register input callback (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Create the recording device's out format based on our required audio settings. */
        AudioStreamBasicDescription reqFmt;
        rc = coreAudioStreamCfgToASBD(pCfgReq, &reqFmt);
        if (RT_FAILURE(rc))
        {
            LogRel(("CoreAudio: Failed to convert requested input format to native format (%Rrc)\n", rc));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Requested stream input format", &reqFmt);

        /* Fetch the input format of the recording device. */
        AudioStreamBasicDescription devInFmt;
        RT_ZERO(devInFmt);
        uSize = sizeof(devInFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   1, &devInFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get input device input format (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Input device in (initial)", &devInFmt);

        /* Fetch the output format of the recording device. */
        AudioStreamBasicDescription devOutFmt;
        RT_ZERO(devOutFmt);
        uSize = sizeof(devOutFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   1, &devOutFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get input device output format (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Input device out (initial)", &devOutFmt);

        /* Set the output format for the input device so that it matches the initial input format.
         * This apparently is needed for some picky / buggy USB headsets. */

        /*
         * The only thing we tweak here is the actual format flags: A lot of USB headsets tend
         * to have float PCM data, which we can't handle (yet).
         *
         * So set this as signed integers in a packed, non-iterleaved format.
         */
        devInFmt.mFormatFlags =   kAudioFormatFlagIsSignedInteger
                                | kAudioFormatFlagIsPacked;

        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   1, &devInFmt, sizeof(devInFmt));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set new output format for input device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * Also set the frame buffer size of the device on our AudioUnit. This
         * should make sure that the frames count which we receive in the render
         * thread is as we like.
         */
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                                   1, &cFrames, sizeof(cFrames));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set maximum frame buffer size for input stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * Initialize the new AudioUnit.
         */
        err = AudioUnitInitialize(pCAStream->Unit.audioUnit);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to initialize input audio device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Get final input format afer initialization. */
        uSize = sizeof(devInFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   1, &devInFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to re-getting input device input format (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Print the device/stream formats again after the audio unit has been initialized.
         * Note that the formats could have changed now. */
        coreAudioPrintASBD("Input device in (after initialization)", &devInFmt);

        /* Get final output format afer initialization. */
        uSize = sizeof(devOutFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   1, &devOutFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to re-getting input device output format (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Print the device/stream formats again after the audio unit has been initialized.
         * Note that the formats could have changed now. */
        coreAudioPrintASBD("Input device out (after initialization)", &devOutFmt);

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER

        /* If the frequency of the device' output format different from the requested one we
         * need a converter. The same counts if the number of channels or the bits per channel are different. */
        if (   devOutFmt.mChannelsPerFrame != reqFmt.mChannelsPerFrame
            || devOutFmt.mSampleRate       != reqFmt.mSampleRate)
        {
            LogRel2(("CoreAudio: Input converter is active\n"));

            err = AudioConverterNew(&devOutFmt /* Input */, &reqFmt /* Output */, &pCAStream->In.ConverterRef);
            if (RT_UNLIKELY(err != noErr))
            {
                LogRel(("CoreAudio: Failed to create the audio converter (%RI32)\n", err));
                CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
            }

            if (   devOutFmt.mChannelsPerFrame  == 1 /* Mono */
                && reqFmt.mChannelsPerFrame     == 2 /* Stereo */)
            {
                LogRel2(("CoreAudio: Mono to stereo conversion active\n"));

                /*
                 * If the channel count is different we have to tell this the converter
                 * and supply a channel mapping. For now we only support mapping
                 * from mono to stereo. For all other cases the core audio defaults
                 * are used, which means dropping additional channels in most
                 * cases.
                 */
                const SInt32 channelMap[2] = {0, 0}; /* Channel map for mono -> stereo. */

                err = AudioConverterSetProperty(pCAStream->In.ConverterRef, kAudioConverterChannelMap, sizeof(channelMap), channelMap);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to set channel mapping (mono -> stereo) for the audio input converter (%RI32)\n", err));
                    CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
                }
            }

            /* Set sample rate converter quality to maximum. */
            uFlag = kAudioConverterQuality_Max;
            err = AudioConverterSetProperty(pCAStream->In.ConverterRef, kAudioConverterSampleRateConverterQuality,
                                            sizeof(uFlag), &uFlag);
            if (err != noErr)
                LogRel2(("CoreAudio: Failed to set input audio converter quality to the maximum (%RI32)\n", err));

            uSize = sizeof(UInt32);
            UInt32 maxOutputSize;
            err = AudioConverterGetProperty(pStreamIn->ConverterRef, kAudioConverterPropertyMaximumOutputPacketSize,
                                            &uSize, &maxOutputSize);
            if (RT_UNLIKELY(err != noErr))
            {
                LogRel(("CoreAudio: Failed to retrieve converter's maximum output size (%RI32)\n", err));
                CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
            }

            LogFunc(("Maximum converter packet output size is: %RI32\n", maxOutputSize));
        }
#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
        if (pCAStream->In.ConverterRef)
        {
            /* Save the requested format as our stream format. */
            memcpy(&pCAStream->Unit.streamFmt, &reqFmt, sizeof(AudioStreamBasicDescription));
        }
        else
        {
#endif /* VBOX_WITH_AUDIO_CA_CONVERTER */

            /* Save the final output format as our stream format. */
            memcpy(&pCAStream->Unit.streamFmt, &devOutFmt, sizeof(AudioStreamBasicDescription));

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
        }
#endif
        /*
         * There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
         * the frame buffer size set in the previous calls. So finally get the
         * frame buffer size after the AudioUnit was initialized.
         */
        uSize = sizeof(cFrames);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                                   0, &cFrames, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get maximum frame buffer size from input audio device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Calculate the ratio between the device and the stream sample rate. */
        pCAStream->In.sampleRatio = devOutFmt.mSampleRate / devInFmt.mSampleRate;

        /*
         * Make sure that the ring buffer is big enough to hold the recording
         * data. Compare the maximum frames per slice value with the frames
         * necessary when using the converter where the sample rate could differ.
         * The result is always multiplied by the channels per frame to get the
         * samples count.
         */
        cSamples = RT_MAX(cFrames,
                          (cFrames * reqFmt.mBytesPerFrame * pCAStream->In.sampleRatio)
                           / reqFmt.mBytesPerFrame)
                           * reqFmt.mChannelsPerFrame;
        if (!cSamples)
        {
            LogRel(("CoreAudio: Failed to determine samples buffer count input stream\n"));
            CA_BREAK_STMT(rc = VERR_INVALID_PARAMETER);
        }

        rc = RTCircBufCreate(&pCAStream->pCircBuf, cSamples << 1 /*pHstStrmIn->Props.cShift*/); /** @todo FIX THIS !!! */
        if (RT_FAILURE(rc))
            break;

#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
        /* Init the converter callback context. */

        /* As source, use the input device' output format,
         * as destination, use the initially requested format. */
        rc = coreAudioInitConvCbCtx(&pCAStream->In.convCbCtx, pCAStream,
                                    &devOutFmt /* Source */, &reqFmt /* Dest */);
#endif

    } while (0);

    if (RT_SUCCESS(rc))
    {
        pCAStream->enmDir = PDMAUDIODIR_IN;

        rc = coreAudioASBDToStreamCfg(&pCAStream->Unit.streamFmt, pCfgAcq);
        AssertRC(rc);

        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_INIT);

        pCfgAcq->cSampleBufferSize = cSamples;
    }
    else
    {
        int rc2 = coreAudioStreamUninit(pCAStream);
        AssertRC(rc2);

        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_UNINIT);
    }

    LogFunc(("cSamples=%RU32, rc=%Rrc\n", cSamples, rc));
    return rc;
}

/** @todo Eventually split up this function, as this already is huge! */
static int coreAudioStreamInitOut(PCOREAUDIOSTREAM pCAStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc = VINF_SUCCESS;
    UInt32 cSamples = 0;

    OSStatus err = noErr;

    LogFunc(("pCAStream=%p, pCfgReq=%p, pCfgAcq=%p\n", pCAStream, pCfgReq, pCfgAcq));

    PPDMAUDIODEVICE pDev = pCAStream->Unit.pDevice;
    AssertPtr(pDev);

    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    AssertPtr(pData);

    AudioDeviceID deviceID = pData->deviceID;
    LogFunc(("deviceID=%RU32\n", deviceID));
    Assert(deviceID != kAudioDeviceUnknown);

    do
    {
        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_IN_INIT);

        /* Get the default frames buffer size, so that we can setup our internal buffers. */
        UInt32 cFrames;
        UInt32 uSize = sizeof(cFrames);

        AudioObjectPropertyAddress propAdr;
        propAdr.mSelector = kAudioDevicePropertyBufferFrameSize;
        propAdr.mScope    = kAudioDevicePropertyScopeInput;
        propAdr.mElement  = kAudioObjectPropertyElementMaster;
        err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &uSize, &cFrames);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to determine frame buffer size of the audio playback device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Set the frame buffer size and honor any minimum/maximum restrictions on the device. */
        err = coreAudioSetFrameBufferSize(deviceID, false /* fInput */, cFrames, &cFrames);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set frame buffer size for the audio playback device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Try to find the default HAL output component. */
        AudioComponentDescription cd;
        RT_ZERO(cd);

        cd.componentType         = kAudioUnitType_Output;
        cd.componentSubType      = kAudioUnitSubType_HALOutput;
        cd.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent cp = AudioComponentFindNext(NULL, &cd);
        if (cp == 0)
        {
            LogRel(("CoreAudio: Failed to find HAL output component\n")); /** @todo Return error value? */
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Open the default HAL output component. */
        err = AudioComponentInstanceNew(cp, &pCAStream->Unit.audioUnit);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to open output component (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Switch the I/O mode for output to on. */
        UInt32 uFlag = 1;
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                                   0, &uFlag, sizeof(uFlag));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to disable I/O mode for output stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Set the default audio playback device as the device for the new AudioUnit. */
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
                                   0, &deviceID, sizeof(deviceID));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set current device for output stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * CoreAudio will inform us on a second thread for new incoming audio data.
         * Therefor register a callback function which will process the new data.
         */
        AURenderCallbackStruct cb;
        RT_ZERO(cb);
        cb.inputProc       = coreAudioPlaybackCb;
        cb.inputProcRefCon = pCAStream; /* pvUser */

        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                   0, &cb, sizeof(cb));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to register playback callback (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        AudioStreamBasicDescription reqFmt;
        coreAudioStreamCfgToASBD(pCfgReq, &reqFmt);
        coreAudioPrintASBD("Requested stream output format", &reqFmt);

        /* Fetch the initial input format of the device. */
        AudioStreamBasicDescription devInFmt;
        uSize = sizeof(devInFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   0, &devInFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get input format of playback device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Output device in (initial)", &devInFmt);

        /* Fetch the initial output format of the device. */
        AudioStreamBasicDescription devOutFmt;
        uSize = sizeof(devOutFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   0, &devOutFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get output format of playback device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Output device out (initial)", &devOutFmt);

        /* Set the new input format for the output device. */
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   0, &reqFmt, sizeof(reqFmt));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set stream format for output stream (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * Also set the frame buffer size off the device on our AudioUnit. This
         * should make sure that the frames count which we receive in the render
         * thread is as we like.
         */
        err = AudioUnitSetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                                   0, &cFrames, sizeof(cFrames));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set maximum frame buffer size for output AudioUnit (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * Initialize the new AudioUnit.
         */
        err = AudioUnitInitialize(pCAStream->Unit.audioUnit);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to initialize the output audio device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /* Fetch the final output format of the device after the audio unit has been initialized. */
        uSize = sizeof(devInFmt);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   0, &devInFmt, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed re-getting input format of output device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        coreAudioPrintASBD("Output device in (after initialization)", &devInFmt);

        /* Save this final output format as our stream format. */
        memcpy(&pCAStream->Unit.streamFmt, &devInFmt, sizeof(AudioStreamBasicDescription));

        /*
         * There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
         * the frame buffer size set in the previous calls. So finally get the
         * frame buffer size after the AudioUnit was initialized.
         */
        uSize = sizeof(cFrames);
        err = AudioUnitGetProperty(pCAStream->Unit.audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                                   0, &cFrames, &uSize);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to get maximum frame buffer size from output audio device (%RI32)\n", err));
            CA_BREAK_STMT(rc = VERR_AUDIO_BACKEND_INIT_FAILED);
        }

        /*
         * Make sure that the ring buffer is big enough to hold the recording
         * data. Compare the maximum frames per slice value with the frames
         * necessary when using the converter where the sample rate could differ.
         * The result is always multiplied by the channels per frame to get the
         * samples count.
         */
        cSamples = cFrames * reqFmt.mChannelsPerFrame;
        if (!cSamples)
        {
            LogRel(("CoreAudio: Failed to determine samples buffer count output stream\n"));
            CA_BREAK_STMT(rc = VERR_INVALID_PARAMETER);
        }

        /* Create the internal ring buffer. */
        rc = RTCircBufCreate(&pCAStream->pCircBuf, cSamples << 1 /*pHstStrmOut->Props.cShift*/); /** @todo FIX THIS !!! */

    } while (0);

    if (RT_SUCCESS(rc))
    {
        pCAStream->enmDir = PDMAUDIODIR_OUT;

        rc = coreAudioASBDToStreamCfg(&pCAStream->Unit.streamFmt, pCfgAcq);
        AssertRC(rc);

        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_INIT);

        pCfgAcq->cSampleBufferSize = cSamples;
    }
    else
    {
        int rc2 = coreAudioStreamUninit(pCAStream);
        AssertRC(rc2);

        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_UNINIT);
    }

    LogFunc(("cSamples=%RU32, rc=%Rrc\n", cSamples, rc));
    return rc;
}


/**
 * Uninitializes a Core Audio stream.
 *
 * @return IPRT status code.
 * @param  pCAStream            Stream to uninitialize.
 */
static int coreAudioStreamUninit(PCOREAUDIOSTREAM pCAStream)
{
    OSStatus err = noErr;

    if (pCAStream->Unit.audioUnit)
    {
        err = AudioUnitUninitialize(pCAStream->Unit.audioUnit);
        if (err == noErr)
        {
            err = AudioComponentInstanceDispose(pCAStream->Unit.audioUnit);
            if (err == noErr)
                pCAStream->Unit.audioUnit = NULL;
        }
    }

    if (err == noErr)
    {
        if (pCAStream->pCircBuf)
        {
            RTCircBufDestroy(pCAStream->pCircBuf);
            pCAStream->pCircBuf = NULL;
        }

        pCAStream->enmStatus = COREAUDIOSTATUS_UNINIT;

        pCAStream->enmDir = PDMAUDIODIR_UNKNOWN;
        pCAStream->pDrv   = NULL;

        pCAStream->Unit.pDevice = NULL;
        RT_ZERO(pCAStream->Unit.streamFmt);

        if (pCAStream->enmDir == PDMAUDIODIR_IN)
        {
#ifdef VBOX_WITH_AUDIO_CA_CONVERTER
            if (pCAStream->In.ConverterRef)
            {
                AudioConverterDispose(pCAStream->In.ConverterRef);
                pCAStream->In.ConverterRef = NULL;
            }

            drvHostCoreAudioUninitConvCbCtx(&pCAStream->In.convCbCtx);
#endif
            pCAStream->In.sampleRatio = 1;
        }
        else if (pCAStream->enmDir == PDMAUDIODIR_OUT)
        {

        }
    }
    else
        LogRel(("CoreAudio: Failed to uninit stream (%RI32)\n", err));

    return err == noErr ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Fudge! */
}


/**
 * Registers callbacks for a specific Core Audio device.
 *
 * @return IPRT status code.
 * @param  pThis                Host audio driver instance.
 * @param  pDev                 Audio device to use for the registered callbacks.
 */
static int coreAudioDeviceRegisterCallbacks(PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev)
{
    RT_NOREF(pThis);

    AudioDeviceID deviceID = kAudioDeviceUnknown;

    PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
    if (pData)
        deviceID = pData->deviceID;

    if (deviceID != kAudioDeviceUnknown)
    {
        LogFunc(("deviceID=%RU32\n", deviceID));

        /*
         * Register device callbacks.
         */
        AudioObjectPropertyAddress propAdr = { kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMaster };
        OSStatus err = AudioObjectAddPropertyListener(deviceID, &propAdr,
                                                      coreAudioDeviceStateChangedCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareIllegalOperationError)
        {
            LogRel(("CoreAudio: Failed to add the recording device state changed listener (%RI32)\n", err));
        }

        propAdr.mSelector = kAudioDeviceProcessorOverload;
        propAdr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &propAdr,
                                             coreAudioDevPropChgCb, pDev /* pvUser */);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register processor overload listener (%RI32)\n", err));

        propAdr.mSelector = kAudioDevicePropertyNominalSampleRate;
        propAdr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(deviceID, &propAdr,
                                             coreAudioDevPropChgCb, pDev /* pvUser */);
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
static int coreAudioDeviceUnregisterCallbacks(PDRVHOSTCOREAUDIO pThis, PPDMAUDIODEVICE pDev)
{
    RT_NOREF(pThis);

    AudioDeviceID deviceID = kAudioDeviceUnknown;

    if (pDev)
    {
        PCOREAUDIODEVICEDATA pData = (PCOREAUDIODEVICEDATA)pDev->pvData;
        if (pData)
            deviceID = pData->deviceID;
    }

    if (deviceID != kAudioDeviceUnknown)
    {
        LogFunc(("deviceID=%RU32\n", deviceID));

        /*
         * Unregister per-device callbacks.
         */
        AudioObjectPropertyAddress propAdr = { kAudioDeviceProcessorOverload, kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMaster };
        OSStatus err = AudioObjectRemovePropertyListener(deviceID, &propAdr,
                                                         coreAudioDevPropChgCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
        {
            LogRel(("CoreAudio: Failed to remove the recording processor overload listener (%RI32)\n", err));
        }

        propAdr.mSelector = kAudioDevicePropertyNominalSampleRate;
        err = AudioObjectRemovePropertyListener(deviceID, &propAdr,
                                                coreAudioDevPropChgCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
        {
            LogRel(("CoreAudio: Failed to remove the sample rate changed listener (%RI32)\n", err));
        }

        propAdr.mSelector = kAudioDevicePropertyDeviceIsAlive;
        err = AudioObjectRemovePropertyListener(deviceID, &propAdr,
                                                coreAudioDeviceStateChangedCb, pDev /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareBadObjectError)
        {
            LogRel(("CoreAudio: Failed to remove the device alive listener (%RI32)\n", err));
        }
    }

    return VINF_SUCCESS;
}

/* Callback for getting notified when some of the properties of an audio device have changed. */
static DECLCALLBACK(OSStatus) coreAudioDevPropChgCb(AudioObjectID                     propertyID,
                                                    UInt32                            cAddresses,
                                                    const AudioObjectPropertyAddress  properties[],
                                                    void                             *pvUser)
{
    RT_NOREF(cAddresses, properties);

#ifdef DEBUG
    PPDMAUDIODEVICE pDev = (PPDMAUDIODEVICE)pvUser;
    AssertPtr(pDev);

    LogFlowFunc(("propertyID=%u, nAddresses=%u, pDev=%p\n", propertyID, cAddresses, pDev));
#endif

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
#ifndef VBOX_WITH_AUDIO_CALLBACKS
            int rc2 = coreAudioDevicePropagateStatus(pDev, COREAUDIOSTATUS_REINIT);
            AssertRC(rc2);
#endif
            break;
        }

        default:
            /* Just skip. */
            break;
    }

    return noErr;
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
    DrvAudioHlpDeviceEnumFree(&pThis->Devices);

    /* Enumerate all devices internally. */
    int rc = coreAudioDevicesEnumerateAll(pThis, &pThis->Devices);
    if (RT_SUCCESS(rc))
    {
        /*
         * Default input device.
         */
        pThis->pDefaultDevIn = DrvAudioHlpDeviceEnumGetDefaultDevice(&pThis->Devices, PDMAUDIODIR_IN);
        if (pThis->pDefaultDevIn)
        {
            LogRel2(("CoreAudio: Default capturing device is '%s'\n", pThis->pDefaultDevIn->szName));

#ifdef DEBUG
            PCOREAUDIODEVICEDATA pDevData = (PCOREAUDIODEVICEDATA)pThis->pDefaultDevIn->pvData;
            AssertPtr(pDevData);
            LogFunc(("pDefaultDevIn=%p, ID=%RU32\n", pThis->pDefaultDevIn, pDevData->deviceID));
#endif
            rc = coreAudioDeviceRegisterCallbacks(pThis, pThis->pDefaultDevIn);
        }
        else
            LogRel2(("CoreAudio: No default capturing device found\n"));

        /*
         * Default output device.
         */
        pThis->pDefaultDevOut = DrvAudioHlpDeviceEnumGetDefaultDevice(&pThis->Devices, PDMAUDIODIR_OUT);
        if (pThis->pDefaultDevOut)
        {
            LogRel2(("CoreAudio: Default playback device is '%s'\n", pThis->pDefaultDevOut->szName));

#ifdef DEBUG
            PCOREAUDIODEVICEDATA pDevData = (PCOREAUDIODEVICEDATA)pThis->pDefaultDevOut->pvData;
            AssertPtr(pDevData);
            LogFunc(("pDefaultDevOut=%p, ID=%RU32\n", pThis->pDefaultDevOut, pDevData->deviceID));
#endif
            rc = coreAudioDeviceRegisterCallbacks(pThis, pThis->pDefaultDevOut);
        }
        else
            LogRel2(("CoreAudio: No default playback device found\n"));
    }

    LogFunc(("Returning %Rrc\n", rc));
    return rc;
}

/* Callback to feed audio output buffer. */
static DECLCALLBACK(OSStatus) coreAudioPlaybackCb(void                       *pvUser,
                                                  AudioUnitRenderActionFlags *pActionFlags,
                                                  const AudioTimeStamp       *pAudioTS,
                                                  UInt32                      uBusID,
                                                  UInt32                      cFrames,
                                                  AudioBufferList            *pBufData)
{
    RT_NOREF(pActionFlags, pAudioTS, uBusID, cFrames);

    PCOREAUDIOSTREAM pStream = (PCOREAUDIOSTREAM)pvUser;

    /* Sanity. */
    AssertPtr(pStream);
    AssertPtr(pStream->pDrv);
    Assert   (pStream->enmDir == PDMAUDIODIR_OUT);
    AssertPtr(pStream->Unit.pDevice);

    if (ASMAtomicReadU32(&pStream->enmStatus) != COREAUDIOSTATUS_INIT)
    {
        pBufData->mBuffers[0].mDataByteSize = 0;
        return noErr;
    }

    /* How much space is used in the ring buffer? */
    size_t cbToRead = RT_MIN(RTCircBufUsed(pStream->pCircBuf), pBufData->mBuffers[0].mDataByteSize);
    if (!cbToRead)
    {
        pBufData->mBuffers[0].mDataByteSize = 0;
        return noErr;
    }

    uint8_t *pbSrc = NULL;
    size_t cbRead  = 0;

    size_t cbLeft  = cbToRead;
    while (cbLeft)
    {
        /* Try to acquire the necessary block from the ring buffer. */
        RTCircBufAcquireReadBlock(pStream->pCircBuf, cbLeft, (void **)&pbSrc, &cbToRead);

        /* Break if nothing is used anymore. */
        if (!cbToRead)
            break;

        /* Copy the data from our ring buffer to the core audio buffer. */
        memcpy((uint8_t *)pBufData->mBuffers[0].mData + cbRead, pbSrc, cbToRead);

        /* Release the read buffer, so it could be used for new data. */
        RTCircBufReleaseReadBlock(pStream->pCircBuf, cbToRead);

        /* Move offset. */
        cbRead += cbToRead;

        /* Check if we're lagging behind. */
        if (cbRead > pBufData->mBuffers[0].mDataByteSize)
        {
            LogRel2(("CoreAudio: Host output lagging behind, expect stuttering guest audio output\n"));
            cbRead = pBufData->mBuffers[0].mDataByteSize;
            break;
        }

        Assert(cbToRead <= cbLeft);
        cbLeft -= cbToRead;
    }

    /* Write the bytes to the core audio buffer which were really written. */
    Assert(pBufData->mBuffers[0].mDataByteSize >= cbRead);
    pBufData->mBuffers[0].mDataByteSize = cbRead;

    Log3Func(("Read %zu / %zu bytes\n", cbRead, cbToRead));

    return noErr;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                       void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pvBuf, cbBuf); /** @todo r=bird: this looks totally weird at first glance! */

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    PCOREAUDIOSTREAM  pCAStream = (PCOREAUDIOSTREAM)pStream;
    PDRVHOSTCOREAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

#ifndef VBOX_WITH_AUDIO_CALLBACKS
    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&pCAStream->enmStatus) == COREAUDIOSTATUS_REINIT)
    {
        /* For now re just re-initialize with the current input device. */
        if (pThis->pDefaultDevIn)
        {
            int rc2 = coreAudioStreamReinit(pThis, pCAStream, pThis->pDefaultDevIn);
            if (RT_FAILURE(rc2))
                return VERR_NOT_AVAILABLE;
        }
        else
            return VERR_NOT_AVAILABLE;
    }
#else
    RT_NOREF(pThis);
#endif

    if (ASMAtomicReadU32(&pCAStream->enmStatus) != COREAUDIOSTATUS_INIT)
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    uint32_t cbWrittenTotal = 0;

    do
    {
        size_t cbMixBuf  = AudioMixBufSizeBytes(&pStream->MixBuf);
        size_t cbToWrite = RT_MIN(cbMixBuf, RTCircBufUsed(pCAStream->pCircBuf));

        uint32_t cWritten, cbWritten;
        uint8_t *puBuf;
        size_t   cbToRead;

        Log3Func(("cbMixBuf=%zu, cbToWrite=%zu/%zu\n", cbMixBuf, cbToWrite, RTCircBufSize(pCAStream->pCircBuf)));

        while (cbToWrite)
        {
            /* Try to acquire the necessary block from the ring buffer. */
            RTCircBufAcquireReadBlock(pCAStream->pCircBuf, cbToWrite, (void **)&puBuf, &cbToRead);
            if (!cbToRead)
            {
                RTCircBufReleaseReadBlock(pCAStream->pCircBuf, cbToRead);
                break;
            }

#ifdef DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            rc = RTFileOpen(&fh, DEBUG_DUMP_PCM_DATA_PATH "ca-capture.pcm",
                            RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                RTFileWrite(fh, puBuf + cbWrittenTotal, cbToRead, NULL);
                RTFileClose(fh);
            }
            else
                AssertFailed();
#endif
            rc = AudioMixBufWriteCirc(&pStream->MixBuf, puBuf + cbWrittenTotal, cbToRead, &cWritten);

            /* Release the read buffer, so it could be used for new data. */
            RTCircBufReleaseReadBlock(pCAStream->pCircBuf, cbToRead);

            if (   RT_FAILURE(rc)
                || !cWritten)
            {
                RTCircBufReleaseReadBlock(pCAStream->pCircBuf, cbToRead);
                break;
            }

            cbWritten = AUDIOMIXBUF_S2B(&pStream->MixBuf, cWritten);

            Assert(cbToWrite >= cbWritten);
            cbToWrite      -= cbWritten;
            cbWrittenTotal += cbWritten;
        }

        Log3Func(("cbToWrite=%zu, cbToRead=%zu, cbWrittenTotal=%RU32, rc=%Rrc\n", cbToWrite, cbToRead, cbWrittenTotal, rc));
    }
    while (0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cCaptured     = 0;
        uint32_t cWrittenTotal = AUDIOMIXBUF_B2S(&pStream->MixBuf, cbWrittenTotal);
        if (cWrittenTotal)
            rc = AudioMixBufMixToParent(&pStream->MixBuf, cWrittenTotal, &cCaptured);

        Log3Func(("cWrittenTotal=%RU32 (%RU32 bytes), cCaptured=%RU32, rc=%Rrc\n", cWrittenTotal, cbWrittenTotal, cCaptured, rc));

        if (cCaptured)
            LogFlowFunc(("%RU32 samples captured\n", cCaptured));

        if (pcbRead)
            *pcbRead = cCaptured;
    }

    if (RT_FAILURE(rc))
        LogFunc(("Failed with rc=%Rrc\n", rc));

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                    PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf,
                                                    uint32_t *pcbWritten)
{
    RT_NOREF(pvBuf, cbBuf);

    PDRVHOSTCOREAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);
    PCOREAUDIOSTREAM  pCAStream = (PCOREAUDIOSTREAM)pStream;

#ifndef VBOX_WITH_AUDIO_CALLBACKS
    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&pCAStream->enmStatus) == COREAUDIOSTATUS_REINIT)
    {
        if (pThis->pDefaultDevOut)
        {
            /* For now re just re-initialize with the current output device. */
            int rc2 = coreAudioStreamReinit(pThis, pCAStream, pThis->pDefaultDevOut);
            if (RT_FAILURE(rc2))
                return VERR_NOT_AVAILABLE;
        }
        else
            return VERR_NOT_AVAILABLE;
    }
#else
    RT_NOREF(pThis);
#endif

    uint32_t cLive = AudioMixBufLive(&pStream->MixBuf);
    if (!cLive) /* Not live samples to play? Bail out. */
    {
        if (pcbWritten)
            *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    size_t cbLive  = AUDIOMIXBUF_S2B(&pStream->MixBuf, cLive);

    uint32_t cbReadTotal = 0;

    size_t cbToRead = RT_MIN(cbLive, RTCircBufFree(pCAStream->pCircBuf));
    Log3Func(("cbToRead=%zu\n", cbToRead));

    int rc = VINF_SUCCESS;

    while (cbToRead)
    {
        uint32_t cRead, cbRead;
        uint8_t *puBuf;
        size_t   cbCopy;

        /* Try to acquire the necessary space from the ring buffer. */
        RTCircBufAcquireWriteBlock(pCAStream->pCircBuf, cbToRead, (void **)&puBuf, &cbCopy);
        if (!cbCopy)
        {
            RTCircBufReleaseWriteBlock(pCAStream->pCircBuf, cbCopy);
            break;
        }

        Assert(cbCopy <= cbToRead);

        rc = AudioMixBufReadCirc(&pStream->MixBuf, puBuf, cbCopy, &cRead);
        if (   RT_FAILURE(rc)
            || !cRead)
        {
            RTCircBufReleaseWriteBlock(pCAStream->pCircBuf, 0);
            break;
        }

        cbRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead);

        /* Release the ring buffer, so the read thread could start reading this data. */
        RTCircBufReleaseWriteBlock(pCAStream->pCircBuf, cbRead);

        Assert(cbToRead >= cbRead);
        cbToRead -= cbRead;
        cbReadTotal += cbRead;
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pStream->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pStream->MixBuf, cReadTotal);

        Log3Func(("cReadTotal=%RU32 (%RU32 bytes)\n", cReadTotal, cbReadTotal));

        if (pcbWritten)
            *pcbWritten = cReadTotal;
    }

    return rc;
}

static DECLCALLBACK(int) coreAudioStreamControl(PDRVHOSTCOREAUDIO pThis,
                                                PCOREAUDIOSTREAM pCAStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pThis);

    uint32_t enmStatus = ASMAtomicReadU32(&pCAStream->enmStatus);

    LogFlowFunc(("enmStreamCmd=%RU32, enmStatus=%RU32\n", enmStreamCmd, enmStatus));

    if (!(   enmStatus == COREAUDIOSTATUS_INIT
#ifndef VBOX_WITH_AUDIO_CALLBACKS
          || enmStatus == COREAUDIOSTATUS_REINIT
#endif
          ))
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;

    OSStatus err;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Only start the device if it is actually stopped */
            if (!coreAudioUnitIsRunning(pCAStream))
            {
                err = AudioUnitReset(pCAStream->Unit.audioUnit, kAudioUnitScope_Input, 0);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to reset AudioUnit (%RI32)\n", err));
                    /* Keep going. */
                }

                RTCircBufReset(pCAStream->pCircBuf);

                err = AudioOutputUnitStart(pCAStream->Unit.audioUnit);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to start playback (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                }
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /* Only stop the device if it is actually running */
            if (coreAudioUnitIsRunning(pCAStream))
            {
                err = AudioOutputUnitStop(pCAStream->Unit.audioUnit);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to stop playback (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
                }

                err = AudioUnitReset(pCAStream->Unit.audioUnit, kAudioUnitScope_Input, 0);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to reset AudioUnit (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                }
            }
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
 * @interface_method_impl{PDMIHOSTAUDIO, pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostCoreAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RT_BZERO(pBackendCfg, sizeof(PDMAUDIOBACKENDCFG));

    pBackendCfg->cbStreamIn  = sizeof(COREAUDIOSTREAM);
    pBackendCfg->cbStreamOut = sizeof(COREAUDIOSTREAM);

    pBackendCfg->cMaxStreamsIn  = UINT32_MAX;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    LogFlowFunc(("Returning %Rrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnGetDevices}
 */
static DECLCALLBACK(int) drvHostCoreAudioGetDevices(PPDMIHOSTAUDIO pInterface, PPDMAUDIODEVICEENUM pDeviceEnum)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pDeviceEnum, VERR_INVALID_POINTER);

    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        rc = coreAudioEnumerateDevices(pThis);
        if (RT_SUCCESS(rc))
        {
            if (pDeviceEnum)
            {
                rc = DrvAudioHlpDeviceEnumInit(pDeviceEnum);
                if (RT_SUCCESS(rc))
                    rc = DrvAudioHlpDeviceEnumCopy(pDeviceEnum, &pThis->Devices);

                if (RT_FAILURE(rc))
                    DrvAudioHlpDeviceEnumFree(pDeviceEnum);
            }
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}


#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnSetCallback}
 */
static DECLCALLBACK(int) drvHostCoreAudioSetCallback(PPDMIHOSTAUDIO pInterface, PFNPDMHOSTAUDIOCALLBACK pfnCallback)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pfnCallback will be handled below. */

    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("pfnCallback=%p\n", pfnCallback));

        if (pfnCallback) /* Register. */
        {
            Assert(pThis->pfnCallback == NULL);
            pThis->pfnCallback = pfnCallback;
        }
        else /* Unregister. */
        {
            if (pThis->pfnCallback)
                pThis->pfnCallback = NULL;
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return rc;
}
#endif


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostCoreAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                      PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    PCOREAUDIOSTREAM pCAStream = (PCOREAUDIOSTREAM)pStream;

    int rc;

    /* Input or output device? */
    bool fIn = pCfgReq->enmDir == PDMAUDIODIR_IN;

    /* For now, just use the default device available. */
    PPDMAUDIODEVICE pDev = fIn ? pThis->pDefaultDevIn : pThis->pDefaultDevOut;

    LogFunc(("pStream=%p, pCfgReq=%p, pCfgAcq=%p, fIn=%RTbool, pDev=%p\n", pStream, pCfgReq, pCfgAcq, fIn, pDev));

    if (pDev) /* (Default) device available? */
    {
        /* Init the Core Audio stream. */
        rc = coreAudioStreamInit(pCAStream, pThis, pDev);
        if (RT_SUCCESS(rc))
        {
            /* Sanity. */
            AssertPtr(pDev->pvData);
            Assert(pDev->cbData);

            if (RT_SUCCESS(rc))
            {
                if (fIn)
                    rc = coreAudioStreamInitIn (pCAStream, pCfgReq, pCfgAcq);
                else
                    rc = coreAudioStreamInitOut(pCAStream, pCfgReq, pCfgAcq);

                if (RT_FAILURE(rc))
                {
                    int rc2 = coreAudioStreamUninit(pCAStream);
                    AssertRC(rc2);
                }
            }
        }
    }
    else
        rc = VERR_NOT_AVAILABLE;

    LogFunc(("Returning %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    PCOREAUDIOSTREAM pCAStream = (PCOREAUDIOSTREAM)pStream;

    uint32_t status = ASMAtomicReadU32(&pCAStream->enmStatus);
    if (!(   status == COREAUDIOSTATUS_INIT
#ifndef VBOX_WITH_AUDIO_CALLBACKS
          || status == COREAUDIOSTATUS_REINIT
#endif
          ))
    {
        return VINF_SUCCESS;
    }

    int rc = coreAudioStreamControl(pThis, pCAStream, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
    {
        ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_IN_UNINIT);

        rc = coreAudioStreamUninit(pCAStream);
        if (RT_SUCCESS(rc))
            ASMAtomicXchgU32(&pCAStream->enmStatus, COREAUDIOSTATUS_UNINIT);
    }

    LogFlowFunc(("%s: rc=%Rrc\n", pStream->szName, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamControl(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    PCOREAUDIOSTREAM pCAStream = (PCOREAUDIOSTREAM)pStream;

    return coreAudioStreamControl(pThis, pCAStream, enmStreamCmd);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostCoreAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    PDMAUDIOSTRMSTS strmSts = PDMAUDIOSTRMSTS_FLAG_NONE;

    PCOREAUDIOSTREAM pCAStream = (PCOREAUDIOSTREAM)pStream;

    if (ASMAtomicReadU32(&pCAStream->enmStatus) == COREAUDIOSTATUS_INIT)
        strmSts |= PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED;

    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
        if (strmSts & PDMAUDIOSTRMSTS_FLAG_ENABLED)
            strmSts |= PDMAUDIOSTRMSTS_FLAG_DATA_READABLE;
    }
    else if (pStream->enmDir == PDMAUDIODIR_OUT)
    {
        if (strmSts & PDMAUDIOSTRMSTS_FLAG_ENABLED)
            strmSts |= PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE;
    }
    else
        AssertFailed();

    return strmSts;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostCoreAudioStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    /* Nothing to do here for Core Audio. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnInit}
 */
static DECLCALLBACK(int) drvHostCoreAudioInit(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    int rc = DrvAudioHlpDeviceEnumInit(&pThis->Devices);
    if (RT_SUCCESS(rc))
    {
        /* Do the first (initial) internal device enumeration. */
        rc = coreAudioEnumerateDevices(pThis);
    }

    if (RT_SUCCESS(rc))
    {
        /* Register system callbacks. */
        AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMaster };

        OSStatus err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propAdr,
                                                      coreAudioDefaultDeviceChangedCb, pThis /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareIllegalOperationError)
        {
            LogRel(("CoreAudio: Failed to add the input default device changed listener (%RI32)\n", err));
        }

        propAdr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propAdr,
                                             coreAudioDefaultDeviceChangedCb, pThis /* pvUser */);
        if (   err != noErr
            && err != kAudioHardwareIllegalOperationError)
        {
            LogRel(("CoreAudio: Failed to add the output default device changed listener (%RI32)\n", err));
        }
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO, pfnShutdown}
 */
static DECLCALLBACK(void) drvHostCoreAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTCOREAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTCOREAUDIO(pInterface);

    /*
     * Unregister system callbacks.
     */
    AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };

    OSStatus err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propAdr,
                                                     coreAudioDefaultDeviceChangedCb, pThis /* pvUser */);
    if (   err != noErr
        && err != kAudioHardwareBadObjectError)
    {
        LogRel(("CoreAudio: Failed to remove the default input device changed listener (%RI32)\n", err));
    }

    propAdr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propAdr,
                                            coreAudioDefaultDeviceChangedCb, pThis /* pvUser */);
    if (   err != noErr
        && err != kAudioHardwareBadObjectError)
    {
        LogRel(("CoreAudio: Failed to remove the default output device changed listener (%RI32)\n", err));
    }

    LogFlowFuncEnter();
}


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
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostCoreAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostCoreAudio);

    /* This backend supports device enumeration. */
    pThis->IHostAudio.pfnGetDevices  = drvHostCoreAudioGetDevices;

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /* This backend supports host audio callbacks. */
    pThis->IHostAudio.pfnSetCallback = drvHostCoreAudioSetCallback;
    pThis->pfnCallback               = NULL;
#endif

    int rc = RTCritSectInit(&pThis->CritSect);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @callback_method_impl{FNPDMDRVDESTRUCT}
 */
static DECLCALLBACK(void) drvHostCoreAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc2);
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
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

