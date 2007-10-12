/** @file
 * INETNET - Internal Networking.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_intnet_h
#define ___VBox_intnet_h

#include <VBox/types.h>
#include <VBox/stam.h>
#include <VBox/sup.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

__BEGIN_DECLS


/** Pointer to an internal network ring-0 instance. */
typedef struct INTNET *PINTNET;

/**
 * Generic two-sided ring buffer.
 *
 * The deal is that there is exactly one writer and one reader.
 * When offRead equals offWrite the buffer is empty. In the other
 * extreme the writer will not use the last free byte in the buffer.
 */
typedef struct INTNETRINGBUF
{
    /** The start of the buffer offset relative to the. (inclusive) */
    uint32_t            offStart;
    /** The offset to the end of the buffer. (exclusive) */
    uint32_t            offEnd;
    /** The current read offset. */
    uint32_t volatile   offRead;
    /** The current write offset. */
    uint32_t volatile   offWrite;
} INTNETRINGBUF;
/** Pointer to a ring buffer. */
typedef INTNETRINGBUF *PINTNETRINGBUF;

/**
 * Get the amount of space available for writing.
 *
 * @returns Number of available bytes.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(uint32_t) INTNETRingGetWritable(PINTNETRINGBUF pRingBuf)
{
    return pRingBuf->offRead <= pRingBuf->offWrite
        ?  pRingBuf->offEnd  - pRingBuf->offWrite + pRingBuf->offRead - pRingBuf->offStart - 1
        :  pRingBuf->offRead - pRingBuf->offWrite - 1;
}


/**
 * Get the amount of data ready for reading.
 *
 * @returns Number of ready bytes.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(uint32_t) INTNETRingGetReadable(PINTNETRINGBUF pRingBuf)
{
    return pRingBuf->offRead <= pRingBuf->offWrite
        ?  pRingBuf->offWrite - pRingBuf->offRead
        :  pRingBuf->offEnd - pRingBuf->offRead + pRingBuf->offWrite - pRingBuf->offStart;
}


/**
 * A interface buffer.
 */
typedef struct INTNETBUF
{
    /** The size of the entire buffer. */
    uint32_t        cbBuf;
    /** The size of the send area. */
    uint32_t        cbSend;
    /** The size of the receive area. */
    uint32_t        cbRecv;
    /** The receive buffer. */
    INTNETRINGBUF   Recv;
    /** The send buffer. */
    INTNETRINGBUF   Send;
    /** Number of times yields help solve an overflow. */
    STAMCOUNTER     cStatYieldsOk;
    /** Number of times yields didn't help solve an overflow. */
    STAMCOUNTER     cStatYieldsNok;
    /** Number of lost packets due to overflows. */
    STAMCOUNTER     cStatLost;
    /** Number of packets received (not counting lost ones). */
    STAMCOUNTER     cStatRecvs;
    /** Number of frame bytes received (not couting lost frames). */
    STAMCOUNTER     cbStatRecv;
    /** Number of packets received. */
    STAMCOUNTER     cStatSends;
    /** Number of frame bytes sent. */
    STAMCOUNTER     cbStatSend;
} INTNETBUF;
typedef INTNETBUF *PINTNETBUF;

/** Internal networking interface handle. */
typedef uint32_t    INTNETIFHANDLE;
/** Pointer to an internal networking interface handle. */
typedef INTNETIFHANDLE *PINTNETIFHANDLE;

/** Or mask to obscure the handle index. */
#define INTNET_HANDLE_MAGIC         0x88880000
/** Mask to extract the handle index. */
#define INTNET_HANDLE_INDEX_MASK    0xffff
/** The maximum number of handles (exclusive) */
#define INTNET_HANDLE_MAX           0xffff
/** Invalid handle. */
#define INTNET_HANDLE_INVALID       (0)


/**
 * The packet header.
 *
 * The header is intentionally 8 bytes long. It will always
 * start at an 8 byte aligned address. Assuming that the buffer
 * size is a multiple of 8 bytes, that means that we can guarantee
 * that the entire header is contiguous in both virtual and physical
 * memory.
 */
#pragma pack(1)
typedef struct INTNETHDR
{
    /** Header type. This is currently serving as a magic, it
     * can be extended later to encode special command packets and stuff.. */
    uint16_t        u16Type;
    /** The size of the frame. */
    uint16_t        cbFrame;
    /** The offset from the start of this header to where the actual frame starts.
     * This is used to keep the frame it self continguous in virtual memory and
     * thereby both simplify reading and   */
    int32_t         offFrame;
} INTNETHDR, *PINTNETHDR;
#pragma pack()

/** INTNETHDR::u16Type value for normal frames. */
#define INTNETHDR_TYPE_FRAME    0x2442


/**
 * Calculates the pointer to the frame.
 *
 * @returns Pointer to the start of the frame.
 * @param   pHdr        Pointer to the packet header
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 */
DECLINLINE(void *) INTNETHdrGetFramePtr(PINTNETHDR pHdr, PINTNETBUF pBuf)
{
    uint8_t *pu8 = (uint8_t *)pHdr + pHdr->offFrame;
#ifdef VBOX_STRICT
    const uintptr_t off = (uintptr_t)pu8 - (uintptr_t)pBuf;
    Assert(pHdr->u16Type == INTNETHDR_TYPE_FRAME);
    Assert(off < pBuf->cbBuf);
    Assert(off + pHdr->cbFrame <= pBuf->cbBuf);
#endif
    NOREF(pBuf);
    return pu8;
}


/**
 * Skips to the next (read) frame in the buffer.
 *
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer in question.
 */
DECLINLINE(void) INTNETRingSkipFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf)
{
    Assert(pRingBuf->offRead < pBuf->cbBuf);
    Assert(pRingBuf->offRead >= pRingBuf->offStart);
    Assert(pRingBuf->offRead < pRingBuf->offEnd);
    uint32_t    offRead   = pRingBuf->offRead;
    PINTNETHDR  pHdr      = (PINTNETHDR)((uint8_t *)pBuf + offRead);

    /* skip the frame */
    offRead += pHdr->offFrame + pHdr->cbFrame;
    offRead = RT_ALIGN_32(offRead, sizeof(INTNETHDR));
    Assert(offRead <= pRingBuf->offEnd && offRead >= pRingBuf->offStart);
    if (offRead >= pRingBuf->offEnd)
        offRead = pRingBuf->offStart;
    ASMAtomicXchgU32(&pRingBuf->offRead, offRead);
}

/** The maximum length of a network name. */
#define INTNET_MAX_NETWORK_NAME 128


/**
 * Request buffer for INTNETR0OpenReq / VMMR0_DO_INTNET_OPEN.
 * @see INTNETR0Open.
 */
typedef struct INTNETOPENREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** The network name. (input) */
    char            szNetwork[INTNET_MAX_NETWORK_NAME];
    /** The size of the send buffer. (input) */
    uint32_t        cbSend;
    /** The size of the receive buffer. (input) */
    uint32_t        cbRecv;
    /** Whether new participants should be subjected to access check or not. */
    bool            fRestrictAccess;
    /** The handle to the network interface. (output) */
    INTNETIFHANDLE  hIf;
} INTNETOPENREQ;
/** Pointer to an INTNETR0OpenReq / VMMR0_DO_INTNET_OPEN request buffer. */
typedef INTNETOPENREQ *PINTNETOPENREQ;

INTNETR0DECL(int) INTNETR0OpenReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETOPENREQ pReq);


/**
 * Request buffer for INTNETR0IfCloseReq / VMMR0_DO_INTNET_IF_CLOSE.
 * @see INTNETR0IfClose.
 */
typedef struct INTNETIFCLOSEREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** The handle to the network interface. */
    INTNETIFHANDLE  hIf;
} INTNETIFCLOSEREQ;
/** Pointer to an INTNETR0IfCloseReq / VMMR0_DO_INTNET_IF_CLOSE request buffer. */
typedef INTNETIFCLOSEREQ *PINTNETIFCLOSEREQ;

INTNETR0DECL(int) INTNETR0IfCloseReq(PINTNET pIntNet, PINTNETIFCLOSEREQ pReq);


/**
 * Request buffer for INTNETR0IfGetRing3BufferReq / VMMR0_DO_INTNET_IF_GET_RING3_BUFFER.
 * @see INTNETR0IfGetRing3Buffer.
 */
typedef struct INTNETIFGETRING3BUFFERREQ
{
    /** The request header. */
    SUPVMMR0REQHDR          Hdr;
    /** Handle to the interface. */
    INTNETIFHANDLE          hIf;
    /** The pointer to the ring3 buffer. (output) */
    R3PTRTYPE(PINTNETBUF)   pRing3Buf;
} INTNETIFGETRING3BUFFERREQ;
/** Pointer to an INTNETR0IfGetRing3BufferReq / VMMR0_DO_INTNET_IF_GET_RING3_BUFFER request buffer. */
typedef INTNETIFGETRING3BUFFERREQ *PINTNETIFGETRING3BUFFERREQ;

INTNETR0DECL(int) INTNETR0IfGetRing3BufferReq(PINTNET pIntNet, PINTNETIFGETRING3BUFFERREQ pReq);


/**
 * Request buffer for INTNETR0IfSetPromiscuousModeReq / VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE.
 * @see INTNETR0IfSetPromiscuousMode.
 */
typedef struct INTNETIFSETPROMISCUOUSMODEREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** Handle to the interface. */
    INTNETIFHANDLE  hIf;
    /** The new promiscuous mode. */
    bool            fPromiscuous;
} INTNETIFSETPROMISCUOUSMODEREQ;
/** Pointer to an INTNETR0IfSetPromiscuousModeReq / VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE request buffer. */
typedef INTNETIFSETPROMISCUOUSMODEREQ *PINTNETIFSETPROMISCUOUSMODEREQ;

INTNETR0DECL(int) INTNETR0IfSetPromiscuousModeReq(PINTNET pIntNet, PINTNETIFSETPROMISCUOUSMODEREQ pReq);


/**
 * Request buffer for INTNETR0IfSendReq / VMMR0_DO_INTNET_IF_SEND.
 * @see INTNETR0IfSend.
 */
typedef struct INTNETIFSENDREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** Handle to the interface. */
    INTNETIFHANDLE  hIf;
} INTNETIFSENDREQ;
/** Pointer to an INTNETR0IfSend() argument package. */
typedef INTNETIFSENDREQ *PINTNETIFSENDREQ;

INTNETR0DECL(int) INTNETR0IfSendReq(PINTNET pIntNet, PINTNETIFSENDREQ pReq);


/**
 * Request buffer for INTNETR0IfWaitReq / VMMR0_DO_INTNET_IF_WAIT.
 * @see INTNETR0IfWait.
 */
typedef struct INTNETIFWAITREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** Handle to the interface. */
    INTNETIFHANDLE  hIf;
    /** The number of milliseconds to wait. */
    uint32_t        cMillies;
} INTNETIFWAITREQ;
/** Pointer to an INTNETR0IfWaitReq / VMMR0_DO_INTNET_IF_WAIT request buffer. */
typedef INTNETIFWAITREQ *PINTNETIFWAITREQ;

INTNETR0DECL(int) INTNETR0IfWaitReq(PINTNET pIntNet, PINTNETIFWAITREQ pReq);


#if defined(IN_RING0) || defined(IN_INTNET_TESTCASE)
/** @name
 * @{
 */

/**
 * Create an instance of the Ring-0 internal networking service.
 *
 * @returns VBox status code.
 * @param   ppIntNet    Where to store the instance pointer.
 */
INTNETR0DECL(int) INTNETR0Create(PINTNET *ppIntNet);

/**
 * Destroys an instance of the Ring-0 internal networking service.
 *
 * @param   pIntNet     Pointer to the instance data.
 */
INTNETR0DECL(void) INTNETR0Destroy(PINTNET pIntNet);

/**
 * Opens a network interface and attaches it to the specified network.
 *
 * @returns VBox status code.
 * @param   pIntNet         The internal network instance.
 * @param   pSession        The session handle.
 * @param   pszNetwork      The network name.
 * @param   cbSend          The send buffer size.
 * @param   cbRecv          The receive buffer size.
 * @param   fRestrictAccess Whether new participants should be subjected to access check or not.
 * @param   phIf            Where to store the handle to the network interface.
 */
INTNETR0DECL(int) INTNETR0Open(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, unsigned cbSend, unsigned cbRecv, bool fRestrictAccess, PINTNETIFHANDLE phIf);

/**
 * Close an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance handle.
 * @param   hIf         The interface handle.
 */
INTNETR0DECL(int) INTNETR0IfClose(PINTNET pIntNet, INTNETIFHANDLE hIf);

/**
 * Gets the ring-0 address of the current buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIF         The interface handle.
 * @param   ppRing0Buf  Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing0Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PINTNETBUF *ppRing0Buf);

/**
 * Maps the default buffer into ring 3.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIF         The interface handle.
 * @param   ppRing3Buf  Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing3Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, R3PTRTYPE(PINTNETBUF) *ppRing3Buf);

/**
 * Sets the promiscuous mode property of an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   fPromiscuous    Set if the interface should be in promiscuous mode, clear if not.
 */
INTNETR0DECL(int) INTNETR0IfSetPromiscuousMode(PINTNET pIntNet, INTNETIFHANDLE hIf, bool fPromiscuous);

/**
 * Sends one or more frames.
 *
 * The function will first the frame which is passed as the optional
 * arguments pvFrame and cbFrame. These are optional since it also
 * possible to chain together one or more frames in the send buffer
 * which the function will process after considering it's arguments.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIF         The interface handle.
 * @param   pvFrame     Pointer to the frame.
 * @param   cbFrame     Size of the frame.
 */
INTNETR0DECL(int) INTNETR0IfSend(PINTNET pIntNet, INTNETIFHANDLE hIf, const void *pvFrame, unsigned cbFrame);

/**
 * Wait for the interface to get signaled.
 * The interface will be signaled when is put into the receive buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance handle.
 * @param   hIf         The interface handle.
 * @param   cMillies    Number of milliseconds to wait. RT_INDEFINITE_WAIT should be
 *                      used if indefinite wait is desired.
 */
INTNETR0DECL(int) INTNETR0IfWait(PINTNET pIntNet, INTNETIFHANDLE hIf, uint32_t cMillies);

/** @} */
#endif /* IN_RING0 */

__END_DECLS

#endif
