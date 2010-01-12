/* $Id$ */
/** @file
 * VBox audio devices: Mac OS X CoreAudio audio driver
 */

/*
 * Copyright (c) 2005 Mike Kronenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/cdefs.h>

#define AUDIO_CAP "coreaudio"
#include "vl_vbox.h"
#include "audio.h"
#include "audio_int.h"

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

/* todo:
 * - checking for properties changes of the devices
 * - set frame size (use config)
 * - AudioUnit converter for changing the sample rate
 * - maybe get rid of the extra input sample buffer
 * - maybe make sure the thread is immediately stopped if playing/recording stops
 */

/* Most of this is based on:
 * http://developer.apple.com/mac/library/technotes/tn2004/tn2097.html
 * http://developer.apple.com/mac/library/technotes/tn2002/tn2091.html
 * http://developer.apple.com/mac/library/documentation/AudioUnit/Reference/AUComponentServicesReference/Reference/reference.html
 */

/*******************************************************************************
 *
 * IO Ring Buffer section
 *
 ******************************************************************************/

/* Implementation of a lock free ring buffer which could be used in a multi
 * threaded environment. */
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
    return pBuffer->cBufSize - pBuffer->cBufferUsed;
}

DECL_FORCE_INLINE(uint32_t) IORingBufferUsed(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufferUsed;
}

static void IORingBufferAquireReadBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uUsed = 0;
    uint32_t uSize = 0;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is in use? */
    uUsed = ASMAtomicAddU32(&pBuffer->cBufferUsed, 0);
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

    ASMAtomicSubU32((int32_t*)&pBuffer->cBufferUsed, cSize);
}

static void IORingBufferAquireWriteBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uFree;
    uint32_t uSize;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is free? */
    uFree = pBuffer->cBufSize - ASMAtomicAddU32(&pBuffer->cBufferUsed, 0);
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

static void caAudioSettingsToAudioStreamBasicDescription(const audsettings_t *pAS, AudioStreamBasicDescription *pStreamDesc)
{
    pStreamDesc->mFormatID = kAudioFormatLinearPCM;
    pStreamDesc->mFormatFlags = kAudioFormatFlagIsPacked;
    pStreamDesc->mFramesPerPacket = 1;
    pStreamDesc->mSampleRate = (Float64)pAS->freq;
    pStreamDesc->mChannelsPerFrame = pAS->nchannels;
    switch (pAS->fmt)
    {
        case AUD_FMT_U8:
            {
                pStreamDesc->mBitsPerChannel = 8;
                break;
            }
        case AUD_FMT_S8:
            {
                pStreamDesc->mBitsPerChannel = 8;
                pStreamDesc->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
                break;
            }
        case AUD_FMT_U16:
            {
                pStreamDesc->mBitsPerChannel = 16;
                break;
            }
        case AUD_FMT_S16:
            {
                pStreamDesc->mBitsPerChannel = 16;
                pStreamDesc->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
                break;
            }
#ifdef PA_SAMPLE_S32LE
        case AUD_FMT_U32:
            {
                pStreamDesc->mBitsPerChannel = 32;
                break;
            }
        case AUD_FMT_S32:
            {
                pStreamDesc->mBitsPerChannel = 32;
                pStreamDesc->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
                break;
            }
#endif
        default:
            break;
    }
    pStreamDesc->mBytesPerFrame = pStreamDesc->mChannelsPerFrame * (pStreamDesc->mBitsPerChannel / 8);
    pStreamDesc->mBytesPerPacket = pStreamDesc->mFramesPerPacket * pStreamDesc->mBytesPerFrame;
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

/*******************************************************************************
 *
 * Global structures section
 *
 ******************************************************************************/

struct
{
    int cBufferFrames;
} conf =
{
    INIT_FIELD(.cBufferFrames =) 512
};

typedef struct caVoiceOut
{
    /* HW voice output struture defined by VBox */
    HWVoiceOut hw;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID audioDeviceId;
    /* The AudioUnit used */
    AudioUnit audioUnit;
    /* A ring buffer for transfering data to the playback thread */
    PIORINGBUFFER pBuf;
} caVoiceOut;

typedef struct caVoiceIn
{
    /* HW voice input struture defined by VBox */
    HWVoiceIn hw;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID audioDeviceId;
    /* The AudioUnit used */
    AudioUnit audioUnit;
    /* An extra buffer used for render the audio data in the recording thread */
    AudioBufferList *pcaBufferList;
    /* A ring buffer for transfering from to the recording thread */
    PIORINGBUFFER pBuf;
} caVoiceIn;


/*******************************************************************************
 *
 * CoreAudio output section
 *
 ******************************************************************************/

/* callback to feed audiooutput buffer */
static OSStatus caPlaybackCallback(void* inRefCon,
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

    /* How much space is used in the ring buffer? */
    csAvail = IORingBufferUsed(caVoice->pBuf) >> caVoice->hw.info.shift; /* bytes -> samples */
    /* How much space is available in the core audio buffer. Use the smaller
     * size of the too. */
    csAvail = RT_MIN(csAvail, ioData->mBuffers[0].mDataByteSize >> caVoice->hw.info.shift);

    Log2(("CoreAudio: [Output] Start reading buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

    /* Iterate as long as data is available */
    while(csReads < csAvail)
    {
        /* How much is left? */
        csToRead = csAvail - csReads;
        cbToRead = csToRead << caVoice->hw.info.shift; /* samples -> bytes */
        Log2(("CoreAudio: [Output] Try reading %RU32 samples (%RU32 bytes)\n", csToRead, cbToRead));
        /* Try to aquire the necessary block from the ring buffer. */
        IORingBufferAquireReadBlock(caVoice->pBuf, cbToRead, &pcSrc, &cbToRead);
        /* How much to we get? */
        csToRead = cbToRead >> caVoice->hw.info.shift; /* bytes -> samples */
        Log2(("CoreAudio: [Output] There are %RU32 samples (%RU32 bytes) available\n", csToRead, cbToRead));
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

    Log2(("CoreAudio: [Output] Finished reading buffer with %RU32 samples (%RU32 bytes)\n", csReads, csReads << caVoice->hw.info.shift));

    return noErr;
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

    /* How much space is available in the ring buffer */
    csAvail = IORingBufferFree(caVoice->pBuf) >> hw->info.shift; /* bytes -> samples */
    /* How much data is availabe. Use the smaller size of the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)audio_pcm_hw_get_live_out(hw));

    Log2(("CoreAudio: [Output] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << hw->info.shift));

    /* Iterate as long as data is available */
    while (csWritten < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToWrite = RT_MIN(csAvail - csWritten, (uint32_t)(hw->samples - hw->rpos));
        cbToWrite = csToWrite << hw->info.shift; /* samples -> bytes */
        Log2(("CoreAudio: [Output] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));
        /* Try to aquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(caVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);
        /* How much to we get? */
        csToWrite = cbToWrite >> hw->info.shift;
        Log2(("CoreAudio: [Output] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));
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

    Log2(("CoreAudio: [Output] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << hw->info.shift));

    /* Return the count of samples we have processed. */
    return csWritten;
}

static int coreaudio_write(SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int coreaudio_init_out(HWVoiceOut *hw, audsettings_t *as)
{
    OSStatus err = noErr;
    UInt32 uSize = 0; /* temporary size of properties */
    UInt32 uFlag = 0; /* for setting flags */
    CFStringRef name; /* for the temporary device name fetching */
    const char *pszName;
    ComponentDescription cd; /* description for an audio component */
    Component cp; /* an audio component */
    AURenderCallbackStruct cb; /* holds the callback structure */
    UInt32 cFrames; /* default frame count */

    caVoiceOut *caVoice = (caVoiceOut *) hw;

    /* Initialize the hardware info section with the audio settings */
    audio_pcm_init_info(&hw->info, as);

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

    /* Try to get the name of the default output device and log it. It's not
     * fatal if it fails. */
    uSize = sizeof(CFStringRef);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 0,
                                 kAudioObjectPropertyName,
                                 &uSize,
                                 &name);
    if (RT_LIKELY(err == noErr))
    {
        pszName = CFStringGetCStringPtr(name, kCFStringEncodingMacRoman);
        if (pszName)
            LogRel(("CoreAudio: Using default output device: %s\n", pszName));
        CFRelease(name);
    }
    else
        LogRel(("CoreAudio: [Output] Unable to get output device name (%RI32)\n", err));

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

    /* CoreAudio will inform us on a second thread when it needs more data for
     * output. Therfor register an callback function which will provide the new
     * data. */
    cb.inputProc = caPlaybackCallback;
    cb.inputProcRefCon = caVoice;

    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Global,
                               0,
                               &cb,
                               sizeof(cb));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set callback (%RI32)\n", err));
        return -1;
    }

    /* Set the default audio output device as the device for the new AudioUnit. */
    err = AudioUnitSetProperty(caVoice->audioUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global,
                               0,
                               &caVoice->audioDeviceId,
                               sizeof(caVoice->audioDeviceId));
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to set current device (%RI32)\n", err));
        return -1;
    }

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
    caAudioSettingsToAudioStreamBasicDescription(as, &caVoice->streamFormat);

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

    /* Get the default frames buffer size, so that we can setup our internal
     * buffers. */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioDevicePropertyBufferFrameSize,
                               kAudioUnitScope_Output,
                               0,
                               &cFrames,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to get frame buffer size (%RI32)\n", err));
        return -1;
    }

/*    cFrames = 1024;*/
/*    err = AudioUnitSetProperty(caVoice->audioUnit,*/
/*                               kAudioDevicePropertyBufferFrameSize,*/
/*                               kAudioUnitScope_Output,*/
/*                               0,*/
/*                               &cFrames,*/
/*                               sizeof(cFrames));*/
/*    if (RT_UNLIKELY(err != noErr))*/
/*    {*/
/*        LogRel(("CoreAudio: [Output] Failed to set frame buffer size (%RI32)\n", err));*/
/*        return -1;*/
/*    }*/

    /* Finaly init the new AudioUnit. */
    err = AudioUnitInitialize(caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Output] Failed to initialize the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Create the internal ring buffer. */
    hw->samples = cFrames * caVoice->streamFormat.mChannelsPerFrame;
    IORingBufferCreate(&caVoice->pBuf, hw->samples << hw->info.shift);
    if (!VALID_PTR(caVoice->pBuf))
    {
        LogRel(("CoreAudio: [Output] Failed to create internal ring buffer\n"));
        AudioUnitUninitialize(caVoice->audioUnit);
        return -1;
    }

    Log(("CoreAudio: [Output] HW samples: %d; Frame count: %RU32\n", hw->samples, cFrames));

    return 0;
}

static void coreaudio_fini_out(HWVoiceOut *hw)
{
    OSStatus err = noErr;
    caVoiceOut *caVoice = (caVoiceOut *) hw;

    /* Only stop the device if it is actually running */
    if (caIsRunning(caVoice->audioDeviceId))
        err = AudioOutputUnitStop(caVoice->audioUnit);
    if (RT_LIKELY(err == noErr))
    {
        err = AudioUnitUninitialize(caVoice->audioUnit);
        if (RT_LIKELY(err == noErr))
        {
            caVoice->audioDeviceId = kAudioDeviceUnknown;
            IORingBufferDestroy(caVoice->pBuf);
        }
        else
            LogRel(("CoreAudio: [Output] Failed to uninitialize the AudioUnit (%RI32)\n", err));
    }
    else
        LogRel(("CoreAudio: [Output] Failed to stop playback (%RI32)\n", err));
}

static int coreaudio_ctl_out(HWVoiceOut *hw, int cmd, ...)
{
    OSStatus err = noErr;
    caVoiceOut *caVoice = (caVoiceOut *) hw;

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
                    LogRel(("CoreAudio: [Output] Failed to start playback (%RI32)\n", err));
                    return -1;
                }
                break;
            }
        case VOICE_DISABLE:
            {
                /* Only stop the device if it is actually running */
                if (caIsRunning(caVoice->audioDeviceId))
                    err = AudioOutputUnitStop(caVoice->audioUnit);
                if (RT_UNLIKELY(err != noErr))
                {
                    LogRel(("CoreAudio: [Input] Failed to stop playback (%RI32)\n", err));
                    return -1;
                }
                break;
            }
    }
    return 0;
}

/*******************************************************************************
 *
 * CoreAudio input section
 *
 ******************************************************************************/

/* callback to feed audio input buffer */
static OSStatus caRecordingCallback(void* inRefCon,
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

    caVoiceIn *caVoice = (caVoiceIn *) inRefCon;

    /* If nothing is pending return immediately. */
    if (inNumberFrames == 0)
        return noErr;

    caVoice->pcaBufferList->mBuffers[0].mDataByteSize = caVoice->streamFormat.mBytesPerFrame * inNumberFrames;
    caVoice->pcaBufferList->mBuffers[0].mData = RTMemAlloc(caVoice->pcaBufferList->mBuffers[0].mDataByteSize);

    err = AudioUnitRender(caVoice->audioUnit,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames,
                          caVoice->pcaBufferList);
    if(RT_UNLIKELY(err != noErr))
    {
        Log(("CoreAudio: [Input] Failed to render audio data (%RI32)\n", err));
        RTMemFree(caVoice->pcaBufferList->mBuffers[0].mData);
        return err;
    }

    /* How much space is free in the ring buffer? */
    csAvail = IORingBufferFree(caVoice->pBuf) >> caVoice->hw.info.shift; /* bytes -> samples */
    /* How much space is used in the core audio buffer. Use the smaller size of
     * the too. */
    csAvail = RT_MIN(csAvail, caVoice->pcaBufferList->mBuffers[0].mDataByteSize >> caVoice->hw.info.shift);

    Log2(("CoreAudio: [Input] Start writing buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

    /* Iterate as long as data is available */
    while(csWritten < csAvail)
    {
        /* How much is left? */
        csToWrite = csAvail - csWritten;
        cbToWrite = csToWrite << caVoice->hw.info.shift;
        Log2(("CoreAudio: [Input] Try writing %RU32 samples (%RU32 bytes)\n", csToWrite, cbToWrite));
        /* Try to aquire the necessary space from the ring buffer. */
        IORingBufferAquireWriteBlock(caVoice->pBuf, cbToWrite, &pcDst, &cbToWrite);
        /* How much to we get? */
        csToWrite = cbToWrite >> caVoice->hw.info.shift;
        Log2(("CoreAudio: [Input] There is space for %RU32 samples (%RU32 bytes) available\n", csToWrite, cbToWrite));
        /* Break if nothing is free anymore. */
        if (RT_UNLIKELY(cbToWrite == 0))
            break;
        /* Copy the data from the core audio buffer to the ring buffer. */
        memcpy(pcDst, (char*)caVoice->pcaBufferList->mBuffers[0].mData + (csWritten << caVoice->hw.info.shift), cbToWrite);
        /* Release the ring buffer, so the main thread could start reading this data. */
        IORingBufferReleaseWriteBlock(caVoice->pBuf, cbToWrite);
        csWritten += csToWrite;
    }
    /* Cleanup */
    RTMemFree(caVoice->pcaBufferList->mBuffers[0].mData);

    Log2(("CoreAudio: [Input] Finished writing buffer with %RU32 samples (%RU32 bytes)\n", csWritten, csWritten << caVoice->hw.info.shift));

    return noErr;
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

    /* How much space is used in the ring buffer? */
    csAvail = IORingBufferUsed(caVoice->pBuf) >> hw->info.shift; /* bytes -> samples */
    /* How much space is available in the mix buffer. Use the smaller size of
     * the too. */
    csAvail = RT_MIN(csAvail, (uint32_t)(hw->samples - audio_pcm_hw_get_live_in (hw)));

    Log2(("CoreAudio: [Input] Start reading buffer with %RU32 samples (%RU32 bytes)\n", csAvail, csAvail << caVoice->hw.info.shift));

    /* Iterate as long as data is available */
    while (csReads < csAvail)
    {
        /* How much is left? Split request at the end of our samples buffer. */
        csToRead = RT_MIN(csAvail - csReads, (uint32_t)(hw->samples - hw->wpos));
        cbToRead = csToRead << hw->info.shift;
        Log2(("CoreAudio: [Input] Try reading %RU32 samples (%RU32 bytes)\n", csToRead, cbToRead));
        /* Try to aquire the necessary block from the ring buffer. */
        IORingBufferAquireReadBlock(caVoice->pBuf, cbToRead, &pcSrc, &cbToRead);
        /* How much to we get? */
        csToRead = cbToRead >> hw->info.shift;
        Log2(("CoreAudio: [Input] There are %RU32 samples (%RU32 bytes) available\n", csToRead, cbToRead));
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

    Log2(("CoreAudio: [Input] Finished reading buffer with %RU32 samples (%RU32 bytes)\n", csReads, csReads << caVoice->hw.info.shift));

    return csReads;
}

static int coreaudio_read(SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}

static int coreaudio_init_in(HWVoiceIn *hw, audsettings_t *as)
{
    OSStatus err = noErr;
    int rc = -1;
    UInt32 uSize = 0; /* temporary size of properties */
    UInt32 uFlag = 0; /* for setting flags */
    CFStringRef name; /* for the temporary device name fetching */
    const char *pszName;
    ComponentDescription cd; /* description for an audio component */
    Component cp; /* an audio component */
    AURenderCallbackStruct cb; /* holds the callback structure */
    UInt32 cFrames; /* default frame count */

    caVoiceIn *caVoice = (caVoiceIn *) hw;

    /* Initialize the hardware info section with the audio settings */
    audio_pcm_init_info(&hw->info, as);

    /* Fetch the default audio input device currently in use */
    uSize = sizeof(caVoice->audioDeviceId);
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                   &uSize,
                                   &caVoice->audioDeviceId);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Unable to find default input device (%RI32)\n", err));
        return -1;
    }

    /* Try to get the name of the default input device and log it. It's not
     * fatal if it fails. */
    uSize = sizeof(CFStringRef);
    err = AudioDeviceGetProperty(caVoice->audioDeviceId,
                                 0,
                                 1,
                                 kAudioObjectPropertyName,
                                 &uSize,
                                 &name);
    if (RT_LIKELY(err == noErr))
    {
        pszName = CFStringGetCStringPtr(name, kCFStringEncodingMacRoman);
        if (pszName)
            LogRel(("CoreAudio: Using default input device: %s\n", pszName));
        CFRelease(name);
    }
    else
        LogRel(("CoreAudio: [Input] Unable to get input device name (%RI32)\n", err));

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

    /* CoreAudio will inform us on a second thread for new incoming audio data.
     * Therfor register an callback function, which will process the new data.
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

/*    UInt32 fpb = 1024;*/
/*    if (AudioDeviceSetProperty(core->audioDeviceId,*/
/*                               NULL,*/
/*                               0,*/
/*                               1,*/
/*                               kAudioDevicePropertyBufferSize,*/
/*                               sizeof(fpb), &fpb) != noErr)*/
/*    {*/
/*        LogRel(("CoreAudio: [Input] Unable to set frame buffer size)\n"));*/
/*        return -1;*/
/*    }*/

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
    caAudioSettingsToAudioStreamBasicDescription(as, &caVoice->streamFormat);

#if DEBUG
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Input] device", &caVoice->deviceFormat);
    caDebugOutputAudioStreamBasicDescription("CoreAudio: [Input] input", &caVoice->streamFormat);
#endif /* DEBUG */

    /* If the frequency of the device is different from the requested one we
     * need a converter. */
    if (caVoice->deviceFormat.mSampleRate != caVoice->streamFormat.mSampleRate)
    {
        Log(("CoreAudio: [Input] Converter in use\n"));
        /* Set the device format description for the stream. */
        err = AudioUnitSetProperty(caVoice->audioUnit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Output,
                                   1,
                                   &caVoice->deviceFormat,
                                   sizeof(caVoice->deviceFormat));
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
    }
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to set stream format (%RI32)\n", err));
        return -1;
    }

    /* Get the default frames per slice size, so that we can setup our internal
     * buffers. */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(caVoice->audioUnit,
                               kAudioDevicePropertyBufferFrameSize,
                               kAudioUnitScope_Global,
                               0,
                               &cFrames,
                               &uSize);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to get maximum frame per slice (%RI32)\n", err));
        return -1;
    }

    /* Finaly init the new AudioUnit. */
    err = AudioUnitInitialize(caVoice->audioUnit);
    if (RT_UNLIKELY(err != noErr))
    {
        LogRel(("CoreAudio: [Input] Failed to initialize the AudioUnit (%RI32)\n", err));
        return -1;
    }

    /* Set to zero first */
    caVoice->pcaBufferList = NULL;
    caVoice->pBuf = NULL;
    /* Create the AudioBufferList structure with one buffer. */
    caVoice->pcaBufferList = RTMemAllocZ(sizeof(AudioBufferList) + sizeof(AudioBuffer));
    if (VALID_PTR(caVoice->pcaBufferList))
    {
        caVoice->pcaBufferList->mNumberBuffers = 1;
        /* Initialize the buffer to nothing. */
        caVoice->pcaBufferList->mBuffers[0].mNumberChannels = caVoice->streamFormat.mChannelsPerFrame;
        caVoice->pcaBufferList->mBuffers[0].mDataByteSize = 0;
        caVoice->pcaBufferList->mBuffers[0].mData = NULL;

        /* Create the internal ring buffer. */
        hw->samples = cFrames * caVoice->streamFormat.mChannelsPerFrame;
        IORingBufferCreate(&caVoice->pBuf, hw->samples << hw->info.shift);
        if (VALID_PTR(caVoice->pBuf))
            rc = 0;
        else
            LogRel(("CoreAudio: [Input] Failed to create internal ring buffer\n"));
    }
    else
        LogRel(("CoreAudio: [Input] Failed to create the audio buffer list\n"));

    if (rc != 0)
    {
        if (caVoice->pcaBufferList)
            RTMemFree(caVoice->pcaBufferList);
        if (caVoice->pBuf)
            IORingBufferDestroy(caVoice->pBuf);
        AudioUnitUninitialize(caVoice->audioUnit);
    }

    Log(("CoreAudio: [Input] HW samples: %d; Frame count: %RU32\n", hw->samples, cFrames));

    return 0;
}

static void coreaudio_fini_in(HWVoiceIn *hw)
{
    OSStatus err = noErr;
    caVoiceIn *caVoice = (caVoiceIn *) hw;

    /* Only stop the device if it is actually running */
    if (caIsRunning(caVoice->audioDeviceId))
        err = AudioOutputUnitStop(caVoice->audioUnit);
    if (RT_LIKELY(err == noErr))
    {
        err = AudioUnitUninitialize(caVoice->audioUnit);
        if (RT_LIKELY(err == noErr))
        {
            caVoice->audioDeviceId = kAudioDeviceUnknown;
            IORingBufferDestroy(caVoice->pBuf);
            RTMemFree(caVoice->pcaBufferList);
        }
        else
            LogRel(("CoreAudio: [Input] Failed to uninitialize the AudioUnit (%RI32)\n", err));
    }
    else
        LogRel(("CoreAudio: [Input] Failed to stop recording (%RI32)\n", err));
}

static int coreaudio_ctl_in(HWVoiceIn *hw, int cmd, ...)
{
    OSStatus err = noErr;
    caVoiceIn *caVoice = (caVoiceIn *) hw;

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
                    err = AudioOutputUnitStop(caVoice->audioUnit);
                if (RT_UNLIKELY(err != noErr))
                {
                    LogRel(("CoreAudio: [Input] Failed to stop recording (%RI32)\n", err));
                    return -1;
                }
                break;
            }
    }
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
    {"BUFFER_SIZE", AUD_OPT_INT, &conf.cBufferFrames,
     "Size of the buffer in frames", NULL, 0},
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

