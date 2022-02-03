/* $Id$ */
/** @file
 * Dev3C501 - 3Com EtherLink (3C501) Ethernet Adapter Emulation.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_dev_3c501  3Com 3C501 Ethernet Controller Emulation.
 *
 * This software was written based on the following documents:
 *
 *     - 3Com IBM Ethernet (IE) Controller/Transceiver
 *          External Reference Specification, March 15, 1983
 *     - 3Com EtherLink (3C501) Adapter Technical Reference
 *          Manual Part No. 6405-00, November 26, 1988
 *     - SEEQ 8001 EDLC Ethernet Data Link Controller
 *          Preliminary Data Sheet, December 1982
 *
 * The emulation is compatible with 3Com 3C501 EtherLink aka IE4. It also
 * appears to be compatible with the original 1982 3C500 IBM Ethernet aka
 * IE; the IE and IE4 documentation is nearly identical.
 *
 * The EtherLink is a very early design. It has only a single 2K buffer for
 * both send and receive, and was desgined long before full-duplex Ethernet
 * was possible (it is capable of simultaneous send and receive, but only in
 * loopback mode). If it has just received a packet, the EtherLink can't
 * receive another packet until the first one has been processed by the
 * host.
 *
 * The above problem is greatly alleviated in a VM because incoming packets
 * can be buffered for a short while and don't have to be immediately
 * dropped just because the adapter is currently sending or because the
 * receive status register has not been read yet.
 *
 * The first 8 registers (station address, receive and transmit command and
 * status) are implemented in the SEEQ 8001 EDLC chip. The remaining 8
 * registers are provided by the 3Com ASIC (0755-02) on the 3C501 or
 * discrete chips on the 3C500.
 *
 * The '16 collisions' bit in the transmit command/status register is nearly
 * useless. The SEEQ 8001 could retransmit automatically, but the IE/IE4 can
 * not because the GP Buffer Pointer needs to be reinitialized by software
 * prior to each transmit attempt. It is unclear if the 16-collision counter
 * only rolls over modulo 16 or if it is cleared by something other than
 * reset.
 *
 * The 3C501 supports DMA transfers to/from the packet buffer. Many drivers
 * don't use DMA by default or at all. Due to the overhead of programming
 * the DMA controller, direct I/O access (rep insb/outsb) is always faster
 * in a VM. DMA would only be a win for very ancient drivers which don't use
 * the rep insb/outsb instructions (those didn't exist on the 8086/8088).
 *
 * NB: The default DMA channel (channel 1) conflicts with the default Sound
 * Blaster settings. If both 3C501 and SB16 are used, then one of them
 * either needs to be reconfigured to use DMA channel other than 1 or the
 * 3C501 must not use DMA.
 *
 * The 3Com documentation implies that writing the low byte of the Receive
 * Buffer Pointer is enough to clear the pointer. Yet almost all drivers,
 * including 3Com's sample code, write zeros to both the low and high bytes
 * of the Receive Buffer Pointer when clearing it. BSD drivers (if_el.c)
 * notably only write the low byte. It has been verified on a real 3C501
 * that the documentation is correct. Writing anything to the Receive Buffer
 * Pointer LSB clears the pointer (writing to the MSB appears to have no
 * effect whatsoever).
 *
 * If the Receive Buffer Pointer is not explicitly cleared prior to
 * receiving a packet, it will simply keep incrementing from wherever it
 * was. Once it hits the end of the buffer (wraps around to zero), a
 * receive overflow will be triggered (because the EDLC's FIFO will no
 * longer be serviced) but the buffer will contain however much data there
 * was room for. Note that the SEEQ 8001 datasheet is not explicit, but the
 * EDLC can probably receive frames with more than 1,500 octets of payload.
 *
 * The GP Buffer Pointer behavior is quite curious. It appears to be
 * internally a 12-bit pointer, and its top bit (that is, bit 11) is ignored
 * when addressing into the 2K buffer. When writing the MSB, the top 5 bits
 * are masked (always written as zero), i.e. only a 11-bit value can be
 * written. Through auto-increment, the GP Buffer Pointer can reach values
 * that can be read but not written.
 *
 * The implementation was tested for correctness using 3Com's diagnostic
 * utility (3C501.EXE, Version 2.4, 1986 and also DIAGNOSE.COM, Version 2.0,
 * 1983) and "passes diagnose with flying colors". Note that the interrupt
 * test does not pass in V2.3 diagnostics by default because it writes an
 * EOI to port 0F820h instead of 20h, relying on the system board to decode
 * only the low 10 bits of the address. PCI-based systems decode all address
 * bits and writes to address 0F820h do not reach the interrupt controller.
 * The 3C501.EXE utility can be run with the '-i' switch to skip interrupt
 * tests; the older DIAGNOSE.COM does not have that problem. In both
 * versions, the preliminary test fails if the MAC address OID is not
 * 02:60:8C (the utility thinks the PROM is corrupted).
 *
 * 3Com's XNS driver (ETH.SYS) likewise requires the OID to be 02:60:8C,
 * otherwise the driver uses 00:00:00:00:00:00 as its MAC address, which is
 * not something that produces useful results. Most old drivers (NetWare,
 * NDIS, XENIX) don't care about the OID, but some (BSDs, Linux, some SCO
 * UNIX versions) want to see the 3Com OID.
 *
 * The MS Networks Client setup also requires the OID to match 3Com's when
 * detecting the hardware, but the actual NDIS driver does not care. Note
 * that the setup fails to detect the emulated 3C501 at the default 0x300
 * base address, but finds it at 0x310 and other addresses.
 *
 * Note that especially newer Linux/BSD OSes are a lost cause. Their 3C501
 * drivers are very hard to configure, broken in various ways, and likely
 * untested. For example the Linux driver clears the receive buffer pointer
 * at the end of the interrupt handler, which may easily happen after a
 * packet was already received. In FreeBSD 6.4, the kernel crashes when the
 * el0 driver is loaded. In FreeBSD 5.0, the el0 driver sends packets and
 * reads packets from the card, but the OS never sees any incoming data
 * (even though the receive packet counter keeps going up).
 *
 * The precise receive logic (when a packet is copied to the buffer, when an
 * interrupt is signaled, when receive goes idle) is difficult to understand
 * from the 3Com documentation, but is extensively tested by the diagnostic
 * utility. The SEEQ 8001 datasheet may be easier to understand than the
 * EtherLink documentation.
 *
 * Some drivers (e.g. NetWare DOS IPX shell and ODI drivers) like to reset
 * the chip more or less after every packet is sent or received. That leads
 * to a situation where the NIC is briefly unable to receive anything. If we
 * drop packets in that case, we end up with well over 10% packet loss and
 * terrible performance. We have to hold off and not drop packets just
 * because the receiver is disabled for a moment.
 *
 * Note that the reset bit in the auxiliary command register does not nearly
 * reset the entire chip as the documentation suggests. It may only truly
 * reset the SEEQ 8001 EDLC chip. It is impossible to say how going out of
 * reset affects the auxiliary command register itself, since it must be
 * written to exit the reset state. The reset bit clears the EDLC transmit
 * and command registers, but not the programmed station address. It also
 * does not disturb the packet buffer, and it does not clear the GP Buffer
 * Pointer.
 *
 * The default EtherLink configuration uses I/O base 300h, IRQ 3, DMA
 * channel 1. Prior to May 1983, the default IRQ was 5. On old EtherLink
 * cards, the I/O address was configurable from 200h-3F0h in increments of
 * 16, DMA 1 or 3, and IRQ 3 or 5. Newer EtherLinks (starting circa in 1984)
 * in addition allow DMA 2 and IRQ 2, 4, 6, and 7.
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_ELNK
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pgm.h>
#include <VBox/version.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/net.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/semaphore.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

#define ELNK_SAVEDSTATE_VERSION         1

/** Maximum number of times we report a link down to the guest (failure to send frame) */
#define ELNK_MAX_LINKDOWN_REPORTED      3

/** Maximum frame size we handle */
#define MAX_FRAME                       1536

#define ELNKSTATE_2_DEVINS(pThis)       ((pThis)->CTX_SUFF(pDevIns))
#define ELNK_INSTANCE                   (ELNKSTATE_2_DEVINS(pThis)->iInstance)

/* Size of the packet buffer. */
#define ELNK_BUF_SIZE       2048u

/* The packet buffer address mask. */
#define ELNK_BUF_ADR_MASK   (ELNK_BUF_SIZE - 1)

/* The GP buffer pointer address within the buffer. */
#define ELNK_GP(pThis)      ((pThis)->uGPBufPtr & ELNK_BUF_ADR_MASK)

/* The GP buffer pointer mask.
 * NB: The GP buffer pointer is internally a 12-bit counter. When addressing into the
 * packet buffer, bit 11 is ignored. Required to pass 3C501 diagnostics.
 */
#define ELNK_GP_MASK        0xfff

/* The EtherLink is an 8-bit adapter, hence DMA channels up to 3 are available. */
#define ELNK_MAX_VALID_DMA  3


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/**
 *  EtherLink Transmit Command Register.
 */
typedef struct ELNK_XMIT_CMD {
    uint8_t     det_ufl   : 1;  /* Detect underflow. */
    uint8_t     det_coll  : 1;  /* Detect collision. */
    uint8_t     det_16col : 1;  /* Detect collision 16. */
    uint8_t     det_succ  : 1;  /* Detect successful xmit. */
    uint8_t     unused    : 4;
} EL_XMT_CMD;

/**
 *  EtherLink Transmit Status Register.
 *
 *  We will never see any real collisions, although collisions (including 16
 *  successive collisions) may be useful to report when the link is down
 *  (something the 3C501 does not have a concept of).
 */
typedef struct ELNK_XMIT_STAT {
    uint8_t     uflow  : 1;     /* Underflow on transmit. */
    uint8_t     coll   : 1;     /* Collision on transmit. */
    uint8_t     coll16 : 1;     /* 16 collisions on transmit. */
    uint8_t     ready  : 1;     /* Ready for a new frame. */
    uint8_t     undef  : 4;
} EL_XMT_STAT;

/** Address match (adr_match) modes. */
typedef enum {
    EL_ADRM_DISABLED    = 0,        /* Receiver disabled. */
    EL_ADRM_PROMISC     = 1,        /* Receive all addresses. */
    EL_ADRM_BCAST       = 2,        /* Receive station + broadcast. */
    EL_ADRM_MCAST       = 3         /* Receive station + multicast. */
} EL_ADDR_MATCH;

/**
 *  EtherLink Receive Command Register.
 */
typedef struct ELNK_RECV_CMD {
    uint8_t     det_ofl   : 1;  /* Detect overflow errors. */
    uint8_t     det_fcs   : 1;  /* Detect FCS errors. */
    uint8_t     det_drbl  : 1;  /* Detect dribble error. */
    uint8_t     det_runt  : 1;  /* Detect short frames. */
    uint8_t     det_eof   : 1;  /* Detect EOF (frames without overflow). */
    uint8_t     acpt_good : 1;  /* Accept good frames. */
    uint8_t     adr_match : 2;  /* Address match mode. */
} EL_RCV_CMD;

/**
 *  EtherLink Receive Status Register.
 */
typedef struct ELNK_RECV_STAT {
    uint8_t     oflow   : 1;    /* Overflow on receive. */
    uint8_t     fcs     : 1;    /* FCS error. */
    uint8_t     dribble : 1;    /* Dribble error. */
    uint8_t     runt    : 1;    /* Short frame. */
    uint8_t     no_ovf  : 1;    /* Received packet w/o overflow. */
    uint8_t     good    : 1;    /* Received good packet. */
    uint8_t     undef   : 1;
    uint8_t     stale   : 1;    /* Stale receive status. */
} EL_RCV_STAT;

/** Buffer control (buf_ctl) modes. */
typedef enum {
    EL_BCTL_SYSTEM      = 0,        /* Host has buffer access. */
    EL_BCTL_XMT_RCV     = 1,        /* Transmit, then receive. */
    EL_BCTL_RECEIVE     = 2,        /* Receive. */
    EL_BCTL_LOOPBACK    = 3         /* Loopback. */
} EL_BUFFER_CONTROL;

/**
 *  EtherLink Auxiliary Status Register.
 */
typedef struct ELNK_AUX_CMD {
    uint8_t     ire      : 1;   /* Interrupt Request Enable. */
    uint8_t     xmit_bf  : 1;   /* Xmit packets with bad FCS. */
    uint8_t     buf_ctl  : 2;   /* Packet buffer control. */
    uint8_t     unused   : 1;
    uint8_t     dma_req  : 1;   /* DMA request. */
    uint8_t     ride     : 1;   /* Request Interrupt and DMA Enable. */
    uint8_t     reset    : 1;   /* Card in reset while set. */
} EL_AUX_CMD;

/**
 *  EtherLink Auxiliary Status Register.
 */
typedef struct ELNK_AUX_STAT {
    uint8_t     recv_bsy : 1;   /* Receive busy. */
    uint8_t     xmit_bf  : 1;   /* Xmit packets with bad FCS. */
    uint8_t     buf_ctl  : 2;   /* Packet buffer control. */
    uint8_t     dma_done : 1;   /* DMA done. */
    uint8_t     dma_req  : 1;   /* DMA request. */
    uint8_t     ride     : 1;   /* Request Interrupt and DMA Enable. */
    uint8_t     xmit_bsy : 1;   /* Transmit busy. */
} EL_AUX_STAT;

/**
 *  Internal interrupt status.
 */
typedef struct ELNK_INTR_STAT {
    uint8_t     recv_intr  : 1; /* Receive interrupt status. */
    uint8_t     xmit_intr  : 1; /* Transmit interrupt status. */
    uint8_t     dma_intr   : 1; /* DMA interrupt status. */
    uint8_t     unused     : 5;
} EL_INTR_STAT;


/**
 * EtherLink 3C501 state.
 *
 * @extends     PDMPCIDEV
 * @implements  PDMIBASE
 * @implements  PDMINETWORKDOWN
 * @implements  PDMINETWORKCONFIG
 * @implements  PDMILEDPORTS
 */
typedef struct ELNKSTATE
{
    /** Pointer to the device instance - R3. */
    PPDMDEVINSR3                        pDevInsR3;
    /** Pointer to the connector of the attached network driver - R3. */
    PPDMINETWORKUPR3                    pDrvR3;
    /** Pointer to the attached network driver. */
    R3PTRTYPE(PPDMIBASE)                pDrvBase;
    /** LUN\#0 + status LUN: The base interface. */
    PDMIBASE                            IBase;
    /** LUN\#0: The network port interface. */
    PDMINETWORKDOWN                     INetworkDown;
    /** LUN\#0: The network config port interface. */
    PDMINETWORKCONFIG                   INetworkConfig;
    /** Restore timer.
     *  This is used to disconnect and reconnect the link after a restore. */
    TMTIMERHANDLE                       hTimerRestore;

    /** Pointer to the device instance - R0. */
    PPDMDEVINSR0                        pDevInsR0;
    /** Pointer to the connector of the attached network driver - R0. */
    PPDMINETWORKUPR0                    pDrvR0;

    /** Pointer to the device instance - RC. */
    PPDMDEVINSRC                        pDevInsRC;
    /** Pointer to the connector of the attached network driver - RC. */
    PPDMINETWORKUPRC                    pDrvRC;

    /** Transmit signaller. */
    PDMTASKHANDLE                       hXmitTask;
    /** Receive ready signaller. */
    PDMTASKHANDLE                       hCanRxTask;

    /** Internal interrupt flag. */
    bool                                fISR;
    /** Internal DMA active flag. */
    bool                                fDMA;
    /** Internal in-reset flag. */
    bool                                fInReset;

    /** The PROM contents. Only 8 bytes addressable, R/O. */
    uint8_t                             aPROM[8];

    /** The station address programmed by the guest, W/O. */
    uint8_t                             aStationAddr[6];
    /** General Purpose (GP) Buffer Pointer, R/W. */
    uint16_t                            uGPBufPtr;

    /** Receive (RCV) Buffer Pointer, R/WC. */
    uint16_t                            uRCVBufPtr;
    /** Transmit Command Register, W/O. */
    union {
        uint8_t                         XmitCmdReg;
        EL_XMT_CMD                      XmitCmd;
    };
    /** Transmit Status Register, R/O. */
    union {
        uint8_t                         XmitStatReg;
        EL_XMT_STAT                     XmitStat;
    };
    /** Receive Command Register, W/O. */
    union {
        uint8_t                         RcvCmdReg;
        EL_RCV_CMD                      RcvCmd;
    };
    /** Receive Status Register, R/O. */
    union {
        uint8_t                         RcvStatReg;
        EL_RCV_STAT                     RcvStat;
    };
    /** Auxiliary Command Register, W/O. */
    union {
        uint8_t                         AuxCmdReg;
        EL_AUX_CMD                      AuxCmd;
    };
    /** Auxiliary Status Register, R/O. */
    union {
        uint8_t                         AuxStatReg;
        EL_AUX_STAT                     AuxStat;
    };

    /** Base port of the I/O space region. */
    RTIOPORT                            IOPortBase;
    /** The configured ISA IRQ. */
    uint8_t                             uIsaIrq;
    /** The configured ISA DMA channel. */
    uint8_t                             uIsaDma;
    /** If set the link is currently up. */
    bool                                fLinkUp;
    /** If set the link is temporarily down because of a saved state load. */
    bool                                fLinkTempDown;
    /** Number of times we've reported the link down. */
    uint16_t                            cLinkDownReported;

    /** The "hardware" MAC address. */
    RTMAC                               MacConfigured;
    /** Internal interrupt state. */
    union {
        uint8_t                         IntrStateReg;
        EL_INTR_STAT                    IntrState;
    };

    /** The LED. */
    PDMLED                              Led;
    /** Status LUN: The LED ports. */
    PDMILEDPORTS                        ILeds;
    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)       pLedsConnector;

    /** Access critical section. */
    PDMCRITSECT                         CritSect;
    /** Event semaphore for blocking on receive. */
    RTSEMEVENT                          hEventOutOfRxSpace;
    /** We are waiting/about to start waiting for more receive buffers. */
    bool volatile                       fMaybeOutOfSpace;

    /* MS to wait before we enable the link. */
    uint32_t                            cMsLinkUpDelay;
    /** The device instance number (for logging). */
    uint32_t                            iInstance;

    STAMCOUNTER                         StatReceiveBytes;
    STAMCOUNTER                         StatTransmitBytes;
    STAMCOUNTER                         StatPktsLostReset;
#ifdef VBOX_WITH_STATISTICS
    STAMPROFILEADV                      StatIOReadRZ;
    STAMPROFILEADV                      StatIOReadR3;
    STAMPROFILEADV                      StatIOWriteRZ;
    STAMPROFILEADV                      StatIOWriteR3;
    STAMPROFILEADV                      StatReceive;
    STAMPROFILEADV                      StatTransmitR3;
    STAMPROFILEADV                      StatTransmitRZ;
    STAMPROFILE                         StatTransmitSendR3;
    STAMPROFILE                         StatTransmitSendRZ;
    STAMPROFILE                         StatRxOverflow;
    STAMCOUNTER                         StatRxOverflowWakeup;
    STAMPROFILEADV                      StatInterrupt;
    STAMCOUNTER                         StatResets;
    STAMCOUNTER                         StatDropPktAdrmDis;
    STAMCOUNTER                         StatDropPktZeroLen;
    STAMCOUNTER                         StatDropPktVMNotRunning;
    STAMCOUNTER                         StatDropPktNoLink;
    STAMCOUNTER                         StatDropPktStaleRcv;
#endif /* VBOX_WITH_STATISTICS */

    /** ISA I/O ports. */
    IOMIOPORTHANDLE                     hIoPortsIsa;

    /** The loopback transmit buffer (avoid stack allocations). */
    uint8_t                             abLoopBuf[ELNK_BUF_SIZE];

    /** The runt pad buffer (only really needs 60 bytes). */
    uint8_t                             abRuntBuf[64];

    /** The packet buffer. */
    uint8_t                             abPacketBuf[ELNK_BUF_SIZE];
} ELNKSTATE, *PELNKSTATE;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static int elnkAsyncTransmit(PPDMDEVINS pDevIns, PELNKSTATE pThis, bool fOnWorkerThread);

/**
 * Checks if the link is up.
 * @returns true if the link is up.
 * @returns false if the link is down.
 */
DECLINLINE(bool) elnkIsLinkUp(PELNKSTATE pThis)
{
    return pThis->pDrvR3 && !pThis->fLinkTempDown && pThis->fLinkUp;
}


#ifndef ETHER_IS_MULTICAST /* Net/Open BSD macro it seems */
#define ETHER_IS_MULTICAST(a) ((*(uint8_t *)(a)) & 1)
#endif

#define ETHER_ADDR_LEN ETH_ALEN
#define ETH_ALEN 6
#pragma pack(1)
struct ether_header /** @todo Use RTNETETHERHDR? */
{
    uint8_t  ether_dhost[ETH_ALEN]; /**< destination ethernet address */
    uint8_t  ether_shost[ETH_ALEN]; /**< source ethernet address */
    uint16_t ether_type;            /**< packet type ID field */
};
#pragma pack()


/**
 * Check if incoming frame matches the station address.
 */
DECLINLINE(int) padr_match(PELNKSTATE pThis, const uint8_t *buf)
{
    struct  ether_header *hdr = (struct ether_header *)buf;
    int     result;

    /* Checks own + broadcast as well as own + multicast. */
    result = (pThis->RcvCmd.adr_match >= EL_ADRM_BCAST) && !memcmp(hdr->ether_dhost, pThis->aStationAddr, 6);

    return result;
}


/**
 * Check if incoming frame is an accepted broadcast frame.
 */
DECLINLINE(int) padr_bcast(PELNKSTATE pThis, const uint8_t *buf)
{
    static uint8_t aBCAST[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    struct ether_header *hdr = (struct ether_header *)buf;
    int result = (pThis->RcvCmd.adr_match == EL_ADRM_BCAST) && !memcmp(hdr->ether_dhost, aBCAST, 6);
    return result;
}


/**
 * Check if incoming frame is an accepted multicast frame.
 */
DECLINLINE(int) padr_mcast(PELNKSTATE pThis, const uint8_t *buf)
{
    struct ether_header *hdr = (struct ether_header *)buf;
    int result = (pThis->RcvCmd.adr_match == EL_ADRM_MCAST) && ETHER_IS_MULTICAST(hdr->ether_dhost);
    return result;
}


/**
 * Update the device IRQ line based on internal state.
 */
static void elnkUpdateIrq(PELNKSTATE pThis)
{
    bool     fISR = false;

    STAM_PROFILE_ADV_START(&pThis->StatInterrupt, a);

    /* IRQ is active if any interrupt source is active and interrupts
     * are enabled via RIDE or IRE.
     */
    if (pThis->IntrStateReg && (pThis->AuxCmd.ride || pThis->AuxCmd.ire))
        fISR = true;

    Log2(("#%d set irq fISR=%d\n", ELNK_INSTANCE, fISR));

    /* The IRQ line typically does not change. */
    if (RT_UNLIKELY(fISR != pThis->fISR))
    {
        Log(("#%d IRQ=%d, state=%d\n", ELNK_INSTANCE, pThis->uIsaIrq, fISR));
        PDMDevHlpISASetIrq(ELNKSTATE_2_DEVINS(pThis), pThis->uIsaIrq, fISR);
        pThis->fISR = fISR;
    }
    STAM_PROFILE_ADV_STOP(&pThis->StatInterrupt, a);
}


/**
 * Perform a software reset of the NIC.
 */
static void elnkSoftReset(PELNKSTATE pThis)
{
    LogFlow(("#%d elnkSoftReset:\n", ELNK_INSTANCE));

    /* Clear some of the user-visible register state. */
    pThis->XmitCmdReg  = 0;
    pThis->XmitStatReg = 0;
    pThis->RcvCmdReg   = 0;
    pThis->RcvStatReg  = 0;
    pThis->AuxCmdReg   = 0;
    pThis->AuxStatReg  = 0;

    /* The "stale receive status" is cleared by receiving an "interesting" packet. */
    pThis->RcvStat.stale = 1;

    /* By virtue of setting the buffer control to system, transmit is set to busy. */
    pThis->AuxStat.xmit_bsy = 1;

    /* Clear internal interrupt state. */
    pThis->IntrStateReg = 0;
    elnkUpdateIrq(pThis);

    /* Note that a soft reset does not clear the packet buffer; software often
     * assumes that it survives soft reset. The programmed station address is
     * likewise not reset, and the buffer pointers are not reset either.
     * Verified on a real 3C501.
     */

    /* No longer in reset state. */
    pThis->fInReset = false;
}

#ifdef IN_RING3

static DECLCALLBACK(void) elnkR3WakeupReceive(PPDMDEVINS pDevIns)
{
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    STAM_COUNTER_INC(&pThis->StatRxOverflowWakeup);
    if (pThis->hEventOutOfRxSpace != NIL_RTSEMEVENT)
        RTSemEventSignal(pThis->hEventOutOfRxSpace);
}

/**
 * @callback_method_impl{FNPDMTASKDEV,
 * Signal to R3 that NIC is ready to receive a packet.
 */
static DECLCALLBACK(void) elnkR3CanRxTaskCallback(PPDMDEVINS pDevIns, void *pvUser)
{
    RT_NOREF(pvUser);
    elnkR3WakeupReceive(pDevIns);
}

#endif /* IN_RING3 */


/**
 * Write incoming data into the packet buffer.
 */
static void elnkReceiveLocked(PELNKSTATE pThis, const uint8_t *src, size_t cbToRecv, bool fLoopback)
{
    PPDMDEVINS pDevIns = ELNKSTATE_2_DEVINS(pThis);
    int is_padr = 0, is_bcast = 0, is_mcast = 0;
    union {
        uint8_t     RcvStatNewReg;
        EL_RCV_STAT RcvStatNew;
    };

    /*
     * Drop all packets if the VM is not running yet/anymore.
     */
    VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
    if (    enmVMState != VMSTATE_RUNNING
        &&  enmVMState != VMSTATE_RUNNING_LS)
    {
        STAM_COUNTER_INC(&pThis->StatDropPktVMNotRunning);
        return;
    }

    /* Drop everything if address matching is disabled. */
    if (RT_UNLIKELY(pThis->RcvCmd.adr_match == EL_ADRM_DISABLED))
    {
        STAM_COUNTER_INC(&pThis->StatDropPktAdrmDis);
        return;
    }

    /* Drop zero-length packets (how does that even happen?). */
    if (RT_UNLIKELY(!cbToRecv))
    {
        STAM_COUNTER_INC(&pThis->StatDropPktZeroLen);
        return;
    }

    /*
     * Drop all packets if the cable is not connected (and not in loopback).
     */
    if (RT_UNLIKELY(!elnkIsLinkUp(pThis) && !fLoopback))
    {
        STAM_COUNTER_INC(&pThis->StatDropPktNoLink);
        return;
    }

    /*
     * Do not receive further packets until receive status was read.
     */
    if (RT_UNLIKELY(pThis->RcvStat.stale == 0))
    {
        STAM_COUNTER_INC(&pThis->StatDropPktStaleRcv);
        return;
    }

    LogFlowFunc(("#%d: size on wire=%d, RCV ptr=%u\n", ELNK_INSTANCE, cbToRecv, pThis->uRCVBufPtr));

    /*
     * Perform address matching. Packets which do not pass the address
     * filter are always ignored.
     */
    /// @todo cbToRecv must be 6 or more (complete address)
    if (   pThis->RcvCmd.adr_match == EL_ADRM_PROMISC   /* promiscuous enabled */
        || (is_padr  = padr_match(pThis, src))
        || (is_bcast = padr_bcast(pThis, src))
        || (is_mcast = padr_mcast(pThis, src)))
    {
        uint8_t     *dst = pThis->abPacketBuf + pThis->uRCVBufPtr;

        Log2Func(("#%d Packet passed address filter (is_padr=%d, is_bcast=%d, is_mcast=%d), size=%d\n", ELNK_INSTANCE, cbToRecv, is_padr, is_bcast, is_mcast));

        /* Receive status is evaluated from scratch. The stale bit must remain set until we know better. */
        RcvStatNewReg = 0;
        RcvStatNew.stale = 1;
        pThis->RcvStatReg = 0x80;

        /* Detect errors: Runts, overflow, and FCS errors.
         * NB: Dribble errors can not happen because we can only receive an
         * integral number of bytes. FCS errors are only possible in loopback
         * mode in case the FCS is deliberately corrupted.
         */

        /* See if we need to pad, and how much. Have to be careful because the
         * Receive Buffer Pointer might be near the end of the buffer.
         */
        if (RT_UNLIKELY(cbToRecv < 60))
        {
            /* In loopback mode only, short packets are flagged as errors because
             * diagnostic tools want to see the errors. Otherwise they're padded to
             * minimum length (if packet came over the wire, it should have been
             * properly padded).
             */
            /// @todo This really is kind of wrong. We shouldn't be doing any
            /// padding here, it should be done by the sending side!
            if (!fLoopback)
            {
                memset(pThis->abRuntBuf, 0, sizeof(pThis->abRuntBuf));
                memcpy(pThis->abRuntBuf, src, cbToRecv);
                cbToRecv = 60;
                src = pThis->abRuntBuf;
            }
            else
            {
                LogFunc(("#%d runt, size=%d\n", ELNK_INSTANCE, cbToRecv));
                RcvStatNew.runt = 1;
            }
        }

        /* We don't care how big the frame is; if it fits into the buffer, all is
         * good. But conversely if the Receive Buffer Pointer is initially near the
         * end of the buffer, a small frame can trigger an overflow.
         */
        if (pThis->uRCVBufPtr + cbToRecv <= ELNK_BUF_SIZE)
        {
            RcvStatNew.no_ovf = 1;
        }
        else
        {
            LogFunc(("#%d overflow, size=%d\n", ELNK_INSTANCE, cbToRecv));
            RcvStatNew.oflow = 1;
        }

        if (fLoopback && pThis->AuxCmd.xmit_bf)
        {
            LogFunc(("#%d bad FCS\n", ELNK_INSTANCE));
            RcvStatNew.fcs = 1;
        }

        /* Error-free packets are considered good. */
        if (RcvStatNew.no_ovf && !RcvStatNew.fcs && !RcvStatNew.runt)
            RcvStatNew.good = 1;

        uint16_t    cbCopy = (uint16_t)RT_MIN(ELNK_BUF_SIZE - pThis->uRCVBufPtr, cbToRecv);

        /* All packets that passed the address filter are copied to the buffer. */

        STAM_REL_COUNTER_ADD(&pThis->StatReceiveBytes, cbCopy);

        /* Copy incoming data to the packet buffer. NB: Starts at the current
         * Receive Buffer Pointer position.
         */
        memcpy(dst, src, cbCopy);

        /* Packet length is indicated via the receive buffer pointer. */
        pThis->uRCVBufPtr = (pThis->uRCVBufPtr + cbCopy) & ELNK_GP_MASK;

        Log2Func(("Received packet, size=%d, RP=%u\n", cbCopy, pThis->uRCVBufPtr));

        /*
         * If one of the "interesting" conditions was hit, stop receiving until
         * the status register is read (mark it not stale).
         * NB: The precise receive logic is not very well described in the EtherLink
         * documentation. It was refined using the 3C501.EXE diagnostic utility.
         */
        if (   (RcvStatNew.good    && pThis->RcvCmd.acpt_good)
            || (RcvStatNew.no_ovf  && pThis->RcvCmd.det_eof)
            || (RcvStatNew.runt    && pThis->RcvCmd.det_runt)
            || (RcvStatNew.dribble && pThis->RcvCmd.det_drbl)
            || (RcvStatNew.fcs     && pThis->RcvCmd.det_fcs)
            || (RcvStatNew.oflow   && pThis->RcvCmd.det_ofl))
        {
            pThis->AuxStat.recv_bsy    = 0;
            pThis->IntrState.recv_intr = 1;
            RcvStatNew.stale           = 0; /* Prevents further receive until set again. */
        }
        /* Finally update the receive status. */
        pThis->RcvStat = RcvStatNew;

        LogFlowFunc(("#%d: RcvCmd=%02X, RcvStat=%02X, RCVBufPtr=%u\n", ELNK_INSTANCE, pThis->RcvCmdReg, pThis->RcvStatReg, pThis->uRCVBufPtr));
        elnkUpdateIrq(pThis);
    }
}


/**
 * Transmit data from the packet buffer.
 *
 * @returns VBox status code.  VERR_TRY_AGAIN is returned if we're busy.
 *
 * @param   pThis               The device instance data.
 * @param   fOnWorkerThread     Whether we're on a worker thread or on an EMT.
 */
static int elnkXmitBuffer(PELNKSTATE pThis, bool fOnWorkerThread)
{
    RT_NOREF_PV(fOnWorkerThread);
    int rc;

    /*
     * Grab the xmit lock of the driver as well as the 3C501 device state.
     */
    PPDMINETWORKUP pDrv = pThis->CTX_SUFF(pDrv);
    if (pDrv)
    {
        rc = pDrv->pfnBeginXmit(pDrv, false /*fOnWorkerThread*/);
        if (RT_FAILURE(rc))
            return rc;
    }
    PPDMDEVINS pDevIns = ELNKSTATE_2_DEVINS(pThis);
    rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    if (RT_SUCCESS(rc))
    {
        /** @todo check if we're supposed to suspend now. */
        /*
         * Do the transmitting.
         */
        int rc2 = elnkAsyncTransmit(pDevIns, pThis, false /*fOnWorkerThread*/);
        AssertReleaseRC(rc2);

        /*
         * Release the locks.
         */
        PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
    }
    else
        AssertLogRelRC(rc);
    if (pDrv)
        pDrv->pfnEndXmit(pDrv);

    return rc;
}


#ifdef IN_RING3

/**
 * @callback_method_impl{FNPDMTASKDEV,
 * This is just a very simple way of delaying sending to R3.
 */
static DECLCALLBACK(void) elnkR3XmitTaskCallback(PPDMDEVINS pDevIns, void *pvUser)
{
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    NOREF(pvUser);

    /*
     * Transmit if we can.
     */
    elnkXmitBuffer(pThis, true /*fOnWorkerThread*/);
}
#endif /* IN_RING3 */


/**
 * Allocates a scatter/gather buffer for a transfer.
 *
 * @returns See PPDMINETWORKUP::pfnAllocBuf.
 * @param   pThis       The device instance.
 * @param   cbMin       The minimum buffer size.
 * @param   fLoopback   Set if we're in loopback mode.
 * @param   pSgLoop     Pointer to stack storage for the loopback SG.
 * @param   ppSgBuf     Where to return the SG buffer descriptor on success.
 *                      Always set.
 */
DECLINLINE(int) elnkXmitAllocBuf(PELNKSTATE pThis, size_t cbMin, bool fLoopback,
                                 PPDMSCATTERGATHER pSgLoop, PPPDMSCATTERGATHER ppSgBuf)
{
    int rc;

    if (!fLoopback)
    {
        PPDMINETWORKUP pDrv = pThis->CTX_SUFF(pDrv);
        if (RT_LIKELY(pDrv))
        {
            rc = pDrv->pfnAllocBuf(pDrv, cbMin, NULL /*pGso*/, ppSgBuf);
            AssertMsg(rc == VINF_SUCCESS || rc == VERR_TRY_AGAIN || rc == VERR_NET_DOWN || rc == VERR_NO_MEMORY, ("%Rrc\n", rc));
            if (RT_FAILURE(rc))
                *ppSgBuf = NULL;
        }
        else
        {
            rc = VERR_NET_DOWN;
            *ppSgBuf = NULL;
        }
    }
    else
    {
        /* Fake loopback allocator. */
        pSgLoop->fFlags      = PDMSCATTERGATHER_FLAGS_MAGIC | PDMSCATTERGATHER_FLAGS_OWNER_1;
        pSgLoop->cbUsed      = 0;
        pSgLoop->cbAvailable = sizeof(pThis->abLoopBuf);
        pSgLoop->pvAllocator = pThis;
        pSgLoop->pvUser      = NULL;
        pSgLoop->cSegs       = 1;
        pSgLoop->aSegs[0].cbSeg = sizeof(pThis->abLoopBuf);
        pSgLoop->aSegs[0].pvSeg = pThis->abLoopBuf;
        *ppSgBuf = pSgLoop;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/**
 * Frees an unsent buffer.
 *
 * @param   pThis           The device instance.
 * @param   fLoopback       Set if we're in loopback mode.
 * @param   pSgBuf          The SG to free.  Can be NULL.
 */
DECLINLINE(void) elnkXmitFreeBuf(PELNKSTATE pThis, bool fLoopback, PPDMSCATTERGATHER pSgBuf)
{
    if (pSgBuf)
    {
        if (RT_UNLIKELY(fLoopback))
            pSgBuf->pvAllocator = NULL;
        else
        {
            PPDMINETWORKUP pDrv = pThis->CTX_SUFF(pDrv);
            if (RT_LIKELY(pDrv))
                pDrv->pfnFreeBuf(pDrv, pSgBuf);
        }
    }
}


/**
 * Sends the scatter/gather buffer.
 *
 * Wrapper around PDMINETWORKUP::pfnSendBuf, so check it out for the fine print.
 *
 * @returns See PDMINETWORKUP::pfnSendBuf.
 * @param   pThis           The device instance.
 * @param   fLoopback       Set if we're in loopback mode.
 * @param   pSgBuf          The SG to send.
 * @param   fOnWorkerThread Set if we're being called on a work thread.  Clear
 *                          if an EMT.
 */
DECLINLINE(int) elnkXmitSendBuf(PELNKSTATE pThis, bool fLoopback, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
    int rc;
    STAM_REL_COUNTER_ADD(&pThis->StatTransmitBytes, pSgBuf->cbUsed);
    if (!fLoopback)
    {
        STAM_PROFILE_START(&pThis->CTX_SUFF_Z(StatTransmitSend), a);
        if (pSgBuf->cbUsed > 70) /* unqualified guess */
            pThis->Led.Asserted.s.fWriting = pThis->Led.Actual.s.fWriting = 1;

        PPDMINETWORKUP pDrv = pThis->CTX_SUFF(pDrv);
        if (RT_LIKELY(pDrv))
        {
            rc = pDrv->pfnSendBuf(pDrv, pSgBuf, fOnWorkerThread);
            AssertMsg(rc == VINF_SUCCESS || rc == VERR_NET_DOWN || rc == VERR_NET_NO_BUFFER_SPACE, ("%Rrc\n", rc));
        }
        else
            rc = VERR_NET_DOWN;

        pThis->Led.Actual.s.fWriting = 0;
        STAM_PROFILE_STOP(&pThis->CTX_SUFF_Z(StatTransmitSend), a);
    }
    else
    {
        /* Loopback, immediately send buffer to the receive path. */
        Assert(pSgBuf->pvAllocator == (void *)pThis);
        pThis->Led.Asserted.s.fReading = pThis->Led.Actual.s.fReading = 1;

        LogFlowFunc(("#%d: loopback (%u bytes)\n", ELNK_INSTANCE, pSgBuf->cbUsed));
        elnkReceiveLocked(pThis, pThis->abLoopBuf, pSgBuf->cbUsed, fLoopback);
        pThis->Led.Actual.s.fReading = 0;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/**
 * Reads the entire frame into the scatter gather buffer.
 */
DECLINLINE(void) elnkXmitRead(PPDMDEVINS pDevIns, PELNKSTATE pThis, const unsigned cbFrame, PPDMSCATTERGATHER pSgBuf)
{
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect)); RT_NOREF(pDevIns);
    Assert(pSgBuf->cbAvailable >= cbFrame);

    pSgBuf->cbUsed = cbFrame;
    memcpy(pSgBuf->aSegs[0].pvSeg, &pThis->abPacketBuf[ELNK_GP(pThis)], cbFrame);
}

/**
 * Try to transmit a frame.
 */
static void elnkTransmit(PELNKSTATE pThis)
{
    /*
     * Transmit the packet if possible, defer it if we cannot do it
     * in the current context.
     */
#if defined(IN_RING0) || defined(IN_RC)
    if (!pThis->CTX_SUFF(pDrv))
    {
        int rc = PDMDevHlpTaskTrigger(pThis->CTX_SUFF(pDevIns), pThis->hXmitTask);
        AssertRC(rc);
    }
    else
#endif
    {
        int rc = elnkXmitBuffer(pThis, false /*fOnWorkerThread*/);
        if (rc == VERR_TRY_AGAIN)
            rc = VINF_SUCCESS;
        AssertRC(rc);
    }
}


/**
 * If a packet is waiting, poke the receiving machinery.
 *
 * @threads EMT.
 */
static void elnkKickReceive(PELNKSTATE pThis)
{
    /* Some drivers (e.g. NetWare IPX shell/ODI drivers) first go to receive mode through
     * the aux command register and only then enable address matching.
     */
    if ((pThis->AuxStat.recv_bsy == 1) && (pThis->RcvCmd.adr_match != EL_ADRM_DISABLED))
    {
        if (pThis->fMaybeOutOfSpace)
        {
#ifdef IN_RING3
            elnkR3WakeupReceive(ELNKSTATE_2_DEVINS(pThis));
#else
            int rc = PDMDevHlpTaskTrigger(pThis->CTX_SUFF(pDevIns), pThis->hCanRxTask);
            AssertRC(rc);
#endif
        }
    }

}

/**
 * Try transmitting a frame.
 *
 * @threads TX or EMT.
 */
static int elnkAsyncTransmit(PPDMDEVINS pDevIns, PELNKSTATE pThis, bool fOnWorkerThread)
{
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));

    /*
     * Just drop it if not transmitting. Can happen with delayed transmits
     * if transmit was disabled in the meantime.
     */
    if (RT_UNLIKELY(!pThis->AuxStat.xmit_bsy))
    {
        LogFunc(("#%d: Nope, xmit disabled (fOnWorkerThread=%RTbool)\n", ELNK_INSTANCE, fOnWorkerThread));
        return VINF_SUCCESS;
    }

    if (RT_UNLIKELY((pThis->AuxCmd.buf_ctl != EL_BCTL_XMT_RCV) && (pThis->AuxCmd.buf_ctl != EL_BCTL_LOOPBACK)))
    {
        LogFunc(("#%d: Nope, not in xmit-then-receive or loopback state (fOnWorkerThread=%RTbool)\n", ELNK_INSTANCE, fOnWorkerThread));
        return VINF_SUCCESS;
    }

    /*
     * Blast out data from the packet buffer.
     */
    int         rc;
    STAM_PROFILE_ADV_START(&pThis->CTX_SUFF_Z(StatTransmit), a);
    do
    {
        /* Don't send anything when the link is down. */
        if (RT_UNLIKELY(   !elnkIsLinkUp(pThis)
                        &&  pThis->cLinkDownReported > ELNK_MAX_LINKDOWN_REPORTED)
            )
            break;

        bool const          fLoopback = pThis->AuxCmd.buf_ctl == EL_BCTL_LOOPBACK;
        PDMSCATTERGATHER    SgLoop;
        PPDMSCATTERGATHER   pSgBuf;

        /*
         * Sending is easy peasy, there is by definition always
         * a complete packet on hand.
         */
        const unsigned cb = ELNK_BUF_SIZE - ELNK_GP(pThis); /* Packet size. */
        Log(("#%d elnkAsyncTransmit: cb=%d\n", ELNK_INSTANCE, cb));

        pThis->XmitStatReg = 0; /* Clear transmit status before filling it out. */

        if (RT_LIKELY(elnkIsLinkUp(pThis) || fLoopback))
        {
            if (RT_LIKELY(cb <= MAX_FRAME))
            {
                rc = elnkXmitAllocBuf(pThis, cb, fLoopback, &SgLoop, &pSgBuf);
                if (RT_SUCCESS(rc))
                {
                    elnkXmitRead(pDevIns, pThis, cb, pSgBuf);
                    rc = elnkXmitSendBuf(pThis, fLoopback, pSgBuf, fOnWorkerThread);
                    Log2(("#%d elnkAsyncTransmit: rc=%Rrc\n", ELNK_INSTANCE, rc));
                }
                else if (rc == VERR_TRY_AGAIN)
                {
                    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF_Z(StatTransmit), a);
                    Log(("#%d elnkAsyncTransmit: rc=%Rrc\n", ELNK_INSTANCE, rc));
                    return VINF_SUCCESS;
                }
                if (RT_SUCCESS(rc))
                    pThis->XmitStat.ready = 1;
                else
                    pThis->XmitStat.coll = 1;   /* Pretend there was a collision. */
            }
            else
            {
                /* Signal error, as this violates the Ethernet specs. */
                /** @todo check if the correct error is generated. */
                LogRel(("3C501#%d: elnkAsyncTransmit: illegal giant frame (%u bytes) -> signalling error\n", ELNK_INSTANCE, cb));
            }
        }
        else
        {
            /* Signal a transmit error pretending there was a collision. */
            pThis->XmitStat.coll = 1;
        }
        /* Transmit officially done, update register state. */
        pThis->AuxStat.xmit_bsy    = 0;
        pThis->IntrState.xmit_intr = !!(pThis->XmitCmdReg & pThis->XmitStatReg);
        LogFlowFunc(("#%d: XmitCmd=%02X, XmitStat=%02X\n", ELNK_INSTANCE, pThis->XmitCmdReg, pThis->XmitStatReg));

        /* NB: After a transmit, the GP Buffer Pointer points just past
         * the end of the packet buffer (3C501 diagnostics).
         */
        pThis->uGPBufPtr           = ELNK_BUF_SIZE;

        /* NB: The buffer control does *not* change to Receive and stays the way it was. */
        if (RT_UNLIKELY(!fLoopback))
        {
            pThis->AuxStat.recv_bsy = 1;    /* Receive Busy now set until a packet is received. */
            elnkKickReceive(pThis);
        }
    } while (0);    /* No loop, because there isn't ever more than one packet to transmit. */

    elnkUpdateIrq(pThis);

    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF_Z(StatTransmit), a);

    return VINF_SUCCESS;
}

/* -=-=-=-=-=- I/O Port access -=-=-=-=-=- */


static int elnkCsrWrite(PELNKSTATE pThis, uint8_t data)
{
    int         rc = VINF_SUCCESS;
    bool        fTransmit = false;
    bool        fReceive  = false;
    bool        fDMAR;
    union {
        uint8_t     reg;
        EL_AUX_CMD  val;
    };

    reg = data;

    /* Handle reset first. */
    if (pThis->AuxCmd.reset != val.reset)
    {
        if (val.reset)
        {
            /* Card is placed into reset. Just set the flag. NB: When in reset
             * state, we permit writes to other registers, but those have no
             * effect and will be overwritten when the card is taken out of reset.
             */
            Log(("#%d elnkCsrWrite: Card going into reset\n", ELNK_INSTANCE));
            pThis->fInReset = true;

            /* Many EtherLink drivers like to reset the card a lot. That can lead to
             * packet loss if a packet was already received before the card was reset.
             */
            if (RT_UNLIKELY(!pThis->RcvStat.stale))
                STAM_REL_COUNTER_INC(&pThis->StatPktsLostReset);
        }
        else
        {
            /* Card is being taken out of reset. */
            Log(("#%d elnkCsrWrite: Card going out of reset\n", ELNK_INSTANCE));
            STAM_COUNTER_INC(&pThis->StatResets);
            elnkSoftReset(pThis);
        }
        pThis->AuxCmd.reset = val.reset;    /* Update the reset bit, if nothing else. */
    }

    /* If the card is in reset, stop right here. */
    if (pThis->fInReset)
        return rc;

    /* Evaluate DMA state. If it changed, we'll have to go back to R3. */
    fDMAR = val.dma_req && val.ride;
    if (fDMAR != pThis->fDMA)
#ifdef IN_RING3
    {
        /* Start/stop DMA as requested. */
        pThis->fDMA = fDMAR;
        PDMDevHlpDMASetDREQ(pThis->CTX_SUFF(pDevIns), pThis->uIsaDma, fDMAR);
        if (fDMAR)
            PDMDevHlpDMASchedule(pThis->CTX_SUFF(pDevIns));
        Log(("3C501#%d: DMARQ for channel %u set to %u\n", ELNK_INSTANCE, pThis->uIsaDma, fDMAR));
    }
#else
        return VINF_IOM_R3_IOPORT_WRITE;
#endif

    /* Interrupt enable changes. */
    if ((pThis->AuxCmd.ire != val.ire) || (pThis->AuxCmd.ride != val.ride))
    {
        pThis->AuxStat.ride = pThis->AuxCmd.ride = val.ride;
        pThis->AuxCmd.ire   = val.ire;  /* NB: IRE is not visible in the aux status register. */
    }

    /* DMA Request changes. */
    if (pThis->AuxCmd.dma_req != val.dma_req)
    {
        pThis->AuxStat.dma_req = pThis->AuxCmd.dma_req = val.dma_req;
        if (!val.dma_req)
        {
            /* Clearing the DMA Request bit also clears the DMA Done status bit and any DMA interrupt. */
            pThis->IntrState.dma_intr = 0;
            pThis->AuxStat.dma_done   = 0;
        }
    }

    /* Packet buffer control changes. */
    if (pThis->AuxCmd.buf_ctl != val.buf_ctl)
    {
#ifdef LOG_ENABLED
        static const char   *apszBuffCntrl[4] = { "System", "Xmit then Recv", "Receive", "Loopback" };
        Log(("3C501#%d: Packet buffer control `%s' -> `%s'\n", ELNK_INSTANCE, apszBuffCntrl[pThis->AuxCmd.buf_ctl], apszBuffCntrl[val.buf_ctl]));
#endif
        if (val.buf_ctl == EL_BCTL_XMT_RCV)
        {
            /* Transmit, then receive. */
            Log2(("3C501#%d: Transmit %u bytes\n%Rhxs\nxmit_bsy=%u\n", ELNK_INSTANCE, ELNK_BUF_SIZE - pThis->uGPBufPtr, &pThis->abPacketBuf[pThis->uGPBufPtr], pThis->AuxStat.xmit_bsy));
            fTransmit = true;
            pThis->AuxStat.recv_bsy = 0;
        }
        else if (val.buf_ctl == EL_BCTL_SYSTEM)
        {
            pThis->AuxStat.xmit_bsy = 1;    /* Transmit Busy is set here and cleared once actual transmit completes. */
            pThis->AuxStat.recv_bsy = 0;
        }
        else if (val.buf_ctl == EL_BCTL_RECEIVE)
        {
            /* Special case: If going from xmit-then-receive mode to receive mode, and we received
             * a packet already (right after the receive), don't restart receive and lose the already
             * received packet.
             */
            if (!pThis->uRCVBufPtr)
                fReceive = true;
        }
        else
        {
            /* For loopback, we go through the regular transmit and receive path. That may be an
             * overkill but the receive path is too complex for a special loopback-only case.
             */
            fTransmit = true;
            pThis->AuxStat.recv_bsy = 1;    /* Receive Busy now set until a packet is received. */
        }
        pThis->AuxStat.buf_ctl = pThis->AuxCmd.buf_ctl = val.buf_ctl;
    }

    /* NB: Bit 1 (xmit_bf, transmit packets with bad FCS) is a simple control
     * bit which does not require special handling here. Just copy it over.
     */
    pThis->AuxStat.xmit_bf = pThis->AuxCmd.xmit_bf = val.xmit_bf;

    /* There are multiple bits that affect interrupt state. Handle them now. */
    elnkUpdateIrq(pThis);

    /* After fully updating register state, do a transmit (including loopback) or receive. */
    if (fTransmit)
        elnkTransmit(pThis);
    else if (fReceive)
    {
        pThis->AuxStat.recv_bsy = 1;    /* Receive Busy now set until a packet is received. */
        elnkKickReceive(pThis);
    }

    return rc;
}

static int elIoWrite(PELNKSTATE pThis, uint32_t addr, uint32_t val)
{
    int     reg = addr & 0xf;
    int     rc = VINF_SUCCESS;

    Log2(("#%d elIoWrite: addr=%#06x val=%#04x\n", ELNK_INSTANCE, addr, val & 0xff));

    switch (reg)
    {
        case 0x00:  /* Six bytes of station address. */
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
            pThis->aStationAddr[reg] = val;
            break;

        case 0x06:  /* Receive command. */
        {
            EL_RCV_CMD  OldRcvCmd = pThis->RcvCmd;

            pThis->RcvCmdReg = val;
            /* If address filter just got enabled, receive may need a kick. */
            if (OldRcvCmd.adr_match == EL_ADRM_DISABLED && pThis->RcvCmd.adr_match != EL_ADRM_DISABLED)
                elnkKickReceive(pThis);
            Log2(("Receive Command register set to %02X\n", pThis->RcvCmdReg));
            break;
        }

        case 0x07:  /* Transmit command. */
            pThis->XmitCmdReg = val;
            Log2(("Transmit Command register set to %02X\n", pThis->XmitCmdReg));
            break;

        case 0x08:  /* GP Buffer pointer LSB. */
            pThis->uGPBufPtr = (pThis->uGPBufPtr & 0xff00) | (uint8_t)val;
            Log2(("GP Buffer Pointer LSB write, now %u\n", pThis->uGPBufPtr));
            break;

        case 0x09:  /* GP Buffer pointer MSB. */
            pThis->uGPBufPtr = ((uint8_t)val << 8) | RT_LOBYTE(pThis->uGPBufPtr);
            Log2(("GP Buffer Pointer MSB write, now %u\n", pThis->uGPBufPtr));
            break;

        case 0x0a:  /* RCV Buffer pointer clear. */
            pThis->uRCVBufPtr = 0;
            Log2(("RCV Buffer Pointer cleared (%02X)\n", val));
            break;

        case 0x0b:  /* RCV buffer pointer MSB. */
        case 0x0c:  /* Ethernet address PROM window. */
        case 0x0d:  /* Undocumented. */
            Log(("Writing read-only register %02X!\n", reg));
            break;

        case 0x0e:  /* Auxiliary Command (CSR). */
            rc = elnkCsrWrite(pThis, val);
            break;

        case 0x0f:  /* Buffer window. */
            /* Writes use low 11 bits of GP buffer pointer, auto-increment. */
            if (pThis->AuxCmd.buf_ctl != EL_BCTL_SYSTEM)
            {
                Log(("Packet buffer write ignored, buf_ctl=%u!\n", pThis->AuxCmd.buf_ctl));
                /// @todo Does this still increment GPBufPtr?
                break;
            }
            pThis->abPacketBuf[ELNK_GP(pThis)] = val;
            pThis->uGPBufPtr = (pThis->uGPBufPtr + 1) & ELNK_GP_MASK;
            break;
    }

    return rc;
}

static uint32_t elIoRead(PELNKSTATE pThis, uint32_t addr, int *pRC)
{
    uint32_t val = UINT32_MAX;

    *pRC = VINF_SUCCESS;

    switch (addr & 0x0f)
    {
        case 0x00:  /* Receive status register aliases.  The SEEQ 8001 */
        case 0x02:  /* EDLC clearly only decodes one bit for reads.    */
        case 0x04:
        case 0x06:  /* Receive status register. */
            val = pThis->RcvStatReg;
            pThis->RcvStat.stale = 1;       /* Allows further reception. */
            pThis->IntrState.recv_intr = 0; /* Reading clears receive interrupt. */
            elnkUpdateIrq(pThis);
            break;

        case 0x01:  /* Transmit status register aliases. */
        case 0x03:
        case 0x05:
        case 0x07:  /* Transmit status register. */
            val = pThis->XmitStatReg;
            pThis->IntrState.xmit_intr = 0; /* Reading clears transmit interrupt. */
            elnkUpdateIrq(pThis);
            break;

        case 0x08:  /* GP Buffer pointer LSB. */
            val = RT_LOBYTE(pThis->uGPBufPtr);
            break;

        case 0x09:  /* GP Buffer pointer MSB. */
            val = RT_HIBYTE(pThis->uGPBufPtr);
            break;

        case 0x0a:  /* RCV Buffer pointer LSB. */
            val = RT_LOBYTE(pThis->uRCVBufPtr);
            break;

        case 0x0b:  /* RCV Buffer pointer MSB. */
            val = RT_HIBYTE(pThis->uRCVBufPtr);
            break;

        case 0x0c:  /* Ethernet address PROM window. */
        case 0x0d:  /* Alias. */
            /* Reads use low 3 bits of GP buffer pointer, no auto-increment. */
            val = pThis->aPROM[pThis->uGPBufPtr & 7];
            break;

        case 0x0e:  /* Auxiliary status register. */
            val = pThis->AuxStatReg;
            break;

        case 0x0f:  /* Buffer window. */
            /* Reads use low 11 bits of GP buffer pointer, auto-increment. */
            val = pThis->abPacketBuf[ELNK_GP(pThis)];
            pThis->uGPBufPtr = (pThis->uGPBufPtr + 1) & ELNK_GP_MASK;
            break;
    }

    elnkUpdateIrq(pThis);

    Log2(("#%d elIoRead: addr=%#06x val=%#04x\n", ELNK_INSTANCE, addr, val & 0xff));
    return val;
}


/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
static DECLCALLBACK(VBOXSTRICTRC)
elnkIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    int         rc    = VINF_SUCCESS;
    uint8_t     u8Lo, u8Hi;
    STAM_PROFILE_ADV_START(&pThis->CTX_SUFF_Z(StatIORead), a);
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    RT_NOREF_PV(pvUser);

    switch (cb)
    {
        case 1:
            *pu32 = elIoRead(pThis, Port, &rc);
            break;
        case 2:
            /* Manually split word access. */
            u8Lo = elIoRead(pThis, Port + 0, &rc);
            Assert(RT_SUCCESS(rc));
            u8Hi = elIoRead(pThis, Port + 1, &rc);
            Assert(RT_SUCCESS(rc));
            *pu32 = RT_MAKE_U16(u8Lo, u8Hi);
            break;
        default:
            rc = PDMDevHlpDBGFStop(pThis->CTX_SUFF(pDevIns), RT_SRC_POS,
                                   "elnkIOPortRead: unsupported operation size: offset=%#10x cb=%u\n",
                                   Port, cb);
    }

    Log2(("#%d elnkIOPortRead: Port=%RTiop *pu32=%#RX32 cb=%d rc=%Rrc\n", ELNK_INSTANCE, Port, *pu32, cb, rc));
    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF_Z(StatIORead), a);
    return rc;
}


/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC)
elnkIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PELNKSTATE  pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    int         rc    = VINF_SUCCESS;
    STAM_PROFILE_ADV_START(&pThis->CTX_SUFF_Z(StatIOWrite), a);
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    RT_NOREF_PV(pvUser);

    switch (cb)
    {
        case 1:
            rc = elIoWrite(pThis, Port, RT_LOBYTE(u32));
            break;
        case 2:
            /* Manually split word access. */
            rc = elIoWrite(pThis, Port + 0, RT_LOBYTE(u32));
            if (!RT_SUCCESS(rc))
                break;
            rc = elIoWrite(pThis, Port + 1, RT_HIBYTE(u32));
            break;
        default:
            rc = PDMDevHlpDBGFStop(pThis->CTX_SUFF(pDevIns), RT_SRC_POS,
                                   "elnkIOPortWrite: unsupported operation size: offset=%#10x cb=%u\n",
                                   Port, cb);
    }

    Log2(("#%d elnkIOPortWrite: Port=%RTiop u32=%#RX32 cb=%d rc=%Rrc\n", ELNK_INSTANCE, Port, u32, cb, rc));
    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF_Z(StatIOWrite), a);
    return rc;
}


#ifdef IN_RING3

/* Shamelessly stolen from DevDMA.cpp */

/* Test the decrement bit of mode register. */
#define IS_MODE_DEC(c)      ((c) & 0x20)
/* Test the auto-init bit of mode register. */
#define IS_MODE_AI(c)       ((c) & 0x10)
/* Extract the transfer type bits of mode register. */
#define GET_MODE_XTYP(c)    (((c) & 0x0c) >> 2)

/* DMA transfer modes. */
enum {
    DMODE_DEMAND,   /* Demand transfer mode. */
    DMODE_SINGLE,   /* Single transfer mode. */
    DMODE_BLOCK,    /* Block transfer mode. */
    DMODE_CASCADE   /* Cascade mode. */
};

/* DMA transfer types. */
enum {
    DTYPE_VERIFY,   /* Verify transfer type. */
    DTYPE_WRITE,    /* Write transfer type. */
    DTYPE_READ,     /* Read transfer type. */
    DTYPE_ILLEGAL   /* Undefined. */
};

static DECLCALLBACK(uint32_t) elnkR3DMAXferHandler(PPDMDEVINS pDevIns, void *opaque,
                                                   unsigned nchan, uint32_t dma_pos, uint32_t dma_len)
{
    RT_NOREF(pDevIns);
    PELNKSTATE  pThis = (PELNKSTATE)opaque;
    int         dma_mode;
    int         dma_type;
    uint32_t    cbToXfer;
    uint32_t    cbXferred;
    uint16_t    uLastPos;
    int         rc;

    /*
     * The 3C501 EtherLink uses DMA as an alternative to accessing
     * the buffer window register. The GP Buffer Pointer controls
     * the address into the packet buffer for both writing to and
     * reading from the buffer.
     */
    dma_mode = PDMDevHlpDMAGetChannelMode(pThis->CTX_SUFF(pDevIns), pThis->uIsaDma);
    dma_type = GET_MODE_XTYP(dma_mode);
    LogFlowFunc(("dma_mode=%d, dma_type=%d, dma_pos=%u, dma_len=%u, GPBP=%u\n", dma_mode, dma_type, dma_pos, dma_len, pThis->uGPBufPtr));

    cbToXfer = dma_len;

    if (dma_type == DTYPE_WRITE)
    {
        /* Write transfer type. Reading from device, writing to memory. */
        rc = PDMDevHlpDMAWriteMemory(pThis->CTX_SUFF(pDevIns), nchan,
                                     &pThis->abPacketBuf[ELNK_GP(pThis)],
                                     dma_pos, cbToXfer, &cbXferred);
        AssertMsgRC(rc, ("DMAWriteMemory -> %Rrc\n", rc));
        uLastPos = pThis->uRCVBufPtr;
    }
    else
    {
        /* Read of Verify transfer type. Reading from memory, writing to device. */
        rc = PDMDevHlpDMAReadMemory(pThis->CTX_SUFF(pDevIns), nchan,
                                    &pThis->abPacketBuf[ELNK_GP(pThis)],
                                    dma_pos, cbToXfer, &cbXferred);
        AssertMsgRC(rc, ("DMAReadMemory -> %Rrc\n", rc));
        uLastPos = 0;   /* Stop when buffer address wraps back to zero. */
    }
    Log2Func(("After DMA transfer: GPBufPtr=%u, lastpos=%u, cbXferred=%u\n", pThis->uGPBufPtr, uLastPos, cbXferred));

    /* Advance the GP buffer pointer and see if transfer completed (it almost certainly did). */
    pThis->uGPBufPtr = (pThis->uGPBufPtr + cbXferred) & ELNK_GP_MASK;
    if (ELNK_GP(pThis) == uLastPos || 1)
    {
        Log2(("DMA completed\n"));
        PDMDevHlpDMASetDREQ(pThis->CTX_SUFF(pDevIns), pThis->uIsaDma, 0);
        pThis->IntrState.dma_intr = 1;
        pThis->AuxStat.dma_done   = 1;
        elnkUpdateIrq(pThis);
    }
    else
    {
        Log(("DMA continuing: GPBufPtr=%u, lastpos=%u, cbXferred=%u\n", pThis->uGPBufPtr, uLastPos, cbXferred));
        PDMDevHlpDMASchedule(pThis->CTX_SUFF(pDevIns));
    }

    /* Returns the updated transfer count. */
    return dma_pos + cbXferred;
}


/* -=-=-=-=-=- Timer Callbacks -=-=-=-=-=- */

/**
 * @callback_method_impl{FNTMTIMERDEV, Restore timer callback}
 *
 * This is only called when we restore a saved state and temporarily
 * disconnected the network link to inform the guest that network connections
 * should be considered lost.
 */
static DECLCALLBACK(void) elnkTimerRestore(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, void *pvUser)
{
    RT_NOREF(pvUser);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    int         rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    AssertReleaseRC(rc);

    rc = VERR_GENERAL_FAILURE;
    if (pThis->cLinkDownReported <= ELNK_MAX_LINKDOWN_REPORTED)
        rc = PDMDevHlpTimerSetMillies(pDevIns, hTimer, 1500);
    if (RT_FAILURE(rc))
    {
        pThis->fLinkTempDown = false;
        if (pThis->fLinkUp)
        {
            LogRel(("3C501#%d: The link is back up again after the restore.\n",
                    pDevIns->iInstance));
            Log(("#%d elnkTimerRestore: Clearing ERR and CERR after load. cLinkDownReported=%d\n",
                 pDevIns->iInstance, pThis->cLinkDownReported));
            pThis->Led.Actual.s.fError = 0;
        }
    }
    else
        Log(("#%d elnkTimerRestore: cLinkDownReported=%d, wait another 1500ms...\n",
             pDevIns->iInstance, pThis->cLinkDownReported));

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
}


/* -=-=-=-=-=- Debug Info Handler -=-=-=-=-=- */

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) elnkInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PELNKSTATE          pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    bool                fStationAddr = false;
    bool                fRecvBuffer  = false;
    bool                fSendBuffer  = false;
    static const char   *apszAddrMatch[4] = { "Disabled", "Promiscuous", "Broadcast", "Multicast" };
    static const char   *apszBuffCntrl[4] = { "System", "Xmit then Recv", "Receive", "Loopback" };
    /*
     * Parse args.
     */
    if (pszArgs)
    {
        fStationAddr = strstr(pszArgs, "verbose") || strstr(pszArgs, "addr");
        fRecvBuffer  = strstr(pszArgs, "verbose") || strstr(pszArgs, "recvbuf");
        fSendBuffer  = strstr(pszArgs, "verbose") || strstr(pszArgs, "sendbuf");
    }

    /*
     * Show info.
     */
    pHlp->pfnPrintf(pHlp,
                    "3C501 #%d: port=%RTiop IRQ=%u DMA=%u mac-cfg=%RTmac %s%s\n",
                    pDevIns->iInstance,
                    pThis->IOPortBase, pThis->uIsaIrq, pThis->uIsaDma, &pThis->MacConfigured,
                    pDevIns->fRCEnabled ? " RC" : "", pDevIns->fR0Enabled ? " R0" : "");

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_INTERNAL_ERROR); /* Take it here so we know why we're hanging... */
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, &pThis->CritSect, rcLock);

    pHlp->pfnPrintf(pHlp, "  GP Buf Ptr : %u (masked %u)\n", pThis->uGPBufPtr, ELNK_GP(pThis));
    pHlp->pfnPrintf(pHlp, "  RCV Buf Ptr: %u\n", pThis->uRCVBufPtr);
    pHlp->pfnPrintf(pHlp, "  Recv Command: %02X   Recv Status: %02X\n", pThis->RcvCmdReg,  pThis->RcvStatReg);
    pHlp->pfnPrintf(pHlp, "  Xmit Command: %02X   Xmit Status: %02X\n", pThis->XmitCmdReg, pThis->XmitStatReg);
    pHlp->pfnPrintf(pHlp, "  Aux  Command: %02X   Aux  Status: %02X\n", pThis->AuxCmdReg,  pThis->AuxStatReg);

    pHlp->pfnPrintf(pHlp, "  Address matching: %s\n", apszAddrMatch[pThis->RcvCmd.adr_match]);
    pHlp->pfnPrintf(pHlp, "  Buffer control  : %s\n", apszBuffCntrl[pThis->AuxCmd.buf_ctl]);
    pHlp->pfnPrintf(pHlp, "  Interrupt state : xmit=%u recv=%u dma=%u\n", pThis->IntrState.xmit_intr, pThis->IntrState.recv_intr, pThis->IntrState.dma_intr);

    /* Dump the station address. */
    if (fStationAddr)
    {
        pHlp->pfnPrintf(pHlp, "  Station address : %RTmac\n", &pThis->aStationAddr);
    }

    /* Dump the beginning of the send buffer. */
    if (fSendBuffer)
    {
        pHlp->pfnPrintf(pHlp, "Send buffer (start at %u):\n", ELNK_GP(pThis));
        unsigned dump_end = RT_MIN((ELNK_GP(pThis)) + 64, sizeof(pThis->abPacketBuf) - 16);
        for (unsigned ofs = ELNK_GP(pThis); ofs < dump_end; ofs += 16)
            pHlp->pfnPrintf(pHlp, "  %04X: %Rhxs\n", ofs, &pThis->abPacketBuf[ofs]);
        pHlp->pfnPrintf(pHlp, "pktbuf at %p, end at %p\n", &pThis->abPacketBuf[ELNK_GP(pThis)], &pThis->abPacketBuf[ELNK_BUF_SIZE]);
    }

    /* Dump the beginning of the receive buffer. */
    if (fRecvBuffer)
    {
        pHlp->pfnPrintf(pHlp, "Receive buffer (start at 0):\n", pThis->uRCVBufPtr);
        unsigned dump_end = RT_MIN(pThis->uRCVBufPtr, 64);
        for (unsigned ofs = 0; ofs < dump_end; ofs += 16)
            pHlp->pfnPrintf(pHlp, "  %04X: %Rhxs\n", ofs, &pThis->abPacketBuf[ofs]);
        pHlp->pfnPrintf(pHlp, "pktbuf at %p, end at %p\n", pThis->abPacketBuf, &pThis->abPacketBuf[pThis->uRCVBufPtr]);
    }

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
}


/* -=-=-=-=-=- Helper(s) -=-=-=-=-=- */


static void elnkR3HardReset(PELNKSTATE pThis)
{
    LogFlow(("#%d elnkHardReset:\n", ELNK_INSTANCE));

    /* Initialize the PROM */
    Assert(sizeof(pThis->MacConfigured) == 6);
    memcpy(pThis->aPROM, &pThis->MacConfigured, sizeof(pThis->MacConfigured));
    pThis->aPROM[6] = pThis->aPROM[7] = 0;  /* The two padding bytes. */

    /* Clear the packet buffer and station address. */
    memset(pThis->abPacketBuf, 0, sizeof(pThis->abPacketBuf));
    memset(pThis->aStationAddr, 0, sizeof(pThis->aStationAddr));

    /* Reset the buffer pointers. */
    pThis->uGPBufPtr  = 0;
    pThis->uRCVBufPtr = 0;

    elnkSoftReset(pThis);
}

/**
 * Takes down the link temporarily if it's current status is up.
 *
 * This is used during restore and when replumbing the network link.
 *
 * The temporary link outage is supposed to indicate to the OS that all network
 * connections have been lost and that it for instance is appropriate to
 * renegotiate any DHCP lease.
 *
 * @param  pThis        The device instance data.
 */
static void elnkTempLinkDown(PELNKSTATE pThis)
{
    if (pThis->fLinkUp)
    {
        pThis->fLinkTempDown = true;
        pThis->cLinkDownReported = 0;
        pThis->Led.Asserted.s.fError = pThis->Led.Actual.s.fError = 1;
        int rc = PDMDevHlpTimerSetMillies(ELNKSTATE_2_DEVINS(pThis), pThis->hTimerRestore, pThis->cMsLinkUpDelay);
        AssertRC(rc);
    }
}


/* -=-=-=-=-=- Saved State -=-=-=-=-=- */

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC, Pass 0 only.}
 */
static DECLCALLBACK(int) elnkLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    RT_NOREF(uPass);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    pDevIns->pHlpR3->pfnSSMPutMem(pSSM, &pThis->MacConfigured, sizeof(pThis->MacConfigured));
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEPREP,
 *      Serializes the receive thread, it may be working inside the critsect.}
 */
static DECLCALLBACK(int) elnkSavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);

    int rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);
    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) elnkSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PELNKSTATE      pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMPutU16(pSSM, pThis->uGPBufPtr);
    pHlp->pfnSSMPutU16(pSSM, pThis->uRCVBufPtr);
    pHlp->pfnSSMPutU8(pSSM, pThis->XmitCmdReg);
    pHlp->pfnSSMPutU8(pSSM, pThis->XmitStatReg);
    pHlp->pfnSSMPutU8(pSSM, pThis->RcvCmdReg);
    pHlp->pfnSSMPutU8(pSSM, pThis->RcvStatReg);
    pHlp->pfnSSMPutU8(pSSM, pThis->AuxCmdReg);
    pHlp->pfnSSMPutU8(pSSM, pThis->AuxStatReg);

    pHlp->pfnSSMPutU8(pSSM, pThis->IntrStateReg);
    pHlp->pfnSSMPutBool(pSSM, pThis->fInReset);
    pHlp->pfnSSMPutBool(pSSM, pThis->fLinkUp);
    pHlp->pfnSSMPutBool(pSSM, pThis->fISR);
    pHlp->pfnSSMPutMem(pSSM, pThis->aStationAddr, sizeof(pThis->aStationAddr));

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADPREP},
 *      Serializes the receive thread, it may be working inside the critsect.}
 */
static DECLCALLBACK(int) elnkLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    RT_NOREF(pSSM);

    int rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);

    return rc;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) elnkLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PELNKSTATE      pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    if (   SSM_VERSION_MAJOR_CHANGED(uVersion, ELNK_SAVEDSTATE_VERSION)
        || SSM_VERSION_MINOR(uVersion) < 7)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    if (uPass == SSM_PASS_FINAL)
    {
        /* restore data */
        pHlp->pfnSSMGetU16(pSSM, &pThis->uGPBufPtr);
        pHlp->pfnSSMGetU16(pSSM, &pThis->uRCVBufPtr);
        pHlp->pfnSSMGetU8(pSSM, &pThis->XmitCmdReg);
        pHlp->pfnSSMGetU8(pSSM, &pThis->XmitStatReg);
        pHlp->pfnSSMGetU8(pSSM, &pThis->RcvCmdReg);
        pHlp->pfnSSMGetU8(pSSM, &pThis->RcvStatReg);
        pHlp->pfnSSMGetU8(pSSM, &pThis->AuxCmdReg);
        pHlp->pfnSSMGetU8(pSSM, &pThis->AuxStatReg);

        pHlp->pfnSSMGetU8(pSSM, &pThis->IntrStateReg);
        pHlp->pfnSSMGetBool(pSSM, &pThis->fInReset);
        pHlp->pfnSSMGetBool(pSSM, &pThis->fLinkUp);
        pHlp->pfnSSMGetBool(pSSM, &pThis->fISR);
        pHlp->pfnSSMGetMem(pSSM, &pThis->aStationAddr, sizeof(pThis->aStationAddr));
    }

    /* check config */
    RTMAC       Mac;
    int rc = pHlp->pfnSSMGetMem(pSSM, &Mac, sizeof(Mac));
    AssertRCReturn(rc, rc);
    if (    memcmp(&Mac, &pThis->MacConfigured, sizeof(Mac))
        && (uPass == 0 || !PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns)) )
        LogRel(("3C501#%u: The mac address differs: config=%RTmac saved=%RTmac\n", ELNK_INSTANCE, &pThis->MacConfigured, &Mac));

    if (uPass == SSM_PASS_FINAL)
    {
        /* update promiscuous mode. */
        if (pThis->pDrvR3)
            pThis->pDrvR3->pfnSetPromiscuousMode(pThis->pDrvR3, 0 /* promiscuous enabled */);

        /* Indicate link down to the guest OS that all network connections have
           been lost, unless we've been teleported here. */
        if (!PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns))
            elnkTempLinkDown(pThis);
    }

    return VINF_SUCCESS;
}


/* -=-=-=-=-=- ELNKSTATE::INetworkDown -=-=-=-=-=- */

/**
 * Check if the device/driver can receive data now.
 *
 * Worker for elnkNet_WaitReceiveAvail().  This must be called before
 * the pfnRecieve() method is called.
 *
 * @returns VBox status code.
 * @param   pThis           The device instance data.
 */
static int elnkCanReceive(PELNKSTATE pThis)
{
    PPDMDEVINS pDevIns = ELNKSTATE_2_DEVINS(pThis);
    int rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    AssertReleaseRC(rc);

    rc = VINF_SUCCESS;

    /*
     * The real 3C501 is very limited in that the packet buffer can only hold one
     * frame and and it is shared between transmit and receive, which means the card
     * frequently drops packets on a busy network. We cheat a bit and try to hold
     * off when it looks like receive is only temporarily unavailable.
     *
     * If the receiver is disabled, accept packet and drop it to avoid
     * packet pile-ups. If it's enabled, take a closer look.
     */
#if 0
    if (pThis->RcvCmd.adr_match != EL_ADRM_DISABLED) {
        /* The 3C501 is only prepared to accept a packet if the receiver is busy.
         * When not busy, try to delay packets.
         */
        if (!pThis->AuxStat.recv_bsy)
        {
            rc = VERR_NET_NO_BUFFER_SPACE;
        }
    }
#else
    if (pThis->RcvCmd.adr_match == EL_ADRM_DISABLED || !pThis->AuxStat.recv_bsy)
    {
        rc = VERR_NET_NO_BUFFER_SPACE;
    }
#endif
    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnWaitReceiveAvail}
 */
static DECLCALLBACK(int) elnkNet_WaitReceiveAvail(PPDMINETWORKDOWN pInterface, RTMSINTERVAL cMillies)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkDown);
    PPDMDEVINS pDevIns = ELNKSTATE_2_DEVINS(pThis);

    int rc = elnkCanReceive(pThis);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    if (RT_UNLIKELY(cMillies == 0))
        return VERR_NET_NO_BUFFER_SPACE;

    rc = VERR_INTERRUPTED;
    ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, true);
    STAM_PROFILE_START(&pThis->StatRxOverflow, a);
    VMSTATE enmVMState;
    while (RT_LIKELY(   (enmVMState = PDMDevHlpVMState(pThis->CTX_SUFF(pDevIns))) == VMSTATE_RUNNING
                     || enmVMState == VMSTATE_RUNNING_LS))
    {
        int rc2 = elnkCanReceive(pThis);
        if (RT_SUCCESS(rc2))
        {
            rc = VINF_SUCCESS;
            break;
        }
        LogFlow(("elnkNet_WaitReceiveAvail: waiting cMillies=%u...\n", cMillies));

        /* Start the poll timer once which will remain active as long fMaybeOutOfSpace
         * is true -- even if (transmit) polling is disabled. */
        rc2 = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
        AssertReleaseRC(rc2);
        PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
        RTSemEventWait(pThis->hEventOutOfRxSpace, cMillies);
    }
    STAM_PROFILE_STOP(&pThis->StatRxOverflow, a);
    ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, false);

    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceive}
 */
static DECLCALLBACK(int) elnkNet_Receive(PPDMINETWORKDOWN pInterface, const void *pvBuf, size_t cb)
{
    PELNKSTATE  pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkDown);
    PPDMDEVINS  pDevIns = ELNKSTATE_2_DEVINS(pThis);
    int         rc;

    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    AssertReleaseRC(rc);

    if (cb > 50) /* unqualified guess */
        pThis->Led.Asserted.s.fReading = pThis->Led.Actual.s.fReading = 1;
    elnkReceiveLocked(pThis, (const uint8_t *)pvBuf, cb, false);
    pThis->Led.Actual.s.fReading = 0;

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
    STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnXmitPending}
 */
static DECLCALLBACK(void) elnkNet_XmitPending(PPDMINETWORKDOWN pInterface)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkDown);
    elnkXmitBuffer(pThis, true /*fOnWorkerThread*/);
}


/* -=-=-=-=-=- ELNKSTATE::INetworkConfig -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnGetMac}
 */
static DECLCALLBACK(int) elnkGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkConfig);
    /// @todo This is broken!! We can't properly get the MAC address set by the guest
#if 0
    memcpy(pMac, pThis->aStationAddr, sizeof(*pMac));
#else
    memcpy(pMac, pThis->aPROM, sizeof(*pMac));
#endif
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnGetLinkState}
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) elnkGetLinkState(PPDMINETWORKCONFIG pInterface)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkConfig);
    if (pThis->fLinkUp && !pThis->fLinkTempDown)
        return PDMNETWORKLINKSTATE_UP;
    if (!pThis->fLinkUp)
        return PDMNETWORKLINKSTATE_DOWN;
    if (pThis->fLinkTempDown)
        return PDMNETWORKLINKSTATE_DOWN_RESUME;
    AssertMsgFailed(("Invalid link state!\n"));
    return PDMNETWORKLINKSTATE_INVALID;
}


/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnSetLinkState}
 */
static DECLCALLBACK(int) elnkSetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, INetworkConfig);
    bool fLinkUp;

    AssertMsgReturn(enmState > PDMNETWORKLINKSTATE_INVALID && enmState <= PDMNETWORKLINKSTATE_DOWN_RESUME,
                    ("Invalid link state: enmState=%d\n", enmState), VERR_INVALID_PARAMETER);

    if (enmState == PDMNETWORKLINKSTATE_DOWN_RESUME)
    {
        elnkTempLinkDown(pThis);
        /*
         * Note that we do not notify the driver about the link state change because
         * the change is only temporary and can be disregarded from the driver's
         * point of view (see @bugref{7057}).
         */
        return VINF_SUCCESS;
    }
    /* has the state changed? */
    fLinkUp = enmState == PDMNETWORKLINKSTATE_UP;
    if (pThis->fLinkUp != fLinkUp)
    {
        pThis->fLinkUp = fLinkUp;
        if (fLinkUp)
        {
            /* Connect with a configured delay. */
            pThis->fLinkTempDown = true;
            pThis->cLinkDownReported = 0;
            pThis->Led.Asserted.s.fError = pThis->Led.Actual.s.fError = 1;
            int rc = PDMDevHlpTimerSetMillies(ELNKSTATE_2_DEVINS(pThis), pThis->hTimerRestore, pThis->cMsLinkUpDelay);
            AssertRC(rc);
        }
        else
        {
            /* Disconnect. */
            pThis->cLinkDownReported = 0;
            pThis->Led.Asserted.s.fError = pThis->Led.Actual.s.fError = 1;
        }
        Assert(!PDMDevHlpCritSectIsOwner(ELNKSTATE_2_DEVINS(pThis), &pThis->CritSect));
        if (pThis->pDrvR3)
            pThis->pDrvR3->pfnNotifyLinkChanged(pThis->pDrvR3, enmState);
    }
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- ELNKSTATE::ILeds (LUN#0) -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMILEDPORTS,pfnQueryStatusLed}
 */
static DECLCALLBACK(int) elnkQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, ILeds);
    if (iLUN == 0)
    {
        *ppLed = &pThis->Led;
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/* -=-=-=-=-=- ELNKSTATE::IBase (LUN#0) -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) elnkQueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PELNKSTATE pThis = RT_FROM_MEMBER(pInterface, ELNKSTATE, IBase);
    Assert(&pThis->IBase == pInterface);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKDOWN, &pThis->INetworkDown);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThis->INetworkConfig);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThis->ILeds);
    return NULL;
}


/* -=-=-=-=-=- PDMDEVREG -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnPowerOff}
 */
static DECLCALLBACK(void) elnkPowerOff(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    elnkR3WakeupReceive(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 *
 * One port on the network card has been disconnected from the network.
 */
static DECLCALLBACK(void) elnkDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    Log(("#%d elnkDetach:\n", ELNK_INSTANCE));

    AssertLogRelReturnVoid(iLUN == 0);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, &pThis->CritSect, rcLock);

    /*
     * Zero some important members.
     */
    pThis->pDrvBase = NULL;
    pThis->pDrvR3 = NULL;
    pThis->pDrvR0 = NIL_RTR0PTR;
    pThis->pDrvRC = NIL_RTRCPTR;

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 * One port on the network card has been connected to a network.
 */
static DECLCALLBACK(int) elnkAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    LogFlow(("#%d elnkAttach:\n", ELNK_INSTANCE));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_SEM_BUSY);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, &pThis->CritSect, rcLock);

    /*
     * Attach the driver.
     */
    int rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvR3 = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMINETWORKUP);
        AssertMsgStmt(pThis->pDrvR3, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                      rc = VERR_PDM_MISSING_INTERFACE_BELOW);
        pThis->pDrvR0 = PDMIBASER0_QUERY_INTERFACE(PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBASER0), PDMINETWORKUP);
        pThis->pDrvRC = PDMIBASERC_QUERY_INTERFACE(PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBASERC), PDMINETWORKUP);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        /* This should never happen because this function is not called
         * if there is no driver to attach! */
        Log(("#%d No attached driver!\n", ELNK_INSTANCE));
    }

    /*
     * Temporary set the link down if it was up so that the guest
     * will know that we have change the configuration of the
     * network card
     */
    if (RT_SUCCESS(rc))
        elnkTempLinkDown(pThis);

    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
    return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnSuspend}
 */
static DECLCALLBACK(void) elnkSuspend(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    elnkR3WakeupReceive(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) elnkReset(PPDMDEVINS pDevIns)
{
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    if (pThis->fLinkTempDown)
    {
        pThis->cLinkDownReported = 0x1000;
        PDMDevHlpTimerStop(pDevIns, pThis->hTimerRestore);
        elnkTimerRestore(pDevIns, pThis->hTimerRestore, pThis);
    }

    /** @todo How to flush the queues? */
    elnkR3HardReset(pThis);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) elnkRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) elnkDestruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PELNKSTATE pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);

    if (PDMDevHlpCritSectIsInitialized(pDevIns, &pThis->CritSect))
    {
        RTSemEventSignal(pThis->hEventOutOfRxSpace);
        RTSemEventDestroy(pThis->hEventOutOfRxSpace);
        pThis->hEventOutOfRxSpace = NIL_RTSEMEVENT;
        PDMDevHlpCritSectDelete(pDevIns, &pThis->CritSect);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) elnkConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PELNKSTATE      pThis = PDMINS_2_DATA(pDevIns, PELNKSTATE);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    PPDMIBASE       pBase;
    char            szTmp[128];
    int             rc;

    /*
     * Init what's required to make the destructor safe.
     */
    pThis->iInstance            = iInstance;
    pThis->hEventOutOfRxSpace   = NIL_SUPSEMEVENT;
    pThis->hIoPortsIsa          = NIL_IOMIOPORTHANDLE;

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "MAC|CableConnected|Port|IRQ|DMA|LinkUpDelay|LineSpeed", "");

    /*
     * Read the configuration.
     */
    rc = pHlp->pfnCFGMQueryBytes(pCfg, "MAC", &pThis->MacConfigured, sizeof(pThis->MacConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"MAC\" value"));
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "CableConnected", &pThis->fLinkUp, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"CableConnected\" value"));

    /*
     * Process ISA configuration options.
     */
    rc = pHlp->pfnCFGMQueryPortDef(pCfg, "Port", &pThis->IOPortBase, 0x300);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Port\" value"));

    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "IRQ", &pThis->uIsaIrq, 3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));

    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "DMA", &pThis->uIsaDma, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DMA\" value"));

    rc = pHlp->pfnCFGMQueryU32Def(pCfg, "LinkUpDelay", (uint32_t*)&pThis->cMsLinkUpDelay, 5000); /* ms */
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the value of 'LinkUpDelay'"));
    Assert(pThis->cMsLinkUpDelay <= 300000); /* less than 5 minutes */
    if (pThis->cMsLinkUpDelay > 5000 || pThis->cMsLinkUpDelay < 100)
    {
        LogRel(("3C501#%d WARNING! Link up delay is set to %u seconds!\n",
                iInstance, pThis->cMsLinkUpDelay / 1000));
    }
    Log(("#%d Link up delay is set to %u seconds\n",
         iInstance, pThis->cMsLinkUpDelay / 1000));


    /*
     * Initialize data (most of it anyway).
     */
    pThis->pDevInsR3                        = pDevIns;
    pThis->pDevInsR0                        = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC                        = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->Led.u32Magic                     = PDMLED_MAGIC;
    /* IBase */
    pThis->IBase.pfnQueryInterface          = elnkQueryInterface;
    /* INetworkPort */
    pThis->INetworkDown.pfnWaitReceiveAvail = elnkNet_WaitReceiveAvail;
    pThis->INetworkDown.pfnReceive          = elnkNet_Receive;
    pThis->INetworkDown.pfnXmitPending      = elnkNet_XmitPending;
    /* INetworkConfig */
    pThis->INetworkConfig.pfnGetMac         = elnkGetMac;
    pThis->INetworkConfig.pfnGetLinkState   = elnkGetLinkState;
    pThis->INetworkConfig.pfnSetLinkState   = elnkSetLinkState;
    /* ILeds */
    pThis->ILeds.pfnQueryStatusLed          = elnkQueryStatusLed;

    /*
     * We use our own critical section (historical reasons).
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "3C501#%u", iInstance);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pThis->hEventOutOfRxSpace);
    AssertRCReturn(rc, rc);

    /*
     * Register ISA I/O ranges for the EtherLink 3C501.
     */
    rc = PDMDevHlpIoPortCreateAndMap(pDevIns, pThis->IOPortBase, 0x10 /*cPorts*/, elnkIOPortWrite, elnkIOPortRead,
                                     "3C501", NULL /*paExtDesc*/, &pThis->hIoPortsIsa);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register DMA channel.
     */
    if (pThis->uIsaDma <= ELNK_MAX_VALID_DMA)
    {
        rc = PDMDevHlpDMARegister(pDevIns, pThis->uIsaDma, elnkR3DMAXferHandler, pThis);
        if (RT_FAILURE(rc))
            return rc;
        LogRel(("3C501#%d: Enabling DMA channel %u\n", iInstance, pThis->uIsaDma));
    }
    else
        LogRel(("3C501#%d: Disabling DMA\n", iInstance));

    rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL, elnkTimerRestore, NULL, TMTIMER_FLAGS_NO_CRIT_SECT | TMTIMER_FLAGS_NO_RING0,
                              "3C501 Restore Timer", &pThis->hTimerRestore);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpSSMRegisterEx(pDevIns, ELNK_SAVEDSTATE_VERSION, sizeof(*pThis), NULL,
                                NULL,         elnkLiveExec, NULL,
                                elnkSavePrep, elnkSaveExec, NULL,
                                elnkLoadPrep, elnkLoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the transmit queue.
     */
    rc = PDMDevHlpTaskCreate(pDevIns, PDMTASK_F_RZ, "3C501-Xmit", elnkR3XmitTaskCallback, NULL /*pvUser*/, &pThis->hXmitTask);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the RX notifier signaller.
     */
    rc = PDMDevHlpTaskCreate(pDevIns, PDMTASK_F_RZ, "3C501-Rcv", elnkR3CanRxTaskCallback, NULL /*pvUser*/, &pThis->hCanRxTask);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the info item.
     */
    RTStrPrintf(szTmp, sizeof(szTmp), "elnk%d", pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "3C501 info", elnkInfo);

    /*
     * Attach status driver (optional).
     */
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
    else if (   rc != VERR_PDM_NO_ATTACHED_DRIVER
             && rc != VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Attach driver.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvR3 = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMINETWORKUP);
        AssertMsgReturn(pThis->pDrvR3, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                        VERR_PDM_MISSING_INTERFACE_BELOW);
        pThis->pDrvR0 = PDMIBASER0_QUERY_INTERFACE(PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBASER0), PDMINETWORKUP);
        pThis->pDrvRC = PDMIBASERC_QUERY_INTERFACE(PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBASERC), PDMINETWORKUP);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        /* No error! */
        Log(("No attached driver!\n"));
    }
    else
        return rc;

    /*
     * Reset the device state. (Do after attaching.)
     */
    elnkR3HardReset(pThis);

    /*
     * Register statistics counters.
     */
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatReceiveBytes,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data received",                "/Public/Net/EtherLink%u/BytesReceived", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitBytes,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data transmitted",             "/Public/Net/EtherLink%u/BytesTransmitted", iInstance);

    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatReceiveBytes,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data received",                "/Devices/EtherLink%d/ReceiveBytes", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitBytes,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data transmitted",             "/Devices/EtherLink%d/TransmitBytes", iInstance);

#ifdef VBOX_WITH_STATISTICS
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatIOReadRZ,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in RZ",               "/Devices/EtherLink%d/IO/ReadRZ", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatIOReadR3,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in R3",               "/Devices/EtherLink%d/IO/ReadR3", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatIOWriteRZ,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in RZ",              "/Devices/EtherLink%d/IO/WriteRZ", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatIOWriteR3,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in R3",              "/Devices/EtherLink%d/IO/WriteR3", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatReceive,            STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling receive",                      "/Devices/EtherLink%d/Receive", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatRxOverflow,         STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling RX overflows",            "/Devices/EtherLink%d/RxOverflow", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatRxOverflowWakeup,   STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Nr of RX overflow wakeups",         "/Devices/EtherLink%d/RxOverflowWakeup", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitRZ,         STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling transmits in RZ",              "/Devices/EtherLink%d/Transmit/TotalRZ", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitR3,         STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling transmits in R3",              "/Devices/EtherLink%d/Transmit/TotalR3", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitSendRZ,     STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling send transmit in RZ",          "/Devices/EtherLink%d/Transmit/SendRZ", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitSendR3,     STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling send transmit in R3",          "/Devices/EtherLink%d/Transmit/SendR3", iInstance);

    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatInterrupt,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling interrupt checks",             "/Devices/EtherLink%d/UpdateIRQ", iInstance);

    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatResets,             STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of soft resets",                  "/Devices/EtherLink%d/SoftResets", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatDropPktAdrmDis,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Dropped packet, disabled address match", "/Devices/EtherLink%d/DropPktAdrmDis", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatDropPktZeroLen,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Dropped zero length packet",             "/Devices/EtherLink%d/DropPktZeroLen", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatDropPktVMNotRunning,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Dropped packet, VM not running",         "/Devices/EtherLink%d/DropPktVMNotRunning", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatDropPktNoLink,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Dropped packet, no link", "/Devices/EtherLink%d/DropPktNoLink", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatDropPktStaleRcv,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Dropped packet, status register unread", "/Devices/EtherLink%d/DropPktStaleRcv", iInstance);
#endif /* VBOX_WITH_STATISTICS */
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatPktsLostReset,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of packets lost due to resets",   "/Devices/EtherLink%d/PktsLostByReset", iInstance);

    return VINF_SUCCESS;
}

#else

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) elnkRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PELNKSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PELNKSTATE);

    /* Critical section setup: */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    /* ISA I/O ports: */
    if (pThis->hIoPortsIsa != NIL_IOMIOPORTHANDLE)
    {
        rc = PDMDevHlpIoPortSetUpContext(pDevIns, pThis->hIoPortsIsa, elnkIOPortWrite, elnkIOPortRead, NULL /*pvUser*/);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_Device3C501 =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "3c501",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_NETWORK,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(ELNKSTATE),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "3Com EtherLink 3C501 adapter.\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           elnkConstruct,
    /* .pfnDestruct = */            elnkDestruct,
    /* .pfnRelocate = */            elnkRelocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               elnkReset,
    /* .pfnSuspend = */             elnkSuspend,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              elnkAttach,
    /* .pfnDetach = */              elnkDetach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            elnkPowerOff,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           elnkRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
