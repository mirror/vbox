/* $Id$ */
/** @file
 * Video recording code header.
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

#ifndef ____H_VIDEOREC
#define ____H_VIDEOREC

#include <VBox/com/array.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/err.h>
#include <VBox/settings.h>

using namespace com;

#include "VideoRecInternals.h"
#include "VideoRecStream.h"

class Console;

/**
 * Class for managing a capturing context.
 */
class CaptureContext
{
public:

    CaptureContext(Console *pConsole);

    CaptureContext(Console *pConsole, const settings::CaptureSettings &a_Settings);

    virtual ~CaptureContext(void);

public:

    const settings::CaptureSettings &GetConfig(void) const;
    CaptureStream *GetStream(unsigned uScreen) const;
    size_t GetStreamCount(void) const;

    int Create(const settings::CaptureSettings &a_Settings);
    int Destroy(void);

    int Start(void);
    int Stop(void);

    int SendAudioFrame(const void *pvData, size_t cbData, uint64_t uTimestampMs);
    int SendVideoFrame(uint32_t uScreen,
                       uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP,
                       uint32_t uBytesPerLine, uint32_t uSrcWidth, uint32_t uSrcHeight,
                       uint8_t *puSrcData, uint64_t uTimeStampMs);
public:

    bool IsFeatureEnabled(CaptureFeature_T enmFeature) const;
    bool IsReady(void) const;
    bool IsReady(uint32_t uScreen, uint64_t uTimeStampMs) const;
    bool IsStarted(void) const;
    bool IsLimitReached(uint32_t uScreen, uint64_t tsNowMs) const;

protected:

    int createInternal(const settings::CaptureSettings &a_Settings);
    int startInternal(void);
    int stopInternal(void);

    int destroyInternal(void);

    CaptureStream *getStreamInternal(unsigned uScreen) const;

    static DECLCALLBACK(int) threadMain(RTTHREAD hThreadSelf, void *pvUser);

    int threadNotify(void);

protected:

    /**
     * Enumeration for a recording context state.
     */
    enum VIDEORECSTS
    {
        /** Context not initialized. */
        VIDEORECSTS_UNINITIALIZED = 0,
        /** Context was created. */
        VIDEORECSTS_CREATED       = 1,
        /** Context was started. */
        VIDEORECSTS_STARTED       = 2,
        /** The usual 32-bit hack. */
        VIDEORECSTS_32BIT_HACK    = 0x7fffffff
    };

    /** Pointer to the console object. */
    Console                  *pConsole;
    /** Used recording configuration. */
    settings::CaptureSettings Settings;
    /** The current state. */
    VIDEORECSTS               enmState;
    /** Critical section to serialize access. */
    RTCRITSECT                CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT                WaitEvent;
    /** Shutdown indicator. */
    bool                      fShutdown;
    /** Worker thread. */
    RTTHREAD                  Thread;
    /** Vector of current recording streams.
     *  Per VM screen (display) one recording stream is being used. */
    VideoRecStreams           vecStreams;
    /** Timestamp (in ms) of when recording has been started. */
    uint64_t                  tsStartMs;
    /** Block map of common blocks which need to get multiplexed
     *  to all recording streams. This common block maps should help
     *  reducing the time spent in EMT and avoid doing the (expensive)
     *  multiplexing work in there.
     *
     *  For now this only affects audio, e.g. all recording streams
     *  need to have the same audio data at a specific point in time. */
    VideoRecBlockMap          mapBlocksCommon;
};
#endif /* !____H_VIDEOREC */

