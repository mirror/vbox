/* $Id$ */
/** @file
 * Helpers for writing libpcap files.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "Pcap.h"

#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/err.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/* "libpcap" magic */
#define PCAP_MAGIC  0xa1b2c3d4

/* "libpcap" file header (minus magic number). */
struct pcap_hdr
{
    uint16_t    version_major;  /* major version number                         = 2 */
    uint16_t    version_minor;  /* minor version number                         = 4 */
    int32_t     thiszone;       /* GMT to local correction                      = 0 */
    uint32_t    sigfigs;        /* accuracy of timestamps                       = 0 */
    uint32_t    snaplen;        /* max length of captured packets, in octets    = 0xffff */
    uint32_t    network;        /* data link type                               = 01 */
};

/* "libpcap" record header. */
struct pcaprec_hdr
{
    uint32_t    ts_sec;         /* timestamp seconds */
    uint32_t    ts_usec;        /* timestamp microseconds */
    uint32_t    incl_len;       /* number of octets of packet saved in file */
    uint32_t    orig_len;       /* actual length of packet */
};

struct pcaprec_hdr_init
{
    uint32_t            u32Magic;
    struct pcap_hdr     pcap;
    struct pcaprec_hdr  rec0;
};


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static pcaprec_hdr_init const s_Hdr =
{
    PCAP_MAGIC,
    { 2, 4, 0, 0, 0xffff, 1 },
    /* force ethereal to start at 0.000000. */
    { 0, 1, 0, 60 }

};


/**
 * Internal helper.
 */
static void pcapCalcHeader(struct pcaprec_hdr *pHdr, uint64_t StartNanoTS, size_t cbFrame, size_t cbMax)
{
    uint64_t u64TS = RTTimeNanoTS() - StartNanoTS;
    pHdr->ts_sec   = (uint32_t)(u64TS / 1000000000);
    pHdr->ts_usec  = (uint32_t)((u64TS / 1000) % 1000000);
    pHdr->incl_len = (uint32_t)RT_MIN(cbFrame, cbMax);
    pHdr->orig_len = (uint32_t)cbFrame;
}


/**
 * Writes the stream header.
 *
 * @returns IPRT status code, @see RTStrmWrite.
 *
 * @param   pStream         The stream handle.
 * @param   StartNanoTS     What to subtract from the RTTimeNanoTS output.
 */
int PcapStreamHdr(PRTSTREAM pStream, uint64_t StartNanoTS)
{
    pcaprec_hdr_init Hdr = s_Hdr;
    pcapCalcHeader(&Hdr.rec0, StartNanoTS, 60, 0);
    return RTStrmWrite(pStream, &Hdr, sizeof(Hdr));
}


/**
 * Writes a frame to a stream.
 *
 * @returns IPRT status code, @see RTStrmWrite.
 *
 * @param   pStream         The stream handle.
 * @param   StartNanoTS     What to subtract from the RTTimeNanoTS output.
 * @param   pvFrame         The start of the frame.
 * @param   cbFrame         The size of the frame.
 * @param   cbMax           The max number of bytes to include in the file.
 */
int PcapStreamFrame(PRTSTREAM pStream, uint64_t StartNanoTS, const void *pvFrame, size_t cbFrame, size_t cbMax)
{
    struct pcaprec_hdr Hdr;
    pcapCalcHeader(&Hdr, StartNanoTS, cbFrame, cbMax);
    int rc1 = RTStrmWrite(pStream, &Hdr, sizeof(Hdr));
    int rc2 = RTStrmWrite(pStream, pvFrame, Hdr.incl_len);
    return RT_SUCCESS(rc1) ? rc2 : rc1;
}


/**
 * Writes the file header.
 *
 * @returns IPRT status code, @see RTFileWrite.
 *
 * @param   File            The file handle.
 * @param   StartNanoTS     What to subtract from the RTTimeNanoTS output.
 */
int PcapFileHdr(RTFILE File, uint64_t StartNanoTS)
{
    pcaprec_hdr_init Hdr = s_Hdr;
    pcapCalcHeader(&Hdr.rec0, StartNanoTS, 60, 0);
    return RTFileWrite(File, &Hdr, sizeof(Hdr), NULL);
}


/**
 * Writes a frame to a file.
 *
 * @returns IPRT status code, @see RTFileWrite.
 *
 * @param   File            The file handle.
 * @param   StartNanoTS     What to subtract from the RTTimeNanoTS output.
 * @param   pvFrame         The start of the frame.
 * @param   cbFrame         The size of the frame.
 * @param   cbMax           The max number of bytes to include in the file.
 */
int PcapFileFrame(RTFILE File, uint64_t StartNanoTS, const void *pvFrame, size_t cbFrame, size_t cbMax)
{
    struct pcaprec_hdr  Hdr;
    pcapCalcHeader(&Hdr, StartNanoTS, cbFrame, cbMax);
    int rc1 = RTFileWrite(File, &Hdr, sizeof(Hdr), NULL);
    int rc2 = RTFileWrite(File, pvFrame, Hdr.incl_len, NULL);
    return RT_SUCCESS(rc1) ? rc2 : rc1;
}

