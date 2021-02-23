/* $Id$ */
/** @file
 * Audio testcase - Tests for the IAudioClient3 interface.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#include <iprt/win/windows.h>

#include <Audioclient.h>
#include <mmdeviceapi.h>

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

    /* Note: IAudioClient3 is supported on Win8 or newer. */

    /** @todo Very crude for now, lacks error checking and such. Later. */

    HRESULT hr = CoInitialize(NULL);

    IMMDeviceEnumerator* pEnumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&pEnumerator));

    IMMDevice* pDevice;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    IAudioClient3* pAudioClient;
    hr = pDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&pAudioClient));

    WAVEFORMATEX* pFormat;
    hr = pAudioClient->GetMixFormat(&pFormat);

    UINT32 defaultPeriodInFrames;
    UINT32 fundamentalPeriodInFrames;
    UINT32 minPeriodInFrames;
    UINT32 maxPeriodInFrames;
    hr = pAudioClient->GetSharedModeEnginePeriod(pFormat,
                                                 &defaultPeriodInFrames,
                                                 &fundamentalPeriodInFrames,
                                                 &minPeriodInFrames,
                                                 &maxPeriodInFrames);

    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "def=%RU32, fundamental=%RU32, min=%RU32, max=%RU32\n",
                 defaultPeriodInFrames, fundamentalPeriodInFrames, minPeriodInFrames,  maxPeriodInFrames);

    uint32_t cfDefault = defaultPeriodInFrames * 2;

    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Trying to set %RU32 as default ...\n", cfDefault);

    hr = pAudioClient->InitializeSharedAudioStream(0, cfDefault, pFormat, NULL);
    if (hr != S_OK)
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Unable to set new period");
    else
    {
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "OK");

        hr = pAudioClient->Start();

        /** @todo Do some waiting / testing here. */
    }

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}