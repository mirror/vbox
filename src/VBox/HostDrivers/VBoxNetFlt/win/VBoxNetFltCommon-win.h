/* $Id$ */
/** @file
 * VBoxNetFltCommon.h - Network Filter Driver (Host), Windows Specific Code. Common headeer with commonly used defines and decls
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#ifndef ___VBoxNetFltCommon_win_h___
#define ___VBoxNetFltCommon_win_h___

#ifndef VBOX_NETFLT_ONDEMAND_BIND
# define NDIS_MINIPORT_DRIVER
# define NDIS50_MINIPORT 1
#endif

#define BINARY_COMPATIBLE 0
#define NDIS_WDM 0
#define NDIS50 1
#define NTSTRSAFE_LIB

#ifdef DEBUG
//# define DEBUG_NETFLT_PACKETS
# ifndef DEBUG_misha
#  define DEBUG_NETFLT_NOASSERT
# endif
/* # define DEBUG_NETFLT_LOOPBACK */

/* receive logic has several branches */
/* the DEBUG_NETFLT_RECV* macros used to debug the ProtocolReceive callback
 * which is typically not used in case the underlying miniport indicates the packets with NdisMIndicateReceivePacket
 * the best way to debug the ProtocolReceive (which in turn has several branches) is to enable the DEBUG_NETFLT_RECV
 * one by one in the below order, i.e.
 * first DEBUG_NETFLT_RECV
 * then DEBUG_NETFLT_RECV + DEBUG_NETFLT_RECV_NOPACKET */
//# define DEBUG_NETFLT_RECV
//# define DEBUG_NETFLT_RECV_NOPACKET
//# define DEBUG_NETFLT_RECV_TRANSFERDATA
#endif

#define LOG_GROUP LOG_GROUP_NET_FLT_DRV

#include <VBox/intnet.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/time.h>
#include <iprt/net.h>

RT_C_DECLS_BEGIN
#include <ndis.h>
RT_C_DECLS_END



#define VBOXNETFLT_OS_SPECFIC 1

#ifdef VBOX_NETFLT_ONDEMAND_BIND
# define VBOXNETFLT_PROTOCOL_NAME L"VBoxNetFltPt"
#else
# ifndef VBOXNETADP
#  define VBOXNETFLT_PROTOCOL_NAME L"VBoxNetFlt"

/** this is to support ioctl interface */
#  define LINKNAME_STRING     L"\\DosDevices\\Global\\VBoxNetFlt"
#  define NTDEVICE_STRING     L"\\Device\\VBoxNetFlt"
# else
#  define LINKNAME_STRING     L"\\DosDevices\\Global\\VBoxNetAdp"
#  define NTDEVICE_STRING     L"\\Device\\VBoxNetAdp"
# endif
//# define VBOXNETFLT_WIN_IOCTL_INIT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER,  FILE_WRITE_ACCESS)
//# define VBOXNETFLT_WIN_IOCTL_FINI CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER,  FILE_WRITE_ACCESS)
#endif

/** version
 * NOTE: we are NOT using NDIS 5.1 features now, the code under "#ifdef NDIS51xxx" is not tested and may not work and should be removed soon */
#ifdef NDIS51_MINIPORT
# define VBOXNETFLT_MAJOR_NDIS_VERSION            5
# define VBOXNETFLT_MINOR_NDIS_VERSION            1
#else
# define VBOXNETFLT_MAJOR_NDIS_VERSION            5
# define VBOXNETFLT_MINOR_NDIS_VERSION            0
#endif

#ifdef NDIS51
# define VBOXNETFLT_PROT_MAJOR_NDIS_VERSION    5
# define VBOXNETFLT_PROT_MINOR_NDIS_VERSION    0
#else
# define VBOXNETFLT_PROT_MAJOR_NDIS_VERSION    5
# define VBOXNETFLT_PROT_MINOR_NDIS_VERSION    0
#endif

/** advance declaration */
typedef struct _ADAPT ADAPT, *PADAPT;

typedef struct VBOXNETFLTINS *PVBOXNETFLTINS;

/** configuration */

/** received packets queue size. the queue is used when the driver is working in a pass-thru mode */
#define MAX_RECEIVE_PACKET_ARRAY_SIZE           40

/** Ndis Packet pool settings
 * these are applied to both receive and send packet pools */
#define MAX_PACKET_POOL_SIZE 0x0000FFFF
#define MIN_PACKET_POOL_SIZE 0x000000FF

/** packet queue size used when the driver is working in the "active" mode */
#define PACKET_INFO_POOL_SIZE 0x0000FFFF

#ifndef VBOXNETADP
/** memory tag used for memory allocations
 * (VBNF stands for VBox NetFlt) */
# define MEM_TAG 'FNBV'
#else
/** memory tag used for memory allocations
 * (VBNA stands for VBox NetAdp) */
# define MEM_TAG 'ANBV'
#endif

/** receive and transmit Ndis buffer pool size */
#define TX_BUFFER_POOL_SIZE 128
#define RX_BUFFER_POOL_SIZE 128

#define ETH_HEADER_SIZE 14

#define PACKET_QUEUE_SG_SEGS_ALLOC 32

#define VBOX_NETFLT_PACKET_HEADER_MATCH_SIZE 24

#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
# define VBOXNETFLT_PACKETMATCH_LENGTH (ETH_HEADER_SIZE + 2)
#endif

#ifdef VBOXNETADP
#define     VBOXNETADP_HEADER_SIZE             14
#define     VBOXNETADP_MAX_DATA_SIZE           1500
#define     VBOXNETADP_MAX_PACKET_SIZE         VBOXNETADP_HEADER_SIZE + VBOXNETADP_MAX_DATA_SIZE
#define     VBOXNETADP_MIN_PACKET_SIZE         60
#define     VBOXNETADP_LINK_SPEED 1000000 //The unit of measurement is 100 bps, 100Mbps
#define VBOXNETADP_MAX_LOOKAHEAD_SIZE VBOXNETADP_MAX_DATA_SIZE
#define VBOXNETADP_VENDOR_ID 0x080027
#define VBOXNETADP_VENDOR_DRIVER_VERSION       0x00010000
#define VBOXNETADP_VENDOR_DESC "Sun"
#define VBOXNETADP_MAX_MCAST_LIST 32
#define VBOXNETADP_ETH_ADDRESS_LENGTH 6

//#define VBOXNETADP_REPORT_DISCONNECTED
#endif
/* type defs */

/** Flag specifying that the type of enqueued packet
 * if set the info contains the PINTNETSG packet
 * if clear the packet info contains the PNDIS_PACKET packet
 * Typically the packet queue we are maintaining contains PNDIS_PACKETs only,
 * however in case the underlying miniport indicates a packet with the NDIS_STATUS_RESOURCES status
 * we MUST return the packet back to the miniport immediately
 * this is why we are creating the INTNETSG, copying the ndis packet info there and enqueueing it */
#define PACKET_SG                   0x00000001

/** the flag specifying that the packet source
 * if set the packet comes from the host (upperlying protocol)
 * if clear the packet comes from the wire (underlying miniport) */
#define PACKET_SRC_HOST             0x00000002

/** flag specifying the packet was originated by our driver
 * i.e. we could use it on our needs and should not return it
 * we are enqueueing "our" packets on ProtocolReceive call-back when
 * Ndis does not give us a receive acket (the driver below us has called NdisM..IndicateReceive)
 * this is supported for Ndis Packet only */
#define PACKET_MINE                 0x00000004

/** flag passed to vboxNetFltWinQuEnqueuePacket specifying that the packet should be copied
 * this is supported for Ndis Packet only */
#define PACKET_COPY                 0x00000008

/** packet queue element containing the packet info */
typedef struct _PACKET_INFO
{
    /** list entry used for enqueueing the info */
    LIST_ENTRY ListEntry;
    /** pointer to the pool containing this packet info */
    struct _PACKET_INFO_POOL * pPool;
    /** flags describing the referenced packet. Contains PACKET_xxx flags (i.e. PACKET_SG, PACKET_SRC_HOST) */
    uint32_t fFlags;
    /** pointer to the packet this info represents */
    PVOID pPacket;
}PACKET_INFO, *PPACKET_INFO;

/* paranoid check to make sure the elements in the packet info array are properly aligned */
C_ASSERT((sizeof(PACKET_INFO) & (sizeof(PVOID) - 1)) == 0);

/** represents the packet queue */
typedef LIST_ENTRY PACKET_QUEUE, *PPACKET_QUEUE;

/*
 * we are using non-interlocked versions of LIST_ENTRY-related operations macros and synchronize
 * access to the queue and its elements by aquiring/releasing a spinlock using Ndis[Acquire,Release]Spinlock
 *
 * we are NOT using interlocked versions of insert/remove head/tail list functions because we need to iterate though
 * the queue elements as well as remove elements from the midle of the queue
 *
 * * @todo: it seems that we can switch to using interlocked versions of list-entry functions
 * since we have removed all functionality (mentioned above, i.e. queue elements iteration, etc.) that might prevent us from doing this
 */
typedef struct _INTERLOCKED_PACKET_QUEUE
{
    /** queue */
    PACKET_QUEUE Queue;
    /** queue lock */
    NDIS_SPIN_LOCK Lock;
}INTERLOCKED_PACKET_QUEUE, *PINTERLOCKED_PACKET_QUEUE;

typedef struct _SINGLE_LIST
{
    /** queue */
    SINGLE_LIST_ENTRY Head;
    /** pointer to the list tail. used to enqueue elements to the tail of the list */
    PSINGLE_LIST_ENTRY pTail;
} SINGLE_LIST, *PSINGLE_LIST;

typedef struct _INTERLOCKED_SINGLE_LIST
{
    /** queue */
    SINGLE_LIST List;
    /** queue lock */
    NDIS_SPIN_LOCK Lock;
} INTERLOCKED_SINGLE_LIST, *PINTERLOCKED_SINGLE_LIST;

/** packet info pool contains free packet info elements to be used for the packet queue
 * we are using the pool mechanism to allocate packet queue elements
 * the pool mechanism is pretty simple now, we are allocating a bunch of memory
 * for maintaining PACKET_INFO_POOL_SIZE queue elements and just returning null when the pool is exhausted
 * This mechanism seems to be enough for now since we are using PACKET_INFO_POOL_SIZE = 0xffff which is
 * the maximum size of packets the ndis packet pool supports */
typedef struct _PACKET_INFO_POOL
{
    /** free packet info queue */
    INTERLOCKED_PACKET_QUEUE Queue;
    /** memory bugger used by the pool */
    PVOID pBuffer;
}PACKET_INFO_POOL, *PPACKET_INFO_POOL;

typedef enum VBOXNETDEVOPSTATE
{
    kVBoxNetDevOpState_InvalidValue = 0,
    kVBoxNetDevOpState_Initializing,
    kVBoxNetDevOpState_Initialized,
    kVBoxNetDevOpState_Deinitializing,
    kVBoxNetDevOpState_Deinitialized,

} VBOXNETDEVOPSTATE;

typedef enum VBOXADAPTSTATE
{
   /** The usual invalid state. */
    kVBoxAdaptState_Invalid = 0,
    /** Initialization. */
    kVBoxAdaptState_Connecting,
    /** Connected fuly functional state */
    kVBoxAdaptState_Connected,
    /** Disconnecting  */
    kVBoxAdaptState_Disconnecting,
    /** Disconnected  */
    kVBoxAdaptState_Disconnected,
} VBOXADAPTSTATE;

/** structure used to maintain the state and reference count of the miniport and protocol */
typedef struct _ADAPT_DEVICE
{
    /** initialize state */
    VBOXNETDEVOPSTATE              OpState;
    /** ndis power state */
    NDIS_DEVICE_POWER_STATE        PowerState;
    /** reference count */
    uint32_t                       cReferences;
/*    NDIS_HANDLE                    hHandle; */
} ADAPT_DEVICE, *PADAPT_DEVICE;

/* packet filter processing mode constants */
#define VBOXNETFLT_PFP_NETFLT   1
#define VBOXNETFLT_PFP_PASSTHRU 2

/** represents filter driver device context*/
typedef struct _ADAPT
{
#ifndef VBOXNETADP
    /** handle the lower miniport */
    NDIS_HANDLE                    hBindingHandle;
    /** Protocol's Device state */
    ADAPT_DEVICE             PTState;
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    /** NDIS Handle to for miniport up-calls */
    NDIS_HANDLE                    hMiniportHandle;
    /** miniport device state */
    ADAPT_DEVICE             MPState;
    /** ndis packet pool used for receives */
    NDIS_HANDLE                    hRecvPacketPoolHandle;
    /** ndis buffer pool used for receives */
    NDIS_HANDLE                    hRecvBufferPoolHandle;
#ifndef VBOXNETADP
    /** This is used to wrap a request coming down to us.
     * This exploits the fact that requests are serialized down to us.*/
    NDIS_REQUEST                   Request;
    /** Ndis Request Bytes needed */
    PULONG                         BytesNeeded;
    /** Ndis Request Bytes Read or Written */
    PULONG                         BytesReadOrWritten;
#else
    volatile ULONG cTxSuccess;
    volatile ULONG cRxSuccess;
    volatile ULONG cTxError;
    volatile ULONG cRxError;
#endif
    /** driver bind adapter state. */
    VBOXADAPTSTATE                 enmState;
#ifndef VBOXNETADP
    /** true if we should indicate the receive complete used by the ProtocolReceeive mechanism */
    bool                        bIndicateRcvComplete;

    /** TRUE iff a request is pending at the miniport below */
    bool                        bOutstandingRequests;
    /**  TRUE iff a request is queued at this IM miniport*/
    bool                        bQueuedRequest;
    /** @todo join all boolean states to one field treated as flags bitmap */
    /** true iff we are processing Set packet filter OID */
    uint8_t                        fProcessingPacketFilter;
    /** true iff the upper protocol filter cache was initialized */
    bool                        bUpperProtSetFilterInitialized;
    /** trus if the adapter is closing */
    bool                        bClosingAdapter;
    /** Pending transfer data packet queue (i.e. packets that were indicated as pending on NdisTransferData call */
    INTERLOCKED_SINGLE_LIST     TransferDataList;
    /* mac options initialized on OID_GEN_MAC_OPTIONS */
    ULONG                          fMacOptions;
    /** For initializing the miniport edge */
    NDIS_STRING                    DeviceName;
    /** For blocking UnbindAdapter while an IM Init is in progress.*/
    NDIS_EVENT                     MiniportInitEvent;
    /** The last indicated media status */
    NDIS_STATUS                    LastIndicatedStatus;
    /** The latest suppressed media status */
    NDIS_STATUS                    LatestUnIndicateStatus;
    /** when working in the passthru mode the driver puts the received packets to this array
     * instead of passing them up immediately
     * we are flushing the packets on ProtocolReceiveComplete or when the underlying miniport
     * indicates NDIS_STATUS_RESOURCES or when this array is full */
    PNDIS_PACKET                   aReceivedPackets[MAX_RECEIVE_PACKET_ARRAY_SIZE];
    /** number of packets in the aReceivedPackets array*/
    ULONG                          cReceivedPacketCount;
    /** packet filter flags set by the upper protocols */
    ULONG                          fUpperProtocolSetFilter;
    /** packet filter flags set by the upper protocols */
    ULONG                          fSetFilterBuffer;
    /** packet filter flags set by us */
    ULONG                          fOurSetFilter;
#endif /* !VBOXNETADP */
#endif /* !VBOX_NETFLT_ONDEMAND_BIND */

#ifndef VBOXNETADP
#if defined(DEBUG_NETFLT_LOOPBACK) || !defined(VBOX_LOOPBACK_USEFLAGS)
    /** used for maintaining the pending send packets for handling packet loopback */
    INTERLOCKED_SINGLE_LIST SendPacketQueue;
#endif
    /** used for serializing calls to the NdisRequest in the vboxNetFltWinSynchNdisRequest */
    RTSEMFASTMUTEX                 hSynchRequestMutex;
    /** event used to synchronize with the Ndis Request completion in the vboxNetFltWinSynchNdisRequest */
    KEVENT                         hSynchCompletionEvent;
    /** status of the Ndis Request initiated by the vboxNetFltWinSynchNdisRequest */
    NDIS_STATUS   volatile         fSynchCompletionStatus;
    /** pointer to the Ndis Request being executed by the vboxNetFltWinSynchNdisRequest */
    PNDIS_REQUEST volatile         pSynchRequest;
    /** ndis packet pool used for sends */
    NDIS_HANDLE                    hSendPacketPoolHandle;
    /** ndis buffer pool used for sends */
    NDIS_HANDLE                    hSendBufferPoolHandle;
    /** open/close adapter status.
     * Since ndis adapter open and close requests may complete assynchronously,
     * we are using event mechanism to wait for open/close completion
     * the status field is being set by the completion call-back */
    NDIS_STATUS                    Status;
    /** open/close adaptor completion event */
    NDIS_EVENT                     hEvent;
    /** medium we are attached to */
    NDIS_MEDIUM                    Medium;
//    /** physical medium we are attached to */
//    NDIS_PHYSICAL_MEDIUM           PhMedium;
    /** True - When the miniport or protocol is transitioning from a D0 to Standby (>D0) State
     *  False - At all other times, - Flag is cleared after a transition to D0 */
    BOOLEAN                        bStandingBy;
#endif
} ADAPT, *PADAPT;

typedef struct _PACKET_QUEUE_WORKER
{
    /** this event is used to initiate a packet queue worker thread kill */
    KEVENT                         KillEvent;
    /** this event is used to notify a worker thread that the packets are added to the queue */
    KEVENT                         NotifyEvent;
    /** pointer to the packet queue worker thread object */
    PKTHREAD                       pThread;
    /** pointer to the SG used by the packet queue for IntNet receive notifications */
    PINTNETSG                      pSG;
    /** Packet queue */
    INTERLOCKED_PACKET_QUEUE       PacketQueue;
    /** Packet info pool, i.e. the pool for the packet queue elements */
    PACKET_INFO_POOL               PacketInfoPool;
} PACKET_QUEUE_WORKER, *PPACKET_QUEUE_WORKER;

/** Protocol reserved part of a sent packet that is allocated by us. */
typedef struct _SEND_RSVD
{
    /** original packet receiver from the upperlying protocol
     * can be null if the packet was originated by intnet */
    PNDIS_PACKET    pOriginalPkt;
    /** pointer to the buffer to be freed on send completion
     * can be null if no buffer is to be freed */
    PVOID           pBufToFree;
#if !defined(VBOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)
    SINGLE_LIST_ENTRY ListEntry;
    /* true if the packet is from IntNet */
    bool bFromIntNet;
#endif
} SEND_RSVD, *PSEND_RSVD;

/** represents the data stored in the protocol recerved field of ndis packet on NdisTransferData processing*/
typedef struct _TRANSFERDATA_RSVD
{
    /** next packet in a list */
    SINGLE_LIST_ENTRY ListEntry;
    /* packet buffer start */
    PNDIS_BUFFER pOriginalBuffer;
} TRANSFERDATA_RSVD, *PTRANSFERDATA_RSVD;

/** Miniport reserved part of a received packet that is allocated by
 * us. Note that this should fit into the MiniportReserved space
 * in an NDIS_PACKET. */
typedef struct _RECV_RSVD
{
    /** original packet receiver from the underling miniport
     * can be null if the packet was originated by intnet */
    PNDIS_PACKET    pOriginalPkt;
    /** pointer to the buffer to be freed on receive completion
     * can be null if no buffer is to be freed */
    PVOID           pBufToFree;
} RECV_RSVD, *PRECV_RSVD;

#ifndef VBOX_NETFLT_ONDEMAND_BIND

C_ASSERT(sizeof(RECV_RSVD) <= sizeof(((PNDIS_PACKET)0)->MiniportReserved));
C_ASSERT(sizeof(TRANSFERDATA_RSVD) <= PROTOCOL_RESERVED_SIZE_IN_PACKET);
#endif

C_ASSERT(sizeof(NDIS_DEVICE_POWER_STATE) == sizeof(uint32_t));
C_ASSERT(sizeof(UINT) == sizeof(uint32_t));

#ifdef VBOX_LOOPBACK_USEFLAGS
#define NDIS_FLAGS_SKIP_LOOPBACK_W2K    0x400
#endif

#include "../VBoxNetFltInternal.h"
#include "VBoxNetFlt-win.h"
#ifndef VBOXNETADP
#include "VBoxNetFltPt-win.h"
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
# include "VBoxNetFltMp-win.h"
#endif

#ifdef DEBUG_NETFLT_NOASSERT
# ifdef Assert
#  undef Assert
# endif

# define Assert(_expr) do {} while (0)
#endif /* #ifdef DEBUG_NETFLT_NOASSERT */

#endif
