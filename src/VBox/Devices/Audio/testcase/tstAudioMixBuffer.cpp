/* $Id$ */
/** @file
 * Audio testcase - Mixing buffer.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
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
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "../AudioMixBuffer.h"
#include "../AudioHlp.h"

#define _USE_MATH_DEFINES
#include <math.h> /* sin, M_PI */


static void tstBasics(RTTEST hTest)
{
    RTTestSub(hTest, "Basics");

    const PDMAUDIOPCMPROPS Cfg441StereoS16 = PDMAUDIOPCMPROPS_INITIALIZER(
        /* a_cb: */             2,
        /* a_fSigned: */        true,
        /* a_cChannels: */      2,
        /* a_uHz: */            44100,
        /* a_fSwapEndian: */    false
    );
    const PDMAUDIOPCMPROPS Cfg441StereoU16 = PDMAUDIOPCMPROPS_INITIALIZER(
        /* a_cb: */             2,
        /* a_fSigned: */        false,
        /* a_cChannels: */      2,
        /* a_uHz: */            44100,
        /* a_fSwapEndian: */    false
    );
    const PDMAUDIOPCMPROPS Cfg441StereoU32 = PDMAUDIOPCMPROPS_INITIALIZER(
        /* a_cb: */             4,
        /* a_fSigned: */        false,
        /* a_cChannels: */      2,
        /* a_uHz: */            44100,
        /* a_fSwapEndian: */    false
    );

    RTTESTI_CHECK(PDMAudioPropsGetBitrate(&Cfg441StereoS16) == 44100*4*8);
    RTTESTI_CHECK(PDMAudioPropsGetBitrate(&Cfg441StereoU16) == 44100*4*8);
    RTTESTI_CHECK(PDMAudioPropsGetBitrate(&Cfg441StereoU32) == 44100*8*8);

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&Cfg441StereoS16));
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&Cfg441StereoU16) == false); /* go figure */
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&Cfg441StereoU32) == false); /* go figure */


    RTTESTI_CHECK_MSG(PDMAUDIOPCMPROPS_F2B(&Cfg441StereoS16, 1) == 4,
                      ("got %x, expected 4\n", PDMAUDIOPCMPROPS_F2B(&Cfg441StereoS16, 1)));
    RTTESTI_CHECK_MSG(PDMAUDIOPCMPROPS_F2B(&Cfg441StereoU16, 1) == 4,
                      ("got %x, expected 4\n", PDMAUDIOPCMPROPS_F2B(&Cfg441StereoU16, 1)));
    RTTESTI_CHECK_MSG(PDMAUDIOPCMPROPS_F2B(&Cfg441StereoU32, 1) == 8,
                      ("got %x, expected 4\n", PDMAUDIOPCMPROPS_F2B(&Cfg441StereoU32, 1)));

    RTTESTI_CHECK_MSG(PDMAudioPropsBytesPerFrame(&Cfg441StereoS16) == 4,
                      ("got %x, expected 4\n", PDMAudioPropsBytesPerFrame(&Cfg441StereoS16)));
    RTTESTI_CHECK_MSG(PDMAudioPropsBytesPerFrame(&Cfg441StereoU16) == 4,
                      ("got %x, expected 4\n", PDMAudioPropsBytesPerFrame(&Cfg441StereoU16)));
    RTTESTI_CHECK_MSG(PDMAudioPropsBytesPerFrame(&Cfg441StereoU32) == 8,
                      ("got %x, expected 4\n", PDMAudioPropsBytesPerFrame(&Cfg441StereoU32)));

    uint32_t u32;
    for (uint32_t i = 0; i < 256; i += 8)
    {
        RTTESTI_CHECK(PDMAudioPropsIsSizeAligned(&Cfg441StereoU32, i) == true);
        for (uint32_t j = 1; j < 8; j++)
            RTTESTI_CHECK(PDMAudioPropsIsSizeAligned(&Cfg441StereoU32, i + j) == false);
        for (uint32_t j = 0; j < 8; j++)
            RTTESTI_CHECK(PDMAudioPropsFloorBytesToFrame(&Cfg441StereoU32, i + j) == i);
    }
    for (uint32_t i = 0; i < 4096; i += 4)
    {
        RTTESTI_CHECK(PDMAudioPropsIsSizeAligned(&Cfg441StereoS16, i) == true);
        for (uint32_t j = 1; j < 4; j++)
            RTTESTI_CHECK(PDMAudioPropsIsSizeAligned(&Cfg441StereoS16, i + j) == false);
        for (uint32_t j = 0; j < 4; j++)
            RTTESTI_CHECK(PDMAudioPropsFloorBytesToFrame(&Cfg441StereoS16, i + j) == i);
    }

    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsFramesToBytes(&Cfg441StereoS16, 44100)) == 44100 * 2 * 2,
                      ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsFramesToBytes(&Cfg441StereoS16, 2)) == 2 * 2 * 2,
                      ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsFramesToBytes(&Cfg441StereoS16, 1)) == 4,
                      ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsFramesToBytes(&Cfg441StereoU16, 1)) == 4,
                      ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsFramesToBytes(&Cfg441StereoU32, 1)) == 8,
                      ("cb=%RU32\n", u32));

    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsBytesToFrames(&Cfg441StereoS16, 4)) == 1, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsBytesToFrames(&Cfg441StereoU16, 4)) == 1, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsBytesToFrames(&Cfg441StereoU32, 8)) == 1, ("cb=%RU32\n", u32));

    uint64_t u64;
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsBytesToNano(&Cfg441StereoS16, 44100 * 2 * 2)) == RT_NS_1SEC,
                      ("ns=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsBytesToMicro(&Cfg441StereoS16, 44100 * 2 * 2)) == RT_US_1SEC,
                      ("us=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsBytesToMilli(&Cfg441StereoS16, 44100 * 2 * 2)) == RT_MS_1SEC,
                      ("ms=%RU64\n", u64));

    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToNano(&Cfg441StereoS16, 44100)) == RT_NS_1SEC, ("ns=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToNano(&Cfg441StereoS16,     1)) == 22675,      ("ns=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToNano(&Cfg441StereoS16,    31)) == 702947,     ("ns=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToNano(&Cfg441StereoS16,   255)) == 5782312,    ("ns=%RU64\n", u64));
    //RTTESTI_CHECK_MSG((u64 = DrvAudioHlpFramesToMicro(&Cfg441StereoS16, 44100)) == RT_US_1SEC,
    //                  ("us=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToMilli(&Cfg441StereoS16, 44100)) == RT_MS_1SEC, ("ms=%RU64\n", u64));
    RTTESTI_CHECK_MSG((u64 = PDMAudioPropsFramesToMilli(&Cfg441StereoS16,   255)) == 5,          ("ms=%RU64\n", u64));

    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsNanoToFrames(&Cfg441StereoS16,  RT_NS_1SEC)) == 44100, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsNanoToFrames(&Cfg441StereoS16,      215876)) == 10,    ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsMilliToFrames(&Cfg441StereoS16, RT_MS_1SEC)) == 44100, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsMilliToFrames(&Cfg441StereoU32,          6)) == 265,   ("cb=%RU32\n", u32));

    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsNanoToBytes(&Cfg441StereoS16,  RT_NS_1SEC)) == 44100*2*2, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsNanoToBytes(&Cfg441StereoS16,      702947)) == 31*2*2,    ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsMilliToBytes(&Cfg441StereoS16, RT_MS_1SEC)) == 44100*2*2, ("cb=%RU32\n", u32));
    RTTESTI_CHECK_MSG((u32 = PDMAudioPropsMilliToBytes(&Cfg441StereoS16,          5)) == 884,       ("cb=%RU32\n", u32));

    /* DrvAudioHlpClearBuf: */
    uint8_t *pbPage;
    int rc = RTTestGuardedAlloc(hTest, PAGE_SIZE, 0, false /*fHead*/, (void **)&pbPage);
    RTTESTI_CHECK_RC_OK_RETV(rc);

    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoS16, pbPage, PAGE_SIZE, PAGE_SIZE / 4);
    RTTESTI_CHECK(ASMMemIsZero(pbPage, PAGE_SIZE));

    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoU16, pbPage, PAGE_SIZE, PAGE_SIZE / 4);
    for (uint32_t off = 0; off < PAGE_SIZE; off += 2)
        RTTESTI_CHECK_MSG(pbPage[off] == 0x80 && pbPage[off + 1] == 0, ("off=%#x: %#x %x\n", off, pbPage[off], pbPage[off + 1]));

    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoU32, pbPage, PAGE_SIZE, PAGE_SIZE / 8);
    for (uint32_t off = 0; off < PAGE_SIZE; off += 4)
        RTTESTI_CHECK(pbPage[off] == 0x80 && pbPage[off + 1] == 0 && pbPage[off + 2] == 0 && pbPage[off + 3] == 0);


    RTTestDisableAssertions(hTest);
    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoS16, pbPage, PAGE_SIZE, PAGE_SIZE); /* should adjust down the frame count. */
    RTTESTI_CHECK(ASMMemIsZero(pbPage, PAGE_SIZE));

    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoU16, pbPage, PAGE_SIZE, PAGE_SIZE); /* should adjust down the frame count. */
    for (uint32_t off = 0; off < PAGE_SIZE; off += 2)
        RTTESTI_CHECK_MSG(pbPage[off] == 0x80 && pbPage[off + 1] == 0, ("off=%#x: %#x %x\n", off, pbPage[off], pbPage[off + 1]));

    memset(pbPage, 0x42, PAGE_SIZE);
    PDMAudioPropsClearBuffer(&Cfg441StereoU32, pbPage, PAGE_SIZE, PAGE_SIZE); /* should adjust down the frame count. */
    for (uint32_t off = 0; off < PAGE_SIZE; off += 4)
        RTTESTI_CHECK(pbPage[off] == 0x80 && pbPage[off + 1] == 0 && pbPage[off + 2] == 0 && pbPage[off + 3] == 0);
    RTTestRestoreAssertions(hTest);

    RTTestGuardedFree(hTest, pbPage);
}


static int tstSingle(RTTEST hTest)
{
    RTTestSub(hTest, "Single buffer");

    /* 44100Hz, 2 Channels, S16 */
    PDMAUDIOPCMPROPS config = PDMAUDIOPCMPROPS_INITIALIZER(
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        2,                                                                  /* Channels */
        44100,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&config));

    uint32_t cBufSize = _1K;

    /*
     * General stuff.
     */
    AUDIOMIXBUF mb;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&mb, "Single", &config, cBufSize));
    RTTESTI_CHECK(AudioMixBufSize(&mb) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_B2F(&mb, AudioMixBufSizeBytes(&mb)) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_F2B(&mb, AudioMixBufSize(&mb)) == AudioMixBufSizeBytes(&mb));
    RTTESTI_CHECK(AudioMixBufFree(&mb) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_F2B(&mb, AudioMixBufFree(&mb)) == AudioMixBufFreeBytes(&mb));

    /*
     * Absolute writes.
     */
    uint32_t cFramesRead  = 0, cFramesWritten = 0, cFramesWrittenAbs = 0;
    int8_t  aFrames8 [2] = { 0x12, 0x34 };
    int16_t aFrames16[2] = { 0xAA, 0xBB };
    int32_t aFrames32[2] = { 0xCC, 0xDD };

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteAt(&mb, 0 /* Offset */, &aFrames8, sizeof(aFrames8), &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == 0 /* Frames */);
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == 0);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteAt(&mb, 0 /* Offset */, &aFrames16, sizeof(aFrames16), &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == 1 /* Frames */);
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == 1);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteAt(&mb, 2 /* Offset */, &aFrames32, sizeof(aFrames32), &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == 2 /* Frames */);
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == 2);

    /* Beyond buffer. */
    RTTESTI_CHECK_RC(AudioMixBufWriteAt(&mb, AudioMixBufSize(&mb) + 1, &aFrames16, sizeof(aFrames16),
                                        &cFramesWritten), VERR_BUFFER_OVERFLOW);

    /* Offset wrap-around: When writing as much (or more) frames the mixing buffer can hold. */
    uint32_t  cbSamples = cBufSize * sizeof(int16_t) * 2 /* Channels */;
    RTTESTI_CHECK(cbSamples);
    uint16_t *paSamples = (uint16_t *)RTMemAlloc(cbSamples);
    RTTESTI_CHECK(paSamples);
    RTTESTI_CHECK_RC_OK(AudioMixBufWriteAt(&mb, 0 /* Offset */, paSamples, cbSamples, &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == cBufSize /* Frames */);
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == cBufSize);
    RTTESTI_CHECK(AudioMixBufReadPos(&mb) == 0);
    RTTESTI_CHECK(AudioMixBufWritePos(&mb) == 0);
    RTMemFree(paSamples);
    cbSamples = 0;

    /*
     * Circular writes.
     */
    AudioMixBufReset(&mb);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteAt(&mb, 2 /* Offset */, &aFrames32, sizeof(aFrames32), &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == 2 /* Frames */);
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == 2);

    cFramesWrittenAbs = AudioMixBufUsed(&mb);

    uint32_t cToWrite = AudioMixBufSize(&mb) - cFramesWrittenAbs - 1; /* -1 as padding plus -2 frames for above. */
    for (uint32_t i = 0; i < cToWrite; i++)
    {
        RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&mb, &aFrames16, sizeof(aFrames16), &cFramesWritten));
        RTTESTI_CHECK(cFramesWritten == 1);
    }
    RTTESTI_CHECK(!AudioMixBufIsEmpty(&mb));
    RTTESTI_CHECK(AudioMixBufFree(&mb) == 1);
    RTTESTI_CHECK(AudioMixBufFreeBytes(&mb) == AUDIOMIXBUF_F2B(&mb, 1U));
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == cToWrite + cFramesWrittenAbs /* + last absolute write */);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&mb, &aFrames16, sizeof(aFrames16), &cFramesWritten));
    RTTESTI_CHECK(cFramesWritten == 1);
    RTTESTI_CHECK(AudioMixBufFree(&mb) == 0);
    RTTESTI_CHECK(AudioMixBufFreeBytes(&mb) == AUDIOMIXBUF_F2B(&mb, 0U));
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == cBufSize);

    /* Circular reads. */
    uint32_t cToRead = AudioMixBufSize(&mb) - cFramesWrittenAbs - 1;
    for (uint32_t i = 0; i < cToRead; i++)
    {
        RTTESTI_CHECK_RC_OK(AudioMixBufAcquireReadBlock(&mb, &aFrames16, sizeof(aFrames16), &cFramesRead));
        RTTESTI_CHECK(cFramesRead == 1);
        AudioMixBufReleaseReadBlock(&mb, cFramesRead);
        AudioMixBufFinish(&mb, cFramesRead);
    }
    RTTESTI_CHECK(!AudioMixBufIsEmpty(&mb));
    RTTESTI_CHECK(AudioMixBufFree(&mb) == AudioMixBufSize(&mb) - cFramesWrittenAbs - 1);
    RTTESTI_CHECK(AudioMixBufFreeBytes(&mb) == AUDIOMIXBUF_F2B(&mb, cBufSize - cFramesWrittenAbs - 1));
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == cBufSize - cToRead);

    RTTESTI_CHECK_RC_OK(AudioMixBufAcquireReadBlock(&mb, &aFrames16, sizeof(aFrames16), &cFramesRead));
    RTTESTI_CHECK(cFramesRead == 1);
    AudioMixBufReleaseReadBlock(&mb, cFramesRead);
    AudioMixBufFinish(&mb, cFramesRead);
    RTTESTI_CHECK(AudioMixBufFree(&mb) == cBufSize - cFramesWrittenAbs);
    RTTESTI_CHECK(AudioMixBufFreeBytes(&mb) == AUDIOMIXBUF_F2B(&mb, cBufSize - cFramesWrittenAbs));
    RTTESTI_CHECK(AudioMixBufUsed(&mb) == cFramesWrittenAbs);

    AudioMixBufDestroy(&mb);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}

static int tstParentChild(RTTEST hTest)
{
    RTTestSub(hTest, "2 Children -> Parent (AudioMixBufWriteAt)");

    uint32_t cParentBufSize = RTRandU32Ex(_1K /* Min */, _16K /* Max */); /* Enough room for random sizes */

    /* 44100Hz, 2 Channels, S16 */
    PDMAUDIOPCMPROPS cfg_p = PDMAUDIOPCMPROPS_INITIALIZER(
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        2,                                                                  /* Channels */
        44100,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_p));

    AUDIOMIXBUF parent;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&parent, "Parent", &cfg_p, cParentBufSize));

    /* 22050Hz, 2 Channels, S16 */
    PDMAUDIOPCMPROPS cfg_c1 = PDMAUDIOPCMPROPS_INITIALIZER(/* Upmixing to parent */
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        2,                                                                  /* Channels */
        22050,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_c1));

    uint32_t cFrames      = 16;
    uint32_t cChildBufSize = RTRandU32Ex(cFrames /* Min */, 64 /* Max */);

    AUDIOMIXBUF child1;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&child1, "Child1", &cfg_c1, cChildBufSize));
    RTTESTI_CHECK_RC_OK(AudioMixBufLinkTo(&child1, &parent));

    /* 48000Hz, 2 Channels, S16 */
    PDMAUDIOPCMPROPS cfg_c2 = PDMAUDIOPCMPROPS_INITIALIZER(/* Downmixing to parent */
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        2,                                                                  /* Channels */
        48000,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_c2));

    AUDIOMIXBUF child2;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&child2, "Child2", &cfg_c2, cChildBufSize));
    RTTESTI_CHECK_RC_OK(AudioMixBufLinkTo(&child2, &parent));

    /*
     * Writing + mixing from child/children -> parent, sequential.
     */
    uint32_t cbBuf = _1K;
    char pvBuf[_1K];
    int16_t aFrames16[32] = { 0xAA, 0xBB };
    uint32_t cFramesRead, cFramesWritten, cFramesMixed;

    uint32_t cFramesChild1  = cFrames;
    uint32_t cFramesChild2  = cFrames;

    uint32_t t = RTRandU32() % 32;

    RTTestPrintf(hTest, RTTESTLVL_DEBUG,
                 "cParentBufSize=%RU32, cChildBufSize=%RU32, %RU32 frames -> %RU32 iterations total\n",
                 cParentBufSize, cChildBufSize, cFrames, t);

    /*
     * Using AudioMixBufWriteAt for writing to children.
     */
    uint32_t cChildrenSamplesMixedTotal = 0;

    for (uint32_t i = 0; i < t; i++)
    {
        RTTestPrintf(hTest, RTTESTLVL_DEBUG, "i=%RU32\n", i);

        uint32_t cChild1Writes = RTRandU32() % 8;

        for (uint32_t c1 = 0; c1 < cChild1Writes; c1++)
        {
            /* Child 1. */
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufWriteAt(&child1, 0, &aFrames16, sizeof(aFrames16), &cFramesWritten));
            RTTESTI_CHECK_MSG_BREAK(cFramesWritten == cFramesChild1, ("Child1: Expected %RU32 written frames, got %RU32\n", cFramesChild1, cFramesWritten));
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufMixToParent(&child1, cFramesWritten, &cFramesMixed));

            cChildrenSamplesMixedTotal += cFramesMixed;

            RTTESTI_CHECK_MSG_BREAK(cFramesWritten == cFramesMixed, ("Child1: Expected %RU32 mixed frames, got %RU32\n", cFramesWritten, cFramesMixed));
            RTTESTI_CHECK_MSG_BREAK(AudioMixBufUsed(&child1) == 0, ("Child1: Expected %RU32 used frames, got %RU32\n", 0, AudioMixBufUsed(&child1)));
        }

        uint32_t cChild2Writes = RTRandU32() % 8;

        for (uint32_t c2 = 0; c2 < cChild2Writes; c2++)
        {
            /* Child 2. */
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufWriteAt(&child2, 0, &aFrames16, sizeof(aFrames16), &cFramesWritten));
            RTTESTI_CHECK_MSG_BREAK(cFramesWritten == cFramesChild2, ("Child2: Expected %RU32 written frames, got %RU32\n", cFramesChild2, cFramesWritten));
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufMixToParent(&child2, cFramesWritten, &cFramesMixed));

            cChildrenSamplesMixedTotal += cFramesMixed;

            RTTESTI_CHECK_MSG_BREAK(cFramesWritten == cFramesMixed, ("Child2: Expected %RU32 mixed frames, got %RU32\n", cFramesWritten, cFramesMixed));
            RTTESTI_CHECK_MSG_BREAK(AudioMixBufUsed(&child2) == 0, ("Child2: Expected %RU32 used frames, got %RU32\n", 0, AudioMixBufUsed(&child2)));
        }

        /*
         * Read out all frames from the parent buffer and also mark the just-read frames as finished
         * so that both connected children buffers can keep track of their stuff.
         */
        uint32_t cParentSamples = AudioMixBufUsed(&parent);
        while (cParentSamples)
        {
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&parent, pvBuf, cbBuf, &cFramesRead));
            if (!cFramesRead)
                break;

            AudioMixBufReleaseReadBlock(&parent, cFramesRead);
            AudioMixBufFinish(&parent, cFramesRead);

            RTTESTI_CHECK(cParentSamples >= cFramesRead);
            cParentSamples -= cFramesRead;
        }

        RTTESTI_CHECK(cParentSamples == 0);
    }

    RTTESTI_CHECK(AudioMixBufUsed(&parent) == 0);
    RTTESTI_CHECK(AudioMixBufLive(&child1) == 0);
    RTTESTI_CHECK(AudioMixBufLive(&child2) == 0);

    AudioMixBufDestroy(&parent);
    AudioMixBufDestroy(&child1);
    AudioMixBufDestroy(&child2);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}


static void tstDownsampling(RTTEST hTest, uint32_t uFromHz, uint32_t uToHz)
{
    RTTestSubF(hTest, "Downsampling %u to %u Hz (S16)", uFromHz, uToHz);

    struct { int16_t l, r; }
        aSrcFrames[4096],
        aDstFrames[4096];

    /* Parent (destination) buffer is xxxHz 2ch S16 */
    uint32_t const         cFramesParent = RTRandU32Ex(16, RT_ELEMENTS(aDstFrames));
    PDMAUDIOPCMPROPS const CfgDst = PDMAUDIOPCMPROPS_INITIALIZER(2 /*cbSample*/, true /*fSigned*/, 2 /*ch*/, uToHz, false /*fSwap*/);
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&CfgDst));
    AUDIOMIXBUF Parent;
    RTTESTI_CHECK_RC_OK_RETV(AudioMixBufInit(&Parent, "ParentDownsampling", &CfgDst, cFramesParent));

    /* Child (source) buffer is yyykHz 2ch S16 */
    PDMAUDIOPCMPROPS const CfgSrc = PDMAUDIOPCMPROPS_INITIALIZER(2 /*cbSample*/, true /*fSigned*/, 2 /*ch*/, uFromHz, false /*fSwap*/);
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&CfgSrc));
    uint32_t const cFramesChild = RTRandU32Ex(32, RT_ELEMENTS(aSrcFrames));
    AUDIOMIXBUF Child;
    RTTESTI_CHECK_RC_OK_RETV(AudioMixBufInit(&Child, "ChildDownsampling", &CfgSrc, cFramesChild));
    RTTESTI_CHECK_RC_OK_RETV(AudioMixBufLinkTo(&Child, &Parent));

    /*
     * Test parameters.
     */
    uint32_t const cMaxSrcFrames = RT_MIN(cFramesParent * uFromHz / uToHz - 1, cFramesChild);
    uint32_t const cIterations   = RTRandU32Ex(4, 128);
    RTTestErrContext(hTest, "cFramesParent=%RU32 cFramesChild=%RU32 cMaxSrcFrames=%RU32 cIterations=%RU32",
                     cFramesParent, cFramesChild, cMaxSrcFrames, cIterations);
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "cFramesParent=%RU32 cFramesChild=%RU32 cMaxSrcFrames=%RU32 cIterations=%RU32\n",
                 cFramesParent, cFramesChild, cMaxSrcFrames, cIterations);

    /*
     * We generate a simple "A" sine wave as input.
     */
    uint32_t iSrcFrame = 0;
    uint32_t iDstFrame = 0;
    double   rdFixed = 2.0 * M_PI * 440.0 /* A */ / PDMAudioPropsHz(&CfgSrc); /* Fixed sin() input. */
    for (uint32_t i = 0; i < cIterations; i++)
    {
        RTTestPrintf(hTest, RTTESTLVL_DEBUG, "i=%RU32\n", i);

        /*
         * Generate source frames and write them.
         */
        uint32_t const cSrcFrames = i < cIterations / 2
                                  ? RTRandU32Ex(2, cMaxSrcFrames) & ~(uint32_t)1
                                  : RTRandU32Ex(1, cMaxSrcFrames - 1) | 1;
        for (uint32_t j = 0; j < cSrcFrames; j++, iSrcFrame++)
            aSrcFrames[j].r = aSrcFrames[j].l = 32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame);

        uint32_t cSrcFramesWritten = UINT32_MAX / 2;
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufWriteAt(&Child, 0, &aSrcFrames, cSrcFrames * sizeof(aSrcFrames[0]),
                                                     &cSrcFramesWritten));
        RTTESTI_CHECK_MSG_BREAK(cSrcFrames == cSrcFramesWritten,
                                ("cSrcFrames=%RU32 vs cSrcFramesWritten=%RU32\n", cSrcFrames, cSrcFramesWritten));

        /*
         * Mix them.
         */
        uint32_t cSrcFramesMixed = UINT32_MAX / 2;
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufMixToParent(&Child, cSrcFramesWritten, &cSrcFramesMixed));
        RTTESTI_CHECK_MSG(AudioMixBufUsed(&Child) == 0, ("%RU32\n", AudioMixBufUsed(&Child)));
        RTTESTI_CHECK_MSG_BREAK(cSrcFramesWritten == cSrcFramesMixed,
                                ("cSrcFramesWritten=%RU32 cSrcFramesMixed=%RU32\n", cSrcFramesWritten, cSrcFramesMixed));
        RTTESTI_CHECK_MSG_BREAK(AudioMixBufUsed(&Child) == 0, ("%RU32\n", AudioMixBufUsed(&Child)));

        /*
         * Read out the parent buffer.
         */
        uint32_t cDstFrames = AudioMixBufUsed(&Parent);
        while (cDstFrames > 0)
        {
            uint32_t cFramesRead = UINT32_MAX / 2;
            RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&Parent, aDstFrames, sizeof(aDstFrames), &cFramesRead));
            RTTESTI_CHECK_MSG(cFramesRead > 0 && cFramesRead <= cDstFrames,
                              ("cFramesRead=%RU32 cDstFrames=%RU32\n", cFramesRead, cDstFrames));

            AudioMixBufReleaseReadBlock(&Parent, cFramesRead);
            AudioMixBufFinish(&Parent, cFramesRead);

            iDstFrame  += cFramesRead;
            cDstFrames -= cFramesRead;
            RTTESTI_CHECK(AudioMixBufUsed(&Parent) == cDstFrames);
        }
    }

    RTTESTI_CHECK(AudioMixBufUsed(&Parent) == 0);
    RTTESTI_CHECK(AudioMixBufLive(&Child) == 0);
    uint32_t const cDstMinExpect =  (uint64_t)iSrcFrame * uToHz                / uFromHz;
    uint32_t const cDstMaxExpect = ((uint64_t)iSrcFrame * uToHz + uFromHz - 1) / uFromHz;
    RTTESTI_CHECK_MSG(iDstFrame == cDstMinExpect || iDstFrame == cDstMaxExpect,
                      ("iSrcFrame=%#x -> %#x,%#x; iDstFrame=%#x\n", iSrcFrame, cDstMinExpect, cDstMaxExpect, iDstFrame));

    AudioMixBufDestroy(&Parent);
    AudioMixBufDestroy(&Child);
}


static void tstNewPeek(RTTEST hTest, uint32_t uFromHz, uint32_t uToHz)
{
    RTTestSubF(hTest, "New peek %u to %u Hz (S16)", uFromHz, uToHz);

    struct { int16_t l, r; }
        aSrcFrames[4096],
        aDstFrames[4096];

    /* Mix buffer is uFromHz 2ch S16 */
    uint32_t const         cFrames = RTRandU32Ex(16, RT_ELEMENTS(aSrcFrames));
    PDMAUDIOPCMPROPS const CfgSrc  = PDMAUDIOPCMPROPS_INITIALIZER(2 /*cbSample*/, true /*fSigned*/, 2 /*ch*/, uFromHz, false /*fSwap*/);
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&CfgSrc));
    AUDIOMIXBUF MixBuf;
    RTTESTI_CHECK_RC_OK_RETV(AudioMixBufInit(&MixBuf, "NewPeekMixBuf", &CfgSrc, cFrames));

    /* Peek state (destination) is uToHz 2ch S16 */
    PDMAUDIOPCMPROPS const CfgDst = PDMAUDIOPCMPROPS_INITIALIZER(2 /*cbSample*/, true /*fSigned*/, 2 /*ch*/, uToHz, false /*fSwap*/);
    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&CfgDst));
    AUDIOMIXBUFPEEKSTATE PeekState;
    RTTESTI_CHECK_RC_OK_RETV(AudioMixBufInitPeekState(&MixBuf, &PeekState, &CfgDst));

    /*
     * Test parameters.
     */
    uint32_t const cMaxSrcFrames = RT_MIN(cFrames * uFromHz / uToHz - 1, cFrames);
    uint32_t const cIterations   = RTRandU32Ex(64, 1024);
    RTTestErrContext(hTest, "cFrames=%RU32 cMaxSrcFrames=%RU32 cIterations=%RU32", cFrames, cMaxSrcFrames, cIterations);
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "cFrames=%RU32 cMaxSrcFrames=%RU32 cIterations=%RU32\n",
                 cFrames, cMaxSrcFrames, cIterations);

    /*
     * We generate a simple "A" sine wave as input.
     */
    uint32_t iSrcFrame = 0;
    uint32_t iDstFrame = 0;
    double   rdFixed = 2.0 * M_PI * 440.0 /* A */ / PDMAudioPropsHz(&CfgSrc); /* Fixed sin() input. */
    for (uint32_t i = 0; i < cIterations; i++)
    {
        RTTestPrintf(hTest, RTTESTLVL_DEBUG, "i=%RU32\n", i);

        /*
         * Generate source frames and write them.
         */
        uint32_t const cSrcFrames = i < cIterations / 2
                                  ? RTRandU32Ex(2, cMaxSrcFrames) & ~(uint32_t)1
                                  : RTRandU32Ex(1, cMaxSrcFrames - 1) | 1;
        for (uint32_t j = 0; j < cSrcFrames; j++, iSrcFrame++)
            aSrcFrames[j].r = aSrcFrames[j].l = 32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame);

        uint32_t cSrcFramesWritten = UINT32_MAX / 2;
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufWriteCirc(&MixBuf, &aSrcFrames, cSrcFrames * sizeof(aSrcFrames[0]), &cSrcFramesWritten));
        RTTESTI_CHECK_MSG_BREAK(cSrcFrames == cSrcFramesWritten,
                                ("cSrcFrames=%RU32 vs cSrcFramesWritten=%RU32 cLiveFrames=%RU32\n",
                                 cSrcFrames, cSrcFramesWritten, AudioMixBufLive(&MixBuf)));

        /*
         * Read out all the frames using the peek function.
         */
        uint32_t offSrcFrame = 0;
        while (offSrcFrame < cSrcFramesWritten)
        {
            uint32_t cSrcFramesToRead = cSrcFramesWritten - offSrcFrame;
            uint32_t cTmp = (uint64_t)cSrcFramesToRead * uToHz / uFromHz;
            if (cTmp + 32 >= RT_ELEMENTS(aDstFrames))
                cSrcFramesToRead = ((uint64_t)RT_ELEMENTS(aDstFrames) - 32) * uFromHz / uToHz; /* kludge */

            uint32_t cSrcFramesPeeked = UINT32_MAX / 4;
            uint32_t cbDstPeeked      = UINT32_MAX / 2;
            RTRandBytes(aDstFrames, sizeof(aDstFrames));
            AudioMixBufPeek(&MixBuf, offSrcFrame, cSrcFramesToRead, &cSrcFramesPeeked,
                            &PeekState, aDstFrames, sizeof(aDstFrames), &cbDstPeeked);
            uint32_t cDstFramesPeeked = PDMAudioPropsBytesToFrames(&CfgDst, cbDstPeeked);
            RTTESTI_CHECK(cbDstPeeked > 0 || cSrcFramesPeeked > 0);

            if (uFromHz == uToHz)
            {
                for (uint32_t iDst = 0; iDst < cDstFramesPeeked; iDst++)
                    if (memcmp(&aDstFrames[iDst], &aSrcFrames[offSrcFrame + iDst], sizeof(aSrcFrames[0])) != 0)
                        RTTestFailed(hTest, "Frame #%u differs: %#x / %#x, expected %#x / %#x\n", iDstFrame + iDst,
                                     aDstFrames[iDst].l, aDstFrames[iDst].r,
                                     aSrcFrames[iDst + offSrcFrame].l, aSrcFrames[iDst + offSrcFrame].r);
            }

            offSrcFrame += cSrcFramesPeeked;
            iDstFrame   += cDstFramesPeeked;
        }

        /*
         * Then advance.
         */
        AudioMixBufAdvance(&MixBuf, cSrcFrames);
        RTTESTI_CHECK(AudioMixBufLive(&MixBuf) == 0);
    }

    /** @todo this is a bit lax...   */
    uint32_t const cDstMinExpect = ((uint64_t)iSrcFrame * uToHz - uFromHz - 1) / uFromHz;
    uint32_t const cDstMaxExpect = ((uint64_t)iSrcFrame * uToHz + uFromHz - 1) / uFromHz;
    RTTESTI_CHECK_MSG(iDstFrame >= cDstMinExpect && iDstFrame <= cDstMaxExpect,
                      ("iSrcFrame=%#x -> %#x..%#x; iDstFrame=%#x (delta %d)\n",
                       iSrcFrame, cDstMinExpect, cDstMaxExpect, iDstFrame, (cDstMinExpect + cDstMaxExpect) / 2 - iDstFrame));

    AudioMixBufDestroy(&MixBuf);
}


/* Test 8-bit sample conversion (8-bit -> internal -> 8-bit). */
static int tstConversion8(RTTEST hTest)
{
    RTTestSub(hTest, "Convert 22kHz/U8 to 44.1kHz/S16 (mono)");
    unsigned         i;
    uint32_t         cBufSize = 256;


    /* 44100Hz, 1 Channel, U8 */
    PDMAUDIOPCMPROPS cfg_p = PDMAUDIOPCMPROPS_INITIALIZER(
        1,                                                                  /* Bytes */
        false,                                                              /* Signed */
        1,                                                                  /* Channels */
        44100,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_p));

    AUDIOMIXBUF parent;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&parent, "Parent", &cfg_p, cBufSize));

    /* Child uses half the sample rate; that ensures the mixing engine can't
     * take shortcuts and performs conversion. Because conversion to double
     * the sample rate effectively inserts one additional sample between every
     * two source frames, N source frames will be converted to N * 2 - 1
     * frames. However, the last source sample will be saved for later
     * interpolation and not immediately output.
     */

    /* 22050Hz, 1 Channel, U8 */
    PDMAUDIOPCMPROPS cfg_c = PDMAUDIOPCMPROPS_INITIALIZER( /* Upmixing to parent */
        1,                                                                  /* Bytes */
        false,                                                              /* Signed */
        1,                                                                  /* Channels */
        22050,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_c));

    AUDIOMIXBUF child;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&child, "Child", &cfg_c, cBufSize));
    RTTESTI_CHECK_RC_OK(AudioMixBufLinkTo(&child, &parent));

    /* 8-bit unsigned frames. Often used with SB16 device. */
    uint8_t aFrames8U[16]  = { 0xAA, 0xBB, 0, 1, 43, 125, 126, 127,
                               128, 129, 130, 131, 132, UINT8_MAX - 1, UINT8_MAX, 0 };

    /*
     * Writing + mixing from child -> parent, sequential.
     */
    uint32_t    cbBuf = 256;
    char        achBuf[256];
    uint32_t    cFramesRead, cFramesWritten, cFramesMixed;

    uint32_t cFramesChild  = 16;
    uint32_t cFramesParent = cFramesChild * 2 - 2;
    uint32_t cFramesTotalRead   = 0;

    /**** 8-bit unsigned samples ****/
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Conversion test %uHz %uch 8-bit\n", cfg_c.uHz, PDMAudioPropsChannels(&cfg_c));
    RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&child, &aFrames8U, sizeof(aFrames8U), &cFramesWritten));
    RTTESTI_CHECK_MSG(cFramesWritten == cFramesChild, ("Child: Expected %RU32 written frames, got %RU32\n", cFramesChild, cFramesWritten));
    RTTESTI_CHECK_RC_OK(AudioMixBufMixToParent(&child, cFramesWritten, &cFramesMixed));
    uint32_t cFrames = AudioMixBufUsed(&parent);
    RTTESTI_CHECK_MSG(AudioMixBufLive(&child) == cFrames, ("Child: Expected %RU32 mixed frames, got %RU32\n", AudioMixBufLive(&child), cFrames));

    RTTESTI_CHECK(AudioMixBufUsed(&parent) == AudioMixBufLive(&child));

    for (;;)
    {
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&parent, achBuf, cbBuf, &cFramesRead));
        if (!cFramesRead)
            break;
        cFramesTotalRead += cFramesRead;
        AudioMixBufReleaseReadBlock(&parent, cFramesRead);
        AudioMixBufFinish(&parent, cFramesRead);
    }

    RTTESTI_CHECK_MSG(cFramesTotalRead == cFramesParent, ("Parent: Expected %RU32 mixed frames, got %RU32\n", cFramesParent, cFramesTotalRead));

    /* Check that the frames came out unharmed. Every other sample is interpolated and we ignore it. */
    /* NB: This also checks that the default volume setting is 0dB attenuation. */
    uint8_t *pSrc8 = &aFrames8U[0];
    uint8_t *pDst8 = (uint8_t *)achBuf;

    for (i = 0; i < cFramesChild - 1; ++i)
    {
        RTTESTI_CHECK_MSG(*pSrc8 == *pDst8, ("index %u: Dst=%d, Src=%d\n", i, *pDst8, *pSrc8));
        pSrc8 += 1;
        pDst8 += 2;
    }

    RTTESTI_CHECK(AudioMixBufUsed(&parent) == 0);
    RTTESTI_CHECK(AudioMixBufLive(&child)  == 0);

    AudioMixBufDestroy(&parent);
    AudioMixBufDestroy(&child);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}

/* Test 16-bit sample conversion (16-bit -> internal -> 16-bit). */
static int tstConversion16(RTTEST hTest)
{
    RTTestSub(hTest, "Convert 22kHz/S16 to 44.1kHz/S16 (mono)");
    unsigned         i;
    uint32_t         cBufSize = 256;

    /* 44100Hz, 1 Channel, S16 */
    PDMAUDIOPCMPROPS cfg_p = PDMAUDIOPCMPROPS_INITIALIZER(
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        1,                                                                  /* Channels */
        44100,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_p));

    AUDIOMIXBUF parent;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&parent, "Parent", &cfg_p, cBufSize));

    /* 22050Hz, 1 Channel, S16 */
    PDMAUDIOPCMPROPS cfg_c = PDMAUDIOPCMPROPS_INITIALIZER( /* Upmixing to parent */
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        1,                                                                  /* Channels */
        22050,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg_c));

    AUDIOMIXBUF child;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&child, "Child", &cfg_c, cBufSize));
    RTTESTI_CHECK_RC_OK(AudioMixBufLinkTo(&child, &parent));

    /* 16-bit signed. More or less exclusively used as output, and usually as input, too. */
    int16_t     aFrames16S[16] = { 0xAA, 0xBB, INT16_MIN, INT16_MIN + 1, INT16_MIN / 2, -3, -2, -1,
                                   0, 1, 2, 3, INT16_MAX / 2, INT16_MAX - 1, INT16_MAX, 0 };

    /*
     * Writing + mixing from child -> parent, sequential.
     */
    uint32_t    cbBuf = 256;
    char        achBuf[256];
    uint32_t    cFramesRead, cFramesWritten, cFramesMixed;

    uint32_t cFramesChild  = 16;
    uint32_t cFramesParent = cFramesChild * 2 - 2;
    uint32_t cFramesTotalRead   = 0;

    /**** 16-bit signed samples ****/
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Conversion test %uHz %uch 16-bit\n", cfg_c.uHz, PDMAudioPropsChannels(&cfg_c));
    RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&child, &aFrames16S, sizeof(aFrames16S), &cFramesWritten));
    RTTESTI_CHECK_MSG(cFramesWritten == cFramesChild, ("Child: Expected %RU32 written frames, got %RU32\n", cFramesChild, cFramesWritten));
    RTTESTI_CHECK_RC_OK(AudioMixBufMixToParent(&child, cFramesWritten, &cFramesMixed));
    uint32_t cFrames = AudioMixBufUsed(&parent);
    RTTESTI_CHECK_MSG(AudioMixBufLive(&child) == cFrames, ("Child: Expected %RU32 mixed frames, got %RU32\n", AudioMixBufLive(&child), cFrames));

    RTTESTI_CHECK(AudioMixBufUsed(&parent) == AudioMixBufLive(&child));

    for (;;)
    {
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&parent, achBuf, cbBuf, &cFramesRead));
        if (!cFramesRead)
            break;
        cFramesTotalRead += cFramesRead;
        AudioMixBufReleaseReadBlock(&parent, cFramesRead);
        AudioMixBufFinish(&parent, cFramesRead);
    }
    RTTESTI_CHECK_MSG(cFramesTotalRead == cFramesParent, ("Parent: Expected %RU32 mixed frames, got %RU32\n", cFramesParent, cFramesTotalRead));

    /* Check that the frames came out unharmed. Every other sample is interpolated and we ignore it. */
    /* NB: This also checks that the default volume setting is 0dB attenuation. */
    int16_t *pSrc16 = &aFrames16S[0];
    int16_t *pDst16 = (int16_t *)achBuf;

    for (i = 0; i < cFramesChild - 1; ++i)
    {
        RTTESTI_CHECK_MSG(*pSrc16 == *pDst16, ("index %u: Dst=%d, Src=%d\n", i, *pDst16, *pSrc16));
        pSrc16 += 1;
        pDst16 += 2;
    }

    RTTESTI_CHECK(AudioMixBufUsed(&parent) == 0);
    RTTESTI_CHECK(AudioMixBufLive(&child)  == 0);

    AudioMixBufDestroy(&parent);
    AudioMixBufDestroy(&child);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}

/* Test volume control. */
static int tstVolume(RTTEST hTest)
{
    RTTestSub(hTest, "Volume control (44.1kHz S16 2ch)");
    uint32_t         cBufSize = 256;

    /* Same for parent/child. */
    /* 44100Hz, 2 Channels, S16 */
    PDMAUDIOPCMPROPS cfg = PDMAUDIOPCMPROPS_INITIALIZER(
        2,                                                                  /* Bytes */
        true,                                                               /* Signed */
        2,                                                                  /* Channels */
        44100,                                                              /* Hz */
        false                                                               /* Swap Endian */
    );

    RTTESTI_CHECK(AudioHlpPcmPropsAreValid(&cfg));

    PDMAUDIOVOLUME vol = { false, 0, 0 };   /* Not muted. */
    AUDIOMIXBUF parent;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&parent, "Parent", &cfg, cBufSize));

    AUDIOMIXBUF child;
    RTTESTI_CHECK_RC_OK(AudioMixBufInit(&child, "Child", &cfg, cBufSize));
    RTTESTI_CHECK_RC_OK(AudioMixBufLinkTo(&child, &parent));

    /* A few 16-bit signed samples. */
    int16_t     aFrames16S[16] = { INT16_MIN, INT16_MIN + 1, -128, -64, -4, -1, 0, 1,
                                   2, 255, 256, INT16_MAX / 2, INT16_MAX - 2, INT16_MAX - 1, INT16_MAX, 0 };

    /*
     * Writing + mixing from child -> parent.
     */
    uint32_t    cbBuf = 256;
    char        achBuf[256];
    uint32_t    cFramesRead, cFramesWritten, cFramesMixed;

    uint32_t cFramesChild  = 8;
    uint32_t cFramesParent = cFramesChild;
    uint32_t cFramesTotalRead;
    int16_t *pSrc16;
    int16_t *pDst16;

    /**** Volume control test ****/
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Volume control test %uHz %uch \n", cfg.uHz, PDMAudioPropsChannels(&cfg));

    /* 1) Full volume/0dB attenuation (255). */
    vol.uLeft = vol.uRight = 255;
    AudioMixBufSetVolume(&child, &vol);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&child, &aFrames16S, sizeof(aFrames16S), &cFramesWritten));
    RTTESTI_CHECK_MSG(cFramesWritten == cFramesChild, ("Child: Expected %RU32 written frames, got %RU32\n", cFramesChild, cFramesWritten));
    RTTESTI_CHECK_RC_OK(AudioMixBufMixToParent(&child, cFramesWritten, &cFramesMixed));

    cFramesTotalRead = 0;
    for (;;)
    {
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&parent, achBuf, cbBuf, &cFramesRead));
        if (!cFramesRead)
            break;
        cFramesTotalRead += cFramesRead;
        AudioMixBufReleaseReadBlock(&parent, cFramesRead);
        AudioMixBufFinish(&parent, cFramesRead);
    }
    RTTESTI_CHECK_MSG(cFramesTotalRead == cFramesParent, ("Parent: Expected %RU32 mixed frames, got %RU32\n", cFramesParent, cFramesTotalRead));

    /* Check that at 0dB the frames came out unharmed. */
    pSrc16 = &aFrames16S[0];
    pDst16 = (int16_t *)achBuf;

    for (unsigned i = 0; i < cFramesParent * 2 /* stereo */; ++i)
    {
        RTTESTI_CHECK_MSG(*pSrc16 == *pDst16, ("index %u: Dst=%d, Src=%d\n", i, *pDst16, *pSrc16));
        ++pSrc16;
        ++pDst16;
    }
    AudioMixBufReset(&child);

    /* 2) Half volume/-6dB attenuation (16 steps down). */
    vol.uLeft = vol.uRight = 255 - 16;
    AudioMixBufSetVolume(&child, &vol);

    RTTESTI_CHECK_RC_OK(AudioMixBufWriteCirc(&child, &aFrames16S, sizeof(aFrames16S), &cFramesWritten));
    RTTESTI_CHECK_MSG(cFramesWritten == cFramesChild, ("Child: Expected %RU32 written frames, got %RU32\n", cFramesChild, cFramesWritten));
    RTTESTI_CHECK_RC_OK(AudioMixBufMixToParent(&child, cFramesWritten, &cFramesMixed));

    cFramesTotalRead = 0;
    for (;;)
    {
        RTTESTI_CHECK_RC_OK_BREAK(AudioMixBufAcquireReadBlock(&parent, achBuf, cbBuf, &cFramesRead));
        if (!cFramesRead)
            break;
        cFramesTotalRead += cFramesRead;
        AudioMixBufReleaseReadBlock(&parent, cFramesRead);
        AudioMixBufFinish(&parent, cFramesRead);
    }
    RTTESTI_CHECK_MSG(cFramesTotalRead == cFramesParent, ("Parent: Expected %RU32 mixed frames, got %RU32\n", cFramesParent, cFramesTotalRead));

    /* Check that at -6dB the sample values are halved. */
    pSrc16 = &aFrames16S[0];
    pDst16 = (int16_t *)achBuf;

    for (unsigned i = 0; i < cFramesParent * 2 /* stereo */; ++i)
    {
        /* Watch out! For negative values, x >> 1 is not the same as x / 2. */
        RTTESTI_CHECK_MSG(*pSrc16 >> 1 == *pDst16, ("index %u: Dst=%d, Src=%d\n", i, *pDst16, *pSrc16));
        ++pSrc16;
        ++pDst16;
    }

    AudioMixBufDestroy(&parent);
    AudioMixBufDestroy(&child);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}

int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstAudioMixBuffer", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstBasics(hTest);
    tstSingle(hTest);
    tstParentChild(hTest);
    tstConversion8(hTest);
    tstConversion16(hTest);
    tstVolume(hTest);
    tstDownsampling(hTest, 44100, 22050);
    tstDownsampling(hTest, 48000, 44100);
    tstDownsampling(hTest, 48000, 22050);
    tstDownsampling(hTest, 48000, 11000);
    tstNewPeek(hTest, 48000, 48000);
    tstNewPeek(hTest, 48000, 11000);
    tstNewPeek(hTest, 48000, 44100);
    tstNewPeek(hTest, 44100, 22050);
    tstNewPeek(hTest, 44100, 11000);
    //tstNewPeek(hTest, 11000, 48000);
    //tstNewPeek(hTest, 22050, 44100);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}
