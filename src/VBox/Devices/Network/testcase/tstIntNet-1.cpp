/* $Id$ */
/** @file
 * VBox - Testcase for internal networking, simple NetFlt trunk creation.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/intnet.h>
#include <VBox/sup.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <iprt/initterm.h>
#include <iprt/alloc.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/param.h>
#include <iprt/getopt.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int g_cErrors = 0;


/**
 * Does packet sniffing for a given period of time.
 *
 * @param   hIf             The interface handle.
 * @param   pSession        The session.
 * @param   pBuf            The shared interface buffer.
 * @param   cMillies        The time period, ms.
 * @param   pFileRaw        The file to write the raw data to (optional).
 * @param   pFileText       The file to write a textual packet summary to (optional).
 */
static void doPacketSniffing(INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PINTNETBUF pBuf, uint32_t cMillies,
                             PRTSTREAM pFileRaw, PRTSTREAM pFileText)
{
    /*
     * Write the raw file header.
     */


    /*
     * The loop.
     */
    uint64_t const StartTS = RTTimeNanoTS();
    PINTNETRINGBUF pRingBuf = &pBuf->Recv;
    for (;;)
    {
        /*
         * Wait for a packet to become available.
         */
        uint64_t cElapsedMillies = (RTTimeNanoTS() - StartTS) / 1000000;
        if (cElapsedMillies >= cMillies)
            break;
        INTNETIFWAITREQ WaitReq;
        WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        WaitReq.Hdr.cbReq = sizeof(WaitReq);
        WaitReq.pSession = pSession;
        WaitReq.hIf = hIf;
        WaitReq.cMillies = cMillies - (uint32_t)cElapsedMillies;
        int rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_WAIT, 0, &WaitReq.Hdr);
        if (rc == VERR_TIMEOUT)
            break;
        if (RT_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("tstIntNet-1: VMMR0_DO_INTNET_IF_WAIT returned %Rrc\n", rc);
            break;
        }

        /*
         * Process the receive buffer.
         */
        while (INTNETRingGetReadable(pRingBuf) > 0)
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uintptr_t)pBuf + pRingBuf->offRead);
            if (pHdr->u16Type == INTNETHDR_TYPE_FRAME)
            {
                size_t      cbFrame = pHdr->cbFrame;
                const void *pvFrame = INTNETHdrGetFramePtr(pHdr, pBuf);
                uint64_t    NanoTS = RTTimeNanoTS() - StartTS;

                if (pFileRaw)
                {
                }

                if (pFileText)
                    RTStrmPrintf(pFileText, "%3RU64.%09u: cb=%04x dst=%.6Rhxs src=%.6Rhxs\n",
                                 NanoTS / 1000000000, (uint32_t)(NanoTS % 1000000000),
                                 cbFrame, pvFrame, (uint8_t *)pvFrame + 6);
            }
            else
            {
                RTPrintf("tstIntNet-1: Unknown frame type %d\n", pHdr->u16Type);
                g_cErrors++;
            }

            /* Advance to the next frame. */
            INTNETRingSkipFrame(pBuf, pRingBuf);
        }
    }

    uint64_t NanoTS = RTTimeNanoTS() - StartTS;
    RTStrmPrintf(pFileText ? pFileText : g_pStdOut,
                 "%3RU64.%09u: stopped. cRecvs=%RU64 cbRecv=%RU64 cLost=%RU64 cOYs=%RU64 cNYs=%RU64\n",
                 NanoTS / 1000000000, (uint32_t)(NanoTS % 1000000000),
                 pBuf->cStatRecvs.c,
                 pBuf->cbStatRecv.c,
                 pBuf->cStatLost.c,
                 pBuf->cStatYieldsOk.c,
                 pBuf->cStatYieldsNok.c
                 );
}


int main(int argc, char **argv)
{
    /*
     * Init the runtime and parse the arguments.
     */
    RTR3Init(false, 0);

    static RTOPTIONDEF const s_aOptions[] =
    {
        { "--duration",     'd', RTGETOPT_REQ_UINT32 },
        { "--file",         'f', RTGETOPT_REQ_STRING },
        { "--promiscuous",  'p', RTGETOPT_REQ_NOTHING },
        { "--recv-buffer",  'r', RTGETOPT_REQ_UINT32 },
        { "--send-buffer",  's', RTGETOPT_REQ_UINT32 },
        { "--sniffer",      'S', RTGETOPT_REQ_NOTHING },
        { "--text-file",    't', RTGETOPT_REQ_STRING },
        { "--xmit-test",    'x', RTGETOPT_REQ_NOTHING },
    };

    uint32_t    cMillies = 1000;
    PRTSTREAM   pFileRaw = NULL;
#ifdef RT_OS_DARWIN
    const char *pszIf = "en0";
#elif defined(RT_OS_LINUX)
    const char *pszIf = "eth0";
#else
    const char *pszIf = "em0";
#endif
    bool        fPromiscuous = false;
    const char *pszNetwork = "tstIntNet-1";
    uint32_t    cbRecv = 0;
    uint32_t    cbSend = 0;
    bool        fSniffer = false;
    PRTSTREAM   pFileText = g_pStdOut;
    bool        fXmitTest = false;

    int rc;
    int ch;
    int iArg = 1;
    RTOPTIONUNION Value;
    while ((ch = RTGetOpt(argc,argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), &iArg, &Value)))
        switch (ch)
        {
            case 'd':
                cMillies = Value.u32 * 1000;
                if (cMillies / 1000 != Value.u32)
                {
                    RTPrintf("tstIntNet-1: warning duration overflowed\n");
                    cMillies = UINT32_MAX - 1;
                }
                break;

            case 'f':
                rc = RTStrmOpen(Value.psz, "w+b", &pFileRaw);
                if (RT_FAILURE(rc))
                {
                    RTPrintf("tstIntNet-1: Failed to creating \"%s\" for writing: %Rrc\n", Value.psz, rc);
                    return 1;
                }
                break;

            case 'i':
                pszIf = Value.psz;
                if (strlen(pszIf) >= INTNET_MAX_TRUNK_NAME)
                {
                    RTPrintf("tstIntNet-1: Interface name is too long (max %d chars): %s\n", INTNET_MAX_TRUNK_NAME - 1, pszIf);
                    return 1;
                }
                break;

            case 'n':
                pszNetwork = Value.psz;
                if (strlen(pszNetwork) >= INTNET_MAX_NETWORK_NAME)
                {
                    RTPrintf("tstIntNet-1: Network name is too long (max %d chars): %s\n", INTNET_MAX_NETWORK_NAME - 1, pszNetwork);
                    return 1;
                }
                break;

            case 'p':
                fPromiscuous = true;
                break;

            case 'r':
                cbRecv = Value.u32;
                break;

            case 's':
                cbSend = Value.u32;
                break;

            case 'S':
                fSniffer = true;
                break;

            case 't':
                if (!*Value.psz)
                    pFileText = NULL;
                else if (!strcmp(Value.psz, "-"))
                    pFileText = g_pStdOut;
                else if (!strcmp(Value.psz, "!"))
                    pFileText = g_pStdErr;
                else
                {
                    rc = RTStrmOpen(Value.psz, "w", &pFileText);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("tstIntNet-1: Failed to creating \"%s\" for writing: %Rrc\n", Value.psz, rc);
                        return 1;
                    }
                }
                break;

            case 'x':
                fXmitTest = true;
                break;

            case '?':
            case 'h':
                RTPrintf("syntax: tstIntNet-1 [-pSt] [-d <secs>] [-f <file>] [-r <size>] [-s <size>]\n");
                return 1;

            default:
                if (RT_SUCCESS(ch))
                    RTPrintf("tstIntNetR0: invalid argument (%#x): %s\n", ch, Value.psz);
                else
                    RTPrintf("tstIntNetR0: invalid argument: %Rrc - \n", ch, Value.pDef->pszLong);
                return 1;
        }
    if (iArg < argc)
    {
        RTPrintf("tstIntNetR0: invalid argument: %s\n", argv[iArg]);
        return 1;
    }


    RTPrintf("tstIntNet-1: TESTING...\n");

    /*
     * Open the session, load ring-0 and issue the request.
     */
    PSUPDRVSESSION pSession;
    rc = SUPInit(&pSession, 0);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: SUPInit -> %Rrc\n", rc);
        return 1;
    }

    char szPath[RTPATH_MAX];
    rc = RTPathProgram(szPath, sizeof(szPath) - sizeof("/../VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: RTPathProgram -> %Rrc\n", rc);
        return 1;
    }

    rc = SUPLoadVMM(strcat(szPath, "/../VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: SUPLoadVMM(\"%s\") -> %Rrc\n", szPath, rc);
        return 1;
    }

    /*
     * Create the request, picking the network and trunk names from argv[2]
     * and argv[1] if present.
     */
    INTNETOPENREQ OpenReq;
    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.pSession = pSession;
    strncpy(OpenReq.szNetwork, pszNetwork, sizeof(OpenReq.szNetwork));
    strncpy(OpenReq.szTrunk, pszIf, sizeof(OpenReq.szTrunk));
    OpenReq.enmTrunkType = kIntNetTrunkType_NetFlt;
    OpenReq.fFlags = 0;
    OpenReq.cbSend = cbSend;
    OpenReq.cbRecv = cbRecv;
    OpenReq.hIf = INTNET_HANDLE_INVALID;

    /*
     * Issue the request.
     */
    RTPrintf("tstIntNet-1: attempting to open/create network \"%s\" with NetFlt trunk \"%s\"...\n",
             OpenReq.szNetwork, OpenReq.szTrunk);
    RTStrmFlush(g_pStdOut);
    rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_OPEN, 0, &OpenReq.Hdr);
    if (RT_SUCCESS(rc))
    {
        RTPrintf("tstIntNet-1: successfully opened/created \"%s\" with NetFlt trunk \"%s\" - hIf=%#x\n",
                 OpenReq.szNetwork, OpenReq.szTrunk, OpenReq.hIf);
        RTStrmFlush(g_pStdOut);

        /*
         * Get the ring-3 address of the shared interface buffer.
         */
        INTNETIFGETRING3BUFFERREQ GetRing3BufferReq;
        GetRing3BufferReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        GetRing3BufferReq.Hdr.cbReq = sizeof(GetRing3BufferReq);
        GetRing3BufferReq.pSession = pSession;
        GetRing3BufferReq.hIf = OpenReq.hIf;
        GetRing3BufferReq.pRing3Buf = NULL;
        rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_GET_RING3_BUFFER, 0, &GetRing3BufferReq.Hdr);
        if (RT_SUCCESS(rc))
        {
            PINTNETBUF pBuf = GetRing3BufferReq.pRing3Buf;
            RTPrintf("tstIntNet-1: pBuf=%p cbBuf=%d cbSend=%d cbRecv=%d\n",
                     pBuf, pBuf->cbBuf, pBuf->cbSend, pBuf->cbRecv);
            RTStrmFlush(g_pStdOut);
            if (fPromiscuous)
            {
                INTNETIFSETPROMISCUOUSMODEREQ PromiscReq;
                PromiscReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
                PromiscReq.Hdr.cbReq    = sizeof(PromiscReq);
                PromiscReq.pSession     = pSession;
                PromiscReq.hIf          = OpenReq.hIf;
                PromiscReq.fPromiscuous = true;
                rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE, 0, &PromiscReq.Hdr);
                if (RT_SUCCESS(rc))
                    RTPrintf("tstIntNet-1: interface in promiscuous mode\n");
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Either enter sniffing mode or do a timeout thing.
                 */
                if (fSniffer)
                    doPacketSniffing(OpenReq.hIf, pSession, pBuf, cMillies, pFileRaw, pFileText);
                else
                    RTThreadSleep(cMillies);
            }
            else
            {
                RTPrintf("tstIntNet-1: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE,) failed, rc=%Rrc\n", rc);
                g_cErrors++;
            }
        }
        else
        {
            RTPrintf("tstIntNet-1: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_IF_GET_RING3_BUFFER,) failed, rc=%Rrc\n", rc);
            g_cErrors++;
        }
    }
    else
    {
        RTPrintf("tstIntNet-1: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_OPEN,) failed, rc=%Rrc\n", rc);
        g_cErrors++;
    }

    SUPTerm(false /* not forced */);

    /* close open files  */
    if (pFileRaw)
        RTStrmClose(pFileRaw);
    if (pFileText && pFileText != g_pStdErr && pFileText != g_pStdOut)
        RTStrmClose(pFileText);

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstIntNet-1: SUCCESS\n");
    else
        RTPrintf("tstIntNet-1: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

