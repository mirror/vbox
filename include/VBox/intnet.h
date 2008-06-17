/** @file
 * INETNET - Internal Networking.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
/** Pointer to an interface buffer. */
typedef INTNETBUF *PINTNETBUF;
/** Pointer to a const interface buffer. */
typedef INTNETBUF const *PCINTNETBUF;

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
     * can be extended later to encode special command packets and stuff. */
    uint16_t        u16Type;
    /** The size of the frame. */
    uint16_t        cbFrame;
    /** The offset from the start of this header to where the actual frame starts.
     * This is used to keep the frame it self continguous in virtual memory and
     * thereby both simplify reading and   */
    int32_t         offFrame;
} INTNETHDR;
#pragma pack()
/** Pointer to a packet header.*/
typedef INTNETHDR *PINTNETHDR;
/** Pointer to a const packet header.*/
typedef INTNETHDR const *PCINTNETHDR;

/** INTNETHDR::u16Type value for normal frames. */
#define INTNETHDR_TYPE_FRAME    0x2442


/**
 * Calculates the pointer to the frame.
 *
 * @returns Pointer to the start of the frame.
 * @param   pHdr        Pointer to the packet header
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 */
DECLINLINE(void *) INTNETHdrGetFramePtr(PCINTNETHDR pHdr, PCINTNETBUF pBuf)
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
    uint32_t    offRead   = pRingBuf->offRead;
    PINTNETHDR  pHdr      = (PINTNETHDR)((uint8_t *)pBuf + offRead);
    Assert(pRingBuf->offRead < pBuf->cbBuf);
    Assert(pRingBuf->offRead >= pRingBuf->offStart);
    Assert(pRingBuf->offRead < pRingBuf->offEnd);

    /* skip the frame */
    offRead += pHdr->offFrame + pHdr->cbFrame;
    offRead = RT_ALIGN_32(offRead, sizeof(INTNETHDR));
    Assert(offRead <= pRingBuf->offEnd && offRead >= pRingBuf->offStart);
    if (offRead >= pRingBuf->offEnd)
        offRead = pRingBuf->offStart;
    ASMAtomicXchgU32(&pRingBuf->offRead, offRead);
}


/**
 * Scatter / Gather segment (internal networking).
 */
typedef struct INTNETSEG
{
    /** The physical address. NIL_RTHCPHYS is not set. */
    RTHCPHYS        Phys;
    /** Pointer to the segment data. */
    void           *pv;
    /** The segment size. */
    uint32_t        cb;
} INTNETSEG;
/** Pointer to a internal networking packet segment. */
typedef INTNETSEG *PINTNETSEG;
/** Pointer to a internal networking packet segment. */
typedef INTNETSEG const *PCINTNETSEG;


/**
 * Scatter / Gather list (internal networking).
 *
 * This is used when communicating with the trunk port.
 */
typedef struct INTNETSG
{
    /** The total length of the scatter gather list. */
    uint32_t        cbTotal;
    /** The number of users (references).
     * This is used by the SGRelease code to decide when it can be freed. */
    uint16_t volatile cUsers;
    /** Flags, see INTNETSG_FLAGS_* */
    uint16_t volatile fFlags;
    /** The number of segments allocated. */
    uint16_t        cSegsAlloc;
    /** The number of segments actually used. */
    uint16_t        cSegsUsed;
    /** Variable sized list of segments. */
    INTNETSEG       aSegs[1];
} INTNETSG;
/** Pointer to a scatter / gather list. */
typedef INTNETSG *PINTNETSG;
/** Pointer to a const scatter / gather list. */
typedef INTNETSG const *PCINTNETSG;

/** @name INTNETSG::fFlags definitions.
 * @{ */
/** Set if the SG is free. */
#define INTNETSG_FLAGS_FREE     RT_BIT_32(1)
/** Set if the SG is a temporary one that will become invalid upon return.
 * Try to finish using it before returning, and if that's not possible copy
 * to other buffers.
 * When not set, the callee should always free the SG.
 * Attempts to free it made by the callee will be quietly ignored. */
#define INTNETSG_FLAGS_TEMP     RT_BIT_32(2)
/** @} */


/**
 * Initializes a scatter / gather buffer from a internal networking packet.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 *                      (The fFlags, cUsers, and cSegsAlloc members are left untouched.)
 * @param   pHdr        Pointer to the packet header.
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 * @remarks Perhaps move this...
 */
DECLINLINE(void) INTNETSgInitFromPkt(PINTNETSG pSG, PCINTNETHDR pPktHdr, PCINTNETBUF pBuf)
{
    pSG->cSegsUsed = 1;
    pSG->cbTotal = pSG->aSegs[0].cb = pPktHdr->cbFrame;
    pSG->aSegs[0].pv = INTNETHdrGetFramePtr(pPktHdr, pBuf);
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;
}



/** Pointer to the switch side of a trunk port. */
typedef struct INTNETTRUNKSWPORT *PINTNETTRUNKSWPORT;
/**
 * This is the port on the internal network 'switch', i.e.
 * what the driver is connected to.
 *
 * This is only used for the in-kernel trunk connections.
 */
typedef struct INTNETTRUNKSWPORT
{
    /** Structure version number. (INTNETTRUNKSWPORT_VERSION) */
    uint32_t u32Version;

    /**
     * Selects whether outgoing SGs should have their physical address set.
     *
     * By enabling physical addresses in the scatter / gather segments it should
     * be possible to save some unnecessary address translation and memory locking
     * in the network stack. (Internal networking knows the physical address for
     * all the INTNETBUF data and that it's locked memory.) There is a negative
     * side effects though, frames that crosses page boundraries will require
     * multiple scather / gather segments.
     *
     * @returns The old setting.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   fEnable     Whether to enable or disable it.
     *
     * @remarks Will grab the network semaphore.
     */
    DECLR0CALLBACKMEMBER(bool, pfnSetSGPhys,(PINTNETTRUNKSWPORT pIfPort, bool fEnable));

    /**
     * Frame from the host that's about to hit the wire.
     *
     * @returns true if we've handled it and it should be dropped.
     *          false if it should hit the wire.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   pSG         The (scatter /) gather structure for the frame.
     *                      This will only be use during the call, so a temporary one can
     *                      be used. The Phys member will not be used.
     *
     * @remarks Will grab the network semaphore.
     *
     * @remark  NAT and TAP will use this interface.
     */
    DECLR0CALLBACKMEMBER(bool, pfnRecvHost,(PINTNETTRUNKSWPORT pIfPort, PINTNETSG pSG));

    /**
     * Frame from the wire that's about to hit the network stack.
     *
     * @returns true if we've handled it and it should be dropped.
     *          false if it should hit the network stack.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   pSG         The (scatter /) gather structure for the frame.
     *                      This will only be use during the call, so a temporary one can
     *                      be used. The Phys member will not be used.
     *
     * @remarks Will grab the network semaphore.
     *
     * @remark  NAT and TAP will not this interface.
     */
    DECLR0CALLBACKMEMBER(bool, pfnRecvWire,(PINTNETTRUNKSWPORT pIfPort, PINTNETSG pSG));

    /** Structure version number. (INTNETTRUNKSWPORT_VERSION) */
    uint32_t u32VersionEnd;
} INTNETTRUNKSWPORT;

/** Version number for the INTNETTRUNKIFPORT::u32Version and INTNETTRUNKIFPORT::u32VersionEnd fields. */
#define INTNETTRUNKSWPORT_VERSION   UINT32_C(0xA2CDf001)


/** Pointer to the interface side of a trunk port. */
typedef struct INTNETTRUNKIFPORT *PINTNETTRUNKIFPORT;
/**
 * This is the port on the trunk interface, i.e. the driver
 * side which the internal network is connected to.
 *
 * This is only used for the in-kernel trunk connections.
 */
typedef struct INTNETTRUNKIFPORT
{
    /** Structure version number. (INTNETTRUNKIFPORT_VERSION) */
    uint32_t u32Version;

    /**
     * Changes the active state of the interface.
     *
     * The interface is created in the suspended (non-active) state and then activated
     * when the VM/network is started. It may be suspended and re-activated later
     * for various reasons. It will finally be suspended again before disconnecting
     * the interface from the internal network, however, this might be done immediately
     * before disconnecting and may leave an incoming frame waiting on the internal network
     * semaphore.
     *
     * @returns The previous state.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   fActive     True if the new state is 'active', false if the new state is 'suspended'.
     *
     * @remarks Called while owning the network semaphore.
     */
    DECLR0CALLBACKMEMBER(bool, pfnSetActive,(PINTNETTRUNKIFPORT pIfPort, bool fActive));

    /**
     * Tests if the mac address belongs to any of the host NICs
     * and should take the pfnSendToHost route.
     *
     * @returns true / false.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   pvMac       Pointer to the mac address. This can be cast to PCPDMMAC (fixme: make it an common type?)
     *
     * @remarks Called while owning the network semaphore.
     *
     * @remarks TAP and NAT will compare with their own MAC address and let all their
     *          traffic go over the pfnSendToHost method.
     */
    DECLR0CALLBACKMEMBER(bool, pfnIsHostMac,(PINTNETTRUNKIFPORT pIfPort, /*PCPDMMAC*/ void const *pvMac));

    /**
     * Tests whether the host is operating the interface is promiscuous mode.
     *
     * The default behavior of internal networking 'switch' is to 'autodetect'
     * promiscuous mode on the trunk port, which is where this method is used.
     * For security reasons this default my of course be overridden so that the
     * host cannot sniff at what's going on.
     *
     * Note that this differs from operating the trunk port on the switch in
     * 'promiscuous' mode, because that relates to the bits going to the wire.
     *
     * @returns true / false.
     *
     * @param   pIfPort     Pointer to this structure.
     *
     * @remarks Called while owning the network semaphore.
     */
    DECLR0CALLBACKMEMBER(bool, pfnIsPromiscuous,(PINTNETTRUNKIFPORT pIfPort));

    /**
     * Send the frame to the host.
     *
     * This path is taken if pfnIsHostMac returns true and the trunk port on the
     * internal network is configured to let traffic thru to the host. It may also
     * be taken if the host is in promiscuous mode and the internal network is
     * configured to respect this for internal targets.
     *
     * @return  VBox status code. Error generally means we'll drop the packet.
     * @param   pIfPort     Pointer to this structure.
     * @param   pSG         Pointer to the (scatter /) gather structure for the frame.
     *                      This will never be a temporary one, so, it's safe to
     *                      do this asynchronously to save unnecessary buffer
     *                      allocating and copying.
     *
     * @remarks Called while owning the network semaphore?
     *
     * @remarks TAP and NAT will use this interface for all their traffic, see pfnIsHostMac.
     */
    DECLR0CALLBACKMEMBER(int, pfnSendToHost,(PINTNETTRUNKIFPORT pIfPort, PINTNETSG pSG));

    /**
     * Put the frame on the wire.
     *
     * This path is taken if pfnIsHostMac returns false and the trunk port on the
     * internal network is configured to let traffic out on the wire. This may also
     * be taken for both internal and host traffic if the trunk port is configured
     * to be in promiscuous mode.
     *
     * @return  VBox status code. Error generally means we'll drop the packet.
     * @param   pIfPort     Pointer to this structure.
     * @param   pSG         Pointer to the (scatter /) gather structure for the frame.
     *                      This will never be a temporary one, so, it's safe to
     *                      do this asynchronously to save unnecessary buffer
     *                      allocating and copying.
     *
     * @remarks Called while owning the network semaphore?
     *
     * @remarks TAP and NAT will call pfnSGRelease and return successfully.
     */
    DECLR0CALLBACKMEMBER(int, pfnSendToWire,(PINTNETTRUNKIFPORT pIfPort, PINTNETSG pSG));

    /**
     * This is called by the pfnSendToHost and pfnSendToWire code when they are
     * done with a SG.
     *
     * It may be called after they return if the frame was pushed in an
     * async manner.
     *
     * @param   pIfPort     Pointer to this structure.
     * @param   pSG         Pointer to the (scatter /) gather structure.
     *
     * @remarks Will grab the network semaphore.
     */
    DECLR0CALLBACKMEMBER(void, pfnSGRelease,(PINTNETTRUNKIFPORT pIfPort, PINTNETSG pSG));

    /**
     * Destroys this network interface port.
     *
     * This is called either when disconnecting the trunk interface at runtime or
     * when the network is being torn down. In both cases, the interface will be
     * suspended first. Note that this may still cause races in the receive path...
     *
     * @param   pIfPort     Pointer to this structure.
     *
     * @remarks Called while owning the network semaphore.
     */
    DECLR0CALLBACKMEMBER(bool, pfnDestroy,(PINTNETTRUNKIFPORT pIfPort));

    /** Structure version number. (INTNETTRUNKIFPORT_VERSION) */
    uint32_t u32VersionEnd;
} INTNETTRUNKIFPORT;

/** Version number for the INTNETTRUNKIFPORT::u32Version and INTNETTRUNKIFPORT::u32VersionEnd fields. */
#define INTNETTRUNKIFPORT_VERSION   UINT32_C(0xA2CDe001)


/**
 * The component factory interface for create a network
 * interface filter (like VBoxNetFlt).
 */
typedef struct INTNETTRUNKNETFLTFACTORY
{
    /**
     * Create an instance for the specfied host interface.
     *
     * The initial interface active state is false (suspended).
     *
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS and *ppIfPort set on success.
     * @retval  VERR_INTNET_FLT_IF_NOT_FOUND if the interface was not found.
     * @retval  VERR_INTNET_FLT_IF_BUSY if the interface is already connected.
     * @retval  VERR_INTNET_FLT_IF_FAILED if it failed for some other reason.
     *
     * @param   pIfFactory          Pointer to this structure.
     * @param   pszName             The interface name (OS specific).
     * @param   pSwitchPort         Pointer to the port interface on the switch that
     *                              this interface is being connected to.
     * @param   ppIfPort            Where to store the pointer to the interface port
     *                              on success.
     *
     * @remarks Called while owning the network semaphore.
     */
    DECLR0CALLBACKMEMBER(int, pfnCreate,(struct INTNETTRUNKNETFLTFACTORY *pIfFactory, const char *pszName,
                                         PINTNETTRUNKSWPORT pSwitchPort, PINTNETTRUNKIFPORT *ppIfPort));
} INTNETTRUNKNETFLTFACTORY;
/** Pointer to the trunk factory. */
typedef INTNETTRUNKNETFLTFACTORY *PINTNETTRUNKNETFLTFACTORY;

/** The UUID for the current network interface filter factory. */
#define INTNETTRUNKNETFLTFACTORY_UUID_STR   "0e32db7d-165d-4fc9-9bce-acb2798ce7fb"




/** The maximum length of a network name. */
#define INTNET_MAX_NETWORK_NAME     128


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
