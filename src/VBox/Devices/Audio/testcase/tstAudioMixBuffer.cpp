/* $Id$ */
/** @file
 * Audio testcase - Mixing buffer.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


#include "../AudioMixBuffer.h"
#include "../DrvAudio.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

static int tstSingle(RTTEST hTest)
{
    RTTestSubF(hTest, "Single buffer");

    PDMAUDIOSTREAMCFG config =
    {
        44100,                   /* Hz */
        2                        /* Channels */,
        AUD_FMT_S16              /* Format */,
        PDMAUDIOENDIANESS_LITTLE /* Endianess */
    };
    PDMPCMPROPS props;

    int rc = drvAudioStreamCfgToProps(&config, &props);
    AssertRC(rc);

    uint32_t cBufSize = _1K;

    /*
     * General stuff.
     */
    PDMAUDIOMIXBUF mb;
    RTTESTI_CHECK_RC_OK(audioMixBufInit(&mb, "Single", &props, cBufSize));
    RTTESTI_CHECK(audioMixBufSize(&mb) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_B2S(&mb,  audioMixBufSizeBytes(&mb)) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_S2B(&mb, audioMixBufSize(&mb)) == audioMixBufSizeBytes(&mb));
    RTTESTI_CHECK(audioMixBufFree(&mb) == cBufSize);
    RTTESTI_CHECK(AUDIOMIXBUF_S2B(&mb, audioMixBufFree(&mb)) == audioMixBufFreeBytes(&mb));

    /*
     * Absolute writes.
     */
    uint32_t read  = 0, written = 0, written_abs = 0;
    int8_t  samples8 [2] = { 0x12, 0x34 };
    int16_t samples16[2] = { 0xAA, 0xBB };
    int32_t samples32[2] = { 0xCC, 0xDD };
    int64_t samples64[2] = { 0xEE, 0xFF };

    RTTESTI_CHECK_RC_OK(audioMixBufWriteAt(&mb, 0, &samples8, sizeof(samples8), &written));
    RTTESTI_CHECK(written == 0 /* Samples */);

    RTTESTI_CHECK_RC_OK(audioMixBufWriteAt(&mb, 0, &samples16, sizeof(samples16), &written));
    RTTESTI_CHECK(written == 1 /* Samples */);

    RTTESTI_CHECK_RC_OK(audioMixBufWriteAt(&mb, 2, &samples32, sizeof(samples32), &written));
    RTTESTI_CHECK(written == 2 /* Samples */);
    written_abs = 0;

    /* Beyond buffer. */
    RTTESTI_CHECK_RC(audioMixBufWriteAt(&mb, audioMixBufSize(&mb) + 1, &samples16, sizeof(samples16),
                                        &written), VERR_BUFFER_OVERFLOW);

    /*
     * Circular writes.
     */
    size_t cToWrite = audioMixBufSize(&mb) - written_abs - 1; /* -1 as padding plus -2 samples for above. */
    for (size_t i = 0; i < cToWrite; i++)
    {
        RTTESTI_CHECK_RC_OK(audioMixBufWriteCirc(&mb, &samples16, sizeof(samples16), &written));
        RTTESTI_CHECK(written == 1);
    }
    RTTESTI_CHECK(!audioMixBufIsEmpty(&mb));
    RTTESTI_CHECK(audioMixBufFree(&mb) == 1);
    RTTESTI_CHECK(audioMixBufFreeBytes(&mb) == AUDIOMIXBUF_S2B(&mb, 1));
    RTTESTI_CHECK(audioMixBufProcessed(&mb) == cToWrite + written_abs /* + last absolute write */);

    RTTESTI_CHECK_RC_OK(audioMixBufWriteCirc(&mb, &samples16, sizeof(samples16), &written));
    RTTESTI_CHECK(written == 1);
    RTTESTI_CHECK(audioMixBufFree(&mb) == 0);
    RTTESTI_CHECK(audioMixBufFreeBytes(&mb) == AUDIOMIXBUF_S2B(&mb, 0));
    RTTESTI_CHECK(audioMixBufProcessed(&mb) == cBufSize);

    /* Circular reads. */
    size_t cToRead = audioMixBufSize(&mb) - written_abs - 1;
    for (size_t i = 0; i < cToWrite; i++)
    {
        RTTESTI_CHECK_RC_OK(audioMixBufReadCirc(&mb, &samples16, sizeof(samples16), &read));
        RTTESTI_CHECK(read == 1);
        audioMixBufFinish(&mb, read);
    }
    RTTESTI_CHECK(!audioMixBufIsEmpty(&mb));
    RTTESTI_CHECK(audioMixBufFree(&mb) == audioMixBufSize(&mb) - written_abs - 1);
    RTTESTI_CHECK(audioMixBufFreeBytes(&mb) == AUDIOMIXBUF_S2B(&mb, cBufSize - written_abs - 1));
    RTTESTI_CHECK(audioMixBufProcessed(&mb) == cBufSize - cToRead + written_abs);

    RTTESTI_CHECK_RC_OK(audioMixBufReadCirc(&mb, &samples16, sizeof(samples16), &read));
    RTTESTI_CHECK(read == 1);
    audioMixBufFinish(&mb, read);
    RTTESTI_CHECK(audioMixBufFree(&mb) == cBufSize - written_abs);
    RTTESTI_CHECK(audioMixBufFreeBytes(&mb) == AUDIOMIXBUF_S2B(&mb, cBufSize - written_abs));
    RTTESTI_CHECK(audioMixBufProcessed(&mb) == written_abs);

    return RTTestSubErrorCount(hTest) ? VERR_GENERAL_FAILURE : VINF_SUCCESS;
}

static int tstParentChild(RTTEST hTest)
{
    RTTestSubF(hTest, "2 Children -> Parent");

    uint32_t cBufSize = _1K;

    PDMAUDIOSTREAMCFG cfg_p =
    {
        44100,                   /* Hz */
        2                        /* Channels */,
        AUD_FMT_S16              /* Format */,
        PDMAUDIOENDIANESS_LITTLE /* Endianess */
    };
    PDMPCMPROPS props;

    int rc = drvAudioStreamCfgToProps(&cfg_p, &props);
    AssertRC(rc);

    PDMAUDIOMIXBUF parent;
    RTTESTI_CHECK_RC_OK(audioMixBufInit(&parent, "Parent", &props, cBufSize));

    PDMAUDIOSTREAMCFG cfg_c1 = /* Downmixing */
    {
        22100,                   /* Hz */
        2                        /* Channels */,
        AUD_FMT_S16              /* Format */,
        PDMAUDIOENDIANESS_LITTLE /* Endianess */
    };

    rc = drvAudioStreamCfgToProps(&cfg_c1, &props);
    AssertRC(rc);

    PDMAUDIOMIXBUF child1;
    RTTESTI_CHECK_RC_OK(audioMixBufInit(&child1, "Child1", &props, cBufSize));
    RTTESTI_CHECK_RC_OK(audioMixBufLinkTo(&child1, &parent));

    PDMAUDIOSTREAMCFG cfg_c2 = /* Upmixing */
    {
        48000,                   /* Hz */
        2                        /* Channels */,
        AUD_FMT_S16              /* Format */,
        PDMAUDIOENDIANESS_LITTLE /* Endianess */
    };

    rc = drvAudioStreamCfgToProps(&cfg_c2, &props);
    AssertRC(rc);

    PDMAUDIOMIXBUF child2;
    RTTESTI_CHECK_RC_OK(audioMixBufInit(&child2, "Child2", &props, cBufSize));
    RTTESTI_CHECK_RC_OK(audioMixBufLinkTo(&child2, &parent));

    /*
     * Writing + mixing from child/children -> parent, sequential.
     */
    size_t cbBuf = _1K;
    char pvBuf[_1K];
    int16_t samples16[32] = { 0xAA, 0xBB };
    uint32_t read , written, proc, mixed;

    uint32_t cSamplesParent1 = 16;
    uint32_t cSamplesChild1  = 16;
    uint32_t cSamplesParent2 = 32;
    uint32_t cSamplesChild2  = 16;

    for (int i = 0; i < 32; i++)
    {
        RTTESTI_CHECK_RC_OK(audioMixBufWriteAt(&child1, 0, &samples16, sizeof(samples16), &written));
        RTTESTI_CHECK(written == cSamplesChild1);
        RTTESTI_CHECK_RC_OK(audioMixBufMixToParent(&child1, written, &mixed));
        RTTESTI_CHECK(mixed == cSamplesChild1);

        RTTESTI_CHECK_RC_OK(audioMixBufReadCirc(&parent, pvBuf, cbBuf, &read));
        RTTESTI_CHECK(read == 31);

        RTTESTI_CHECK_RC_OK(audioMixBufWriteAt(&child2, 0, &samples16, sizeof(samples16), &written));
        RTTESTI_CHECK(written == cSamplesChild2);
        RTTESTI_CHECK_RC_OK(audioMixBufMixToParent(&child2, written, &mixed));
        RTTESTI_CHECK(mixed == cSamplesChild2);

        RTTESTI_CHECK_RC_OK(audioMixBufReadCirc(&parent, pvBuf, cbBuf, &read));
        RTTESTI_CHECK(read == 15);
    }

    RTTESTI_CHECK(audioMixBufProcessed(&parent) == 0);

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

    /*rc = tstSingle(hTest);
    if (RT_SUCCESS(rc))*/
        rc = tstParentChild(hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

