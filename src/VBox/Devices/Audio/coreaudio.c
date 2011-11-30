/* $Id$ */
/** @file
 * VBox audio devices: Mac OS X CoreAudio audio driver
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/cdefs.h>

#define AUDIO_CAP "coreaudio"
#include "vl_vbox.h"
#include "audio.h"
#include "audio_int.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioConverter.h>

/* todo:
 * - maybe make sure the threads are immediately stopped if playing/recording stops
 */

/* Most of this is based on:
 * http://developer.apple.com/mac/library/technotes/tn2004/tn2097.html
 * http://developer.apple.com/mac/library/technotes/tn2002/tn2091.html
 * http://developer.apple.com/mac/library/qa/qa2007/qa1533.html
 * http://developer.apple.com/mac/library/qa/qa2001/qa1317.html
 * http://developer.apple.com/mac/library/documentation/AudioUnit/Reference/AUComponentServicesReference/Reference/reference.html
 */

/*#define CA_EXTENSIVE_LOGGING*/

/*******************************************************************************
 *
 * IO Ring Buffer section
 *
 ******************************************************************************/

/* Implementation of a lock free ring buffer which could be used in a multi
 * threaded environment. Note that only the acquire, release and getter
 * functions are threading aware. So don't use reset if the ring buffer is
 * still in use. */
typedef struct IORINGBUFFER
{
    /* The current read position in the buffer */
    uint32_t uReadPos;
    /* The current write position in the buffer */
    uint32_t uWritePos;
    /* How much space of the buffer is currently in use */
    volatile uint32_t cBufferUsed;
    /* How big is the buffer */
    uint32_t cBufSize;
    /* The buffer itself */
    char *pBuffer;
} IORINGBUFFER;
/* Pointer to an ring buffer structure */
typedef IORINGBUFFER* PIORINGBUFFER;


static void IORingBufferCreate(PIORINGBUFFER *ppBuffer, uint32_t cSize)
{
    PIORINGBUFFER pTmpBuffer;

    AssertPtr(ppBuffer);

    *ppBuffer = NULL;
    pTmpBuffer = RTMemAllocZ(sizeof(IORINGBUFFER));
    if (pTmpBuffer)
    {
        pTmpBuffer->pBuffer = RTMemAlloc(cSize);
        if(pTmpBuffer->pBuffer)
        {
            pTmpBuffer->cBufSize = cSize;
            *ppBuffer = pTmpBuffer;
        }
        else
            RTMemFree(pTmpBuffer);
    }
}

static void IORingBufferDestroy(PIORINGBUFFER pBuffer)
{
    if (pBuffer)
    {
        if (pBuffer->pBuffer)
            RTMemFree(pBuffer->pBuffer);
        RTMemFree(pBuffer);
    }
}

DECL_FORCE_INLINE(void) IORingBufferReset(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);

    pBuffer->uReadPos = 0;
    pBuffer->uWritePos = 0;
    pBuffer->cBufferUsed = 0;
}

DECL_FORCE_INLINE(uint32_t) IORingBufferFree(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferUsed(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferSize(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize;
}

static void IORingBufferAquireReadBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uUsed = 0;
    uint32_t uSize = 0;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is in use? */
    uUsed = ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uUsed > 0)
    {
        /* Get the size out of the requested size, the read block till the end
         * of the buffer & the currently used size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uReadPos, uUsed));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current read
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uReadPos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseReadBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uReadPos = (pBuffer->uReadPos + cSize) % pBuffer->cBufSize;

    ASMAtomicSubU32(&pBuffer->cBufferUsed, cSize);
}

static void IORingBufferAquireWriteBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uFree;
    uint32_t uSize;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is free? */
    uFree = pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uFree > 0)
    {
        /* Get the size out of the requested size, the write block till the end
         * of the buffer & the currently free size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uWritePos, uFree));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current write
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uWritePos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseWriteBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uWritePos = (pBuffer->uWritePos + cSize) % pBuffer->cBufSize;

    ASMAtomicAddU32(&pBuffer->cBufferUsed, cSize);
}

/*******************************************************************************
 *
 * Helper function section
 *
 ******************************************************************************/

#if DEBUG
static void caDebugOutputAudioStreamBasicDescription(const char *pszDesc, const AudioStreamBasicDescription *pStreamDesc)
{
    char pszSampleRate[32];
    Log(("%s AudioStreamBasicDescription:\n", pszDesc));
    Log(("CoreAudio: Format ID: %RU32 (%c%c%c%c)\n", pStreamDesc->mFormatID, RT_BYTE4(pStreamDesc->mFormatID), RT_BYTE3(pStreamDesc->mFormatID), RT_BYTE2(pStreamDesc->mFormatID), RT_BYTE1(pStreamDesc->mFormatID)));
    Log(("CoreAudio: Flags: %RU32", pStreamDesc->mFormatFlags));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsFloat)
        Log((" Float"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsBigEndian)
        Log((" BigEndian"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsSignedInteger)
        Log((" SignedInteger"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsPacked)
        Log((" Packed"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsAlignedHigh)
        Log((" AlignedHigh"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
        Log((" NonInterleaved"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagIsNonMixable)
        Log((" NonMixable"));
    if (pStreamDesc->mFormatFlags & kAudioFormatFlagsAreAllClear)
        Log((" AllClear"));
    Log(("\n"));
    snprintf(pszSampleRate, 32, "%.2f", (float)pStreamDesc->mSampleRate);
    Log(("CoreAudio: SampleRate: %s\n", pszSampleRate));
    Log(("CoreAudio: ChannelsPerFrame: %RU32\n", pStreamDesc->mChannelsPerFrame));
    Log(("CoreAudio: FramesPerPacket: %RU32\n", pStreamDesc->mFramesPerPacket));
    Log(("CoreAudio: BitsPerChannel: %RU32\n", pStreamDesc->mBitsPerChannel));
    Log(("CoreAudio: BytesPerFrame: %RU32\n", pStreamDesc->mBytesPerFrame));
    Log(("CoreAudio: BytesPerPacket: %RU32\n", pStreamDesc->mBytesPerPacket));
}
#endif /* DEBUG */

static void caPCMInfoToAudioStreamBasicDescription(struct audio_pcm_info *pInfo, AudioStreamBasicDescription *pStreamDesc)
{
    pStreamDesc->mFormatID = kAudioFormatLinearPCM;
    pStreamDesc->mFormatFlags = kAudioFormatFlagIsPacked;
    pStreamDesc->mFramesPerPacket = 1;
    pStreamDesc->mSampleRate = (Float64)pInfo->freq;
    pStreamDesc->mChannelsPerFrame = pInfo->nchannels;
    pStreamDesc->mBitsPerChannel = pInfo->bits;
    if (pInfo->sign == 1)
        pStreamDesc->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    pStreamDesc->mBytesPerFrame = pStreamDesc->mChannelsPerFrame * (pStreamDesc->mBitsPerChannel / 8);
    pStreamDesc->mBytesPerPacket = pStreamDesc->mFramesPerPacket * pStreamDesc->mBytesPerFrame;
}

static OSStatus caSetFrameBufferSize(AudioDeviceID device, bool fInput, UInt32 cReqSize, UInt32 *pcActSize)
{
    OSStatus err = noErr;
    UInt32 cSize = 0;
    AudioValueRange *pRange = NULL;
    size_t a = 0;
    Float64 cMin = -1;
    Float64 cMax = -1;

    /* First try to set the new frame buffer size. */
    AudioDeviceSetProperty(device,
                           NULL,
                           0,
                           fInput,
                           kAudioDevicePropertyBufferFrameSize,
                           sizeof(cReqSize),
                           &cReqSize);
    /* Check if it really was set. */
    cSize = sizeof(*pcActSize);
    err = AudioDeviceGetProperty(device,
                                 0,
                                 fInput,
                                 kAudioDevicePropertyBufferFrameSize,
                                 &cSize,
                                 pcActSize);
    if (RT_UNLIKELY(err != noErr))
        return err;
    /* If both sizes are the same, we are done. */
    if (cReqSize == *pcActSize)
        return noErr;
    /* If not we have to check the limits of the device. First get the size of
       the buffer size range property. */
    err = AudioDeviceGetPropertyInfo(device,
                                     0,
                                     fInput,
                                     kAudioDevicePropertyBufferSizeRange,
                                     &cSize,
                                     NULL);
    if (RT_UNLIKELY(err != noErr))
        return err;
    pRange = RTMemAllocZ(cSize);
    if (RT_VALID_PTR(pRange))
    {
        err = AudioDeviceGetProperty(device,
                                     0,
                                     fInput,
                                     kAudioDevicePropertyBufferSizeRange,
                                     &cSize,
                                     pRange);
        if (RT_LIKELY(err == noErr))
        {
            for (a=0; a < cSize/sizeof(AudioValueRange); ++a)
            {
                /* Search for the absolute minimum. */
                if (   pRange[a].mMinimum < cMin
                    || cMin == -1)
                    cMin = pRange[a].mMinimum;
                /* Search for the best maximum which isn't bigger than
                   cReqSize. */
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
            AudioDeviceSetProperty(device,
                                   NULL,
                                   0,
                                   fInput,
                                   kAudioDevicePropertyBufferFrameSize,
                                   sizeof(cReqSize),
                                   &cReqSize);
            /* Check if it really was set. */
            cSize = sizeof(*pcActSize);
            err = AudioDeviceGetProperty(device,
                                         0,
                                         fInput,
                                         kAudioDevicePropertyBufferFrameSize,
                                         &cSize,
                                         pcActSize);
        }
    }
    else
        return notEnoughMemoryErr;

    RTMemFree(pRange);
    return err;
}

DECL_FORCE_INLINE(bool) caIsRunning(AudioDeviceID deviceID)
{
    OSStatus err = noErr;
    UInt32 uFlag = 0;
    UInt32 uSize = sizeof(uFlag);

    err = AudioDeviceGetProperty(deviceID,
                                 0,
                                 0,
                                 kAudioDevicePropertyDeviceIsRunning,
                                 &uSize,
                                 &uFlag);
    if (err != kAudioHardwareNoError)
        LogRel(("CoreAudio: Could not determine whether the device is running (%RI32)\n", err));
    return uFlag >= 1;
}

static char* caCFStringToCString(const CFStringRef pCFString)
{
    char *pszResult = NULL;
    CFIndex cLen;
#if 0
    /**
     * CFStringGetCStringPtr doesn't reliably return requested string instead return depends on "many factors" (not clear which)
     * ( please follow the link
     *  http://developer.apple.com/library/mac/#documentation/CoreFoundation/Reference/CFStringRef/Reference/reference.html
     * for more details). Branch below allocates memory using mechanisms which hasn't got single method for memory free:
     * RTStrDup - RTStrFree
     * RTMemAllocZTag - RTMemFree
     * which aren't compatible, opposite to CFStringGetCStringPtr CFStringGetCString has well defined
     * behaviour and confident return value.
     */
    const char *pszTmp = NULL;

    /* First try to get the pointer directly. */
    pszTmp = CFStringGetCStringPtr(pCFString, kCFStringEncodingUTF8);
    if (pszTmp)
    {
        /* On success make a copy */
        pszResult = RTStrDup(pszTmp);
    }
    else
    {
        /* If the pointer isn't available directly, we have to make a copy. */
        cLen = CFStringGetLength(pCFString) + 1;
        pszResult = RTMemAllocZTag(cLen * sizeof(char), RTSTR_TAG);
        if (!CFStringGetCString(pCFString, pszResult, cLen, kCFStringEncodingUTF8))
        {
            RTStrFree(pszResult);
            pszResult = NULL;
        }
    }
#else
    /* If the pointer isn't available directly, we have to make a copy. */
    cLen = CFStringGetLength(pCFString) + 1;
    pszResult = RTMemAllocZTag(cLen * sizeof(char), RTSTR_TAG);
    if (!CFStringGetCString(pCFString, pszResult, cLen, kCFStringEncodingUTF8))
    {
        RTStrFree(pszResult);
        pszResult = NULL;
    }
#endif

    return pszResult;
}

static AudioDeviceID caDeviceUIDtoID(const char* pszUID)
{
    OSStatus err = noErr;
    UInt32 uSize;
    AudioValueTranslation translation;
    CFStringRef strUID;
    AudioDeviceID audioId;

    /* Create a CFString out of our CString */
    strUID = CFStringCreateWithCString(NULL,
                                       pszUID,
                                       kCFStringEncodingMacRoman);

    /* Fill the translation structure */
    translation.mInputData = &strUID;
    translation.mInputDataSize = sizeof(CFStringRef);
    translation.mOutputData = &audioId;
    translation.mOutputDataSize = sizeof(AudioDeviceID);
    uSize = sizeof(AudioValueTranslation);
    /* Fetch the translation from the UID to the audio Id */
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID,
                                   &uSize,
                                   &translation);
    /* Release the temporary CFString */
    CFRelease(strUID);

    if (RT_LIKELY(err == noErr))
        return audioId;
    /* Return the unknown device on error */
    return kAudioDeviceUnknown;
}

/*******************************************************************************
 *
 * Global structures section
 *
 ******************************************************************************/

/* Initialization status indicator used for the recreation of the AudioUnits. */
#define CA_STATUS_UNINIT    UINT32_C(0) /* The device is uninitialized */
#define CA_STATUS_IN_INIT   UINT32_C(1) /* The device is currently initializing */
#define CA_STATUS_INIT      UINT32_C(2) /* The device is initialized */
#define CA_STATUS_IN_UNINIT UINT32_C(3) /* The device is currently uninitializing */
#define CA_STATUS_REINIT    UINT32_C(4) /* The device has to be reinitialized */

/* Error code which indicates "End of data" */
static const OSStatus caConverterEOFDErr = 0x656F6664; /* 'eofd' */

struct
{
    const char *pszOutputDeviceUID;
    const char *pszInputDeviceUID;
} conf =
{
    INIT_FIELD(.pszOutputDeviceUID =) NULL,
    INIT_FIELD(.pszInputDeviceUID =) NULL
};

typedef struct caVoiceOut
{
    /* HW voice output structure defined by VBox */
    HWVoiceOut hw;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID audioDeviceId;
    /* The AudioUnit used */
    AudioUnit audioUnit;
    /* A ring buffer for transferring data to the playback thread */
    PIORINGBUFFER pBuf;
    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;
} caVoiceOut;

typedef struct caVoiceIn
{
    /* HW voice input structure defined by VBox */
    HWVoiceIn hw;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID audioDeviceId;
    /* The AudioUnit used */
    AudioUnit audioUnit;
    /* The audio converter if necessary */
    AudioConverterRef converter;
    /* A temporary position value used in the caConverterCallback function */
    uint32_t rpos;
    /* The ratio between the device & the stream sample rate */
    Float64 sampleRatio;
    /* An extra buffer used for render the audio data in the recording thread */
    AudioBufferList bufferList;
    /* A ring buffer for transferring data from the recording thread */
    PIORINGBUFFER pBuf;
    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;
} caVoiceIn;

#ifdef CA_EXTENSIVE_LOGGING
# define CA_EXT_DEBUG_LOG(a) Log2(a)
#else
# define CA_EXT_DEBUG_LOG(a) do {} while(0)
#endif

/*******************************************************************************
 *
 * CoreAudio output section
 *
 ******************************************************************************/

/* We need some forward declarations */
static int coreaudio_run_out(HWVoiceOut *hw);
static int coreaudio_write(SWVoiceOut *sw, void *buf, int len);
static int coreaudio_ctl_out(HWVoiceOut *hw, int cmd, ...);
static void coreaudio_fini_out(HWVoiceOut *hw);
static int coreaudio_init_out(HWVoiceOut *hw, audsettings_t *as);
static int caInitOutput(HWVoiceOut *hw);
static void caReinitOutput(HWVoiceOut *hw);

/* Callback for getting notified when the default output device was changed */
static DECLCALLBACK(OSStatus) caPlaybackDefaultDeviceChanged(AudioHardwarePropertyID inPropertyID,
                                                             void *inClientData)
{
    OSStatus err = noErr;
    UInt32 uSize = 0;
    UInt32 ad = 0;
    bool fRun = false;

    caVoiceOut *caVoice = (caVoiceOut *) inClientData;

    switch (inPropertyID)
    {
        case kAudioHardwarePropertyDefaultOutputDevice:
            {
                /* This listener is called on every change of the hardware
                 * device. So check if the default device has really changed. */
                uSize = sizeof(caVoice->audioDeviceId);
                err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                               &uSize,
                                               &ad);
                if (caVoice->audioDeviceId != ad)
                {
                    Log2(("CoreAudio: [Output] Default output device changed!\n"));
                    /* We move the reinitialization to the next output event.
                     * This make sure this thread isn't blocked and the
                     * reinitialization is done when necessary only. */
                    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_REINIT);
                }
                break;
            }
    }

    return noErr;
}

/* Callback for getting notified when some of the properties of an audio device has changed */
static DECLCALLBACK(OSStatus) caPlaybackAudioDevicePropertyChanged(AudioDeviceID inDevice,
                                                                   UInt32 inChannel,
                                                                   Boolean isInput,
                                                                   AudioDevicePropertyID inPropertyID,
                                                                   void *inClientData)
{
    switch (inPropertyID)
    {
#ifdef DEBUG
        case kAudioDeviceProcessorOverload:
            {
                Log2(("CoreAudio: [Output] Processor overload detected!\n"));
                break;
            }
#endif /* DEBUG */
        default: break;
    }

    return noErr;
}

/* Callback to feed audio output buffer */
static DECLCALLBACK(OSStatus) caPlaybackCallback(void* inRefCon,
                                                 AudioUnitRenderActionFlags* ioActionFlags,
                                                 const AudioTimeStamp* inTimeStamp,
                                                 UInt32 inBusNumber,
                                                 UInt32 inNumberFrames,
                                                 AudioBufferList* ioData)
{
    uint32_t csAvail = 0;
    uint32_t cbToRead = 0;
    uint32_t csToRead = 0;
    uint32_t csReads = 0;
    char *pcSrc = NULL;

    caVoiceOut *caVoice = (caVoiceOut *) inRefCon;

    if (ASMAtomicReadU32(&caVoice->status) != CA_STATUS_INIT)
        return noErr;

    /* How much space is used in the ring buffer? */
    csAvail = IORingBufferUsed(caVoice->pBuf) >> caVoice->hw.info.shift; /* bytes -> samples */
    /* How much space is available in the core audio buffer. Use the smaller
     * size of the too. */
    csAvail = RT_MIN(csAvail, ioData->mBuffers[0].mDataByteSize >> caVoice->hw.info.shift);

    CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Start reading buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

    /* Iterate as long as data is available */
    while(csReads < csAvail)
    {
        /* How much is left? */
        csToRead = csAvail - csReads;
        cbToRead = csToRead << caVoice->hw.info.shift; /* samples -> bytes */
        CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Try reading %RU32 samples (%RU32 bytes)\n", csToRead, cbToRead));
        /* Try to acquire the necessary block from the ring buffer. */
        IORingBufferAquireReadBlock(caVoice->pBuf, cbToRead, &pcSrc, &cbToRead);
        /* How much to we get? */
        csToRead = cbToRead >> caVoice->hw.info.shift; /* bytes -> samples */
        CA_EXT_DEBUG_LOG(("CoreAudio: [Output] There are %RU32 samples (%RU32 bytes) available\n", csToRead, cbToRead));
        /* Break if nothing is used anymore. */
        if (RT_UNLIKELY(cbToRead == 0))
            break;
        /* Copy the data from our ring buffer to the core audio buffer. */
        memcpy((char*)ioData->mBuffers[0].mData + (csReads << caVoice->hw.info.shift), pcSrc, cbToRead);
        /* Release the read buffer, so it could be used for new data. */
        IORingBufferReleaseReadBlock(caVoice->pBuf, cbToRead);
        /* How much have we reads so far. */
        csReads += csToRead;
    }
    /* Write the bytes to the core audio buffer which where really written. */
    ioData->mBuffers[0].mDataByteSize = csReads << caVoice->hw.info.shift; /* samples -> bytes */

    CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Finished reading buffer with %RU32 samples (%RU32 bytes)\n", csReads, csReads << caVoice->hw.info.shift));

    return noErr;
}

static int caInitOutput(HWVoiceOut *hw)
{
    OSStatus err = noErr;
    UInt32 uSize = 0; /* temporary size of properties */
    UInt32 uFlag = 0; /* for setting flags */
    CFStringRef name; /* for the temporary device name fetching */
    char *pszName = NULL;
    char *pszUID = NULL;
    ComponentDescription cd; /* description for an audio component */
    Component cp; /* an audio component */
    AURenderCallbackStruct cb; /* holds the callback structure */
    UInt32 cFrames; /* default frame count */
    UInt32 cSamples; /* samples count */

    caVoiceOut *caVoice = (caVoiceOut *) hw;

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_IN_INIT);

    if (caVoice->audioDeviceId == kAudioDeviceUnknown)
    {
        /* Fetch the default audio output device currently in use */
        uSize = sizeof(caVoice->audioDeviceId);
        err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                       &uSize,
                                       &caVoice->audioDeviceId);
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Output] Unable to find default output device (%RI32)\n", err));
            return -1;
        }
    }

    /* Try to get the name of the output device and log it. It's not fatal if
     * it fails. */
    uSize = sizeof(CFStringRef);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 0,
                                 kAudioObjectPropertyName,
                                 &uSize,
                                 &name);
    if (RT_LIKELY(err == noErr))
    {
        pszName = caCFStringToCString(name);
        CFRelease(name);
        err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                     0,
                                     0,
                                     kAudioDevicePropertyDeviceUID,
                                     &uSize,
                                     &name);
        if (RT_LIKELY(err == noErr))
        {
            pszUID = caCFStringToCString(name);
            CFRelease(name);
            if (pszName && pszUID)
                LogRel(("CoreAudio: Using output device: %s (UID: %s)\n", pszName, pszUID));
            RTMemFree(pszUID);
        }
        RTMemFree(pszName);
    }
    else
        LogRel(("CoreAudio: [Output] Unable to get output device name (%RI32)\n", err));

    /* Get the default frames buffer size, so that we can setup our internal
     * buffers. */
    uSize = sizeof(cFrames);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 false,
                                 kAudioDevicePropertyBufferFrameSize,
                                 &uSize,
                                 &cFrames);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to get frame buffer size of the audio device (%RI32)\n", err));
        return -1;
    }
    /* Set the frame buffer size and honor any minimum/maximum restrictions on
       the device. */
    err = caSetFrameBufferSize(caVoice->audioDeviceId,
                               false,
                               cFrames,
                               &cFrames);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set frame buffer size on the audio device (%RI32)\n", err));
        return -1;
    }

    cd.componentType = kAudioUnitType_Output;
    cd.componentSubType = kAudioUnitSubType_HALOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    /* Try to find the default HAL output component. */
    cp = FindNextComponent(NULL, &cd);
    if (RT_UNLIKELY(cp == 0))
    {
        LogRel(("CoreAudio: [Output] Failed to find HAL output component\n"));
        return -1;
    }

    /* Open the default HAL output component. */
    err = OpenAComponent(cp, &caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to open output component (%RI32)\n", err));
        return -1;
    }

    /* Switch the I/O mode for output to on. */
    uFlag = 1;
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output,
                               0,
                               &uFlag,
                               sizeof(uFlag));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set output I/O mode enabled (%RI32)\n", err));
        return -1;
    }

    /* Set the default audio output device as the device for the new AudioUnit. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Output,
                               0,
                               &caVoice->audioDeviceId,
                               sizeof(caVoice->audioDeviceId));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set current device (%RI32)\n", err));
        return -1;
    }

    /* CoreAudio will inform us on a second thread when it needs more data for
     * output. Therefor register an callback function which will provide the new
     * data. */
    cb.inputProc = caPlaybackCallback;
    cb.inputProcRefCon = caVoice;

    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &cb,
                               sizeof(cb));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set callback (%RI32)\n", err));
        return -1;
    }

    /* Set the quality of the output render to the maximum. */
/*    uFlag = kRenderQuality_High;*/
/*    err = AudioUnitSetProperty(caVoice->audioUnit,*/
/*                               kAudioUnitProperty_RenderQuality,*/
/*                               kAudioUnitScope_Global,*/
/*                               0,*/
/*                               &uFlag,*/
/*                               sizeof(uFlag));*/
    /* Not fatal */
/*    if (RT_UNLIKELY(err != noErr))*/
/*        LogRel(("CoreAudio: [Output] Failed to set the render quality to the maximum (%RI32)\n", err));*/

    /* Fetch the current stream format of the device. */
    uSize = sizeof(caVoice->deviceFormat);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0,
                               &caVoice->deviceFormat,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to get device format (%RI32)\n", err));
        return -1;
    }

    /* Create an AudioStreamBasicDescription based on the audio settings of
     * VirtualBox. */
    caPCMInfoToAudioStreamBasicDescription(&caVoice->hw.info, &caVoice->streamFormat);

#if DEBUG
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Output] device", &caVoice->deviceFormat);
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Output] output", &caVoice->streamFormat);
#endif /* DEBUG */

    /* Set the device format description for the stream. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0,
                               &caVoice->streamFormat,
                               sizeof(caVoice->streamFormat));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set stream format (%RI32)\n", err));
        return -1;
    }

    uSize = sizeof(caVoice->deviceFormat);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0,
                               &caVoice->deviceFormat,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to get device format (%RI32)\n", err));
        return -1;
    }

    /* Also set the frame buffer size off the device on our AudioUnit. This
       should make sure that the frames count which we receive in the render
       thread is as we like. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_MaximumFramesPerSlice,
                               kAudioUnitScope_Global,
                               0,
                               &cFrames,
                               sizeof(cFrames));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set maximum frame buffer size on the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Finally initialize the new AudioUnit. */
    err = AudioUnitInitialize(caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to initialize the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
     * the frame buffer size set in the previous calls. So finally get the
     * frame buffer size after the AudioUnit was initialized. */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_MaximumFramesPerSlice,
                               kAudioUnitScope_Global,
                               0,
                               &cFrames,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to get maximum frame buffer size from the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Create the internal ring buffer. */
    cSamples = cFrames * caVoice->streamFormat.mChannelsPerFrame;
    IORingBufferCreate(&caVoice->pBuf, cSamples << hw->info.shift);
    if (!RT_VALID_PTR(caVoice->pBuf))
    {
        LogRel(("CoreAudio: [Output] Failed to create internal ring buffer\n"));
        AudioUnitUninitialize(caVoice->audioUnit);
        return -1;
    }

    if (   hw->samples != 0
        && hw->samples != (int32_t)cSamples)
        LogRel(("CoreAudio: [Output] Warning! After recreation, the CoreAudio ring buffer doesn't has the same size as the device buffer (%RU32 vs. %RU32).\n", cSamples, (uint32_t)hw->samples));

#ifdef DEBUG
    err = AudioDeviceAddPropertyListener(caVoice->audioDeviceId,
                                         0,
                                         false,
                                         kAudioDeviceProcessorOverload,
                                         caPlaybackAudioDevicePropertyChanged,
                                         caVoice);
    /* Not Fatal */
    if (RT_UNLIKELY(err != noErr))
        LogRel(("CoreAudio: [Output] Failed to add the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_INIT);

    Log(("CoreAudio: [Output] Frame count: %RU32\n", cFrames));

    return 0;
}

static void caReinitOutput(HWVoiceOut *hw)
{
    caVoiceOut *caVoice = (caVoiceOut *) hw;

    coreaudio_fini_out(&caVoice->hw);
    caInitOutput(&caVoice->hw);

    coreaudio_ctl_out(&caVoice->hw, VOICE_ENABLE);
}

static int coreaudio_run_out(HWVoiceOut *hw)
{
    uint32_t csAvail = 0;
    uint32_t cbToWrite = 0;
    uint32_t csToWrite = 0;
    uint32_t csWritten = 0;
    char *pcDst = NULL;
    st_sample_t *psSrc = NULL;

    caVoiceOut *caVoice = (caVoiceOut *) hw;

    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&caVoice->status) == CA_STATUS_REINIT)
        caReinitOutput(&caVoice->hw);

    /* We return the live count in the case we are not initialized. This should
     * prevent any under runs. */
    if (ASMAtomicReadU32(&caVoice->status) != CA_STATUS_INIT)
        return audio_pcm_hw_get_live_out(hw);

    /* Make sure the device is running */
    coreaudio_ctl_out(&caVoice->hw, VOICE_ENABLE);

    /* How much space is available in the ring buffer */
    csAvail = IORingBufferFree(caVoice->pBuf) >> hw->info.shift; /* bytes -> samples */

    /* How much data is available. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)audio_pcm_hw_get_live_out(hw));

    CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << hw->info.shift));

    /* Iterate as long as data is available */
    while (csWritten < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToWrite = RT_MIN(csAvail - csWritten, (uint32_t)(hw->samples - hw->rpos));
        cbToWrite = csToWrite << hw->info.shift; /* samples -> bytes */
        CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));
        /* Try to acquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(caVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);
        /* How much to we get? */
        csToWrite = cbToWrite >> hw->info.shift;
        CA_EXT_DEBUG_LOG(("CoreAudio: [Output] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));
        /* Break if nothing is free anymore. */
        if (RT_UNLIKELY(cbToWrite == 0))
            break;
        /* Copy the data from our mix buffer to the ring buffer. */
        psSrc = hw->mix_buf + hw->rpos;
        hw->clip((uint8_t*)pcDst, psSrc, csToWrite);
        /* Release the ring buffer, so the read thread could start reading this data. */
        IORingBufferReleaseWriteBlock(caVoice->pBuf, cbToWrite);
        hw->rpos = (hw->rpos + csToWrite) % hw->samples;
        /* How much have we written so far. */
        csWritten += csToWrite;
    }

    CA_EXT_DEBUG_LOG(("CoreAudio: [Output] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << hw->info.shift));

    /* Return the count of samples we have processed. */
    return csWritten;
}

static int coreaudio_write(SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int coreaudio_ctl_out(HWVoiceOut *hw, int cmd, ...)
{
    OSStatus err = noErr;
    uint32_t status;
    caVoiceOut *caVoice = (caVoiceOut *) hw;

    status = ASMAtomicReadU32(&caVoice->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
        return 0;

    switch (cmd)
    {
        case VOICE_ENABLE:
            {
                /* Only start the device if it is actually stopped */
                if (!caIsRunning(caVoice->audioDeviceId))
                {
                    err = AudioUnitReset(caVoice->audioUnit,
                                         kAudioUnitScope_Input,
                                         0);
                    IORingBufferReset(caVoice->pBuf);
                    err = AudioOutputUnitStart(caVoice->audioUnit);
                    if (RT_UNLIKELY(err != noErr))
                    {
                        LogRel(("CoreAudio: [Output] Failed to start playback (%RI32)\n", err));
                        return -1;
                    }
                }
                break;
            }
        case VOICE_DISABLE:
            {
                /* Only stop the device if it is actually running */
                if (caIsRunning(caVoice->audioDeviceId))
                {
                    err = AudioOutputUnitStop(caVoice->audioUnit);
                    if (RT_UNLIKELY(err != noErr))
                    {
                        LogRel(("CoreAudio: [Output] Failed to stop playback (%RI32)\n", err));
                        return -1;
                    }
                    err = AudioUnitReset(caVoice->audioUnit,
                                         kAudioUnitScope_Input,
                                         0);
                    if (RT_UNLIKELY(err != noErr))
                    {
                        LogRel(("CoreAudio: [Output] Failed to reset AudioUnit (%RI32)\n", err));
                        return -1;
                    }
                }
                break;
            }
    }
    return 0;
}

static void coreaudio_fini_out(HWVoiceOut *hw)
{
    int rc = 0;
    uint32_t status;
    OSStatus err = noErr;
    caVoiceOut *caVoice = (caVoiceOut *) hw;

    status = ASMAtomicReadU32(&caVoice->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
        return;

    rc = coreaudio_ctl_out(hw, VOICE_DISABLE);
    if (RT_LIKELY(rc == 0))
    {
        ASMAtomicXchgU32(&caVoice->status, CA_STATUS_IN_UNINIT);
#ifdef DEBUG
        err = AudioDeviceRemovePropertyListener(caVoice->audioDeviceId,
                                                0,
                                                false,
                                                kAudioDeviceProcessorOverload,
                                                caPlaybackAudioDevicePropertyChanged);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: [Output] Failed to remove the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */
        err = AudioUnitUninitialize(caVoice->audioUnit);
        if (RT_LIKELY(err == noErr))
        {
            err = CloseComponent(caVoice->audioUnit);
            if (RT_LIKELY(err == noErr))
            {
                IORingBufferDestroy(caVoice->pBuf);
                caVoice->audioUnit = NULL;
                caVoice->audioDeviceId = kAudioDeviceUnknown;
                caVoice->pBuf = NULL;
                ASMAtomicXchgU32(&caVoice->status, CA_STATUS_UNINIT);
            }
            else
                LogRel(("CoreAudio: [Output] Failed to close the AudioUnit (%RI32)\n", err));
        }
        else
            LogRel(("CoreAudio: [Output] Failed to uninitialize the AudioUnit (%RI32)\n", err));
    }
    else
        LogRel(("CoreAudio: [Output] Failed to stop playback (%RI32)\n", err));
}

static int coreaudio_init_out(HWVoiceOut *hw, audsettings_t *as)
{
    OSStatus err = noErr;
    int rc = 0;
    bool fDeviceByUser = false; /* use we a device which was set by the user? */

    caVoiceOut *caVoice = (caVoiceOut *) hw;

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_UNINIT);
    caVoice->audioUnit = NULL;
    caVoice->audioDeviceId = kAudioDeviceUnknown;
    hw->samples = 0;

    /* Initialize the hardware info section with the audio settings */
    audio_pcm_init_info(&hw->info, as);

    /* Try to find the audio device set by the user. Use
     * export VBOX_COREAUDIO_OUTPUT_DEVICE_UID=AppleHDAEngineOutput:0
     * to set it. */
    if (conf.pszOutputDeviceUID)
    {
        caVoice->audioDeviceId = caDeviceUIDtoID(conf.pszOutputDeviceUID);
        /* Not fatal */
        if (caVoice->audioDeviceId == kAudioDeviceUnknown)
            LogRel(("CoreAudio: [Output] Unable to find output device %s. Falling back to the default audio device. \n", conf.pszOutputDeviceUID));
        else
            fDeviceByUser = true;
    }

    rc = caInitOutput(hw);
    if (RT_UNLIKELY(rc != 0))
        return rc;

    /* The samples have to correspond to the internal ring buffer size. */
    hw->samples = (IORingBufferSize(caVoice->pBuf) >> hw->info.shift) / caVoice->streamFormat.mChannelsPerFrame;

    /* When the devices isn't forced by the user, we want default device change
     * notifications. */
    if (!fDeviceByUser)
    {
        err = AudioHardwareAddPropertyListener(kAudioHardwarePropertyDefaultOutputDevice,
                                               caPlaybackDefaultDeviceChanged,
                                               caVoice);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: [Output] Failed to add the default device changed listener (%RI32)\n", err));
    }

    Log(("CoreAudio: [Output] HW samples: %d\n", hw->samples));

    return 0;
}

/*******************************************************************************
 *
 * CoreAudio input section
 *
 ******************************************************************************/

/* We need some forward declarations */
static int coreaudio_run_in(HWVoiceIn *hw);
static int coreaudio_read(SWVoiceIn *sw, void *buf, int size);
static int coreaudio_ctl_in(HWVoiceIn *hw, int cmd, ...);
static void coreaudio_fini_in(HWVoiceIn *hw);
static int coreaudio_init_in(HWVoiceIn *hw, audsettings_t *as);
static int caInitInput(HWVoiceIn *hw);
static void caReinitInput(HWVoiceIn *hw);

/* Callback for getting notified when the default input device was changed */
static DECLCALLBACK(OSStatus) caRecordingDefaultDeviceChanged(AudioHardwarePropertyID inPropertyID,
                                                              void *inClientData)
{
    OSStatus err = noErr;
    UInt32 uSize = 0;
    UInt32 ad = 0;
    bool fRun = false;

    caVoiceIn *caVoice = (caVoiceIn *) inClientData;

    switch (inPropertyID)
    {
        case kAudioHardwarePropertyDefaultInputDevice:
            {
                /* This listener is called on every change of the hardware
                 * device. So check if the default device has really changed. */
                uSize = sizeof(caVoice->audioDeviceId);
                err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                               &uSize,
                                               &ad);
                if (caVoice->audioDeviceId != ad)
                {
                    Log2(("CoreAudio: [Input] Default input device changed!\n"));
                    /* We move the reinitialization to the next input event.
                     * This make sure this thread isn't blocked and the
                     * reinitialization is done when necessary only. */
                    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_REINIT);
                }
                break;
            }
    }

    return noErr;
}

/* Callback for getting notified when some of the properties of an audio device has changed */
static DECLCALLBACK(OSStatus) caRecordingAudioDevicePropertyChanged(AudioDeviceID inDevice,
                                                                    UInt32 inChannel,
                                                                    Boolean isInput,
                                                                    AudioDevicePropertyID inPropertyID,
                                                                    void *inClientData)
{
    caVoiceIn *caVoice = (caVoiceIn *) inClientData;

    switch (inPropertyID)
    {
#ifdef DEBUG
        case kAudioDeviceProcessorOverload:
            {
                Log2(("CoreAudio: [Input] Processor overload detected!\n"));
                break;
            }
#endif /* DEBUG */
        case kAudioDevicePropertyNominalSampleRate:
            {
                Log2(("CoreAudio: [Input] Sample rate changed!\n"));
                /* We move the reinitialization to the next input event.
                 * This make sure this thread isn't blocked and the
                 * reinitialization is done when necessary only. */
                ASMAtomicXchgU32(&caVoice->status, CA_STATUS_REINIT);
                break;
            }
        default: break;
    }

    return noErr;
}

/* Callback to convert audio input data from one format to another */
static DECLCALLBACK(OSStatus) caConverterCallback(AudioConverterRef inAudioConverter,
                                                  UInt32 *ioNumberDataPackets,
                                                  AudioBufferList *ioData,
                                                  AudioStreamPacketDescription **outDataPacketDescription,
                                                  void *inUserData)
{
    /* In principle we had to check here if the source is non interleaved & if
     * so go through all buffers not only the first one like now. */
    UInt32 cSize = 0;

    caVoiceIn *caVoice = (caVoiceIn *) inUserData;

    const AudioBufferList *pBufferList = &caVoice->bufferList;

    if (ASMAtomicReadU32(&caVoice->status) != CA_STATUS_INIT)
        return noErr;

/*    Log2(("converting .... ################ %RU32  %RU32 %RU32 %RU32 %RU32\n", *ioNumberDataPackets, bufferList->mBuffers[i].mNumberChannels, bufferList->mNumberBuffers, bufferList->mBuffers[i].mDataByteSize, ioData->mNumberBuffers));*/

    /* Use the lower one of the packets to process & the available packets in
     * the buffer */
    cSize = RT_MIN(*ioNumberDataPackets * caVoice->deviceFormat.mBytesPerPacket,
                   pBufferList->mBuffers[0].mDataByteSize - caVoice->rpos);
    /* Set the new size on output, so the caller know what we have processed. */
    *ioNumberDataPackets = cSize / caVoice->deviceFormat.mBytesPerPacket;
    /* If no data is available anymore we return with an error code. This error
     * code will be returned from AudioConverterFillComplexBuffer. */
    if (*ioNumberDataPackets == 0)
    {
        ioData->mBuffers[0].mDataByteSize = 0;
        ioData->mBuffers[0].mData = NULL;
        return caConverterEOFDErr;
    }
    else
    {
        ioData->mBuffers[0].mNumberChannels = pBufferList->mBuffers[0].mNumberChannels;
        ioData->mBuffers[0].mDataByteSize = cSize;
        ioData->mBuffers[0].mData = (char*)pBufferList->mBuffers[0].mData + caVoice->rpos;
        caVoice->rpos += cSize;

        /*    Log2(("converting .... ################ %RU32 %RU32\n", size, caVoice->rpos));*/
    }

    return noErr;
}

/* Callback to feed audio input buffer */
static DECLCALLBACK(OSStatus) caRecordingCallback(void* inRefCon,
                                                  AudioUnitRenderActionFlags* ioActionFlags,
                                                  const AudioTimeStamp* inTimeStamp,
                                                  UInt32 inBusNumber,
                                                  UInt32 inNumberFrames,
                                                  AudioBufferList* ioData)
{
    OSStatus err = noErr;
    uint32_t csAvail = 0;
    uint32_t csToWrite = 0;
    uint32_t cbToWrite = 0;
    uint32_t csWritten = 0;
    char *pcDst = NULL;
    AudioBufferList tmpList;
    UInt32 ioOutputDataPacketSize = 0;

    caVoiceIn *caVoice = (caVoiceIn *) inRefCon;

    if (ASMAtomicReadU32(&caVoice->status) != CA_STATUS_INIT)
        return noErr;

    /* If nothing is pending return immediately. */
    if (inNumberFrames == 0)
        return noErr;

    /* Are we using an converter? */
    if (RT_VALID_PTR(caVoice->converter))
    {
        /* Firstly render the data as usual */
        caVoice->bufferList.mBuffers[0].mNumberChannels = caVoice->deviceFormat.mChannelsPerFrame;
        caVoice->bufferList.mBuffers[0].mDataByteSize = caVoice->deviceFormat.mBytesPerFrame * inNumberFrames;
        caVoice->bufferList.mBuffers[0].mData = RTMemAlloc(caVoice->bufferList.mBuffers[0].mDataByteSize);

        err = AudioUnitRender(caVoice->audioUnit,
                              ioActionFlags,
                              inTimeStamp,
                              inBusNumber,
                              inNumberFrames,
                              &caVoice->bufferList);
        if(RT_UNLIKELY(err != noErr))
        {
            Log(("CoreAudio: [Input] Failed to render audio data (%RI32)\n", err));
            RTMemFree(caVoice->bufferList.mBuffers[0].mData);
            return err;
        }

        /* How much space is free in the ring buffer? */
        csAvail = IORingBufferFree(caVoice->pBuf) >> caVoice->hw.info.shift; /* bytes -> samples */
        /* How much space is used in the core audio buffer. Use the smaller size of
         * the too. */
        csAvail = RT_MIN(csAvail, (uint32_t)((caVoice->bufferList.mBuffers[0].mDataByteSize / caVoice->deviceFormat.mBytesPerFrame) * caVoice->sampleRatio));

        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));
        /* Initialize the temporary output buffer */
        tmpList.mNumberBuffers = 1;
        tmpList.mBuffers[0].mNumberChannels = caVoice->streamFormat.mChannelsPerFrame;
        /* Set the read position to zero. */
        caVoice->rpos = 0;
        /* Iterate as long as data is available */
        while(csWritten < csAvail)
        {
            /* How much is left? */
            csToWrite = csAvail - csWritten;
            cbToWrite = csToWrite << caVoice->hw.info.shift;
            CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));
            /* Try to acquire the necessary space from the ring buffer. */
            IORingBufferAquireWriteBlock(caVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);
            /* How much to we get? */
            csToWrite = cbToWrite >> caVoice->hw.info.shift;
            CA_EXT_DEBUG_LOG(("CoreAudio: [Input] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));
            /* Break if nothing is free anymore. */
            if (RT_UNLIKELY(cbToWrite == 0))
                break;

            /* Now set how much space is available for output */
            ioOutputDataPacketSize = cbToWrite / caVoice->streamFormat.mBytesPerPacket;
            /* Set our ring buffer as target. */
            tmpList.mBuffers[0].mDataByteSize = cbToWrite;
            tmpList.mBuffers[0].mData = pcDst;
            AudioConverterReset(caVoice->converter);
            err = AudioConverterFillComplexBuffer(caVoice->converter,
                                                  caConverterCallback,
                                                  caVoice,
                                                  &ioOutputDataPacketSize,
                                                  &tmpList,
                                                  NULL);
            if(   RT_UNLIKELY(err != noErr)
               && err != caConverterEOFDErr)
            {
                Log(("CoreAudio: [Input] Failed to convert audio data (%RI32:%c%c%c%c)\n", err, RT_BYTE4(err), RT_BYTE3(err), RT_BYTE2(err), RT_BYTE1(err)));
                break;
            }
            /* Check in any case what processed size is returned. It could be
             * much littler than we expected. */
            cbToWrite = ioOutputDataPacketSize * caVoice->streamFormat.mBytesPerPacket;
            csToWrite = cbToWrite >> caVoice->hw.info.shift;
            /* Release the ring buffer, so the main thread could start reading this data. */
            IORingBufferReleaseWriteBlock(caVoice->pBuf, cbToWrite);
            csWritten += csToWrite;
            /* If the error is "End of Data" it means there is no data anymore
             * which could be converted. So end here now. */
            if (err == caConverterEOFDErr)
                break;
        }
        /* Cleanup */
        RTMemFree(caVoice->bufferList.mBuffers[0].mData);
        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << caVoice->hw.info.shift));
    }
    else
    {
        caVoice->bufferList.mBuffers[0].mNumberChannels = caVoice->streamFormat.mChannelsPerFrame;
        caVoice->bufferList.mBuffers[0].mDataByteSize = caVoice->streamFormat.mBytesPerFrame * inNumberFrames;
        caVoice->bufferList.mBuffers[0].mData = RTMemAlloc(caVoice->bufferList.mBuffers[0].mDataByteSize);

        err = AudioUnitRender(caVoice->audioUnit,
                              ioActionFlags,
                              inTimeStamp,
                              inBusNumber,
                              inNumberFrames,
                              &caVoice->bufferList);
        if(RT_UNLIKELY(err != noErr))
        {
            Log(("CoreAudio: [Input] Failed to render audio data (%RI32)\n", err));
            RTMemFree(caVoice->bufferList.mBuffers[0].mData);
            return err;
        }

        /* How much space is free in the ring buffer? */
        csAvail = IORingBufferFree(caVoice->pBuf) >> caVoice->hw.info.shift; /* bytes -> samples */
        /* How much space is used in the core audio buffer. Use the smaller size of
         * the too. */
        csAvail = RT_MIN(csAvail, caVoice->bufferList.mBuffers[0].mDataByteSize >> caVoice->hw.info.shift);

        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

        /* Iterate as long as data is available */
        while(csWritten < csAvail)
        {
            /* How much is left? */
            csToWrite = csAvail - csWritten;
            cbToWrite = csToWrite << caVoice->hw.info.shift;
            CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));
            /* Try to acquire the necessary space from the ring buffer. */
            IORingBufferAquireWriteBlock(caVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);
            /* How much to we get? */
            csToWrite = cbToWrite >> caVoice->hw.info.shift;
            CA_EXT_DEBUG_LOG(("CoreAudio: [Input] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));
            /* Break if nothing is free anymore. */
            if (RT_UNLIKELY(cbToWrite == 0))
                break;
            /* Copy the data from the core audio buffer to the ring buffer. */
            memcpy(pcDst, (char*)caVoice->bufferList.mBuffers[0].mData + (csWritten << caVoice->hw.info.shift), cbToWrite);
            /* Release the ring buffer, so the main thread could start reading this data. */
            IORingBufferReleaseWriteBlock(caVoice->pBuf, cbToWrite);
            csWritten += csToWrite;
        }
        /* Cleanup */
        RTMemFree(caVoice->bufferList.mBuffers[0].mData);

        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << caVoice->hw.info.shift));
    }

    return err;
}

static int caInitInput(HWVoiceIn *hw)
{
    OSStatus err = noErr;
    int rc = -1;
    UInt32 uSize = 0; /* temporary size of properties */
    UInt32 uFlag = 0; /* for setting flags */
    CFStringRef name; /* for the temporary device name fetching */
    char *pszName = NULL;
    char *pszUID = NULL;
    ComponentDescription cd; /* description for an audio component */
    Component cp; /* an audio component */
    AURenderCallbackStruct cb; /* holds the callback structure */
    UInt32 cFrames; /* default frame count */
    const SInt32 channelMap[2] = {0, 0}; /* Channel map for mono -> stereo */
    UInt32 cSamples; /* samples count */

    caVoiceIn *caVoice = (caVoiceIn *) hw;

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_IN_INIT);

    if (caVoice->audioDeviceId == kAudioDeviceUnknown)
    {
        /* Fetch the default audio output device currently in use */
        uSize = sizeof(caVoice->audioDeviceId);
        err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                       &uSize,
                                       &caVoice->audioDeviceId);
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Input] Unable to find default input device (%RI32)\n", err));
            return -1;
        }
    }

    /* Try to get the name of the input device and log it. It's not fatal if
     * it fails. */
    uSize = sizeof(CFStringRef);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 0,
                                 kAudioObjectPropertyName,
                                 &uSize,
                                 &name);
    if (RT_LIKELY(err == noErr))
    {
        pszName = caCFStringToCString(name);
        CFRelease(name);
        err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                     0,
                                     0,
                                     kAudioDevicePropertyDeviceUID,
                                     &uSize,
                                     &name);
        if (RT_LIKELY(err == noErr))
        {
            pszUID = caCFStringToCString(name);
            CFRelease(name);
            if (pszName && pszUID)
                LogRel(("CoreAudio: Using input device: %s (UID: %s)\n", pszName, pszUID));
            if (pszUID)
                RTMemFree(pszUID);
        }
        if (pszName)
            RTMemFree(pszName);
    }
    else
        LogRel(("CoreAudio: [Input] Unable to get input device name (%RI32)\n", err));

    /* Get the default frames buffer size, so that we can setup our internal
     * buffers. */
    uSize = sizeof(cFrames);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 true,
                                 kAudioDevicePropertyBufferFrameSize,
                                 &uSize,
                                 &cFrames);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to get frame buffer size of the audio device (%RI32)\n", err));
        return -1;
    }
    /* Set the frame buffer size and honor any minimum/maximum restrictions on
       the device. */
    err = caSetFrameBufferSize(caVoice->audioDeviceId,
                               true,
                               cFrames,
                               &cFrames);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set frame buffer size on the audio device (%RI32)\n", err));
        return -1;
    }

    cd.componentType = kAudioUnitType_Output;
    cd.componentSubType = kAudioUnitSubType_HALOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    /* Try to find the default HAL output component. */
    cp = FindNextComponent(NULL, &cd);
    if (RT_UNLIKELY(cp == 0))
    {
        LogRel(("CoreAudio: [Input] Failed to find HAL output component\n"));
        return -1;
    }

    /* Open the default HAL output component. */
    err = OpenAComponent(cp, &caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to open output component (%RI32)\n", err));
        return -1;
    }

    /* Switch the I/O mode for input to on. */
    uFlag = 1;
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Input,
                               1,
                               &uFlag,
                               sizeof(uFlag));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set input I/O mode enabled (%RI32)\n", err));
        return -1;
    }

    /* Switch the I/O mode for output to off. This is important, as this is a
     * pure input stream. */
    uFlag = 0;
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output,
                               0,
                               &uFlag,
                               sizeof(uFlag));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set output I/O mode disabled (%RI32)\n", err));
        return -1;
    }

    /* Set the default audio input device as the device for the new AudioUnit. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global,
                               0,
                               &caVoice->audioDeviceId,
                               sizeof(caVoice->audioDeviceId));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set current device (%RI32)\n", err));
        return -1;
    }

    /* CoreAudio will inform us on a second thread for new incoming audio data.
     * Therefor register an callback function, which will process the new data.
     * */
    cb.inputProc = caRecordingCallback;
    cb.inputProcRefCon = caVoice;

    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_SetInputCallback,
                               kAudioUnitScope_Global,
                               0,
                               &cb,
                               sizeof(cb));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set callback (%RI32)\n", err));
        return -1;
    }

    /* Fetch the current stream format of the device. */
    uSize = sizeof(caVoice->deviceFormat);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               1,
                               &caVoice->deviceFormat,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to get device format (%RI32)\n", err));
        return -1;
    }

    /* Create an AudioStreamBasicDescription based on the audio settings of
     * VirtualBox. */
    caPCMInfoToAudioStreamBasicDescription(&caVoice->hw.info, &caVoice->streamFormat);

#if DEBUG
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Input] device", &caVoice->deviceFormat);
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Input] input", &caVoice->streamFormat);
#endif /* DEBUG */

    /* If the frequency of the device is different from the requested one we
     * need a converter. The same count if the number of channels is different. */
    if (   caVoice->deviceFormat.mSampleRate != caVoice->streamFormat.mSampleRate
        || caVoice->deviceFormat.mChannelsPerFrame != caVoice->streamFormat.mChannelsPerFrame)
    {
        err = AudioConverterNew(&caVoice->deviceFormat,
                                &caVoice->streamFormat,
                                &caVoice->converter);
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Input] Failed to create the audio converter (%RI32)\n", err));
            return -1;
        }

        if (caVoice->deviceFormat.mChannelsPerFrame == 1 &&
            caVoice->streamFormat.mChannelsPerFrame == 2)
        {
            /* If the channel count is different we have to tell this the converter
               and supply a channel mapping. For now we only support mapping
               from mono to stereo. For all other cases the core audio defaults
               are used, which means dropping additional channels in most
               cases. */
            err = AudioConverterSetProperty(caVoice->converter,
                                            kAudioConverterChannelMap,
                                            sizeof(channelMap),
                                            channelMap);
            if (RT_UNLIKELY(err != noErr))
            {
                LogRel(("CoreAudio: [Input] Failed to add a channel mapper to the audio converter (%RI32)\n", err));
                return -1;
            }
        }
        /* Set sample rate converter quality to maximum */
/*        uFlag = kAudioConverterQuality_Max;*/
/*        err = AudioConverterSetProperty(caVoice->converter,*/
/*                                        kAudioConverterSampleRateConverterQuality,*/
/*                                        sizeof(uFlag),*/
/*                                        &uFlag);*/
        /* Not fatal */
/*        if (RT_UNLIKELY(err != noErr))*/
/*            LogRel(("CoreAudio: [Input] Failed to set the audio converter quality to the maximum (%RI32)\n", err));*/

        Log(("CoreAudio: [Input] Converter in use\n"));
        /* Set the new format description for the stream. */
        err = AudioUnitSetProperty(caVoice->audioUnit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Output,
                                   1,
                                   &caVoice->deviceFormat,
                                   sizeof(caVoice->deviceFormat));
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Input] Failed to set stream format (%RI32)\n", err));
            return -1;
        }
        err = AudioUnitSetProperty(caVoice->audioUnit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   1,
                                   &caVoice->deviceFormat,
                                   sizeof(caVoice->deviceFormat));
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Input] Failed to set stream format (%RI32)\n", err));
            return -1;
        }
    }
    else
    {
        /* Set the new format description for the stream. */
        err = AudioUnitSetProperty(caVoice->audioUnit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Output,
                                   1,
                                   &caVoice->streamFormat,
                                   sizeof(caVoice->streamFormat));
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: [Input] Failed to set stream format (%RI32)\n", err));
            return -1;
        }
    }

    /* Also set the frame buffer size off the device on our AudioUnit. This
       should make sure that the frames count which we receive in the render
       thread is as we like. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_MaximumFramesPerSlice,
                               kAudioUnitScope_Global,
                               1,
                               &cFrames,
                               sizeof(cFrames));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set maximum frame buffer size on the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Finally initialize the new AudioUnit. */
    err = AudioUnitInitialize(caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to initialize the AudioUnit (%RI32)\n", err));
        return -1;
    }

    uSize = sizeof(caVoice->deviceFormat);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Output,
                               1,
                               &caVoice->deviceFormat,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to get device format (%RI32)\n", err));
        return -1;
    }

    /* There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
     * the frame buffer size set in the previous calls. So finally get the
     * frame buffer size after the AudioUnit was initialized. */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_MaximumFramesPerSlice,
                               kAudioUnitScope_Global,
                               0,
                               &cFrames,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to get maximum frame buffer size from the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Calculate the ratio between the device and the stream sample rate. */
    caVoice->sampleRatio = caVoice->streamFormat.mSampleRate / caVoice->deviceFormat.mSampleRate;

    /* Set to zero first */
    caVoice->pBuf = NULL;
    /* Create the AudioBufferList structure with one buffer. */
    caVoice->bufferList.mNumberBuffers = 1;
    /* Initialize the buffer to nothing. */
    caVoice->bufferList.mBuffers[0].mNumberChannels = caVoice->streamFormat.mChannelsPerFrame;
    caVoice->bufferList.mBuffers[0].mDataByteSize = 0;
    caVoice->bufferList.mBuffers[0].mData = NULL;

    /* Make sure that the ring buffer is big enough to hold the recording
     * data. Compare the maximum frames per slice value with the frames
     * necessary when using the converter where the sample rate could differ.
     * The result is always multiplied by the channels per frame to get the
     * samples count. */
    cSamples = RT_MAX(cFrames,
                      (cFrames * caVoice->deviceFormat.mBytesPerFrame * caVoice->sampleRatio) / caVoice->streamFormat.mBytesPerFrame)
               * caVoice->streamFormat.mChannelsPerFrame;
    if (   hw->samples != 0
        && hw->samples != (int32_t)cSamples)
        LogRel(("CoreAudio: [Input] Warning! After recreation, the CoreAudio ring buffer doesn't has the same size as the device buffer (%RU32 vs. %RU32).\n", cSamples, (uint32_t)hw->samples));
    /* Create the internal ring buffer. */
    IORingBufferCreate(&caVoice->pBuf, cSamples << hw->info.shift);
    if (RT_VALID_PTR(caVoice->pBuf))
        rc = 0;
    else
        LogRel(("CoreAudio: [Input] Failed to create internal ring buffer\n"));

    if (rc != 0)
    {
        if (caVoice->pBuf)
            IORingBufferDestroy(caVoice->pBuf);
        AudioUnitUninitialize(caVoice->audioUnit);
        return -1;
    }

#ifdef DEBUG
    err = AudioDeviceAddPropertyListener(caVoice->audioDeviceId,
                                         0,
                                         true,
                                         kAudioDeviceProcessorOverload,
                                         caRecordingAudioDevicePropertyChanged,
                                         caVoice);
    /* Not Fatal */
    if (RT_UNLIKELY(err != noErr))
        LogRel(("CoreAudio: [Input] Failed to add the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */
    err = AudioDeviceAddPropertyListener(caVoice->audioDeviceId,
                                         0,
                                         true,
                                         kAudioDevicePropertyNominalSampleRate,
                                         caRecordingAudioDevicePropertyChanged,
                                         caVoice);
    /* Not Fatal */
    if (RT_UNLIKELY(err != noErr))
        LogRel(("CoreAudio: [Input] Failed to add the sample rate changed listener (%RI32)\n", err));

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_INIT);

    Log(("CoreAudio: [Input] Frame count: %RU32\n", cFrames));

    return 0;
}

static void caReinitInput(HWVoiceIn *hw)
{
    caVoiceIn *caVoice = (caVoiceIn *) hw;

    coreaudio_fini_in(&caVoice->hw);
    caInitInput(&caVoice->hw);

    coreaudio_ctl_in(&caVoice->hw, VOICE_ENABLE);
}

static int coreaudio_run_in(HWVoiceIn *hw)
{
    uint32_t csAvail = 0;
    uint32_t cbToRead = 0;
    uint32_t csToRead = 0;
    uint32_t csReads = 0;
    char *pcSrc;
    st_sample_t *psDst;

    caVoiceIn *caVoice = (caVoiceIn *) hw;

    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&caVoice->status) == CA_STATUS_REINIT)
        caReinitInput(&caVoice->hw);

    if (ASMAtomicReadU32(&caVoice->status) != CA_STATUS_INIT)
        return 0;

    /* How much space is used in the ring buffer? */
    csAvail = IORingBufferUsed(caVoice->pBuf) >> hw->info.shift; /* bytes -> samples */
    /* How much space is available in the mix buffer. Use the smaller size of
     * the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)(hw->samples - audio_pcm_hw_get_live_in (hw)));

    CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Start reading buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

    /* Iterate as long as data is available */
    while (csReads < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToRead = RT_MIN(csAvail - csReads, (uint32_t)(hw->samples - hw->wpos));
        cbToRead = csToRead << hw->info.shift;
        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Try reading %RU32 samples (%RU32 bytes)\n", csToRead, cbToRead));
        /* Try to acquire the necessary block from the ring buffer. */
        IORingBufferAquireReadBlock(caVoice->pBuf, cbToRead, &pcSrc, &cbToRead);
        /* How much to we get? */
        csToRead = cbToRead >> hw->info.shift;
        CA_EXT_DEBUG_LOG(("CoreAudio: [Input] There are %RU32 samples (%RU32 bytes) available\n", csToRead, cbToRead));
        /* Break if nothing is used anymore. */
        if (cbToRead == 0)
            break;
        /* Copy the data from our ring buffer to the mix buffer. */
        psDst = hw->conv_buf + hw->wpos;
        hw->conv(psDst, pcSrc, csToRead, &nominal_volume);
        /* Release the read buffer, so it could be used for new data. */
        IORingBufferReleaseReadBlock(caVoice->pBuf, cbToRead);
        hw->wpos = (hw->wpos + csToRead) % hw->samples;
        /* How much have we reads so far. */
        csReads += csToRead;
    }

    CA_EXT_DEBUG_LOG(("CoreAudio: [Input] Finished reading buffer with %RU32 samples (%RU32 bytes)\n", csReads, csReads << caVoice->hw.info.shift));

    return csReads;
}

static int coreaudio_read(SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}

static int coreaudio_ctl_in(HWVoiceIn *hw, int cmd, ...)
{
    OSStatus err = noErr;
    uint32_t status;
    caVoiceIn *caVoice = (caVoiceIn *) hw;

    status = ASMAtomicReadU32(&caVoice->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
        return 0;

    switch (cmd)
    {
        case VOICE_ENABLE:
            {
                /* Only start the device if it is actually stopped */
                if (!caIsRunning(caVoice->audioDeviceId))
                {
                    IORingBufferReset(caVoice->pBuf);
                    err = AudioOutputUnitStart(caVoice->audioUnit);
                }
                if (RT_UNLIKELY(err != noErr))
                {
                    LogRel(("CoreAudio: [Input] Failed to start recording (%RI32)\n", err));
                    return -1;
                }
                break;
            }
        case VOICE_DISABLE:
            {
                /* Only stop the device if it is actually running */
                if (caIsRunning(caVoice->audioDeviceId))
                {
                    err = AudioOutputUnitStop(caVoice->audioUnit);
                    if (RT_UNLIKELY(err != noErr))
                    {
                        LogRel(("CoreAudio: [Input] Failed to stop recording (%RI32)\n", err));
                        return -1;
                    }
                    err = AudioUnitReset(caVoice->audioUnit,
                                         kAudioUnitScope_Input,
                                         0);
                    if (RT_UNLIKELY(err != noErr))
                    {
                        LogRel(("CoreAudio: [Input] Failed to reset AudioUnit (%RI32)\n", err));
                        return -1;
                    }
                }
                break;
            }
    }
    return 0;
}

static void coreaudio_fini_in(HWVoiceIn *hw)
{
    int rc = 0;
    OSStatus err = noErr;
    uint32_t status;
    caVoiceIn *caVoice = (caVoiceIn *) hw;

    status = ASMAtomicReadU32(&caVoice->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
        return;

    rc = coreaudio_ctl_in(hw, VOICE_DISABLE);
    if (RT_LIKELY(rc == 0))
    {
        ASMAtomicXchgU32(&caVoice->status, CA_STATUS_IN_UNINIT);
#ifdef DEBUG
        err = AudioDeviceRemovePropertyListener(caVoice->audioDeviceId,
                                                0,
                                                true,
                                                kAudioDeviceProcessorOverload,
                                                caRecordingAudioDevicePropertyChanged);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: [Input] Failed to remove the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */
        err = AudioDeviceRemovePropertyListener(caVoice->audioDeviceId,
                                                0,
                                                true,
                                                kAudioDevicePropertyNominalSampleRate,
                                                caRecordingAudioDevicePropertyChanged);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: [Input] Failed to remove the sample rate changed listener (%RI32)\n", err));
        if (caVoice->converter)
        {
            AudioConverterDispose(caVoice->converter);
            caVoice->converter = NULL;
        }
        err = AudioUnitUninitialize(caVoice->audioUnit);
        if (RT_LIKELY(err == noErr))
        {
            err = CloseComponent(caVoice->audioUnit);
            if (RT_LIKELY(err == noErr))
            {
                IORingBufferDestroy(caVoice->pBuf);
                caVoice->audioUnit = NULL;
                caVoice->audioDeviceId = kAudioDeviceUnknown;
                caVoice->pBuf = NULL;
                caVoice->sampleRatio = 1;
                caVoice->rpos = 0;
                ASMAtomicXchgU32(&caVoice->status, CA_STATUS_UNINIT);
            }
            else
                LogRel(("CoreAudio: [Input] Failed to close the AudioUnit (%RI32)\n", err));
        }
        else
            LogRel(("CoreAudio: [Input] Failed to uninitialize the AudioUnit (%RI32)\n", err));
    }
    else
        LogRel(("CoreAudio: [Input] Failed to stop recording (%RI32)\n", err));
}

static int coreaudio_init_in(HWVoiceIn *hw, audsettings_t *as)
{
    OSStatus err = noErr;
    int rc = -1;
    bool fDeviceByUser = false;

    caVoiceIn *caVoice = (caVoiceIn *) hw;

    ASMAtomicXchgU32(&caVoice->status, CA_STATUS_UNINIT);
    caVoice->audioUnit = NULL;
    caVoice->audioDeviceId = kAudioDeviceUnknown;
    caVoice->converter = NULL;
    caVoice->sampleRatio = 1;
    caVoice->rpos = 0;
    hw->samples = 0;

    /* Initialize the hardware info section with the audio settings */
    audio_pcm_init_info(&hw->info, as);

    /* Try to find the audio device set by the user */
    if (conf.pszInputDeviceUID)
    {
        caVoice->audioDeviceId = caDeviceUIDtoID(conf.pszInputDeviceUID);
        /* Not fatal */
        if (caVoice->audioDeviceId == kAudioDeviceUnknown)
            LogRel(("CoreAudio: [Input] Unable to find input device %s. Falling back to the default audio device. \n", conf.pszInputDeviceUID));
        else
            fDeviceByUser = true;
    }

    rc = caInitInput(hw);
    if (RT_UNLIKELY(rc != 0))
        return rc;

    /* The samples have to correspond to the internal ring buffer size. */
    hw->samples = (IORingBufferSize(caVoice->pBuf) >> hw->info.shift) / caVoice->streamFormat.mChannelsPerFrame;

    /* When the devices isn't forced by the user, we want default device change
     * notifications. */
    if (!fDeviceByUser)
    {
        err = AudioHardwareAddPropertyListener(kAudioHardwarePropertyDefaultInputDevice,
                                               caRecordingDefaultDeviceChanged,
                                               caVoice);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: [Input] Failed to add the default device changed listener (%RI32)\n", err));
    }

    Log(("CoreAudio: [Input] HW samples: %d\n", hw->samples));

    return 0;
}

/*******************************************************************************
 *
 * CoreAudio global section
 *
 ******************************************************************************/

static void *coreaudio_audio_init(void)
{
    return &conf;
}

static void coreaudio_audio_fini(void *opaque)
{
    NOREF(opaque);
}

static struct audio_option coreaudio_options[] =
{
    {"OUTPUT_DEVICE_UID", AUD_OPT_STR, &conf.pszOutputDeviceUID,
     "UID of the output device to use", NULL, 0},
    {"INPUT_DEVICE_UID", AUD_OPT_STR, &conf.pszInputDeviceUID,
     "UID of the input device to use", NULL, 0},
    {NULL, 0, NULL, NULL, NULL, 0}
};

static struct audio_pcm_ops coreaudio_pcm_ops =
{
    coreaudio_init_out,
    coreaudio_fini_out,
    coreaudio_run_out,
    coreaudio_write,
    coreaudio_ctl_out,

    coreaudio_init_in,
    coreaudio_fini_in,
    coreaudio_run_in,
    coreaudio_read,
    coreaudio_ctl_in
};

struct audio_driver coreaudio_audio_driver =
{
    INIT_FIELD(name           =) "coreaudio",
    INIT_FIELD(descr          =)
    "CoreAudio http://developer.apple.com/audio/coreaudio.html",
    INIT_FIELD(options        =) coreaudio_options,
    INIT_FIELD(init           =) coreaudio_audio_init,
    INIT_FIELD(fini           =) coreaudio_audio_fini,
    INIT_FIELD(pcm_ops        =) &coreaudio_pcm_ops,
    INIT_FIELD(can_be_default =) 1,
    INIT_FIELD(max_voices_out =) 1,
    INIT_FIELD(max_voices_in  =) 1,
    INIT_FIELD(voice_size_out =) sizeof(caVoiceOut),
    INIT_FIELD(voice_size_in  =) sizeof(caVoiceIn)
};

