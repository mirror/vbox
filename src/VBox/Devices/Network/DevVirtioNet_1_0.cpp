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
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include <VBox/sup.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/msi.h>
#include <VBox/version.h>
#include <VBox/log.h>

#ifdef IN_RING3
# include <VBox/VBoxPktDmp.h>
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/semaphore.h>
# include <iprt/sg.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "../VirtIO/Virtio_1_0.h"

#include "VBoxDD.h"

#define VIRTIONET_SAVED_STATE_VERSION          UINT32_C(1)
#define VIRTIONET_MAX_QPAIRS                   1
#define VIRTIONET_MAX_VIRTQS                   (VIRTIONET_MAX_QPAIRS * 2 + 1)
#define VIRTIONET_MAX_FRAME_SIZE               65535 + 18     /**< Max IP pkt size + Ethernet header with VLAN tag  */
#define VIRTIONET_MAC_FILTER_LEN               32
#define VIRTIONET_MAX_VLAN_ID                  (1 << 12)
#define VIRTIONET_PREALLOCATE_RX_SEG_COUNT     32

#define VIRTQNAME(idxVirtq)       (pThis->aVirtqs[idxVirtq]->szName)
#define CBVIRTQNAME(idxVirtq)     RTStrNLen(VIRTQNAME(idxVirtq), sizeof(VIRTQNAME(idxVirtq)))
#define FEATURE_ENABLED(feature)  RT_BOOL(pThis->fNegotiatedFeatures & VIRTIONET_F_##feature)
#define FEATURE_DISABLED(feature) (!FEATURE_ENABLED(feature))
#define FEATURE_OFFERED(feature)  VIRTIONET_HOST_FEATURES_OFFERED & VIRTIONET_F_##feature

#define IS_VIRTQ_EMPTY(pDevIns, pVirtio, idxVirtq) \
            (virtioCoreVirtqAvailCount(pDevIns, pVirtio, idxVirtq) == 0)

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
#define IS_TX_VIRTQ(n)    ((n) != CTRLQIDX && ((n) & 1))
#define IS_RX_VIRTQ(n)    ((n) != CTRLQIDX && !IS_TX_VIRTQ(n))
#define IS_CTRL_VIRTQ(n)  ((n) == CTRLQIDX)
#define RXQIDX(qPairIdx)  (qPairIdx * 2)
#define TXQIDX(qPairIdx)  (qPairIdx * 2 + 1)
#define CTRLQIDX          (FEATURE_ENABLED(MQ) ? ((VIRTIONET_MAX_QPAIRS - 1) * 2 + 2) : 2)

#define LUN0    0

#ifdef USING_CRITICAL_SECTION
#  define ENTER_CRITICAL_SECTION \
        do { \
            int rc = virtioNetR3CsEnter(pDevIns, pThis, VERR_SEM_BUSY); \
            AssertRCReturnVoid(rc); \
            RT_NOREF(rc);
        } while(0)
#  define LEAVE_CRITICAL_SECTION \
        do { \
            virtioNetR3CsLeave(pDevIns, pThis); \
        } while(0)
#else
#   define ENTER_CRITICAL_SECTION do { } while(0)
#   define LEAVE_CRITICAL_SECTION do { } while(0)
#endif

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
    | VIRTIONET_F_MRG_RXBUF

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
 * device-specific queue info
 */
struct VIRTIONETWORKER;
struct VIRTIONETWORKERR3;

typedef struct VIRTIONETVIRTQ
{
    struct VIRTIONETWORKER         *pWorker;                    /**< Pointer to R0 worker struct                      */
    struct VIRTIONETWORKERR3       *pWorkerR3;                  /**< Pointer to R3 worker struct                      */
    uint16_t                       idx;                         /**< Index of this queue                              */
    uint16_t                       align;
    char                           szName[VIRTIO_MAX_VIRTQ_NAME_SIZE]; /**< Virtq name                                */
    bool                           fCtlVirtq;                   /**< If set this queue is the control queue           */
    bool                           fHasWorker;                  /**< If set this queue has an associated worker       */
    bool                           fAttachedToVirtioCore;       /**< Set if queue attached to virtio core             */
    uint8_t                        pad;
} VIRTIONETVIRTQ, *PVIRTIONETVIRTQ;

/**
 * Worker thread context, shared state.
 */
typedef struct VIRTIONETWORKER
{
    SUPSEMEVENT                     hEvtProcess;                /**< handle of associated sleep/wake-up semaphore      */
    PVIRTIONETVIRTQ                 pVirtq;                     /**< pointer to queue                                  */
    uint16_t                        idx;                        /**< Index of this worker                              */
    bool volatile                   fSleeping;                  /**< Flags whether worker thread is sleeping or not    */
    bool volatile                   fNotified;                  /**< Flags whether worker thread notified              */
    bool                            fAssigned;                  /**< Flags whether worker thread has been set up       */
    uint8_t                         pad;
} VIRTIONETWORKER;
/** Pointer to a virtio net worker. */
typedef VIRTIONETWORKER *PVIRTIONETWORKER;

/**
 * Worker thread context, ring-3 state.
 */
typedef struct VIRTIONETWORKERR3
{
    R3PTRTYPE(PPDMTHREAD)           pThread;                    /**< pointer to worker thread's handle                 */
    PVIRTIONETVIRTQ                 pVirtq;                     /**< pointer to queue                                  */
    uint16_t                        idx;                        /**< Index of this worker                              */
    uint16_t                        pad;
} VIRTIONETWORKERR3;
/** Pointer to a virtio net worker. */
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
    VIRTIONETWORKER         aWorkers[VIRTIONET_MAX_VIRTQS];

    /** Track which VirtIO queues we've attached to */
    VIRTIONETVIRTQ          aVirtqs[VIRTIONET_MAX_VIRTQS];

    /** PDM device Instance name */
    char                    szInst[16];

    /** VirtIO features negotiated with the guest, including generic core and device specific */
    uint64_t                fNegotiatedFeatures;

    /** Number of Rx/Tx queue pairs (only one if MQ feature not negotiated */
    uint16_t                cVirtqPairs;

    /** Number of virtqueues total (which includes each queue of each pair plus one control queue */
    uint16_t                cVirtVirtqs;

    /** Number of worker threads (one for the control queue and one for each Tx queue) */
    uint16_t                cWorkers;

    /** Alighnment */
    uint16_t                alignment;

    /** Indicates transmission in progress -- only one thread is allowed. */
    uint32_t                uIsTransmitting;

    /** virtio-net-1-dot-0 (in milliseconds). */
    uint32_t                cMsLinkUpDelay;

    /** The number of actually used slots in aMacMulticastFilter. */
    uint32_t                cMulticastFilterMacs;

    /** The number of actually used slots in aMacUniicastFilter. */
    uint32_t                cUnicastFilterMacs;

    /** Semaphore leaf device's thread waits on until guest driver sends empty Rx bufs */
    SUPSEMEVENT             hEventRxDescAvail;

    /** Array of MAC multicast addresses accepted by RX filter. */
    RTMAC                   aMacMulticastFilter[VIRTIONET_MAC_FILTER_LEN];

    /** Array of MAC unicast addresses accepted by RX filter. */
    RTMAC                   aMacUnicastFilter[VIRTIONET_MAC_FILTER_LEN];

    /** Default MAC address which rx filtering accepts */
    RTMAC                   rxFilterMacDefault;

    /** MAC address obtained from the configuration. */
    RTMAC                   macConfigured;

    /** Bit array of VLAN filter, one bit per VLAN ID. */
    uint8_t                 aVlanFilter[VIRTIONET_MAX_VLAN_ID / sizeof(uint8_t)];

    /** Set if PDM leaf device at the network interface is starved for Rx buffers */
    bool volatile           fLeafWantsEmptyRxBufs;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t                uPktNo;

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

    /** True if physical cable is attached in configuration. */
    bool                    fCableConnected;

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
    VIRTIONETWORKERR3               aWorkers[VIRTIONET_MAX_VIRTQS];

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

    /** Link Up(/Restore) Timer. */
    TMTIMERHANDLE                   hLinkUpTimer;

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

#ifdef IN_RING3
static DECLCALLBACK(int) virtioNetR3WorkerThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread);

DECLINLINE(const char *) virtioNetThreadStateName(PPDMTHREAD pThread)
{
    if (!pThread)
        return "<null>";

    switch(pThread->enmState)
    {
        case PDMTHREADSTATE_INVALID:
            return "invalid state";
        case PDMTHREADSTATE_INITIALIZING:
            return "initializing";
        case PDMTHREADSTATE_SUSPENDING:
            return "suspending";
        case PDMTHREADSTATE_SUSPENDED:
            return "suspended";
        case PDMTHREADSTATE_RESUMING:
            return "resuming";
        case PDMTHREADSTATE_RUNNING:
            return "running";
        case PDMTHREADSTATE_TERMINATING:
            return "terminating";
        case PDMTHREADSTATE_TERMINATED:
            return "terminated";
        default:
            return "unknown state";
    }
}
#endif

/**
 * Wakeup the RX thread.
 */
static void virtioNetWakeupRxBufWaiter(PPDMDEVINS pDevIns)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    AssertReturnVoid(pThis->hEventRxDescAvail != NIL_SUPSEMEVENT);

    STAM_COUNTER_INC(&pThis->StatRxOverflowWakeup);
    if (pThis->hEventRxDescAvail != NIL_SUPSEMEVENT)
    {
        Log10Func(("%s Waking downstream device's Rx buf waiter thread\n", pThis->szInst));
        int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->hEventRxDescAvail);
        AssertRC(rc);
    }
}

/**
 * @callback_method_impl{VIRTIOCORER0,pfnVirtqNotified}
 */
static DECLCALLBACK(void) virtioNetVirtqNotified(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxVirtq)
{
    RT_NOREF(pVirtio);
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    PVIRTIONETVIRTQ  pVirtq = &pThis->aVirtqs[idxVirtq];
    PVIRTIONETWORKER pWorker = pVirtq->pWorker;

#if defined (IN_RING3) && defined (LOG_ENABLED)
     RTLogFlush(NULL);
#endif

    if (IS_RX_VIRTQ(idxVirtq))
    {
        uint16_t cBufsAvailable = virtioCoreVirtqAvailCount(pDevIns, pVirtio, idxVirtq);

        if (cBufsAvailable)
        {
            Log10Func(("%s %u empty bufs added to %s by guest (notifying leaf device)\n",
                        pThis->szInst, cBufsAvailable, pVirtq->szName));
            virtioNetWakeupRxBufWaiter(pDevIns);
        }
        else
            LogRel(("%s \n\n***WARNING: %s notified but no empty bufs added by guest! (skip notifying of leaf device)\n\n",
                    pThis->szInst, pVirtq->szName));
    }
    else if (IS_TX_VIRTQ(idxVirtq) || IS_CTRL_VIRTQ(idxVirtq))
    {
        /* Wake queue's worker thread up if sleeping (e.g. a Tx queue, or the control queue */
        if (!ASMAtomicXchgBool(&pWorker->fNotified, true))
        {
            if (ASMAtomicReadBool(&pWorker->fSleeping))
            {
                Log10Func(("%s %s has available buffers - waking worker.\n", pThis->szInst, pVirtq->szName));
                int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pWorker->hEvtProcess);
                AssertRC(rc);
            }
            else
            {
                Log10Func(("%s %s has available buffers - worker already awake\n", pThis->szInst, pVirtq->szName));
            }
        }
        else
        {
            Log10Func(("%s %s has available buffers - waking worker.\n", pThis->szInst, pVirtq->szName));
        }
    }
    else
        LogRelFunc(("%s unrecognized queue %s (idx=%d) notified\n", pVirtq->szName, idxVirtq));
}

#ifdef IN_RING3 /* spans most of the file, at the moment. */

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDEV}
 */
static DECLCALLBACK(int) virtioNetR3WakeupWorker(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVIRTIONET       pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETWORKER pWorker = (PVIRTIONETWORKER)pThread->pvUser;

    Log10Func(("%s\n", pThis->szInst));
    RT_NOREF(pThis);

    return PDMDevHlpSUPSemEventSignal(pDevIns, pWorker->hEvtProcess);
}


DECLINLINE(void) virtioNetR3SetVirtqNames(PVIRTIONET pThis)
{
    RTStrCopy(pThis->aVirtqs[CTRLQIDX].szName, VIRTIO_MAX_VIRTQ_NAME_SIZE, "controlq");
    for (uint16_t qPairIdx = 0; qPairIdx < pThis->cVirtqPairs; qPairIdx++)
    {
        RTStrPrintf(pThis->aVirtqs[RXQIDX(qPairIdx)].szName, VIRTIO_MAX_VIRTQ_NAME_SIZE, "receiveq<%d>",  qPairIdx);
        RTStrPrintf(pThis->aVirtqs[TXQIDX(qPairIdx)].szName, VIRTIO_MAX_VIRTQ_NAME_SIZE, "transmitq<%d>", qPairIdx);
    }
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
    if (!LogIs12Enabled())
        return;

    vboxEthPacketDump(pThis->szInst, pszText, pbPacket, (uint32_t)cb);
}

DECLINLINE(void) virtioNetPrintFeatures(VIRTIONET *pThis, PCDBGFINFOHLP pHlp)
{
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
        uint64_t isOffered = fFeaturesOfferedMask & s_aFeatures[i].fFeatureBit;
        uint64_t isNegotiated = pThis->fNegotiatedFeatures & s_aFeatures[i].fFeatureBit;
        cp += RTStrPrintf(cp, cbBuf - (cp - pszBuf), "        %s       %s   %s",
                          isOffered ? "+" : "-", isNegotiated ? "x" : " ", s_aFeatures[i].pcszDesc);
    }
    if (pHlp)
        pHlp->pfnPrintf(pHlp, "VirtIO Net Features Configuration\n\n"
              "    Offered  Accepted  Feature              Description\n"
              "    -------  --------  -------              -----------\n"
              "%s\n", pszBuf);
#ifdef LOG_ENABLED
    else
        Log3(("VirtIO Net Features Configuration\n\n"
              "    Offered  Accepted  Feature              Description\n"
              "    -------  --------  -------              -----------\n"
              "%s\n", pszBuf));
#endif
    RTMemFree(pszBuf);
}

#ifdef LOG_ENABLED
void virtioNetDumpGcPhysRxBuf(PPDMDEVINS pDevIns, PVIRTIONET_PKT_HDR_T pRxPktHdr,
                     uint16_t cDescs, uint8_t *pvBuf, uint16_t cb, RTGCPHYS GCPhysRxBuf, uint8_t cbRxBuf)
{
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

    virtioCoreGCPhysHexDump(pDevIns, GCPhysRxBuf, cbRxBuf, 0, "Phys Mem Dump of Rx pkt");
    LogFunc(("-------------------------------------------------------------------\n"));
}

#endif /* LOG_ENABLED */

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, virtio-net debugger info callback.}
 */
static DECLCALLBACK(void) virtioNetR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVIRTIONET     pThis   = PDMDEVINS_2_DATA(pDevIns,  PVIRTIONET);
    PVIRTIONETCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);

    bool fNone     = pszArgs && *pszArgs == '\0';
    bool fAll      = pszArgs && (*pszArgs == 'a' || *pszArgs == 'A'); /* "all"      */
    bool fNetwork  = pszArgs && (*pszArgs == 'n' || *pszArgs == 'N'); /* "network"  */
    bool fFeatures = pszArgs && (*pszArgs == 'f' || *pszArgs == 'F'); /* "features" */
    bool fState    = pszArgs && (*pszArgs == 's' || *pszArgs == 'S'); /* "state"    */
    bool fPointers = pszArgs && (*pszArgs == 'p' || *pszArgs == 'P'); /* "pointers" */
    bool fVirtqs   = pszArgs && (*pszArgs == 'q' || *pszArgs == 'Q'); /* "queues    */

    /* Show basic information. */
    pHlp->pfnPrintf(pHlp,
        "\n"
        "---------------------------------------------------------------------------\n"
        "Debug Info: %s\n"
        "        (options: [a]ll, [n]et, [f]eatures, [s]tate, [p]ointers, [q]ueues)\n"
        "---------------------------------------------------------------------------\n\n",
        pThis->szInst, pDevIns->pReg->szName);

    if (fNone)
        return;

    /* Show offered/unoffered, accepted/rejected features */
    if (fAll || fFeatures)
    {
        virtioCorePrintFeatures(&pThis->Virtio, pHlp);
        virtioNetPrintFeatures(pThis, pHlp);
        pHlp->pfnPrintf(pHlp, "\n");
    }

    /* Show queues (and associate worker info if applicable) */
    if (fAll || fVirtqs)
    {
        pHlp->pfnPrintf(pHlp, "Virtq information:\n\n");

        for (int idxVirtq = 0; idxVirtq < pThis->cVirtVirtqs; idxVirtq++)
        {
            PVIRTIONETVIRTQ pVirtq = &pThis->aVirtqs[idxVirtq];

            if (pVirtq->fHasWorker)
            {
                PVIRTIONETWORKER pWorker = pVirtq->pWorker;
                PVIRTIONETWORKERR3 pWorkerR3 = pVirtq->pWorkerR3;

                if (pWorker->fAssigned)
                {
                    pHlp->pfnPrintf(pHlp, "    %-15s (pThread: %p %s) ",
                        pVirtq->szName,
                        pWorkerR3->pThread,
                        virtioNetThreadStateName(pWorkerR3->pThread));
                    if (pVirtq->fAttachedToVirtioCore)
                    {
                        pHlp->pfnPrintf(pHlp, "worker: ");
                        pHlp->pfnPrintf(pHlp, "%s", pWorker->fSleeping ? "blocking" : "unblocked");
                        pHlp->pfnPrintf(pHlp, "%s", pWorker->fNotified ? ", notified" : "");
                    }
                    else
                    if (pWorker->fNotified)
                        pHlp->pfnPrintf(pHlp, "not attached to virtio core");
                }
            }
            else
            {
                pHlp->pfnPrintf(pHlp, "    %-15s (INetworkDown's thread) %s", pVirtq->szName,
                    pVirtq->fAttachedToVirtioCore  ? "" : "not attached to virtio core");
            }
            pHlp->pfnPrintf(pHlp, "\n");
            virtioCoreR3VirtqInfo(pDevIns, pHlp, pszArgs, idxVirtq);
            pHlp->pfnPrintf(pHlp, "    ---------------------------------------------------------------------\n");
            pHlp->pfnPrintf(pHlp, "\n");
        }
        pHlp->pfnPrintf(pHlp, "\n");
    }

    /* Show various pointers */
    if (fAll || fPointers)
    {

        pHlp->pfnPrintf(pHlp, "Internal pointers:\n\n");

        pHlp->pfnPrintf(pHlp, "    pDevIns ................... %p\n",  pDevIns);
        pHlp->pfnPrintf(pHlp, "    PVIRTIONET ................ %p\n",  pThis);
        pHlp->pfnPrintf(pHlp, "    PVIRTIONETCC .............. %p\n", pThisCC);
        pHlp->pfnPrintf(pHlp, "    pDrvBase .................. %p\n",  pThisCC->pDrvBase);
        pHlp->pfnPrintf(pHlp, "    pDrv ...................... %p\n",  pThisCC->pDrv);
        pHlp->pfnPrintf(pHlp, "    pDrv ...................... %p\n",  pThisCC->pDrv);
        pHlp->pfnPrintf(pHlp, "\n");
        pHlp->pfnPrintf(pHlp, "Misc state\n");
        pHlp->pfnPrintf(pHlp, "\n");
        pHlp->pfnPrintf(pHlp, "    fVirtioReady .............. %d\n",  pThis->fVirtioReady);
        pHlp->pfnPrintf(pHlp, "    fGenUpdatePending ......... %d\n",  pThis->Virtio.fGenUpdatePending);
        pHlp->pfnPrintf(pHlp, "    fMsiSupport ............... %d\n",  pThis->Virtio.fMsiSupport);
        pHlp->pfnPrintf(pHlp, "    uConfigGeneration ......... %d\n",  pThis->Virtio.uConfigGeneration);
        pHlp->pfnPrintf(pHlp, "    uDeviceStatus ............. 0x%x\n", pThis->Virtio.uDeviceStatus);
        pHlp->pfnPrintf(pHlp, "    cVirtqPairs .,............. %d\n",  pThis->cVirtqPairs);
        pHlp->pfnPrintf(pHlp, "    cVirtVirtqs .,............. %d\n",  pThis->cVirtVirtqs);
        pHlp->pfnPrintf(pHlp, "    cWorkers .................. %d\n",  pThis->cWorkers);
        pHlp->pfnPrintf(pHlp, "    MMIO mapping name ......... %d\n",  pThisCC->Virtio.pcszMmioName);

    }

    /* Show device state info */
    if (fAll || fState)
    {
        pHlp->pfnPrintf(pHlp, "Device state:\n\n");
        uint32_t fTransmitting = ASMAtomicReadU32(&pThis->uIsTransmitting);

        pHlp->pfnPrintf(pHlp, "    Transmitting: ............. %s\n", fTransmitting ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "    Quiescing: ................ %s %s\n",
                pThis->fQuiescing ? "true" : "false",
                pThis->fQuiescing ? "(" : "",
                pThis->fQuiescing ? virtioCoreGetStateChangeText(pThisCC->enmQuiescingFor) : "",
                pThis->fQuiescing ? ")" : "");
        pHlp->pfnPrintf(pHlp, "    Resetting: ................ %s\n", pThis->fResetting ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "\n");
    }

    /* Show network related information */
    if (fAll || fNetwork)
    {
        pHlp->pfnPrintf(pHlp, "Network configuration:\n\n");

        pHlp->pfnPrintf(pHlp, "    MAC: ...................... %RTmac\n", &pThis->macConfigured);
        pHlp->pfnPrintf(pHlp, "\n");
        pHlp->pfnPrintf(pHlp, "    Cable: .................... %s\n",      pThis->fCableConnected ? "connected" : "disconnected");
        pHlp->pfnPrintf(pHlp, "    Link-up delay: ............ %d ms\n",   pThis->cMsLinkUpDelay);
        pHlp->pfnPrintf(pHlp, "\n");
        pHlp->pfnPrintf(pHlp, "    Accept all multicast: ..... %s\n",      pThis->fAllMulticast  ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "    Suppress broadcast: ....... %s\n",      pThis->fNoBroadcast   ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "    Suppress unicast: ......... %s\n",      pThis->fNoUnicast     ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "    Suppress multicast: ....... %s\n",      pThis->fNoMulticast   ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "    Promiscuous: .............. %s\n",      pThis->fPromiscuous   ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "\n");
        pHlp->pfnPrintf(pHlp, "    Default Rx MAC filter: .... %RTmac\n", pThis->rxFilterMacDefault);
        pHlp->pfnPrintf(pHlp, "\n");

        pHlp->pfnPrintf(pHlp, "    Unicast filter MACs:\n");

        if (!pThis->cUnicastFilterMacs)
            pHlp->pfnPrintf(pHlp, "        <none>\n");

        for (uint32_t i = 0; i < pThis->cUnicastFilterMacs; i++)
            pHlp->pfnPrintf(pHlp, "        %RTmac\n", &pThis->aMacUnicastFilter[i]);

        pHlp->pfnPrintf(pHlp, "\n    Multicast filter MACs:\n");

        if (!pThis->cMulticastFilterMacs)
            pHlp->pfnPrintf(pHlp, "        <none>\n");

        for (uint32_t i = 0; i < pThis->cMulticastFilterMacs; i++)
            pHlp->pfnPrintf(pHlp, "        %RTmac\n", &pThis->aMacMulticastFilter[i]);

        pHlp->pfnPrintf(pHlp, "\n\n");
        pHlp->pfnPrintf(pHlp, "    Leaf starved: ............. %s\n",      pThis->fLeafWantsEmptyRxBufs ? "true" : "false");
        pHlp->pfnPrintf(pHlp, "\n");

    }
    pHlp->pfnPrintf(pHlp, "\n");
    virtioCoreR3Info(pDevIns, pHlp, pszArgs);
    pHlp->pfnPrintf(pHlp, "\n");
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
         || (offConfig + cb <= RT_UOFFSETOF(VIRTIONET_CONFIG_T, member) \
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
            LogFunc(("%s Guest attempted to write readonly virtio_pci_common_cfg.%s\n", pThis->szInst, #member)); \
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
        LogFunc(("%s Bad access by guest to virtio_net_config: off=%u (%#x), cb=%u\n", pThis->szInst, offConfig, offConfig, cb));
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

    LogFunc(("%s uOffset: %d, cb: %d\n",  pThis->szInst, uOffset, cb));
    RT_NOREF(pThis);
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, pv, cb, false /*fRead*/);
}

/**
 * @callback_method_impl{VIRTIOCORER3,pfnDevCapWrite}
 */
static DECLCALLBACK(int) virtioNetR3DevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, uint32_t cb)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    Log10Func(("%s uOffset: %d, cb: %d: %.*Rhxs\n", pThis->szInst, uOffset, cb, RT_MAX(cb, 8) , pv));
    RT_NOREF(pThis);
    return virtioNetR3CfgAccessed(PDMDEVINS_2_DATA(pDevIns, PVIRTIONET), uOffset, (void *)pv, cb, true /*fWrite*/);
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
    Log7Func(("%s LOAD EXEC!!\n", pThis->szInst));

    AssertReturn(uPass == SSM_PASS_FINAL, VERR_SSM_UNEXPECTED_PASS);
    AssertLogRelMsgReturn(uVersion == VIRTIONET_SAVED_STATE_VERSION,
                          ("uVersion=%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    virtioNetR3SetVirtqNames(pThis);

    pHlp->pfnSSMGetU64(     pSSM, &pThis->fNegotiatedFeatures);

    pHlp->pfnSSMGetU16(     pSSM, &pThis->cVirtVirtqs);
    pHlp->pfnSSMGetU16(     pSSM, &pThis->cWorkers);

    for (int idxVirtq = 0; idxVirtq < pThis->cVirtVirtqs; idxVirtq++)
        pHlp->pfnSSMGetBool(pSSM, &pThis->aVirtqs[idxVirtq].fAttachedToVirtioCore);

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
            pThis->szInst, &pThis->macConfigured, &macConfigured));
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
        PVIRTIONETWORKER pWorker = &pThis->aWorkers[idxWorker];
        PVIRTIONETVIRTQ  pVirtq  = pWorker->pVirtq;
        if (pVirtq->fAttachedToVirtioCore)
        {
            Log7Func(("%s Waking %s worker.\n", pThis->szInst, pVirtq->szName));
            rc = PDMDevHlpSUPSemEventSignal(pDevIns, pWorker->hEvtProcess);
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
    Log7Func(("%s SAVE EXEC!!\n", pThis->szInst));

    pHlp->pfnSSMPutU64(     pSSM, pThis->fNegotiatedFeatures);

    pHlp->pfnSSMPutU16(     pSSM, pThis->cVirtVirtqs);
    pHlp->pfnSSMPutU16(     pSSM, pThis->cWorkers);

    for (int idxVirtq = 0; idxVirtq < pThis->cVirtVirtqs; idxVirtq++)
        pHlp->pfnSSMPutBool(pSSM, pThis->aVirtqs[idxVirtq].fAttachedToVirtioCore);

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
        pThis->szInst, virtioCoreGetStateChangeText(pThisCC->enmQuiescingFor)));

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
    virtioNetWakeupRxBufWaiter(pDevIns);

    PDMDevHlpSetAsyncNotification(pDevIns, virtioNetR3DeviceQuiesced);

    /* If already quiesced invoke async callback.  */
    if (!ASMAtomicReadBool(&pThis->fLeafWantsEmptyRxBufs))
        PDMDevHlpAsyncNotificationCompleted(pDevIns);

    /** @todo make sure Rx and Tx are really quiesced (how do we synchronize w/downstream driver?) */
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnReset}
 */
static DECLCALLBACK(void) virtioNetR3Reset(PPDMDEVINS pDevIns)
{
    PVIRTIONET pThis = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s\n", pThis->szInst));
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
    Log7Func(("%s\n", pThis->szInst));

    RT_NOREF2(pThis, pThisCC);

    virtioNetR3QuiesceDevice(pDevIns, enmType);
    virtioNetWakeupRxBufWaiter(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3PowerOff(PPDMDEVINS pDevIns)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s\n", pThis->szInst));
    RT_NOREF(pThis);
    virtioNetR3SuspendOrPowerOff(pDevIns, kvirtIoVmStateChangedPowerOff);
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnSuspend}
 */
static DECLCALLBACK(void) virtioNetR3Suspend(PPDMDEVINS pDevIns)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    Log7Func(("%s \n", pThis->szInst));
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
    if (fOn)
        pThisR3->led.Asserted.s.fWriting = pThisR3->led.Actual.s.fWriting = 1;
    else
        pThisR3->led.Actual.s.fWriting = fOn;
}

/*
 * Returns true if VirtIO core and device are in a running and operational state
 */
DECLINLINE(bool) virtioNetIsOperational(PVIRTIONET pThis, PPDMDEVINS pDevIns)
{
    if (RT_LIKELY(pThis->fVirtioReady) && RT_LIKELY(!pThis->fQuiescing))
    {
        VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
        if (RT_LIKELY(enmVMState == VMSTATE_RUNNING || enmVMState == VMSTATE_RUNNING_LS))
            return true;
    }
    return false;
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
static int virtioNetR3CheckRxBufsAvail(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETVIRTQ pRxVirtq)
{
    int rc = VERR_INVALID_STATE;

    if (!virtioNetIsOperational(pThis, pDevIns))
        Log8Func(("%s No Rx bufs available. (VirtIO core not ready)\n", pThis->szInst));

    else if (!virtioCoreIsVirtqEnabled(&pThis->Virtio, pRxVirtq->idx))
        Log8Func(("%s No Rx bufs available. (%s not enabled)\n",  pThis->szInst, pRxVirtq->szName));

    else if (IS_VIRTQ_EMPTY(pDevIns, &pThis->Virtio, pRxVirtq->idx))
        Log8Func(("%s No Rx bufs available. (%s empty)\n",  pThis->szInst, pRxVirtq->szName));

    else
    {
        Log8Func(("%s Empty guest buffers available in %s\n",  pThis->szInst,pRxVirtq->szName));
        rc = VINF_SUCCESS;
    }
    virtioCoreVirtqNotifyEnable(&pThis->Virtio, pRxVirtq->idx, rc == VERR_INVALID_STATE /* fEnable */);
    return rc;
}

static bool virtioNetR3RxBufsAvail(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETVIRTQ *pRxVirtq)
{
    for (int idxVirtqPair = 0; idxVirtqPair < pThis->cVirtqPairs; idxVirtqPair++)
    {
        PVIRTIONETVIRTQ pThisRxVirtq = &pThis->aVirtqs[RXQIDX(idxVirtqPair)];
        if (RT_SUCCESS(virtioNetR3CheckRxBufsAvail(pDevIns, pThis, pThisRxVirtq)))
        {
            if (pRxVirtq)
                *pRxVirtq = pThisRxVirtq;
            return true;
        }
    }
    return false;
}

/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnWaitReceiveAvail}
 */
static DECLCALLBACK(int) virtioNetR3NetworkDown_WaitReceiveAvail(PPDMINETWORKDOWN pInterface, RTMSINTERVAL timeoutMs)
{
    PVIRTIONETCC pThisCC = RT_FROM_MEMBER(pInterface, VIRTIONETCC, INetworkDown);
    PPDMDEVINS   pDevIns = pThisCC->pDevIns;
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);

    if (virtioNetR3RxBufsAvail(pDevIns, pThis, NULL /* pRxVirtq */))
    {
        Log10Func(("%s Rx bufs now available, releasing waiter...\n", pThis->szInst));
        return VINF_SUCCESS;
    }
    if (!timeoutMs)
        return VERR_NET_NO_BUFFER_SPACE;

    LogFunc(("%s %s\n", pThis->szInst, timeoutMs == RT_INDEFINITE_WAIT ? "<indefinite wait>" : ""));

    ASMAtomicXchgBool(&pThis->fLeafWantsEmptyRxBufs, true);
    STAM_PROFILE_START(&pThis->StatRxOverflow, a);

    do {
        if (virtioNetR3RxBufsAvail(pDevIns, pThis, NULL /* pRxVirtq */))
        {
            Log10Func(("%s Rx bufs now available, releasing waiter...\n", pThis->szInst));
            return VINF_SUCCESS;
        }
        Log9Func(("%s Starved for empty guest Rx bufs. Waiting...\n", pThis->szInst));

        int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->hEventRxDescAvail, timeoutMs);

        if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
        {
            LogFunc(("Waken due to %s\n", rc == VERR_TIMEOUT ? "timeout" : "interrupted"));
            continue;
        }
        if (RT_FAILURE(rc)) {
            LogFunc(("Waken due to failure %Rrc\n", rc));
            RTThreadSleep(1);
        }
LogFunc(("\n\n\n********** WAKEN!!!!! ********\n\n\n"));
    } while (virtioNetIsOperational(pThis, pDevIns));

    STAM_PROFILE_STOP(&pThis->StatRxOverflow, a);
    ASMAtomicXchgBool(&pThis->fLeafWantsEmptyRxBufs, false);

    Log7Func(("%s Wait for Rx buffers available was interrupted\n", pThis->szInst));
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
                 pThis->szInst, pThis->virtioNetConfig.uMacAddress.au8,
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
        Log11Func(("\n%s not our VLAN, returning false\n", pThis->szInst));
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
                                       PVIRTIONETVIRTQ pRxVirtq)
{
    uint8_t fAddPktHdr = true;
    RTGCPHYS GCPhysPktHdrNumBuffers = 0;
    uint16_t cDescs;
    uint64_t uOffset;
    for (cDescs = uOffset = 0; uOffset < cb; )
    {
        PVIRTIO_DESC_CHAIN_T pDescChain = NULL;

        int rc = virtioCoreR3VirtqGet(pDevIns, &pThis->Virtio, pRxVirtq->idx, &pDescChain, true);
        AssertMsgReturn(rc == VINF_SUCCESS || rc == VERR_NOT_AVAILABLE, ("%Rrc\n", rc), rc);

        /** @todo  Find a better way to deal with this */
        AssertMsgReturnStmt(rc == VINF_SUCCESS && pDescChain->cbPhysReturn,
                            ("Not enough Rx buffers in queue to accomodate ethernet packet\n"),
                            virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain),
                            VERR_INTERNAL_ERROR);

        /* Length of first seg of guest Rx buf should never be less than sizeof(virtio_net_pkt_hdr).
         * Otherwise code has to become more complicated, e.g. locate & cache seg idx & offset of
         * virtio_net_header.num_buffers, to defer updating (in GCPhys). Re-visit if needed */

        AssertMsgReturnStmt(pDescChain->pSgPhysReturn->paSegs[0].cbSeg >= sizeof(VIRTIONET_PKT_HDR_T),
                            ("Desc chain's first seg has insufficient space for pkt header!\n"),
                            virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain),
                            VERR_INTERNAL_ERROR);

        size_t cbDescChainLeft = pDescChain->cbPhysReturn;
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

                /* Calculate & cache addr of field to update after final value is known, in GCPhys mem */
                GCPhysPktHdrNumBuffers = pDescChain->pSgPhysReturn->paSegs[0].GCPhys
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
            size_t cbCropped = RT_MIN(cb, cbDescChainLeft);
            paVirtSegsToGuest[cSegs].cbSeg = cbCropped;
            paVirtSegsToGuest[cSegs].pvSeg = ((uint8_t *)pvBuf) + uOffset;
            cbDescChainLeft -= cbCropped;
            uOffset += cbCropped;
            cDescs++;
            cSegs++;
            RTSgBufInit(pVirtSegBufToGuest, paVirtSegsToGuest, cSegs);
            Log7Func(("Send Rx pkt to guest...\n"));
            STAM_PROFILE_START(&pThis->StatReceiveStore, a);
            virtioCoreR3VirtqPut(pDevIns, &pThis->Virtio, pRxVirtq->idx,
                                 pVirtSegBufToGuest, pDescChain, true /* fFence */);
            STAM_PROFILE_STOP(&pThis->StatReceiveStore, a);

            if (FEATURE_DISABLED(MRG_RXBUF))
                break;
        }

        virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain);
    }

    if (uOffset < cb)
    {
        LogFunc(("%s Packet did not fit into RX queue (packet size=%u)!\n", pThis->szInst, cb));
        return VERR_TOO_MUCH_DATA;
    }

    /* Fix-up pkthdr (in guest phys. memory) with number buffers (descriptors) processed */

    int rc = PDMDevHlpPCIPhysWrite(pDevIns, GCPhysPktHdrNumBuffers, &cDescs, sizeof(cDescs));
    AssertMsgRCReturn(rc,
                  ("Failure updating descriptor count in pkt hdr in guest physical memory\n"),
                  rc);

    virtioCoreVirtqSync(pDevIns, &pThis->Virtio, pRxVirtq->idx);

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
 * @param   idxRxVirtq      Rx queue to work with
 * @thread  RX
 */
static int virtioNetR3HandleRxPacket(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                const void *pvBuf, size_t cb, PCPDMNETWORKGSO pGso, PVIRTIONETVIRTQ pRxVirtq)
{
    RT_NOREF(pThisCC);

    LogFunc(("%s (%RTmac) pGso %s\n", pThis->szInst, pvBuf, pGso ? "present" : "not present"));
    VIRTIONET_PKT_HDR_T rxPktHdr = { 0 };

    if (pGso)
    {
        Log2Func(("%s gso type=%x cbPktHdrsTotal=%u cbPktHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
              pThis->szInst, pGso->u8Type, pGso->cbHdrsTotal,
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
                                        pVirtSegBufToGuest, paVirtSegsToGuest, pRxVirtq);

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
LogFunc(("\n"));
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
            LogFunc(("%s GSO type (0x%x) not supported\n", pThis->szInst, pGso->u8Type));
            return VERR_NOT_SUPPORTED;
        }
    }

    Log10Func(("%s pvBuf=%p cb=%3u pGso=%p ...\n", pThis->szInst, pvBuf, cb, pGso));

    /** @todo If we ever start using more than one Rx/Tx queue pair, is a random queue
              selection algorithm feasible or even necessary to prevent starvation? */

    for (int idxVirtqPair = 0; idxVirtqPair < pThis->cVirtqPairs; idxVirtqPair++)
    {

        PVIRTIONETVIRTQ pRxVirtq = &pThis->aVirtqs[RXQIDX(idxVirtqPair)];
        if (RT_SUCCESS(!virtioNetR3CheckRxBufsAvail(pDevIns, pThis, pRxVirtq)))
        {
            /* Drop packets if VM is not running or cable is disconnected. */
            if (!virtioNetIsOperational(pThis, pDevIns) || !IS_LINK_UP(pThis))
                return VINF_SUCCESS;

            STAM_PROFILE_START(&pThis->StatReceive, a);
            virtioNetR3SetReadLed(pThisCC, true);

            int rc = VINF_SUCCESS;
            if (virtioNetR3AddressFilter(pThis, pvBuf, cb))
            {
                rc = virtioNetR3HandleRxPacket(pDevIns, pThis, pThisCC, pvBuf, cb, pGso, pRxVirtq);
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
static void virtioNetR3PullChain(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIO_DESC_CHAIN_T pDescChain, void *pv, size_t cb)
{
    uint8_t *pb = (uint8_t *)pv;
    size_t cbLim = RT_MIN(pDescChain->cbPhysSend, cb);
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
             pThis->szInst, cb - cbLim, cb, pDescChain->cbPhysSend));
    RT_NOREF(pThis);
}

static uint8_t virtioNetR3CtrlRx(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                 PVIRTIONET_CTRL_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{

#define LOG_VIRTIONET_FLAG(fld) LogFunc(("%s Setting %s=%d\n", pThis->szInst, #fld, pThis->fld))

    LogFunc(("%s Processing CTRL Rx command\n", pThis->szInst));
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
    virtioNetR3PullChain(pDevIns, pThis, pDescChain, &fOn, (size_t)RT_MIN(pDescChain->cbPhysSend, sizeof(fOn)));

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
        if (pThis->fPromiscuous | pThis->fAllMulticast)
        {
            pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, true);
        }
        else
        {
            pThisCC->pDrv->pfnSetPromiscuousMode(pThisCC->pDrv, false);
        }
    }

    return VIRTIONET_OK;
}

static uint8_t virtioNetR3CtrlMac(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                  PVIRTIONET_CTRL_HDR_T pCtrlPktHdr, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    LogFunc(("%s Processing CTRL MAC command\n", pThis->szInst));

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
            Log7Func(("%s Guest provided %d unicast MAC Table entries\n", pThis->szInst, cMacs));
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
            Log10Func(("%s Guest provided %d multicast MAC Table entries\n", pThis->szInst, cMacs));
            if (cMacs)
            {
                uint32_t cbMacs = cMacs * sizeof(RTMAC);
                ASSERT_CTRL_TABLE_SET(cbRemaining >= cbMacs);
                virtioNetR3PullChain(pDevIns, pThis, pDescChain, &pThis->aMacMulticastFilter, cbMacs);
                cbRemaining -= cbMacs;
            }
            pThis->cMulticastFilterMacs = cMacs;

#ifdef LOG_ENABLED
            LogFunc(("%s unicast MACs:\n", pThis->szInst));
            for(unsigned i = 0; i < cMacs; i++)
                LogFunc(("         %RTmac\n", &pThis->aMacUnicastFilter[i]));

            LogFunc(("%s multicast MACs:\n", pThis->szInst));
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
    LogFunc(("%s Processing CTRL VLAN command\n", pThis->szInst));

    RT_NOREF(pThisCC);

    uint16_t uVlanId;
    size_t cbRemaining = pDescChain->cbPhysSend - sizeof(*pCtrlPktHdr);
    AssertMsgReturn(cbRemaining > sizeof(uVlanId),
        ("DESC chain too small for VIRTIO_NET_CTRL_VLAN cmd processing"), VIRTIONET_ERROR);
    virtioNetR3PullChain(pDevIns, pThis, pDescChain, &uVlanId, sizeof(uVlanId));
    AssertMsgReturn(uVlanId > VIRTIONET_MAX_VLAN_ID,
        ("%s VLAN ID out of range (VLAN ID=%u)\n", pThis->szInst, uVlanId), VIRTIONET_ERROR);
    LogFunc(("%s uCommand=%u VLAN ID=%u\n", pThis->szInst, pCtrlPktHdr->uCmd, uVlanId));
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
    LogFunc(("%s Received CTRL packet from guest\n", pThis->szInst));

    if (pDescChain->cbPhysSend < 2)
    {
        LogFunc(("%s CTRL packet from guest driver incomplete. Skipping ctrl cmd\n", pThis->szInst));
        return;
    }
    else if (pDescChain->cbPhysReturn < sizeof(VIRTIONET_CTRL_HDR_T_ACK))
    {
        LogFunc(("%s Guest driver didn't allocate memory to receive ctrl pkt ACK. Skipping ctrl cmd\n", pThis->szInst));
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

    Log7Func(("%s CTRL COMMAND: class=%d command=%d\n", pThis->szInst, pCtrlPktHdr->uClass, pCtrlPktHdr->uCmd));

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
                         "VIRTIO_F_STATUS or VIRTIO_F_GUEST_ANNOUNCE feature not enabled\n", pThis->szInst));
                break;
            }
            if (pCtrlPktHdr->uCmd != VIRTIONET_CTRL_ANNOUNCE_ACK)
            {
                LogFunc(("%s Ignoring CTRL class VIRTIONET_CTRL_ANNOUNCE. Unrecognized uCmd\n", pThis->szInst));
                break;
            }
            pThis->virtioNetConfig.uStatus &= ~VIRTIONET_F_ANNOUNCE;
            Log7Func(("%s Clearing VIRTIONET_F_ANNOUNCE in config status\n", pThis->szInst));
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

    virtioCoreR3VirtqPut(pDevIns, &pThis->Virtio, CTRLQIDX, pReturnSegBuf, pDescChain, true /* fFence */);
    virtioCoreVirtqSync(pDevIns, &pThis->Virtio, CTRLQIDX);

    for (int i = 0; i < cSegs; i++)
        RTMemFree(paReturnSegs[i].pvSeg);

    RTMemFree(paReturnSegs);
    RTMemFree(pReturnSegBuf);

    LogFunc(("%s Finished processing CTRL command with status %s\n",
             pThis->szInst, uAck == VIRTIONET_OK ? "VIRTIONET_OK" : "VIRTIONET_ERROR"));
}

static int virtioNetR3ReadHeader(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, PVIRTIONET_PKT_HDR_T pPktHdr, size_t cbFrame)
{
    int rc = PDMDevHlpPCIPhysRead(pDevIns, GCPhys, pPktHdr, sizeof(*pPktHdr));
    if (RT_FAILURE(rc))
        return rc;

    LogFunc(("pktHdr (flags=%x gso-type=%x len=%x gso-size=%x Chksum-start=%x Chksum-offset=%x) cbFrame=%d\n",
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
                  pThis->szInst, pGso->cbHdrsTotal));
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
                  pThis->szInst, pGso->cbHdrsTotal));
        }
        Log2Func(("%s gso type=%x cbHdrsTotal=%u cbHdrsSeg=%u mss=%u off1=0x%x off2=0x%x\n",
                  pThis->szInst, pGso->u8Type, pGso->cbHdrsTotal, pGso->cbHdrsSeg,
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

    return pThisCC->pDrv->pfnSendBuf(pThisCC->pDrv, pSgBuf, true /* fOnWorkerThread */);
}

static void virtioNetR3TransmitPendingPackets(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC,
                                         PVIRTIONETVIRTQ pTxVirtq, bool fOnWorkerThread)
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
                pThis->szInst, pThis->virtioNetConfig.uStatus));
        return;
    }

    if (!pThis->fCableConnected)
    {
        Log(("%s Ignoring transmit requests while cable is disconnected.\n", pThis->szInst));
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

    int cPkts = virtioCoreVirtqAvailCount(pVirtio->pDevInsR3, pVirtio, pTxVirtq->idx);
    if (!cPkts)
    {
        LogFunc(("%s No packets to send found on %s\n", pThis->szInst, pTxVirtq->szName));

        if (pDrv)
            pDrv->pfnEndXmit(pDrv);

        ASMAtomicWriteU32(&pThis->uIsTransmitting, 0);
        return;
    }
    LogFunc(("%s About to transmit %d pending packet%c\n", pThis->szInst, cPkts, cPkts == 1 ? ' ' : 's'));

    virtioNetR3SetWriteLed(pThisCC, true);

    int rc;
    PVIRTIO_DESC_CHAIN_T pDescChain = NULL;
    while ((rc = virtioCoreR3VirtqPeek(pVirtio->pDevInsR3, pVirtio, pTxVirtq->idx, &pDescChain)) == VINF_SUCCESS)
    {
        Log10Func(("%s fetched descriptor chain from %s\n", pThis->szInst, pTxVirtq->szName));

        PVIRTIOSGBUF pSgPhysSend = pDescChain->pSgPhysSend;
        PVIRTIOSGSEG paSegsFromGuest = pSgPhysSend->paSegs;
        uint32_t cSegsFromGuest = pSgPhysSend->cSegs;

        VIRTIONET_PKT_HDR_T PktHdr;
        size_t uSize = 0;

        Assert(paSegsFromGuest[0].cbSeg >= sizeof(PktHdr));

        /* Compute total frame size. */
        for (unsigned i = 0; i < cSegsFromGuest && uSize < VIRTIONET_MAX_FRAME_SIZE; i++)
            uSize +=  paSegsFromGuest[i].cbSeg;

        Log5Func(("%s complete frame is %u bytes.\n", pThis->szInst, uSize));
        Assert(uSize <= VIRTIONET_MAX_FRAME_SIZE);

        /* Truncate oversized frames. */
        if (uSize > VIRTIONET_MAX_FRAME_SIZE)
            uSize = VIRTIONET_MAX_FRAME_SIZE;

        if (pThisCC->pDrv)
        {
            uint64_t uOffset;

            uSize -= sizeof(PktHdr);
            rc = virtioNetR3ReadHeader(pDevIns, paSegsFromGuest[0].GCPhys, &PktHdr, uSize);
            if (RT_FAILURE(rc))
                return;
            virtioCoreSgBufAdvance(pSgPhysSend, sizeof(PktHdr));

            PDMNETWORKGSO  Gso, *pGso = virtioNetR3SetupGsoCtx(&Gso, &PktHdr);

            PPDMSCATTERGATHER pSgBufToPdmLeafDevice;
            rc = pThisCC->pDrv->pfnAllocBuf(pThisCC->pDrv, uSize, pGso, &pSgBufToPdmLeafDevice);
            if (RT_SUCCESS(rc))
            {
                STAM_REL_COUNTER_INC(&pThis->StatTransmitPackets);
                STAM_PROFILE_START(&pThis->StatTransmitSend, a);

                size_t cbCopied = 0;
                size_t cbTotal = 0;
                size_t cbRemain = pSgBufToPdmLeafDevice->cbUsed = uSize;
                uOffset = 0;
                while (cbRemain)
                {
                    PVIRTIOSGSEG paSeg  = &pSgPhysSend->paSegs[pSgPhysSend->idxSeg];
                    uint64_t srcSgStart = (uint64_t)paSeg->GCPhys;
                    uint64_t srcSgLen   = (uint64_t)paSeg->cbSeg;
                    uint64_t srcSgCur   = (uint64_t)pSgPhysSend->GCPhysCur;
                    cbCopied = RT_MIN((uint64_t)cbRemain, srcSgLen - (srcSgCur - srcSgStart));
                    PDMDevHlpPCIPhysRead(pDevIns,
                                         (RTGCPHYS)pSgPhysSend->GCPhysCur,
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
                    LogFunc(("%s Failed to transmit frame, rc = %Rrc\n", pThis->szInst, rc));
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
            virtioCoreR3VirtqSkip(pVirtio, pTxVirtq->idx);

            /* No data to return to guest, but call is needed put elem (e.g. desc chain) on used ring */
            virtioCoreR3VirtqPut(pVirtio->pDevInsR3, pVirtio, pTxVirtq->idx, NULL, pDescChain, true /* fFence */);

            /* Update used ring idx and notify guest that we've transmitted the data it sent */
            virtioCoreVirtqSync(pVirtio->pDevInsR3, pVirtio, pTxVirtq->idx);
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
    PVIRTIONETVIRTQ pTxVirtq  = &pThis->aVirtqs[TXQIDX(0)];
    STAM_COUNTER_INC(&pThis->StatTransmitByNetwork);

    /** @todo If we ever start using more than one Rx/Tx queue pair, is a random queue
          selection algorithm feasible or even necessary */
    virtioNetR3TransmitPendingPackets(pDevIns, pThis, pThisCC, pTxVirtq, true /*fOnWorkerThread*/);
}


#ifdef USING_CRITICAL_SECTION
DECLINLINE(int) virtioNetR3CsEnter(PPDMDEVINS pDevIns, PVIRTIONET pThis, int rcBusy)
{
    RT_NOREF(pDevIns, pThis, rcBusy);
    /* Original DevVirtioNet uses CS in attach/detach/link-up timer/tx timer/transmit */
    LogFunc(("%s CS unimplemented. What does the critical section protect in orig driver??", pThis->szInst));
    return VINF_SUCCESS;
}

DECLINLINE(void) virtioNetR3CsLeave(PPDMDEVINS pDevIns, PVIRTIONET pThis)
{
    RT_NOREF(pDevIns, pThis);
    LogFunc(("%s CS unimplemented. What does the critical section protect in orig driver??", pThis->szInst));
}
#endif /* USING_CRITICAL_SECTION */

/**
 * @callback_method_impl{FNTMTIMERDEV, Link Up Timer handler.}
 */
static DECLCALLBACK(void) virtioNetR3LinkUpTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PVIRTIONET   pThis   = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    RT_NOREF(pTimer, pvUser);

    ENTER_CRITICAL_SECTION;

    SET_LINK_UP(pThis);

    virtioNetWakeupRxBufWaiter(pDevIns);

    LEAVE_CRITICAL_SECTION;

    LogFunc(("%s Link is up\n", pThis->szInst));

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

        LogFunc(("%s Link is down temporarily\n", pThis->szInst));
    }
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

    if (LogIs7Enabled())
    {
        LogFunc(("%s", pThis->szInst));
        switch(enmState)
        {
        case PDMNETWORKLINKSTATE_UP:
            Log(("UP\n"));
            break;
        case PDMNETWORKLINKSTATE_DOWN:
            Log(("DOWN\n"));
            break;
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            Log(("DOWN (RESUME)\n"));
            break;
        default:
            Log(("UNKNOWN)\n"));
        }
    }

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
            Log(("%s Link is up\n", pThis->szInst));
            pThis->fCableConnected = true;
            SET_LINK_UP(pThis);
            virtioCoreNotifyConfigChanged(&pThis->Virtio);
        }
        else /* cached Link state is down */
        {
            /* The link was brought down explicitly, make sure it won't come up by timer.  */
            PDMDevHlpTimerStop(pDevIns, pThisCC->hLinkUpTimer);
            Log(("%s Link is down\n", pThis->szInst));
            pThis->fCableConnected = false;
            SET_LINK_DOWN(pThis);
            virtioCoreNotifyConfigChanged(&pThis->Virtio);
        }
        if (pThisCC->pDrv)
            pThisCC->pDrv->pfnNotifyLinkChanged(pThisCC->pDrv, enmState);
    }
    return VINF_SUCCESS;
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

static int virtioNetR3DestroyWorkerThreads(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC)
{
    Log10Func(("%s\n", pThis->szInst));
    int rc = VINF_SUCCESS;
    for (unsigned idxWorker = 0; idxWorker < pThis->cWorkers; idxWorker++)
    {
        PVIRTIONETWORKER   pWorker   = &pThis->aWorkers[idxWorker];
        PVIRTIONETWORKERR3 pWorkerR3 = &pThisCC->aWorkers[idxWorker];

        if (pWorker->hEvtProcess != NIL_SUPSEMEVENT)
        {
            PDMDevHlpSUPSemEventClose(pDevIns, pWorker->hEvtProcess);
            pWorker->hEvtProcess = NIL_SUPSEMEVENT;
        }
        if (pWorkerR3->pThread)
        {
            int rcThread;
            rc = PDMDevHlpThreadDestroy(pDevIns, pWorkerR3->pThread, &rcThread);
            if (RT_FAILURE(rc) || RT_FAILURE(rcThread))
                AssertMsgFailed(("%s Failed to destroythread rc=%Rrc rcThread=%Rrc\n", __FUNCTION__, rc, rcThread));
            pWorkerR3->pThread = NULL;
        }
    }
    return rc;
}

static int virtioNetR3CreateOneWorkerThread(PPDMDEVINS pDevIns, PVIRTIONET pThis, uint16_t idxWorker,
                                            PVIRTIONETWORKER pWorker, PVIRTIONETWORKERR3 pWorkerR3,
                                            PVIRTIONETVIRTQ pVirtq)
{
    Log10Func(("%s\n", pThis->szInst));
    RT_NOREF(pThis);

    int rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pWorker->hEvtProcess);

    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("DevVirtioNET: Failed to create SUP event semaphore"));

    LogFunc(("creating thread for queue %s\n", pVirtq->szName));

    rc = PDMDevHlpThreadCreate(pDevIns, &pWorkerR3->pThread,
                               (void *)pWorker, virtioNetR3WorkerThread,
                               virtioNetR3WakeupWorker, 0, RTTHREADTYPE_IO, pVirtq->szName);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Error creating thread for Virtual Virtq %s\n"), pVirtq->idx);

    pWorker->pVirtq    = pWorkerR3->pVirtq   = pVirtq;
    pWorker->idx       = pWorkerR3->idx      = idxWorker;
    pVirtq->pWorker    = pWorker;
    pVirtq->pWorkerR3  = pWorkerR3;
    pWorker->fAssigned = true;

    LogFunc(("%s pThread: %p\n", pVirtq->szName, pWorkerR3->pThread));

    return rc;
}

static int virtioNetR3CreateWorkerThreads(PPDMDEVINS pDevIns, PVIRTIONET pThis, PVIRTIONETCC pThisCC)
{

#define CTRLWIDX 0 /* First worker is for the control queue */

    Log10Func(("%s\n", pThis->szInst));

    PVIRTIONETVIRTQ pCtlVirtq = &pThis->aVirtqs[CTRLQIDX];
    int rc = virtioNetR3CreateOneWorkerThread(pDevIns, pThis, CTRLQIDX /* idxWorker */,
                                              &pThis->aWorkers[CTRLWIDX], &pThisCC->aWorkers[CTRLWIDX], pCtlVirtq);
    AssertRCReturn(rc, rc);

    pCtlVirtq->fHasWorker = true;

    uint16_t idxWorker = CTRLWIDX + 1;
    for (uint16_t idxVirtqPair = 0; idxVirtqPair < pThis->cVirtqPairs; idxVirtqPair++, idxWorker++)
    {
        PVIRTIONETVIRTQ pTxVirtq = &pThis->aVirtqs[TXQIDX(idxVirtqPair)];
        PVIRTIONETVIRTQ pRxVirtq = &pThis->aVirtqs[RXQIDX(idxVirtqPair)];

        rc = virtioNetR3CreateOneWorkerThread(pDevIns, pThis, idxWorker, &pThis->aWorkers[idxWorker],
                                              &pThisCC->aWorkers[idxWorker], pTxVirtq);
        AssertRCReturn(rc, rc);

        pTxVirtq->fHasWorker = true;
        pRxVirtq->fHasWorker = false;
    }
    pThis->cWorkers = pThis->cVirtqPairs + 1 /* Control virtq */;
    return rc;
}

/**
 * @callback_method_impl{FNPDMTHREADDEV}
 */
static DECLCALLBACK(int) virtioNetR3WorkerThread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVIRTIONET         pThis     = PDMDEVINS_2_DATA(pDevIns, PVIRTIONET);
    PVIRTIONETCC       pThisCC   = PDMDEVINS_2_DATA_CC(pDevIns, PVIRTIONETCC);
    PVIRTIONETWORKER   pWorker   = (PVIRTIONETWORKER)pThread->pvUser;
    PVIRTIONETVIRTQ    pVirtq    = pWorker->pVirtq;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    LogFunc(("%s worker thread started for %s\n", pThis->szInst, pVirtq->szName));

    /** @todo Race w/guest enabling/disabling guest notifications cyclically.
              See BugRef #8651, Comment #82 */
    virtioCoreVirtqNotifyEnable(&pThis->Virtio, pVirtq->idx, true /* fEnable */);

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (IS_VIRTQ_EMPTY(pDevIns, &pThis->Virtio, pVirtq->idx))
        {
            /* Atomic interlocks avoid missing alarm while going to sleep & notifier waking the awoken */
            ASMAtomicWriteBool(&pWorker->fSleeping, true);
            bool fNotificationSent = ASMAtomicXchgBool(&pWorker->fNotified, false);
            if (!fNotificationSent)
            {
                Log10Func(("%s %s worker sleeping...\n\n", pThis->szInst, pVirtq->szName));
                Assert(ASMAtomicReadBool(&pWorker->fSleeping));
                int rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
                STAM_COUNTER_INC(&pThis->StatTransmitByThread);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    return VINF_SUCCESS;
                if (rc == VERR_INTERRUPTED)
                    continue;
               ASMAtomicWriteBool(&pWorker->fNotified, false);
            }
            ASMAtomicWriteBool(&pWorker->fSleeping, false);
        }

        /* Dispatch to the handler for the queue this worker is set up to drive */

        if (!pThisCC->fQuiescing)
        {
             if (pVirtq->fCtlVirtq)
             {
                 Log10Func(("%s %s worker woken. Fetching desc chain\n", pThis->szInst, pVirtq->szName));
                 PVIRTIO_DESC_CHAIN_T pDescChain = NULL;
                 int rc = virtioCoreR3VirtqGet(pDevIns, &pThis->Virtio, pVirtq->idx, &pDescChain, true);
                 if (rc == VERR_NOT_AVAILABLE)
                 {
                    Log10Func(("%s %s worker woken. Nothing found in queue/n", pThis->szInst, pVirtq->szName));
                    continue;
                 }
                 virtioNetR3Ctrl(pDevIns, pThis, pThisCC, pDescChain);
                 virtioCoreR3DescChainRelease(&pThis->Virtio, pDescChain);
             }
             else /* Must be Tx queue */
             {
                 Log10Func(("%s %s worker woken. Virtq has data to transmit\n",  pThis->szInst, pVirtq->szName));
                 virtioNetR3TransmitPendingPackets(pDevIns, pThis, pThisCC, pVirtq, false /* fOnWorkerThread */);
             }

             /* Rx queues aren't handled by our worker threads. Instead, the PDM network
              * leaf driver invokes PDMINETWORKDOWN.pfnWaitReceiveAvail() callback,
              * which waits until notified directly by virtioNetVirtqNotified()
              * that guest IN buffers have been added to receive virt queue.
              */
        }
    }
    Log10(("%s %s worker thread exiting\n", pThis->szInst, pVirtq->szName));
    return VINF_SUCCESS;
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
                 pThis->szInst));

        pThis->fNegotiatedFeatures = virtioCoreGetNegotiatedFeatures(pVirtio);

#ifdef LOG_ENABLED
        virtioCorePrintFeatures(pVirtio, NULL);
        virtioNetPrintFeatures(pThis, NULL);
#endif

        pThis->virtioNetConfig.uStatus = pThis->fCableConnected ? VIRTIONET_F_LINK_UP : 0;

        pThis->fResetting = false;
        pThisCC->fQuiescing = false;

        for (unsigned idxVirtq = 0; idxVirtq < pThis->cVirtVirtqs; idxVirtq++)
        {
            PVIRTIONETVIRTQ pVirtq = &pThis->aVirtqs[idxVirtq];
            pVirtq->idx = idxVirtq;
            (void) virtioCoreR3VirtqAttach(&pThis->Virtio, pVirtq->idx, pVirtq->szName);
            pVirtq->fAttachedToVirtioCore = true;
            if (IS_VIRTQ_EMPTY(pThisCC->pDevIns, &pThis->Virtio, pVirtq->idx))
                virtioCoreVirtqNotifyEnable(&pThis->Virtio, pVirtq->idx, true /* fEnable */);
        }
    }
    else
    {
        LogFunc(("%s VirtIO is resetting\n", pThis->szInst));

        pThis->virtioNetConfig.uStatus = pThis->fCableConnected ? VIRTIONET_F_LINK_UP : 0;
        Log7Func(("%s Link is %s\n", pThis->szInst, pThis->fCableConnected ? "up" : "down"));

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

        for (uint16_t idxVirtq = 0; idxVirtq < pThis->cVirtVirtqs; idxVirtq++)
            pThis->aVirtqs[idxVirtq].fAttachedToVirtioCore = false;
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

    Log7Func(("%s\n", pThis->szInst));
    AssertLogRelReturnVoid(iLUN == 0);

    ENTER_CRITICAL_SECTION;

    RT_NOREF(pThis);

    /*
     * Zero important members.
     */
    pThisCC->pDrvBase = NULL;
    pThisCC->pDrv     = NULL;

    LEAVE_CRITICAL_SECTION;
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

    Log7Func(("%s", pThis->szInst));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    ENTER_CRITICAL_SECTION;

    RT_NOREF(pThis);

    int rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThisCC->IBase, &pThisCC->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        pThisCC->pDrv = PDMIBASE_QUERY_INTERFACE(pThisCC->pDrvBase, PDMINETWORKUP);
        AssertMsgStmt(pThisCC->pDrv, ("Failed to obtain the PDMINETWORKUP interface!\n"),
                      rc = VERR_PDM_MISSING_INTERFACE_BELOW);
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
                    Log(("%s No attached driver!\n", pThis->szInst));

    LEAVE_CRITICAL_SECTION;
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
LogFunc(("pInterface=%p %s\n", pInterface, pszIID));
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

    Log(("%s Destroying instance\n", pThis->szInst));

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

    RTStrPrintf(pThis->szInst, sizeof(pThis->szInst), "VNET%d", iInstance);

// Temporary for less logging clutter for singled-instance debugging
*pThis->szInst = '\0';

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

    pThis->hEventRxDescAvail = NIL_SUPSEMEVENT;

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
                pThis->szInst, pThis->cMsLinkUpDelay / 1000));

    Log(("%s Link up delay is set to %u seconds\n", pThis->szInst, pThis->cMsLinkUpDelay / 1000));

    /* Copy the MAC address configured for the VM to the MMIO accessible Virtio dev-specific config area */
    memcpy(pThis->virtioNetConfig.uMacAddress.au8, pThis->macConfigured.au8, sizeof(pThis->virtioNetConfig.uMacAddress)); /* TBD */


    LogFunc(("RC=%RTbool R0=%RTbool\n", pDevIns->fRCEnabled, pDevIns->fR0Enabled));

    /*
     * Do core virtio initialization.
     */

#   if FEATURE_OFFERED(STATUS)
        pThis->virtioNetConfig.uStatus = 0;
#   endif

#   if FEATURE_OFFERED(MQ)
        pThis->virtioNetConfig.uMaxVirtqPairs = VIRTIONET_MAX_QPAIRS;
#   endif

    /* Initialize the generic Virtio core: */
    pThisCC->Virtio.pfnVirtqNotified        = virtioNetVirtqNotified;
    pThisCC->Virtio.pfnStatusChanged        = virtioNetR3StatusChanged;
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
     * Create the semaphore that will be used to synchronize/throttle
     * the downstream LUN's Rx waiter thread.
     */
    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->hEventRxDescAvail);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to create event semaphore"));

    /*
     * Initialize VirtIO core. This will result in a "status changed" callback
     * when VirtIO is ready, at which time the Rx queue and ctrl queue worker threads will be created.
     */
    rc = virtioCoreR3Init(pDevIns, &pThis->Virtio, &pThisCC->Virtio, &VirtioPciParams, pThis->szInst,
                          VIRTIONET_HOST_FEATURES_OFFERED,
                          &pThis->virtioNetConfig /*pvDevSpecificCap*/, sizeof(pThis->virtioNetConfig));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: failed to initialize VirtIO"));

    pThis->fNegotiatedFeatures = virtioCoreGetNegotiatedFeatures(&pThis->Virtio);
    if (!virtioNetValidateRequiredFeatures(pThis->fNegotiatedFeatures))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-net: Required features not successfully negotiated."));

    pThis->cVirtqPairs =  (pThis->fNegotiatedFeatures & VIRTIONET_F_MQ)
                         ? pThis->virtioNetConfig.uMaxVirtqPairs  :  1;

    pThis->cVirtVirtqs += pThis->cVirtqPairs * 2 + 1;

    /* Create Link Up Timer */
    rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL, virtioNetR3LinkUpTimer, NULL, TMTIMER_FLAGS_NO_CRIT_SECT,
                              "VirtioNet Link Up Timer", &pThisCC->hLinkUpTimer);

    /*
     * Initialize queues.
     */
    virtioNetR3SetVirtqNames(pThis);
    pThis->aVirtqs[CTRLQIDX].fCtlVirtq = true;

    /*
     * Create queue workers for life of instance. (I.e. they persist through VirtIO bounces)
     */
    rc = virtioNetR3CreateWorkerThreads(pDevIns, pThis, pThisCC);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to worker threads"));

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
                    Log(("%s No attached driver!\n", pThis->szInst));

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
    rc = PDMDevHlpDBGFInfoRegister(pDevIns, "virtio-net", "Display virtio-net info (help, net, features, state, pointers, queues, all)", virtioNetR3Info);
    if (RT_FAILURE(rc))
        LogRel(("Failed to register DBGF info for device %s\n", szTmp));
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
    pThisCC->Virtio.pfnVirtqNotified = virtioNetVirtqNotified;
    return virtioCoreRZInit(pDevIns, &pThis->Virtio);
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioNet_1_0 =
{
    /* .uVersion = */               PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "virtio-net-1-dot-0",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE | PDM_DEVREG_FLAGS_RZ,
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

