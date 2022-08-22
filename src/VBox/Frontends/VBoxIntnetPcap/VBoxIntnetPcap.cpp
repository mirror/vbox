/* $Id$ */
/** @file
 * VBoxIntnetPcap - packet capture for VirtualBox internal networks
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
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
#include "IntNetIf.h"
#include "Pcap.h"

#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/stream.h>

#include <iprt/cpp/ministring.h>

#include <VBox/version.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(void) captureFrame(void *pvUser, void *pvFrame, uint32_t cbFrame);
static DECLCALLBACK(void) captureGSO(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static IntNetIf     g_net;
static PRTSTREAM    g_pStrmOut;
static uint64_t     g_StartNanoTS;
static bool         g_fPacketBuffered;
static uint64_t     g_cCountDown;
static size_t       g_cbSnapLen = 0xffff;

static const RTGETOPTDEF g_aGetOptDef[] =
{
    { "--count",                'c',   RTGETOPT_REQ_UINT64 },
    { "--network",              'i',   RTGETOPT_REQ_STRING },
    { "--snaplen",              's',   RTGETOPT_REQ_UINT32 },
    { "--packet-buffered",      'U',   RTGETOPT_REQ_NOTHING },
    { "--write",                'w',   RTGETOPT_REQ_STRING },
};


int
main(int argc, char *argv[])
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse options
     */
    RTGETOPTSTATE State;
    rc = RTGetOptInit(&State, argc, argv, g_aGetOptDef, RT_ELEMENTS(g_aGetOptDef), 1, 0);
    AssertRC(rc);

    const char *pszNetworkName = NULL;
    const char *pszPcapFile    = NULL;

    int ch;
    RTGETOPTUNION Val;
    while ((ch = RTGetOpt(&State, &Val)) != 0)
    {
        switch (ch)
        {
            case 'c':           /* --count */
                if (Val.u64 == 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--count must be greater than zero");
                g_cCountDown = Val.u64;
                break;

            case 'i':           /* --network */
                if (Val.psz[0] == '\0')
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "empty --network option");
                pszNetworkName = Val.psz;
                break;

            case 's':           /* --snaplen */
                if (Val.u32 == 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "--snaplen must be greater than zero");
                g_cbSnapLen = Val.u32;
                break;

            case 'U':           /* --packet-buffered */
                g_fPacketBuffered = true;
                break;

            case 'w':           /* --write */
                if (Val.psz[0] == '\0')
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "empty --write option");
                pszPcapFile = Val.psz;
                break;


            /*
             * Standard options recognized by RTGetOpt()
             */
            case 'V':           /* --version */
                RTPrintf("%sr%u\n", RTBldCfgVersion(), RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            case 'h':           /* --help */
                RTPrintf("%s Version %sr%u\n"
                         "Copyright (C) 2009-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                         "\n"
                         "Usage: %s <options>\n"
                         "\n"
                         "Options:\n",
                         RTProcShortName(), RTBldCfgVersion(), RTBldCfgRevision(),
                         RTProcShortName());
                for (size_t i = 0; i < RT_ELEMENTS(g_aGetOptDef); ++i)
                    RTPrintf("    -%c, %s\n",
                             g_aGetOptDef[i].iShort, g_aGetOptDef[i].pszLong);
                return RTEXITCODE_SUCCESS;

            default:
            case VINF_GETOPT_NOT_OPTION:
                return RTGetOptPrintError(ch, &Val);
        }
    }
    if (!pszNetworkName)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No network specified. Please use the --network option");
    if (!pszPcapFile)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No output file specified. Please use the --write option");

    /*
     * Open the output file.
     */
    if (strcmp(pszPcapFile, "-") == 0)
        g_pStrmOut = g_pStdOut;
    else
    {
        rc = RTStrmOpen(pszPcapFile, "wb", &g_pStrmOut);
        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("%s: %Rrf", pszPcapFile, rc);
    }

    /*
     * Configure the snooper.
     */
    g_net.setInputCallback(captureFrame, NULL);
    g_net.setInputGSOCallback(captureGSO, NULL);

    /*
     * NB: There's currently no way to prevent an intnet from being
     * created when one doesn't exist, so there's no way to catch a
     * typo...  beware.
     */
    rc = g_net.init(pszNetworkName);
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("%s: %Rrf", pszNetworkName, rc);

    rc = g_net.ifSetPromiscuous();
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("%s: failed to set promiscuous mode: %Rrf", pszNetworkName, rc);

    /*
     * Snoop traffic.
     */
    g_StartNanoTS = RTTimeNanoTS();
    rc = PcapStreamHdr(g_pStrmOut, g_StartNanoTS);
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("write: %Rrf", rc);
    if (g_fPacketBuffered)
        RTStrmFlush(g_pStrmOut);

    g_net.ifPump();

    RTEXITCODE rcExit = RT_SUCCESS(RTStrmError(g_pStrmOut)) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    rc = RTStrmClose(g_pStrmOut);
    if (RT_FAILURE(rc))
        rcExit = RTMsgErrorExitFailure("close: %Rrf", rc);
    return rcExit;
}


static void checkCaptureLimit(void)
{
    if (g_cCountDown > 0)
    {
        if (g_cCountDown-- == 1)
            g_net.ifAbort();
    }
}


static DECLCALLBACK(void) captureFrame(void *pvUser, void *pvFrame, uint32_t cbFrame)
{
    RT_NOREF(pvUser);

    int rc = PcapStreamFrame(g_pStrmOut, g_StartNanoTS, pvFrame, cbFrame, g_cbSnapLen);
    if (RT_FAILURE(rc))
    {
        RTMsgError("write: %Rrf", rc);
        g_net.ifAbort();
    }

    if (g_fPacketBuffered)
        RTStrmFlush(g_pStrmOut);

    checkCaptureLimit();
}


static DECLCALLBACK(void) captureGSO(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame)
{
    RT_NOREF(pvUser, pcGso, cbFrame);

    checkCaptureLimit();
}
