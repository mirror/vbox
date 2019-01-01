/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - VMSVGA FIFO.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SvgaFifo.h"
#include "SvgaHw.h"

#include <iprt/alloc.h>
#include <iprt/thread.h>

NTSTATUS SvgaFifoInit(PVBOXWDDM_EXT_VMSVGA pSvga)
{
// ASMBreakpoint();
    PVMSVGAFIFO pFifo = &pSvga->fifo;

    GALOG(("FIFO: resolution %dx%dx%d\n",
           SVGARegRead(pSvga, SVGA_REG_WIDTH),
           SVGARegRead(pSvga, SVGA_REG_HEIGHT),
           SVGARegRead(pSvga, SVGA_REG_BITS_PER_PIXEL)));

    memset(pFifo, 0, sizeof(*pFifo));

    ExInitializeFastMutex(&pFifo->FifoMutex);

    /** @todo Why these are read here? */
    uint32_t u32EnableState = SVGARegRead(pSvga, SVGA_REG_ENABLE);
    uint32_t u32ConfigDone = SVGARegRead(pSvga, SVGA_REG_CONFIG_DONE);
    uint32_t u32TracesState = SVGARegRead(pSvga, SVGA_REG_TRACES);
    GALOG(("enable %d, config done %d, traces %d\n",
           u32EnableState, u32ConfigDone, u32TracesState));

    SVGARegWrite(pSvga, SVGA_REG_ENABLE, SVGA_REG_ENABLE_ENABLE | SVGA_REG_ENABLE_HIDE);
    SVGARegWrite(pSvga, SVGA_REG_TRACES, 0);

    uint32_t offMin = 4;
    if (pSvga->u32Caps & SVGA_CAP_EXTENDED_FIFO)
    {
        offMin = SVGARegRead(pSvga, SVGA_REG_MEM_REGS);
    }
    /* Minimum offset in bytes. */
    offMin *= sizeof(uint32_t);
    if (offMin < PAGE_SIZE)
    {
        offMin = PAGE_SIZE;
    }

    SVGAFifoWrite(pSvga, SVGA_FIFO_MIN, offMin);
    SVGAFifoWrite(pSvga, SVGA_FIFO_MAX, pSvga->u32FifoSize);
    ASMCompilerBarrier();

    SVGAFifoWrite(pSvga, SVGA_FIFO_NEXT_CMD, offMin);
    SVGAFifoWrite(pSvga, SVGA_FIFO_STOP, offMin);
    SVGAFifoWrite(pSvga, SVGA_FIFO_BUSY, 0);
    ASMCompilerBarrier();

    SVGARegWrite(pSvga, SVGA_REG_CONFIG_DONE, 1);

    pFifo->u32FifoCaps = SVGAFifoRead(pSvga, SVGA_FIFO_CAPABILITIES);

    GALOG(("FIFO: min 0x%08X, max 0x%08X, caps  0x%08X\n",
           SVGAFifoRead(pSvga, SVGA_FIFO_MIN),
           SVGAFifoRead(pSvga, SVGA_FIFO_MAX),
           pFifo->u32FifoCaps));

    SVGAFifoWrite(pSvga, SVGA_FIFO_FENCE, 0);

    return STATUS_SUCCESS;
}

void *SvgaFifoReserve(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t cbReserve)
{
    Assert((cbReserve & 0x3) == 0);

    PVMSVGAFIFO pFifo = &pSvga->fifo;
    void *pvRet = NULL;

    ExAcquireFastMutex(&pFifo->FifoMutex);
    /** @todo The code in SvgaFifoReserve/SvgaFifoCommit runs at IRQL = APC_LEVEL. */

    const uint32_t offMin = SVGAFifoRead(pSvga, SVGA_FIFO_MIN);
    const uint32_t offMax = SVGAFifoRead(pSvga, SVGA_FIFO_MAX);
    const uint32_t offNextCmd = SVGAFifoRead(pSvga, SVGA_FIFO_NEXT_CMD);
    GALOG(("cb %d offMin 0x%08X, offMax 0x%08X, offNextCmd 0x%08X\n",
           cbReserve, offMin, offMax, offNextCmd));

    if (cbReserve < offMax - offMin)
    {
        Assert(pFifo->cbReserved == 0);
        Assert(pFifo->pvBuffer == NULL);

        pFifo->cbReserved = cbReserve;

        for (;;)
        {
            bool fNeedBuffer = false;

            const uint32_t offStop = SVGAFifoRead(pSvga, SVGA_FIFO_STOP);
            GALOG(("    offStop 0x%08X\n", offStop));

            if (offNextCmd >= offStop)
            {
                if (   offNextCmd + cbReserve < offMax
                    || (offNextCmd + cbReserve == offMax && offStop > offMin))
                {
                    /* Enough space for command in FIFO. */
                }
                else if ((offMax - offNextCmd) + (offStop - offMin) <= cbReserve)
                {
                    /* FIFO full. */
                    /** @todo Implement waiting for FIFO space. */
                    RTThreadSleep(10);
                    continue;
                }
                else
                {
                    fNeedBuffer = true;
                }
            }
            else
            {
                if (offNextCmd + cbReserve < offStop)
                {
                    /* Enough space in FIFO. */
                }
                else
                {
                    /* FIFO full. */
                    /** @todo Implement waiting for FIFO space. */
                    RTThreadSleep(10);
                    continue;
                }
            }

            if (!fNeedBuffer)
            {
                if (pFifo->u32FifoCaps & SVGA_FIFO_CAP_RESERVE)
                {
                    SVGAFifoWrite(pSvga, SVGA_FIFO_RESERVED, cbReserve);
                }

                pvRet = (void *)SVGAFifoPtrFromOffset(pSvga, offNextCmd); /** @todo Return ptr to volatile data? */
                GALOG(("    in place %p\n", pvRet));
                break;
            }

            if (fNeedBuffer)
            {
                pvRet = RTMemAlloc(cbReserve);
                pFifo->pvBuffer = pvRet;
                GALOG(("     %p\n", pvRet));
                break;
            }
        }

    }

    if (pvRet)
    {
        return pvRet;
    }

    pFifo->cbReserved = 0;
    ExReleaseFastMutex(&pFifo->FifoMutex);
    return NULL;
}

static void svgaFifoPingHost(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Reason)
{
    if (ASMAtomicCmpXchgU32(&pSvga->pu32FIFO[SVGA_FIFO_BUSY], 1, 0))
    {
        SVGARegWrite(pSvga, SVGA_REG_SYNC, u32Reason);
    }
}

void SvgaFifoCommit(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t cbActual)
{
    Assert((cbActual & 0x3) == 0);

    PVMSVGAFIFO pFifo = &pSvga->fifo;

    const uint32_t offMin = SVGAFifoRead(pSvga, SVGA_FIFO_MIN);
    const uint32_t offMax = SVGAFifoRead(pSvga, SVGA_FIFO_MAX);
    uint32_t offNextCmd = SVGAFifoRead(pSvga, SVGA_FIFO_NEXT_CMD);
    GALOG(("cb %d, offMin 0x%08X, offMax 0x%08X, offNextCmd 0x%08X\n",
           cbActual, offMin, offMax, offNextCmd));

    pFifo->cbReserved = 0;

    if (pFifo->pvBuffer)
    {
        if (pFifo->u32FifoCaps & SVGA_FIFO_CAP_RESERVE)
        {
            SVGAFifoWrite(pSvga, SVGA_FIFO_RESERVED, cbActual);
        }

        const uint32_t cbToWrite = RT_MIN(offMax - offNextCmd, cbActual);
        memcpy((void *)SVGAFifoPtrFromOffset(pSvga, offNextCmd), pFifo->pvBuffer, cbToWrite);
        if (cbActual > cbToWrite)
        {
            memcpy((void *)SVGAFifoPtrFromOffset(pSvga, offMin),
                   (uint8_t *)pFifo->pvBuffer + cbToWrite, cbActual - cbToWrite);
        }
        ASMCompilerBarrier();
    }

    offNextCmd += cbActual;
    if (offNextCmd >= offMax)
    {
        offNextCmd -= offMax - offMin;
    }
    SVGAFifoWrite(pSvga, SVGA_FIFO_NEXT_CMD, offNextCmd);

    RTMemFree(pFifo->pvBuffer);
    pFifo->pvBuffer = NULL;

    if (pFifo->u32FifoCaps & SVGA_FIFO_CAP_RESERVE)
    {
        SVGAFifoWrite(pSvga, SVGA_FIFO_RESERVED, 0);
    }

    svgaFifoPingHost(pSvga, SVGA_SYNC_GENERIC);

    ExReleaseFastMutex(&pFifo->FifoMutex);
}
