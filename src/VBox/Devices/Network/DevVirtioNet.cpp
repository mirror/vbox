/* $Id$ */
/** @file
 * DevVirtioNet - Virtio Network Device
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO_NET
#define VNET_GC_SUPPORT
#define VNET_WITH_GSO
#define VNET_WITH_MERGEABLE_RX_BUFS

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmnetifs.h>
#include <iprt/asm.h>
#include <iprt/net.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/uuid.h>
#endif
#include <VBox/VBoxPktDmp.h>
#include "VBoxDD.h"
#include "../VirtIO/Virtio.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define INSTANCE(pThis) pThis->VPCI.szInstance
#define STATUS pThis->config.uStatus

#ifdef IN_RING3

# define VNET_PCI_CLASS              0x0200
# define VNET_N_QUEUES               3
# define VNET_NAME_FMT               "VNet%d"

# if 0
/* Virtio Block Device */
#  define VNET_PCI_CLASS             0x0180
#  define VNET_N_QUEUES              2
#  define VNET_NAME_FMT              "VBlk%d"
# endif

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

/*
 * Commenting out VNET_TX_DELAY enables async transmission in a dedicated thread.
 * When VNET_TX_DELAY is defined, a timer handler does the job.
 */
//#define VNET_TX_DELAY           150   /**< 150 microseconds */
#define VNET_MAX_FRAME_SIZE     65535 + 18  /**< Max IP packet size + Ethernet header with VLAN tag */
#define VNET_MAC_FILTER_LEN     32
#define VNET_MAX_VID            (1 << 12)

/** @name Virtio net features
 * @{  */
#define VNET_F_CSUM       0x00000001  /**< Host handles pkts w/ partial csum */
#define VNET_F_GUEST_CSUM 0x00000002  /**< Guest handles pkts w/ partial csum */
#define VNET_F_MAC        0x00000020  /**< Host has given MAC address. */
#define VNET_F_GSO        0x00000040  /**< Host handles pkts w/ any GSO type */
#define VNET_F_GUEST_TSO4 0x00000080  /**< Guest can handle TSOv4 in. */
#define VNET_F_GUEST_TSO6 0x00000100  /**< Guest can handle TSOv6 in. */
#define VNET_F_GUEST_ECN  0x00000200  /**< Guest can handle TSO[6] w/ ECN in. */
#define VNET_F_GUEST_UFO  0x00000400  /**< Guest can handle UFO in. */
#define VNET_F_HOST_TSO4  0x00000800  /**< Host can handle TSOv4 in. */
#define VNET_F_HOST_TSO6  0x00001000  /**< Host can handle TSOv6 in. */
#define VNET_F_HOST_ECN   0x00002000  /**< Host can handle TSO[6] w/ ECN in. */
#define VNET_F_HOST_UFO   0x00004000  /**< Host can handle UFO in. */
#define VNET_F_MRG_RXBUF  0x00008000  /**< Host can merge receive buffers. */
#define VNET_F_STATUS     0x00010000  /**< virtio_net_config.status available */
#define VNET_F_CTRL_VQ    0x00020000  /**< Control channel available */
#define VNET_F_CTRL_RX    0x00040000  /**< Control channel RX mode support */
#define VNET_F_CTRL_VLAN  0x00080000  /**< Control channel VLAN filtering */
/** @} */

#define VNET_S_LINK_UP    1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef _MSC_VER
struct VNetPCIConfig
#else /* !_MSC_VER */
struct __attribute__ ((__packed__)) VNetPCIConfig /** @todo r=bird: Use #pragma pack if necessary, that's portable! */
#endif /* !_MSC_VER */
{
    RTMAC    mac;
    uint16_t uStatus;
};
AssertCompileMemberOffset(struct VNetPCIConfig, uStatus, 6);

/**
 * Device state structure. Holds the current state of device.
 *
 * @extends     VPCISTATE
 * @implements  PDMINETWORKDOWN
 * @implements  PDMINETWORKCONFIG
 */
typedef struct VNetState_st
{
    /* VPCISTATE must be the first member! */
    VPCISTATE               VPCI;

//    PDMCRITSECT             csRx;                           /**< Protects RX queue. */

    PDMINETWORKDOWN         INetworkDown;
    PDMINETWORKCONFIG       INetworkConfig;
    R3PTRTYPE(PPDMIBASE)    pDrvBase;                 /**< Attached network driver. */
    R3PTRTYPE(PPDMINETWORKUP) pDrv;    /**< Connector of attached network driver. */

    R3PTRTYPE(PPDMQUEUE)    pCanRxQueueR3;           /**< Rx wakeup signaller - R3. */
    R0PTRTYPE(PPDMQUEUE)    pCanRxQueueR0;           /**< Rx wakeup signaller - R0. */
    RCPTRTYPE(PPDMQUEUE)    pCanRxQueueRC;           /**< Rx wakeup signaller - RC. */
# if HC_ARCH_BITS == 64
    uint32_t                padding;
# endif

    /**< Link Up(/Restore) Timer. */
    PTMTIMERR3              pLinkUpTimer;

#ifdef VNET_TX_DELAY
    /**< Transmit Delay Timer - R3. */
    PTMTIMERR3              pTxTimerR3;
    /**< Transmit Delay Timer - R0. */
    PTMTIMERR0              pTxTimerR0;
    /**< Transmit Delay Timer - GC. */
    PTMTIMERRC              pTxTimerRC;

# if HC_ARCH_BITS == 64
    uint32_t                padding2;
# endif

    uint32_t                u32i;
    uint32_t                u32AvgDiff;
    uint32_t                u32MinDiff;
    uint32_t                u32MaxDiff;
    uint64_t                u64NanoTS;
#else /* !VNET_TX_DELAY */
    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;
    /** The event semaphore TX thread waits on. */
    SUPSEMEVENT                     hTxEvent;
    R3PTRTYPE(PPDMTHREAD)           pTxThread;
#endif /* !VNET_TX_DELAY */

    /** Indicates transmission in progress -- only one thread is allowed. */
    uint32_t                uIsTransmitting;

    /** PCI config area holding MAC address as well as TBD. */
    struct VNetPCIConfig    config;
    /** MAC address obtained from the configuration. */
    RTMAC                   macConfigured;
    /** True if physical cable is attached in configuration. */
    bool                    fCableConnected;
    /** Link up delay (in milliseconds). */
    uint32_t                cMsLinkUpDelay;

    uint32_t                alignment;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t                u32PktNo;

    /** N/A: */
    bool volatile           fMaybeOutOfSpace;

    /** Promiscuous mode -- RX filter accepts all packets. */
    bool                    fPromiscuous;
    /** AllMulti mode -- RX filter accepts all multicast packets. */
    bool                    fAllMulti;
    /** The number of actually used slots in aMacTable. */
    uint32_t                nMacFilterEntries;
    /** Array of MAC addresses accepted by RX filter. */
    RTMAC                   aMacFilter[VNET_MAC_FILTER_LEN];
    /** Bit array of VLAN filter, one bit per VLAN ID. */
    uint8_t                 aVlanFilter[VNET_MAX_VID / sizeof(uint8_t)];

    R3PTRTYPE(PVQUEUE)      pRxQueue;
    R3PTRTYPE(PVQUEUE)      pTxQueue;
    R3PTRTYPE(PVQUEUE)      pCtlQueue;
    /* Receive-blocking-related fields ***************************************/

    /** EMT: Gets signalled when more RX descriptors become available. */
    RTSEMEVENT              hEventMoreRxDescAvail;

    /** @name Statistic
     * @{ */
    STAMCOUNTER             StatReceiveBytes;
    STAMCOUNTER             StatTransmitBytes;
    STAMCOUNTER             StatReceiveGSO;
    STAMCOUNTER             StatTransmitPackets;
    STAMCOUNTER             StatTransmitGSO;
    STAMCOUNTER             StatTransmitCSum;
#ifdef VBOX_WITH_STATISTICS
    STAMPROFILE             StatReceive;
    STAMPROFILE             StatReceiveStore;
    STAMPROFILEADV          StatTransmit;
    STAMPROFILE             StatTransmitSend;
    STAMPROFILE             StatRxOverflow;
    STAMCOUNTER             StatRxOverflowWakeup;
    STAMCOUNTER             StatTransmitByNetwork;
    STAMCOUNTER             StatTransmitByThread;
#endif
    /** @}  */
} VNETSTATE;
/** Pointer to a virtual I/O network device state. */
typedef VNETSTATE *PVNETSTATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define VNETHDR_F_NEEDS_CSUM     1       // Use u16CSumStart, u16CSumOffset

#define VNETHDR_GSO_NONE         0       // Not a GSO frame
#define VNETHDR_GSO_TCPV4        1       // GSO frame, IPv4 TCP (TSO)
#define VNETHDR_GSO_UDP          3       // GSO frame, IPv4 UDP (UFO)
#define VNETHDR_GSO_TCPV6        4       // GSO frame, IPv6 TCP
#define VNETHDR_GSO_ECN          0x80    // TCP has ECN set

struct VNetHdr
{
    uint8_t  u8Flags;
    uint8_t  u8GSOType;
    uint16_t u16HdrLen;
    uint16_t u16GSOSize;
    uint16_t u16CSumStart;
    uint16_t u16CSumOffset;
};
typedef struct VNetHdr VNETHDR;
typedef VNETHDR *PVNETHDR;
AssertCompileSize(VNETHDR, 10);

struct VNetHdrMrx
{
    VNETHDR  Hdr;
    uint16_t u16NumBufs;
};
typedef struct VNetHdrMrx VNETHDRMRX;
typedef VNETHDRMRX *PVNETHDRMRX;
AssertCompileSize(VNETHDRMRX, 12);

AssertCompileMemberOffset(VNETSTATE, VPCI, 0);

#define VNET_OK                    0
#define VNET_ERROR                 1
typedef uint8_t VNETCTLACK;

#define VNET_CTRL_CLS_RX_MODE          0
#define VNET_CTRL_CMD_RX_MODE_PROMISC  0
#define VNET_CTRL_CMD_RX_MODE_ALLMULTI 1

#define VNET_CTRL_CLS_MAC              1
#define VNET_CTRL_CMD_MAC_TABLE_SET    0

#define VNET_CTRL_CLS_VLAN             2
#define VNET_CTRL_CMD_VLAN_ADD         0
#define VNET_CTRL_CMD_VLAN_DEL         1


typedef struct VNetCtlHdr
{
    uint8_t  u8Class;
    uint8_t  u8Command;
} VNETCTLHDR;
AssertCompileSize(VNETCTLHDR, 2);
typedef VNETCTLHDR *PVNETCTLHDR;



#ifdef IN_RING3

/** Returns true if large packets are written into several RX buffers. */
DECLINLINE(bool) vnetR3MergeableRxBuffers(PVNETSTATE pThis)
{
    return !!(pThis->VPCI.uGuestFeatures & VNET_F_MRG_RXBUF);
}

DECLINLINE(int) vnetR3CsEnter(PVNETSTATE pThis, int rcBusy)
{
    return vpciCsEnter(&pThis->VPCI, rcBusy);
}

DECLINLINE(void) vnetR3CsLeave(PVNETSTATE pThis)
{
    vpciCsLeave(&pThis->VPCI);
}

#endif /* IN_RING3 */

DECLINLINE(int) vnetCsRxEnter(PVNETSTATE pThis, int rcBusy)
{
    RT_NOREF_PV(pThis);
    RT_NOREF_PV(rcBusy);
    // STAM_PROFILE_START(&pThis->CTXSUFF(StatCsRx), a);
    // int rc = PDMCritSectEnter(&pThis->csRx, rcBusy);
    // STAM_PROFILE_STOP(&pThis->CTXSUFF(StatCsRx), a);
    // return rc;
    return VINF_SUCCESS;
}

DECLINLINE(void) vnetCsRxLeave(PVNETSTATE pThis)
{
    RT_NOREF_PV(pThis);
    // PDMCritSectLeave(&pThis->csRx);
}

#ifdef IN_RING3
/**
 * Dump a packet to debug log.
 *
 * @param   pThis       The device state structure.
 * @param   pbPacket    The packet.
 * @param   cb          The size of the packet.
 * @param   pszText     A string denoting direction of packet transfer.
 */
DECLINLINE(void) vnetR3PacketDump(PVNETSTATE pThis, const uint8_t *pbPacket, size_t cb, const char *pszText)
{
# ifdef DEBUG
#  if 0
    Log(("%s %s packet #%d (%d bytes):\n",
         INSTANCE(pThis), pszText, ++pThis->u32PktNo, cb));
    Log3(("%.*Rhxd\n", cb, pbPacket));
#  else
    vboxEthPacketDump(INSTANCE(pThis), pszText, pbPacket, (uint32_t)cb);
#  endif
# else
    RT_NOREF4(pThis, pbPacket, cb, pszText);
# endif
}
#endif /* IN_RING3 */

/**
 * Print features given in uFeatures to debug log.
 *
 * @param   pThis      The device state structure.
 * @param   fFeatures   Descriptions of which features to print.
 * @param   pcszText    A string to print before the list of features.
 */
DECLINLINE(void) vnetPrintFeatures(PVNETSTATE pThis, uint32_t fFeatures, const char *pcszText)
{
#ifdef LOG_ENABLED
    static struct
    {
        uint32_t fMask;
        const char *pcszDesc;
    } const s_aFeatures[] =
    {
        { VNET_F_CSUM,       "host handles pkts w/ partial csum" },
        { VNET_F_GUEST_CSUM, "guest handles pkts w/ partial csum" },
        { VNET_F_MAC,        "host has given MAC address" },
        { VNET_F_GSO,        "host handles pkts w/ any GSO type" },
        { VNET_F_GUEST_TSO4, "guest can handle TSOv4 in" },
        { VNET_F_GUEST_TSO6, "guest can handle TSOv6 in" },
        { VNET_F_GUEST_ECN,  "guest can handle TSO[6] w/ ECN in" },
        { VNET_F_GUEST_UFO,  "guest can handle UFO in" },
        { VNET_F_HOST_TSO4,  "host can handle TSOv4 in" },
        { VNET_F_HOST_TSO6,  "host can handle TSOv6 in" },
        { VNET_F_HOST_ECN,   "host can handle TSO[6] w/ ECN in" },
        { VNET_F_HOST_UFO,   "host can handle UFO in" },
        { VNET_F_MRG_RXBUF,  "host can merge receive buffers" },
        { VNET_F_STATUS,     "virtio_net_config.status available" },
        { VNET_F_CTRL_VQ,    "control channel available" },
        { VNET_F_CTRL_RX,    "control channel RX mode support" },
        { VNET_F_CTRL_VLAN,  "control channel VLAN filtering" }
    };

    Log3(("%s %s:\n", INSTANCE(pThis), pcszText));
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFeatures); ++i)
    {
        if (s_aFeatures[i].fMask & fFeatures)
            Log3(("%s --> %s\n", INSTANCE(pThis), s_aFeatures[i].pcszDesc));
    }
#else  /* !LOG_ENABLED */
    RT_NOREF3(pThis, fFeatures, pcszText);
#endif /* !LOG_ENABLED */
}

static DECLCALLBACK(uint32_t) vnetIoCb_GetHostFeatures(void *pvState)
{
    RT_NOREF_PV(pvState);

    /* We support:
     * - Host-provided MAC address
     * - Link status reporting in config space
     * - Control queue
     * - RX mode setting
     * - MAC filter table
     * - VLAN filter
     */
    return VNET_F_MAC
         | VNET_F_STATUS
         | VNET_F_CTRL_VQ
         | VNET_F_CTRL_RX
         | VNET_F_CTRL_VLAN
#ifdef VNET_WITH_GSO
         | VNET_F_CSUM
         | VNET_F_HOST_TSO4
         | VNET_F_HOST_TSO6
         | VNET_F_HOST_UFO
         | VNET_F_GUEST_CSUM   /* We expect the guest to accept partial TCP checksums (see @bugref{4796}) */
         | VNET_F_GUEST_TSO4
         | VNET_F_GUEST_TSO6
         | VNET_F_GUEST_UFO
#endif
#ifdef VNET_WITH_MERGEABLE_RX_BUFS
         | VNET_F_MRG_RXBUF
#endif
         ;
}

static DECLCALLBACK(uint32_t) vnetIoCb_GetHostMinimalFeatures(void *pvState)
{
    RT_NOREF_PV(pvState);
    return VNET_F_MAC;
}

static DECLCALLBACK(void) vnetIoCb_SetHostFeatures(void *pvState, uint32_t fFeatures)
{
    /** @todo Nothing to do here yet */
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    LogFlow(("%s vnetIoCb_SetHostFeatures: uFeatures=%x\n", INSTANCE(pThis), fFeatures));
    vnetPrintFeatures(pThis, fFeatures, "The guest negotiated the following features");
}

static DECLCALLBACK(int) vnetIoCb_GetConfig(void *pvState, uint32_t offCfg, uint32_t cb, void *data)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    if (offCfg + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetIoCb_GetConfig: Read beyond the config structure is attempted (offCfg=%#x cb=%x).\n", INSTANCE(pThis), offCfg, cb));
        return VERR_IOM_IOPORT_UNUSED;
    }
    memcpy(data, (uint8_t *)&pThis->config + offCfg, cb);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vnetIoCb_SetConfig(void *pvState, uint32_t offCfg, uint32_t cb, void *data)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    if (offCfg + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetIoCb_SetConfig: Write beyond the config structure is attempted (offCfg=%#x cb=%x).\n", INSTANCE(pThis), offCfg, cb));
        if (offCfg < sizeof(struct VNetPCIConfig))
            memcpy((uint8_t *)&pThis->config + offCfg, data,
                   sizeof(struct VNetPCIConfig) - offCfg);
        return VINF_SUCCESS;
    }
    memcpy((uint8_t *)&pThis->config + offCfg, data, cb);
    return VINF_SUCCESS;
}

/**
 * Hardware reset. Revert all registers to initial values.
 *
 * @param   pThis      The device state structure.
 */
static DECLCALLBACK(int) vnetIoCb_Reset(void *pvState)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    Log(("%s Reset triggered\n", INSTANCE(pThis)));

    int rc = vnetCsRxEnter(pThis, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        LogRel(("vnetIoCb_Reset failed to enter RX critical section!\n"));
        return rc;
    }
    vpciReset(&pThis->VPCI);
    vnetCsRxLeave(pThis);

    /// @todo Implement reset
    if (pThis->fCableConnected)
        STATUS = VNET_S_LINK_UP;
    else
        STATUS = 0;
    Log(("%s vnetIoCb_Reset: Link is %s\n", INSTANCE(pThis), pThis->fCableConnected ? "up" : "down"));

    /*
     * By default we pass all packets up since the older guests cannot control
     * virtio mode.
     */
    pThis->fPromiscuous      = true;
    pThis->fAllMulti         = false;
    pThis->nMacFilterEntries = 0;
    memset(pThis->aMacFilter,  0, VNET_MAC_FILTER_LEN * sizeof(RTMAC));
    memset(pThis->aVlanFilter, 0, sizeof(pThis->aVlanFilter));
    pThis->uIsTransmitting   = 0;
#ifndef IN_RING3
    return VINF_IOM_R3_IOPORT_WRITE;
#else
    if (pThis->pDrv)
        pThis->pDrv->pfnSetPromiscuousMode(pThis->pDrv, true);
    return VINF_SUCCESS;
#endif
}

#ifdef IN_RING3

/**
 * Wakeup the RX thread.
 */
static void vnetR3WakeupReceive(PPDMDEVINS pDevIns)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    if (    pThis->fMaybeOutOfSpace
        &&  pThis->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        STAM_COUNTER_INC(&pThis->StatRxOverflowWakeup);
        Log(("%s Waking up Out-of-RX-space semaphore\n",  INSTANCE(pThis)));
/**
 * @todo r=bird: We can wake stuff up from ring-0 too, see vmsvga, nvme,
 *        buslogic, lsilogic, ata, ahci, xhci.  Also, please address similar
 *        TODO in E1000.
 *
 *        The API Is SUPSem*, btw.
 */
        RTSemEventSignal(pThis->hEventMoreRxDescAvail);
    }
}


/**
 * Helper function that raises an interrupt if the guest is ready to receive it.
 */
static int vnetR3RaiseInterrupt(PVNETSTATE pThis, int rcBusy, uint8_t u8IntCause)
{
    if (pThis->VPCI.uStatus & VPCI_STATUS_DRV_OK)
        return vpciRaiseInterrupt(&pThis->VPCI, rcBusy, u8IntCause);
    return rcBusy;
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
 * @param  pThis        The Virtual I/O network device state.
 */
static void vnetR3TempLinkDown(PVNETSTATE pThis)
{
    if (STATUS & VNET_S_LINK_UP)
    {
        STATUS &= ~VNET_S_LINK_UP;
        vnetR3RaiseInterrupt(pThis, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        /* Restore the link back in 5 seconds. */
        int rc = TMTimerSetMillies(pThis->pLinkUpTimer, pThis->cMsLinkUpDelay);
        AssertRC(rc);
        Log(("%s vnetR3TempLinkDown: Link is down temporarily\n", INSTANCE(pThis)));
    }
}


/**
 * @callback_method_impl{FNTMTIMERDEV, Link Up Timer handler.}
 */
static DECLCALLBACK(void) vnetR3LinkUpTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RT_NOREF(pTimer);
    PVNETSTATE pThis = (PVNETSTATE)pvUser;

    int rc = vnetR3CsEnter(pThis, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return;
    STATUS |= VNET_S_LINK_UP;
    vnetR3RaiseInterrupt(pThis, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
    vnetR3WakeupReceive(pDevIns);
    vnetR3CsLeave(pThis);
    Log(("%s vnetR3LinkUpTimer: Link is up\n", INSTANCE(pThis)));
    if (pThis->pDrv)
        pThis->pDrv->pfnNotifyLinkChanged(pThis->pDrv, PDMNETWORKLINKSTATE_UP);
}


/**
 * @callback_method_impl{FNPDMQUEUEDEV, Handler for the wakeup signaller queue.}
 */
static DECLCALLBACK(bool) vnetR3CanRxQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    RT_NOREF(pItem);
    vnetR3WakeupReceive(pDevIns);
    return true;
}

#endif /* IN_RING3 */

/**
 * This function is called when the driver becomes ready.
 *
 * @param   pThis      The device state structure.
 */
static DECLCALLBACK(void) vnetIoCb_Ready(void *pvState)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    Log(("%s Driver became ready, waking up RX thread...\n", INSTANCE(pThis)));
/**
 * @todo r=bird: We can wake stuff up from ring-0 too, see vmsvga, nvme,
 *        buslogic, lsilogic, ata, ahci, xhci.  Also, please address similar
 *        TODO in E1000.
 *
 *        The API Is SUPSem*, btw.
 */
#ifdef IN_RING3
    vnetR3WakeupReceive(pThis->VPCI.CTX_SUFF(pDevIns));
#else
    PPDMQUEUEITEMCORE pItem = PDMQueueAlloc(pThis->CTX_SUFF(pCanRxQueue));
    if (pItem)
        PDMQueueInsert(pThis->CTX_SUFF(pCanRxQueue), pItem);
#endif
}


/**
 * I/O port callbacks.
 */
static const VPCIIOCALLBACKS g_IOCallbacks =
{
     vnetIoCb_GetHostFeatures,
     vnetIoCb_GetHostMinimalFeatures,
     vnetIoCb_SetHostFeatures,
     vnetIoCb_GetConfig,
     vnetIoCb_SetConfig,
     vnetIoCb_Reset,
     vnetIoCb_Ready,
};


/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) vnetIOPortIn(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t *pu32, unsigned cb)
{
    return vpciIOPortIn(pDevIns, pvUser, port, pu32, cb, &g_IOCallbacks);
}


/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) vnetIOPortOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t u32, unsigned cb)
{
    return vpciIOPortOut(pDevIns, pvUser, port, u32, cb, &g_IOCallbacks);
}


#ifdef IN_RING3

/**
 * Check if the device can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @remarks As a side effect this function enables queue notification
 *          if it cannot receive because the queue is empty.
 *          It disables notification if it can receive.
 *
 * @returns VERR_NET_NO_BUFFER_SPACE if it cannot.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  RX
 */
static int vnetR3CanReceive(PVNETSTATE pThis)
{
    int rc = vnetCsRxEnter(pThis, VERR_SEM_BUSY);
    AssertRCReturn(rc, rc);

    LogFlow(("%s vnetR3CanReceive\n", INSTANCE(pThis)));
    if (!(pThis->VPCI.uStatus & VPCI_STATUS_DRV_OK))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (!vqueueIsReady(&pThis->VPCI, pThis->pRxQueue))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (vqueueIsEmpty(&pThis->VPCI, pThis->pRxQueue))
    {
        vringSetNotification(&pThis->VPCI, &pThis->pRxQueue->VRing, true);
        rc = VERR_NET_NO_BUFFER_SPACE;
    }
    else
    {
        vringSetNotification(&pThis->VPCI, &pThis->pRxQueue->VRing, false);
        rc = VINF_SUCCESS;
    }

    LogFlow(("%s vnetR3CanReceive -> %Rrc\n", INSTANCE(pThis), rc));
    vnetCsRxLeave(pThis);
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnWaitReceiveAvail}
 */
static DECLCALLBACK(int) vnetR3NetworkDown_WaitReceiveAvail(PPDMINETWORKDOWN pInterface, RTMSINTERVAL cMillies)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkDown);
    LogFlow(("%s vnetR3NetworkDown_WaitReceiveAvail(cMillies=%u)\n", INSTANCE(pThis), cMillies));
    int rc = vnetR3CanReceive(pThis);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    if (RT_UNLIKELY(cMillies == 0))
        return VERR_NET_NO_BUFFER_SPACE;

    rc = VERR_INTERRUPTED;
    ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, true);
    STAM_PROFILE_START(&pThis->StatRxOverflow, a);

    VMSTATE enmVMState;
    while (RT_LIKELY(   (enmVMState = PDMDevHlpVMState(pThis->VPCI.CTX_SUFF(pDevIns))) == VMSTATE_RUNNING
                     ||  enmVMState == VMSTATE_RUNNING_LS))
    {
        int rc2 = vnetR3CanReceive(pThis);
        if (RT_SUCCESS(rc2))
        {
            rc = VINF_SUCCESS;
            break;
        }
        Log(("%s vnetR3NetworkDown_WaitReceiveAvail: waiting cMillies=%u...\n", INSTANCE(pThis), cMillies));
        RTSemEventWait(pThis->hEventMoreRxDescAvail, cMillies);
    }
    STAM_PROFILE_STOP(&pThis->StatRxOverflow, a);
    ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, false);

    LogFlow(("%s vnetR3NetworkDown_WaitReceiveAvail -> %d\n", INSTANCE(pThis), rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) vnetQueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, VPCI.IBase);
    Assert(&pThis->VPCI.IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKDOWN, &pThis->INetworkDown);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThis->INetworkConfig);
    return vpciQueryInterface(pInterface, pszIID);
}

/**
 * Returns true if it is a broadcast packet.
 *
 * @returns true if destination address indicates broadcast.
 * @param   pvBuf           The ethernet packet.
 */
DECLINLINE(bool) vnetR3IsBroadcast(const void *pvBuf)
{
    static const uint8_t s_abBcastAddr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    return memcmp(pvBuf, s_abBcastAddr, sizeof(s_abBcastAddr)) == 0;
}

/**
 * Returns true if it is a multicast packet.
 *
 * @remarks returns true for broadcast packets as well.
 * @returns true if destination address indicates multicast.
 * @param   pvBuf           The ethernet packet.
 */
DECLINLINE(bool) vnetR3IsMulticast(const void *pvBuf)
{
    return (*(char*)pvBuf) & 1;
}

/**
 * Determines if the packet is to be delivered to upper layer.
 *
 * @returns true if packet is intended for this node.
 * @param   pThis          Pointer to the state structure.
 * @param   pvBuf           The ethernet packet.
 * @param   cb              Number of bytes available in the packet.
 */
static bool vnetR3AddressFilter(PVNETSTATE pThis, const void *pvBuf, size_t cb)
{
    if (pThis->fPromiscuous)
        return true;

    /* Ignore everything outside of our VLANs */
    uint16_t *u16Ptr = (uint16_t*)pvBuf;
    /* Compare TPID with VLAN Ether Type */
    if (   u16Ptr[6] == RT_H2BE_U16(0x8100)
        && !ASMBitTest(pThis->aVlanFilter, RT_BE2H_U16(u16Ptr[7]) & 0xFFF))
    {
        Log4(("%s vnetR3AddressFilter: not our VLAN, returning false\n", INSTANCE(pThis)));
        return false;
    }

    if (vnetR3IsBroadcast(pvBuf))
        return true;

    if (pThis->fAllMulti && vnetR3IsMulticast(pvBuf))
        return true;

    if (!memcmp(pThis->config.mac.au8, pvBuf, sizeof(RTMAC)))
        return true;
    Log4(("%s vnetR3AddressFilter: %RTmac (conf) != %RTmac (dest)\n", INSTANCE(pThis), pThis->config.mac.au8, pvBuf));

    for (unsigned i = 0; i < pThis->nMacFilterEntries; i++)
        if (!memcmp(&pThis->aMacFilter[i], pvBuf, sizeof(RTMAC)))
            return true;

    Log2(("%s vnetR3AddressFilter: failed all tests, returning false, packet dump follows:\n", INSTANCE(pThis)));
    vnetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");

    return false;
}

/**
 * Pad and store received packet.
 *
 * @remarks Make sure that the packet appears to upper layer as one coming
 *          from real Ethernet: pad it and insert FCS.
 *
 * @returns VBox status code.
 * @param   pThis          The device state structure.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static int vnetR3HandleRxPacket(PVNETSTATE pThis, const void *pvBuf, size_t cb, PCPDMNETWORKGSO pGso)
{
    VNETHDRMRX  Hdr;
    unsigned    cbHdr;
    RTGCPHYS    addrHdrMrx = 0;

    if (pGso)
    {
        Log2(("%s vnetR3HandleRxPacket: gso type=%x cbHdrsTotal=%u cbHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
              INSTANCE(pThis), pGso->u8Type, pGso->cbHdrsTotal, pGso->cbHdrsSeg, pGso->cbMaxSeg, pGso->offHdr1, pGso->offHdr2));
        Hdr.Hdr.u8Flags = VNETHDR_F_NEEDS_CSUM;
        switch (pGso->u8Type)
        {
            case PDMNETWORKGSOTYPE_IPV4_TCP:
                Hdr.Hdr.u8GSOType = VNETHDR_GSO_TCPV4;
                Hdr.Hdr.u16CSumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV6_TCP:
                Hdr.Hdr.u8GSOType = VNETHDR_GSO_TCPV6;
                Hdr.Hdr.u16CSumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV4_UDP:
                Hdr.Hdr.u8GSOType = VNETHDR_GSO_UDP;
                Hdr.Hdr.u16CSumOffset = RT_OFFSETOF(RTNETUDP, uh_sum);
                break;
            default:
                return VERR_INVALID_PARAMETER;
        }
        Hdr.Hdr.u16HdrLen = pGso->cbHdrsTotal;
        Hdr.Hdr.u16GSOSize = pGso->cbMaxSeg;
        Hdr.Hdr.u16CSumStart = pGso->offHdr2;
        STAM_REL_COUNTER_INC(&pThis->StatReceiveGSO);
    }
    else
    {
        Hdr.Hdr.u8Flags   = 0;
        Hdr.Hdr.u8GSOType = VNETHDR_GSO_NONE;
    }

    if (vnetR3MergeableRxBuffers(pThis))
        cbHdr = sizeof(VNETHDRMRX);
    else
        cbHdr = sizeof(VNETHDR);

    vnetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");

    unsigned int uOffset = 0;
    unsigned int nElem;
    for (nElem = 0; uOffset < cb; nElem++)
    {
        VQUEUEELEM elem;
        unsigned int nSeg = 0, uElemSize = 0, cbReserved = 0;

        if (!vqueueGet(&pThis->VPCI, pThis->pRxQueue, &elem))
        {
            /*
             * @todo: It is possible to run out of RX buffers if only a few
             * were added and we received a big packet.
             */
            Log(("%s vnetR3HandleRxPacket: Suddenly there is no space in receive queue!\n", INSTANCE(pThis)));
            return VERR_INTERNAL_ERROR;
        }

        if (elem.nIn < 1)
        {
            Log(("%s vnetR3HandleRxPacket: No writable descriptors in receive queue!\n", INSTANCE(pThis)));
            return VERR_INTERNAL_ERROR;
        }

        if (nElem == 0)
        {
            if (vnetR3MergeableRxBuffers(pThis))
            {
                addrHdrMrx = elem.aSegsIn[nSeg].addr;
                cbReserved = cbHdr;
            }
            else
            {
                /* The very first segment of the very first element gets the header. */
                if (elem.aSegsIn[nSeg].cb != sizeof(VNETHDR))
                {
                    Log(("%s vnetR3HandleRxPacket: The first descriptor does match the header size!\n", INSTANCE(pThis)));
                    return VERR_INTERNAL_ERROR;
                }
                elem.aSegsIn[nSeg++].pv = &Hdr;
            }
            uElemSize += cbHdr;
        }
        while (nSeg < elem.nIn && uOffset < cb)
        {
            unsigned int uSize = (unsigned int)RT_MIN(elem.aSegsIn[nSeg].cb - (nSeg?0:cbReserved),
                                                      cb - uOffset);
            elem.aSegsIn[nSeg++].pv = (uint8_t*)pvBuf + uOffset;
            uOffset += uSize;
            uElemSize += uSize;
        }
        STAM_PROFILE_START(&pThis->StatReceiveStore, a);
        vqueuePut(&pThis->VPCI, pThis->pRxQueue, &elem, uElemSize, cbReserved);
        STAM_PROFILE_STOP(&pThis->StatReceiveStore, a);
        if (!vnetR3MergeableRxBuffers(pThis))
            break;
        cbReserved = 0;
    }
    if (vnetR3MergeableRxBuffers(pThis))
    {
        Hdr.u16NumBufs = nElem;
        int rc = PDMDevHlpPCIPhysWrite(pThis->VPCI.CTX_SUFF(pDevIns), addrHdrMrx,
                                       &Hdr, sizeof(Hdr));
        if (RT_FAILURE(rc))
        {
            Log(("%s vnetR3HandleRxPacket: Failed to write merged RX buf header: %Rrc\n", INSTANCE(pThis), rc));
            return rc;
        }
    }
    vqueueSync(&pThis->VPCI, pThis->pRxQueue);
    if (uOffset < cb)
    {
        Log(("%s vnetR3HandleRxPacket: Packet did not fit into RX queue (packet size=%u)!\n", INSTANCE(pThis), cb));
        return VERR_TOO_MUCH_DATA;
    }

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceiveGso}
 */
static DECLCALLBACK(int) vnetR3NetworkDown_ReceiveGso(PPDMINETWORKDOWN pInterface,
                                                    const void *pvBuf, size_t cb,
                                                    PCPDMNETWORKGSO pGso)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkDown);

    if (pGso)
    {
        uint32_t uFeatures = pThis->VPCI.uGuestFeatures;

        switch (pGso->u8Type)
        {
            case PDMNETWORKGSOTYPE_IPV4_TCP:
                uFeatures &= VNET_F_GUEST_TSO4;
                break;
            case PDMNETWORKGSOTYPE_IPV6_TCP:
                uFeatures &= VNET_F_GUEST_TSO6;
                break;
            case PDMNETWORKGSOTYPE_IPV4_UDP:
            case PDMNETWORKGSOTYPE_IPV6_UDP:
                uFeatures &= VNET_F_GUEST_UFO;
                break;
            default:
                uFeatures = 0;
                break;
        }
        if (!uFeatures)
        {
            Log2(("%s vnetR3NetworkDown_ReceiveGso: GSO type (0x%x) not supported\n", INSTANCE(pThis), pGso->u8Type));
            return VERR_NOT_SUPPORTED;
        }
    }

    Log2(("%s vnetR3NetworkDown_ReceiveGso: pvBuf=%p cb=%u pGso=%p\n", INSTANCE(pThis), pvBuf, cb, pGso));
    int rc = vnetR3CanReceive(pThis);
    if (RT_FAILURE(rc))
        return rc;

    /* Drop packets if VM is not running or cable is disconnected. */
    VMSTATE enmVMState = PDMDevHlpVMState(pThis->VPCI.CTX_SUFF(pDevIns));
    if ((   enmVMState != VMSTATE_RUNNING
         && enmVMState != VMSTATE_RUNNING_LS)
        || !(STATUS & VNET_S_LINK_UP))
        return VINF_SUCCESS;

    STAM_PROFILE_START(&pThis->StatReceive, a);
    vpciSetReadLed(&pThis->VPCI, true);
    if (vnetR3AddressFilter(pThis, pvBuf, cb))
    {
        rc = vnetCsRxEnter(pThis, VERR_SEM_BUSY);
        if (RT_SUCCESS(rc))
        {
            rc = vnetR3HandleRxPacket(pThis, pvBuf, cb, pGso);
            STAM_REL_COUNTER_ADD(&pThis->StatReceiveBytes, cb);
            vnetCsRxLeave(pThis);
        }
    }
    vpciSetReadLed(&pThis->VPCI, false);
    STAM_PROFILE_STOP(&pThis->StatReceive, a);
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceive}
 */
static DECLCALLBACK(int) vnetR3NetworkDown_Receive(PPDMINETWORKDOWN pInterface, const void *pvBuf, size_t cb)
{
    return vnetR3NetworkDown_ReceiveGso(pInterface, pvBuf, cb, NULL);
}

/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetR3NetworkConfig_GetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkConfig);
    memcpy(pMac, pThis->config.mac.au8, sizeof(RTMAC));
    return VINF_SUCCESS;
}

/**
 * Gets the new link state.
 *
 * @returns The current link state.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) vnetR3NetworkConfig_GetLinkState(PPDMINETWORKCONFIG pInterface)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkConfig);
    if (STATUS & VNET_S_LINK_UP)
        return PDMNETWORKLINKSTATE_UP;
    return PDMNETWORKLINKSTATE_DOWN;
}


/**
 * Sets the new link state.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmState        The new link state
 */
static DECLCALLBACK(int) vnetR3NetworkConfig_SetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkConfig);
    bool fOldUp = !!(STATUS & VNET_S_LINK_UP);
    bool fNewUp = enmState == PDMNETWORKLINKSTATE_UP;

    Log(("%s vnetR3NetworkConfig_SetLinkState: enmState=%d\n", INSTANCE(pThis), enmState));
    if (enmState == PDMNETWORKLINKSTATE_DOWN_RESUME)
    {
        if (fOldUp)
        {
            /*
             * We bother to bring the link down only if it was up previously. The UP link state
             * notification will be sent when the link actually goes up in vnetR3LinkUpTimer().
             */
            vnetR3TempLinkDown(pThis);
            if (pThis->pDrv)
                pThis->pDrv->pfnNotifyLinkChanged(pThis->pDrv, enmState);
        }
    }
    else if (fNewUp != fOldUp)
    {
        if (fNewUp)
        {
            Log(("%s Link is up\n", INSTANCE(pThis)));
            pThis->fCableConnected = true;
            STATUS |= VNET_S_LINK_UP;
            vnetR3RaiseInterrupt(pThis, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        else
        {
            /* The link was brought down explicitly, make sure it won't come up by timer.  */
            TMTimerStop(pThis->pLinkUpTimer);
            Log(("%s Link is down\n", INSTANCE(pThis)));
            pThis->fCableConnected = false;
            STATUS &= ~VNET_S_LINK_UP;
            vnetR3RaiseInterrupt(pThis, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        if (pThis->pDrv)
            pThis->pDrv->pfnNotifyLinkChanged(pThis->pDrv, enmState);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vnetR3QueueReceive(void *pvState, PVQUEUE pQueue)
{
    RT_NOREF(pQueue);
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    Log(("%s Receive buffers has been added, waking up receive thread.\n", INSTANCE(pThis)));
    vnetR3WakeupReceive(pThis->VPCI.CTX_SUFF(pDevIns));
}

/**
 * Sets up the GSO context according to the Virtio header.
 *
 * @param   pGso                The GSO context to setup.
 * @param   pCtx                The context descriptor.
 */
DECLINLINE(PPDMNETWORKGSO) vnetR3SetupGsoCtx(PPDMNETWORKGSO pGso, VNETHDR const *pHdr)
{
    pGso->u8Type = PDMNETWORKGSOTYPE_INVALID;

    if (pHdr->u8GSOType & VNETHDR_GSO_ECN)
    {
        AssertMsgFailed(("Unsupported flag in virtio header: ECN\n"));
        return NULL;
    }
    switch (pHdr->u8GSOType & ~VNETHDR_GSO_ECN)
    {
        case VNETHDR_GSO_TCPV4:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV4_TCP;
            pGso->cbHdrsSeg = pHdr->u16HdrLen;
            break;
        case VNETHDR_GSO_TCPV6:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV6_TCP;
            pGso->cbHdrsSeg = pHdr->u16HdrLen;
            break;
        case VNETHDR_GSO_UDP:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV4_UDP;
            pGso->cbHdrsSeg = pHdr->u16CSumStart;
            break;
        default:
            return NULL;
    }
    if (pHdr->u8Flags & VNETHDR_F_NEEDS_CSUM)
        pGso->offHdr2  = pHdr->u16CSumStart;
    else
    {
        AssertMsgFailed(("GSO without checksum offloading!\n"));
        return NULL;
    }
    pGso->offHdr1     = sizeof(RTNETETHERHDR);
    pGso->cbHdrsTotal = pHdr->u16HdrLen;
    pGso->cbMaxSeg    = pHdr->u16GSOSize;
    return pGso;
}

DECLINLINE(uint16_t) vnetR3CSum16(const void *pvBuf, size_t cb)
{
    uint32_t  csum = 0;
    uint16_t *pu16 = (uint16_t *)pvBuf;

    while (cb > 1)
    {
        csum += *pu16++;
        cb -= 2;
    }
    if (cb)
        csum += *(uint8_t*)pu16;
    while (csum >> 16)
        csum = (csum >> 16) + (csum & 0xFFFF);
    return ~csum;
}

DECLINLINE(void) vnetR3CompleteChecksum(uint8_t *pBuf, size_t cbSize, uint16_t uStart, uint16_t uOffset)
{
    AssertReturnVoid(uStart < cbSize);
    AssertReturnVoid(uStart + uOffset + sizeof(uint16_t) <= cbSize);
    *(uint16_t*)(pBuf + uStart + uOffset) = vnetR3CSum16(pBuf + uStart, cbSize - uStart);
}

static bool vnetR3ReadHeader(PVNETSTATE pThis, RTGCPHYS GCPhys, PVNETHDR pHdr, uint32_t cbMax)
{
    int rc = PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns), GCPhys, pHdr, sizeof(*pHdr)); /** @todo r=bird: Why not PDMDevHlpPCIPhysRead? */
    if (RT_FAILURE(rc))
        return false;

    Log4(("virtio-net: header flags=%x gso-type=%x len=%x gso-size=%x csum-start=%x csum-offset=%x cb=%x\n",
          pHdr->u8Flags, pHdr->u8GSOType, pHdr->u16HdrLen, pHdr->u16GSOSize, pHdr->u16CSumStart, pHdr->u16CSumOffset, cbMax));

    if (pHdr->u8GSOType)
    {
        uint32_t u32MinHdrSize;

        /* Segmentation offloading cannot be done without checksumming. */
        if (RT_UNLIKELY(!(pHdr->u8Flags & VNETHDR_F_NEEDS_CSUM)))
            return false;
        /* We do not support ECN. */
        if (RT_UNLIKELY(pHdr->u8GSOType & VNETHDR_GSO_ECN))
            return false;
        switch (pHdr->u8GSOType)
        {
            case VNETHDR_GSO_TCPV4:
            case VNETHDR_GSO_TCPV6:
                u32MinHdrSize = sizeof(RTNETTCP);
                break;
            case VNETHDR_GSO_UDP:
                u32MinHdrSize = 0;
                break;
            default:
                return false;
        }
        /* Header+MSS must not exceed the packet size. */
        if (RT_UNLIKELY(u32MinHdrSize + pHdr->u16CSumStart + pHdr->u16GSOSize > cbMax))
            return false;
    }
    /* Checksum must fit into the frame (validating both checksum fields). */
    if (   (pHdr->u8Flags & VNETHDR_F_NEEDS_CSUM)
        && sizeof(uint16_t) + pHdr->u16CSumStart + pHdr->u16CSumOffset > cbMax)
        return false;
    Log4(("virtio-net: return true\n"));
    return true;
}

static int vnetR3TransmitFrame(PVNETSTATE pThis, PPDMSCATTERGATHER pSgBuf, PPDMNETWORKGSO pGso, PVNETHDR pHdr)
{
    vnetR3PacketDump(pThis, (uint8_t *)pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed, "--> Outgoing");
    if (pGso)
    {
        /* Some guests (RHEL) may report HdrLen excluding transport layer header! */
        /*
         * We cannot use cdHdrs provided by the guest because of different ways
         * it gets filled out by different versions of kernels.
         */
        //if (pGso->cbHdrs < pHdr->u16CSumStart + pHdr->u16CSumOffset + 2)
        {
            Log4(("%s vnetR3TransmitPendingPackets: HdrLen before adjustment %d.\n",
                  INSTANCE(pThis), pGso->cbHdrsTotal));
            switch (pGso->u8Type)
            {
                case PDMNETWORKGSOTYPE_IPV4_TCP:
                case PDMNETWORKGSOTYPE_IPV6_TCP:
                    pGso->cbHdrsTotal = pHdr->u16CSumStart +
                        ((PRTNETTCP)(((uint8_t*)pSgBuf->aSegs[0].pvSeg) + pHdr->u16CSumStart))->th_off * 4;
                    pGso->cbHdrsSeg   = pGso->cbHdrsTotal;
                    break;
                case PDMNETWORKGSOTYPE_IPV4_UDP:
                    pGso->cbHdrsTotal = (uint8_t)(pHdr->u16CSumStart + sizeof(RTNETUDP));
                    pGso->cbHdrsSeg   = pHdr->u16CSumStart;
                    break;
            }
            /* Update GSO structure embedded into the frame */
            ((PPDMNETWORKGSO)pSgBuf->pvUser)->cbHdrsTotal = pGso->cbHdrsTotal;
            ((PPDMNETWORKGSO)pSgBuf->pvUser)->cbHdrsSeg   = pGso->cbHdrsSeg;
            Log4(("%s vnetR3TransmitPendingPackets: adjusted HdrLen to %d.\n",
                  INSTANCE(pThis), pGso->cbHdrsTotal));
        }
        Log2(("%s vnetR3TransmitPendingPackets: gso type=%x cbHdrsTotal=%u cbHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
              INSTANCE(pThis), pGso->u8Type, pGso->cbHdrsTotal, pGso->cbHdrsSeg, pGso->cbMaxSeg, pGso->offHdr1, pGso->offHdr2));
        STAM_REL_COUNTER_INC(&pThis->StatTransmitGSO);
    }
    else if (pHdr->u8Flags & VNETHDR_F_NEEDS_CSUM)
    {
        STAM_REL_COUNTER_INC(&pThis->StatTransmitCSum);
        /*
         * This is not GSO frame but checksum offloading is requested.
         */
        vnetR3CompleteChecksum((uint8_t*)pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed,
                             pHdr->u16CSumStart, pHdr->u16CSumOffset);
    }

    return pThis->pDrv->pfnSendBuf(pThis->pDrv, pSgBuf, false);
}

static void vnetR3TransmitPendingPackets(PVNETSTATE pThis, PVQUEUE pQueue, bool fOnWorkerThread)
{
    /*
     * Only one thread is allowed to transmit at a time, others should skip
     * transmission as the packets will be picked up by the transmitting
     * thread.
     */
    if (!ASMAtomicCmpXchgU32(&pThis->uIsTransmitting, 1, 0))
        return;

    if ((pThis->VPCI.uStatus & VPCI_STATUS_DRV_OK) == 0)
    {
        Log(("%s Ignoring transmit requests from non-existent driver (status=0x%x).\n", INSTANCE(pThis), pThis->VPCI.uStatus));
        return;
    }

    if (!pThis->fCableConnected)
    {
        Log(("%s Ignoring transmit requests while cable is disconnected.\n", INSTANCE(pThis)));
        return;
    }

    PPDMINETWORKUP pDrv = pThis->pDrv;
    if (pDrv)
    {
        int rc = pDrv->pfnBeginXmit(pDrv, fOnWorkerThread);
        Assert(rc == VINF_SUCCESS || rc == VERR_TRY_AGAIN);
        if (rc == VERR_TRY_AGAIN)
        {
            ASMAtomicWriteU32(&pThis->uIsTransmitting, 0);
            return;
        }
    }

    unsigned int cbHdr;
    if (vnetR3MergeableRxBuffers(pThis))
        cbHdr = sizeof(VNETHDRMRX);
    else
        cbHdr = sizeof(VNETHDR);

    Log3(("%s vnetR3TransmitPendingPackets: About to transmit %d pending packets\n",
          INSTANCE(pThis), vringReadAvailIndex(&pThis->VPCI, &pThis->pTxQueue->VRing) - pThis->pTxQueue->uNextAvailIndex));

    vpciSetWriteLed(&pThis->VPCI, true);

    /*
     * Do not remove descriptors from available ring yet, try to allocate the
     * buffer first.
     */
    VQUEUEELEM elem; /* This bugger is big! ~48KB on 64-bit hosts. */
    while (vqueuePeek(&pThis->VPCI, pQueue, &elem))
    {
        unsigned int uOffset = 0;
        if (elem.nOut < 2 || elem.aSegsOut[0].cb != cbHdr)
        {
            Log(("%s vnetR3QueueTransmit: The first segment is not the header! (%u < 2 || %u != %u).\n",
                 INSTANCE(pThis), elem.nOut, elem.aSegsOut[0].cb, cbHdr));
            break; /* For now we simply ignore the header, but it must be there anyway! */
        }
        RT_UNTRUSTED_VALIDATED_FENCE();

        VNETHDR Hdr;
        unsigned int uSize = 0;
        STAM_PROFILE_ADV_START(&pThis->StatTransmit, a);

        /* Compute total frame size. */
        for (unsigned int i = 1; i < elem.nOut && uSize < VNET_MAX_FRAME_SIZE; i++)
            uSize += elem.aSegsOut[i].cb;
        Log5(("%s vnetR3TransmitPendingPackets: complete frame is %u bytes.\n", INSTANCE(pThis), uSize));
        Assert(uSize <= VNET_MAX_FRAME_SIZE);

        /* Truncate oversized frames. */
        if (uSize > VNET_MAX_FRAME_SIZE)
            uSize = VNET_MAX_FRAME_SIZE;
        if (pThis->pDrv && vnetR3ReadHeader(pThis, elem.aSegsOut[0].addr, &Hdr, uSize))
        {
            RT_UNTRUSTED_VALIDATED_FENCE();
            STAM_REL_COUNTER_INC(&pThis->StatTransmitPackets);
            STAM_PROFILE_START(&pThis->StatTransmitSend, a);

            PDMNETWORKGSO Gso;
            PDMNETWORKGSO *pGso = vnetR3SetupGsoCtx(&Gso, &Hdr);

            /** @todo Optimize away the extra copying! (lazy bird) */
            PPDMSCATTERGATHER pSgBuf;
            int rc = pThis->pDrv->pfnAllocBuf(pThis->pDrv, uSize, pGso, &pSgBuf);
            if (RT_SUCCESS(rc))
            {
                Assert(pSgBuf->cSegs == 1);
                pSgBuf->cbUsed = uSize;

                /* Assemble a complete frame. */
                for (unsigned int i = 1; i < elem.nOut && uSize > 0; i++)
                {
                    unsigned int cbSegment = RT_MIN(uSize, elem.aSegsOut[i].cb);
                    PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns), elem.aSegsOut[i].addr,
                                      ((uint8_t*)pSgBuf->aSegs[0].pvSeg) + uOffset,
                                      cbSegment);
                    uOffset += cbSegment;
                    uSize -= cbSegment;
                }
                rc = vnetR3TransmitFrame(pThis, pSgBuf, pGso, &Hdr);
            }
            else
            {
                Log4(("virtio-net: failed to allocate SG buffer: size=%u rc=%Rrc\n", uSize, rc));
                STAM_PROFILE_STOP(&pThis->StatTransmitSend, a);
                STAM_PROFILE_ADV_STOP(&pThis->StatTransmit, a);
                /* Stop trying to fetch TX descriptors until we get more bandwidth. */
                break;
            }

            STAM_PROFILE_STOP(&pThis->StatTransmitSend, a);
            STAM_REL_COUNTER_ADD(&pThis->StatTransmitBytes, uOffset);
        }

        /* Remove this descriptor chain from the available ring */
        vqueueSkip(&pThis->VPCI, pQueue);
        vqueuePut(&pThis->VPCI, pQueue, &elem, sizeof(VNETHDR) + uOffset);
        vqueueSync(&pThis->VPCI, pQueue);
        STAM_PROFILE_ADV_STOP(&pThis->StatTransmit, a);
    }
    vpciSetWriteLed(&pThis->VPCI, false);

    if (pDrv)
        pDrv->pfnEndXmit(pDrv);
    ASMAtomicWriteU32(&pThis->uIsTransmitting, 0);
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnXmitPending}
 */
static DECLCALLBACK(void) vnetR3NetworkDown_XmitPending(PPDMINETWORKDOWN pInterface)
{
    PVNETSTATE pThis = RT_FROM_MEMBER(pInterface, VNETSTATE, INetworkDown);
    STAM_COUNTER_INC(&pThis->StatTransmitByNetwork);
    vnetR3TransmitPendingPackets(pThis, pThis->pTxQueue, false /*fOnWorkerThread*/);
}

# ifdef VNET_TX_DELAY

static DECLCALLBACK(void) vnetR3QueueTransmit(void *pvState, PVQUEUE pQueue)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;

    if (TMTimerIsActive(pThis->CTX_SUFF(pTxTimer)))
    {
        TMTimerStop(pThis->CTX_SUFF(pTxTimer));
        Log3(("%s vnetR3QueueTransmit: Got kicked with notification disabled, re-enable notification and flush TX queue\n", INSTANCE(pThis)));
        vnetR3TransmitPendingPackets(pThis, pQueue, false /*fOnWorkerThread*/);
        if (RT_FAILURE(vnetR3CsEnter(pThis, VERR_SEM_BUSY)))
            LogRel(("vnetR3QueueTransmit: Failed to enter critical section!/n"));
        else
        {
            vringSetNotification(&pThis->VPCI, &pThis->pTxQueue->VRing, true);
            vnetR3CsLeave(pThis);
        }
    }
    else
    {
        if (RT_FAILURE(vnetR3CsEnter(pThis, VERR_SEM_BUSY)))
            LogRel(("vnetR3QueueTransmit: Failed to enter critical section!/n"));
        else
        {
            vringSetNotification(&pThis->VPCI, &pThis->pTxQueue->VRing, false);
            TMTimerSetMicro(pThis->CTX_SUFF(pTxTimer), VNET_TX_DELAY);
            pThis->u64NanoTS = RTTimeNanoTS();
            vnetR3CsLeave(pThis);
        }
    }
}

/**
 * @callback_method_impl{FNTMTIMERDEV, Transmit Delay Timer handler.}
 */
static DECLCALLBACK(void) vnetR3TxTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RT_NOREF(pDevIns, pTimer);
    PVNETSTATE pThis = (PVNETSTATE)pvUser;

    uint32_t u32MicroDiff = (uint32_t)((RTTimeNanoTS() - pThis->u64NanoTS)/1000);
    if (u32MicroDiff < pThis->u32MinDiff)
        pThis->u32MinDiff = u32MicroDiff;
    if (u32MicroDiff > pThis->u32MaxDiff)
        pThis->u32MaxDiff = u32MicroDiff;
    pThis->u32AvgDiff = (pThis->u32AvgDiff * pThis->u32i + u32MicroDiff) / (pThis->u32i + 1);
    pThis->u32i++;
    Log3(("vnetR3TxTimer: Expired, diff %9d usec, avg %9d usec, min %9d usec, max %9d usec\n",
          u32MicroDiff, pThis->u32AvgDiff, pThis->u32MinDiff, pThis->u32MaxDiff));

//    Log3(("%s vnetR3TxTimer: Expired\n", INSTANCE(pThis)));
    vnetR3TransmitPendingPackets(pThis, pThis->pTxQueue, false /*fOnWorkerThread*/);
    if (RT_FAILURE(vnetR3CsEnter(pThis, VERR_SEM_BUSY)))
    {
        LogRel(("vnetR3TxTimer: Failed to enter critical section!/n"));
        return;
    }
    vringSetNotification(&pThis->VPCI, &pThis->pTxQueue->VRing, true);
    vnetR3CsLeave(pThis);
}

DECLINLINE(int) vnetR3CreateTxThreadAndEvent(PPDMDEVINS pDevIns, PVNETSTATE pThis)
{
    RT_NOREF(pDevIns, pThis);
    return VINF_SUCCESS;
}

DECLINLINE(void) vnetR3DestroyTxThreadAndEvent(PVNETSTATE pThis)
{
    RT_NOREF(pThis);
}

# else /* !VNET_TX_DELAY */

static DECLCALLBACK(int) vnetR3TxThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pDevIns);
    PVNETSTATE pThis = (PVNETSTATE)pThread->pvUser;
    int rc = VINF_SUCCESS;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        rc = SUPSemEventWaitNoResume(pThis->pSupDrvSession, pThis->hTxEvent, RT_INDEFINITE_WAIT);
        if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
            break;
        STAM_COUNTER_INC(&pThis->StatTransmitByThread);
        while (true)
        {
            vnetR3TransmitPendingPackets(pThis, pThis->pTxQueue, false /*fOnWorkerThread*/); /// @todo shouldn't it be true instead?
            Log(("vnetR3TxThread: enable kicking and get to sleep\n"));
            vringSetNotification(&pThis->VPCI, &pThis->pTxQueue->VRing, true);
            if (vqueueIsEmpty(&pThis->VPCI, pThis->pTxQueue))
                break;
            vringSetNotification(&pThis->VPCI, &pThis->pTxQueue->VRing, false);
        }
    }

    return rc;
}

/**
 * Unblock TX thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) vnetTxThreadWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pDevIns);
    PVNETSTATE pThis = (PVNETSTATE)pThread->pvUser;
    return SUPSemEventSignal(pThis->pSupDrvSession, pThis->hTxEvent);
}

static int vnetR3CreateTxThreadAndEvent(PPDMDEVINS pDevIns, PVNETSTATE pThis)
{
    int rc = SUPSemEventCreate(pThis->pSupDrvSession, &pThis->hTxEvent);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("VNET: Failed to create SUP event semaphore"));
    rc = PDMDevHlpThreadCreate(pDevIns, &pThis->pTxThread, pThis, vnetR3TxThread,
                               vnetTxThreadWakeUp, 0, RTTHREADTYPE_IO, INSTANCE(pThis));
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("VNET: Failed to create worker thread %s"), INSTANCE(pThis));
    return VINF_SUCCESS;
}

static void vnetR3DestroyTxThreadAndEvent(PVNETSTATE pThis)
{
    if (pThis->pTxThread)
    {
        int rcThread;
        /* Destroy the thread. */
        int rc = PDMR3ThreadDestroy(pThis->pTxThread, &rcThread);
        if (RT_FAILURE(rc) || RT_FAILURE(rcThread))
            AssertMsgFailed(("%s Failed to destroy async IO thread rc=%Rrc rcThread=%Rrc\n", __FUNCTION__, rc, rcThread));
        pThis->pTxThread = NULL;
    }
    if (pThis->hTxEvent != NIL_SUPSEMEVENT)
    {
        SUPSemEventClose(pThis->pSupDrvSession, pThis->hTxEvent);
        pThis->hTxEvent = NIL_SUPSEMEVENT;
    }
}

static DECLCALLBACK(void) vnetR3QueueTransmit(void *pvState, PVQUEUE pQueue)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;

    Log(("vnetR3QueueTransmit: disable kicking and wake up TX thread\n"));
    vringSetNotification(&pThis->VPCI, &pQueue->VRing, false);
    SUPSemEventSignal(pThis->pSupDrvSession, pThis->hTxEvent);
}

# endif /* !VNET_TX_DELAY */

static uint8_t vnetR3ControlRx(PVNETSTATE pThis, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint8_t u8Ack = VNET_OK;
    uint8_t fOn, fDrvWasPromisc = pThis->fPromiscuous | pThis->fAllMulti;
    PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &fOn, sizeof(fOn));
    Log(("%s vnetR3ControlRx: uCommand=%u fOn=%u\n", INSTANCE(pThis), pCtlHdr->u8Command, fOn));
    switch (pCtlHdr->u8Command)
    {
        case VNET_CTRL_CMD_RX_MODE_PROMISC:
            pThis->fPromiscuous = !!fOn;
            break;
        case VNET_CTRL_CMD_RX_MODE_ALLMULTI:
            pThis->fAllMulti = !!fOn;
            break;
        default:
            u8Ack = VNET_ERROR;
    }
    if (fDrvWasPromisc != (pThis->fPromiscuous | pThis->fAllMulti) && pThis->pDrv)
        pThis->pDrv->pfnSetPromiscuousMode(pThis->pDrv,
            (pThis->fPromiscuous | pThis->fAllMulti));

    return u8Ack;
}

static uint8_t vnetR3ControlMac(PVNETSTATE pThis, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint32_t nMacs = 0;

    if (pCtlHdr->u8Command != VNET_CTRL_CMD_MAC_TABLE_SET
        || pElem->nOut != 3
        || pElem->aSegsOut[1].cb < sizeof(nMacs)
        || pElem->aSegsOut[2].cb < sizeof(nMacs))
    {
        Log(("%s vnetR3ControlMac: Segment layout is wrong (u8Command=%u nOut=%u cb1=%u cb2=%u)\n",
             INSTANCE(pThis), pCtlHdr->u8Command, pElem->nOut, pElem->aSegsOut[1].cb, pElem->aSegsOut[2].cb));
        return VNET_ERROR;
    }

    /* Load unicast addresses */
    PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &nMacs, sizeof(nMacs));

    if (pElem->aSegsOut[1].cb < nMacs * sizeof(RTMAC) + sizeof(nMacs))
    {
        Log(("%s vnetR3ControlMac: The unicast mac segment is too small (nMacs=%u cb=%u)\n",
             INSTANCE(pThis), nMacs, pElem->aSegsOut[1].cb));
        return VNET_ERROR;
    }

    if (nMacs > VNET_MAC_FILTER_LEN)
    {
        Log(("%s vnetR3ControlMac: MAC table is too big, have to use promiscuous mode (nMacs=%u)\n", INSTANCE(pThis), nMacs));
        pThis->fPromiscuous = true;
    }
    else
    {
        if (nMacs)
            PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                              pElem->aSegsOut[1].addr + sizeof(nMacs),
                              pThis->aMacFilter, nMacs * sizeof(RTMAC));
        pThis->nMacFilterEntries = nMacs;
#ifdef DEBUG
        Log(("%s vnetR3ControlMac: unicast macs:\n", INSTANCE(pThis)));
        for(unsigned i = 0; i < nMacs; i++)
            Log(("         %RTmac\n", &pThis->aMacFilter[i]));
#endif /* DEBUG */
    }

    /* Load multicast addresses */
    PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[2].addr,
                      &nMacs, sizeof(nMacs));

    if (pElem->aSegsOut[2].cb < nMacs * sizeof(RTMAC) + sizeof(nMacs))
    {
        Log(("%s vnetR3ControlMac: The multicast mac segment is too small (nMacs=%u cb=%u)\n",
             INSTANCE(pThis), nMacs, pElem->aSegsOut[2].cb));
        return VNET_ERROR;
    }

    if (nMacs > VNET_MAC_FILTER_LEN - pThis->nMacFilterEntries)
    {
        Log(("%s vnetR3ControlMac: MAC table is too big, have to use allmulti mode (nMacs=%u)\n", INSTANCE(pThis), nMacs));
        pThis->fAllMulti = true;
    }
    else
    {
        if (nMacs)
            PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                              pElem->aSegsOut[2].addr + sizeof(nMacs),
                              &pThis->aMacFilter[pThis->nMacFilterEntries],
                              nMacs * sizeof(RTMAC));
#ifdef DEBUG
        Log(("%s vnetR3ControlMac: multicast macs:\n", INSTANCE(pThis)));
        for(unsigned i = 0; i < nMacs; i++)
            Log(("         %RTmac\n",
                 &pThis->aMacFilter[i+pThis->nMacFilterEntries]));
#endif /* DEBUG */
        pThis->nMacFilterEntries += nMacs;
    }

    return VNET_OK;
}

static uint8_t vnetR3ControlVlan(PVNETSTATE pThis, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint8_t  u8Ack = VNET_OK;
    uint16_t u16Vid;

    if (pElem->nOut != 2 || pElem->aSegsOut[1].cb != sizeof(u16Vid))
    {
        Log(("%s vnetR3ControlVlan: Segment layout is wrong (u8Command=%u nOut=%u cb=%u)\n",
             INSTANCE(pThis), pCtlHdr->u8Command, pElem->nOut, pElem->aSegsOut[1].cb));
        return VNET_ERROR;
    }

    PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &u16Vid, sizeof(u16Vid));

    if (u16Vid >= VNET_MAX_VID)
    {
        Log(("%s vnetR3ControlVlan: VLAN ID is out of range (VID=%u)\n", INSTANCE(pThis), u16Vid));
        return VNET_ERROR;
    }

    Log(("%s vnetR3ControlVlan: uCommand=%u VID=%u\n", INSTANCE(pThis), pCtlHdr->u8Command, u16Vid));

    switch (pCtlHdr->u8Command)
    {
        case VNET_CTRL_CMD_VLAN_ADD:
            ASMBitSet(pThis->aVlanFilter, u16Vid);
            break;
        case VNET_CTRL_CMD_VLAN_DEL:
            ASMBitClear(pThis->aVlanFilter, u16Vid);
            break;
        default:
            u8Ack = VNET_ERROR;
    }

    return u8Ack;
}


static DECLCALLBACK(void) vnetR3QueueControl(void *pvState, PVQUEUE pQueue)
{
    PVNETSTATE pThis = (PVNETSTATE)pvState;
    uint8_t u8Ack;
    VQUEUEELEM elem;
    while (vqueueGet(&pThis->VPCI, pQueue, &elem))
    {
        if (elem.nOut < 1 || elem.aSegsOut[0].cb < sizeof(VNETCTLHDR))
        {
            Log(("%s vnetR3QueueControl: The first 'out' segment is not the header! (%u < 1 || %u < %u).\n",
                 INSTANCE(pThis), elem.nOut, elem.aSegsOut[0].cb,sizeof(VNETCTLHDR)));
            break; /* Skip the element and hope the next one is good. */
        }
        if (   elem.nIn < 1
            || elem.aSegsIn[elem.nIn - 1].cb < sizeof(VNETCTLACK))
        {
            Log(("%s vnetR3QueueControl: The last 'in' segment is too small to hold the acknowledge! (%u < 1 || %u < %u).\n",
                 INSTANCE(pThis), elem.nIn, elem.aSegsIn[elem.nIn - 1].cb, sizeof(VNETCTLACK)));
            break; /* Skip the element and hope the next one is good. */
        }

        VNETCTLHDR CtlHdr;
        PDMDevHlpPhysRead(pThis->VPCI.CTX_SUFF(pDevIns),
                          elem.aSegsOut[0].addr,
                          &CtlHdr, sizeof(CtlHdr));
        switch (CtlHdr.u8Class)
        {
            case VNET_CTRL_CLS_RX_MODE:
                u8Ack = vnetR3ControlRx(pThis, &CtlHdr, &elem);
                break;
            case VNET_CTRL_CLS_MAC:
                u8Ack = vnetR3ControlMac(pThis, &CtlHdr, &elem);
                break;
            case VNET_CTRL_CLS_VLAN:
                u8Ack = vnetR3ControlVlan(pThis, &CtlHdr, &elem);
                break;
            default:
                u8Ack = VNET_ERROR;
        }
        Log(("%s Processed control message %u, ack=%u.\n", INSTANCE(pThis), CtlHdr.u8Class, u8Ack));
        PDMDevHlpPCIPhysWrite(pThis->VPCI.CTX_SUFF(pDevIns),
                              elem.aSegsIn[elem.nIn - 1].addr,
                              &u8Ack, sizeof(u8Ack));

        vqueuePut(&pThis->VPCI, pQueue, &elem, sizeof(u8Ack));
        vqueueSync(&pThis->VPCI, pQueue);
    }
}


/* -=-=-=-=- Saved state -=-=-=-=- */

/**
 * Saves the configuration.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The VNET state.
 * @param   pSSM        The handle to the saved state.
 */
static void vnetR3SaveConfig(PPDMDEVINS pDevIns, PVNETSTATE pThis, PSSMHANDLE pSSM)
{
    pDevIns->pHlpR3->pfnSSMPutMem(pSSM, &pThis->macConfigured, sizeof(pThis->macConfigured));
}


/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) vnetR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    RT_NOREF(uPass);
    vnetR3SaveConfig(pDevIns, pThis, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEPREP}
 */
static DECLCALLBACK(int) vnetR3SavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    RT_NOREF(pSSM);

    int rc = vnetCsRxEnter(pThis, VERR_SEM_BUSY);
    if (RT_SUCCESS(rc))
        vnetCsRxLeave(pThis);
    return rc;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) vnetR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVNETSTATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    /* Save config first */
    vnetR3SaveConfig(pDevIns, pThis, pSSM);

    /* Save the common part */
    int rc = vpciSaveExec(&pThis->VPCI, pSSM);
    AssertRCReturn(rc, rc);

    /* Save device-specific part */
    pHlp->pfnSSMPutMem(     pSSM, pThis->config.mac.au8, sizeof(pThis->config.mac));
    pHlp->pfnSSMPutBool(    pSSM, pThis->fPromiscuous);
    pHlp->pfnSSMPutBool(    pSSM, pThis->fAllMulti);
    pHlp->pfnSSMPutU32(     pSSM, pThis->nMacFilterEntries);
    pHlp->pfnSSMPutMem(     pSSM, pThis->aMacFilter, pThis->nMacFilterEntries * sizeof(RTMAC));
    rc = pHlp->pfnSSMPutMem(pSSM, pThis->aVlanFilter, sizeof(pThis->aVlanFilter));
    AssertRCReturn(rc, rc);

    Log(("%s State has been saved\n", INSTANCE(pThis)));
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADPREP,
 * Serializes the receive thread - it may be working inside the critsect. }
 */
static DECLCALLBACK(int) vnetR3LoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    RT_NOREF(pSSM);

    int rc = vnetCsRxEnter(pThis, VERR_SEM_BUSY);
    if (RT_SUCCESS(rc))
        vnetCsRxLeave(pThis);
    return rc;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) vnetR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PVNETSTATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    /* config checks */
    RTMAC macConfigured;
    int rc = pHlp->pfnSSMGetMem(pSSM, &macConfigured, sizeof(macConfigured));
    AssertRCReturn(rc, rc);
    if (memcmp(&macConfigured, &pThis->macConfigured, sizeof(macConfigured))
        && (uPass == 0 || !PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns)))
        LogRel(("%s: The mac address differs: config=%RTmac saved=%RTmac\n", INSTANCE(pThis), &pThis->macConfigured, &macConfigured));

    rc = vpciLoadExec(&pThis->VPCI, pSSM, uVersion, uPass, VNET_N_QUEUES);
    AssertRCReturn(rc, rc);

    if (uPass == SSM_PASS_FINAL)
    {
        rc = pHlp->pfnSSMGetMem( pSSM, pThis->config.mac.au8, sizeof(pThis->config.mac));
        AssertRCReturn(rc, rc);

        if (uVersion > VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1)
        {
            pHlp->pfnSSMGetBool(pSSM, &pThis->fPromiscuous);
            pHlp->pfnSSMGetBool(pSSM, &pThis->fAllMulti);
            pHlp->pfnSSMGetU32(pSSM, &pThis->nMacFilterEntries);
            pHlp->pfnSSMGetMem(pSSM, pThis->aMacFilter, pThis->nMacFilterEntries * sizeof(RTMAC));

            /* Clear the rest. */
            if (pThis->nMacFilterEntries < VNET_MAC_FILTER_LEN)
                memset(&pThis->aMacFilter[pThis->nMacFilterEntries],
                       0,
                       (VNET_MAC_FILTER_LEN - pThis->nMacFilterEntries) * sizeof(RTMAC));
            rc = pHlp->pfnSSMGetMem(pSSM, pThis->aVlanFilter, sizeof(pThis->aVlanFilter));
            AssertRCReturn(rc, rc);
        }
        else
        {
            pThis->fPromiscuous = true;
            pThis->fAllMulti = false;
            pThis->nMacFilterEntries = 0;
            memset(pThis->aMacFilter, 0, VNET_MAC_FILTER_LEN * sizeof(RTMAC));
            memset(pThis->aVlanFilter, 0, sizeof(pThis->aVlanFilter));
            if (pThis->pDrv)
                pThis->pDrv->pfnSetPromiscuousMode(pThis->pDrv, true);
        }
    }

    return rc;
}


/**
 * @callback_method_impl{FNSSMDEVLOADDONE, Link status adjustments after
 *                      loading.}
 */
static DECLCALLBACK(int) vnetR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);

    if (pThis->pDrv)
        pThis->pDrv->pfnSetPromiscuousMode(pThis->pDrv,
            (pThis->fPromiscuous | pThis->fAllMulti));
    /*
     * Indicate link down to the guest OS that all network connections have
     * been lost, unless we've been teleported here.
     */
    if (!PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns))
        vnetR3TempLinkDown(pThis);

    return VINF_SUCCESS;
}


/* -=-=-=-=- PCI Device -=-=-=-=- */

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) vnetR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                   RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    RT_NOREF(pPciDev, iRegion);

    if (enmType != PCI_ADDRESS_SPACE_IO)
    {
        /* We should never get here */
        AssertMsgFailed(("Invalid PCI address space param in map callback"));
        return VERR_INTERNAL_ERROR;
    }

    pThis->VPCI.IOPortBase = (RTIOPORT)GCPhysAddress;
    int rc = PDMDevHlpIOPortRegister(pDevIns, pThis->VPCI.IOPortBase,
                                     cb, 0, vnetIOPortOut, vnetIOPortIn,
                                     NULL, NULL, "VirtioNet");
#ifdef VNET_GC_SUPPORT
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterR0(pDevIns, pThis->VPCI.IOPortBase,
                                   cb, 0, "vnetIOPortOut", "vnetIOPortIn",
                                   NULL, NULL, "VirtioNet");
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterRC(pDevIns, pThis->VPCI.IOPortBase,
                                   cb, 0, "vnetIOPortOut", "vnetIOPortIn",
                                   NULL, NULL, "VirtioNet");
#endif
    AssertRC(rc);
    return rc;
}


/* -=-=-=-=- PDMDEVREG -=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREGR3,pfnDetach}
 */
static DECLCALLBACK(void) vnetR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    Log(("%s vnetR3Detach:\n", INSTANCE(pThis)));

    AssertLogRelReturnVoid(iLUN == 0);

    int rc = vnetR3CsEnter(pThis, VERR_SEM_BUSY);
    if (RT_FAILURE(rc))
    {
        LogRel(("vnetR3Detach failed to enter critical section!\n"));
        return;
    }

    vnetR3DestroyTxThreadAndEvent(pThis);
    /*
     * Zero some important members.
     */
    pThis->pDrvBase = NULL;
    pThis->pDrv = NULL;

    vnetR3CsLeave(pThis);
}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnAttach}
 */
static DECLCALLBACK(int) vnetR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    LogFlow(("%s vnetR3Attach:\n",  INSTANCE(pThis)));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    int rc = vnetR3CsEnter(pThis, VERR_SEM_BUSY);
    if (RT_FAILURE(rc))
    {
        LogRel(("vnetR3Attach failed to enter critical section!\n"));
        return rc;
    }

    /*
     * Attach the driver.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->VPCI.IBase, &pThis->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
#ifdef RT_OS_LINUX
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Please check your /etc/resolv.conf for <tt>nameserver</tt> entries. Either add one manually (<i>man resolv.conf</i>) or ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#else
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#endif
        }
        pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMINETWORKUP);
        AssertMsgStmt(pThis->pDrv, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                      rc = VERR_PDM_MISSING_INTERFACE_BELOW);

        vnetR3CreateTxThreadAndEvent(pDevIns, pThis);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        /* This should never happen because this function is not called
         * if there is no driver to attach! */
        Log(("%s No attached driver!\n", INSTANCE(pThis)));
    }

    /*
     * Temporary set the link down if it was up so that the guest
     * will know that we have change the configuration of the
     * network card
     */
    if (RT_SUCCESS(rc))
        vnetR3TempLinkDown(pThis);

    vnetR3CsLeave(pThis);
    return rc;

}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) vnetR3Suspend(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetR3WakeupReceive(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnPowerOff}
 */
static DECLCALLBACK(void) vnetR3PowerOff(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetR3WakeupReceive(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnRelocate}
 */
static DECLCALLBACK(void) vnetR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    vpciRelocate(pDevIns, offDelta);
    pThis->pCanRxQueueRC = PDMQueueRCPtr(pThis->pCanRxQueueR3);
#ifdef VNET_TX_DELAY
    pThis->pTxTimerRC    = TMTimerRCPtr(pThis->pTxTimerR3);
#endif /* VNET_TX_DELAY */
    // TBD
}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnDestruct}
 */
static DECLCALLBACK(int) vnetR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PVNETSTATE pThis = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);

#ifdef VNET_TX_DELAY
    LogRel(("TxTimer stats (avg/min/max): %7d usec %7d usec %7d usec\n",
            pThis->u32AvgDiff, pThis->u32MinDiff, pThis->u32MaxDiff));
#endif /* VNET_TX_DELAY */
    Log(("%s Destroying instance\n", INSTANCE(pThis)));
    if (pThis->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        RTSemEventSignal(pThis->hEventMoreRxDescAvail);
        RTSemEventDestroy(pThis->hEventMoreRxDescAvail);
        pThis->hEventMoreRxDescAvail = NIL_RTSEMEVENT;
    }

    // if (PDMCritSectIsInitialized(&pThis->csRx))
    //     PDMR3CritSectDelete(&pThis->csRx);

    return vpciDestruct(&pThis->VPCI);
}


/**
 * @interface_method_impl{PDMDEVREGR3,pfnConstruct}
 */
static DECLCALLBACK(int) vnetR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVNETSTATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PVNETSTATE);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    int             rc;

    /*
     * Initialize the instance data suffiencently for the destructor not to blow up.
     */
    pThis->hEventMoreRxDescAvail = NIL_RTSEMEVENT;

    /* Do our own locking. */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Initialize PCI part. */
    pThis->VPCI.IBase.pfnQueryInterface    = vnetQueryInterface;

    rc = vpciConstruct(pDevIns, &pThis->VPCI, iInstance, VNET_NAME_FMT, VIRTIO_NET_ID, VNET_PCI_CLASS, VNET_N_QUEUES);
    AssertRCReturn(rc, rc);

    pThis->pRxQueue  = vpciAddQueue(&pThis->VPCI, 256, vnetR3QueueReceive,  "RX ");
    pThis->pTxQueue  = vpciAddQueue(&pThis->VPCI, 256, vnetR3QueueTransmit, "TX ");
    pThis->pCtlQueue = vpciAddQueue(&pThis->VPCI, 16,  vnetR3QueueControl,  "CTL");

    Log(("%s Constructing new instance\n", INSTANCE(pThis)));

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "MAC|CableConnected|LineSpeed|LinkUpDelay", "");

    /* Get config params */
    rc = pHlp->pfnCFGMQueryBytes(pCfg, "MAC", pThis->macConfigured.au8, sizeof(pThis->macConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get MAC address"));
    rc = pHlp->pfnCFGMQueryBool(pCfg, "CableConnected", &pThis->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'CableConnected'"));
    rc = pHlp->pfnCFGMQueryU32Def(pCfg, "LinkUpDelay", &pThis->cMsLinkUpDelay, 5000); /* ms */
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'LinkUpDelay'"));
    Assert(pThis->cMsLinkUpDelay <= 300000); /* less than 5 minutes */
    if (pThis->cMsLinkUpDelay > 5000 || pThis->cMsLinkUpDelay < 100)
        LogRel(("%s WARNING! Link up delay is set to %u seconds!\n",
                INSTANCE(pThis), pThis->cMsLinkUpDelay / 1000));
    Log(("%s Link up delay is set to %u seconds\n",
         INSTANCE(pThis), pThis->cMsLinkUpDelay / 1000));

    vnetPrintFeatures(pThis, vnetIoCb_GetHostFeatures(pThis), "Device supports the following features");

    /* Initialize PCI config space */
    memcpy(pThis->config.mac.au8, pThis->macConfigured.au8, sizeof(pThis->config.mac.au8));
    pThis->config.uStatus = 0;

    /* Initialize state structure */
    pThis->u32PktNo     = 1;

    /* Interfaces */
    pThis->INetworkDown.pfnWaitReceiveAvail = vnetR3NetworkDown_WaitReceiveAvail;
    pThis->INetworkDown.pfnReceive          = vnetR3NetworkDown_Receive;
    pThis->INetworkDown.pfnReceiveGso       = vnetR3NetworkDown_ReceiveGso;
    pThis->INetworkDown.pfnXmitPending      = vnetR3NetworkDown_XmitPending;

    pThis->INetworkConfig.pfnGetMac         = vnetR3NetworkConfig_GetMac;
    pThis->INetworkConfig.pfnGetLinkState   = vnetR3NetworkConfig_GetLinkState;
    pThis->INetworkConfig.pfnSetLinkState   = vnetR3NetworkConfig_SetLinkState;

    /* Initialize critical section. */
    // char szTmp[sizeof(pThis->VPCI.szInstance) + 2];
    // RTStrPrintf(szTmp, sizeof(szTmp), "%sRX", pThis->VPCI.szInstance);
    // rc = PDMDevHlpCritSectInit(pDevIns, &pThis->csRx, szTmp);
    // if (RT_FAILURE(rc))
    //     return rc;

    /* Map our ports to IO space. */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, VPCI_CONFIG + sizeof(VNetPCIConfig), PCI_ADDRESS_SPACE_IO, vnetR3Map);
    AssertRCReturn(rc, rc);

    /* Register save/restore state handlers. */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIO_SAVEDSTATE_VERSION, sizeof(VNETSTATE), NULL,
                                NULL,           vnetR3LiveExec, NULL,
                                vnetR3SavePrep, vnetR3SaveExec, NULL,
                                vnetR3LoadPrep, vnetR3LoadExec, vnetR3LoadDone);
    AssertRCReturn(rc, rc);

    /* Create the RX notifier signaller. */
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 1, 0,
                              vnetR3CanRxQueueConsumer, true, "VNet-Rcv", &pThis->pCanRxQueueR3);
    AssertRCReturn(rc, rc);
    pThis->pCanRxQueueR0 = PDMQueueR0Ptr(pThis->pCanRxQueueR3);
    pThis->pCanRxQueueRC = PDMQueueRCPtr(pThis->pCanRxQueueR3);

    /* Create Link Up Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetR3LinkUpTimer, pThis, TMTIMER_FLAGS_NO_CRIT_SECT,
                                "VirtioNet Link Up Timer", &pThis->pLinkUpTimer);
    AssertRCReturn(rc, rc);

#ifndef VNET_TX_DELAY
    pThis->pSupDrvSession = PDMDevHlpGetSupDrvSession(pDevIns);
    pThis->hTxEvent       = NIL_SUPSEMEVENT;
    pThis->pTxThread      = NULL;
#else /* VNET_TX_DELAY */
    /* Create Transmit Delay Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetR3TxTimer, pThis, TMTIMER_FLAGS_NO_CRIT_SECT,
                                "VirtioNet TX Delay Timer", &pThis->pTxTimerR3);
    AssertRCReturn(rc, rc);
    pThis->pTxTimerR0 = TMTimerR0Ptr(pThis->pTxTimerR3);
    pThis->pTxTimerRC = TMTimerRCPtr(pThis->pTxTimerR3);

    pThis->u32i = pThis->u32AvgDiff = pThis->u32MaxDiff = 0;
    pThis->u32MinDiff = UINT32_MAX;
#endif /* VNET_TX_DELAY */

    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->VPCI.IBase, &pThis->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
        }
        pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMINETWORKUP);
        AssertMsgReturn(pThis->pDrv, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                        VERR_PDM_MISSING_INTERFACE_BELOW);

        vnetR3CreateTxThreadAndEvent(pDevIns, pThis);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME )
    {
         /* No error! */
        Log(("%s This adapter is not attached to any network!\n", INSTANCE(pThis)));
    }
    else
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the network LUN"));

    rc = RTSemEventCreate(&pThis->hEventMoreRxDescAvail);
    AssertRCReturn(rc, rc);

    rc = vnetIoCb_Reset(pThis);
    AssertRCReturn(rc, rc);

    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatReceiveBytes,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data received",            "/Public/Net/VNet%u/BytesReceived", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitBytes,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data transmitted",         "/Public/Net/VNet%u/BytesTransmitted", iInstance);

    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatReceiveBytes,        STAMTYPE_COUNTER, "ReceiveBytes",           STAMUNIT_BYTES,          "Amount of data received");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitBytes,       STAMTYPE_COUNTER, "TransmitBytes",          STAMUNIT_BYTES,          "Amount of data transmitted");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatReceiveGSO,          STAMTYPE_COUNTER, "Packets/ReceiveGSO",     STAMUNIT_COUNT,          "Number of received GSO packets");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitPackets,     STAMTYPE_COUNTER, "Packets/Transmit",       STAMUNIT_COUNT,          "Number of sent packets");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitGSO,         STAMTYPE_COUNTER, "Packets/Transmit-Gso",   STAMUNIT_COUNT,          "Number of sent GSO packets");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitCSum,        STAMTYPE_COUNTER, "Packets/Transmit-Csum",  STAMUNIT_COUNT,          "Number of completed TX checksums");
# ifdef VBOX_WITH_STATISTICS
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatReceive,             STAMTYPE_PROFILE, "Receive/Total",          STAMUNIT_TICKS_PER_CALL, "Profiling receive");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatReceiveStore,        STAMTYPE_PROFILE, "Receive/Store",          STAMUNIT_TICKS_PER_CALL, "Profiling receive storing");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRxOverflow,          STAMTYPE_PROFILE, "RxOverflow",             STAMUNIT_TICKS_PER_OCCURENCE, "Profiling RX overflows");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRxOverflowWakeup,    STAMTYPE_COUNTER, "RxOverflowWakeup",       STAMUNIT_OCCURENCES,     "Nr of RX overflow wakeups");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmit,            STAMTYPE_PROFILE, "Transmit/Total",         STAMUNIT_TICKS_PER_CALL, "Profiling transmits in HC");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitSend,        STAMTYPE_PROFILE, "Transmit/Send",          STAMUNIT_TICKS_PER_CALL, "Profiling send transmit in HC");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitByNetwork,   STAMTYPE_COUNTER, "Transmit/ByNetwork",     STAMUNIT_COUNT,          "Network-initiated transmissions");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTransmitByThread,    STAMTYPE_COUNTER, "Transmit/ByThread",      STAMUNIT_COUNT,          "Thread-initiated transmissions");
# endif

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioNet =
{
    /* .u32version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "virtio-net",
#ifdef VNET_GC_SUPPORT
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ,
#else
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS,
#endif
    /* .fClass = */                 PDM_DEVREG_CLASS_NETWORK,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(VNETSTATE),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Virtio Ethernet.\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           vnetR3Construct,
    /* .pfnDestruct = */            vnetR3Destruct,
    /* .pfnRelocate = */            vnetR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               NULL,
    /* .pfnSuspend = */             vnetR3Suspend,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              vnetR3Attach,
    /* .pfnDetach = */              vnetR3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            vnetR3PowerOff,
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
    /* .pfnConstruct = */           NULL,
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
