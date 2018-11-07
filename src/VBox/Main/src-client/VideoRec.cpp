/* $Id$ */
/** @file
 * Video recording (with optional audio recording) code.
 *
 * This code employs a separate encoding thread per recording context
 * to keep time spent in EMT as short as possible. Each configured VM display
 * is represented by an own recording stream, which in turn has its own rendering
 * queue. Common recording data across all recording streams is kept in a
 * separate queue in the recording context to minimize data duplication and
 * multiplexing overhead in EMT.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_MAIN_DISPLAY
#include "LoggingNew.h"

#include <stdexcept>
#include <vector>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/err.h>
#include <VBox/com/VirtualBox.h>

#include "ConsoleImpl.h"
#include "VideoRec.h"
#include "VideoRecInternals.h"
#include "VideoRecStream.h"
#include "VideoRecUtils.h"
#include "WebMWriter.h"

using namespace com;

#ifdef DEBUG_andy
/** Enables dumping audio / video data for debugging reasons. */
//# define VBOX_VIDEOREC_DUMP
#endif

/**
 * Enumeration for a recording state.
 */
enum VIDEORECSTS
{
    /** Not initialized. */
    VIDEORECSTS_UNINITIALIZED = 0,
    /** Created. */
    VIDEORECSTS_CREATED       = 1,
    /** Started. */
    VIDEORECSTS_STARTED       = 2,
    /** The usual 32-bit hack. */
    VIDEORECSTS_32BIT_HACK    = 0x7fffffff
};


#ifdef VBOX_VIDEOREC_DUMP
#pragma pack(push)
#pragma pack(1)
typedef struct
{
    uint16_t u16Magic;
    uint32_t u32Size;
    uint16_t u16Reserved1;
    uint16_t u16Reserved2;
    uint32_t u32OffBits;
} VIDEORECBMPHDR, *PVIDEORECBMPHDR;
AssertCompileSize(VIDEORECBMPHDR, 14);

typedef struct
{
    uint32_t u32Size;
    uint32_t u32Width;
    uint32_t u32Height;
    uint16_t u16Planes;
    uint16_t u16BitCount;
    uint32_t u32Compression;
    uint32_t u32SizeImage;
    uint32_t u32XPelsPerMeter;
    uint32_t u32YPelsPerMeter;
    uint32_t u32ClrUsed;
    uint32_t u32ClrImportant;
} VIDEORECBMPDIBHDR, *PVIDEORECBMPDIBHDR;
AssertCompileSize(VIDEORECBMPDIBHDR, 40);

#pragma pack(pop)
#endif /* VBOX_VIDEOREC_DUMP */


CaptureContext::CaptureContext(Console *a_pConsole)
    : pConsole(a_pConsole)
    , enmState(VIDEORECSTS_UNINITIALIZED) { }

CaptureContext::CaptureContext(Console *a_pConsole, const settings::CaptureSettings &a_Settings)
    : pConsole(a_pConsole)
    , enmState(VIDEORECSTS_UNINITIALIZED)
{
    int rc = CaptureContext::createInternal(a_Settings);
    if (RT_FAILURE(rc))
        throw rc;
}

CaptureContext::~CaptureContext(void)
{
    destroyInternal();
}

/**
 * Worker thread for all streams of a video recording context.
 *
 * For video frames, this also does the RGB/YUV conversion and encoding.
 */
DECLCALLBACK(int) CaptureContext::threadMain(RTTHREAD hThreadSelf, void *pvUser)
{
    CaptureContext *pThis = (CaptureContext *)pvUser;

    /* Signal that we're up and rockin'. */
    RTThreadUserSignal(hThreadSelf);

    LogFunc(("Thread started\n"));

    for (;;)
    {
        int rc = RTSemEventWait(pThis->WaitEvent, RT_INDEFINITE_WAIT);
        AssertRCBreak(rc);

        Log2Func(("Processing %zu streams\n", pThis->vecStreams.size()));

        /** @todo r=andy This is inefficient -- as we already wake up this thread
         *               for every screen from Main, we here go again (on every wake up) through
         *               all screens.  */
        VideoRecStreams::iterator itStream = pThis->vecStreams.begin();
        while (itStream != pThis->vecStreams.end())
        {
            CaptureStream *pStream = (*itStream);

            rc = pStream->Process(pThis->mapBlocksCommon);
            if (RT_FAILURE(rc))
                break;

            ++itStream;
        }

        if (RT_FAILURE(rc))
            LogRel(("VideoRec: Encoding thread failed with rc=%Rrc\n", rc));

        /* Keep going in case of errors. */

        if (ASMAtomicReadBool(&pThis->fShutdown))
        {
            LogFunc(("Thread is shutting down ...\n"));
            break;
        }

    } /* for */

    LogFunc(("Thread ended\n"));
    return VINF_SUCCESS;
}

/**
 * Notifies a recording context's encoding thread.
 *
 * @returns IPRT status code.
 */
int CaptureContext::threadNotify(void)
{
    return RTSemEventSignal(this->WaitEvent);
}

/**
 * Creates a video recording context.
 *
 * @returns IPRT status code.
 * @param   a_Settings          Capture settings to use for context creation.
 */
int CaptureContext::createInternal(const settings::CaptureSettings &a_Settings)
{
    int rc = RTCritSectInit(&this->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    settings::CaptureScreenMap::const_iterator itScreen = a_Settings.mapScreens.begin();
    while (itScreen != a_Settings.mapScreens.end())
    {
        CaptureStream *pStream = NULL;
        try
        {
            pStream = new CaptureStream(itScreen->first /* Screen ID */, itScreen->second);
            this->vecStreams.push_back(pStream);
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        this->tsStartMs = RTTimeMilliTS();
        this->enmState  = VIDEORECSTS_CREATED;
        this->fStarted  = false;
        this->fShutdown = false;

        /* Copy the settings to our context. */
        this->Settings  = a_Settings;

        rc = RTSemEventCreate(&this->WaitEvent);
        AssertRCReturn(rc, rc);
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = destroyInternal();
        AssertRC(rc2);
    }

    return rc;
}

int CaptureContext::startInternal(void)
{
    if (this->enmState == VIDEORECSTS_STARTED)
        return VINF_SUCCESS;

    Assert(this->enmState == VIDEORECSTS_CREATED);

    int rc = RTThreadCreate(&this->Thread, CaptureContext::threadMain, (void *)this, 0,
                            RTTHREADTYPE_MAIN_WORKER, RTTHREADFLAGS_WAITABLE, "VideoRec");

    if (RT_SUCCESS(rc)) /* Wait for the thread to start. */
        rc = RTThreadUserWait(this->Thread, 30 * RT_MS_1SEC /* 30s timeout */);

    if (RT_SUCCESS(rc))
    {
        this->enmState = VIDEORECSTS_STARTED;
        this->fStarted = true;
    }

    return rc;
}

int CaptureContext::stopInternal(void)
{
    if (this->enmState != VIDEORECSTS_STARTED)
        return VINF_SUCCESS;

    LogFunc(("Shutting down thread ...\n"));

    /* Set shutdown indicator. */
    ASMAtomicWriteBool(&this->fShutdown, true);

    /* Signal the thread and wait for it to shut down. */
    int rc = threadNotify();
    if (RT_SUCCESS(rc))
        rc = RTThreadWait(this->Thread, 30 * 1000 /* 10s timeout */, NULL);

    return rc;
}

/**
 * Destroys a video recording context.
 */
int CaptureContext::destroyInternal(void)
{
    int rc = VINF_SUCCESS;

    if (this->enmState == VIDEORECSTS_STARTED)
    {
        rc = stopInternal();
        if (RT_SUCCESS(rc))
        {
            /* Disable the context. */
            ASMAtomicWriteBool(&this->fStarted, false);

            int rc2 = RTSemEventDestroy(this->WaitEvent);
            AssertRC(rc2);

            this->WaitEvent = NIL_RTSEMEVENT;
        }
    }

    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

    rc = RTCritSectEnter(&this->CritSect);
    if (RT_SUCCESS(rc))
    {
        VideoRecStreams::iterator it = this->vecStreams.begin();
        while (it != this->vecStreams.end())
        {
            CaptureStream *pStream = (*it);

            int rc2 = pStream->Uninit();
            if (RT_SUCCESS(rc))
                rc = rc2;

            delete pStream;
            pStream = NULL;

            this->vecStreams.erase(it);
            it = this->vecStreams.begin();

            delete pStream;
            pStream = NULL;
        }

        /* Sanity. */
        Assert(this->vecStreams.empty());
        Assert(this->mapBlocksCommon.size() == 0);

        int rc2 = RTCritSectLeave(&this->CritSect);
        AssertRC(rc2);

        RTCritSectDelete(&this->CritSect);
    }

    return rc;
}

const settings::CaptureSettings &CaptureContext::GetConfig(void) const
{
    return this->Settings;
}

CaptureStream *CaptureContext::getStreamInternal(unsigned uScreen) const
{
    CaptureStream *pStream;

    try
    {
        pStream = this->vecStreams.at(uScreen);
    }
    catch (std::out_of_range &)
    {
        pStream = NULL;
    }

    return pStream;
}

/**
 * Retrieves a specific recording stream of a recording context.
 *
 * @returns Pointer to recording stream if found, or NULL if not found.
 * @param   uScreen             Screen number of recording stream to look up.
 */
CaptureStream *CaptureContext::GetStream(unsigned uScreen) const
{
    return getStreamInternal(uScreen);
}

size_t CaptureContext::GetStreamCount(void) const
{
    return this->vecStreams.size();
}

int CaptureContext::Create(const settings::CaptureSettings &a_Settings)
{
    return createInternal(a_Settings);
}

int CaptureContext::Destroy(void)
{
    return destroyInternal();
}

int CaptureContext::Start(void)
{
    return startInternal();
}

int CaptureContext::Stop(void)
{
    return stopInternal();
}

bool CaptureContext::IsFeatureEnabled(CaptureFeature_T enmFeature) const
{
    VideoRecStreams::const_iterator itStream = this->vecStreams.begin();
    while (itStream != this->vecStreams.end())
    {
        if ((*itStream)->GetConfig().isFeatureEnabled(enmFeature))
            return true;
        ++itStream;
    }

    return false;
}

bool CaptureContext::IsReady(void) const
{
    return this->fStarted;
}

/**
 * Checks if recording engine is ready to accept new recording data for a given screen.
 *
 * @returns true if recording engine is ready, false if not.
 * @param   uScreen             Screen ID.
 * @param   uTimeStampMs        Current time stamp (in ms). Currently not being used.
 */
bool CaptureContext::IsReady(uint32_t uScreen, uint64_t uTimeStampMs) const
{
    RT_NOREF(uTimeStampMs);

    if (this->enmState != VIDEORECSTS_STARTED)
        return false;

    bool fIsReady = false;

    const CaptureStream *pStream = GetStream(uScreen);
    if (pStream)
        fIsReady = pStream->IsReady();

    /* Note: Do not check for other constraints like the video FPS rate here,
     *       as this check then also would affect other (non-FPS related) stuff
     *       like audio data. */

    return fIsReady;
}

/**
 * Returns whether a given recording context has been started or not.
 *
 * @returns true if active, false if not.
 */
bool CaptureContext::IsStarted(void) const
{
    return this->fStarted;
}

/**
 * Checks if a specified limit for recording has been reached.
 *
 * @returns true if any limit has been reached.
 * @param   uScreen             Screen ID.
 * @param   tsNowMs             Current time stamp (in ms).
 */
bool CaptureContext::IsLimitReached(uint32_t uScreen, uint64_t tsNowMs) const
{
    const CaptureStream *pStream = GetStream(uScreen);
    if (   !pStream
        || pStream->IsLimitReached(tsNowMs))
    {
        return true;
    }

    return false;
}

/**
 * Sends an audio frame to the video encoding thread.
 *
 * @thread  EMT
 *
 * @returns IPRT status code.
 * @param   pvData              Audio frame data to send.
 * @param   cbData              Size (in bytes) of (encoded) audio frame data.
 * @param   uTimeStampMs        Time stamp (in ms) of audio playback.
 */
int CaptureContext::SendAudioFrame(const void *pvData, size_t cbData, uint64_t uTimeStampMs)
{
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    /* To save time spent in EMT, do the required audio multiplexing in the encoding thread.
     *
     * The multiplexing is needed to supply all recorded (enabled) screens with the same
     * audio data at the same given point in time.
     */
    PVIDEORECBLOCK pBlock = (PVIDEORECBLOCK)RTMemAlloc(sizeof(VIDEORECBLOCK));
    AssertPtrReturn(pBlock, VERR_NO_MEMORY);
    pBlock->enmType = VIDEORECBLOCKTYPE_AUDIO;

    PVIDEORECAUDIOFRAME pFrame = (PVIDEORECAUDIOFRAME)RTMemAlloc(sizeof(VIDEORECAUDIOFRAME));
    AssertPtrReturn(pFrame, VERR_NO_MEMORY);

    pFrame->pvBuf = (uint8_t *)RTMemAlloc(cbData);
    AssertPtrReturn(pFrame->pvBuf, VERR_NO_MEMORY);
    pFrame->cbBuf = cbData;

    memcpy(pFrame->pvBuf, pvData, cbData);

    pBlock->pvData       = pFrame;
    pBlock->cbData       = sizeof(VIDEORECAUDIOFRAME) + cbData;
    pBlock->cRefs        = (uint16_t)this->vecStreams.size(); /* All streams need the same audio data. */
    pBlock->uTimeStampMs = uTimeStampMs;

    int rc = RTCritSectEnter(&this->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    try
    {
        VideoRecBlockMap::iterator itBlocks = this->mapBlocksCommon.find(uTimeStampMs);
        if (itBlocks == this->mapBlocksCommon.end())
        {
            CaptureBlocks *pVideoRecBlocks = new CaptureBlocks();
            pVideoRecBlocks->List.push_back(pBlock);

            this->mapBlocksCommon.insert(std::make_pair(uTimeStampMs, pVideoRecBlocks));
        }
        else
            itBlocks->second->List.push_back(pBlock);
    }
    catch (const std::exception &ex)
    {
        RT_NOREF(ex);
        rc = VERR_NO_MEMORY;
    }

    int rc2 = RTCritSectLeave(&this->CritSect);
    AssertRC(rc2);

    if (RT_SUCCESS(rc))
        rc = threadNotify();

    return rc;
#else
    RT_NOREF(pCtx, pvData, cbData, uTimeStampMs);
    return VINF_SUCCESS;
#endif
}

/**
 * Copies a source video frame to the intermediate RGB buffer.
 * This function is executed only once per time.
 *
 * @thread  EMT
 *
 * @returns IPRT status code.
 * @param   uScreen            Screen number to send video frame to.
 * @param   x                  Starting x coordinate of the video frame.
 * @param   y                  Starting y coordinate of the video frame.
 * @param   uPixelFormat       Pixel format.
 * @param   uBPP               Bits Per Pixel (BPP).
 * @param   uBytesPerLine      Bytes per scanline.
 * @param   uSrcWidth          Width of the video frame.
 * @param   uSrcHeight         Height of the video frame.
 * @param   puSrcData          Pointer to video frame data.
 * @param   uTimeStampMs       Time stamp (in ms).
 */
int CaptureContext::SendVideoFrame(uint32_t uScreen, uint32_t x, uint32_t y,
                                   uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                                   uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData,
                                   uint64_t uTimeStampMs)
{
    AssertReturn(uSrcWidth,  VERR_INVALID_PARAMETER);
    AssertReturn(uSrcHeight, VERR_INVALID_PARAMETER);
    AssertReturn(puSrcData,  VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&this->CritSect);
    AssertRC(rc);

    CaptureStream *pStream = GetStream(uScreen);
    if (!pStream)
    {
        rc = RTCritSectLeave(&this->CritSect);
        AssertRC(rc);

        return VERR_NOT_FOUND;
    }

    rc = pStream->SendVideoFrame(x, y, uPixelFormat, uBPP, uBytesPerLine, uSrcWidth, uSrcHeight, puSrcData, uTimeStampMs);

    int rc2 = RTCritSectLeave(&this->CritSect);
    AssertRC(rc2);

    if (   RT_SUCCESS(rc)
        && rc != VINF_TRY_AGAIN) /* Only signal the thread if operation was successful. */
    {
        threadNotify();
    }

    return rc;
}

