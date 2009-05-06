

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxNetLib.h"
#include <VBox/intnet.h>

/**
 * Flushes the send buffer.
 *
 * @returns VBox status code.
 * @param   pSession        The support driver session.
 * @param   hIf             The interface handle to flush.
 */
int VBoxNetIntIfFlush(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf)
{
    INTNETIFSENDREQ SendReq;
    SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SendReq.Hdr.cbReq    = sizeof(SendReq);
    SendReq.pSession     = pSession;
    SendReq.hIf          = hIf;
    return SUPCallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);
}


/**
 * Copys the SG segments into the specified fram.
 *
 * @param   pvFrame     The frame buffer.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 */
static void vboxnetIntIfCopySG(void *pvFrame, size_t cSegs, PCINTNETSEG paSegs)
{
    uint8_t *pbDst = (uint8_t *)pvFrame;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
    {
        memcpy(pbDst, paSegs[iSeg].pv, paSegs[iSeg].cb);
        pbDst += paSegs[iSeg].cb;
    }
}


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 * @remark  This is the same as INTNETRingWriteFrame and
 *          drvIntNetRingWriteFrame.
 */
int VBoxNetIntIfRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, size_t cSegs, PCINTNETSEG paSegs)
{
    /*
     * Validate input.
     */
    Assert(pBuf);
    Assert(pRingBuf);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));
    AssertPtr(paSegs);
    Assert(cSegs > 0);

    /* Calc frame size. */
    uint32_t cbFrame = 0;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
        cbFrame += paSegs[iSeg].cb;

    Assert(cbFrame >= sizeof(RTMAC) * 2);


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

            vboxnetIntIfCopySG(pHdr + 1, cSegs, paSegs);

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

            vboxnetIntIfCopySG(pvFrameOut, cSegs, paSegs);

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

        vboxnetIntIfCopySG(pHdr + 1, cSegs, paSegs);

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
 * Sends a frame
 *
 * @returns VBox status code.
 * @param   pSession    The support driver session.
 * @param   hIf         The interface handle.
 * @param   pBuf        The interface buffer.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 */
int VBoxNetIntIfSend(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf, size_t cSegs, PCINTNETSEG paSegs)
{
    int rc = VBoxNetIntIfRingWriteFrame(pBuf, &pBuf->Send, RT_ELEMENTS(aSegs), &aSegs[0]);
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        VBoxNetIntIfFlush(pSession, hIf);
        rc = VBoxNetIntIfRingWriteFrame(pBuf, &pBuf->Send, RT_ELEMENTS(aSegs), &aSegs[0]);
    }
    if (RT_SUCCESS(rc) && fFlush)
        rc = VBoxNetIntIfFlush(pSession, hIf);
    return rc;
}
