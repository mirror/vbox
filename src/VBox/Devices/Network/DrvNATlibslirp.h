/** @file
 * DrvNATlibslirp - NATlibslirp network transport driver.
 */

/*
 * Copyright (C) 2022-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VBOX_INCLUDED_SRC_Network_DrvNATlibslirp_h
#define VBOX_INCLUDED_SRC_Network_DrvNATlibslirp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif
#endif

#define RTNET_INCL_IN_ADDR

#include "VBoxDD.h"

#include <libslirp.h>

#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pdmnetinline.h>

#ifndef RT_OS_WINDOWS
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>
# include <errno.h>
#endif
#ifdef RT_OS_FREEBSD
# include <netinet/in.h>
#endif

/** @todo r=jack: do we need to externally define inet* functions on win */
#ifdef RT_OS_WINDOWS
# include <iprt/win/winsock2.h>
# ifdef __cplusplus
extern "C" {
# endif
int inet_pton(int,const char *,void *);
# ifdef __cplusplus
}
# endif
# define inet_aton(x, y) inet_pton(2, x, y)
# define AF_INET6 23
#endif

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/cidr.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/net.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/uuid.h>

#include <iprt/asm.h>

#include <iprt/semaphore.h>
#include <iprt/req.h>
#ifdef RT_OS_DARWIN
# include <SystemConfiguration/SystemConfiguration.h>
# include <CoreFoundation/CoreFoundation.h>
#endif

#define COUNTERS_INIT
#include "slirp/counters.h"

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

#define DRVNAT_MAXFRAMESIZE (16 * 1024)

/**
 * @todo: This is a bad hack to prevent freezing the guest during high network
 *        activity. Windows host only. This needs to be fixed properly.
 */
#define VBOX_NAT_DELAY_HACK

/*
 * ICMP handle state change
 */
# define VBOX_ICMP_EVENT_INDEX           -1

/**
 * This event is for
 *  - slirp_input
 *  - slirp_link_up
 *  - slirp_link_down
 *  - wakeup
 */
# define VBOX_WAKEUP_EVENT_INDEX       0

/*
 * UDP/TCP socket state change (socket ready to receive, to send, ...)
 */
# define VBOX_SOCKET_EVENT_INDEX       1

/*
 * The number of events for WSAWaitForMultipleEvents().
 */
# define VBOX_EVENT_COUNT              2

#define GET_EXTRADATA(pdrvins, node, name, rc, type, type_name, var)                                  \
do {                                                                                                \
    (rc) = (pdrvins)->pHlpR3->pfnCFGMQuery ## type((node), name, &(var));                                               \
    if (RT_FAILURE((rc)) && (rc) != VERR_CFGM_VALUE_NOT_FOUND)                                      \
        return PDMDrvHlpVMSetError((pdrvins), (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \"" name "\" " #type_name " failed"), \
                                   (pdrvins)->iInstance);                                    \
} while (0)

#define GET_ED_STRICT(pdrvins, node, name, rc, type, type_name, var)                                  \
do {                                                                                                \
    (rc) = (pdrvins)->pHlpR3->pfnCFGMQuery ## type((node), name, &(var));                                               \
    if (RT_FAILURE((rc)))                                                                           \
        return PDMDrvHlpVMSetError((pdrvins), (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \"" name "\" " #type_name " failed"), \
                                  (pdrvins)->iInstance);                                     \
} while (0)

#define GET_EXTRADATA_N(pdrvins, node, name, rc, type, type_name, var, var_size)                      \
do {                                                                                                \
    (rc) = (pdrvins)->pHlpR3->pfnCFGMQuery ## type((node), name, &(var), var_size);                                     \
    if (RT_FAILURE((rc)) && (rc) != VERR_CFGM_VALUE_NOT_FOUND)                                      \
        return PDMDrvHlpVMSetError((pdrvins), (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \"" name "\" " #type_name " failed"), \
                                  (pdrvins)->iInstance);                                     \
} while (0)

#define GET_BOOL(rc, pdrvins, node, name, var) \
    GET_EXTRADATA(pdrvins, node, name, (rc), Bool, bolean, (var))
#define GET_STRING(rc, pdrvins, node, name, var, var_size) \
    GET_EXTRADATA_N(pdrvins, node, name, (rc), String, string, (var), (var_size))
#define GET_STRING_ALLOC(rc, pdrvins, node, name, var) \
    GET_EXTRADATA(pdrvins, node, name, (rc), StringAlloc, string, (var))
#define GET_S32(rc, pdrvins, node, name, var) \
    GET_EXTRADATA(pdrvins, node, name, (rc), S32, int, (var))
#define GET_S32_STRICT(rc, pdrvins, node, name, var) \
    GET_ED_STRICT(pdrvins, node, name, (rc), S32, int, (var))

#define DO_GET_IP(rc, node, instance, status, x)                                \
do {                                                                            \
    char    sz##x[32];                                                          \
    GET_STRING((rc), (node), (instance), #x, sz ## x[0],  sizeof(sz ## x));     \
    if (rc != VERR_CFGM_VALUE_NOT_FOUND)                                        \
        (status) = inet_aton(sz ## x, &x);                                      \
} while (0)

#define GETIP_DEF(rc, node, instance, x, def)           \
do                                                      \
{                                                       \
    int status = 0;                                     \
    DO_GET_IP((rc), (node), (instance),  status, x);    \
    if (status == 0 || rc == VERR_CFGM_VALUE_NOT_FOUND) \
        x.s_addr = def;                                 \
} while (0)

// timer struct
typedef struct slirpTimer {
    struct slirpTimer *next;
    int64_t uTimeExpire;
    SlirpTimerCb pHandler;
    void *opaque;
} SlirpTimer;

/**
 * Main state of Libslirp NAT
 */
typedef struct SlirpState
{
    unsigned int nsock;
# ifndef RT_OS_WINDOWS
    /* counter of sockets needed for allocation enough room to
     * process sockets with poll/epoll
     *
     * NSOCK_INC/DEC should be injected before every
     * operation on socket queue (tcb, udb)
     */
#  define NSOCK_INC() do {pData->nsock++;} while (0)
#  define NSOCK_DEC() do {pData->nsock--;} while (0)
#  define NSOCK_INC_EX(ex) do {ex->pData->nsock++;} while (0)
#  define NSOCK_DEC_EX(ex) do {ex->pData->nsock--;} while (0)
# else
#  define NSOCK_INC() do {} while (0)
#  define NSOCK_DEC() do {} while (0)
#  define NSOCK_INC_EX(ex) do {} while (0)
#  define NSOCK_DEC_EX(ex) do {} while (0)
# endif

#if defined(RT_OS_WINDOWS)
# define VBOX_SOCKET_EVENT (pData->phEvents[VBOX_SOCKET_EVENT_INDEX])
    HANDLE phEvents[VBOX_EVENT_COUNT];
#endif

    Slirp *pSlirp;
    struct pollfd *polls;

    // Num Polls (not bytes)
    unsigned int uPollCap = 0;

    SlirpTimer *pTimerHead;
} SlirpState;
typedef SlirpState *pSlirpState;

/**
 * NAT network transport driver instance data.
 *
 * @implements  PDMINETWORKUP
 */
typedef struct DRVNAT
{
    /** The network interface. */
    PDMINETWORKUP           INetworkUp;
    /** The network NAT Engine configuration. */
    PDMINETWORKNATCONFIG    INetworkNATCfg;
    /** The port we're attached to. */
    PPDMINETWORKDOWN        pIAboveNet;
    /** The network config of the port we're attached to. */
    PPDMINETWORKCONFIG      pIAboveConfig;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Link state */
    PDMNETWORKLINKSTATE     enmLinkState;
    /** NAT state */
    pSlirpState             pNATState;
    /** TFTP directory prefix. */
    char                   *pszTFTPPrefix;
    /** Boot file name to provide in the DHCP server response. */
    char                   *pszBootFile;
    /** tftp server name to provide in the DHCP server response. */
    char                   *pszNextServer;
    /** Polling thread. */
    PPDMTHREAD              pSlirpThread;
    /** Queue for NAT-thread-external events. */
    RTREQQUEUE              hSlirpReqQueue;
    /** The guest IP for port-forwarding. */
    uint32_t                GuestIP;
    /** Link state set when the VM is suspended. */
    PDMNETWORKLINKSTATE     enmLinkStateWant;

#ifndef RT_OS_WINDOWS
    /** The write end of the control pipe. */
    RTPIPE                  hPipeWrite;
    /** The read end of the control pipe. */
    RTPIPE                  hPipeRead;
# if HC_ARCH_BITS == 32
    uint32_t                u32Padding;
# endif
#else
    /** for external notification */
    HANDLE                  hWakeupEvent;
#endif

#define DRV_PROFILE_COUNTER(name, dsc)     STAMPROFILE Stat ## name
#define DRV_COUNTING_COUNTER(name, dsc)    STAMCOUNTER Stat ## name
#include "slirp/counters.h"
    /** thread delivering packets for receiving by the guest */
    PPDMTHREAD              pRecvThread;
    /** thread delivering urg packets for receiving by the guest */
    PPDMTHREAD              pUrgRecvThread;
    /** event to wakeup the guest receive thread */
    RTSEMEVENT              EventRecv;
    /** event to wakeup the guest urgent receive thread */
    RTSEMEVENT              EventUrgRecv;
    /** Receive Req queue (deliver packets to the guest) */
    RTREQQUEUE              hRecvReqQueue;
    /** Receive Urgent Req queue (deliver packets to the guest). */
    RTREQQUEUE              hUrgRecvReqQueue;

    /** makes access to device func RecvAvail and Recv atomical. */
    RTCRITSECT              DevAccessLock;
    /** Number of in-flight urgent packets. */
    volatile uint32_t       cUrgPkts;
    /** Number of in-flight regular packets. */
    volatile uint32_t       cPkts;

    /** Transmit lock taken by BeginXmit and released by EndXmit. */
    RTCRITSECT              XmitLock;

#ifdef RT_OS_DARWIN
    /* Handle of the DNS watcher runloop source. */
    CFRunLoopSourceRef      hRunLoopSrcDnsWatcher;
#endif /* !VBOX_INCLUDED_SRC_Network_DrvNATlibslirp_h */
} DRVNAT;
// AssertCompileMemberAlignment(DRVNAT, StatNATRecvWakeups, 8);
/** Pointer to the NAT driver instance data. */
typedef DRVNAT *PDRVNAT;

static DECLCALLBACK(int) drvNATRecv(PPDMDRVINS, PPDMTHREAD);
static DECLCALLBACK(int) drvNATRecvWakeup(PPDMDRVINS, PPDMTHREAD);
static DECLCALLBACK(int) drvNATUrgRecvWakeup(PPDMDRVINS, PPDMTHREAD);
static DECLCALLBACK(void) drvNATRecvWorker(PDRVNAT, void *, int);
static void drvNATFreeSgBuf(PDRVNAT, PPDMSCATTERGATHER);
static DECLCALLBACK(void) drvNATSendWorker(PDRVNAT, PPDMSCATTERGATHER);
static DECLCALLBACK(int) drvNATNetworkUp_AllocBuf(PPDMINETWORKUP, size_t,
                                                  PCPDMNETWORKGSO, PPPDMSCATTERGATHER);
static DECLCALLBACK(int) drvNATNetworkUp_FreeBuf(PPDMINETWORKUP, PPDMSCATTERGATHER);
static DECLCALLBACK(int) drvNATNetworkUp_SendBuf(PPDMINETWORKUP, PPDMSCATTERGATHER, bool);
static DECLCALLBACK(void) drvNATNetworkUp_EndXmit(PPDMINETWORKUP );
static void drvNATNotifyNATThread(PDRVNAT pThis, const char *);
static DECLCALLBACK(void) drvNATNetworkUp_SetPromiscuousMode(PPDMINETWORKUP, bool);
static DECLCALLBACK(void) drvNATNotifyLinkChangedWorker(PDRVNAT, PDMNETWORKLINKSTATE);
static DECLCALLBACK(void) drvNATNetworkUp_NotifyLinkChanged(PPDMINETWORKUP, PDMNETWORKLINKSTATE);
static DECLCALLBACK(int) drvNATAsyncIoThread(PPDMDRVINS, PPDMTHREAD);
static DECLCALLBACK(int) drvNATAsyncIoWakeup(PPDMDRVINS, PPDMTHREAD);
static DECLCALLBACK(void *) drvNATQueryInterface(PPDMIBASE, const char *);
static DECLCALLBACK(void) drvNATInfo(PPDMDRVINS, PCDBGFINFOHLP, const char *);

static void slirpUpdateTimeout(uint32_t *, void *);
static void slirpCheckTimeout(void *);

static DECLCALLBACK(ssize_t) slirpSendPacketCb(const void *, size_t, void *);
static DECLCALLBACK(void) slirpGuestErrorCb(const char *, void *);
static DECLCALLBACK(int64_t) slirpClockGetNsCb(void *);
static DECLCALLBACK(void *) slirpTimerNewCb(SlirpTimerCb, void *, void *);
static DECLCALLBACK(void) slirpTimerFreeCb(void *, void *);
static DECLCALLBACK(void) slirpTimerModCb(void *, int64_t, void *);

static DECLCALLBACK(void) drvNATDestruct(PPDMDRVINS);
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS, PCFGMNODE, uint32_t);
