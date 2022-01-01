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

void captureFrame(void *pvUser, void *pvFrame, uint32_t cbFrame);
void captureGSO(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame);
void checkCaptureLimit();

IntNetIf g_net;
PRTSTREAM g_pStrmOut;
uint64_t g_StartNanoTS;
bool g_fPacketBuffered;
uint64_t g_u64Count;
size_t g_cbSnapLen;


RTGETOPTDEF g_aGetOptDef[] =
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
    int rc;

    RTCString strNetworkName;
    RTCString strPcapFile;

    rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTGETOPTSTATE State;
    rc = RTGetOptInit(&State, argc, argv,
                      g_aGetOptDef, RT_ELEMENTS(g_aGetOptDef),
                      1, 0);

    int ch;
    RTGETOPTUNION Val;
    while ((ch = RTGetOpt(&State, &Val)) != 0)
    {
        switch (ch)
        {
            case 'c':           /* --count */
                if (g_u64Count != 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "multiple --count options");
                if (Val.u64 == 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "--count must be greater than zero");
                g_u64Count = Val.u64;
                break;

            case 'i':           /* --network */
                if (strNetworkName.isNotEmpty())
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "multiple --network options");
                if (Val.psz[0] == '\0')
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "empty --network option");
                strNetworkName = Val.psz;
                break;

            case 's':           /* --snaplen */
                if (g_cbSnapLen != 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "multiple --snaplen options");
                if (Val.u32 == 0)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "--snaplen must be greater than zero");
                g_cbSnapLen = Val.u32;
                break;

            case 'U':           /* --packet-buffered */
                g_fPacketBuffered = true;
                break;

            case 'w':           /* --write */
                if (strPcapFile.isNotEmpty())
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "multiple --write options");
                if (Val.psz[0] == '\0')
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "empty --write option");
                strPcapFile = Val.psz;
                break;


            /*
             * Standard options recognized by RTGetOpt()
             */
            case 'V':           /* --version */
                RTPrintf("%sr%u\n", RTBldCfgVersion(), RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            case 'h':           /* --help */
                RTPrintf("%s Version %sr%u\n"
                         "(C) 2009-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                         "All rights reserved.\n"
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

            case VINF_GETOPT_NOT_OPTION:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                      "unexpected non-option argument");

            default:
                return RTGetOptPrintError(ch, &Val);
        }
    }

    if (strNetworkName.isEmpty())
        return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                              "missing --network option");

    if (strPcapFile.isEmpty())
        return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                              "missing --write option");

    if (g_cbSnapLen == 0)
        g_cbSnapLen = 0xffff;


    if (strPcapFile == "-")
    {
        g_pStrmOut = g_pStdOut;
    }
    else
    {
        rc = RTStrmOpen(strPcapFile.c_str(), "wb", &g_pStrmOut);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                  "%s: %Rrf", strPcapFile.c_str(), rc);
    }

    g_net.setInputCallback(captureFrame, NULL);
    g_net.setInputGSOCallback(captureGSO, NULL);

    /*
     * NB: There's currently no way to prevent an intnet from being
     * created when one doesn't exist, so there's no way to catch a
     * typo...  beware.
     */
    rc = g_net.init(strNetworkName);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "%s: %Rrf", strNetworkName.c_str(), rc);

    rc = g_net.ifSetPromiscuous();
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "%s: failed to set promiscuous mode: %Rrf",
                              strNetworkName.c_str(), rc);

    g_StartNanoTS = RTTimeNanoTS();
    rc = PcapStreamHdr(g_pStrmOut, g_StartNanoTS);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "write: %Rrf", rc);
    if (g_fPacketBuffered)
        RTStrmFlush(g_pStrmOut);

    g_net.ifPump();
    RTStrmClose(g_pStrmOut);

    return RTEXITCODE_SUCCESS;
}


void
checkCaptureLimit()
{
    if (g_u64Count > 0)
    {
        if (g_u64Count-- == 1)
            g_net.ifAbort();
    }
}


void
captureFrame(void *pvUser, void *pvFrame, uint32_t cbFrame)
{
    int rc;

    RT_NOREF(pvUser);

    rc = PcapStreamFrame(g_pStrmOut, g_StartNanoTS,
                         pvFrame, cbFrame, g_cbSnapLen);
    if (RT_FAILURE(rc)) {
        RTMsgError("write: %Rrf", rc);
        g_net.ifAbort();
    }

    if (g_fPacketBuffered)
        RTStrmFlush(g_pStrmOut);

    checkCaptureLimit();
}


void
captureGSO(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame)
{
    RT_NOREF(pvUser);
    RT_NOREF(pcGso, cbFrame);

    checkCaptureLimit();
}
