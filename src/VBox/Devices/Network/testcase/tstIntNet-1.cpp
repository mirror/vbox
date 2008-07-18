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
#include <iprt/rand.h>
#include <iprt/log.h>
#include <iprt/crc32.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int g_cErrors = 0;


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   pvFrame     The frame to write.
 * @param   cbFrame     The size of the frame.
 * @remark  This is the same as INTNETRingWriteFrame and drvIntNetRingWriteFrame.
 */
static int tstIntNetWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, const void *pvFrame, uint32_t cbFrame)
{
    /*
     * Validate input.
     */
    Assert(pBuf);
    Assert(pRingBuf);
    Assert(pvFrame);
    Assert(cbFrame >= sizeof(PDMMAC) * 2);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));

    const uint32_t cb = RT_ALIGN_32(cbFrame, sizeof(INTNETHDR));
    if (offRead <= offWrite)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWrite >= cb + sizeof(INTNETHDR))
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = sizeof(INTNETHDR);

            memcpy(pHdr + 1, pvFrame, cbFrame);

            offWrite += cb + sizeof(INTNETHDR);
            Assert(offWrite <= pRingBuf->offEnd && offWrite >= pRingBuf->offStart);
            if (offWrite >= pRingBuf->offEnd)
                offWrite = pRingBuf->offStart;
            Log2(("WriteFrame: offWrite: %#x -> %#x (1)\n", pRingBuf->offWrite, offWrite));
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            return VINF_SUCCESS;
        }

        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWrite >= sizeof(INTNETHDR), ("offEnd=%x offWrite=%x\n", pRingBuf->offEnd, offWrite));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            PINTNETHDR  pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            void       *pvFrameOut = (PINTNETHDR)((uint8_t *)pBuf + pRingBuf->offStart);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = (intptr_t)pvFrameOut - (intptr_t)pHdr;

            memcpy(pvFrameOut, pvFrame, cbFrame);

            offWrite = pRingBuf->offStart + cb;
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            Log2(("WriteFrame: offWrite: %#x -> %#x (2)\n", pRingBuf->offWrite, offWrite));
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWrite > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = cbFrame;
        pHdr->offFrame = sizeof(INTNETHDR);

        memcpy(pHdr + 1, pvFrame, cbFrame);

        offWrite += cb + sizeof(INTNETHDR);
        ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
        Log2(("WriteFrame: offWrite: %#x -> %#x (3)\n", pRingBuf->offWrite, offWrite));
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    /** @todo stats */
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Transmits one frame after appending the CRC.
 *
 * @param   hIf             The interface handle.
 * @param   pSession        The session.
 * @param   pBuf            The shared interface buffer.
 * @param   pvFrame         The frame without a crc.
 * @param   cbFrame         The size of it.
 */
static void doXmitFrame(INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PINTNETBUF pBuf, void *pvFrame, size_t cbFrame)
{
    /*
     * Calcuate and append the checksum.
     */
    uint32_t u32Crc = RTCrc32(pvFrame, cbFrame);
    u32Crc = RT_H2BE_U32(u32Crc);
    memcpy(pvFrame, &u32Crc, sizeof(u32Crc));
    cbFrame += sizeof(u32Crc);

    /*
     * Write the frame and push the queue.
     *
     * Don't bother with dealing with overflows like DrvIntNet does, because
     * it's not supposed to happen here in this testcase.
     */
    int rc = tstIntNetWriteFrame(pBuf, &pBuf->Send, pvFrame, cbFrame);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: tstIntNetWriteFrame failed, %Rrc; cbFrame=%d pBuf->cbSend=%d\n", rc, cbFrame, pBuf->cbSend);
        g_cErrors++;
    }

    INTNETIFSENDREQ SendReq;
    SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SendReq.Hdr.cbReq = sizeof(SendReq);
    SendReq.pSession = pSession;
    SendReq.hIf = hIf;
    rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_IF_SEND,) failed, rc=%Rrc\n", rc);
        g_cErrors++;
    }
}


/**
 * Does the transmit test.
 *
 * @param   hIf             The interface handle.
 * @param   pSession        The session.
 * @param   pBuf            The shared interface buffer.
 * @param   pSrcMac         The mac address to use as source.
 */
static void doXmitText(INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PINTNETBUF pBuf, PCPDMMAC pSrcMac)
{
#pragma pack(1)
    struct
    {
        PDMMAC      DstMac;
        PDMMAC      SrcMac;
        uint16_t    u16Type;

        union
        {
            uint8_t     abData[4096];
            struct
            {

                uint8_t     Op;
                uint8_t     HType;
                uint8_t     HLen;
                uint8_t     Hops;
                uint32_t    XID;
                uint16_t    Secs;
                uint16_t    Flags;
                uint32_t    CIAddr;
                uint32_t    YIAddr;
                uint32_t    SIAddr;
                uint32_t    GIAddr;
                uint8_t     CHAddr[16];
                uint8_t     SName[64];
                uint8_t     File[128];
                uint8_t     Options[64];
                uint8_t     u8TheEnd;
            } DhcpMsg;
        } u;
    } Frame;
#pragma pack(0)

    /*
     * Create a simple DHCP broadcast request.
     */
    memset(&Frame, 0, sizeof(Frame));
    memset(&Frame.DstMac, 0xff, sizeof(Frame.DstMac));
    Frame.SrcMac = *pSrcMac;
    Frame.u16Type = 0x0800;
    Frame.u.DhcpMsg.Op = 1; /* request */
    Frame.u.DhcpMsg.HType = 6;
    Frame.u.DhcpMsg.HLen = sizeof(PDMMAC);
    Frame.u.DhcpMsg.Hops = 0;
    Frame.u.DhcpMsg.XID = RTRandU32();
    Frame.u.DhcpMsg.Secs = 0;
    Frame.u.DhcpMsg.Flags = 1; /* broadcast */
    Frame.u.DhcpMsg.CIAddr = 0;
    Frame.u.DhcpMsg.YIAddr = 0;
    Frame.u.DhcpMsg.SIAddr = 0;
    Frame.u.DhcpMsg.GIAddr = 0;
    memset(&Frame.u.DhcpMsg.CHAddr[0], '\0', sizeof(Frame.u.DhcpMsg.CHAddr));
    memcpy(&Frame.u.DhcpMsg.CHAddr[0], pSrcMac, sizeof(*pSrcMac));
    memset(&Frame.u.DhcpMsg.SName[0], '\0', sizeof(Frame.u.DhcpMsg.SName));
    memset(&Frame.u.DhcpMsg.File[0], '\0', sizeof(Frame.u.DhcpMsg.File));
    memset(&Frame.u.DhcpMsg.Options[0], '\0', sizeof(Frame.u.DhcpMsg.Options));

    doXmitFrame(hIf,pSession,pBuf, &Frame, &Frame.u.DhcpMsg.u8TheEnd - (uint8_t *)&Frame);
}


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
    PDMMAC      SrcMac;
    SrcMac.au8[0] = 0x08;
    SrcMac.au8[1] = 0x03;
    SrcMac.au8[2] = 0x86;
    RTRandBytes(&SrcMac.au8[3], sizeof(SrcMac) - 3);

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
                 * Do the transmit test first and so we can sniff for the response.
                 */
                if (fXmitTest)
                    doXmitText(OpenReq.hIf, pSession, pBuf, &SrcMac);

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

