/* $Id$ $Revision$ $Date$ $Author$ */

/** @file
 * VBox storage devices - Virtio NET Driver
 *
 * Log-levels used:
 *    - Level 1:   The most important (but usually rare) things to note
 *    - Level 2:   NET command logging
 *    - Level 3:   Vector and I/O transfer summary (shows what client sent an expects and fulfillment)
 *    - Level 6:   Device <-> Guest Driver negotation, traffic, notifications and state handling
 *    - Level 12:  Brief formatted hex dumps of I/O data
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
//#define LOG_GROUP LOG_GROUP_DRV_NET
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO
#define VIRTIONET_WITH_GSO
#include <iprt/types.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/msi.h>
#include <VBox/version.h>
//#include <VBox/asm.h>
#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <VBox/sup.h>
#ifdef IN_RING3
#include <VBox/VBoxPktDmp.h>
#endif
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/semaphore.h>
# include <iprt/sg.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "../VirtIO/Virtio_1_0.h"

#include "VBoxDD.h"

/* After debugging single instance case, restore instance name logging */
#define INSTANCE(pState) (char *)(pState->szInstanceName ? "" : "") // Avoid requiring RT_NOREF in some funcs

#define VIRTIONET_SAVED_STATE_VERSION          UINT32_C(1)
#define VIRTIONET_MAX_QPAIRS                   1
#define VIRTIONET_MAX_QUEUES                   (VIRTIONET_MAX_QPAIRS * 2 + 1)
#define VIRTIONET_MAX_FRAME_SIZE               65535 + 18     /**< Max IP pkt size + Ethernet header with VLAN tag  */
#define VIRTIONET_MAC_FILTER_LEN               32
#define VIRTIONET_MAX_VLAN_ID                  (1 << 12)
#define VIRTIONET_PREALLOCATE_RX_SEG_COUNT     32

#define VIRTQNAME(idxQueue)       (pThis->aszVirtqNames[idxQueue])
#define CBVIRTQNAME(idxQueue)     RTStrNLen(VIRTQNAME(idxQueue), sizeof(VIRTQNAME(idxQueue)))
#define FEATURE_ENABLED(feature)  RT_BOOL(pThis->fNegotiatedFeatures & VIRTIONET_F_##feature)
#define FEATURE_DISABLED(feature) (!FEATURE_ENABLED(feature))
#define FEATURE_OFFERED(feature)  VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_##feature

#define SET_LINK_UP(pState) \
            LogFunc(("SET_LINK_UP\n")); \
            pState->virtioNetConfig.uStatus |= VIRTIONET_F_LINK_UP; \
            virtioCoreNotifyConfigChanged(&pThis->Virtio)

#define SET_LINK_DOWN(pState) \
            LogFunc(("SET_LINK_DOWN\n")); \
            pState->virtioNetConfig.uStatus &= ~VIRTIONET_F_LINK_UP; \
            virtioCoreNotifyConfigChanged(&pThis->Virtio)

#define IS_LINK_UP(pState)   (pState->virtioNetConfig.uStatus & VIRTIONET_F_LINK_UP)
#define IS_LINK_DOWN(pState) !IS_LINK_UP(pState)

/* Macros to calculate queue specific index number VirtIO 1.0, 5.1.2 */
#define IS_TX_QUEUE(n)    ((n) != CTRLQIDX && ((n) & 1))
#define IS_RX_QUEUE(n)    ((n) != CTRLQIDX && !IS_TX_QUEUE(n))
#define IS_CTRL_QUEUE(n)  ((n) == CTRLQIDX)
#define RXQIDX(qPairIdx)  (qPairIdx * 2)
#define TXQIDX(qPairIdx)  (qPairIdx * 2 + 1)
#define CTRLQIDX          (FEATURE_ENABLED(MQ) ? ((VIRTIONET_MAX_QPAIRS - 1) * 2 + 2) : 2)

#define LUN0    0

/*
 * Glossary of networking acronyms used in the following bit definitions:
 *
 * GSO = Generic Segmentation Offload
 * TSO = TCP Segmentation Offload
 * UFO = UDP Fragmentation Offload
 * ECN = Explicit Congestion Notification
 */

/** @name VirtIO 1.0 NET Host feature bits (See VirtIO 1.0 specification, Section 5.6.3)
 * @{  */
#define VIRTIONET_F_CSUM                 RT_BIT_64(0)          /**< Handle packets with partial checksum            */
#define VIRTIONET_F_GUEST_CSUM           RT_BIT_64(1)          /**< Handles packets with partial checksum           */
#define VIRTIONET_F_CTRL_GUEST_OFFLOADS  RT_BIT_64(2)          /**< Control channel offloads reconfig support       */
#define VIRTIONET_F_MAC                  RT_BIT_64(5)          /**< Device has given MAC address                    */
#define VIRTIONET_F_GUEST_TSO4           RT_BIT_64(7)          /**< Driver can receive TSOv4                        */
#define VIRTIONET_F_GUEST_TSO6           RT_BIT_64(8)          /**< Driver can receive TSOv6                        */
#define VIRTIONET_F_GUEST_ECN            RT_BIT_64(9)          /**< Driver can receive TSO with ECN                 */
#define VIRTIONET_F_GUEST_UFO            RT_BIT_64(10)         /**< Driver can receive UFO                          */
#define VIRTIONET_F_HOST_TSO4            RT_BIT_64(11)         /**< Device can receive TSOv4                        */
#define VIRTIONET_F_HOST_TSO6            RT_BIT_64(12)         /**< Device can receive TSOv6                        */
#define VIRTIONET_F_HOST_ECN             RT_BIT_64(13)         /**< Device can receive TSO with ECN                 */
#define VIRTIONET_F_HOST_UFO             RT_BIT_64(14)         /**< Device can receive UFO                          */
#define VIRTIONET_F_MRG_RXBUF            RT_BIT_64(15)         /**< Driver can merge receive buffers                */
#define VIRTIONET_F_STATUS               RT_BIT_64(16)         /**< Config status field is available                */
#define VIRTIONET_F_CTRL_VQ              RT_BIT_64(17)         /**< Control channel is available                    */
#define VIRTIONET_F_CTRL_RX              RT_BIT_64(18)         /**< Control channel RX mode + MAC addr filtering    */
#define VIRTIONET_F_CTRL_VLAN            RT_BIT_64(19)         /**< Control channel VLAN filtering                  */
#define VIRTIONET_F_CTRL_RX_EXTRA        RT_BIT_64(20)         /**< Control channel RX mode extra functions         */
#define VIRTIONET_F_GUEST_ANNOUNCE       RT_BIT_64(21)         /**< Driver can send gratuitous packets              */
#define VIRTIONET_F_MQ                   RT_BIT_64(22)         /**< Support ultiqueue with auto receive steering    */
#define VIRTIONET_F_CTRL_MAC_ADDR        RT_BIT_64(23)         /**< Set MAC address through control channel         */
/** @} */

#ifdef VIRTIONET_WITH_GSO
# define VIRTIONET_HOST_FEATURES_GSO    \
      VIRTIONET_F_CSUM                  \
    | VIRTIONET_F_HOST_TSO4             \
    | VIRTIONET_F_HOST_TSO6             \
    | VIRTIONET_F_HOST_UFO              \
    | VIRTIONET_F_GUEST_TSO4            \
    | VIRTIONET_F_GUEST_TSO6            \
    | VIRTIONET_F_GUEST_UFO             \
    | VIRTIONET_F_GUEST_CSUM                                   /* @bugref(4796) Guest must handle partial chksums   */
#else
# define VIRTIONET_HOST_FEATURES_GSO
#endif

#define VIRTIONET_HOST_FEATURES_OFFERED \
      VIRTIONET_F_STATUS                \
    | VIRTIONET_F_GUEST_ANNOUNCE        \
    | VIRTIONET_F_MAC                   \
    | VIRTIONET_F_CTRL_VQ               \
    | VIRTIONET_F_CTRL_RX               \
    | VIRTIONET_F_CTRL_VLAN             \
    | VIRTIONET_HOST_FEATURES_GSO       \
    | VIRTIONET_F_MRG_RXBUF             \
    | VIRTIO_F_EVENT_IDX  /** @todo  Trying this experimentally as potential workaround for bug
                           *         where virtio seems to expect interrupt for Rx/Used even though
                           *         its set the used ring flag in the Rx queue to skip the notification by device */

#define PCI_DEVICE_ID_VIRTIONET_HOST               0x1041      /**< Informs guest driver of type of VirtIO device   */
#define PCI_CLASS_BASE_NETWORK_CONTROLLER          0x02        /**< PCI Network device class                        */
#define PCI_CLASS_SUB_NET_ETHERNET_CONTROLLER      0x00        /**< PCI NET Controller subclass                     */
#define PCI_CLASS_PROG_UNSPECIFIED                 0x00        /**< Programming interface. N/A.                     */
#define VIRTIONET_PCI_CLASS                        0x01        /**< Base class Mass Storage?                        */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Virtio Net Host Device device-specific configuration (VirtIO 1.0, 5.1.4)
 * VBox VirtIO core issues callback to this VirtIO device-specific implementation to handle
 * MMIO accesses to device-specific configuration parameters.
 */

#pragma pack(1)
typedef struct virtio_net_config
{
    RTMAC  uMacAddress;                                         /**< mac                                            */
#if FEATURE_OFFERED(STATUS)
    uint16_t uStatus;                                           /**< status                                         */
#endif
#if FEATURE_OFFERED(MQ)
    uint16_t uMaxVirtqPairs;                                    /**< max_virtq_pairs                                */
#endif
} VIRTIONET_CONFIG_T, PVIRTIONET_CONFIG_T;
#pragma pack()

#define VIRTIONET_F_LINK_UP                          1          /**< config status: Link is up                      */
#define VIRTIONET_F_ANNOUNCE                         2          /**< config status: Announce                        */

/** @name VirtIO 1.0 NET Host Device device specific control types
 * @{  */
#define VIRTIONET_HDR_F_NEEDS_CSUM                   1          /**< flags: Packet needs checksum                   */
#define VIRTIONET_HDR_GSO_NONE                       0          /**< gso_type: No Global Segmentation Offset        */
#define VIRTIONET_HDR_GSO_TCPV4                      1          /**< gso_type: Global Segment Offset for TCPV4      */
#define VIRTIONET_HDR_GSO_UDP                        3          /**< gso_type: Global Segment Offset for UDP        */
#define VIRTIONET_HDR_GSO_TCPV6                      4          /**< gso_type: Global Segment Offset for TCPV6      */
#define VIRTIONET_HDR_GSO_ECN                     0x80          /**< gso_type: Explicit Congestion Notification     */
/** @} */

/* Device operation: Net header packet (VirtIO 1.0, 5.1.6) */
#pragma pack(1)
struct virtio_net_pkt_hdr {
    uint8_t  uFlags;                                           /**< flags                                           */
    uint8_t  uGsoType;                                         /**< gso_type                                        */
    uint16_t uHdrLen;                                          /**< hdr_len                                         */
    uint16_t uGsoSize;                                         /**< gso_size                                        */
    uint16_t uChksumStart;                                     /**< Chksum_start                                    */
    uint16_t uChksumOffset;                                    /**< Chksum_offset                                   */
    uint16_t uNumBuffers;                                      /**< num_buffers                                     */
};
#pragma pack()
typedef virtio_net_pkt_hdr VIRTIONET_PKT_HDR_T, *PVIRTIONET_PKT_HDR_T;
AssertCompileSize(VIRTIONET_PKT_HDR_T, 12);

/* Control virtq: Command entry (VirtIO 1.0, 5.1.6.5) */
#pragma pack(1)
struct virtio_net_ctrl_hdr {
    uint8_t uClass;                                             /**< class                                          */
    uint8_t uCmd;                                               /**< command                                        */
};
#pragma pack()
typedef virtio_net_ctrl_hdr VIRTIONET_CTRL_HDR_T, *PVIRTIONET_CTRL_HDR_T;

typedef uint8_t VIRTIONET_CTRL_HDR_T_ACK;

/* Command entry fAck values */
#define VIRTIONET_OK                               0
#define VIRTIONET_ERROR                            1

/** @name Control virtq: Receive filtering flags (VirtIO 1.0, 5.1.6.5.1)
 * @{  */
#define VIRTIONET_CTRL_RX                           0           /**< Control class: Receive filtering               */
#define VIRTIONET_CTRL_RX_PROMISC                   0           /**< Promiscuous mode                               */
#define VIRTIONET_CTRL_RX_ALLMULTI                  1           /**< All-multicast receive                          */
#define VIRTIONET_CTRL_RX_ALLUNI                    2           /**< All-unicast receive                            */
#define VIRTIONET_CTRL_RX_NOMULTI                   3           /**< No multicast receive                           */
#define VIRTIONET_CTRL_RX_NOUNI                     4           /**< No unicast receive                             */
#define VIRTIONET_CTRL_RX_NOBCAST                   5           /**< No broadcast receive                           */
/** @} */

typedef uint8_t  VIRTIONET_MAC_ADDRESS[6];
typedef uint32_t VIRTIONET_CTRL_MAC_TABLE_LEN;
typedef uint8_t  VIRTIONET_CTRL_MAC_ENTRIES[][6];

/** @name Control virtq: MAC address filtering flags (VirtIO 1.0, 5.1.6.5.2)
 * @{  */
#define VIRTIONET_CTRL_MAC                          1           /**< Control class: MAC address filtering            */
#define VIRTIONET_CTRL_MAC_TABLE_SET                0           /**< Set MAC table                                   */
#define VIRTIONET_CTRL_MAC_ADDR_SET                 1           /**< Set default MAC address                         */
/** @} */

/** @name Control virtq: MAC address filtering flags (VirtIO 1.0, 5.1.6.5.3)
 * @{  */
#define VIRTIONET_CTRL_VLAN                         2           /**< Control class: VLAN filtering                   */
#define VIRTIONET_CTRL_VLAN_ADD                     0           /**< Add VLAN to filter table                        */
#define VIRTIONET_CTRL_VLAN_DEL                     1           /**< Delete VLAN from filter table                   */
/** @} */

/** @name Control virtq: Gratuitous packet sending (VirtIO 1.0, 5.1.6.5.4)
 * @{  */
#define VIRTIONET_CTRL_ANNOUNCE                     3           /**< Control class: Gratuitous Packet Sending        */
#define VIRTIONET_CTRL_ANNOUNCE_ACK                 0           /**< Gratuitous Packet Sending ACK                   */
/** @} */

struct virtio_net_ctrl_mq {
    uint16_t    uVirtqueuePairs;                                /**<  virtqueue_pairs                                */
};

/** @name Control virtq: Receive steering in multiqueue mode (VirtIO 1.0, 5.1.6.5.5)
 * @{  */
#define VIRTIONET_CTRL_MQ                           4           /**< Control class: Receive steering                 */
#define VIRTIONET_CTRL_MQ_VQ_PAIRS_SET              0           /**< Set number of TX/RX queues                      */
#define VIRTIONET_CTRL_MQ_VQ_PAIRS_MIN              1           /**< Minimum number of TX/RX queues                  */
#define VIRTIONET_CTRL_MQ_VQ_PAIRS_MAX         0x8000           /**< Maximum number of TX/RX queues                  */
/** @} */

uint64_t    uOffloads;                                          /**< offloads                                        */

/** @name Control virtq: Setting Offloads State (VirtIO 1.0, 5.1.6.5.6.1)
 * @{  */
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS             5            /**< Control class: Offloads state configuration     */
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET         0            /** Apply new offloads configuration                 */
/** @} */

/**
 * Worker thread context, shared state.
 */
typedef struct VIRTIONETWORKER
{
    SUPSEMEVENT                     hEvtProcess;                /**< handle of associated sleep/wake-up semaphore      */
} VIRTIONETWORKER;
/** Pointer to a VirtIO SCSI worker. */
typedef VIRTIONETWORKER *PVIRTIONETWORKER;

/**
 * Worker thread context, ring-3 state.
 */
typedef struct VIRTIONETWORKERR3
{
    R3PTRTYPE(PPDMTHREAD)           pThread;                    /**< pointer to worker thread's handle                 */
    bool volatile                   fSleeping;                  /**< Flags whether worker thread is sleeping or not    */
    bool volatile                   fNotified;                  /**< Flags whether worker thread notified              */
    uint16_t                        idxQueue;                   /**< Index of associated queue                         */
} VIRTIONETWORKERR3;
/** Pointer to a VirtIO SCSI worker. */
typedef VIRTIONETWORKERR3 *PVIRTIONETWORKERR3;

/**
 * VirtIO Host NET device state, shared edition.
 *
 * @extends     VIRTIOCORE
 */
typedef struct VIRTIONET
{
    /** The core virtio state.   */
    VIRTIOCORE              Virtio;

    /** Virtio device-specific configuration */
    VIRTIONET_CONFIG_T      virtioNetConfig;

    /** Per device-bound virtq worker-thread contexts (eventq slot unused) */
    VIRTIONETWORKER         aWorkers[VIRTIONET_MAX_QUEUES];

    /** Track which VirtIO queues we've attached to */
    bool                    afQueueAttached[VIRTIONET_MAX_QUEUES];

    /** Device-specific spec-based VirtIO VIRTQNAMEs */
    char                    aszVirtqNames[VIRTIONET_MAX_QUEUES][VIRTIO_MAX_QUEUE_NAME_SIZE];

    /** Instance name */
    char                    szInstanceName[16];

    uint16_t                cVirtqPairs;

    uint16_t                cVirtQueues;

    uint16_t                cWorkers;

    uint64_t                fNegotiatedFeatures;

    SUPSEMEVENT             hTxEvent;

    /** Indicates transmission in progress -- only one thread is allowed. */
    uint32_t                uIsTransmitting;

    /** MAC address obtained from the configuration. */
    RTMAC                   macConfigured;

    /** Default MAC address which rx filtering accepts */
    RTMAC                   rxFilterMacDefault;

    /** True if physical cable is attached in configuration. */
    bool                    fCableConnected;

    /** virtio-net-1-dot-0 (in milliseconds). */
    uint32_t                cMsLinkUpDelay;

    uint32_t                alignment;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t                uPktNo;

    /** N/A: */
    bool volatile           fLeafWantsRxBuffers;

    SUPSEMEVENT             hEventRxDescAvail;

    /** Flags whether VirtIO core is in ready state */
    uint8_t                 fVirtioReady;

    /** Resetting flag */
    uint8_t                 fResetting;

    /** Quiescing I/O activity flag */
    uint8_t                 fQuiescing;


    /** Promiscuous mode -- RX filter accepts all packets. */
    uint8_t                 fPromiscuous;

    /** All multicast mode -- RX filter accepts all multicast packets. */
    uint8_t                 fAllMulticast;

    /** All unicast mode -- RX filter accepts all unicast packets. */
    uint8_t                 fAllUnicast;

    /** No multicast mode - Supresses multicast receive */
    uint8_t                 fNoMulticast;

    /** No unicast mode - Suppresses unicast receive */
    uint8_t                 fNoUnicast;

    /** No broadcast mode - Supresses broadcast receive */
    uint8_t                 fNoBroadcast;

    /** The number of actually used slots in aMacMulticastFilter. */
    uint32_t                cMulticastFilterMacs;

    /** Array of MAC multicast addresses accepted by RX filter. */
    RTMAC                   aMacMulticastFilter[VIRTIONET_MAC_FILTER_LEN];

    /** The number of actually used slots in aMacUniicastFilter. */
    uint32_t                cUnicastFilterMacs;

    /** Array of MAC unicast addresses accepted by RX filter. */
    RTMAC                   aMacUnicastFilter[VIRTIONET_MAC_FILTER_LEN];

    /** Bit array of VLAN filter, one bit per VLAN ID. */
    uint8_t                 aVlanFilter[VIRTIONET_MAX_VLAN_ID / sizeof(uint8_t)];

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
    /** @}  */
#endif
} VIRTIONET;
/** Pointer to the shared state of the VirtIO Host NET device. */
typedef VIRTIONET *PVIRTIONET;

/**
 * VirtIO Host NET device state, ring-3 edition.
 *
 * @extends     VIRTIOCORER3
 */
typedef struct VIRTIONETR3
{
    /** The core virtio ring-3 state. */
    VIRTIOCORER3                    Virtio;

    /** Per device-bound virtq worker-thread contexts (eventq slot unused) */
    VIRTIONETWORKERR3               aWorkers[VIRTIONET_MAX_QUEUES];

    /** The device instance.
     * @note This is _only_ for use when dealing with interface callbacks. */
    PPDMDEVINSR3                    pDevIns;

    /** Status LUN: Base interface. */
    PDMIBASE                        IBase;

    /** Status LUN: LED port interface. */
    PDMILEDPORTS                    ILeds;

    /** Status LUN: LED connector (peer). */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

    /** Status: LED */
    PDMLED                          led;

    /** Attached network driver. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;

    /** Network port interface (down) */
    PDMINETWORKDOWN                 INetworkDown;

    /** Network config port interface (main). */
    PDMINETWORKCONFIG               INetworkConfig;

    /** Connector of attached network driver. */
    R3PTRTYPE(PPDMINETWORKUP)       pDrv;

    R3PTRTYPE(PPDMTHREAD)           pTxThread;

    /** Link Up(/Restore) Timer. */
    TMTIMERHANDLE                   hLinkUpTimer;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;

    /** True if in the process of quiescing I/O */
    uint32_t                        fQuiescing;

    /** For which purpose we're quiescing. */
    VIRTIOVMSTATECHANGED            enmQuiescingFor;

} VIRTIONETR3;


/** Pointer to the ring-3 state of the VirtIO Host NET device. */
typedef VIRTIONETR3 *PVIRTIONETR3;

/**
 * VirtIO Host NET device state, ring-0 edition.
 */
typedef struct VIRTIONETR0
{
    /** The core virtio ring-0 state. */
    VIRTIOCORER0                    Virtio;
} VIRTIONETR0;
/** Pointer to the ring-0 state of the VirtIO Host NET device. */
typedef VIRTIONETR0 *PVIRTIONETR0;


/**
 * VirtIO Host NET device state, raw-mode edition.
 */
typedef struct VIRTIONETRC
{
    /** The core virtio raw-mode state. */
    VIRTIOCORERC                    Virtio;
} VIRTIONETRC;
/** Pointer to the ring-0 state of the VirtIO Host NET device. */
typedef VIRTIONETRC *PVIRTIONETRC;


/** @typedef VIRTIONETCC
 * The instance data for the current context. */
typedef CTX_SUFF(VIRTIONET) VIRTIONETCC;

/** @typedef PVIRTIONETCC
 * Pointer to the instance data for the current context. */
typedef CTX_SUFF(PVIRTIONET) PVIRTIONETCC;

#ifdef IN_RING3 /* spans most of the file, at the moment. */

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDEV}
 */
static DECLCALLBACK(int) virtioNetR3WakeupWorker(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log10Func(("%s\n", INSTANCE(pThis)));
    return PDMDevHlpSUPSemEventSignal(pDevIns, pThis->aWorkers[(uintptr_t)pThread->pvUser].hEvtProcess);
}

/**
 * Wakeup the RX thread.
 */
static void virtioNetR3WakeupRxBufWaiter(PPDMDEVINS pDevIns)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    AssertReturnVoid(pThis->hEventRxDescAvail != NIL_SUPSEMEVENT);

    STAM_COUNTER_INC(&pThis->StatRxOverflowWakeup);

    Log10Func(("%s Waking downstream driver's Rx buf waiter thread\n", INSTANCE(pThis)));
    int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEventRxDescAvail);
    AssertRC(rc);
}

DECLINLINE(void) virtioNetR3SetVirtqNames(PVIRTIONET pThis)
{
    for (uint16_t qPairIdx = 0; qPairIdx < pThis->cVirtqPairs; qPairIdx++)
    {
        RTStrPrintf(pThis->aszVirtqNames[RXQIDX(qPairIdx)], VIRTIO_MAX_QUEUE_NAME_SIZE, "receiveq<%d>",  qPairIdx);
        RTStrPrintf(pThis->aszVirtqNames[TXQIDX(qPairIdx)], VIRTIO_MAX_QUEUE_NAME_SIZE, "transmitq<%d>", qPairIdx);
    }
    RTStrCopy(pThis->aszVirtqNames[CTRLQIDX], VIRTIO_MAX_QUEUE_NAME_SIZE, "controlq");
}

/**
 * Dump a packet to debug log.
 *
 * @param   pThis       The virtio-net shared instance data.
 * @param   pbPacket    The packet.
 * @param   cb          The size of the packet.
 * @param   pszText     A string denoting direction of packet transfer.
 */
DECLINLINE(void) virtioNetR3PacketDump(PVIRTIONET pThis, const uint8_t *pbPacket, size_t cb, const char *pszText)
{
//    if (!LogIs12Enabled())
//        return;

    vboxEthPacketDump(INSTANCE(pThis), pszText, pbPacket, (uint32_t)cb);
}

void virtioNetDumpGcPhysRxBuf(PPDMDEVINS pDevIns, PVIRTIONET_PKT_HDR_T pRxPktHdr,
                     uint16_t cDescs, uint8_t *pvBuf, uint16_t cb, RTGCPHYS gcPhysRxBuf, uint8_t cbRxBuf)
{
#ifdef LOG_ENABLED
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    pRxPktHdr->uNumBuffers = cDescs;
    if (pRxPktHdr)
    {
        LogFunc(("-------------------------------------------------------------------\n"));
        LogFunc(("rxPktHdr\n"
                 "    uFlags ......... %2.2x\n"
                 "    uGsoType ....... %2.2x\n"
                 "    uHdrLen ........ %4.4x\n"
                 "    uGsoSize ....... %4.4x\n"
                 "    uChksumStart ... %4.4x\n"
                 "    uChksumOffset .. %4.4x\n"
                 "    uNumBuffers .... %4.4x\n",
                        pRxPktHdr->uFlags,
                        pRxPktHdr->uGsoType, pRxPktHdr->uHdrLen, pRxPktHdr->uGsoSize,
                        pRxPktHdr->uChksumStart, pRxPktHdr->uChksumOffset, pRxPktHdr->uNumBuffers));

        virtioCoreHexDump((uint8_t *)pRxPktHdr, sizeof(VIRTIONET_PKT_HDR_T), 0, "Dump of virtual rPktHdr");
    }
    virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");
    LogFunc((". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n"));
    virtioCoreGcPhysHexDump(pDevIns, gcPhysRxBuf, cbRxBuf, 0, "Phys Mem Dump of Rx pkt");
    LogFunc(("-------------------------------------------------------------------\n"));
#else
    RT_NOREF7(pDevIns, pRxPktHdr, cDescs, pvBuf, cb, gcPhysRxBuf, cbRxBuf);
#endif
}


DECLINLINE(void) virtioNetPrintFeatures(VIRTIONET *pThis)
{
#ifdef LOG_ENABLED
    static struct
    {
        uint64_t fFeatureBit;
        const char *pcszDesc;
    } const s_aFeatures[] =
    {
        { VIRTIONET_F_CSUM,                "   CSUM                 Host handles packets with partial checksum.\n" },
        { VIRTIONET_F_GUEST_CSUM,          "   GUEST_CSUM           Guest handles packets with partial checksum.\n" },
        { VIRTIONET_F_CTRL_GUEST_OFFLOADS, "   CTRL_GUEST_OFFLOADS  Control channel offloads reconfiguration support.\n" },
        { VIRTIONET_F_MAC,                 "   MAC                  Host has given MAC address.\n" },
        { VIRTIONET_F_GUEST_TSO4,          "   GUEST_TSO4           Guest can receive TSOv4.\n" },
        { VIRTIONET_F_GUEST_TSO6,          "   GUEST_TSO6           Guest can receive TSOv6.\n" },
        { VIRTIONET_F_GUEST_ECN,           "   GUEST_ECN            Guest can receive TSO with ECN.\n" },
        { VIRTIONET_F_GUEST_UFO,           "   GUEST_UFO            Guest can receive UFO.\n" },
        { VIRTIONET_F_HOST_TSO4,           "   HOST_TSO4            Host can receive TSOv4.\n" },
        { VIRTIONET_F_HOST_TSO6,           "   HOST_TSO6            Host can receive TSOv6.\n" },
        { VIRTIONET_F_HOST_ECN,            "   HOST_ECN             Host can receive TSO with ECN.\n" },
        { VIRTIONET_F_HOST_UFO,            "   HOST_UFO             Host can receive UFO.\n" },
        { VIRTIONET_F_MRG_RXBUF,           "   MRG_RXBUF            Guest can merge receive buffers.\n" },
        { VIRTIONET_F_STATUS,              "   STATUS               Configuration status field is available.\n" },
        { VIRTIONET_F_CTRL_VQ,             "   CTRL_VQ              Control channel is available.\n" },
        { VIRTIONET_F_CTRL_RX,             "   CTRL_RX              Control channel RX mode support.\n" },
        { VIRTIONET_F_CTRL_VLAN,           "   CTRL_VLAN            Control channel VLAN filtering.\n" },
        { VIRTIONET_F_GUEST_ANNOUNCE,      "   GUEST_ANNOUNCE       Guest can send gratuitous packets.\n" },
        { VIRTIONET_F_MQ,                  "   MQ                   Host supports multiqueue with automatic receive steering.\n" },
        { VIRTIONET_F_CTRL_MAC_ADDR,       "   CTRL_MAC_ADDR        Set MAC address through control channel.\n" }
    };

#define MAXLINE 80
    /* Display as a single buf to prevent interceding log messages */
    uint64_t fFeaturesOfferedMask = VIRTIONET_HOST_FEATURES_OFFERED;
    uint16_t cbBuf = RT_ELEMENTS(s_aFeatures) * 132;
    char *pszBuf = (char *)RTMemAllocZ(cbBuf);
    Assert(pszBuf);
    char *cp = pszBuf;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFeatures); ++i)
    {
        bool isOffered = fFeaturesOfferedMask & s_aFeatures[i].fFeatureBit;
        bool isNegotiated = pThis->fNegotiatedFeatures & s_aFeatures[i].fFeatureBit;
        cp += RTStrPrintf(cp, cbBuf - (cp - pszBuf), "        %s       %s   %s",
                          isOffered ? "+" : "-", isNegotiated ? "x" : " ", s_aFeatures[i].pcszDesc);
    }
    Log3(("VirtIO Net Features Configuration\n\n"
          "    Offered  Accepted  Feature              Description\n"
          "    -------  --------  -------              -----------\n"
          "%s\n", pszBuf));
    RTMemFree(pszBuf);

#else  /* !LOG_ENABLED */
    RT_NOREF(pThis);
#endif /* !LOG_ENABLED */
}

/*
 * Checks whether negotiated features have required flag combinations.
 * See VirtIO 1.0 specification, Section 5.1.3.1 */
DECLINLINE(bool) virtioNetValidateRequiredFeatures(uint32_t fFeatures)
{
    uint32_t fGuestChksumRequired =   fFeatures & VIRTIONET_F_GUEST_TSO4
                                   || fFeatures & VIRTIONET_F_GUEST_TSO6
                                   || fFeatures & VIRTIONET_F_GUEST_UFO;

    uint32_t fHostChksumRequired =    fFeatures & VIRTIONET_F_HOST_TSO4
                                   || fFeatures & VIRTIONET_F_HOST_TSO6
                                   || fFeatures & VIRTIONET_F_HOST_UFO;

    uint32_t fCtrlVqRequired =        fFeatures & VIRTIONET_F_CTRL_RX
                                   || fFeatures & VIRTIONET_F_CTRL_VLAN
                                   || fFeatures & VIRTIONET_F_GUEST_ANNOUNCE
                                   || fFeatures & VIRTIONET_F_MQ
                                   || fFeatures & VIRTIONET_F_CTRL_MAC_ADDR;

    if (fGuestChksumRequired && !(fFeatures & VIRTIONET_F_GUEST_CSUM))
        return false;

    if (fHostChksumRequired && !(fFeatures & VIRTIONET_F_CSUM))
        return false;

    if (fCtrlVqRequired && !(fFeatures & VIRTIONET_F_CTRL_VQ))
        return false;

    if (   fFeatures & VIRTIONET_F_GUEST_ECN
        && !(   fFeatures & VIRTIONET_F_GUEST_TSO4
             || fFeatures & VIRTIONET_F_GUEST_TSO6))
                    return false;

    if (   fFeatures & VIRTIONET_F_HOST_ECN
        && !(   fFeatures & VIRTIONET_F_HOST_TSO4
             || fFeatures & VIRTIONET_F_HOST_TSO6))
                    return false;
    return true;
}

/**
 * Resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * Assumption is this critereon is mandated by VirtIO 1.0, Section 4.1.3.1)
 * (Easily re-written to allow unaligned bounded access to a field).
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @result           - true or false
 */
#define MATCH_NET_CONFIG(member) \
        (   (   RT_SIZEOFMEMB(VIRTIONET_CONFIG_T, member) == 8 \
             && (   offConfig == RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) \
                 || offConfig == RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) + sizeof(uint32_t)) \
             && cb == sizeof(uint32_t)) \
         || (   offConfig >= RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) \
             && offConfig + cb <= RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) \
                                + RT_SIZEOFMEMB(VIRTIONET_CONFIG_T, member)) )

#ifdef LOG_ENABLED
# define LOG_NET_CONFIG_ACCESSOR(member) \
        virtioCoreLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIONET_CONFIG_T, member), \
                               pv, cb, offIntra, fWrite, false, 0);
#else
# define LOG_NET_CONFIG_ACCESSOR(member) do { } while (0)
#endif

#define NET_CONFIG_ACCESSOR(member) \
    do \
    { \
        uint32_t offIntra = offConfig - RT_UOFFSETOF(VIRTIONET_CONFIG_T, member); \
        if (fWrite) \
            memcpy((char *)&pThis->virtioNetConfig.member + offIntra, pv, cb); \
        else \
            memcpy(pv, (const char *)&pThis->virtioNetConfig.member + offIntra, cb); \
        LOG_NET_CONFIG_ACCESSOR(member); \
    } while(0)

#define NET_CONFIG_ACCESSOR_READONLY(member) \
    do \
    { \
        uint32_t offIntra = offConfig - RT_UOFFSETOF(VIRTIONET_CONFIG_T, member); \
        if (fWrite) \
            LogFunc(("%s Guest attempted to write readonly virtio_pci_common_cfg.%s\n", INSTANCE(pThis), #member)); \
        else \
        { \
            memcpy(pv, (const char *)&pThis->virtioNetConfig.member + offIntra, cb); \
            LOG_NET_CONFIG_ACCESSOR(member); \
        } \
    } while(0)


static int virtioNetR3CfgAccessed(PVIRTIONET pThis, uint32_t offConfig, void *pv, uint32_t cb, bool fWrite)
{
    AssertReturn(pv && cb <= sizeof(uint32_t), fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00);

    if (MATCH_NET_CONFIG(uMacAddress))
        NET_CONFIG_ACCESSOR_READONLY(uMacAddress);
#if FEATURE_OFFERED(STATUS)
    else
    if (MATCH_NET_CONFIG(uStatus))
        NET_CONFIG_ACCESSOR_READONLY(uStatus);
#endif
#if FEATURE_OFFERED(MQ)
    else
    if (MATCH_NET_CONFIG(uMaxVirtqPairs))
        NET_CONFIG_ACCESSOR_READONLY(uMaxVirtqPairs);
#endif
    else
    {
        LogFunc(("%s Bad access by guest to virtio_net_config: off=%u (%#x), cb=%u\n", INSTANCE(pThis), offConfig, offConfig, cb));
        return fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00;
    }
    return VINF_SUCCESS;
}

#undef NET_CONFIG_ACCESSOR_READONLY
#undef NET_CONFIG_ACCESSOR
#undef LOG_ACCESSOR
#undef MATCH_NET_CONFIG

/**
 * @callback_method_impl{VIRTIOCORER3,pfnDevCapRead}
 */
static DECLCALLBACK(int) virtioNetR3DevCapRead(PPDMDEVINS pDevIns, uint32_t uOffset, void *pv, uint32_t cb)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    LogFunc(("%s uOffset: %d, cb: %d\n",  INSTANCE(pThis), uOffset, cb));
    RT_NOREF(pThis);
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, pv, cb, false /*fRead*/);
}

/**
 * @callback_method_impl{VIRTIOCORER3,pfnDevCapWrite}
 */
static DECLCALLBACK(int) virtioNetR3DevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, uint32_t cb)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    Log10Func(("%s uOffset: %d, cb: %d: %.*Rhxs\n", INSTANCE(pThis), uOffset, cb, RT_MAX(cb, 8) , pv));
    RT_NOREF(pThis);
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, (void *)pv, cb, true /*fWrite*/);
}


/*********************************************************************************************************************************
*   Misc                                                                                                                         *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, virtio-net debugger info callback.}
 */
static DECLCALLBACK(void) virtioNetR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    /* Parse arguments. */
    RT_NOREF2(pThis, pszArgs); //bool fVerbose = pszArgs && strstr(pszArgs, "verbose") != NULL;

    /* Show basic information. */
    pHlp->pfnPrintf(pHlp, "%s#%d: virtio-scsci ",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance);
}


/*********************************************************************************************************************************
*   Saved state                                                                                                                  *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) virtioNetR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    PCPDMDEVHLPR3  pHlp    = pDevIns->pHlpR3;

    RT_NOREF(pThisCC);
    Log7Func(("%s LOAD EXEC!!\n", INSTANCE(pThis)));

    AssertReturn(uPass == SSM_PASS_FINAL, VERR_SSM_UNEXPECTED_PASS);
    AssertLogRelMsgReturn(uVersion == VIRTIONET_SAVED_STATE_VERSION,
                          ("uVersion=%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    virtioNetR3SetVirtqNames(pThis);

    pHlp->pfnSSMGetU64(     pSSM, &pThis->fNegotiatedFeatures);

    pHlp->pfnSSMGetU16(     pSSM, &pThis->cVirtQueues);
    pHlp->pfnSSMGetU16(     pSSM, &pThis->cWorkers);

    for (int idxQueue = 0; idxQueue < pThis->cVirtQueues; idxQueue++)
        pHlp->pfnSSMGetBool(pSSM, &pThis->afQueueAttached[idxQueue]);

    int rc;

    if (uPass == SSM_PASS_FINAL)
    {

    /* Load config area */
#if FEATURE_OFFERED(STATUS)
    /* config checks */
    RTMAC macConfigured;
    rc = pHlp->pfnSSMGetMem(pSSM, &macConfigured.au8, sizeof(macConfigured.au8));
    AssertRCReturn(rc, rc);
    if (memcmp(&macConfigured.au8, &pThis->macConfigured.au8, sizeof(macConfigured.au8))
        && (uPass == 0 || !PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns)))
        LogRel(("%s: The mac address differs: config=%RTmac saved=%RTmac\n",
            INSTANCE(pThis), &pThis->macConfigured, &macConfigured));
#endif
#if FEATURE_OFFERED(MQ)
        pHlp->pfnSSMGetU16( pSSM, &pThis->virtioNetConfig.uMaxVirtqPairs);
#endif
        /* Save device-specific part */
        pHlp->pfnSSMGetBool(    pSSM, &pThis->fCableConnected);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fPromiscuous);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fAllMulticast);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fAllUnicast);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fNoMulticast);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fNoUnicast);
        pHlp->pfnSSMGetU8(      pSSM, &pThis->fNoBroadcast);

        pHlp->pfnSSMGetU32(     pSSM, &pThis->cMulticastFilterMacs);
        pHlp->pfnSSMGetMem(     pSSM, pThis->aMacMulticastFilter, pThis->cMulticastFilterMacs * sizeof(RTMAC));

        if (pThis->cMulticastFilterMacs < VIRTIONET_MAC_FILTER_LEN)
            memset(&pThis->aMacMulticastFilter[pThis->cMulticastFilterMacs], 0,
                   (VIRTIONET_MAC_FILTER_LEN - pThis->cMulticastFilterMacs) * sizeof(RTMAC));

        pHlp->pfnSSMGetU32(     pSSM, &pThis->cUnicastFilterMacs);
        pHlp->pfnSSMGetMem(     pSSM, pThis->aMacUnicastFilter, pThis->cUnicastFilterMacs * sizeof(RTMAC));

        if (pThis->cUnicastFilterMacs < VIRTIONET_MAC_FILTER_LEN)
            memset(&pThis->aMacUnicastFilter[pThis->cUnicastFilterMacs], 0,
                   (VIRTIONET_MAC_FILTER_LEN - pThis->cUnicastFilterMacs) * sizeof(RTMAC));

        rc = pHlp->pfnSSMGetMem(pSSM, pThis->aVlanFilter, sizeof(pThis->aVlanFilter));
        AssertRCReturn(rc, rc);
    }

    /*
     * Call the virtio core to let it load its state.
     */
    rc = virtioCoreR3LoadExec(&pThis->Virtio, pDevIns->pHlpR3, pSSM);

    /*
     * Nudge queue workers
     */
    for (int idxWorker = 0; idxWorker < pThis->cWorkers; idxWorker++)
    {
        uint16_t idxQueue = pThisCC->aWorkers[idxWorker].idxQueue;
        if (pThis->afQueueAttached[idxQueue])
        {
            Log7Func(("%s Waking %s worker.\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
            rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->aWorkers[idxWorker].hEvtProcess);
            AssertRCReturn(rc, rc);
        }
    }
    return rc;
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) virtioNetR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    PCPDMDEVHLPR3  pHlp    = pDevIns->pHlpR3;

    RT_NOREF(pThisCC);
    Log7Func(("%s SAVE EXEC!!\n", INSTANCE(pThis)));

    pHlp->pfnSSMPutU64(     pSSM, pThis->fNegotiatedFeatures);

    pHlp->pfnSSMPutU16(     pSSM, pThis->cVirtQueues);
    pHlp->pfnSSMPutU16(     pSSM, pThis->cWorkers);

    for (int idxQueue = 0; idxQueue < pThis->cVirtQueues; idxQueue++)
        pHlp->pfnSSMPutBool(pSSM, pThis->afQueueAttached[idxQueue]);

    /* Save config area */
#if FEATURE_OFFERED(STATUS)
    pHlp->pfnSSMPutMem(     pSSM, pThis->virtioNetConfig.uMacAddress.au8,
                            sizeof(pThis->virtioNetConfig.uMacAddress.au8));
#endif
#if FEATURE_OFFERED(MQ)
    pHlp->pfnSSMPutU16(     pSSM, pThis->virtioNetConfig.uMaxVirtqPairs);
#endif

    /* Save device-specific part */
    pHlp->pfnSSMPutBool(    pSSM, pThis->fCableConnected);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fPromiscuous);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fAllMulticast);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fAllUnicast);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fNoMulticast);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fNoUnicast);
    pHlp->pfnSSMPutU8(      pSSM, pThis->fNoBroadcast);

    pHlp->pfnSSMPutU32(     pSSM, pThis->cMulticastFilterMacs);
    pHlp->pfnSSMPutMem(     pSSM, pThis->aMacMulticastFilter, pThis->cMulticastFilterMacs * sizeof(RTMAC));

    pHlp->pfnSSMPutU32(     pSSM, pThis->cUnicastFilterMacs);
    pHlp->pfnSSMPutMem(     pSSM, pThis->aMacUnicastFilter, pThis->cUnicastFilterMacs * sizeof(RTMAC));

    int rc = pHlp->pfnSSMPutMem(pSSM, pThis->aVlanFilter, sizeof(pThis->aVlanFilter));
    AssertRCReturn(rc, rc);

    /*
     * Call the virtio core to let it save its state.
     */
    return virtioCoreR3SaveExec(&pThis->Virtio, pDevIns->pHlpR3, pSSM);
}


/*********************************************************************************************************************************
*   Device interface.                                                                                                            *
*********************************************************************************************************************************/
/*xx*/
/**
 * @callback_method_impl{FNPDMDEVASYNCNOTIFY}
 */
static DECLCALLBACK(bool) virtioNetR3DeviceQuiesced(PPDMDEVINS pDevIns)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    /** @todo create test to conclusively determine I/O has been quiesced and add it here: */

    Log7Func(("%s Device I/O activity quiesced: %s\n",
        INSTANCE(pThis), virtioCoreGetStateChangeText(pThisCC->enmQuiescingFor)));

    virtioCoreR3VmStateChanged(&pThis->Virtio, pThisCC->enmQuiescingFor);

    pThis->fResetting = false;
    pThisCC->fQuiescing = false;

    return true;
}

/**
 * Worker for virtioNetR3Reset() and virtioNetR3SuspendOrPowerOff().
 */
static void virtioNetR3QuiesceDevice(PPDMDEVINS pDevIns, VIRTIOVMSTATECHANGED enmQuiescingFor)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    RT_NOREF(pThis);

    /* Prevent worker threads from removing/processing elements from virtq's */
    pThisCC->fQuiescing = true;
    pThisCC->enmQuiescingFor = enmQuiescingFor;

    /*
     * Wake downstream network driver thread that's waiting for Rx buffers to be available
     * to tell it that's going to happen...
     */
    virtioNetR3WakeupRxBufWaiter(pDevIns);

    PDMDevHlpSetAsyncNotification(pDevIns, virtioNetR3DeviceQuiesced);

    /* If already quiesced invoke async callback.  */
    if (!ASMAtomicReadBool(&pThis->fLeafWantsRxBuffers))
        PDMDevHlpAsyncNotificationCompleted(pDevIns);

    /** @todo make sure Rx and Tx are really quiesced (how do we synchronize w/downstream driver?) */
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnReset}
 */
static DECLCALLBACK(void) virtioNetR3Reset(PPDMDEVINS pDevIns)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s\n", INSTANCE(pThis)));
    pThis->fResetting = true;
    virtioNetR3QuiesceDevice(pDevIns, kvirtIoVmStateChangedReset);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnPowerOff}
 */
static DECLCALLBACK(void) virtioNetR3SuspendOrPowerOff(PPDMDEVINS pDevIns, VIRTIOVMSTATECHANGED enmType)
{

    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    Log7Func(("%s\n", INSTANCE(pThis)));

    RT_NOREF2(pThis, pThisCC);

    virtioNetR3QuiesceDevice(pDevIns, enmType);
    virtioNetR3WakeupRxBufWaiter(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3PowerOff(PPDMDEVINS pDevIns)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s\n", INSTANCE(pThis)));
    RT_NOREF(pThis);
    virtioNetR3SuspendOrPowerOff(pDevIns, kvirtIoVmStateChangedPowerOff);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3Suspend(PPDMDEVINS pDevIns)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s \n", INSTANCE(pThis)));
    RT_NOREF(pThis);
    virtioNetR3SuspendOrPowerOff(pDevIns, kvirtIoVmStateChangedSuspend);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnResume}
 *
 * Just process the VM device-related state change itself.
 * Unlike SCSI driver, there are no packets to redo. No I/O was halted or saved while
 * quiescing for pfnSuspend(). Any packets in process were simply dropped by the upper
 * layer driver, presumably to be retried or cause erring out at the upper layers
 * of the network stack.
 */
static DECLCALLBACK(void) virtioNetR3Resume(PPDMDEVINS pDevIns)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    Log7Func(("\n"));

    pThisCC->fQuiescing = false;

    /* Ensure guest is working the queues */
    virtioCoreR3VmStateChanged(&pThis->Virtio, kvirtIoVmStateChangedResume);
}

#ifdef IN_RING3

DECLINLINE(uint16_t) virtioNetR3Checkum16(const void *pvBuf, size_t cb)
{
    uint32_t  chksum = 0;
    uint16_t *pu = (uint16_t *)pvBuf;

    while (cb > 1)
    {
        chksum += *pu++;
        cb -= 2;
    }
    if (cb)
        chksum += *(uint8_t*)pu;
    while (chksum >> 16)
        chksum = (chksum >> 16) + (chksum & 0xFFFF);
    return ~chksum;
}

DECLINLINE(void) virtioNetR3CompleteChecksum(uint8_t *pBuf, size_t cbSize, uint16_t uStart, uint16_t uOffset)
{
    AssertReturnVoid(uStart < cbSize);
    AssertReturnVoid(uStart + uOffset + sizeof(uint16_t) <= cbSize);
    *(uint16_t *)(pBuf + uStart + uOffset) = virtioNetR3Checkum16(pBuf + uStart, cbSize - uStart);
}

/**
 * Turns on/off the read status LED.
 *
 * @returns VBox status code.
 * @param   pThis          Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void virtioNetR3SetReadLed(PVIRTIONETR3 pThisR3, bool fOn)
{
    Log10Func(("%s\n", fOn ? "on" : "off"));
    if (fOn)
        pThisR3->led.Asserted.s.fReading = pThisR3->led.Actual.s.fReading = 1;
    else
        pThisR3->led.Actual.s.fReading = fOn;
}

/**
 * Turns on/off the write status LED.
 *
 * @returns VBox status code.
 * @param   pThis          Pointer to the device state structure.
 * @param   fOn            New LED state.
 */
void virtioNetR3SetWriteLed(PVIRTIONETR3 pThisR3, bool fOn)
{
    Log10Func(("%s\n", fOn ? "on" : "off"));
    if (fOn)
        pThisR3->led.Asserted.s.fWriting = pThisR3->led.Actual.s.fWriting = 1;
    else
        pThisR3->led.Actual.s.fWriting = fOn;
}

/**
 * Check whether specific queue is ready and has Rx buffers (virtqueue descriptors)
 * available. This must be called before the pfnRecieve() method is called.
 *
 * @remarks As a side effect this function enables queue notification
 *          if it cannot receive because the queue is empty.
 *          It disables notification if it can receive.
 *
 * @returns VERR_NET_NO_BUFFER_SPACE if it cannot.
 * @thread  RX
 */
static int virtioNetR3IsRxQueuePrimed(PPDMDEVINS pDevIns, PVIRTIONET pThis, uint16_t idxRxQueue)
{
#define LOGPARAMS INSTANCE(pThis), VIRTQNAME(idxRxQueue)

    if (!pThis->fVirtioReady)
    {
        Log8Func(("%s %s VirtIO not ready (rc = VERR_NET_NO_BUFFER_SPACE)\n", LOGPARAMS));
    }
    else if (!virtioCoreIsQueueEnabled(&pThis->Virtio, idxRxQueue))
    {
        Log8Func(("%s %s queue not enabled (rc = VERR_NET_NO_BUFFER_SPACE)\n", LOGPARAMS));
    }
    else if (virtioCoreQueueIsEmpty(pDevIns, &pThis->Virtio, idxRxQueue))
    {
        Log8Func(("%s %s queue is empty (rc = VERR_NET_NO_BUFFER_SPACE)\n", LOGPARAMS));
        virtioCoreQueueSetNotify(&pThis->Virtio, idxRxQueue, true);
    }
    else
    {
        Log8Func(("%s %s ready with available buffers\n", LOGPARAMS));
        virtioCoreQueueSetNotify(&pThis->Virtio, idxRxQueue, false);
        return VINF_SUCCESS;
    }
    return VERR_NET_NO_BUFFER_SPACE;
}


static bool virtioNetR3AreRxBufsAvail(PPDMDEVINS pDevIns, PVIRTIONET pThis)
{
    /** @todo If we ever start using more than one Rx/Tx queue pair, is a random queue
              selection algorithm feasible or even necessary to prevent starvation? */
    for (int idxQueuePair = 0; idxQueuePair < pThis->cVirtqPairs; idxQueuePair++)
        if (RT_SUCCESS(virtioNetR3IsRxQueuePrimed(pDevIns, pThis, RXQIDX(idxQueuePair))))
            return true;
    return false;
}
/*
 * Returns true if VirtIO core and device are in a running and operational state
 */
DECLINLINE(bool) virtioNetIsOperational(PVIRTIONET pThis, PPDMDEVINS pDevIns)
{
    if (!pThis->fVirtioReady)
        return false;

    if (pThis->fQuiescing)
        return false;

    VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
    if (!RT_LIKELY(enmVMState == VMSTATE_RUNNING || enmVMState == VMSTATE_RUNNING_LS))
        return false;

    return true;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnWaitReceiveAvail}
 */
static DECLCALLBACK(int) virtioNetR3NetworkDown_WaitReceiveAvail(PPDMINETWORKDOWN pInterface, RTMSINTERVAL timeoutMs)
{
    PVIRTIONETCC pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkDown);
    PPDMDEVINS   pDevIns = pThisCC->pDevIns;
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    if (virtioNetR3AreRxBufsAvail(pDevIns, pThis))
    {
            Log10Func(("%s Rx bufs now available, releasing waiter...\n", INSTANCE(pThis)));
            return VINF_SUCCESS;
    }
    if (!timeoutMs)
        return VERR_NET_NO_BUFFER_SPACE;

    LogFlowFunc(("%s timeoutMs=%u\n", INSTANCE(pThis), timeoutMs));

    ASMAtomicXchgBool(&pThis->fLeafWantsRxBuffers, true);
    STAM_PROFILE_START(&pThis->StatRxOverflow, a);

    do {
        if (virtioNetR3AreRxBufsAvail(pDevIns, pThis))
        {
                Log10Func(("%s Rx bufs now available, releasing waiter...\n", INSTANCE(pThis)));
                return VINF_SUCCESS;
        }
        Log9Func(("%s Starved for guest Rx bufs, waiting %u ms ...\n",
                 INSTANCE(pThis), timeoutMs));

        int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->hEventRxDescAvail, timeoutMs);

        if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
            continue;

        if (RT_FAILURE(rc))
            RTThreadSleep(1);

    } while (virtioNetIsOperational(pThis, pDevIns));

    STAM_PROFILE_STOP(&pThis->StatRxOverflow, a);
    ASMAtomicXchgBool(&pThis->fLeafWantsRxBuffers, false);

    Log7Func(("%s Wait for Rx buffers available was interrupted\n", INSTANCE(pThis)));
    return VERR_INTERRUPTED;
}


/**
 * Sets up the GSO context according to the Virtio header.
 *
 * @param   pGso                The GSO context to setup.
 * @param   pCtx                The context descriptor.
 */
DECLINLINE(PPDMNETWORKGSO) virtioNetR3SetupGsoCtx(PPDMNETWORKGSO pGso, VIRTIONET_PKT_HDR_T const *pPktHdr)
{
    pGso->u8Type = PDMNETWORKGSOTYPE_INVALID;

    if (pPktHdr->uGsoType & VIRTIONET_HDR_GSO_ECN)
    {
        AssertMsgFailed(("Unsupported flag in virtio header: ECN\n"));
        return NULL;
    }
    switch (pPktHdr->uGsoType & ~VIRTIONET_HDR_GSO_ECN)
    {
        case VIRTIONET_HDR_GSO_TCPV4:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV4_TCP;
            pGso->cbHdrsSeg = pPktHdr->uHdrLen;
            break;
        case VIRTIONET_HDR_GSO_TCPV6:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV6_TCP;
            pGso->cbHdrsSeg = pPktHdr->uHdrLen;
            break;
        case VIRTIONET_HDR_GSO_UDP:
            pGso->u8Type = PDMNETWORKGSOTYPE_IPV4_UDP;
            pGso->cbHdrsSeg = pPktHdr->uChksumStart;
            break;
        default:
            return NULL;
    }
    if (pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM)
        pGso->offHdr2  = pPktHdr->uChksumStart;
    else
    {
        AssertMsgFailed(("GSO without checksum offloading!\n"));
        return NULL;
    }
    pGso->offHdr1     = sizeof(RTNETETHERHDR);
    pGso->cbHdrsTotal = pPktHdr->uHdrLen;
    pGso->cbMaxSeg    = pPktHdr->uGsoSize;
    return pGso;
}

/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnGetMac}
 */
static DECLCALLBACK(int) virtioNetR3NetworkConfig_GetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    PVIRTIONETCC    pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkConfig);
    PVIRTIONET      pThis   = PDMDEVINS_2_DATA(pThisCC->pDevIns, PVIRTIONET);
    memcpy(pMac, pThis->virtioNetConfig.uMacAddress.au8, sizeof(RTMAC));
    return VINF_SUCCESS;
}

/**
 * Returns true if it is a broadcast packet.
 *
 * @returns true if destination address indicates broadcast.
 * @param   pvBuf           The ethernet packet.
 */
DECLINLINE(bool) virtioNetR3IsBroadcast(const void *pvBuf)
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
DECLINLINE(bool) virtioNetR3IsMulticast(const void *pvBuf)
{
    return (*(char*)pvBuf) & 1;
}
/**
 * Determines if the packet is to be delivered to upper layer.
 *
 * @returns true if packet is intended for this node.
 * @param   pThis          Pointer to the state structure.
 * @param   pvBuf          The ethernet packet.
 * @param   cb             Number of bytes available in the packet.
 */
static bool virtioNetR3AddressFilter(PVIRTIONET pThis, const void *pvBuf, size_t cb)
{

    if (LogIs11Enabled())
    {
        char *pszType;
        if (virtioNetR3IsMulticast(pvBuf))
            pszType = (char *)"Multicast";
        else if (virtioNetR3IsBroadcast(pvBuf))
            pszType = (char *)"Broadcast";
        else
            pszType = (char *)"Unicast";

        LogFunc(("%s node(%RTmac %s%s), pkt(%RTmac %s)",
                 INSTANCE(pThis), pThis->virtioNetConfig.uMacAddress.au8,
                 pThis->fPromiscuous ? "promiscuous" : "",
                 pThis->fAllMulticast ? " all-multicast" : "",
                 pvBuf,  pszType));
    }

    if (pThis->fPromiscuous)
        return true;

    /* Ignore everything outside of our VLANs */
    uint16_t *uPtr = (uint16_t *)pvBuf;

    /* Compare TPID with VLAN Ether Type */
    if (   uPtr[6] == RT_H2BE_U16(0x8100)
        && !ASMBitTest(pThis->aVlanFilter, RT_BE2H_U16(uPtr[7]) & 0xFFF))
    {
        Log11Func(("\n%s not our VLAN, returning false\n", INSTANCE(pThis)));
        return false;
    }

    if (virtioNetR3IsBroadcast(pvBuf))
    {
        Log11(("... accept (broadcast)\n"));
        if (LogIs12Enabled())
            virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");
        return true;
    }
    if (pThis->fAllMulticast && virtioNetR3IsMulticast(pvBuf))
    {
        Log11(("... accept (all-multicast mode)\n"));
        if (LogIs12Enabled())
            virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");
        return true;
    }

    if (!memcmp(pThis->virtioNetConfig.uMacAddress.au8, pvBuf, sizeof(RTMAC)))
    {
        Log11((". . . accept (direct to this node)\n"));
        if (LogIs12Enabled())
            virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");
        return true;
    }

    for (uint16_t i = 0; i < pThis->cMulticastFilterMacs; i++)
    {
        if (!memcmp(&pThis->aMacMulticastFilter[i], pvBuf, sizeof(RTMAC)))
        {
            Log11(("... accept (in multicast array)\n"));
            if (LogIs12Enabled())
                virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");
            return true;
        }
    }

    for (uint16_t i = 0; i < pThis->cUnicastFilterMacs; i++)
        if (!memcmp(&pThis->aMacUnicastFilter[i], pvBuf, sizeof(RTMAC)))
        {
            Log11(("... accept (in unicast array)\n"));
            return true;
        }

    if (LogIs12Enabled())
        Log(("... reject\n"));

    return false;
}

static int virtioNetR3CopyRxPktToGuest(PPDMDEVINS pDevIns, PVIRTIONET pThis, const void *pvBuf, size_t cb,
                                       VIRTIONET_PKT_HDR_T *rxPktHdr, uint16_t cSegsAllocated,
                                       PRTSGBUF pVirtSegBufToGuest, PRTSGSEG paVirtSegsToGuest,
                                       uint16_t idxRxQueue)
{
    uint8_t fAddPktHdr = true;
    RTGCPHYS gcPhysPktHdrNumBuffers = 0;
    uint16_t cDescs;
    uint32_t uOffset;
    for (cDescs = uOffset = 0; uOffset < cb; )
    {
        PVIRTIO_DESC_CHAIN_T pDescChain = NULL;

        int rc = virtioCoreR3QueueGet(pDevIns, &pThis->Virtio, RXQIDX(idxRxQueue), &pDescChain, true);
        AssertMsgReturn(rc == VINF_SUCCESS || rc == VERR_NOT_AVAILABLE, ("%Rrc\n", rc), rc);

        /** @todo  Find a better way to deal with this */
        AssertMsgReturnStmt(rc == VINF_SUCCESS && pDescChain->cbPhysReturn,
                            ("Not enough Rx buffers in queue to accomodate ethernet packet\n"),
                            virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain),
                            VERR_INTERNAL_ERROR);

        /* Length of first seg of guest Rx buf should never be less than sizeof(virtio_net_pkt_hdr).
         * Otherwise code has to become more complicated, e.g. locate & cache seg idx & offset of
         * virtio_net_header.num_buffers, to defer updating (in gcPhys). Re-visit if needed */

        AssertMsgReturnStmt(pDescChain->pSgPhysReturn->paSegs[0].cbSeg >= sizeof(VIRTIONET_PKT_HDR_T),
                            ("Desc chain's first seg has insufficient space for pkt header!\n"),
                            virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain),
                            VERR_INTERNAL_ERROR);

        uint32_t cbDescChainLeft = pDescChain->cbPhysReturn;
        uint8_t  cbHdr = sizeof(VIRTIONET_PKT_HDR_T);

        /* Fill the Guest Rx buffer with data received from the interface */
        for (uint16_t cSegs = 0; uOffset < cb && cbDescChainLeft; )
        {
            if (fAddPktHdr)
            {
                /* Lead with packet header */
                paVirtSegsToGuest[0].cbSeg = cbHdr;
                paVirtSegsToGuest[0].pvSeg = RTMemAlloc(cbHdr);
                AssertReturn(paVirtSegsToGuest[0].pvSeg, VERR_NO_MEMORY);
                cbDescChainLeft -= cbHdr;

                memcpy(paVirtSegsToGuest[0].pvSeg, rxPktHdr, cbHdr);

                /* Calculate & cache addr of field to update after final value is known, in gcPhys mem */
                gcPhysPktHdrNumBuffers = pDescChain->pSgPhysReturn->paSegs[0].gcPhys
                                         + RT_UOFFSETOF(VIRTIONET_PKT_HDR_T, uNumBuffers);
                fAddPktHdr = false;
                cSegs++;
            }

            if (cSegs >= cSegsAllocated)
            {
                cSegsAllocated <<= 1; /* double allocation size */
                paVirtSegsToGuest = (PRTSGSEG)RTMemRealloc(paVirtSegsToGuest, sizeof(RTSGSEG) * cSegsAllocated);
                if (!paVirtSegsToGuest)
                    virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain);
                AssertReturn(paVirtSegsToGuest, VERR_NO_MEMORY);
            }

            /* Append remaining Rx pkt or as much current desc chain has room for */
            uint32_t cbCropped = RT_MIN(cb, cbDescChainLeft);
            paVirtSegsToGuest[cSegs].cbSeg = cbCropped;
            paVirtSegsToGuest[cSegs].pvSeg = ((uint8_t *)pvBuf) + uOffset;
            cbDescChainLeft -= cbCropped;
            uOffset += cbCropped;
            cDescs++;
            cSegs++;
            RTSgBufInit(pVirtSegBufToGuest, paVirtSegsToGuest, cSegs);
            Log7Func(("Send Rx pkt to guest...\n"));
            STAM_PROFILE_START(&pThis->StatReceiveStore, a);
            virtioCoreR3QueuePut(pDevIns, &pThis->Virtio, idxRxQueue,
                                 pVirtSegBufToGuest, pDescChain, true);
            STAM_PROFILE_STOP(&pThis->StatReceiveStore, a);

            if (FEATURE_DISABLED(MRG_RXBUF))
                break;
        }

        virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain);
    }

    if (uOffset < cb)
    {
        LogFunc(("%s Packet did not fit into RX queue (packet size=%u)!\n", INSTANCE(pThis), cb));
        return VERR_TOO_MUCH_DATA;
    }

    /* Fix-up pkthdr (in guest phys. memory) with number buffers (descriptors) processed */

    int rc = PDMDevHlpPCIPhysWrite(pDevIns, gcPhysPktHdrNumBuffers, &cDescs, sizeof(cDescs));
    AssertMsgRCReturn(rc,
                  ("Failure updating descriptor count in pkt hdr in guest physical memory\n"),
                  rc);

    /** @todo   WHY *must* we *force* notifying guest that we filled its Rx buffer(s)?
     *          If we don't notify the guest, it doesn't detect it and stalls, even though
     *          guest is responsible for setting the used-ring flag in the Rx queue that tells
     *          us to skip the notification interrupt! Obviously forcing the interrupt is
     *          non-optimal performance-wise and seems to contradict the Virtio spec.
     *          Is that a bug in the linux virtio_net.c driver? */

    virtioCoreQueueSync(pDevIns, &pThis->Virtio, RXQIDX(idxRxQueue), /* fForce */ true);

    return VINF_SUCCESS;
}


/**
 * Pad and store received packet.
 *
 * @remarks Make sure that the packet appears to upper layer as one coming
 *          from real Ethernet: pad it and insert FCS.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pThis           The virtio-net shared instance data.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @param   pGso            Pointer to Global Segmentation Offload structure
 * @param   idxRxQueue      Rx queue to work with
 * @thread  RX
 */
static int virtioNetR3HandleRxPacket(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                const void *pvBuf, size_t cb, PCPDMNETWORKGSO pGso, uint16_t idxRxQueue)
{
    RT_NOREF(pThisCC);

    LogFunc(("%s (%RTmac) pGso %s\n", INSTANCE(pThis), pvBuf, pGso ? "present" : "not present"));
    VIRTIONET_PKT_HDR_T rxPktHdr = { 0 };

    if (pGso)
    {
        Log2Func(("%s gso type=%x cbPktHdrsTotal=%u cbPktHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
              INSTANCE(pThis), pGso->u8Type, pGso->cbHdrsTotal,
              pGso->cbHdrsSeg, pGso->cbMaxSeg, pGso->offHdr1, pGso->offHdr2));

        rxPktHdr.uFlags = VIRTIONET_HDR_F_NEEDS_CSUM;
        switch (pGso->u8Type)
        {
            case PDMNETWORKGSOTYPE_IPV4_TCP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_TCPV4;
                rxPktHdr.uChksumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV6_TCP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_TCPV6;
                rxPktHdr.uChksumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV4_UDP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_UDP;
                rxPktHdr.uChksumOffset = RT_OFFSETOF(RTNETUDP, uh_sum);
                break;
            default:
                return VERR_INVALID_PARAMETER;
        }
        rxPktHdr.uHdrLen = pGso->cbHdrsTotal;
        rxPktHdr.uGsoSize = pGso->cbMaxSeg;
        rxPktHdr.uChksumStart = pGso->offHdr2;
        STAM_REL_COUNTER_INC(&pThis->StatReceiveGSO);
    }
    else
    {
        rxPktHdr.uFlags = 0;
        rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_NONE;
    }

    uint16_t cSegsAllocated = VIRTIONET_PREALLOCATE_RX_SEG_COUNT;

    PRTSGBUF pVirtSegBufToGuest = (PRTSGBUF)RTMemAllocZ(sizeof(RTSGBUF));
    AssertReturn(pVirtSegBufToGuest, VERR_NO_MEMORY);

    PRTSGSEG paVirtSegsToGuest  = (PRTSGSEG)RTMemAllocZ(sizeof(RTSGSEG) * cSegsAllocated);
    AssertReturnStmt(paVirtSegsToGuest, RTMemFree(pVirtSegBufToGuest), VERR_NO_MEMORY);

    int rc = virtioNetR3CopyRxPktToGuest(pDevIns, pThis, pvBuf, cb, &rxPktHdr, cSegsAllocated,
                                        pVirtSegBufToGuest, paVirtSegsToGuest, idxRxQueue);

    RTMemFree(paVirtSegsToGuest);
    RTMemFree(pVirtSegBufToGuest);

    Log7(("\n"));
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceiveGso}
 */
static DECLCALLBACK(int) virtioNetR3NetworkDown_ReceiveGso(PPDMINETWORKDOWN pInterface, const void *pvBuf,
                                               size_t cb, PCPDMNETWORKGSO pGso)
{
    PVIRTIONETCC    pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkDown);
    PPDMDEVINS      pDevIns = pThisCC->pDevIns;
    PVIRTIONET      pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    if (!pThis->fVirtioReady)
    {
        LogRelFunc(("VirtIO not ready, aborting downstream receive\n"));
        return VERR_INTERRUPTED;
    }
    if (pThis->fQuiescing)
    {
        LogRelFunc(("Quiescing I/O for suspend or power off, aborting downstream receive\n"));
        return VERR_INTERRUPTED;
    }

    if (pGso)
    {
        uint32_t uFeatures = pThis->fNegotiatedFeatures;

        switch (pGso->u8Type)
        {
            case PDMNETWORKGSOTYPE_IPV4_TCP:
                uFeatures &= VIRTIONET_F_GUEST_TSO4;
                break;
            case PDMNETWORKGSOTYPE_IPV6_TCP:
                uFeatures &= VIRTIONET_F_GUEST_TSO6;
                break;
            case PDMNETWORKGSOTYPE_IPV4_UDP:
            case PDMNETWORKGSOTYPE_IPV6_UDP:
                uFeatures &= VIRTIONET_F_GUEST_UFO;
                break;
            default:
                uFeatures = 0;
                break;
        }
        if (!uFeatures)
        {
            Log2Func(("GSO type (0x%x) not supported\n", INSTANCE(pThis), pGso->u8Type));
            return VERR_NOT_SUPPORTED;
        }
    }

    Log10Func(("%s pvBuf=%p cb=%3u pGso=%p ...", INSTANCE(pThis), pvBuf, cb, pGso));

    /** @todo If we ever start using more than one Rx/Tx queue pair, is a random queue
              selection algorithm feasible or even necessary to prevent starvation? */

    for (int idxQueuePair = 0; idxQueuePair < pThis->cVirtqPairs; idxQueuePair++)
    {
        if (RT_SUCCESS(!virtioNetR3IsRxQueuePrimed(pDevIns, pThis, RXQIDX(idxQueuePair))))
        {
            /* Drop packets if VM is not running or cable is disconnected. */
            if (!virtioNetIsOperational(pThis, pDevIns) || !IS_LINK_UP(pThis))
                return VINF_SUCCESS;

            STAM_PROFILE_START(&pThis->StatReceive, a);
            virtioNetR3SetReadLed(pThisCC, true);

            int rc = VINF_SUCCESS;
            if (virtioNetR3AddressFilter(pThis, pvBuf, cb))
            {
                rc = virtioNetR3HandleRxPacket(pDevIns, pThis, pThisCC, pvBuf, cb, pGso, RXQIDX(idxQueuePair));
                STAM_REL_COUNTER_ADD(&pThis->StatReceiveBytes, cb);
            }

            virtioNetR3SetReadLed(pThisCC, false);
            STAM_PROFILE_STOP(&pThis->StatReceive, a);
            return rc;
        }
    }
    return VERR_INTERRUPTED;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceive}
 */
static DECLCALLBACK(int) virtioNetR3NetworkDown_Receive(PPDMINETWORKDOWN pInterface, const void *pvBuf, size_t cb)
{
    return virtioNetR3NetworkDown_ReceiveGso(pInterface, pvBuf, cb, NULL);
}

/* Read physical bytes from the out segment(s) of descriptor chain */
static void virtioNetR3PullChain(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIO_DESC_CHAIN_T pDescChain, void *pv, uint16_t cb)
{
    uint8_t *pb = (uint8_t *)pv;
    uint16_t cbLim = RT_MIN(pDescChain->cbPhysSend, cb);
    while (cbLim)
    {
        size_t cbSeg = cbLim;
        RTGCPHYS GCPhys = virtioCoreSgBufGetNextSegment(pDescChain->pSgPhysSend, &cbSeg);
        PDMDevHlpPCIPhysRead(pDevIns, GCPhys, pb, cbSeg);
        pb += cbSeg;
        cbLim -= cbSeg;
        pDescChain->cbPhysSend -= cbSeg;
    }
    LogFunc(("%s Pulled %d/%d bytes from desc chain (%d bytes left)\n",
             INSTANCE(pThis), cb - cbLim, cb, pDescChain->cbPhysSend));
    RT_NOREF(pThis);
}

static uint8_t virtioNetR3CtrlRx(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                 PVIRTIONET_CTRL_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{

#define LOG_VIRTIONET_FLAG(fld) LogFunc(("%s Setting %s=%d\n", INSTANCE(pThis), #fld, pThis->fld))

    LogFunc(("%s Processing CTRL Rx command\n", INSTANCE(pThis)));
    RT_NOREF(pThis);
    switch(pCtrlPktHdr->uCmd)
    {
      case VIRTIONET_CTRL_RX_PROMISC:
        break;
      case VIRTIONET_CTRL_RX_ALLMULTI:
        break;
      case VIRTIONET_CTRL_RX_ALLUNI:
        /* fallthrough */
      case VIRTIONET_CTRL_RX_NOMULTI:
        /* fallthrough */
      case VIRTIONET_CTRL_RX_NOUNI:
        /* fallthrough */
      case VIRTIONET_CTRL_RX_NOBCAST:
        AssertMsgReturn(FEATURE_ENABLED(CTRL_RX_EXTRA),
                        ("CTRL 'extra' cmd w/o VIRTIONET_F_CTRL_RX_EXTRA feature negotiated - skipping\n"),
                        VIRTIONET_ERROR);
        /* fall out */
    }

    uint8_t fOn, fPromiscChanged = false;
    virtioNetR3PullChain(pDevIns, pThis, pDescChain, &fOn, RT_MIN(pDescChain->cbPhysSend, sizeof(fOn)));

    switch(pCtrlPktHdr->uCmd)
    {
      case VIRTIONET_CTRL_RX_PROMISC:
        pThis->fPromiscuous = RT_BOOL(fOn);
        fPromiscChanged = true;
        LOG_VIRTIONET_FLAG(fPromiscuous);
        break;
      case VIRTIONET_CTRL_RX_ALLMULTI:
        pThis->fAllMulticast = RT_BOOL(fOn);
        fPromiscChanged = true;
        LOG_VIRTIONET_FLAG(fAllMulticast);
        break;
      case VIRTIONET_CTRL_RX_ALLUNI:
        pThis->fAllUnicast = RT_BOOL(fOn);
        LOG_VIRTIONET_FLAG(fAllUnicast);
        break;
      case VIRTIONET_CTRL_RX_NOMULTI:
        pThis->fNoMulticast = RT_BOOL(fOn);
        LOG_VIRTIONET_FLAG(fNoMulticast);
        break;
      case VIRTIONET_CTRL_RX_NOUNI:
        pThis->fNoUnicast = RT_BOOL(fOn);
        LOG_VIRTIONET_FLAG(fNoUnicast);
        break;
      case VIRTIONET_CTRL_RX_NOBCAST:
        pThis->fNoBroadcast = RT_BOOL(fOn);
        LOG_VIRTIONET_FLAG(fNoBroadcast);
        break;
    }

    if (pThisCC->pDrv && fPromiscChanged)
    {
        uint8_t fPromiscuous = pThis->fPromiscuous | pThis->fAllMulticast;
        pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, fPromiscuous);
    }

    return VIRTIONET_OK;
}

static uint8_t virtioNetR3CtrlMac(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                  PVIRTIONET_CTRL_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    LogFunc(("%s Processing CTRL MAC command\n", INSTANCE(pThis)));

    RT_NOREF(pThisCC);

#define ASSERT_CTRL_ADDR_SET(v) \
    AssertMsgReturn((v), ("DESC chain too small to process CTRL_MAC_ADDR_SET cmd\n"), VIRTIONET_ERROR)

#define ASSERT_CTRL_TABLE_SET(v) \
    AssertMsgReturn((v), ("DESC chain too small to process CTRL_MAC_TABLE_SET cmd\n"), VIRTIONET_ERROR)

    AssertMsgReturn(pDescChain->cbPhysSend >= sizeof(*pCtrlPktHdr),
                   ("insufficient descriptor space for ctrl pkt hdr"),
                   VIRTIONET_ERROR);

    size_t cbRemaining = pDescChain->cbPhysSend;
    switch(pCtrlPktHdr->uCmd)
    {
        case VIRTIONET_CTRL_MAC_ADDR_SET:
        {
            /* Set default Rx filter MAC */
            ASSERT_CTRL_ADDR_SET(cbRemaining >= sizeof(VIRTIONET_CTRL_MAC_TABLE_LEN));
            virtioNetR3PullChain(pDevIns, pThis, pDescChain, &pThis->rxFilterMacDefault, sizeof(VIRTIONET_CTRL_MAC_TABLE_LEN));
            break;
        }
        case VIRTIONET_CTRL_MAC_TABLE_SET:
        {
            VIRTIONET_CTRL_MAC_TABLE_LEN cMacs;

            /* Load unicast MAC filter table */
            ASSERT_CTRL_TABLE_SET(cbRemaining >= sizeof(cMacs));
            virtioNetR3PullChain(pDevIns, pThis, pDescChain, &cMacs, sizeof(cMacs));
            cbRemaining -= sizeof(cMacs);
            Log7Func(("%s Guest provided %d unicast MAC Table entries\n", INSTANCE(pThis), cMacs));
            if (cMacs)
            {
                uint32_t cbMacs = cMacs * sizeof(RTMAC);
                ASSERT_CTRL_TABLE_SET(cbRemaining >= cbMacs);
                virtioNetR3PullChain(pDevIns, pThis, pDescChain, &pThis->aMacUnicastFilter, cbMacs);
                cbRemaining -= cbMacs;
            }
            pThis->cUnicastFilterMacs = cMacs;

            /* Load multicast MAC filter table */
            ASSERT_CTRL_TABLE_SET(cbRemaining >= sizeof(cMacs));
            virtioNetR3PullChain(pDevIns, pThis, pDescChain, &cMacs, sizeof(cMacs));
            cbRemaining -= sizeof(cMacs);
            Log10Func(("%s Guest provided %d multicast MAC Table entries\n", INSTANCE(pThis), cMacs));
            if (cMacs)
            {
                uint32_t cbMacs = cMacs * sizeof(RTMAC);
                ASSERT_CTRL_TABLE_SET(cbRemaining >= cbMacs);
                virtioNetR3PullChain(pDevIns, pThis, pDescChain, &pThis->aMacMulticastFilter, cbMacs);
                cbRemaining -= cbMacs;
            }
            pThis->cMulticastFilterMacs = cMacs;

#ifdef LOG_ENABLED
            LogFunc(("%s unicast MACs:\n", INSTANCE(pThis)));
            for(unsigned i = 0; i < cMacs; i++)
                LogFunc(("         %RTmac\n", &pThis->aMacUnicastFilter[i]));

            LogFunc(("%s multicast MACs:\n", INSTANCE(pThis)));
            for(unsigned i = 0; i < cMacs; i++)
                LogFunc(("         %RTmac\n", &pThis->aMacMulticastFilter[i]));
#endif
        }
    }
    return VIRTIONET_OK;
}

static uint8_t virtioNetR3CtrlVlan(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                   PVIRTIONET_CTRL_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    LogFunc(("%s Processing CTRL VLAN command\n", INSTANCE(pThis)));

    RT_NOREF(pThisCC);

    uint16_t uVlanId;
    uint16_t cbRemaining = pDescChain->cbPhysSend - sizeof(*pCtrlPktHdr);
    AssertMsgReturn(cbRemaining > sizeof(uVlanId),
        ("DESC chain too small for VIRTIO_NET_CTRL_VLAN cmd processing"), VIRTIONET_ERROR);
    virtioNetR3PullChain(pDevIns, pThis, pDescChain, &uVlanId, sizeof(uVlanId));
    AssertMsgReturn(uVlanId > VIRTIONET_MAX_VLAN_ID,
        ("%s VLAN ID out of range (VLAN ID=%u)\n", INSTANCE(pThis), uVlanId), VIRTIONET_ERROR);
    LogFunc(("%s uCommand=%u VLAN ID=%u\n", INSTANCE(pThis), pCtrlPktHdr->uCmd, uVlanId));
    switch (pCtrlPktHdr->uCmd)
    {
        case VIRTIONET_CTRL_VLAN_ADD:
            ASMBitSet(pThis->aVlanFilter, uVlanId);
            break;
        case VIRTIONET_CTRL_VLAN_DEL:
            ASMBitClear(pThis->aVlanFilter, uVlanId);
            break;
        default:
            return VIRTIONET_ERROR;
    }
    return VIRTIONET_OK;
}

static void virtioNetR3Ctrl(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                            PVIRTIO_DESC_CHAIN_T pDescChain)
{
    LogFunc(("%s Received CTRL packet from guest\n", INSTANCE(pThis)));

    if (pDescChain->cbPhysSend < 2)
    {
        LogFunc(("%s CTRL packet from guest driver incomplete. Skipping ctrl cmd\n", INSTANCE(pThis)));
        return;
    }
    else if (pDescChain->cbPhysReturn < sizeof(VIRTIONET_CTRL_HDR_T_ACK))
    {
        LogFunc(("%s Guest driver didn't allocate memory to receive ctrl pkt ACK. Skipping ctrl cmd\n", INSTANCE(pThis)));
        return;
    }

    /*
     * Allocate buffer and read in the control command
     */
    PVIRTIONET_CTRL_HDR_T pCtrlPktHdr = (PVIRTIONET_CTRL_HDR_T)RTMemAllocZ(sizeof(VIRTIONET_CTRL_HDR_T));

    AssertPtrReturnVoid(pCtrlPktHdr);

    AssertMsgReturnVoid(pDescChain->cbPhysSend >= sizeof(VIRTIONET_CTRL_HDR_T),
                        ("DESC chain too small for CTRL pkt header"));

    virtioNetR3PullChain(pDevIns, pThis, pDescChain, pCtrlPktHdr,
                         RT_MIN(pDescChain->cbPhysSend, sizeof(VIRTIONET_CTRL_HDR_T)));

    Log7Func(("%s CTRL COMMAND: class=%d command=%d\n", INSTANCE(pThis), pCtrlPktHdr->uClass, pCtrlPktHdr->uCmd));

    uint8_t uAck;
    switch (pCtrlPktHdr->uClass)
    {
        case VIRTIONET_CTRL_RX:
            uAck = virtioNetR3CtrlRx(pDevIns, pThis, pThisCC, pCtrlPktHdr, pDescChain);
            break;
        case VIRTIONET_CTRL_MAC:
            uAck = virtioNetR3CtrlMac(pDevIns, pThis, pThisCC, pCtrlPktHdr, pDescChain);
            break;
        case VIRTIONET_CTRL_VLAN:
            uAck = virtioNetR3CtrlVlan(pDevIns, pThis, pThisCC, pCtrlPktHdr, pDescChain);
            break;
        case VIRTIONET_CTRL_ANNOUNCE:
            uAck = VIRTIONET_OK;
            if (FEATURE_DISABLED(STATUS) || FEATURE_DISABLED(GUEST_ANNOUNCE))
            {
                LogFunc(("%s Ignoring CTRL class VIRTIONET_CTRL_ANNOUNCE.\n"
                         "VIRTIO_F_STATUS or VIRTIO_F_GUEST_ANNOUNCE feature not enabled\n", INSTANCE(pThis)));
                break;
            }
            if (pCtrlPktHdr->uCmd != VIRTIONET_CTRL_ANNOUNCE_ACK)
            {
                LogFunc(("%s Ignoring CTRL class VIRTIONET_CTRL_ANNOUNCE. Unrecognized uCmd\n", INSTANCE(pThis)));
                break;
            }
            pThis->virtioNetConfig.uStatus &= ~VIRTIONET_F_ANNOUNCE;
            Log7Func(("%s Clearing VIRTIONET_F_ANNOUNCE in config status\n", INSTANCE(pThis)));
            break;

        default:
            LogRelFunc(("Unrecognized CTRL pkt hdr class (%d)\n", pCtrlPktHdr->uClass));
            uAck = VIRTIONET_ERROR;
    }

    /* Currently CTRL pkt header just returns ack, but keeping segment logic generic/flexible
     * in case that changes to make adapting more straightforward */
    int cSegs = 1;

    /* Return CTRL packet Ack byte (result code) to guest driver */
    PRTSGSEG paReturnSegs = (PRTSGSEG)RTMemAllocZ(sizeof(RTSGSEG));
    AssertMsgReturnVoid(paReturnSegs, ("Out of memory"));

    RTSGSEG aStaticSegs[] = { { &uAck, sizeof(uAck) } };
    memcpy(paReturnSegs, aStaticSegs, sizeof(RTSGSEG));

    PRTSGBUF pReturnSegBuf = (PRTSGBUF)RTMemAllocZ(sizeof(RTSGBUF));
    AssertMsgReturnVoid(pReturnSegBuf, ("Out of memory"));

    /* Copy segment data to malloc'd memory to avoid stack out-of-scope errors sanitizer doesn't detect */
    for (int i = 0; i < cSegs; i++)
    {
        void *pv = paReturnSegs[i].pvSeg;
        paReturnSegs[i].pvSeg = RTMemAlloc(aStaticSegs[i].cbSeg);
        AssertMsgReturnVoid(paReturnSegs[i].pvSeg, ("Out of memory"));
        memcpy(paReturnSegs[i].pvSeg, pv, aStaticSegs[i].cbSeg);
    }

    RTSgBufInit(pReturnSegBuf, paReturnSegs, cSegs);

    virtioCoreR3QueuePut(pDevIns, &pThis->Virtio, CTRLQIDX, pReturnSegBuf, pDescChain, true);
    virtioCoreQueueSync(pDevIns, &pThis->Virtio, CTRLQIDX, false);

    for (int i = 0; i < cSegs; i++)
        RTMemFree(paReturnSegs[i].pvSeg);

    RTMemFree(paReturnSegs);
    RTMemFree(pReturnSegBuf);

    LogFunc(("%s Finished processing CTRL command with status %s\n",
             INSTANCE(pThis), uAck == VIRTIONET_OK ? "VIRTIONET_OK" : "VIRTIONET_ERROR"));
}

static int virtioNetR3ReadHeader(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, PVIRTIONET_PKT_HDR_T pPktHdr, uint32_t cbFrame)
{
    int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhys, pPktHdr, sizeof(*pPktHdr));
    if (RT_FAILURE(rc))
        return rc;

    Log(("virtio-net: header (flags=%x gso-type=%x len=%x gso-size=%x Chksum-start=%x Chksum-offset=%x) cbFrame=%d\n",
          pPktHdr->uFlags, pPktHdr->uGsoType, pPktHdr->uHdrLen,
          pPktHdr->uGsoSize, pPktHdr->uChksumStart, pPktHdr->uChksumOffset, cbFrame));

    if (pPktHdr->uGsoType)
    {
        uint32_t uMinHdrSize;

        /* Segmentation offloading cannot be done without checksumming, and we do not support ECN */
        AssertMsgReturn(    RT_LIKELY(pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM)
                         && !(RT_UNLIKELY(pPktHdr->uGsoType & VIRTIONET_HDR_GSO_ECN)),
                         ("Unsupported ECN request in pkt header\n"), VERR_NOT_SUPPORTED);

        switch (pPktHdr->uGsoType)
        {
            case VIRTIONET_HDR_GSO_TCPV4:
            case VIRTIONET_HDR_GSO_TCPV6:
                uMinHdrSize = sizeof(RTNETTCP);
                break;
            case VIRTIONET_HDR_GSO_UDP:
                uMinHdrSize = 0;
                break;
            default:
                LogFunc(("Bad GSO type in packet header\n"));
                return VERR_INVALID_PARAMETER;
        }
        /* Header + MSS must not exceed the packet size. */
        AssertMsgReturn(RT_LIKELY(uMinHdrSize + pPktHdr->uChksumStart + pPktHdr->uGsoSize <= cbFrame),
                    ("Header plus message exceeds packet size"), VERR_BUFFER_OVERFLOW);
    }

    AssertMsgReturn(  !pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM
                    || sizeof(uint16_t) + pPktHdr->uChksumStart + pPktHdr->uChksumOffset <= cbFrame,
                 ("Checksum (%d bytes) doesn't fit into pkt header (%d bytes)\n",
                 sizeof(uint16_t) + pPktHdr->uChksumStart + pPktHdr->uChksumOffset, cbFrame),
                 VERR_BUFFER_OVERFLOW);

    return VINF_SUCCESS;
}

static int virtioNetR3TransmitFrame(PVIRTIONET pThis, PVIRTIONETCC pThisCC, PPDMSCATTERGATHER pSgBuf,
                               PPDMNETWORKGSO pGso, PVIRTIONET_PKT_HDR_T pPktHdr)
{
    virtioNetR3PacketDump(pThis, (uint8_t *)pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed, "--> Outgoing");
    if (pGso)
    {
        /* Some guests (RHEL) may report HdrLen excluding transport layer header! */
        /*
         * We cannot use cdHdrs provided by the guest because of different ways
         * it gets filled out by different versions of kernels.
         */
        //if (pGso->cbHdrs < pPktHdr->uCSumStart + pPktHdr->uCSumOffset + 2)
        {
            Log4Func(("%s HdrLen before adjustment %d.\n",
                  INSTANCE(pThis), pGso->cbHdrsTotal));
            switch (pGso->u8Type)
            {
                case PDMNETWORKGSOTYPE_IPV4_TCP:
                case PDMNETWORKGSOTYPE_IPV6_TCP:
                    pGso->cbHdrsTotal = pPktHdr->uChksumStart +
                        ((PRTNETTCP)(((uint8_t*)pSgBuf->aSegs[0].pvSeg) + pPktHdr->uChksumStart))->th_off * 4;
                    pGso->cbHdrsSeg   = pGso->cbHdrsTotal;
                    break;
                case PDMNETWORKGSOTYPE_IPV4_UDP:
                    pGso->cbHdrsTotal = (uint8_t)(pPktHdr->uChksumStart + sizeof(RTNETUDP));
                    pGso->cbHdrsSeg = pPktHdr->uChksumStart;
                    break;
            }
            /* Update GSO structure embedded into the frame */
            ((PPDMNETWORKGSO)pSgBuf->pvUser)->cbHdrsTotal = pGso->cbHdrsTotal;
            ((PPDMNETWORKGSO)pSgBuf->pvUser)->cbHdrsSeg   = pGso->cbHdrsSeg;
            Log4Func(("%s adjusted HdrLen to %d.\n",
                  INSTANCE(pThis), pGso->cbHdrsTotal));
        }
        Log2Func(("%s gso type=%x cbHdrsTotal=%u cbHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
                  INSTANCE(pThis), pGso->u8Type, pGso->cbHdrsTotal, pGso->cbHdrsSeg,
                  pGso->cbMaxSeg, pGso->offHdr1, pGso->offHdr2));
        STAM_REL_COUNTER_INC(&pThis->StatTransmitGSO);
    }
    else if (pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM)
    {
        STAM_REL_COUNTER_INC(&pThis->StatTransmitCSum);
        /*
         * This is not GSO frame but checksum offloading is requested.
         */
        virtioNetR3CompleteChecksum((uint8_t*)pSgBuf->aSegs[0].pvSeg, pSgBuf->cbUsed,
                             pPktHdr->uChksumStart, pPktHdr->uChksumOffset);
    }

    return pThisCC->pDrv->pfnSendBuf(pThisCC->pDrv, pSgBuf, false);
}

static void virtioNetR3TransmitPendingPackets(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                         uint16_t idxTxQueue, bool fOnWorkerThread)
{

    PVIRTIOCORE pVirtio = &pThis->Virtio;

    /*
     * Only one thread is allowed to transmit at a time, others should skip
     * transmission as the packets will be picked up by the transmitting
     * thread.
     */
    if (!ASMAtomicCmpXchgU32(&pThis->uIsTransmitting, 1, 0))
        return;

    if (!pThis->fVirtioReady)
    {
        LogFunc(("%s Ignoring Tx requests. VirtIO not ready (status=0x%x).\n",
                INSTANCE(pThis), pThis->virtioNetConfig.uStatus));
        return;
    }

    if (!pThis->fCableConnected)
    {
        Log(("%s Ignoring transmit requests while cable is disconnected.\n", INSTANCE(pThis)));
        return;
    }

    PPDMINETWORKUP pDrv = pThisCC->pDrv;
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

    int cPkts = virtioCoreR3QueuePendingCount(pVirtio->pDevIns, pVirtio, idxTxQueue);
    if (!cPkts)
    {
        LogFunc(("%s No packets to send found on %s\n", INSTANCE(pThis), VIRTQNAME(idxTxQueue)));

        if (pDrv)
            pDrv->pfnEndXmit(pDrv);

        ASMAtomicWriteU32(&pThis->uIsTransmitting, 0);
        return;
    }
    LogFunc(("%s About to transmit %d pending packets\n", INSTANCE(pThis), cPkts));

    virtioNetR3SetWriteLed(pThisCC, true);

    int rc;
    PVIRTIO_DESC_CHAIN_T pDescChain = NULL;
    while ((rc = virtioCoreR3QueuePeek(pVirtio->pDevIns, pVirtio, idxTxQueue, &pDescChain)) == VINF_SUCCESS)
    {
        Log10Func(("%s fetched descriptor chain from %s\n", INSTANCE(pThis), VIRTQNAME(idxTxQueue)));

        PVIRTIOSGBUF pSgPhysSend = pDescChain->pSgPhysSend;
        PVIRTIOSGSEG paSegsFromGuest = pSgPhysSend->paSegs;
        uint32_t cSegsFromGuest = pSgPhysSend->cSegs;

        VIRTIONET_PKT_HDR_T PktHdr;
        uint32_t uSize = 0;

        Assert(paSegsFromGuest[0].cbSeg >= sizeof(PktHdr));

        /* Compute total frame size. */
        for (unsigned i = 0; i < cSegsFromGuest && uSize < VIRTIONET_MAX_FRAME_SIZE; i++)
            uSize +=  paSegsFromGuest[i].cbSeg;

        Log5Func(("%s complete frame is %u bytes.\n", INSTANCE(pThis), uSize));
        Assert(uSize <= VIRTIONET_MAX_FRAME_SIZE);

        /* Truncate oversized frames. */
        if (uSize > VIRTIONET_MAX_FRAME_SIZE)
            uSize = VIRTIONET_MAX_FRAME_SIZE;

        if (pThisCC->pDrv)
        {
            PDMNETWORKGSO  Gso;
            PPDMNETWORKGSO pGso = virtioNetR3SetupGsoCtx(&Gso, &PktHdr);
            uint64_t uOffset;

            /** @todo Optimize away the extra copying! (lazy bird) */
            PPDMSCATTERGATHER pSgBufToPdmLeafDevice;
            rc = pThisCC->pDrv->pfnAllocBuf(pThisCC->pDrv, uSize, pGso, &pSgBufToPdmLeafDevice);
            if (RT_SUCCESS(rc))
            {
                STAM_REL_COUNTER_INC(&pThis->StatTransmitPackets);
                STAM_PROFILE_START(&pThis->StatTransmitSend, a);

                uSize -= sizeof(PktHdr);
                rc = virtioNetR3ReadHeader(pDevIns, paSegsFromGuest[0].gcPhys, &PktHdr, uSize);
                if (RT_FAILURE(rc))
                    return;
                virtioCoreSgBufAdvance(pSgPhysSend, sizeof(PktHdr));

                size_t cbCopied = 0;
                size_t cbTotal = 0;
                size_t cbRemain = pSgBufToPdmLeafDevice->cbUsed = uSize;
                uOffset = 0;
                while (cbRemain)
                {
                    PVIRTIOSGSEG paSeg  = &pSgPhysSend->paSegs[pSgPhysSend->idxSeg];
                    uint64_t srcSgStart = (uint64_t)paSeg->gcPhys;
                    uint64_t srcSgLen   = (uint64_t)paSeg->cbSeg;
                    uint64_t srcSgCur   = (uint64_t)pSgPhysSend->gcPhysCur;
                    cbCopied = RT_MIN((uint64_t)cbRemain, srcSgLen - (srcSgCur - srcSgStart));
                    PDMDevHlpPCIPhysRead(pDevIns,
                                         (RTGCPHYS)pSgPhysSend->gcPhysCur,
                                         ((uint8_t *)pSgBufToPdmLeafDevice->aSegs[0].pvSeg) + uOffset, cbCopied);
                    virtioCoreSgBufAdvance(pSgPhysSend, cbCopied);
                    cbRemain -= cbCopied;
                    uOffset += cbCopied;
                    cbTotal += cbCopied;
                }

                LogFunc((".... Copied %lu bytes to %lu byte guest buffer, residual=%lu\n",
                     cbTotal, pDescChain->cbPhysSend, pDescChain->cbPhysSend - cbTotal));

                rc = virtioNetR3TransmitFrame(pThis, pThisCC, pSgBufToPdmLeafDevice, pGso, &PktHdr);
                if (RT_FAILURE(rc))
                {
                    LogFunc(("%s Failed to transmit frame, rc = %Rrc\n", INSTANCE(pThis), rc));
                    STAM_PROFILE_STOP(&pThis->StatTransmitSend, a);
                    STAM_PROFILE_ADV_STOP(&pThis->StatTransmit, a);
                    pThisCC->pDrv->pfnFreeBuf(pThisCC->pDrv, pSgBufToPdmLeafDevice);
                }
                STAM_PROFILE_STOP(&pThis->StatTransmitSend, a);
                STAM_REL_COUNTER_ADD(&pThis->StatTransmitBytes, uOffset);
            }
            else
            {
                Log4Func(("Failed to allocate S/G buffer: size=%u rc=%Rrc\n", uSize, rc));
                /* Stop trying to fetch TX descriptors until we get more bandwidth. */
                virtioCoreR3DescChainRelease(pVirtio, pDescChain);
                break;
            }

            /* Remove this descriptor chain from the available ring */
            virtioCoreR3QueueSkip(pVirtio, idxTxQueue);

            /* No data to return to guest, but call is needed put elem (e.g. desc chain) on used ring */
            virtioCoreR3QueuePut(pVirtio->pDevIns, pVirtio, idxTxQueue, NULL, pDescChain, false);

            /* Update used ring idx and notify guest that we've transmitted the data it sent */
            virtioCoreQueueSync(pVirtio->pDevIns, pVirtio, idxTxQueue, false);
        }

        virtioCoreR3DescChainRelease(pVirtio, pDescChain);
        pDescChain = NULL;
    }
    virtioNetR3SetWriteLed(pThisCC, false);

    if (pDrv)
        pDrv->pfnEndXmit(pDrv);

    ASMAtomicWriteU32(&pThis->uIsTransmitting, 0);
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnXmitPending}
 */
static DECLCALLBACK(void) virtioNetR3NetworkDown_XmitPending(PPDMINETWORKDOWN pInterface)
{
    LogFunc(("\n"));
    PVIRTIONETCC    pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkDown);
    PPDMDEVINS      pDevIns = pThisCC->pDevIns;
    PVIRTIONET      pThis   = PDMDEVINS_2_DATA(pThisCC->pDevIns, PVIRTIONET);

    STAM_COUNTER_INC(&pThis->StatTransmitByNetwork);

    /** @todo If we ever start using more than one Rx/Tx queue pair, is a random queue
          selection algorithm feasible or even necessary */
    virtioNetR3TransmitPendingPackets(pDevIns, pThis, pThisCC, TXQIDX(0), false /*fOnWorkerThread*/);
}

/**
 * @callback_method_impl{VIRTIOCORER3,pfnQueueNotified}
 */
static DECLCALLBACK(void) virtioNetR3QueueNotified(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint16_t idxQueue)
{
    PVIRTIONET         pThis     = RT_FROM_MEMBER(pVirtio, VIRTIONET, Virtio);
    PVIRTIONETCC       pThisCC   = RT_FROM_MEMBER(pVirtioCC, VIRTIONETCC, Virtio);
    PPDMDEVINS         pDevIns   = pThisCC->pDevIns;

    uint16_t idxWorker;
    if (idxQueue == CTRLQIDX)
        idxWorker = pThis->cWorkers - 1;
    else
        idxWorker = idxQueue / 2;

    PVIRTIONETWORKER   pWorker   = &pThis->aWorkers[idxWorker];
    PVIRTIONETWORKERR3 pWorkerR3 = &pThisCC->aWorkers[idxWorker];
    AssertReturnVoid(idxQueue < pThis->cVirtQueues);

#ifdef LOG_ENABLED
    RTLogFlush(NULL);
#endif

    Log10Func(("%s %s has available buffers\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));

    if (IS_RX_QUEUE(idxQueue))
    {
        Log10Func(("%s Receive buffers have been added, waking Rx thread.\n",
            INSTANCE(pThis)));
        virtioNetR3WakeupRxBufWaiter(pDevIns);
    }
    else
    {
        /* Wake queue's worker thread up if sleeping (e.g. a Tx queue, or the control queue */
        if (!ASMAtomicXchgBool(&pWorkerR3->fNotified, true))
        {
            if (ASMAtomicReadBool(&pWorkerR3->fSleeping))
            {
                Log10Func(("%s waking %s worker.\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
                int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pWorker->hEvtProcess);
                AssertRC(rc);
            }
        }
    }
}

/**
 * @callback_method_impl{FNPDMTHREADDEV}
 */
static DECLCALLBACK(int) virtioNetR3WorkerThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVIRTIONET         pThis     = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC       pThisCC   = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    uint16_t const     idxWorker = (uint16_t)(uintptr_t)pThread->pvUser;
    PVIRTIONETWORKER   pWorker   = &pThis->aWorkers[idxWorker];
    PVIRTIONETWORKERR3 pWorkerR3 = &pThisCC->aWorkers[idxWorker];
    uint16_t const     idxQueue  = pWorkerR3->idxQueue;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
    {
        return VINF_SUCCESS;
    }
    LogFunc(("%s worker thread started for %s\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
    virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, false);
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (virtioCoreQueueIsEmpty(pDevIns, &pThis->Virtio, idxQueue))
        {
            virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, true);
            /* Atomic interlocks avoid missing alarm while going to sleep & notifier waking the awoken */
            ASMAtomicWriteBool(&pWorkerR3->fSleeping, true);
            bool fNotificationSent = ASMAtomicXchgBool(&pWorkerR3->fNotified, false);
            if (!fNotificationSent)
            {
                virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, true);
                Log10Func(("%s %s worker sleeping...\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
                Assert(ASMAtomicReadBool(&pWorkerR3->fSleeping));
                int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
                STAM_COUNTER_INC(&pThis->StatTransmitByThread);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    return VINF_SUCCESS;
                if (rc == VERR_INTERRUPTED)
                {
                    virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, false);
                    continue;
                }
                Log10Func(("%s %s worker woken\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
                ASMAtomicWriteBool(&pWorkerR3->fNotified, false);
            }
            ASMAtomicWriteBool(&pWorkerR3->fSleeping, false);
            virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, false);
        }

        /* Dispatch to the handler for the queue this worker is set up to drive */

        if (!pThisCC->fQuiescing)
        {
             if (IS_CTRL_QUEUE(idxQueue))
             {
                 Log10Func(("%s fetching next descriptor chain from %s\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
                 PVIRTIO_DESC_CHAIN_T pDescChain = NULL;
                 int rc = virtioCoreR3QueueGet(pDevIns, &pThis->Virtio, idxQueue, &pDescChain, true);
                 if (rc == VERR_NOT_AVAILABLE)
                 {
                    Log10Func(("%s Nothing found in %s\n", INSTANCE(pThis), VIRTQNAME(idxQueue)));
                    continue;
                 }
                 virtioNetR3Ctrl(pDevIns, pThis, pThisCC, pDescChain);
                 virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain);
             }
             else if (IS_TX_QUEUE(idxQueue))
             {
                 Log10Func(("%s Notified of data to transmit\n", INSTANCE(pThis)));
                 virtioNetR3TransmitPendingPackets(pDevIns, pThis, pThisCC,
                                                   idxQueue, true /* fOnWorkerThread */);
             }

             /* Rx queues aren't handled by our worker threads. Instead, the PDM network
              * leaf driver invokes PDMINETWORKDOWN.pfnWaitReceiveAvail() callback,
              * which waits until notified directly by virtioNetR3QueueNotified()
              * that guest IN buffers have been added to receive virt queue.
              */
        }
    }
    return VINF_SUCCESS;
}

DECLINLINE(int) virtioNetR3CsEnter(PPDMDEVINS pDevIns, PVIRTIONET pThis, int rcBusy)
{
    RT_NOREF(pDevIns, pThis, rcBusy);
    /* Original DevVirtioNet uses CS in attach/detach/link-up timer/tx timer/transmit */
    LogFunc(("%s CS unimplemented. What does the critical section protect in orig driver??", INSTANCE(pThis)));
    return VINF_SUCCESS;
}

DECLINLINE(void) virtioNetR3CsLeave(PPDMDEVINS pDevIns, PVIRTIONET pThis)
{
    RT_NOREF(pDevIns, pThis);
    LogFunc(("%s CS unimplemented. What does the critical section protect in orig driver??", INSTANCE(pThis)));
}


/**
 * @callback_method_impl{FNTMTIMERDEV, Link Up Timer handler.}
 */
static DECLCALLBACK(void) virtioNetR3LinkUpTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    RT_NOREF(pTimer, pvUser);

    int rc = virtioNetR3CsEnter(pDevIns, pThis, VERR_SEM_BUSY);
    AssertRCReturnVoid(rc);

    SET_LINK_UP(pThis);

    virtioNetR3WakeupRxBufWaiter(pDevIns);

    virtioNetR3CsLeave(pDevIns, pThis);

    LogFunc(("%s Link is up\n", INSTANCE(pThis)));

    if (pThisCC->pDrv)
        pThisCC->pDrv->pfnNotifyLinkChanged(pThisCC->pDrv, PDMNETWORKLINKSTATE_UP);
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
 * @param   pDevIns     The device instance.
 * @param   pThis       The virtio-net shared instance data.
 * @param   pThisCC     The virtio-net ring-3 instance data.
 */
static void virtioNetR3TempLinkDown(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC)
{
    if (IS_LINK_UP(pThis))
    {
        SET_LINK_DOWN(pThis);

        /* Restore the link back in 5 seconds. */
        int rc = PDMDevHlpTimerSetMillies(pDevIns, pThisCC->hLinkUpTimer, pThis->cMsLinkUpDelay);
        AssertRC(rc);

        LogFunc(("%s Link is down temporarily\n", INSTANCE(pThis)));
    }
}

/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnGetLinkState}
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) virtioNetR3NetworkConfig_GetLinkState(PPDMINETWORKCONFIG pInterface)
{
    PVIRTIONETCC pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkConfig);
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pThisCC->pDevIns, PVIRTIONET);

    return IS_LINK_UP(pThis) ? PDMNETWORKLINKSTATE_UP : PDMNETWORKLINKSTATE_DOWN;
}

/**
 * @interface_method_impl{PDMINETWORKCONFIG,pfnSetLinkState}
 */
static DECLCALLBACK(int) virtioNetR3NetworkConfig_SetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    PVIRTIONETCC pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkConfig);
    PPDMDEVINS   pDevIns = pThisCC->pDevIns;
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    bool fCachedLinkIsUp = IS_LINK_UP(pThis);
    bool fActiveLinkIsUp = (enmState == PDMNETWORKLINKSTATE_UP);

    Log7Func(("%s enmState=%d\n", INSTANCE(pThis), enmState));
    if (enmState == PDMNETWORKLINKSTATE_DOWN_RESUME)
    {
        if (fCachedLinkIsUp)
        {
            /*
             * We bother to bring the link down only if it was up previously. The UP link state
             * notification will be sent when the link actually goes up in virtioNetR3LinkUpTimer().
             */
            virtioNetR3TempLinkDown(pDevIns, pThis, pThisCC);
            if (pThisCC->pDrv)
                pThisCC->pDrv->pfnNotifyLinkChanged(pThisCC->pDrv, enmState);
        }
    }
    else if (fActiveLinkIsUp != fCachedLinkIsUp)
    {
        if (fCachedLinkIsUp)
        {
            Log(("%s Link is up\n", INSTANCE(pThis)));
            pThis->fCableConnected = true;
            SET_LINK_UP(pThis);
            virtioCoreNotifyConfigChanged(&pThis->Virtio);
        }
        else /* cached Link state is down */
        {
            /* The link was brought down explicitly, make sure it won't come up by timer.  */
            PDMDevHlpTimerStop(pDevIns, pThisCC->hLinkUpTimer);
            Log(("%s Link is down\n", INSTANCE(pThis)));
            pThis->fCableConnected = false;
            SET_LINK_DOWN(pThis);
            virtioCoreNotifyConfigChanged(&pThis->Virtio);
        }
        if (pThisCC->pDrv)
            pThisCC->pDrv->pfnNotifyLinkChanged(pThisCC->pDrv, enmState);
    }
    return VINF_SUCCESS;
}

static int virtioNetR3DestroyWorkerThreads(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC)
{
    Log10Func(("%s\n", INSTANCE(pThis)));
    int rc = VINF_SUCCESS;
    for (unsigned idxWorker = 0; idxWorker < pThis->cWorkers; idxWorker++)
    {
        PVIRTIONETWORKER pWorker = &pThis->aWorkers[idxWorker];
        if (pWorker->hEvtProcess != NIL_SUPSEMEVENT)
        {
            PDMDevHlpSUPSemEventClose(pDevIns, pWorker->hEvtProcess);
            pWorker->hEvtProcess = NIL_SUPSEMEVENT;
        }
        if (pThisCC->aWorkers[idxWorker].pThread)
        {
            int rcThread;
            rc = PDMDevHlpThreadDestroy(pDevIns, pThisCC->aWorkers[idxWorker].pThread, &rcThread);
            if (RT_FAILURE(rc) || RT_FAILURE(rcThread))
                AssertMsgFailed(("%s Failed to destroythread rc=%Rrc rcThread=%Rrc\n", __FUNCTION__, rc, rcThread));
           pThisCC->aWorkers[idxWorker].pThread = NULL;
        }
    }
    return rc;
}

static int virtioNetR3CreateOneWorkerThread(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                            uint16_t idxWorker, uint16_t idxQueue)
{
    Log10Func(("%s\n", INSTANCE(pThis)));
    int rc = VINF_SUCCESS;
    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->aWorkers[idxWorker].hEvtProcess);

    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("DevVirtioNET: Failed to create SUP event semaphore"));

    LogFunc(("creating thread, idxWorker=%d, idxQueue=%d\n", idxWorker, idxQueue));
    rc = PDMDevHlpThreadCreate(pDevIns, &pThisCC->aWorkers[idxWorker].pThread,
                               (void *)(uintptr_t)idxWorker, virtioNetR3WorkerThread,
                               virtioNetR3WakeupWorker, 0, RTTHREADTYPE_IO, VIRTQNAME(idxQueue));
    if (rc != VINF_SUCCESS)
    {
        LogRel(("Error creating thread for Virtual Queue %s: %Rrc\n", VIRTQNAME(idxQueue), rc));
        return rc;
    }
    pThisCC->aWorkers[idxWorker].idxQueue = idxQueue;
    pThis->afQueueAttached[idxQueue] = true;
    return rc;
}

static int virtioNetR3CreateWorkerThreads(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC)
{
    Log10Func(("%s\n", INSTANCE(pThis)));

    int rc;
    uint16_t idxWorker = 0;
    for (uint16_t idxQueuePair = 0; idxQueuePair < pThis->cVirtqPairs; idxQueuePair++)
    {
        rc = virtioNetR3CreateOneWorkerThread(pDevIns, pThis, pThisCC, idxWorker, TXQIDX(idxQueuePair));
        AssertRCReturn(rc, rc);
        idxWorker++;
    }
    rc = virtioNetR3CreateOneWorkerThread(pDevIns, pThis, pThisCC, idxWorker, CTRLQIDX);
    pThis->cWorkers = idxWorker + 1;
    return rc;
}
/**
 * @callback_method_impl{VIRTIOCORER3,pfnStatusChanged}
 */
static DECLCALLBACK(void) virtioNetR3StatusChanged(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint32_t fVirtioReady)
{
    PVIRTIONET     pThis     = RT_FROM_MEMBER(pVirtio,  VIRTIONET, Virtio);
    PVIRTIONETCC   pThisCC   = RT_FROM_MEMBER(pVirtioCC, VIRTIONETCC, Virtio);

    pThis->fVirtioReady = fVirtioReady;

    if (fVirtioReady)
    {
        LogFunc(("%s VirtIO ready\n-----------------------------------------------------------------------------------------\n",
                 INSTANCE(pThis)));
        pThis->virtioNetConfig.uStatus = pThis->fCableConnected ? VIRTIONET_F_LINK_UP : 0;

        pThis->fResetting    = false;
        pThisCC->fQuiescing  = false;
        pThis->fNegotiatedFeatures = virtioCoreGetAcceptedFeatures(pVirtio);
#ifdef LOG_ENABLED
        virtioPrintFeatures(pVirtio);
        virtioNetPrintFeatures(pThis);
#endif
        for (unsigned idxQueue = 0; idxQueue < pThis->cVirtQueues; idxQueue++)
        {
            (void) virtioCoreR3QueueAttach(&pThis->Virtio, idxQueue, VIRTQNAME(idxQueue));
            pThis->afQueueAttached[idxQueue] = true;
            virtioCoreQueueIsEmpty(pThisCC->pDevIns, &pThis->Virtio, idxQueue);
            virtioCoreQueueSetNotify(&pThis->Virtio, idxQueue, true);
        }
    }
    else
    {
        LogFunc(("%s VirtIO is resetting\n", INSTANCE(pThis)));

        pThis->virtioNetConfig.uStatus = pThis->fCableConnected ? VIRTIONET_F_LINK_UP : 0;
        Log7Func(("%s Link is %s\n", INSTANCE(pThis), pThis->fCableConnected ? "up" : "down"));

        pThis->fPromiscuous         = true;
        pThis->fAllMulticast        = false;
        pThis->fAllUnicast          = false;
        pThis->fNoMulticast         = false;
        pThis->fNoUnicast           = false;
        pThis->fNoBroadcast         = false;
        pThis->uIsTransmitting      = 0;
        pThis->cUnicastFilterMacs   = 0;
        pThis->cMulticastFilterMacs = 0;

        memset(pThis->aMacMulticastFilter,  0, sizeof(pThis->aMacMulticastFilter));
        memset(pThis->aMacUnicastFilter,    0, sizeof(pThis->aMacUnicastFilter));
        memset(pThis->aVlanFilter,          0, sizeof(pThis->aVlanFilter));

        pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, true);

        for (unsigned idxQueue = 0; idxQueue < pThis->cVirtQueues; idxQueue++)
            pThis->afQueueAttached[idxQueue] = false;
    }
}
#endif /* IN_RING3 */

/**
 * @interface_method_impl{PDMDEVREGR3,pfnDetach}
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 */
static DECLCALLBACK(void) virtioNetR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    Log7Func(("%s\n", INSTANCE(pThis)));
    AssertLogRelReturnVoid(iLUN == 0);

    int rc = virtioNetR3CsEnter(pDevIns, pThis, VERR_SEM_BUSY);
    AssertMsgRCReturnVoid(rc, ("Failed to enter critical section"));

    /*
     * Zero important members.
     */
    pThisCC->pDrvBase = NULL;
    pThisCC->pDrv     = NULL;

    virtioNetR3CsLeave(pDevIns, pThis);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnAttach}
 *
 * This is called when we change block driver.
 */
static DECLCALLBACK(int) virtioNetR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PVIRTIONET       pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC     pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    RT_NOREF(fFlags);

    Log7Func(("%s", INSTANCE(pThis)));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    int rc = virtioNetR3CsEnter(pDevIns, pThis, VERR_SEM_BUSY);
    AssertMsgRCReturn(rc, ("Failed to enter critical section"), rc);

    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pDevIns->IBase, &pThisCC->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        pThisCC->pDrv = PDMIBASE_QUERY_INTERFACE(pThisCC->pDrvBase, PDMINETWORKUP);
        AssertMsgStmt(pThisCC->pDrv, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                      rc = VERR_PDM_MISSING_INTERFACE_BELOW);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
                    Log(("%s No attached driver!\n", INSTANCE(pThis)));

    virtioNetR3CsLeave(pDevIns, pThis);
    return rc;

    AssertRelease(!pThisCC->pDrvBase);
    return rc;
}

/**
 * @interface_method_impl{PDMILEDPORTS,pfnQueryStatusLed}
 */
static DECLCALLBACK(int) virtioNetR3QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVIRTIONETR3 pThisR3 = RT_FROM_MEMBER(pInterface, VIRTIONETR3, ILeds);
    if (iLUN)
        return VERR_PDM_LUN_NOT_FOUND;
    *ppLed = &pThisR3->led;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface,
 */
static DECLCALLBACK(void *) virtioNetR3QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PVIRTIONETR3 pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKDOWN,   &pThisCC->INetworkDown);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThisCC->INetworkConfig);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,          &pThisCC->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,      &pThisCC->ILeds);
    return NULL;
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnDestruct}
 */
static DECLCALLBACK(int) virtioNetR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    Log(("%s Destroying instance\n", INSTANCE(pThis)));

    if (pThis->hEventRxDescAvail != NIL_SUPSEMEVENT)
    {
        PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEventRxDescAvail);
        PDMDevHlpSUPSemEventClose(pDevIns, pThis->hEventRxDescAvail);
        pThis->hEventRxDescAvail = NIL_SUPSEMEVENT;
    }

    virtioNetR3DestroyWorkerThreads(pDevIns, pThis, pThisCC);

    virtioCoreR3Term(pDevIns, &pThis->Virtio, &pThisCC->Virtio);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnConstruct}
 */
static DECLCALLBACK(int) virtioNetR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    PCPDMDEVHLPR3 pHlp   = pDevIns->pHlpR3;

    /*
     * Quick initialization of the state data, making sure that the destructor always works.
     */
    Log7Func(("PDM device instance: %d\n", iInstance));


    RTStrPrintf(pThis->szInstanceName, sizeof(pThis->szInstanceName), "VIRTIONET", iInstance);

    pThisCC->pDevIns     = pDevIns;

    pThisCC->IBase.pfnQueryInterface = virtioNetR3QueryInterface;
    pThisCC->ILeds.pfnQueryStatusLed = virtioNetR3QueryStatusLed;
    pThisCC->led.u32Magic = PDMLED_MAGIC;

    /* Interfaces */
    pThisCC->INetworkDown.pfnWaitReceiveAvail = virtioNetR3NetworkDown_WaitReceiveAvail;
    pThisCC->INetworkDown.pfnReceive          = virtioNetR3NetworkDown_Receive;
    pThisCC->INetworkDown.pfnReceiveGso       = virtioNetR3NetworkDown_ReceiveGso;
    pThisCC->INetworkDown.pfnXmitPending      = virtioNetR3NetworkDown_XmitPending;
    pThisCC->INetworkConfig.pfnGetMac         = virtioNetR3NetworkConfig_GetMac;
    pThisCC->INetworkConfig.pfnGetLinkState   = virtioNetR3NetworkConfig_GetLinkState;
    pThisCC->INetworkConfig.pfnSetLinkState   = virtioNetR3NetworkConfig_SetLinkState;

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "MAC|CableConnected|LineSpeed|LinkUpDelay|StatNo", "");

    /* Get config params */
    int rc = pHlp->pfnCFGMQueryBytes(pCfg, "MAC", pThis->macConfigured.au8, sizeof(pThis->macConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get MAC address"));

    rc = pHlp->pfnCFGMQueryBool(pCfg, "CableConnected", &pThis->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'CableConnected'"));

    uint32_t uStatNo = iInstance;
    rc = pHlp->pfnCFGMQueryU32Def(pCfg, "StatNo", &uStatNo, iInstance);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"StatNo\" value"));

    rc = pHlp->pfnCFGMQueryU32Def(pCfg, "LinkUpDelay", &pThis->cMsLinkUpDelay, 5000); /* ms */
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'LinkUpDelay'"));

    Assert(pThis->cMsLinkUpDelay <= 300000); /* less than 5 minutes */

    if (pThis->cMsLinkUpDelay > 5000 || pThis->cMsLinkUpDelay < 100)
        LogRel(("%s WARNING! Link up delay is set to %u seconds!\n",
                INSTANCE(pThis), pThis->cMsLinkUpDelay / 1000));

    Log(("%s Link up delay is set to %u seconds\n", INSTANCE(pThis), pThis->cMsLinkUpDelay / 1000));

    /* Copy the MAC address configured for the VM to the MMIO accessible Virtio dev-specific config area */
    memcpy(pThis->virtioNetConfig.uMacAddress.au8, pThis->macConfigured.au8, sizeof(pThis->virtioNetConfig.uMacAddress)); /* TBD */

    /*
     * Do core virtio initialization.
     */

#if FEATURE_OFFERED(STATUS)
    pThis->virtioNetConfig.uStatus = 0;
#endif

#if FEATURE_OFFERED(MQ)
    pThis->virtioNetConfig.uMaxVirtqPairs = VIRTIONET_MAX_QPAIRS;
#endif

    /* Initialize the generic Virtio core: */
    pThisCC->Virtio.pfnStatusChanged        = virtioNetR3StatusChanged;
    pThisCC->Virtio.pfnQueueNotified        = virtioNetR3QueueNotified;
    pThisCC->Virtio.pfnDevCapRead           = virtioNetR3DevCapRead;
    pThisCC->Virtio.pfnDevCapWrite          = virtioNetR3DevCapWrite;

    VIRTIOPCIPARAMS VirtioPciParams;
    VirtioPciParams.uDeviceId               = PCI_DEVICE_ID_VIRTIONET_HOST;
    VirtioPciParams.uClassBase              = PCI_CLASS_BASE_NETWORK_CONTROLLER;
    VirtioPciParams.uClassSub               = PCI_CLASS_SUB_NET_ETHERNET_CONTROLLER;
    VirtioPciParams.uClassProg              = PCI_CLASS_PROG_UNSPECIFIED;
    VirtioPciParams.uSubsystemId            = PCI_DEVICE_ID_VIRTIONET_HOST;  /* VirtIO 1.0 spec allows PCI Device ID here */
    VirtioPciParams.uInterruptLine          = 0x00;
    VirtioPciParams.uInterruptPin           = 0x01;

    /*
     * Initialize VirtIO core. This will result in a "status changed" callback
     * when VirtIO is ready, at which time the Rx queue and ctrl queue worker threads will be created.
     */
    rc = virtioCoreR3Init(pDevIns, &pThis->Virtio, &pThisCC->Virtio, &VirtioPciParams, INSTANCE(pThis),
                          VIRTIONET_HOST_FEATURES_OFFERED,
                          &pThis->virtioNetConfig /*pvDevSpecificCap*/, sizeof(pThis->virtioNetConfig));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: failed to initialize VirtIO"));

    pThis->fNegotiatedFeatures = virtioCoreGetNegotiatedFeatures(&pThis->Virtio);
    if (!virtioNetValidateRequiredFeatures(pThis->fNegotiatedFeatures))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: Required features not successfully negotiated."));

    pThis->cVirtqPairs =   pThis->fNegotiatedFeatures & VIRTIONET_F_MQ
                         ? pThis->virtioNetConfig.uMaxVirtqPairs : 1;

    pThis->cVirtQueues += pThis->cVirtqPairs * 2 + 1;

    /* Create Link Up Timer */
    rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL, virtioNetR3LinkUpTimer, NULL, TMTIMER_FLAGS_NO_CRIT_SECT,
                              "VirtioNet Link Up Timer", &pThisCC->hLinkUpTimer);

    /*
     * Initialize queues.
     */
    virtioNetR3SetVirtqNames(pThis);

    /*
     * Create queue workers for life of instance. (I.e. they persist through VirtIO bounces)
     */
    rc = virtioNetR3CreateWorkerThreads(pDevIns, pThis, pThisCC);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to worker threads"));

    /*
     * Create the semaphore that will be used to synchronize/throttle
     * the downstream LUN's Rx waiter thread.
     */
    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->hEventRxDescAvail);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to create event semaphore"));

    /*
     * Attach network driver instance
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThisCC->IBase, &pThisCC->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        pThisCC->pDrv = PDMIBASE_QUERY_INTERFACE(pThisCC->pDrvBase, PDMINETWORKUP);
        AssertMsgStmt(pThisCC->pDrv, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                      rc = VERR_PDM_MISSING_INTERFACE_BELOW);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
                    Log(("%s No attached driver!\n", INSTANCE(pThis)));

    /*
     * Status driver
     */
    PPDMIBASE pUpBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThisCC->IBase, &pUpBase, "Status Port");
    if (RT_FAILURE(rc) && rc != VERR_PDM_NO_ATTACHED_DRIVER)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));

    pThisCC->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pUpBase, PDMILEDCONNECTORS);

    /*
     * Register saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, VIRTIONET_SAVED_STATE_VERSION, sizeof(*pThis),
                              virtioNetR3SaveExec, virtioNetR3LoadExec);
    AssertRCReturn(rc, rc);



   /*
     * Statistics and debug stuff.
     * The /Public/ bits are official and used by session info in the GUI.
     */
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatReceiveBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                           "Amount of data received",    "/Public/NetAdapter/%u/BytesReceived", uStatNo);
    PDMDevHlpSTAMRegisterF(pDevIns, &pThis->StatTransmitBytes, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                           "Amount of data transmitted", "/Public/NetAdapter/%u/BytesTransmitted", uStatNo);
    PDMDevHlpSTAMRegisterF(pDevIns, &pDevIns->iInstance,       STAMTYPE_U32,     STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                           "Device instance number",     "/Public/NetAdapter/%u/%s", uStatNo, pDevIns->pReg->szName);

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

    /*
     * Register the debugger info callback (ignore errors).
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%u", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-net info", virtioNetR3Info);
    return rc;
}

#else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) virtioNetRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

LogRelFunc(("\n"));
    return virtioCoreRZInit(pDevIns, &pThis->Virtio, &pThisCC->Virtio);
}

#endif /* !IN_RING3 */



/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioNet_1_0 =
{
    /* .uVersion = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "virtio-net-1-dot-0",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE | PDM_DEVREG_FLAGS_RZ
                                    | PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION
                                    | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
    /* .fClass = */                 PDM_DEVREG_CLASS_NETWORK,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(VIRTIONET),
    /* .cbInstanceCC = */           sizeof(VIRTIONETCC),
    /* .cbInstanceRC = */           sizeof(VIRTIONETRC),
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        VBOX_MSIX_MAX_ENTRIES,
    /* .pszDescription = */         "Virtio Host NET.\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           virtioNetR3Construct,
    /* .pfnDestruct = */            virtioNetR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               virtioNetR3Reset,
    /* .pfnSuspend = */             virtioNetR3Suspend,
    /* .pfnResume = */              virtioNetR3Resume,
    /* .pfnAttach = */              virtioNetR3Attach,
    /* .pfnDetach = */              virtioNetR3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            virtioNetR3PowerOff,
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
    /* .pfnConstruct = */           virtioNetRZConstruct,
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
    /* .pfnConstruct = */           virtioNetRZConstruct,
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
    /* .uVersionEnd = */          PDM_DEVREG_VERSION
};

