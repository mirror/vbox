/* $Id$ */
/** @file
 * VBox audio devices: Mac OS X CoreAudio audio driver.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>
#include <iprt/mem.h>

#include "vl_vbox.h"
#include <iprt/uuid.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioConverter.h>

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

/* TODO:
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

/**
 * Host Coreaudio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTCOREAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS    pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO IHostAudio;
} DRVHOSTCOREAUDIO, *PDRVHOSTCOREAUDIO;

/*******************************************************************************
 *
 * Helper function section
 *
 ******************************************************************************/

#ifdef DEBUG
static void drvHostCoreAudioPrintASBDesc(const char *pszDesc, const AudioStreamBasicDescription *pStreamDesc)
{
    char pszSampleRate[32];
    Log(("%s AudioStreamBasicDescription:\n", pszDesc));
    LogFlowFunc(("Format ID: %RU32 (%c%c%c%c)\n", pStreamDesc->mFormatID,
                 RT_BYTE4(pStreamDesc->mFormatID), RT_BYTE3(pStreamDesc->mFormatID),
                 RT_BYTE2(pStreamDesc->mFormatID), RT_BYTE1(pStreamDesc->mFormatID)));
    LogFlowFunc(("Flags: %RU32", pStreamDesc->mFormatFlags));
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
    snprintf(pszSampleRate, 32, "%.2f", (float)pStreamDesc->mSampleRate); /** @todo r=andy Use RTStrPrint*. */
    LogFlowFunc(("SampleRate      : %s\n", pszSampleRate));
    LogFlowFunc(("ChannelsPerFrame: %RU32\n", pStreamDesc->mChannelsPerFrame));
    LogFlowFunc(("FramesPerPacket : %RU32\n", pStreamDesc->mFramesPerPacket));
    LogFlowFunc(("BitsPerChannel  : %RU32\n", pStreamDesc->mBitsPerChannel));
    LogFlowFunc(("BytesPerFrame   : %RU32\n", pStreamDesc->mBytesPerFrame));
    LogFlowFunc(("BytesPerPacket  : %RU32\n", pStreamDesc->mBytesPerPacket));
}
#endif /* DEBUG */

static void drvHostCoreAudioPCMInfoToASBDesc(PDMPCMPROPS *pPcmProperties, AudioStreamBasicDescription *pStreamDesc)
{
    pStreamDesc->mFormatID         = kAudioFormatLinearPCM;
    pStreamDesc->mFormatFlags      = kAudioFormatFlagIsPacked;
    pStreamDesc->mFramesPerPacket  = 1;
    pStreamDesc->mSampleRate       = (Float64)pPcmProperties->uHz;
    pStreamDesc->mChannelsPerFrame = pPcmProperties->cChannels;
    pStreamDesc->mBitsPerChannel   = pPcmProperties->cBits;
    if (pPcmProperties->fSigned)
        pStreamDesc->mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    pStreamDesc->mBytesPerFrame    = pStreamDesc->mChannelsPerFrame * (pStreamDesc->mBitsPerChannel / 8);
    pStreamDesc->mBytesPerPacket   = pStreamDesc->mFramesPerPacket * pStreamDesc->mBytesPerFrame;
}

static OSStatus drvHostCoreAudioSetFrameBufferSize(AudioDeviceID deviceID, bool fInput, UInt32 cReqSize, UInt32 *pcActSize)
{
    AudioObjectPropertyScope propScope = fInput
                                       ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    AudioObjectPropertyAddress propAdr = { kAudioDevicePropertyBufferFrameSize, propScope,
                                           kAudioObjectPropertyElementMaster };

    /* First try to set the new frame buffer size. */
    OSStatus err = AudioObjectSetPropertyData(deviceID, &propAdr, NULL, 0, sizeof(cReqSize), &cReqSize);

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

DECL_FORCE_INLINE(bool) drvHostCoreAudioIsRunning(AudioDeviceID deviceID)
{
    AudioObjectPropertyAddress propAdr = { kAudioDevicePropertyDeviceIsRunning, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };
    UInt32 uFlag = 0;
    UInt32 uSize = sizeof(uFlag);
    OSStatus err = AudioObjectGetPropertyData(deviceID, &propAdr, 0, NULL, &uSize, &uFlag);
    if (err != kAudioHardwareNoError)
        LogRel(("CoreAudio: Could not determine whether the device is running (%RI32)\n", err));

    return (uFlag >= 1);
}

static int drvHostCoreAudioCFStringToCString(const CFStringRef pCFString, char **ppszString)
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

static AudioDeviceID drvHostCoreAudioDeviceUIDtoID(const char* pszUID)
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

typedef struct COREAUDIOSTREAMOUT
{
    /** Host stream out. */
    PDMAUDIOHSTSTRMOUT streamOut;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID deviceID;
    /* The AudioUnit used */
    AudioUnit audioUnit;
    /* A ring buffer for transferring data to the playback thread. */
    PRTCIRCBUF pBuf;
    /* Temporary buffer for copying over audio data into Core Audio. */
    void *pvPCMBuf;
    /** Size of the temporary buffer. */
    size_t cbPCMBuf;
    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;
} COREAUDIOSTREAMOUT, *PCOREAUDIOSTREAMOUT;

typedef struct COREAUDIOSTREAMIN
{
    /** Host stream in. */
    PDMAUDIOHSTSTRMIN streamIn;
    /* Stream description which is default on the device */
    AudioStreamBasicDescription deviceFormat;
    /* Stream description which is selected for using by VBox */
    AudioStreamBasicDescription streamFormat;
    /* The audio device ID of the currently used device */
    AudioDeviceID deviceID;
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
    PRTCIRCBUF pBuf;
    /* Initialization status tracker. Used when some of the device parameters
     * or the device itself is changed during the runtime. */
    volatile uint32_t status;
} COREAUDIOSTREAMIN, *PCOREAUDIOSTREAMIN;

static int drvHostCoreAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvHostCoreAudioInitInput(PPDMAUDIOHSTSTRMIN pHstStrmIn, uint32_t *pcSamples);
static int drvHostCoreAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn);
static int drvHostCoreAudioReinitInput(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn);

static int drvHostCoreAudioControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvHostCoreAudioInitOutput(PPDMAUDIOHSTSTRMOUT pHstStrmOut, uint32_t *pcSamples);
static int drvHostCoreAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut);
static int drvHostCoreAudioReinitOutput(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut);
static OSStatus drvHostCoreAudioPlaybackAudioDevicePropertyChanged(AudioObjectID propertyID, UInt32 nAddresses, const AudioObjectPropertyAddress properties[], void *pvUser);
static OSStatus drvHostCoreAudioPlaybackCallback(void *pvUser, AudioUnitRenderActionFlags *pActionFlags, const AudioTimeStamp *pAudioTS, UInt32 uBusID, UInt32 cFrames, AudioBufferList* pBufData);

/* Callback for getting notified when the default input/output device has been changed. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioDefaultDeviceChanged(AudioObjectID propertyID,
                                                                   UInt32 nAddresses,
                                                                   const AudioObjectPropertyAddress properties[],
                                                                   void *pvUser)
{
    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pvUser;

    OSStatus err = noErr;

    switch (propertyID)
    {
        case kAudioHardwarePropertyDefaultInputDevice:
        {
            /* This listener is called on every change of the hardware
             * device. So check if the default device has really changed. */
            AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultInputDevice,
                                                   kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

            UInt32 uSize = sizeof(pStreamIn->deviceID);
            UInt32 uResp;
            err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdr, 0, NULL, &uSize, &uResp);

            if (err == noErr)
            {
                if (pStreamIn->deviceID != uResp)
                {
                    LogRel(("CoreAudio: Default input device has changed\n"));

                    /* We move the reinitialization to the next input event.
                     * This make sure this thread isn't blocked and the
                     * reinitialization is done when necessary only. */
                    ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_REINIT);
                }
            }
            break;
        }

        default:
            break;
    }

    return noErr;
}

static int drvHostCoreAudioReinitInput(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    PPDMDRVINS pDrvIns      = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pHstStrmIn;

    drvHostCoreAudioFiniIn(pInterface, &pStreamIn->streamIn);

    drvHostCoreAudioInitInput(&pStreamIn->streamIn, NULL /* pcSamples */);
    drvHostCoreAudioControlIn(pInterface, &pStreamIn->streamIn, PDMAUDIOSTREAMCMD_ENABLE);

    return VINF_SUCCESS;
}

static int drvHostCoreAudioReinitOutput(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PPDMDRVINS pDrvIns      = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    drvHostCoreAudioFiniOut(pInterface, &pStreamOut->streamOut);

    drvHostCoreAudioInitOutput(&pStreamOut->streamOut, NULL /* pcSamples */);
    drvHostCoreAudioControlOut(pInterface, &pStreamOut->streamOut, PDMAUDIOSTREAMCMD_ENABLE);

    return VINF_SUCCESS;
}

/* Callback for getting notified when some of the properties of an audio device has changed. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioRecordingAudioDevicePropertyChanged(AudioObjectID                     propertyID,
                                                                                  UInt32                            cAdresses,
                                                                                  const AudioObjectPropertyAddress  aProperties[],
                                                                                  void                             *pvUser)
{
    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pvUser;

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
            LogRel(("CoreAudio: Recording sample rate changed\n"));

            /* We move the reinitialization to the next input event.
             * This make sure this thread isn't blocked and the
             * reinitialization is done when necessary only. */
            ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_REINIT);
            break;
        }

        default:
            break;
    }

    return noErr;
}

/* Callback to convert audio input data from one format to another. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioConverterCallback(AudioConverterRef              converterID,
                                                                UInt32                        *pcPackets,
                                                                AudioBufferList               *pBufData,
                                                                AudioStreamPacketDescription **ppPacketDesc,
                                                                void                          *pvUser)
{
    /** @todo Check incoming pointers. */

    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pvUser;

    /** @todo Check converter ID? */

    const AudioBufferList *pBufferList = &pStreamIn->bufferList;

    if (ASMAtomicReadU32(&pStreamIn->status) != CA_STATUS_INIT)
        return noErr;

    /** @todo In principle we had to check here if the source is non interleaved, and if so,
     *        so go through all buffers not only the first one like now. */

    /* Use the lower one of the packets to process & the available packets in the buffer. */
    Assert(pBufferList->mBuffers[0].mDataByteSize >= pStreamIn->rpos);
    UInt32 cSize = RT_MIN(*pcPackets * pStreamIn->deviceFormat.mBytesPerPacket,
                          pBufferList->mBuffers[0].mDataByteSize - pStreamIn->rpos);

    /* Set the new size on output, so the caller know what we have processed. */
    Assert(pStreamIn->deviceFormat.mBytesPerPacket);
    *pcPackets = cSize / pStreamIn->deviceFormat.mBytesPerPacket;

    OSStatus err;

    /* If no data is available anymore we return with an error code. This error code will be returned
     * from AudioConverterFillComplexBuffer. */
    if (*pcPackets == 0)
    {
        pBufData->mBuffers[0].mDataByteSize = 0;
        pBufData->mBuffers[0].mData         = NULL;

        err = caConverterEOFDErr;
    }
    else
    {
        pBufData->mBuffers[0].mNumberChannels = pBufferList->mBuffers[0].mNumberChannels;
        pBufData->mBuffers[0].mDataByteSize   = cSize;
        pBufData->mBuffers[0].mData           = (uint8_t *)pBufferList->mBuffers[0].mData + pStreamIn->rpos;

        pStreamIn->rpos += cSize;

        err = noErr;
    }

    return err;
}

/* Callback to feed audio input buffer. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioRecordingCallback(void                       *pvUser,
                                                                AudioUnitRenderActionFlags *pActionFlags,
                                                                const AudioTimeStamp       *pAudioTS,
                                                                UInt32                      uBusID,
                                                                UInt32                      cFrames,
                                                                AudioBufferList            *pBufData)
{
    PCOREAUDIOSTREAMIN pStreamIn  = (PCOREAUDIOSTREAMIN)pvUser;
    PPDMAUDIOHSTSTRMIN pHstStrmIN = &pStreamIn->streamIn;

    if (ASMAtomicReadU32(&pStreamIn->status) != CA_STATUS_INIT)
        return noErr;

    /* If nothing is pending return immediately. */
    if (cFrames == 0)
        return noErr;

    OSStatus err = noErr;
    int rc = VINF_SUCCESS;

    do
    {
        /* Are we using a converter? */
        if (pStreamIn->converter)
        {
            /* First, render the data as usual. */
            pStreamIn->bufferList.mBuffers[0].mNumberChannels = pStreamIn->deviceFormat.mChannelsPerFrame;
            pStreamIn->bufferList.mBuffers[0].mDataByteSize   = pStreamIn->deviceFormat.mBytesPerFrame * cFrames;
            AssertBreakStmt(pStreamIn->bufferList.mBuffers[0].mDataByteSize, rc = VERR_INVALID_PARAMETER);
            pStreamIn->bufferList.mBuffers[0].mData           = RTMemAlloc(pStreamIn->bufferList.mBuffers[0].mDataByteSize);
            if (!pStreamIn->bufferList.mBuffers[0].mData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            err = AudioUnitRender(pStreamIn->audioUnit, pActionFlags, pAudioTS, uBusID, cFrames, &pStreamIn->bufferList);
            if (err != noErr)
            {
                LogFlowFunc(("Failed rendering audio data (%RI32)\n", err));
                rc = VERR_IO_GEN_FAILURE; /** @todo Improve this. */
                break;
            }

            size_t cbAvail = RT_MIN(RTCircBufFree(pStreamIn->pBuf), pStreamIn->bufferList.mBuffers[0].mDataByteSize);

            /* Initialize the temporary output buffer */
            AudioBufferList tmpList;
            tmpList.mNumberBuffers = 1;
            tmpList.mBuffers[0].mNumberChannels = pStreamIn->streamFormat.mChannelsPerFrame;

            /* Iterate as long as data is available. */
            uint8_t *puDst = NULL;
            while (cbAvail)
            {
                /* Try to acquire the necessary space from the ring buffer. */
                size_t cbToWrite = 0;
                RTCircBufAcquireWriteBlock(pStreamIn->pBuf, cbAvail, (void **)&puDst, &cbToWrite);
                if (!cbToWrite)
                    break;

                /* Now set how much space is available for output. */
                Assert(pStreamIn->streamFormat.mBytesPerPacket);

                UInt32 ioOutputDataPacketSize = cbToWrite / pStreamIn->streamFormat.mBytesPerPacket;

                /* Set our ring buffer as target. */
                tmpList.mBuffers[0].mDataByteSize = cbToWrite;
                tmpList.mBuffers[0].mData         = puDst;

                AudioConverterReset(pStreamIn->converter);

                err = AudioConverterFillComplexBuffer(pStreamIn->converter, drvHostCoreAudioConverterCallback, pStreamIn,
                                                      &ioOutputDataPacketSize, &tmpList, NULL);
                if(   err != noErr
                   && err != caConverterEOFDErr)
                {
                    LogFlowFunc(("Failed to convert audio data (%RI32:%c%c%c%c)\n", err,
                                 RT_BYTE4(err), RT_BYTE3(err), RT_BYTE2(err), RT_BYTE1(err)));
                    rc = VERR_IO_GEN_FAILURE;
                    break;
                }

                /* Check in any case what processed size is returned. It could be less than we expected. */
                cbToWrite = ioOutputDataPacketSize * pStreamIn->streamFormat.mBytesPerPacket;

                /* Release the ring buffer, so the main thread could start reading this data. */
                RTCircBufReleaseWriteBlock(pStreamIn->pBuf, cbToWrite);

                /* If the error is "End of Data" it means there is no data anymore
                 * which could be converted. So end here now. */
                if (err == caConverterEOFDErr)
                    break;

                Assert(cbAvail >= cbToWrite);
                cbAvail -= cbToWrite;
            }
        }
        else /* No converter being used. */
        {
            pStreamIn->bufferList.mBuffers[0].mNumberChannels = pStreamIn->streamFormat.mChannelsPerFrame;
            pStreamIn->bufferList.mBuffers[0].mDataByteSize   = pStreamIn->streamFormat.mBytesPerFrame * cFrames;
            AssertBreakStmt(pStreamIn->bufferList.mBuffers[0].mDataByteSize, rc = VERR_INVALID_PARAMETER);
            pStreamIn->bufferList.mBuffers[0].mData           = RTMemAlloc(pStreamIn->bufferList.mBuffers[0].mDataByteSize);
            if (!pStreamIn->bufferList.mBuffers[0].mData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            err = AudioUnitRender(pStreamIn->audioUnit, pActionFlags, pAudioTS, uBusID, cFrames, &pStreamIn->bufferList);
            if (err != noErr)
            {
                LogFlowFunc(("Failed rendering audio data (%RI32)\n", err));
                rc = VERR_IO_GEN_FAILURE; /** @todo Improve this. */
                break;
            }

            size_t cbAvail = RT_MIN(RTCircBufFree(pStreamIn->pBuf), pStreamIn->bufferList.mBuffers[0].mDataByteSize);

            /* Iterate as long as data is available. */
            uint8_t *puDst = NULL;
            uint32_t cbWrittenTotal = 0;
            while(cbAvail)
            {
                /* Try to acquire the necessary space from the ring buffer. */
                size_t cbToWrite = 0;
                RTCircBufAcquireWriteBlock(pStreamIn->pBuf, cbAvail, (void **)&puDst, &cbToWrite);
                if (!cbToWrite)
                    break;

                /* Copy the data from the core audio buffer to the ring buffer. */
                memcpy(puDst, (uint8_t *)pStreamIn->bufferList.mBuffers[0].mData + cbWrittenTotal, cbToWrite);

                /* Release the ring buffer, so the main thread could start reading this data. */
                RTCircBufReleaseWriteBlock(pStreamIn->pBuf, cbToWrite);

                cbWrittenTotal += cbToWrite;

                Assert(cbAvail >= cbToWrite);
                cbAvail -= cbToWrite;
            }
        }

        if (pStreamIn->bufferList.mBuffers[0].mData)
        {
            RTMemFree(pStreamIn->bufferList.mBuffers[0].mData);
            pStreamIn->bufferList.mBuffers[0].mData = NULL;
        }

    } while (0);

    return err;
}

/** @todo Eventually split up this function, as this already is huge! */
static int drvHostCoreAudioInitInput(PPDMAUDIOHSTSTRMIN pHstStrmIn, uint32_t *pcSamples)
{
    OSStatus err = noErr;

    UInt32 cFrames; /* default frame count */
    UInt32 cSamples; /* samples count */

    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pHstStrmIn;

    ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_IN_INIT);

    UInt32 uSize = 0;
    if (pStreamIn->deviceID == kAudioDeviceUnknown)
    {
        /* Fetch the default audio input device currently in use. */
        AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultInputDevice,
                                               kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        uSize = sizeof(pStreamIn->deviceID);
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdr, 0, NULL, &uSize,  &pStreamIn->deviceID);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Unable to determine default input device (%RI32)\n", err));
            return VERR_NOT_FOUND;
        }
    }

    /*
     * Try to get the name of the input device and log it. It's not fatal if it fails.
     */
    CFStringRef strTemp;

    AudioObjectPropertyAddress propAdr = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };
    uSize = sizeof(CFStringRef);
    err = AudioObjectGetPropertyData(pStreamIn->deviceID, &propAdr, 0, NULL, &uSize, &strTemp);
    if (err == noErr)
    {
        char *pszDevName = NULL;
        err = drvHostCoreAudioCFStringToCString(strTemp, &pszDevName);
        if (err == noErr)
        {
            CFRelease(strTemp);

            /* Get the device' UUID. */
            propAdr.mSelector = kAudioDevicePropertyDeviceUID;
            err = AudioObjectGetPropertyData(pStreamIn->deviceID, &propAdr, 0, NULL, &uSize, &strTemp);
            if (err == noErr)
            {
                char *pszUID = NULL;
                err = drvHostCoreAudioCFStringToCString(strTemp, &pszUID);
                if (err == noErr)
                {
                    CFRelease(strTemp);
                    LogRel(("CoreAudio: Using input device: %s (UID: %s)\n", pszDevName, pszUID));

                    RTMemFree(pszUID);
                }
            }

            RTMemFree(pszDevName);
        }
    }
    else
        LogRel(("CoreAudio: Unable to determine input device name (%RI32)\n", err));

    /* Get the default frames buffer size, so that we can setup our internal buffers. */
    uSize = sizeof(cFrames);
    propAdr.mSelector = kAudioDevicePropertyBufferFrameSize;
    propAdr.mScope    = kAudioDevicePropertyScopeInput;
    err = AudioObjectGetPropertyData(pStreamIn->deviceID, &propAdr, 0, NULL, &uSize, &cFrames);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to determine frame buffer size of the audio input device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Set the frame buffer size and honor any minimum/maximum restrictions on the device. */
    err = drvHostCoreAudioSetFrameBufferSize(pStreamIn->deviceID, true /* fInput */, cFrames, &cFrames);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set frame buffer size for the audio input device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    ComponentDescription cd;
    RT_ZERO(cd);
    cd.componentType         = kAudioUnitType_Output;
    cd.componentSubType      = kAudioUnitSubType_HALOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;

    /* Try to find the default HAL output component. */
    Component cp = FindNextComponent(NULL, &cd);
    if (cp == 0)
    {
        LogRel(("CoreAudio: Failed to find HAL output component\n")); /** @todo Return error value? */
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Open the default HAL output component. */
    err = OpenAComponent(cp, &pStreamIn->audioUnit);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to open output component (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Switch the I/O mode for input to on. */
    UInt32 uFlag = 1;
    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
                               1, &uFlag, sizeof(uFlag));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to disable input I/O mode for input stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Switch the I/O mode for input to off. This is important, as this is a pure input stream. */
    uFlag = 0;
    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                               0, &uFlag, sizeof(uFlag));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to disable output I/O mode for input stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Set the default audio input device as the device for the new AudioUnit. */
    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
                               0, &pStreamIn->deviceID, sizeof(pStreamIn->deviceID));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set current device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * CoreAudio will inform us on a second thread for new incoming audio data.
     * Therefor register a callback function which will process the new data.
     */
    AURenderCallbackStruct cb;
    RT_ZERO(cb);
    cb.inputProc       = drvHostCoreAudioRecordingCallback;
    cb.inputProcRefCon = pStreamIn;

    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global,
                               0, &cb, sizeof(cb));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to register input callback (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Fetch the current stream format of the device. */
    uSize = sizeof(pStreamIn->deviceFormat);
    err = AudioUnitGetProperty(pStreamIn->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                               1, &pStreamIn->deviceFormat, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to get device format (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Create an AudioStreamBasicDescription based on our required audio settings. */
    drvHostCoreAudioPCMInfoToASBDesc(&pStreamIn->streamIn.Props, &pStreamIn->streamFormat);

#ifdef DEBUG
    drvHostCoreAudioPrintASBDesc("CoreAudio: Input device", &pStreamIn->deviceFormat);
    drvHostCoreAudioPrintASBDesc("CoreAudio: Input stream", &pStreamIn->streamFormat);
#endif /* DEBUG */

    /* If the frequency of the device is different from the requested one we
     * need a converter. The same count if the number of channels is different. */
    if (   pStreamIn->deviceFormat.mSampleRate       != pStreamIn->streamFormat.mSampleRate
        || pStreamIn->deviceFormat.mChannelsPerFrame != pStreamIn->streamFormat.mChannelsPerFrame)
    {
        err = AudioConverterNew(&pStreamIn->deviceFormat, &pStreamIn->streamFormat, &pStreamIn->converter);
        if (RT_UNLIKELY(err != noErr))
        {
            LogRel(("CoreAudio: Failed to create the audio converte(%RI32). Input Format=%d, Output Foramt=%d\n",
                     err, pStreamIn->deviceFormat, pStreamIn->streamFormat));
            return VERR_GENERAL_FAILURE; /** @todo Fudge! */
        }

        if (   pStreamIn->deviceFormat.mChannelsPerFrame == 1 /* Mono */
            && pStreamIn->streamFormat.mChannelsPerFrame == 2 /* Stereo */)
        {
            /*
             * If the channel count is different we have to tell this the converter
             * and supply a channel mapping. For now we only support mapping
             * from mono to stereo. For all other cases the core audio defaults
             * are used, which means dropping additional channels in most
             * cases.
             */
            const SInt32 channelMap[2] = {0, 0}; /* Channel map for mono -> stereo, */

            err = AudioConverterSetProperty(pStreamIn->converter, kAudioConverterChannelMap, sizeof(channelMap), channelMap);
            if (err != noErr)
            {
                LogRel(("CoreAudio: Failed to set channel mapping for the audio input converter (%RI32)\n", err));
                return VERR_GENERAL_FAILURE; /** @todo Fudge! */
            }
        }

        /* Set the new input format description for the stream. */
        err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   1, &pStreamIn->deviceFormat, sizeof(pStreamIn->deviceFormat));
        if (err != noErr)
        {
            LogRel(("CoreAudio: Failed to set input format for input stream (%RI32)\n", err));
            return VERR_GENERAL_FAILURE; /** @todo Fudge! */
        }
#if 0
        /* Set sample rate converter quality to maximum */
        uFlag = kAudioConverterQuality_Max;
        err = AudioConverterSetProperty(pStreamIn->converter, kAudioConverterSampleRateConverterQuality,
                                        sizeof(uFlag), &uFlag);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to set input audio converter quality to the maximum (%RI32)\n", err));
#endif
        LogRel(("CoreAudio: Input converter is active\n"));
    }

    /* Set the new output format description for the stream. */
    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                               1, &pStreamIn->deviceFormat, sizeof(pStreamIn->deviceFormat));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set output format for input stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * Also set the frame buffer size off the device on our AudioUnit. This
     * should make sure that the frames count which we receive in the render
     * thread is as we like.
     */
    err = AudioUnitSetProperty(pStreamIn->audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                               1, &cFrames, sizeof(cFrames));
    if (err != noErr)    {
        LogRel(("CoreAudio: Failed to set maximum frame buffer size for input stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Finally initialize the new AudioUnit. */
    err = AudioUnitInitialize(pStreamIn->audioUnit);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to initialize audio unit for input stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    uSize = sizeof(pStreamIn->deviceFormat);
    err = AudioUnitGetProperty(pStreamIn->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                               1, &pStreamIn->deviceFormat, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to get input device format (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
     * the frame buffer size set in the previous calls. So finally get the
     * frame buffer size after the AudioUnit was initialized.
     */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(pStreamIn->audioUnit, kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,
                               0, &cFrames, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to get maximum frame buffer size from input audio device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Calculate the ratio between the device and the stream sample rate. */
    pStreamIn->sampleRatio = pStreamIn->streamFormat.mSampleRate / pStreamIn->deviceFormat.mSampleRate;

    /* Set to zero first */
    pStreamIn->pBuf = NULL;
    /* Create the AudioBufferList structure with one buffer. */
    pStreamIn->bufferList.mNumberBuffers = 1;
    /* Initialize the buffer to nothing. */
    pStreamIn->bufferList.mBuffers[0].mNumberChannels = pStreamIn->streamFormat.mChannelsPerFrame;
    pStreamIn->bufferList.mBuffers[0].mDataByteSize = 0;
    pStreamIn->bufferList.mBuffers[0].mData = NULL;

    /* Make sure that the ring buffer is big enough to hold the recording
     * data. Compare the maximum frames per slice value with the frames
     * necessary when using the converter where the sample rate could differ.
     * The result is always multiplied by the channels per frame to get the
     * samples count. */
    cSamples = RT_MAX(cFrames,
                      (cFrames * pStreamIn->deviceFormat.mBytesPerFrame * pStreamIn->sampleRatio) / pStreamIn->streamFormat.mBytesPerFrame)
               * pStreamIn->streamFormat.mChannelsPerFrame;

    int rc = VINF_SUCCESS;
#if 0
    if (   pHstStrmIn->cSamples != 0
        && pHstStrmIn->cSamples != (int32_t)cSamples)
        LogRel(("CoreAudio: Warning! After recreation, the CoreAudio ring buffer doesn't has the same size as the device buffer (%RU32 vs. %RU32).\n", cSamples, (uint32_t)pHstStrmIn->cSamples));

    /* Create the internal ring buffer. */
    RTCircBufCreate(&pStreamIn->pBuf, cSamples << pHstStrmIn->Props.cShift);
    if (RT_VALID_PTR(pStreamIn->pBuf))
        rc = 0;
    else
        LogRel(("CoreAudio: Failed to create internal ring buffer\n"));
#endif

    if (RT_FAILURE(rc))
    {
        if (pStreamIn->pBuf)
            RTCircBufDestroy(pStreamIn->pBuf);

        AudioUnitUninitialize(pStreamIn->audioUnit);
        return rc;
    }

#ifdef DEBUG
    propAdr.mSelector = kAudioDeviceProcessorOverload;
    propAdr.mScope    = kAudioUnitScope_Global;
    err = AudioObjectAddPropertyListener(pStreamIn->deviceID, &propAdr,
                                         drvHostCoreAudioRecordingAudioDevicePropertyChanged, (void *)pStreamIn);
    if (RT_UNLIKELY(err != noErr))
        LogRel(("CoreAudio: Failed to add the processor overload listener for input stream (%RI32)\n", err));
#endif /* DEBUG */
    propAdr.mSelector = kAudioDevicePropertyNominalSampleRate;
    propAdr.mScope    = kAudioUnitScope_Global;
    err = AudioObjectAddPropertyListener(pStreamIn->deviceID, &propAdr,
                                         drvHostCoreAudioRecordingAudioDevicePropertyChanged, (void *)pStreamIn);
    /* Not fatal. */
    if (RT_UNLIKELY(err != noErr))
        LogRel(("CoreAudio: Failed to register sample rate changed listener for input stream (%RI32)\n", err));

    ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_INIT);

    if (pcSamples)
        *pcSamples = cSamples;

    LogFunc(("cSamples=%RU32, rc=%Rrc\n", cSamples, rc));
    return rc;
}

/** @todo Eventually split up this function, as this already is huge! */
static int drvHostCoreAudioInitOutput(PPDMAUDIOHSTSTRMOUT pHstStrmOut, uint32_t *pcSamples)
{
    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    ASMAtomicXchgU32(&pStreamOut->status, CA_STATUS_IN_INIT);

    OSStatus err = noErr;

    UInt32 uSize = 0;
    if (pStreamOut->deviceID == kAudioDeviceUnknown)
    {
        /* Fetch the default audio input device currently in use. */
        AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultOutputDevice,
                                               kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        uSize = sizeof(pStreamOut->deviceID);
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAdr, 0, NULL, &uSize, &pStreamOut->deviceID);
        if (err != noErr)
        {
            LogRel(("CoreAudio: Unable to determine default output device (%RI32)\n", err));
            return VERR_NOT_FOUND;
        }
    }

    /*
     * Try to get the name of the output device and log it. It's not fatal if it fails.
     */
    CFStringRef strTemp;

    AudioObjectPropertyAddress propAdr = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMaster };
    uSize = sizeof(CFStringRef);
    err = AudioObjectGetPropertyData(pStreamOut->deviceID, &propAdr, 0, NULL, &uSize, &strTemp);
    if (err == noErr)
    {
        char *pszDevName = NULL;
        err = drvHostCoreAudioCFStringToCString(strTemp, &pszDevName);
        if (err == noErr)
        {
            CFRelease(strTemp);

            /* Get the device' UUID. */
            propAdr.mSelector = kAudioDevicePropertyDeviceUID;
            err = AudioObjectGetPropertyData(pStreamOut->deviceID, &propAdr, 0, NULL, &uSize, &strTemp);
            if (err == noErr)
            {
                char *pszUID = NULL;
                err = drvHostCoreAudioCFStringToCString(strTemp, &pszUID);
                if (err == noErr)
                {
                    CFRelease(strTemp);
                    LogRel(("CoreAudio: Using output device: %s (UID: %s)\n", pszDevName, pszUID));

                    RTMemFree(pszUID);
                }
            }

            RTMemFree(pszDevName);
        }
    }
    else
        LogRel(("CoreAudio: Unable to determine output device name (%RI32)\n", err));

    /* Get the default frames buffer size, so that we can setup our internal buffers. */
    UInt32 cFrames;
    uSize = sizeof(cFrames);
    propAdr.mSelector = kAudioDevicePropertyBufferFrameSize;
    propAdr.mScope    = kAudioDevicePropertyScopeInput;
    err = AudioObjectGetPropertyData(pStreamOut->deviceID, &propAdr, 0, NULL, &uSize, &cFrames);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to determine frame buffer size of the audio output device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Set the frame buffer size and honor any minimum/maximum restrictions on the device. */
    err = drvHostCoreAudioSetFrameBufferSize(pStreamOut->deviceID, false /* fInput */, cFrames, &cFrames);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set frame buffer size for the audio output device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    ComponentDescription cd;
    RT_ZERO(cd);
    cd.componentType         = kAudioUnitType_Output;
    cd.componentSubType      = kAudioUnitSubType_HALOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;

    /* Try to find the default HAL output component. */
    Component cp = FindNextComponent(NULL, &cd);
    if (cp == 0)
    {
        LogRel(("CoreAudio: Failed to find HAL output component\n")); /** @todo Return error value? */
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Open the default HAL output component. */
    err = OpenAComponent(cp, &pStreamOut->audioUnit);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to open output component (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Switch the I/O mode for output to on. */
    UInt32 uFlag = 1;
    err = AudioUnitSetProperty(pStreamOut->audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                               0, &uFlag, sizeof(uFlag));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to disable I/O mode for output stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Set the default audio output device as the device for the new AudioUnit. */
    err = AudioUnitSetProperty(pStreamOut->audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
                               0, &pStreamOut->deviceID, sizeof(pStreamOut->deviceID));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set current device for output stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * CoreAudio will inform us on a second thread for new incoming audio data.
     * Therefor register a callback function which will process the new data.
     */
    AURenderCallbackStruct cb;
    RT_ZERO(cb);
    cb.inputProc       = drvHostCoreAudioPlaybackCallback; /* pvUser */
    cb.inputProcRefCon = pStreamOut;

    err = AudioUnitSetProperty(pStreamOut->audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                               0, &cb, sizeof(cb));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to register output callback (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Fetch the current stream format of the device. */
    uSize = sizeof(pStreamOut->deviceFormat);
    err = AudioUnitGetProperty(pStreamOut->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                               0, &pStreamOut->deviceFormat, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to get device format (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Create an AudioStreamBasicDescription based on our required audio settings. */
    drvHostCoreAudioPCMInfoToASBDesc(&pStreamOut->streamOut.Props, &pStreamOut->streamFormat);

#ifdef DEBUG
    drvHostCoreAudioPrintASBDesc("CoreAudio: Output device", &pStreamOut->deviceFormat);
    drvHostCoreAudioPrintASBDesc("CoreAudio: Output format", &pStreamOut->streamFormat);
#endif /* DEBUG */

    /* Set the new output format description for the stream. */
    err = AudioUnitSetProperty(pStreamOut->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                               0, &pStreamOut->streamFormat, sizeof(pStreamOut->streamFormat));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set stream format for output stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    uSize = sizeof(pStreamOut->deviceFormat);
    err = AudioUnitGetProperty(pStreamOut->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                               0, &pStreamOut->deviceFormat, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to retrieve device format for output stream (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * Also set the frame buffer size off the device on our AudioUnit. This
     * should make sure that the frames count which we receive in the render
     * thread is as we like.
     */
    err = AudioUnitSetProperty(pStreamOut->audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                               0, &cFrames, sizeof(cFrames));
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to set maximum frame buffer size for output AudioUnit (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /* Finally initialize the new AudioUnit. */
    err = AudioUnitInitialize(pStreamOut->audioUnit);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to initialize the output audio device (%RI32)\n", err));
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * There are buggy devices (e.g. my Bluetooth headset) which doesn't honor
     * the frame buffer size set in the previous calls. So finally get the
     * frame buffer size after the AudioUnit was initialized.
     */
    uSize = sizeof(cFrames);
    err = AudioUnitGetProperty(pStreamOut->audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                               0, &cFrames, &uSize);
    if (err != noErr)
    {
        LogRel(("CoreAudio: Failed to get maximum frame buffer size from output audio device (%RI32)\n", err));

        AudioUnitUninitialize(pStreamOut->audioUnit);
        return VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    /*
     * Make sure that the ring buffer is big enough to hold the recording
     * data. Compare the maximum frames per slice value with the frames
     * necessary when using the converter where the sample rate could differ.
     * The result is always multiplied by the channels per frame to get the
     * samples count.
     */
    UInt32 cSamples = cFrames * pStreamOut->streamFormat.mChannelsPerFrame;

    /* Create the internal ring buffer. */
    int rc = RTCircBufCreate(&pStreamOut->pBuf, cSamples << pHstStrmOut->Props.cShift);
    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG
        propAdr.mSelector = kAudioDeviceProcessorOverload;
        propAdr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(pStreamOut->deviceID, &propAdr,
                                             drvHostCoreAudioPlaybackAudioDevicePropertyChanged, (void *)pStreamOut);
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register processor overload listener for output stream (%RI32)\n", err));
#endif /* DEBUG */

        propAdr.mSelector = kAudioDevicePropertyNominalSampleRate;
        propAdr.mScope    = kAudioUnitScope_Global;
        err = AudioObjectAddPropertyListener(pStreamOut->deviceID, &propAdr,
                                             drvHostCoreAudioPlaybackAudioDevicePropertyChanged, (void *)pStreamOut);
        /* Not fatal. */
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register sample rate changed listener for output stream (%RI32)\n", err));

        /* Allocate temporary buffer. */
        pStreamOut->cbPCMBuf = _4K; /** @todo Make this configurable. */
        pStreamOut->pvPCMBuf = RTMemAlloc(pStreamOut->cbPCMBuf);
        if (!pStreamOut->pvPCMBuf)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        ASMAtomicXchgU32(&pStreamOut->status, CA_STATUS_INIT);

        if (pcSamples)
            *pcSamples = cSamples;
    }
    else
        AudioUnitUninitialize(pStreamOut->audioUnit);

    LogFunc(("cSamples=%RU32, rc=%Rrc\n", cSamples, rc));
    return rc;
}

static DECLCALLBACK(int) drvHostCoreAudioInit(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostCoreAudioCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   uint32_t *pcSamplesCaptured)
{
    PPDMDRVINS pDrvIns      = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pHstStrmIn;

    size_t csReads = 0;
    char *pcSrc;
    PPDMAUDIOSAMPLE psDst;

    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&pStreamIn->status) == CA_STATUS_REINIT)
        drvHostCoreAudioReinitInput(pInterface, &pStreamIn->streamIn);

    if (ASMAtomicReadU32(&pStreamIn->status) != CA_STATUS_INIT)
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    uint32_t cbWrittenTotal = 0;

    do
    {
        size_t cbBuf = audioMixBufSizeBytes(&pHstStrmIn->MixBuf);
        size_t cbToWrite = RT_MIN(cbBuf, RTCircBufFree(pStreamIn->pBuf));
        LogFlowFunc(("cbToWrite=%zu\n", cbToWrite));

        uint32_t cWritten, cbWritten;
        uint8_t *puBuf;
        size_t   cbToRead;

        while (cbToWrite)
        {
            /* Try to acquire the necessary block from the ring buffer. */
            RTCircBufAcquireReadBlock(pStreamIn->pBuf, cbToWrite, (void **)&puBuf, &cbToRead);
            if (!cbToRead)
            {
                RTCircBufReleaseReadBlock(pStreamIn->pBuf, cbToRead);
                break;
            }

            rc = audioMixBufWriteCirc(&pHstStrmIn->MixBuf, puBuf, cbToRead, &cWritten);
            if (RT_FAILURE(rc))
                break;

            cbWritten = AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cWritten);

            /* Release the read buffer, so it could be used for new data. */
            RTCircBufReleaseReadBlock(pStreamIn->pBuf, cbWritten);

            Assert(cbToWrite >= cbWritten);
            cbToWrite -= cbWritten;
            cbWrittenTotal += cbWritten;
        }
    }
    while (0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cWrittenTotal = AUDIOMIXBUF_B2S(&pHstStrmIn->MixBuf, cbWrittenTotal);
        if (cWrittenTotal)
            audioMixBufFinish(&pHstStrmIn->MixBuf, cWrittenTotal);

        LogFlowFunc(("cWrittenTotal=%RU32 (%RU32 bytes)\n", cWrittenTotal, cbWrittenTotal));

        if (pcSamplesCaptured)
            *pcSamplesCaptured = cWrittenTotal;
    }

    return rc;
}

/* Callback for getting notified when some of the properties of an audio device has changed. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioPlaybackAudioDevicePropertyChanged(AudioObjectID propertyID,
                                                                                 UInt32 nAddresses,
                                                                                 const AudioObjectPropertyAddress properties[],
                                                                                 void *pvUser)
{
    switch (propertyID)
    {
#ifdef DEBUG
        case kAudioDeviceProcessorOverload:
        {
            Log2(("CoreAudio: [Output] Processor overload detected!\n"));
            break;
        }
#endif /* DEBUG */
        default:
            break;
    }

    return noErr;
}

/* Callback to feed audio output buffer. */
static DECLCALLBACK(OSStatus) drvHostCoreAudioPlaybackCallback(void                       *pvUser,
                                                               AudioUnitRenderActionFlags *pActionFlags,
                                                               const AudioTimeStamp       *pAudioTS,
                                                               UInt32                      uBusID,
                                                               UInt32                      cFrames,
                                                               AudioBufferList            *pBufData)
{
    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pvUser;
    PPDMAUDIOHSTSTRMOUT pHstStrmOut = &pStreamOut->streamOut;

    if (ASMAtomicReadU32(&pStreamOut->status) != CA_STATUS_INIT)
    {
        pBufData->mBuffers[0].mDataByteSize = 0;
        return noErr;
    }

    /* How much space is used in the ring buffer? */
    size_t cbDataAvail = RT_MIN(RTCircBufUsed(pStreamOut->pBuf), pBufData->mBuffers[0].mDataByteSize);
    if (!cbDataAvail)
    {
        pBufData->mBuffers[0].mDataByteSize = 0;
        return noErr;
    }

    uint8_t *pbSrc = NULL;
    size_t cbRead = 0;
    size_t cbToRead;
    while (cbDataAvail)
    {
        /* Try to acquire the necessary block from the ring buffer. */
        RTCircBufAcquireReadBlock(pStreamOut->pBuf, cbDataAvail, (void **)&pbSrc, &cbToRead);

        /* Break if nothing is used anymore. */
        if (!cbToRead == 0)
            break;

        /* Copy the data from our ring buffer to the core audio buffer. */
        memcpy((uint8_t *)pBufData->mBuffers[0].mData + cbRead, pbSrc, cbToRead);

        /* Release the read buffer, so it could be used for new data. */
        RTCircBufReleaseReadBlock(pStreamOut->pBuf, cbToRead);

        /* Move offset. */
        cbRead += cbToRead;
    }

    /* Write the bytes to the core audio buffer which where really written. */
    pBufData->mBuffers[0].mDataByteSize = cbRead;

    LogFlowFunc(("CoreAudio: [Output] Read %zu / %zu bytes\n", cbRead, cbDataAvail));

    return noErr;
}

static DECLCALLBACK(int) drvHostCoreAudioPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                 uint32_t *pcSamplesPlayed)
{
    PPDMDRVINS pDrvIns      = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    /* Check if the audio device should be reinitialized. If so do it. */
    if (ASMAtomicReadU32(&pStreamOut->status) == CA_STATUS_REINIT)
        drvHostCoreAudioReinitOutput(pInterface, &pStreamOut->streamOut);

    /* Not much else to do here. */

    uint32_t cLive = drvAudioHstOutSamplesLive(pHstStrmOut, NULL /* pcStreamsLive */);
    if (!cLive) /* Not samples to play? Bail out. */
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;

    do
    {
        size_t cbBuf = RT_MIN(audioMixBufSizeBytes(&pHstStrmOut->MixBuf), pStreamOut->cbPCMBuf);
        size_t cbToRead = RT_MIN(cbBuf, RTCircBufFree(pStreamOut->pBuf));
        LogFlowFunc(("cbToRead=%zu\n", cbToRead));

        uint32_t cRead, cbRead;
        uint8_t *puBuf;
        size_t   cbToWrite;

        while (cbToRead)
        {
            rc = audioMixBufReadCirc(&pHstStrmOut->MixBuf,
                                     pStreamOut->pvPCMBuf, cbToRead, &cRead);
            if (   RT_FAILURE(rc)
                || !cRead)
                break;

            cbRead = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cRead);

            /* Try to acquire the necessary space from the ring buffer. */
            RTCircBufAcquireWriteBlock(pStreamOut->pBuf, cbRead, (void **)&puBuf, &cbToWrite);
            if (!cbToWrite)
            {
                RTCircBufReleaseWriteBlock(pStreamOut->pBuf, cbToWrite);
                break;
            }

            /* Transfer data into stream's own ring buffer. The playback will operate on this
             * own ring buffer separately. */
            Assert(cbToWrite <= cbRead);
            memcpy(puBuf, pStreamOut->pvPCMBuf, cbToWrite);

            /* Release the ring buffer, so the read thread could start reading this data. */
            RTCircBufReleaseWriteBlock(pStreamOut->pBuf, cbToWrite);

            Assert(cbToRead >= cRead);
            cbToRead -= cbRead;
            cbReadTotal += cbRead;
        }
    }
    while (0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cbReadTotal);
        if (cReadTotal)
            audioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes)\n", cReadTotal, cbReadTotal));

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostCoreAudioControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                    PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    uint32_t uStatus = ASMAtomicReadU32(&pStreamOut->status);
    if (!(   uStatus == CA_STATUS_INIT
          || uStatus == CA_STATUS_REINIT))
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    OSStatus err;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            /* Only start the device if it is actually stopped */
            if (!drvHostCoreAudioIsRunning(pStreamOut->deviceID))
            {
                err = AudioUnitReset(pStreamOut->audioUnit, kAudioUnitScope_Input, 0);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to reset AudioUnit (%RI32)\n", err));
                    /* Keep going. */
                }
                RTCircBufReset(pStreamOut->pBuf);

                err = AudioOutputUnitStart(pStreamOut->audioUnit);
                if (RT_UNLIKELY(err != noErr))
                {
                    LogRel(("CoreAudio: Failed to start playback (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                }
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            /* Only stop the device if it is actually running */
            if (drvHostCoreAudioIsRunning(pStreamOut->deviceID))
            {
                err = AudioOutputUnitStop(pStreamOut->audioUnit);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to stop playback (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
                }

                err = AudioUnitReset(pStreamOut->audioUnit, kAudioUnitScope_Input, 0);
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

static DECLCALLBACK(int) drvHostCoreAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pHstStrmIn;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    uint32_t uStatus = ASMAtomicReadU32(&pStreamIn->status);
    if (!(   uStatus == CA_STATUS_INIT
          || uStatus == CA_STATUS_REINIT))
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    OSStatus err;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            /* Only start the device if it is actually stopped */
            if (!drvHostCoreAudioIsRunning(pStreamIn->deviceID))
            {
                RTCircBufReset(pStreamIn->pBuf);
                err = AudioOutputUnitStart(pStreamIn->audioUnit);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to start recording (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
                }
            }

            if (err != noErr)
            {
                LogRel(("CoreAudio: Failed to start recording (%RI32)\n", err));
                rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            /* Only stop the device if it is actually running */
            if (drvHostCoreAudioIsRunning(pStreamIn->deviceID))
            {
                err = AudioOutputUnitStop(pStreamIn->audioUnit);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to stop recording (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
                }

                err = AudioUnitReset(pStreamIn->audioUnit, kAudioUnitScope_Input, 0);
                if (err != noErr)
                {
                    LogRel(("CoreAudio: Failed to reset AudioUnit (%RI32)\n", err));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
                    break;
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

static DECLCALLBACK(int) drvHostCoreAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN) pHstStrmIn;

    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    LogFlowFuncEnter();

    uint32_t status = ASMAtomicReadU32(&pStreamIn->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
    {
        return VINF_SUCCESS;
    }

    OSStatus err = noErr;

    int rc = drvHostCoreAudioControlIn(pInterface, &pStreamIn->streamIn, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
    {
        ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_IN_UNINIT);
        AudioObjectPropertyAddress propAdr = { kAudioDeviceProcessorOverload, kAudioUnitScope_Global,
                                               kAudioObjectPropertyElementMaster };
#ifdef DEBUG
        err = AudioObjectRemovePropertyListener(pStreamIn->deviceID, &propAdr,
                                                drvHostCoreAudioRecordingAudioDevicePropertyChanged, NULL);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: Failed to remove the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */
        propAdr.mSelector = kAudioDevicePropertyNominalSampleRate;
        err = AudioObjectRemovePropertyListener(pStreamIn->deviceID, &propAdr,
                                                drvHostCoreAudioRecordingAudioDevicePropertyChanged, NULL);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: Failed to remove the sample rate changed listener (%RI32)\n", err));

        if (pStreamIn->converter)
        {
            AudioConverterDispose(pStreamIn->converter);
            pStreamIn->converter = NULL;
        }
        err = AudioUnitUninitialize(pStreamIn->audioUnit);
        if (RT_LIKELY(err == noErr))
        {
            err = CloseComponent(pStreamIn->audioUnit);
            if (RT_LIKELY(err == noErr))
            {
                RTCircBufDestroy(pStreamIn->pBuf);
                pStreamIn->audioUnit   = NULL;
                pStreamIn->deviceID    = kAudioDeviceUnknown;
                pStreamIn->pBuf        = NULL;
                pStreamIn->sampleRatio = 1;
                pStreamIn->rpos        = 0;
                ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_UNINIT);
            }
            else
            {
                LogRel(("CoreAudio: Failed to close the AudioUnit (%RI32)\n", err));
                rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
            }
        }
        else
        {
            LogRel(("CoreAudio: Failed to uninitialize the AudioUnit (%RI32)\n", err));
            rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
        }
    }
    else
    {
        LogRel(("CoreAudio: Failed to stop recording (%RI32)\n", err));
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostCoreAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PPDMDRVINS pDrvIns      = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);

    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    LogFlowFuncEnter();

    uint32_t status = ASMAtomicReadU32(&pStreamOut->status);
    if (!(   status == CA_STATUS_INIT
          || status == CA_STATUS_REINIT))
    {
        return VINF_SUCCESS;
    }

    int rc = drvHostCoreAudioControlOut(pInterface, &pStreamOut->streamOut, PDMAUDIOSTREAMCMD_DISABLE);
    if (RT_SUCCESS(rc))
    {
        ASMAtomicXchgU32(&pStreamOut->status, CA_STATUS_IN_UNINIT);
#if 0
        err = AudioDeviceRemovePropertyListener(pStreamOut->audioDeviceId, 0, false,
                                                kAudioDeviceProcessorOverload, drvHostCoreAudioPlaybackAudioDevicePropertyChanged);
        /* Not Fatal */
        if (RT_UNLIKELY(err != noErr))
            LogRel(("CoreAudio: Failed to remove the processor overload listener (%RI32)\n", err));
#endif /* DEBUG */
        OSStatus err = AudioUnitUninitialize(pStreamOut->audioUnit);
        if (err == noErr)
        {
            err = CloseComponent(pStreamOut->audioUnit);
            if (err == noErr)
            {
                RTCircBufDestroy(pStreamOut->pBuf);
                pStreamOut->pBuf      = NULL;

                pStreamOut->audioUnit = NULL;
                pStreamOut->deviceID  = kAudioDeviceUnknown;

                if (pStreamOut->pvPCMBuf)
                {
                    RTMemFree(pStreamOut->pvPCMBuf);
                    pStreamOut->pvPCMBuf = NULL;
                    pStreamOut->cbPCMBuf = 0;
                }

                ASMAtomicXchgU32(&pStreamOut->status, CA_STATUS_UNINIT);
            }
            else
                LogRel(("CoreAudio: Failed to close the AudioUnit (%RI32)\n", err));
        }
        else
            LogRel(("CoreAudio: Failed to uninitialize the AudioUnit (%RI32)\n", err));
    }
    else
        LogRel(("CoreAudio: Failed to stop playback, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostCoreAudioInitIn(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                                PDMAUDIORECSOURCE enmRecSource,
                                                uint32_t *pcSamples)
{
    PCOREAUDIOSTREAMIN pStreamIn = (PCOREAUDIOSTREAMIN)pHstStrmIn;

    LogFlowFunc(("enmRecSource=%ld\n"));

    ASMAtomicXchgU32(&pStreamIn->status, CA_STATUS_UNINIT);

    pStreamIn->audioUnit   = NULL;
    pStreamIn->deviceID    = kAudioDeviceUnknown;
    pStreamIn->converter   = NULL;
    pStreamIn->sampleRatio = 1;
    pStreamIn->rpos        = 0;

    bool fDeviceByUser = false;

    /* Initialize the hardware info section with the audio settings */
    int rc = drvAudioStreamCfgToProps(pCfg, &pStreamIn->streamIn.Props);
    if (RT_SUCCESS(rc))
    {
#if 0
        /* Try to find the audio device set by the user */
        if (DeviceUID.pszInputDeviceUID)
        {
            pStreamIn->deviceID = drvHostCoreAudioDeviceUIDtoID(DeviceUID.pszInputDeviceUID);
            /* Not fatal */
            if (pStreamIn->deviceID == kAudioDeviceUnknown)
                LogRel(("CoreAudio: Unable to find input device %s. Falling back to the default audio device. \n", DeviceUID.pszInputDeviceUID));
            else
                fDeviceByUser = true;
        }
#endif
        rc = drvHostCoreAudioInitInput(&pStreamIn->streamIn, pcSamples);
    }

    if (RT_SUCCESS(rc))
    {
        /* When the devices isn't forced by the user, we want default device change notifications. */
        if (!fDeviceByUser)
        {
            AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
                                                   kAudioObjectPropertyElementMaster };
            OSStatus err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propAdr,
                                                          drvHostCoreAudioDefaultDeviceChanged, (void *)pStreamIn);
            /* Not fatal. */
            if (err != noErr)
                LogRel(("CoreAudio: Failed to register the default input device changed listener (%RI32)\n", err));
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostCoreAudioInitOut(PPDMIHOSTAUDIO pInterface,
                                                 PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                                 uint32_t *pcSamples)
{
    PCOREAUDIOSTREAMOUT pStreamOut = (PCOREAUDIOSTREAMOUT)pHstStrmOut;

    OSStatus err = noErr;
    int rc = 0;
    bool fDeviceByUser = false; /* use we a device which was set by the user? */

    LogFlowFuncEnter();

    ASMAtomicXchgU32(&pStreamOut->status, CA_STATUS_UNINIT);

    pStreamOut->audioUnit = NULL;
    pStreamOut->deviceID  = kAudioDeviceUnknown;

    /* Initialize the hardware info section with the audio settings */
    drvAudioStreamCfgToProps(pCfg, &pStreamOut->streamOut.Props);

#if 0
    /* Try to find the audio device set by the user. Use
     * export VBOX_COREAUDIO_OUTPUT_DEVICE_UID=AppleHDAEngineOutput:0
     * to set it. */
    if (DeviceUID.pszOutputDeviceUID)
    {
        pStreamOut->audioDeviceId = drvHostCoreAudioDeviceUIDtoID(DeviceUID.pszOutputDeviceUID);
        /* Not fatal */
        if (pStreamOut->audioDeviceId == kAudioDeviceUnknown)
            LogRel(("CoreAudio: Unable to find output device %s. Falling back to the default audio device. \n", DeviceUID.pszOutputDeviceUID));
        else
            fDeviceByUser = true;
    }
#endif

    rc = drvHostCoreAudioInitOutput(pHstStrmOut, pcSamples);
    if (RT_FAILURE(rc))
        return rc;

    /* When the devices isn't forced by the user, we want default device change notifications. */
    if (!fDeviceByUser)
    {
        AudioObjectPropertyAddress propAdr = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMaster };
        err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propAdr,
                                             drvHostCoreAudioDefaultDeviceChanged, (void *)pStreamOut);
        /* Not fatal. */
        if (err != noErr)
            LogRel(("CoreAudio: Failed to register default device changed listener (%RI32)\n", err));
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(bool) drvHostCoreAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostCoreAudioGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pAudioConf)
{
    pAudioConf->cbStreamOut     = sizeof(COREAUDIOSTREAMOUT);
    pAudioConf->cbStreamIn      = sizeof(COREAUDIOSTREAMIN);
    pAudioConf->cMaxHstStrmsOut = 1;
    pAudioConf->cMaxHstStrmsIn  = 2;

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvHostCoreAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

static DECLCALLBACK(void *) drvHostCoreAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTCOREAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTCOREAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}

 /* Construct a DirectSound Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostCoreAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
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

    return VINF_SUCCESS;
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
    NULL,
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

