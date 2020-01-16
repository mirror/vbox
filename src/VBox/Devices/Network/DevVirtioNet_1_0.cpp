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
 * Copyright (C) 2006-2019 Oracle Corporation
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
#define VIRTIONET_WITH_MERGEABLE_RX_BUFS

#include <VBox/vmm/pdmdev.h>
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

//#include "VBoxNET.h"
#include "VBoxDD.h"

/*
 * GSO = Generic Segmentation Offload
 * TSO = TCP Segmentation Offload
 * UDP = UDP Fragmentation Offset

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current saved state version. */
#define VIRTIONET_SAVED_STATE_VERSION          UINT32_C(1)
#define VIRTIONET_MAX_QPAIRS                   512
#define VIRTIONET_MAX_QUEUES                   (VIRTIONET_MAX_QPAIRS * 2 + 1)
#define VIRTIONET_MAX_FRAME_SIZE               65535 + 18     /**< Max IP pkt size + Ethernet header with VLAN tag  */
#define VIRTIONET_MAC_FILTER_LEN               32
#define VIRTIONET_MAX_VLAN_ID                  (1 << 12)
#define VIRTIONET_PREALLOCATE_RX_SEG_COUNT    32

#define QUEUE_NAME(a_pVirtio, a_idxQueue)   ((a_pVirtio)->virtqState[(a_idxQueue)].szVirtqName)
#define VIRTQNAME(qIdx) (pThis->aszVirtqNames[qIdx])
#define CBVIRTQNAME(qIdx) RTStrNLen(VIRTQNAME(qIdx), sizeof(VIRTQNAME(qIdx)))
#define FEATURE_ENABLED(feature) (pThis->fNegotiatedFeatures & VIRTIONET_F_##feature)
#define FEATURE_DISABLED(feature) (!FEATURE_ENABLED(feature))

/* Macros to calculate queue specific index number VirtIO 1.0, 5.1.2 */
#define RXQIDX(qPairIdx)  (qPairIdx * 2)
#define TXQIDX(qPairIdx)  (qPairIdx * 2 + 1)
#define CTRLQIDX          ((pThis->fNegotiatedFeatures & VIRTIONET_F_MQ) ? VIRTIONET_MAX_QPAIRS * 2 + 2 : 4)

#define RXVIRTQNAME(qPairIdx)  (pThis->aszVirtqNames[RXQIDX(qPairIdx)])
#define TXVIRTQNAME(qPairIdx)  (pThis->aszVirtqNames[TXQIDX(qPairIdx)])
#define CTLVIRTQNAME(qPairIdx) (pThis->aszVirtqNames[CTRLQIDX])

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
      VIRTIONET_F_MAC                   \
    | VIRTIONET_F_STATUS                \
    | VIRTIONET_F_CTRL_VQ               \
    | VIRTIONET_F_CTRL_RX               \
    | VIRTIONET_F_CTRL_VLAN             \
    | VIRTIONET_HOST_FEATURES_GSO

#define PCI_DEVICE_ID_VIRTIONET_HOST               0x1041      /**< Informs guest driver of type of VirtIO device   */
#define PCI_CLASS_BASE_NETWORK_CONTROLLER          0x02        /**< PCI Network device class                   */
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
typedef struct virtio_net_config
{
    uint8_t  uMacAddress[6];                                    /**< mac                                            */
#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_STATUS
    uint16_t uStatus;                                           /**< status                                         */
#endif
#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_MQ
    uint16_t uMaxVirtqPairs;                                    /**< max_virtq_pairs                                */
#endif
} VIRTIONET_CONFIG_T, PVIRTIONET_CONFIG_T;

#define VIRTIONET_F_LINK_UP                RT_BIT_16(1)         /**< config status: Link is up                      */
#define VIRTIONET_F_ANNOUNCE               RT_BIT_16(2)         /**< config status: Announce                        */

/** @name VirtIO 1.0 NET Host Device device specific control types
 * @{  */
#define VIRTIONET_HDR_F_NEEDS_CSUM                   1          /**< Packet needs checksum                          */
#define VIRTIONET_HDR_GSO_NONE                       0          /**< No Global Segmentation Offset                  */
#define VIRTIONET_HDR_GSO_TCPV4                      1          /**< Global Segment Offset for TCPV4                */
#define VIRTIONET_HDR_GSO_UDP                        3          /**< Global Segment Offset for UDP                  */
#define VIRTIONET_HDR_GSO_TCPV6                      4          /**< Global Segment Offset for TCPV6                */
#define VIRTIONET_HDR_GSO_ECN                     0x80          /**< Explicit Congestion Notification               */
/** @} */

/* Device operation: Net header packet (VirtIO 1.0, 5.1.6) */
#pragma pack(1)
struct virtio_net_hdr {
    uint8_t  uFlags;                                           /**< flags                                           */
    uint8_t  uGsoType;                                         /**< gso_type                                        */
    uint16_t uHdrLen;                                          /**< hdr_len                                         */
    uint16_t uGsoSize;                                         /**< gso_size                                        */
    uint16_t uChksumStart;                                     /**< Chksum_start                                      */
    uint16_t uChksumOffset;                                    /**< Chksum_offset                                     */
    uint16_t uNumBuffers;                                      /**< num_buffers                                     */
};
#pragma pack()
typedef virtio_net_hdr VIRTIONET_PKT_HDR_T, *PVIRTIONET_PKT_HDR_T;
AssertCompileSize(VIRTIONET_PKT_HDR_T, 12);

/* Control virtq: Command entry (VirtIO 1.0, 5.1.6.5) */
#pragma pack(1)
struct virtio_net_ctrl_hdr {
    uint8_t uClass;                                             /**< class                                          */
    uint8_t uCmd;                                               /**< command                                        */
    uint8_t uCmdSpecific;                                       /**< command specific                               */
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

/** @name Offload State Configuration Flags (VirtIO 1.0, 5.1.6.5.6.1)
 * @{  */
//#define VIRTIONET_F_GUEST_CSUM                      1           /**< Guest offloads Chksum                             */
//#define VIRTIONET_F_GUEST_TSO4                      7           /**< Guest offloads TSO4                             */
//#define VIRTIONET_F_GUEST_TSO6                      8           /**< Guest Offloads TSO6                             */
//#define VIRTIONET_F_GUEST_ECN                       9           /**< Guest Offloads ECN                              */
//#define VIRTIONET_F_GUEST_UFO                      10           /**< Guest Offloads UFO                              */
/** @} */

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

    uint64_t                fNegotiatedFeatures;

#ifdef VIRTIONET_TX_DELAY
    /** Transmit Delay Timer. */
    TMTIMERHANDLE           hTxTimer;
    uint32_t                ui;
    uint32_t                uAvgDiff;
    uint32_t                uMinDiff;
    uint32_t                uMaxDiff;
    uint64_t                u64NanoTS;
#else  /* !VNET_TX_DELAY */
    /** The event semaphore TX thread waits on. */
    SUPSEMEVENT             hTxEvent;
#endif /* !VNET_TX_DELAY */

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
    bool volatile           fMaybeOutOfSpace;

    /** Flags whether VirtIO core is in ready state */
    uint8_t                 fVirtioReady;

    /** Resetting flag */
    uint8_t                 fResetting;

    /** Promiscuous mode -- RX filter accepts all packets. */
    uint8_t                 fPromiscuous;

    /** All multicast mode -- RX filter accepts all multicast packets. */
    uint8_t                 fAllMulticast;

    /** All unicast mode -- RX filter accepts all unicast packets. */
    uint8_t                 fAllUnicast;

    /** No multicast mode - Supresses multicast receive */
    uint8_t                 fNoMulticat;

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
    uint8_t                 aVlanFilter[VIRTIONET_MAX_VID / sizeof(uint8_t)];

    /* Receive-blocking-related fields ***************************************/

    /** EMT: Gets signalled when more RX descriptors become available. */
    SUPSEMEVENT             hEventMoreRxDescAvail;

    /** Handle of the I/O port range.  */
    IOMIOPORTHANDLE         hIoPorts;

    /** @name Statistic
     * @{ */
    STAMCOUNTER             StatReceiveBytes;
    STAMCOUNTER             StatTransmitBytes;
    STAMCOUNTER             StatReceiveGSO;
    STAMCOUNTER             StatTransmitPackets;
    STAMCOUNTER             StatTransmitGSO;
    STAMCOUNTER             StatTransmitChksum;
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

#ifndef VIRTIONET_TX_DELAY
    R3PTRTYPE(PPDMTHREAD)           pTxThread;
#endif

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


DECLINLINE(void) virtioNetR3SetVirtqNames(PVIRTIONET pThis)
{
    for (uint16_t qPairIdx = 0; qPairIdx < pThis->cVirtqPairs; qPairIdx++)
    {
        RTStrPrintf(pThis->aszVirtqNames[RXQIDX(qPairIdx)], VIRTIO_MAX_QUEUE_NAME_SIZE, "receiveq<%d>",  qPairIdx);
        RTStrPrintf(pThis->aszVirtqNames[TXQIDX(qPairIdx)], VIRTIO_MAX_QUEUE_NAME_SIZE, "transmitq<%d>", qPairIdx);
    }
    RTStrCopy(pThis->aszVirtqNames[CTRLQIDX], VIRTIO_MAX_QUEUE_NAME_SIZE, "controlq");
}


DECLINLINE(void) virtioNetPrintFeatures(PVIRTIONET pThis, uint32_t fFeatures, const char *pcszText)
{
#ifdef LOG_ENABLED
    static struct
    {
        uint32_t fMask;
        const char *pcszDesc;
    } const s_aFeatures[] =
    {
        { VIRTIONET_F_CSUM,                "Host handles packets with partial checksum." },
        { VIRTIONET_F_GUEST_CSUM,          "Guest handles packets with partial checksum." },
        { VIRTIONET_F_CTRL_GUEST_OFFLOADS, "Control channel offloads reconfiguration support." },
        { VIRTIONET_F_MAC,                 "Host has given MAC address." },
        { VIRTIONET_F_GUEST_TSO4,          "Guest can receive TSOv4." },
        { VIRTIONET_F_GUEST_TSO6,          "Guest can receive TSOv6." },
        { VIRTIONET_F_GUEST_ECN,           "Guest can receive TSO with ECN." },
        { VIRTIONET_F_GUEST_UFO,           "Guest can receive UFO." },
        { VIRTIONET_F_HOST_TSO4,           "Host can receive TSOv4." },
        { VIRTIONET_F_HOST_TSO6,           "Host can receive TSOv6." },
        { VIRTIONET_F_HOST_ECN,            "Host can receive TSO with ECN." },
        { VIRTIONET_F_HOST_UFO,            "Host can receive UFO." },
        { VIRTIONET_F_MRG_RXBUF,           "Guest can merge receive buffers." },
        { VIRTIONET_F_STATUS,              "Configuration status field is available." },
        { VIRTIONET_F_CTRL_VQ,             "Control channel is available." },
        { VIRTIONET_F_CTRL_RX,             "Control channel RX mode support." },
        { VIRTIONET_F_CTRL_VLAN,           "Control channel VLAN filtering." },
        { VIRTIONET_F_GUEST_ANNOUNCE,      "Guest can send gratuitous packets." },
        { VIRTIONET_F_MQ,                  "Host supports multiqueue with automatic receive steering." },
        { VIRTIONET_F_CTRL_MAC_ADDR,       "Set MAC address through control channel." }
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

/*
 * Checks whether negotiated features have required flag combinations.
 * See VirtIO 1.0 specification, Section 5.1.3.1 */
DECLINLINE(bool) virtioNetValidateRequiredFeatures(uint32_t fFeatures)
{
    uint32_t fGuestChksumRequired = fFeatures & VIRTIONET_F_GUEST_TSO4
                               || fFeatures & VIRTIONET_F_GUEST_TSO6
                               || fFeatures & VIRTIONET_F_GUEST_UFO;

    uint32_t fHostChksumRequired =  fFeatures & VIRTIONET_F_HOST_TSO4
                               || fFeatures & VIRTIONET_F_HOST_TSO6
                               || fFeatures & VIRTIONET_F_HOST_UFO;

    uint32_t fCtrlVqRequired =    fFeatures & VIRTIONET_F_CTRL_RX
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
 * @callback_method_impl{VIRTIOCORER3,pfnQueueNotified}
 */
static DECLCALLBACK(void) virtioNetR3Notified(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint16_t qIdx)
{
    PVIRTIONET         pThis     = RT_FROM_MEMBER(pVirtio, VIRTIONET, Virtio);
    PVIRTIONETCC       pThisCC   = RT_FROM_MEMBER(pVirtioCC, VIRTIONETCC, Virtio);
    PPDMDEVINS         pDevIns   = pThisCC->pDevIns;
    AssertReturnVoid(qIdx < pThis->cVirtQueues);
    PVIRTIONETWORKER   pWorker   = &pThis->aWorkers[qIdx];
    PVIRTIONETWORKERR3 pWorkerR3 = &pThisCC->aWorkers[qIdx];

#ifdef LOG_ENABLED
    RTLogFlush(NULL);
#endif

    Log6Func(("%s has available data\n", VIRTQNAME(qIdx)));
    /* Wake queue's worker thread up if sleeping */
    if (!ASMAtomicXchgBool(&pWorkerR3->fNotified, true))
    {
        if (ASMAtomicReadBool(&pWorkerR3->fSleeping))
        {
            Log6Func(("waking %s worker.\n", VIRTQNAME(qIdx)));
            int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pWorker->hEvtProcess);
            AssertRC(rc);
        }
    }
}


/*********************************************************************************************************************************
*   Virtio Net config.                                                                                                           *
*********************************************************************************************************************************/

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
         || (   offConfig == RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) \
             && cb == RT_SIZEOFMEMB(VIRTIONET_CONFIG_T, member)) )

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
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy(pv, (const char *)&pThis->virtioNetConfig.member + offIntra, cb); \
            LOG_NET_CONFIG_ACCESSOR(member); \
        } \
    } while(0)


#if 0

static int virtioNetR3CfgAccessed(PVIRTIONET pThis, uint32_t offConfig, void *pv, uint32_t cb, bool fWrite)
{
    AssertReturn(pv && cb <= sizeof(uint32_t), fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00);

    if (MATCH_NET_CONFIG(uMacAddress))
        NET_CONFIG_ACCESSOR_READONLY(uMacAddress);
#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_STATUS
    else
    if (MATCH_NET_CONFIG(uStatus))
        NET_CONFIG_ACCESSOR_READONLY(uStatus);
#endif
#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_MQ
    else
    if (MATCH_NET_CONFIG(uMaxVirtqPairs))
        NET_CONFIG_ACCESSOR_READONLY(uMaxVirtqPairs);
#endif
    else
    {
        LogFunc(("Bad access by guest to virtio_net_config: off=%u (%#x), cb=%u\n", offConfig, offConfig, cb));
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
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, pv, cb, false /*fRead*/);
}

/**
 * @callback_method_impl{VIRTIOCORER3,pfnDevCapWrite}
 */
static DECLCALLBACK(int) virtioNetR3DevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, uint32_t cb)
{
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, (void *)pv, cb, true /*fWrite*/);
}

#endif


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
    LogFunc(("LOAD EXEC!!\n"));

    AssertReturn(uPass == SSM_PASS_FINAL, VERR_SSM_UNEXPECTED_PASS);
    AssertLogRelMsgReturn(uVersion == VIRTIONET_SAVED_STATE_VERSION,
                          ("uVersion=%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    virtioNetR3SetVirtqNames(pThis);
    for (int qIdx = 0; qIdx < pThis->cVirtQueues; qIdx++)
        pHlp->pfnSSMGetBool(pSSM, &pThis->afQueueAttached[qIdx]);

    /*
     * Call the virtio core to let it load its state.
     */
    int rc = virtioCoreR3LoadExec(&pThis->Virtio, pDevIns->pHlpR3, pSSM);

    /*
     * Nudge queue workers
     */
    for (int qIdx = 0; qIdx < pThis->cVirtqPairs; qIdx++)
    {
        if (pThis->afQueueAttached[qIdx])
        {
            LogFunc(("Waking %s worker.\n", VIRTQNAME(qIdx)));
            rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->aWorkers[qIdx].hEvtProcess);
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
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    RT_NOREF(pThisCC);

    LogFunc(("SAVE EXEC!!\n"));

    for (int qIdx = 0; qIdx < pThis->cVirtQueues; qIdx++)
        pHlp->pfnSSMPutBool(pSSM, pThis->afQueueAttached[qIdx]);

    /*
     * Call the virtio core to let it save its state.
     */
    return virtioCoreR3SaveExec(&pThis->Virtio, pDevIns->pHlpR3, pSSM);
}


/*********************************************************************************************************************************
*   Device interface.                                                                                                            *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNPDMDEVASYNCNOTIFY}
 */
static DECLCALLBACK(bool) virtioNetR3DeviceQuiesced(PPDMDEVINS pDevIns)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

//    if (ASMAtomicReadu(&pThis->cActiveReqs))
//        return false;

    LogFunc(("Device I/O activity quiesced: %s\n",
        virtioCoreGetStateChangeText(pThisCC->enmQuiescingFor)));

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
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    RT_NOREF(pThis);

    /* Prevent worker threads from removing/processing elements from virtq's */
    pThisCC->fQuiescing = true;
    pThisCC->enmQuiescingFor = enmQuiescingFor;

    PDMDevHlpSetAsyncNotification(pDevIns, virtioNetR3DeviceQuiesced);

    /* If already quiesced invoke async callback.  */
//    if (!ASMAtomicReadu(&pThis->cActiveReqs))
//        PDMDevHlpAsyncNotificationCompleted(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnReset}
 */
static DECLCALLBACK(void) virtioNetR3Reset(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    pThis->fResetting = true;
    virtioNetR3QuiesceDevice(pDevIns, kvirtIoVmStateChangedReset);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnPowerOff}
 */
static DECLCALLBACK(void) virtioNetR3SuspendOrPowerOff(PPDMDEVINS pDevIns, VIRTIOVMSTATECHANGED enmType)
{
    LogFunc(("\n"));

    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    RT_NOREF2(pThis, pThisCC);

    /* VM is halted, thus no new I/O being dumped into queues by the guest.
     * Workers have been flagged to stop pulling stuff already queued-up by the guest.
     * Now tell lower-level to to suspend reqs (for example, DrvVD suspends all reqs
     * on its wait queue, and we will get a callback as the state changes to
     * suspended (and later, resumed) for each).
     */

    virtioNetR3QuiesceDevice(pDevIns, enmType);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3PowerOff(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));
    virtioNetR3SuspendOrPowerOff(pDevIns, kvirtIoVmStateChangedPowerOff);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3Suspend(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));
    virtioNetR3SuspendOrPowerOff(pDevIns, kvirtIoVmStateChangedSuspend);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnResume}
 */
static DECLCALLBACK(void) virtioNetR3Resume(PPDMDEVINS pDevIns)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    LogFunc(("\n"));

    pThisCC->fQuiescing = false;

    /* Wake worker threads flagged to skip pulling queue entries during quiesce
     * to ensure they re-check their queues. Active request queues may already
     * be awake due to new reqs coming in.
     */
/*
    for (uint16_t qIdx = 0; qIdx < VIRTIONET_REQ_QUEUE_CNT; qIdx++)
    {
        if (ASMAtomicReadBool(&pThisCC->aWorkers[qIdx].fSleeping))
        {
            Log6Func(("waking %s worker.\n", VIRTQNAME(qIdx)));
            int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->aWorkers[qIdx].hEvtProcess);
            AssertRC(rc);
        }
    }
*/
    /* Ensure guest is working the queues too. */
    virtioCoreR3VmStateChanged(&pThis->Virtio, kvirtIoVmStateChangedResume);
}

/* Device operation: Net header packet (VirtIO 1.0, 5.1.6) */
#pragma pack(1)
struct virtio_net_hdr {
    uint8_t  uFlags;                                           /**< flags                                           */
    uint8_t  uGsoType;                                         /**< gso_type                                        */
    uint16_t uHdrLen;                                          /**< hdr_len                                         */
    uint16_t uGsoSize;                                         /**< gso_size                                        */
    uint16_t uChksumStart;                                     /**< csum_start                                      */
    uint16_t uChksumOffset;                                    /**< csum_offset                                     */
    uint16_t uNumBuffers;                                      /**< num_buffers                                     */
};
#pragma pack()
typedef virtio_net_hdr VIRTIONET_PKT_HDR_T, *PVIRTIONET_PKT_HDR_T;
AssertCompileSize(VIRTIONET_PKT_HDR_T, 12);

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
    uint16_t *uPtr = (uint16_t*)pvBuf;
    /* Compare TPID with VLAN Ether Type */
    if (   uPtr[6] == RT_H2BE_u(0x8100)
        && !ASMBitTest(pThis->aVlanFilter, RT_BE2H_u(uPtr[7]) & 0xFFF))
    {
        Log4(("%s vnetR3AddressFilter: not our VLAN, returning false\n", INSTANCE(pThis)));
        return false;
    }

    if (vnetR3IsBroadcast(pvBuf))
        return true;

    if (pThis->fAllMulti && vnetR3IsMulticast(pvBuf))
        return true;

    if (!memcmp(pThis->config.mac.au, pvBuf, sizeof(RTMAC)))
        return true;
    Log4(("%s vnetR3AddressFilter: %RTmac (conf) != %RTmac (dest)\n", INSTANCE(pThis), pThis->config.mac.au, pvBuf));

    for (unsigned i = 0; i < pThis->cMacFilterEntries; i++)
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
 * @param   pDevIns         The device instance.
 * @param   pThis           The virtio-net shared instance data.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static int virtioNetR3HandleRxPacket(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                const void *pvBuf, size_t cb, PCPDMNETWORKGSO pGso)
{
    VIRTIONET_PKT_HDR_T rxPktHdr;

    if (pGso)
    {
        Log2Func(("%s gso type=%x cbPktHdrsTotal=%u cbPktHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
              pThis->szInstanceName, pGso->uType, pGso->cbHdrsTotal,
              pGso->cbHdrsSeg, pGso->cbMaxSeg, pGso->offHdr1, pGso->offHdr2));

        rxPktHdr.uFlags = VIRTIONET_HDR_F_NEEDS_CSUM;
        switch (pGso->uType)
        {
            case PDMNETWORKGSOTYPE_IPV4_TCP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_TCPV4;
                rxPktHdr.uCSumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV6_TCP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_TCPV6;
                rxPktHdr.uCSumOffset = RT_OFFSETOF(RTNETTCP, th_sum);
                break;
            case PDMNETWORKGSOTYPE_IPV4_UDP:
                rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_UDP;
                rxPktHdr.uCSumOffset = RT_OFFSETOF(RTNETUDP, uh_sum);
                break;
            default:
                return VERR_INVALID_PARAMETER;
        }
        rxPktHdr.uHdrLen = pGso->cbHdrsTotal;
        rxPktHdr.uGSOSize = pGso->cbMaxSeg;
        rxPktHdr.uCSumStart = pGso->offHdr2;
        STAM_REL_COUNTER_INC(&pThis->StatReceiveGSO);
    }
    else
    {
        rxPktHdr.uFlags = 0;
        rxPktHdr.uGsoType = VIRTIONET_HDR_GSO_NONE;
    }

    virtioNetR3PacketDump(pThis, (const uint8_t *)pvBuf, cb, "<-- Incoming");

    PRTSGBUF pSegsBuf;
    PRTSGSEG paSegs = (PRTSGSEG)RTMemAllocZ(sizeof(RTSGSEG) * 2);
    AssertReturn(paSegs, VERR_NO_MEMORY);
    RTSgBufInit(pSegsBuf, paSegs, cSegs);

    uint16_t *pPhysPktHdrNumBufs, cDescs = 0;

    uint8_t fFirstIteration = true;
    for (uint32_t uOffset = 0; uOffset < cb; fFirstIteration = false)
    {
        PVIRTIO_DESC_CHAIN_T pDescChain;
        int rc = virtioCoreR3QueueGet(pDevIns, &pThis->Virtio, RXQIDX(0), &pDescChain, true);

        AssertRC(rc == VINF_SUCCESS || rc == VERR_NOT_AVAILABLE, rc);

        /* todo:  Find a better way to deal with this */
        AssertMsgReturn(rc == VINF_SUCCESS && pDescChain->cbPhysSend,
                        ("Not enough Rx buffers in queue to accomodate ethernet packet\n"),
                        VERR_INTERNAL_ERROR);

        AssertMsgReturn(pDescChain->cbPhysReturn >= sizeof(VIRTIONET_PKT_HDR_T),
                        ("Desc chain's phys segs have insufficient space for pkt header!\n"),
                        VERR_INTERNAL_ERROR);

        uint32_t cbDescChainLeft = pDescChain->cbPhysSend;

        uint16_t cSegs = 0;
        if (fFirstIteration)
        {
            /* Lead with packet header */
            paSegs[cSegs].cbSeg = sizeof(VIRTIONET_PKT_HDR_T);
            paSegs[cSegs].pvSeg = RTMemAlloc(paSegs[0].cb);
            AssertReturn(paSegs[0].pvSeg, VERR_NO_MEMORY);
            cbDescChainLeft -= paReqSegs[0].cb;
            *pPhysPktHdrNumBufs = ((uint8_t *)paSegs[cSegs].pvSeg)
                                     + RT_UOFFSETOF(VIRTIONET_PKT_HDR_T, uNumBuffers);
            cSegs++;
        }

        /* Append remaining Rx pkt or as much current desc chain has room for */
        uint32_t uSize = RT_MIN(cb, cbDescChainLeft);
        paSegs[cSegs].cbSeg = uSize;
        paSegs[cSegs++].pvSeg = ((uint8_t)pvBuf) + uOffset;
        uOffset += uSize;
        cDescs++;

        virtioCoreR3QueuePut(pDevIns, &pThis->Virtio, RXQIDX(0), pSegsBuf, pDescChain, true);

        if (FEATURE_DISABLED(MRG_RXBUF))
            break;
    }

    /* Fix up pkt hdr (already in guest phys. memory) with number of descriptors to send */

    int rc = PDMDevHlpPCIPhysWrite(pDevIns, pPhysPktHdrNumBuffers, cDescs, sizeof(cDescs));
    AssertMsgRCReturn(rc, "Failure updating descriptor count in pkt hdr in guest physical memory\n");

    virtioCoreQueueSync(pDevIns, &pThis->Virtio, RXQIDX(0));

    for (int i = 0; i < 2; i++)
        RTMemFree(paSegs[i].pvSeg);
    RTMemFree(paSegs);
    RTMemFree(pSegsBuf);

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
static DECLCALLBACK(int) virtioNetR3ReceiveGso(PPDMINETWORKDOWN pInterface, const void *pvBuf,
                                               size_t cb, PCPDMNETWORKGSO pGso)
{
    PVIRTIONETCC    pThisCC = RT_FROM_MEMBER(pInterface, PVIRTIONETCC, INetworkDown);
    PPDMDEVINS      pDevIns = pThisCC->pDevIns;
    PVIRTIONET      pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    if (pGso)
    {
        uint32_t uFeatures = pThis->VPCI.uGuestFeatures;

        switch (pGso->uType)
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
            Log2Func((GSO type (0x%x) not supported\n", pThis->szInstanceName, pGso->uType));
            return VERR_NOT_SUPPORTED;
        }
    }

    Log2Func(("pvBuf=%p cb=%u pGso=%p\n", pThis->szInstanceName, pvBuf, cb, pGso));

    int rc = virtioR3CanReceive(pDevIns, pThis, pThisCC);
    if (RT_FAILURE(rc))
        return rc;

    /* Drop packets if VM is not running or cable is disconnected. */
    VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
    if ((   enmVMState != VMSTATE_RUNNING
         && enmVMState != VMSTATE_RUNNING_LS)
        || !(pThis->virtioNetConfig.uStatus & VIRTIONET_S_LINK_UP))
        return VINF_SUCCESS;

    STAM_PROFILE_START(&pThis->StatReceive, a);
    virtioNetR3SetReadLed(&pThisCC, true);
    if (virtioNetR3AddressFilter(pThis, pvBuf, cb))
    {
        rc = virtioNetCsRxEnter(pThis, VERR_SEM_BUSY);
        if (RT_SUCCESS(rc))
        {
            rc = virtioNetR3HandleRxPacket(pDevIns, pThis, pThisCC, pvBuf, cb, pGso);
            STAM_REL_COUNTER_ADD(&pThis->StatReceiveBytes, cb);
            virtioNetCsRxLeave(pThis);
        }
    }
    virtioNetR3SetReadLed(&pThisCC, false);
    STAM_PROFILE_STOP(&pThis->StatReceive, a);
    return rc;
}


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
    *(uint16_t *)(pBuf + uStart + uOffset) = vnetR3Chksum16(pBuf + uStart, cbSize - uStart);
}

/* Read physical bytes from the out segment(s) of descriptor chain */
static void virtioNetR3PullChain(PVIRTIO_DESC_CHAIN_T pDecChain, void *pv, uint16_t cb)
{
    uint8_t *pb = (uint8_t *)pv;
    for (size_t cb = RT_MIN(pDescChain->cbPhysSend, sizeof(VIRTIONET_PKT_HDR_T)); cb; )
    {
        size_t cbSeg = cb;
        RTGCPHYS GCPhys = virtioCoreSgBufGetNextSegment(pDescChain->pSgPhysSend, &cbSeg);
        PDMDevHlpPCIPhysRead(pDevIns, GCPhys, pb, cbSeg);
        pb += cbSeg;
        cb -= cbSeg;
    }
}
static bool virtioNetR3ReadHeader(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, PVIRTIONET_PKT_HDR_T pPktHdr, uint32_t cbMax)
{
    int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhys, pPktHdr, sizeof(*pPktHdr));
    if (RT_FAILURE(rc))
        return false;

    Log4(("virtio-net: header flags=%x gso-type=%x len=%x gso-size=%x Chksum-start=%x Chksum-offset=%x cb=%x\n",
          pPktHdr->uFlags, pPktHdr->uGsoType, pPktHdr->uHdrLen, pPktHdr->uGSOSize, pPktHdr->uChksumStart, pPktHdr->uChksumOffset, cbMax));

    if (pPktHdr->uGsoType)
    {
        uint32_t uMinHdrSize;

        /* Segmentation offloading cannot be done without checksumming, and we do not support ECN */
        if (  RT_UNLIKELY(!(pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM))
            | RT_UNLIKELY(pPktHdr->uGsoType & VIRTIONET_HDR_GSO_ECN)))
                return false;

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
                return false;
        }
        /* Header + MSS must not exceed the packet size. */
        if (RT_UNLIKELY(uMinHdrSize + pPktHdr->uChksumStart + pPktHdr->uGSOSize > cbMax))
            return false;
    }
    /* Checksum must fit into the frame (validating both checksum fields). */
    if ((   pPktHdr->uFlags & VIRTIONET_HDR_F_NEEDS_CSUM)
         && sizeof(uint16_t) + pPktHdr->uChksumStart + pPktHdr->uChksumOffset > cbMax)
               return false;
    Log4func(("returning true\n"));
    return true;
}

static uint8_t virtioNetR3CtrlRx(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                 PVIRTIONET_PKT_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
#define LOG_VIRTIONET_FLAG(field) LogFunc(("%s = %d\n", #field, pThis->field));

    LogFunc((""));
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
        AssertMsgReturn(fFeatures & VIRTIONET_F_CTRL_RX_EXTRA,
                        ("CTRL "extra" cmd w/o VIRTIONET_F_CTRL_RX_EXTRA feature negotiated - skipping\n"),
                        VIRTIONET_ERROR);
        /* fall out */
    }

    uint8_t fOn, fPromiscChanged = false;
    virtioNetR3PullChain(pDescChain, &fOn, RT_MIN(pDescChain->cbPhysSend, sizeof(fOn)));

    switch(pCtrlPktHdr->uCmd)
    {
      case VIRTIONET_CTRL_RX_PROMISC:
        pThis->fPromiscuous = !!fOn;
        fPromiscChanged = true;
        LOG_VIRTIONET_FLAG(fPromiscuous)
        break;
      case VIRTIONET_CTRL_RX_ALLMULTI:
        pThis->fAllMulticast = !!fOn;
        fPromiscChanged = true;
        LOG_VIRTIONET_FLAG(fAllMulticast);
        break;
      case VIRTIONET_CTRL_RX_ALLUNI:
        pThis->fAllUnicast = !!fOn;
        LOG_VIRTIONET_FLAG(fAllUnicast);
        break;
      case VIRTIONET_CTRL_RX_NOMULTI:
        pThis->fNoMulticast = !!fOn;
        LOG_VIRTIONET_FLAG(fNoMulticast);
        break;
      case VIRTIONET_CTRL_RX_NOUNI:
        pThis->fNoUnicast = !!fOn;
        LOG_VIRTIONET_FLAG(fNoUnicast);
        break;
      case VIRTIONET_CTRL_RX_NOBCAST:
        pThis->fNoBroadcast = !!fOn;
        LOG_VIRTIONET_FLAG(fNoBroadcast);
        break;
    }

    if (pThisCC->pDrv && fPromiscChanged)
    {
        uint8_t fPromiscuous = pThis->fPromiscuous | pThis->fAllMulticast
        LogFunc(("Setting promiscuous state to %d\n", fPromiscuous));
        pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, fPromiscuous);
    }

    return VIRTIONET_OK;
}

static uint8_t virtioNetR3CtrlMac(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                  PVIRTIONET_PKT_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
#define ASSERT_CTRL_ADDR_SET(v) \
    AssertMsgReturn((v), ("DESC chain too small to process CTRL_MAC_ADDR_SET cmd"), VIRTIONET_ERROR);

#define ASSERT_CTRL_TABLE_SET(v) \
    AssertMsgReturn((v), ("DESC chain too small to process CTRL_MAC_TABLE_SET cmd"), VIRTIONET_ERROR);

    int cbRemaining = pDescChain.cbPhysSend - sizeof(*pCtrlPktHdr);

    switch(pCtrlPktHder->uCmd)
    {
        case VIRTIONET_CTRL_MAC_ADDR_SET:
        {
            /* Set default Rx filter MAC */
            ASSERT_CTRL_ADDR_SET(cbRemaining >= sizeof(VIRTIONET_CTRL_MAC_TABLE_LEN));
            virtioNetR3PullChain(pDescChain, &pThis->rxFilterMacDefault, sizeof(VIRTIONET_CTRL_MAC_TABLE_LEN));
            break;
        }
        case VIRTIONET_CTRL_MAC_TABLE_SET:
        {
            VIRTIONET_CTRL_MAC_TABLE_LEN cMacs;

            /* Load unicast MAC filter table */
            ASSERT_CTRL_TABLE_SET(cbRemaining >= sizeof(cMacs));
            virtioNetR3PullChain(pDescChain, &cMacs, sizeof(cMacs));
            cbRemaining -= sizeof(cMacs);
            uint32_t cbMacs = cMacs * sizeof(RTMAC);
            ASSERT_CTRL_TABLE_SET(cbRemaining >= cbMacs);
            virtioNetR3PullChain(pDescChain, &pThis->aMacUnicastFilter, cbMacs);
            cbRemaining -= cbMacs;
            pThis->cUnicastFilterMacs = cMacs;

            /* Load multicast MAC filter table */
            ASSERT_CTRL_TABLE_SET(cbRemaining >= sizeof(cMacs));
            virtioNetR3PullChain(pDescChain, &cMacs, sizeof(cMacs));
            cbRemaining -= sizeof(cMacs);
            cbMacs = cMacs * sizeof(RTMAC);
            ASSERT_CTRL_TABLE_SET(cbRemaining >= cbMacs);
            virtioNetR3PullChain(pDescChain, &pThis->aMacMulticastFilter, cbMacs);
            cbRemaining -= cbMacs;
            pThis->cMulticastFilterMacs = cMacs;

#ifdef LOG_ENABLED
            LogFunc(("%s: unicast MACs:\n", pThis->szInstanceName)));
            for(unsigned i = 0; i < nMacs; i++)
                LogFunc(("         %RTmac\n", &pThis->aMacUnicastFilter[i]));

            LogFunc(("%s: multicast MACs:\n", pThis->szInstanceName)));
            for(unsigned i = 0; i < nMacs; i++)
                LogFunc(("         %RTmac\n", &pThis->aMacUnicastFilter[i]));
#endif

        }
    }
    return VIRTIONET_OK;
}

static uint8_t virtioNetR3CtrlVlan(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                   PVIRTIONET_PKT_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    uint16_t uVlanId;
    int cbRemaining = pDescChain.cbPhysSend - sizeof(*pCtrlPktHdr);
    AssertMsgReturn(cbRemaining > sizeof(uVlanId),
        ("DESC chain too small for VIRTIO_NET_CTRL_VLAN cmd processing"), VIRTIONET_ERROR);
    virtioNetR3PullChain(pDescChain, &uVlanId, sizeof(uVlanId));
    AssertMsgReturn(uVlanId > VIRTIONET_MAX_VLAN_ID,
        ("%s VLAN ID out of range (VLAN ID=%u)\n", pThis->szInstanceName, uVlanId), VIRTIONET_ERROR);
    LogFunc(("%s: uCommand=%u VLAN ID=%u\n", pThis->szInstanceName, pCtrlPktHdr->uCmd, uVlanId));
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
    PVIRTIONET      pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC    pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);


    if (pDescChain->cbPhysSend < 2)
    {
        LogFunc(("ctrl packet from guest driver incomplete. Skipping ctrl cmd\n"));
        return;
    }
    else if (pDescChain->cbPhysReturn < sizeof(VIRTIONET_CTRL_HDR_T_ACK))
    {
        LogFunc(("Guest driver didn't allocate memory to receive ctrl pkt ACK. Skipping ctrl cmd\n"));
        return;
    }

    /*
     * Allocate buffer and read in the control command
     */
    PVIRTIONET_PKT_HDR_T pCtrlPktHdr = (PVIRTIONET_PKT_HDR_T)RTMemAllocZ(sizeof(VIRTIONET_PKT_HDR_T));
    AssertPtrReturn(pCtrlPktHdr, VERR_NO_MEMORY /*ignored*/);

    AssertMsgReturnVoid(pDescChain >= sizeof(*pCtrlPktHdr), ("DESC chain too small for CTRL pkt header"));
    virtioNetR3PullChain(pDescChain, pCtrlPktHdr, SIZEOF_SEND(pDescChain, VIRTIONET_PKT_HDR_T));

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
        default:
            uAck = VIRTIONET_ERROR;
    }

    PRTSGSEG paReqSegs = (PRTSGSEG)RTMemAllocZ(sizeof(RTSGSEG) * 2);
    AssertReturn(paReqSegs, VERR_NO_MEMORY);

    RTSGSEG aSegs[] = { { &uAck, sizeof(uAck) } };
    memcpy(paReqSegs, aSegs, sizeof(aSegs));

    PRTSGBUF pReqSegBuf = (PRTSGBUF)RTMemAllocZ(sizeof(RTSGBUF));
    AssertReturn(pReqSegBuf, VERR_NO_MEMORY);

    /* Copy segment data to malloc'd memory to avoid stack out-of-scope errors sanitizer doesn't detect */
    for (int i = 0; i < cSegs; i++)
    {
        void *pv = paReqSegs[i].pvSeg;
        paReqSegs[i].pvSeg = RTMemAlloc(paReqSegs[i].cbSeg);
        AssertReturn(paReqSegs[i].pvSeg, VERR_NO_MEMORY);
        memcpy(paReqSegs[i].pvSeg, pv, paReqSegs[i].cbSeg);
    }

    RTSgBufInit(pReqSegBuf, paReqSegs, cSegs);

    virtioCoreR3QueuePut(pDevIns, &pThis->Virtio, qIdx, pReqSegBuf, pDescChain, true);
    virtioCoreQueueSync(pDevIns, &pThis->Virtio, qIdx);

    for (int i = 0; i < cSegs; i++)
        RTMemFree(paReqSegs[i].pvSeg);

    RTMemFree(paReqSegs);
    RTMemFree(pReqSegBuf);

    LogFunc(("Processed ctrl message class/cmd/subcmd = %u/%u/%u. Ack=%u.\n",
    pCtrlPktHdr.uClass, pCtrlPktHdr.uCmd, pCtrlPktHdr.uCmdSpecific, uAck);

}


static void virtioNetR3Transmit(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC, uint16_t qIdx, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    RT_NOREF5(pDevIns, pThis, pThisCC, qIdx, pDescChain);
}
 static void virtioNetR3Receive(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC, uint16_t qIdx, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    RT_NOREF5(pDevIns, pThis, pThisCC, qIdx, pDescChain);
}

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDEV}
 */
static DECLCALLBACK(int) virtioNetR3WorkerWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    return PDMDevHlpSUPSemEventSignal(pDevIns, pThis->aWorkers[(uintptr_t)pThread->pvUser].hEvtProcess);
}

/**
 * @callback_method_impl{FNPDMTHREADDEV}
 */
static DECLCALLBACK(int) virtioNetR3WorkerThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    uint16_t const     qIdx      = (uint16_t)(uintptr_t)pThread->pvUser;
    PVIRTIONET         pThis     = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC       pThisCC   = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    PVIRTIONETWORKER   pWorker   = &pThis->aWorkers[qIdx];
    PVIRTIONETWORKERR3 pWorkerR3 = &pThisCC->aWorkers[qIdx];

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (virtioCoreQueueIsEmpty(pDevIns, &pThis->Virtio, qIdx))
        {
            /* Atomic interlocks avoid missing alarm while going to sleep & notifier waking the awoken */
            ASMAtomicWriteBool(&pWorkerR3->fSleeping, true);
            bool fNotificationSent = ASMAtomicXchgBool(&pWorkerR3->fNotified, false);
            if (!fNotificationSent)
            {
                Log6Func(("%s worker sleeping...\n", VIRTQNAME(qIdx)));
                Assert(ASMAtomicReadBool(&pWorkerR3->fSleeping));
                int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    return VINF_SUCCESS;
                if (rc == VERR_INTERRUPTED)
                    continue;
                Log6Func(("%s worker woken\n", VIRTQNAME(qIdx)));
                ASMAtomicWriteBool(&pWorkerR3->fNotified, false);
            }
            ASMAtomicWriteBool(&pWorkerR3->fSleeping, false);
        }

        if (!pThis->afQueueAttached[qIdx])
        {
            LogFunc(("%s queue not attached, worker aborting...\n", VIRTQNAME(qIdx)));
            break;
        }
        if (!pThisCC->fQuiescing)
        {
             Log6Func(("fetching next descriptor chain from %s\n", VIRTQNAME(qIdx)));
             PVIRTIO_DESC_CHAIN_T pDescChain;
             int rc = virtioCoreR3QueueGet(pDevIns, &pThis->Virtio, qIdx, &pDescChain, true);
             if (rc == VERR_NOT_AVAILABLE)
             {
                Log6Func(("Nothing found in %s\n", VIRTQNAME(qIdx)));
                continue;
             }

             AssertRC(rc);
             if (qIdx == CTRLQIDX)
                 virtioNetR3Ctrl(pDevIns, pThis, pThisCC, pDescChain);
             else if (qIdx & 1)
                 virtioNetR3Transmit(pDevIns, pThis, pThisCC, qIdx, pDescChain);
             else
                 virtioNetR3Receive(pDevIns, pThis, pThisCC, qIdx, pDescChain);
        }
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/** Returns true if large packets are written into several RX buffers. */
DECLINLINE(bool) virtioNetR3MergeableRxBuffers(PVIRTIONET pThis)
{
    return !!(pThis->fFeatures & VIRTIONET_F_MRG_RXBUF);
}

DECLINLINE(int) virtioNetR3CsEnter(PPDMDEVINS pDevIns, PVIRTIONET pThis, int rcBusy)
{
    /* Original DevVirtioNet uses CS in attach/detach/link-up timer/tx timer/transmit */
    LogFunc("CS unimplemented. What does the critical section protect in orig driver??"));
}

DECLINLINE(void) virtioNetR3CsLeave(PPDMDEVINS pDevIns, PVIRTIONET pThis)
{
    LogFunc("CS unimplemented. What does the critical section protect in orig driver??"));
}

/**
 * @callback_method_impl{VIRTIOCORER3,pfnStatusChanged}
 */
static DECLCALLBACK(void) virtioNetR3StatusChanged(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint32_t fVirtioReady)
{
    PVIRTIONET     pThis     = RT_FROM_MEMBER(pVirtio,  VIRTIONET, Virtio);
    PVIRTIONETCC   pThisCC   = RT_FROM_MEMBER(pVirtioCC, VIRTIONETCC, Virtio);

    LogFunc((""));

    pThis->fVirtioReady = fVirtioReady;

    if (fVirtioReady)
    {
        LogFunc(("VirtIO ready\n-----------------------------------------------------------------------------------------\n"));
        uint64_t fFeatures   = virtioCoreGetNegotiatedFeatures(&pThis->Virtio);
        pThis->fResetting    = false;
        pThisCC->fQuiescing  = false;

        for (unsigned i = 0; i < VIRTIONET_MAX_QUEUES; i++)
            pThis->afQueueAttached[i] = true;
    }
    else
    {
        LogFunc(("VirtIO is resetting\n"));

        pThis->virtioNetConfig.status = pThis->fCableConnected ? VIRTIONET_S_LINK_UP : 0;
        LogFunc(("%s Link is %s\n", pThis->szInstanceName, pThis->fCableConnected ? "up" : "down"));

        pThis->fPromiscuous  = true;
        pThis->fAllMulticast = false;
        pThis->fAllUnicast   = false;
        pThis->fNoMulticat   = false;
        pThis->fNoUnicast    = false;
        pThis->fNoBroadcast  = false;
        pThis->uIsTransmitting      = 0;
        pThis->cUnicastFilterMacs   = 0;
        pThis->cMulticastFilterMacs = 0;

        memset(pThis->aMacMulticastFilter,  0, sizeof(pThis->aMacMulticastFilter));
        memset(pThis->aMacUnicastFilter,    0, sizeof(pThis->aMacUnicastFilter));
        memset(pThis->aVlanFilter,          0, sizeof(pThis->aVlanFilter));

        pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, true);

        for (unsigned i = 0; i < VIRTIONET_MAX_QUEUES; i++)
            pThis->afQueueAttached[i] = false;
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
    PVIRTIONET       pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC     pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    LogFunc((""));
    AssertLogRelReturnVoid(iLUN == 0);

    int rc = virtioNetR3CsEnter(pDevIns, pThis, VERR_SEM_BUSY);
    AssertMsgRCReturn(rc, ("Failed to enter critical section"), rc);

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
    LogFunc(("%s",  INSTANCE(pThis)));

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
 * Turns on/off the write status LED.
 *
 * @returns VBox status code.
 * @param   pThis          Pointer to the device state structure.
 * @param   fOn            New LED state.
 */
void virtioNetR3SetWriteLed(PVIRTIONETR3 pThisR3, bool fOn)
{
    Log6Func(("%s\n", fOn ? "on" : "off"));
    if (fOn)
        pThisR3->led.Asserted.s.fWriting = pThisR3->led.Actual.s.fWriting = 1;
    else
        pThisR3->led.Actual.s.fWriting = fOn;
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
    Log6Func(("%s\n", fOn ? "on" : "off"));
    if (fOn)
        pThisR3->led.Asserted.s.fReading = pThisR3->led.Actual.s.fReading = 1;
    else
        pThisR3->led.Actual.s.fReading = fOn;
}
/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface,
 */
static DECLCALLBACK(void *) virtioNetR3QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PVIRTIONETR3 pThisR3 = RT_FROM_MEMBER(pInterface, VIRTIONETR3, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKDOWN,   &pThisR3->INetworkDown);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThisR3->INetworkConfig);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,          &pThisR3->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,      &pThisR3->ILeds);
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


    for (unsigned qIdx = 0; qIdx < pThis->cVirtQueues; qIdx++)
    {
        PVIRTIONETWORKER pWorker = &pThis->aWorkers[qIdx];
        if (pWorker->hEvtProcess != NIL_SUPSEMEVENT)
        {
            PDMDevHlpSUPSemEventClose(pDevIns, pWorker->hEvtProcess);
            pWorker->hEvtProcess = NIL_SUPSEMEVENT;
        }
    }

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
    LogFunc(("PDM device instance: %d\n", iInstance));
    RTStrPrintf(pThis->szInstanceName, sizeof(pThis->szInstanceName), "VIRTIONET%d", iInstance);
    pThisCC->pDevIns     = pDevIns;

    pThisCC->IBase.pfnQueryInterface = virtioNetR3QueryInterface;
    pThisCC->ILeds.pfnQueryStatusLed = virtioNetR3QueryStatusLed;
    pThisCC->led.uMagic = PDMLED_MAGIC;

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "MAC|CableConnected|LineSpeed|LinkUpDelay|StatNo", "");

    /* Get config params */
    int rc = pHlp->pfnCFGMQueryBytes(pCfg, "MAC", pThis->macConfigured.au, sizeof(pThis->macConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get MAC address"));

    rc = pHlp->pfnCFGMQueryBool(pCfg, "CableConnected", &pThis->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'CableConnected'"));

    uint32_t uStatNo = iInstance;
    rc = pHlp->pfnCFGMQueryuDef(pCfg, "StatNo", &uStatNo, iInstance);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"StatNo\" value"));

    rc = pHlp->pfnCFGMQueryuDef(pCfg, "LinkUpDelay", &pThis->cMsLinkUpDelay, 5000); /* ms */
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the value of 'LinkUpDelay'"));

    Assert(pThis->cMsLinkUpDelay <= 300000); /* less than 5 minutes */

    if (pThis->cMsLinkUpDelay > 5000 || pThis->cMsLinkUpDelay < 100)
        LogRel(("%s WARNING! Link up delay is set to %u seconds!\n",
                pThis->szInstanceName, pThis->cMsLinkUpDelay / 1000));

    Log(("%s Link up delay is set to %u seconds\n", pThis->szInstanceName, pThis->cMsLinkUpDelay / 1000));

    /* Copy the MAC address configured for the VM to the MMIO accessible Virtio dev-specific config area */
    memcpy(pThis->virtioNetConfig.uMacAddress, pThis->macConfigured.au, sizeof(pThis->virtioNetConfig.uMacAddress)); /* TBD */

    /*
     * Do core virtio initialization.
     */

#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_STATUS
    pThis->virtioNetConfig.uStatus = 0;
#endif

#if VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_MQ
    pThis->virtioNetConfig.uMaxVirtqPairs = VIRTIONET_MAX_QPAIRS;
#endif

    /* Initialize the generic Virtio core: */
    pThisCC->Virtio.pfnStatusChanged        = virtioNetR3StatusChanged;
    pThisCC->Virtio.pfnQueueNotified        = virtioNetR3Notified;

    VIRTIOPCIPARAMS VirtioPciParams;
    VirtioPciParams.uDeviceId               = PCI_DEVICE_ID_VIRTIONET_HOST;
    VirtioPciParams.uClassBase              = PCI_CLASS_BASE_NETWORK_CONTROLLER;
    VirtioPciParams.uClassSub               = PCI_CLASS_SUB_NET_ETHERNET_CONTROLLER;
    VirtioPciParams.uClassProg              = PCI_CLASS_PROG_UNSPECIFIED;
    VirtioPciParams.uSubsystemId            = PCI_DEVICE_ID_VIRTIONET_HOST;  /* VirtIO 1.0 spec allows PCI Device ID here */
    VirtioPciParams.uInterruptLine          = 0x00;
    VirtioPciParams.uInterruptPin           = 0x01;

    rc = virtioCoreR3Init(pDevIns, &pThis->Virtio, &pThisCC->Virtio, &VirtioPciParams, pThis->szInstanceName,
                          VIRTIONET_HOST_FEATURES_OFFERED,
                          &pThis->virtioNetConfig /*pvDevSpecificCap*/, sizeof(pThis->virtioNetConfig));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: failed to initialize VirtIO"));

    pThis->fNegotiatedFeatures = virtioCoreGetNegotiatedFeatures(&pThis->Virtio);
    if (!virtioNetValidateRequiredFeatures(pThis->fNegotiatedFeatures))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: Required features not successfully negotiated."));

    pThis->cVirtqPairs =   pThis->fNegotiatedFeatures & VIRTIONET_F_MQ
                         ? pThis->virtioNetConfig.uMaxVirtqPairs : 1;
    pThis->cVirtQueues = pThis->cVirtqPairs + 1;

    /*
     * Initialize queues.
     */
    virtioNetR3SetVirtqNames(pThis);

    /* Attach the queues and create worker threads for them: */
    for (uint16_t qIdx = 0; qIdx < pThis->cVirtQueues; qIdx++)
    {
        rc = virtioCoreR3QueueAttach(&pThis->Virtio, qIdx, VIRTQNAME(qIdx));
        if (RT_FAILURE(rc))
            continue;
        rc = PDMDevHlpThreadCreate(pDevIns, &pThisCC->aWorkers[qIdx].pThread,
                                   (void *)(uintptr_t)qIdx, virtioNetR3WorkerThread,
                                   virtioNetR3WorkerWakeUp, 0, RTTHREADTYPE_IO, VIRTQNAME(qIdx));
        if (rc != VINF_SUCCESS)
        {
            LogRel(("Error creating thread for Virtual Queue %s: %Rrc\n", VIRTQNAME(qIdx), rc));
            return rc;
        }

        rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->aWorkers[qIdx].hEvtProcess);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("DevVirtioNET: Failed to create SUP event semaphore"));
        pThis->afQueueAttached[qIdx] = true;
    }

    /*
     * Status driver (optional).
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
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE //| PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE
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

