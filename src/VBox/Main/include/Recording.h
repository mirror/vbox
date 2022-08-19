/* $Id$ */
/** @file
 * Recording code header.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_Recording_h
#define MAIN_INCLUDED_Recording_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>
#include <VBox/settings.h>

#include "RecordingStream.h"

class Console;

/**
 * Class for managing a recording context.
 */
class RecordingContext
{
public:

    RecordingContext();

    RecordingContext(Console *ptrConsole, const settings::RecordingSettings &Settings);

    virtual ~RecordingContext(void);

public:

    const settings::RecordingSettings &GetConfig(void) const;
    RecordingStream *GetStream(unsigned uScreen) const;
    size_t GetStreamCount(void) const;
#ifdef VBOX_WITH_AUDIO_RECORDING
    PRECORDINGCODEC GetCodecAudio(void) { return &this->CodecAudio; }
#endif

    int Create(Console *pConsole, const settings::RecordingSettings &Settings);
    void Destroy(void);

    int Start(void);
    int Stop(void);

    int SendAudioFrame(const void *pvData, size_t cbData, uint64_t uTimestampMs);
    int SendVideoFrame(uint32_t uScreen,
                       uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP,
                       uint32_t uBytesPerLine, uint32_t uSrcWidth, uint32_t uSrcHeight,
                       uint8_t *puSrcData, uint64_t msTimestamp);
public:

    bool IsFeatureEnabled(RecordingFeature_T enmFeature);
    bool IsReady(void);
    bool IsReady(uint32_t uScreen, uint64_t msTimestamp);
    bool IsStarted(void);
    bool IsLimitReached(void);
    bool IsLimitReached(uint32_t uScreen, uint64_t msTimestamp);
    bool NeedsUpdate( uint32_t uScreen, uint64_t msTimestamp);

    DECLCALLBACK(int) OnLimitReached(uint32_t uScreen, int rc);

protected:

    int createInternal(Console *ptrConsole, const settings::RecordingSettings &Settings);
    int startInternal(void);
    int stopInternal(void);

    void destroyInternal(void);

    RecordingStream *getStreamInternal(unsigned uScreen) const;

    int processCommonData(RecordingBlockMap &mapCommon, RTMSINTERVAL msTimeout);
    int writeCommonData(RecordingBlockMap &mapCommon, PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags);

    int lock(void);
    int unlock(void);

    static DECLCALLBACK(int) threadMain(RTTHREAD hThreadSelf, void *pvUser);

    int threadNotify(void);

protected:

    int audioInit(const settings::RecordingScreenSettings &screenSettings);

    static DECLCALLBACK(int) audioCodecWriteDataCallback(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags, void *pvUser);

protected:

    /**
     * Enumeration for a recording context state.
     */
    enum RECORDINGSTS
    {
        /** Context not initialized. */
        RECORDINGSTS_UNINITIALIZED = 0,
        /** Context was created. */
        RECORDINGSTS_CREATED       = 1,
        /** Context was started. */
        RECORDINGSTS_STARTED       = 2,
        /** The usual 32-bit hack. */
        RECORDINGSTS_32BIT_HACK    = 0x7fffffff
    };

    /** Pointer to the console object. */
    Console                     *pConsole;
    /** Used recording configuration. */
    settings::RecordingSettings  m_Settings;
    /** The current state. */
    RECORDINGSTS                 enmState;
    /** Critical section to serialize access. */
    RTCRITSECT                   CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT                   WaitEvent;
    /** Shutdown indicator. */
    bool                         fShutdown;
    /** Encoding worker thread. */
    RTTHREAD                     Thread;
    /** Vector of current recording streams.
     *  Per VM screen (display) one recording stream is being used. */
    RecordingStreams             vecStreams;
    /** Number of streams in vecStreams which currently are enabled for recording. */
    uint16_t                     cStreamsEnabled;
    /** Timestamp (in ms) of when recording has been started. */
    uint64_t                     tsStartMs;
#ifdef VBOX_WITH_AUDIO_RECORDING
    /** Audio codec to use.
     *
     *  We multiplex audio data from this recording context to all streams,
     *  to avoid encoding the same audio data for each stream. We ASSUME that
     *  all audio data of a VM will be the same for each stream at a given
     *  point in time. */
    RECORDINGCODEC               CodecAudio;
#endif /* VBOX_WITH_AUDIO_RECORDING */
    /** Block map of raw common data blocks which need to get encoded first. */
    RecordingBlockMap            mapBlocksRaw;
    /** Block map of encoded common blocks.
     *
     *  Only do the encoding of common data blocks only once and then multiplex
     *  the encoded data to all affected recording streams.
     *
     *  This avoids doing the (expensive) encoding + multiplexing work in other
     *  threads like EMT / audio async I/O..
     *
     *  For now this only affects audio, e.g. all recording streams
     *  need to have the same audio data at a specific point in time. */
    RecordingBlockMap            mapBlocksEncoded;
};
#endif /* !MAIN_INCLUDED_Recording_h */

