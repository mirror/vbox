/* $Id$ */
/** @file
 * VBoxNetLwf-win.cpp - NDIS6 Bridged Networking Driver, Windows-specific code.
 */
/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV

//#define VBOXNETLWF_SYNC_SEND

#include <VBox/version.h>
#include <VBox/err.h>
#include <iprt/initterm.h>
#include <iprt/net.h>
#include <iprt/list.h>
#include <VBox/intnetinline.h>

/// @todo Not sure why, but can it help with build errors?
RT_C_DECLS_BEGIN
/* ntddk.h has a missing #pragma pack(), work around it
 * see #ifdef VBOX_WITH_WORKAROUND_MISSING_PACK below for detail */
#define VBOX_WITH_WORKAROUND_MISSING_PACK
#if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#  define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#  define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#  define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#  define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#  define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#  define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#  define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#  define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#  pragma warning(disable : 4163)
#  ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#    pragma warning(disable : 4103)
#  endif
#  include <ntddk.h>
#  pragma warning(default : 4163)
#  ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#    pragma pack()
#    pragma warning(default : 4103)
#  endif
#  undef  _InterlockedExchange
#  undef  _InterlockedExchangeAdd
#  undef  _InterlockedCompareExchange
#  undef  _InterlockedAddLargeStatistic
#  undef  _interlockedbittestandset
#  undef  _interlockedbittestandreset
#  undef  _interlockedbittestandset64
#  undef  _interlockedbittestandreset64
#  include <ndis.h>
#else
//#  include <ntddk.h>
/* can include ndis.h right away */
#  include <ndis.h>
#endif
RT_C_DECLS_END

#if 0
#undef Log
#define Log(x) DbgPrint x
#undef LogFlow
#define LogFlow(x) DbgPrint x
#endif

/** We have an entirely different structure than the one defined in VBoxNetFltCmn-win.h */
typedef struct VBOXNETFLTWIN
{
    /** filter module context handle */
    NDIS_HANDLE hModuleCtx;
} VBOXNETFLTWIN, *PVBOXNETFLTWIN;
#define VBOXNETFLT_NO_PACKET_QUEUE
#define VBOXNETFLT_OS_SPECFIC 1
#include "VBoxNetFltInternal.h"

#include "VBoxNetLwf-win.h"
#include "VBoxNetCmn-win.h"

/* Forward declarations */
FILTER_ATTACH vboxNetLwfWinAttach;
FILTER_DETACH vboxNetLwfWinDetach;
FILTER_RESTART vboxNetLwfWinRestart;
FILTER_PAUSE vboxNetLwfWinPause;
FILTER_OID_REQUEST vboxNetLwfWinOidRequest;
FILTER_OID_REQUEST_COMPLETE vboxNetLwfWinOidRequestComplete;
//FILTER_CANCEL_OID_REQUEST vboxNetLwfWinCancelOidRequest;
FILTER_STATUS vboxNetLwfWinStatus;
FILTER_SET_MODULE_OPTIONS vboxNetLwfWinSetModuleOptions;
//FILTER_NET_PNP_EVENT vboxNetLwfWinPnPEvent;
FILTER_SEND_NET_BUFFER_LISTS vboxNetLwfWinSendNetBufferLists;
FILTER_SEND_NET_BUFFER_LISTS_COMPLETE vboxNetLwfWinSendNetBufferListsComplete;
FILTER_RECEIVE_NET_BUFFER_LISTS vboxNetLwfWinReceiveNetBufferLists;
FILTER_RETURN_NET_BUFFER_LISTS vboxNetLwfWinReturnNetBufferLists;
KSTART_ROUTINE vboxNetLwfWinInitIdcWorker;

typedef enum {
    LwfState_Detached = 0,
    LwfState_Attaching,
    LwfState_Paused,
    LwfState_Restarting,
    LwfState_Running,
    LwfState_Pausing,
    LwfState_32BitHack = 0x7fffffff
} VBOXNETLWFSTATE;

/*
 * Valid state transitions are:
 * 1) Disconnected -> Connecting   : start the worker thread, attempting to init IDC;
 * 2) Connecting   -> Disconnected : failed to start IDC init worker thread;
 * 3) Connecting   -> Connected    : IDC init successful, terminate the worker;
 * 4) Connecting   -> Stopping     : IDC init incomplete, but the driver is being unloaded, terminate the worker;
 * 5) Connected    -> Stopping     : IDC init was successful, no worker, the driver is being unloaded;
 *
 * Driver terminates in Stopping state.
 */
typedef enum {
    LwfIdcState_Disconnected = 0, /* Initial state */
    LwfIdcState_Connecting,       /* Attemping to init IDC, worker thread running */
    LwfIdcState_Connected,        /* Successfully connected to IDC, worker thread terminated */
    LwfIdcState_Stopping          /* Terminating the worker thread and disconnecting IDC */
} VBOXNETLWFIDCSTATE;

struct _VBOXNETLWF_MODULE;

typedef struct VBOXNETLWFGLOBALS
{
    /** synch event used for device creation synchronization */
    //KEVENT SynchEvent;
    /** Device reference count */
    //int cDeviceRefs;
    /** ndis device */
    NDIS_HANDLE hDevice;
    /** device object */
    PDEVICE_OBJECT pDevObj;
    /** our filter driver handle */
    NDIS_HANDLE hFilterDriver;
    /** lock protecting the module list */
    NDIS_SPIN_LOCK Lock;
    /** the head of module list */
    RTLISTANCHOR listModules;
    /** IDC initialization state */
    volatile uint32_t enmIdcState;
    /** IDC init thread handle */
    HANDLE hInitIdcThread;
} VBOXNETLWFGLOBALS, *PVBOXNETLWFGLOBALS;

/**
 * The (common) global data.
 */
static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;
/* win-specific global data */
VBOXNETLWFGLOBALS g_VBoxNetLwfGlobals;

typedef struct _VBOXNETLWF_MODULE {
    RTLISTNODE node;

    NDIS_HANDLE hFilter;
    NDIS_HANDLE hPool;
    PVBOXNETLWFGLOBALS pGlobals;
    /** Associated instance of NetFlt, one-to-one relationship */
    PVBOXNETFLTINS pNetFlt; /// @todo Consider automic access!
    /** Module state as described in http://msdn.microsoft.com/en-us/library/windows/hardware/ff550017(v=vs.85).aspx */
    volatile uint32_t enmState; /* No lock needed yet, atomic should suffice. */
    /** Mutex to prevent pausing while transmitting on behalf of NetFlt */
    NDIS_MUTEX InTransmit;
#ifdef VBOXNETLWF_SYNC_SEND
    /** Event signalled when sending to the wire is complete */
    KEVENT EventWire;
    /** Event signalled when NDIS returns our receive notification */
    KEVENT EventHost;
#else /* !VBOXNETLWF_SYNC_SEND */
    /** Event signalled when all pending sends (both to wire and host) have completed */
    NDIS_EVENT EventSendComplete;
    /** Counter for pending sends (both to wire and host) */
    int32_t cPendingBuffers;
#endif /* !VBOXNETLWF_SYNC_SEND */
    /** Name of underlying adapter */
    ANSI_STRING strMiniportName;
    /** MAC address of underlying adapter */
    RTMAC MacAddr;
    /** Packet filter of underlying miniport */
    ULONG uPacketFilter;
    /** Saved offload configuration */
    NDIS_OFFLOAD SavedOffloadConfig;
    /** the cloned request we have passed down */
    PNDIS_OID_REQUEST pPendingRequest;
    /** true if the underlying miniport supplied offloading config */
    bool fOffloadConfigValid;
    /** true if the trunk expects data from us */
    bool fActive;
} VBOXNETLWF_MODULE;
typedef VBOXNETLWF_MODULE *PVBOXNETLWF_MODULE;

/*
 * A structure to wrap OID requests in.
 */
typedef struct _VBOXNETLWF_OIDREQ {
    NDIS_OID_REQUEST Request;
    NDIS_STATUS Status;
    NDIS_EVENT Event;
} VBOXNETLWF_OIDREQ;
typedef VBOXNETLWF_OIDREQ *PVBOXNETLWF_OIDREQ;

/* Forward declarations */
static VOID vboxNetLwfWinUnloadDriver(IN PDRIVER_OBJECT pDriver);
static int vboxNetLwfWinInitBase();
static int vboxNetLwfWinFini();

#ifdef DEBUG
static const char *vboxNetLwfWinStatusToText(NDIS_STATUS code)
{
    switch (code)
    {
        case NDIS_STATUS_MEDIA_CONNECT: return "NDIS_STATUS_MEDIA_CONNECT";
        case NDIS_STATUS_MEDIA_DISCONNECT: return "NDIS_STATUS_MEDIA_DISCONNECT";
        case NDIS_STATUS_RESET_START: return "NDIS_STATUS_RESET_START";
        case NDIS_STATUS_RESET_END: return "NDIS_STATUS_RESET_END";
        case NDIS_STATUS_MEDIA_BUSY: return "NDIS_STATUS_MEDIA_BUSY";
        case NDIS_STATUS_MEDIA_SPECIFIC_INDICATION: return "NDIS_STATUS_MEDIA_SPECIFIC_INDICATION";
        case NDIS_STATUS_LINK_SPEED_CHANGE: return "NDIS_STATUS_LINK_SPEED_CHANGE";
        case NDIS_STATUS_LINK_STATE: return "NDIS_STATUS_LINK_STATE";
        case NDIS_STATUS_PORT_STATE: return "NDIS_STATUS_PORT_STATE";
        case NDIS_STATUS_OPER_STATUS: return "NDIS_STATUS_OPER_STATUS";
        case NDIS_STATUS_NETWORK_CHANGE: return "NDIS_STATUS_NETWORK_CHANGE";
        case NDIS_STATUS_PACKET_FILTER: return "NDIS_STATUS_PACKET_FILTER";
        case NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG: return "NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG";
        case NDIS_STATUS_TASK_OFFLOAD_HARDWARE_CAPABILITIES: return "NDIS_STATUS_TASK_OFFLOAD_HARDWARE_CAPABILITIES";
        case NDIS_STATUS_OFFLOAD_ENCASPULATION_CHANGE: return "NDIS_STATUS_OFFLOAD_ENCASPULATION_CHANGE";
        case NDIS_STATUS_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES: return "NDIS_STATUS_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES";
    }
    return "unknown";
}

static void vboxNetLwfWinDumpFilterTypes(ULONG uFlags)
{
    if (uFlags & NDIS_PACKET_TYPE_DIRECTED) Log5(("   NDIS_PACKET_TYPE_DIRECTED\n"));
    if (uFlags & NDIS_PACKET_TYPE_MULTICAST) Log5(("   NDIS_PACKET_TYPE_MULTICAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_MULTICAST) Log5(("   NDIS_PACKET_TYPE_ALL_MULTICAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_BROADCAST) Log5(("   NDIS_PACKET_TYPE_BROADCAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_PROMISCUOUS) Log5(("   NDIS_PACKET_TYPE_PROMISCUOUS\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) Log5(("   NDIS_PACKET_TYPE_ALL_FUNCTIONAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_LOCAL) Log5(("   NDIS_PACKET_TYPE_ALL_LOCAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_FUNCTIONAL) Log5(("   NDIS_PACKET_TYPE_FUNCTIONAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_GROUP) Log5(("   NDIS_PACKET_TYPE_GROUP\n"));
    if (uFlags & NDIS_PACKET_TYPE_MAC_FRAME) Log5(("   NDIS_PACKET_TYPE_MAC_FRAME\n"));
    if (uFlags & NDIS_PACKET_TYPE_SMT) Log5(("   NDIS_PACKET_TYPE_SMT\n"));
    if (uFlags & NDIS_PACKET_TYPE_SOURCE_ROUTING) Log5(("   NDIS_PACKET_TYPE_SOURCE_ROUTING\n"));
    if (uFlags == 0) Log5(("   NONE\n"));
}

DECLINLINE(void) vboxNetLwfWinDumpEncapsulation(const char *pcszText, ULONG uEncapsulation)
{
    if (uEncapsulation == NDIS_ENCAPSULATION_NOT_SUPPORTED)
        Log5(("%s not supported\n", pcszText));
    else
    {
        Log5(("%s", pcszText));
        if (uEncapsulation & NDIS_ENCAPSULATION_NULL)
            Log5((" null"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3)
            Log5((" 802.3"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q)
            Log5((" 802.3pq"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q_IN_OOB)
            Log5((" 802.3pq(oob)"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_LLC_SNAP_ROUTED)
            Log5((" LLC"));
        Log5(("\n"));
    }
}

DECLINLINE(const char *) vboxNetLwfWinSetOnOffText(ULONG uOnOff)
{
    switch (uOnOff)
    {
        case NDIS_OFFLOAD_SET_NO_CHANGE: return "no change";
        case NDIS_OFFLOAD_SET_ON: return "on";
        case NDIS_OFFLOAD_SET_OFF: return "off";
    }
    return "unknown";
}

DECLINLINE(const char *) vboxNetLwfWinOnOffText(ULONG uOnOff)
{
    switch (uOnOff)
    {
        case NDIS_OFFLOAD_NOT_SUPPORTED: return "off";
        case NDIS_OFFLOAD_SUPPORTED: return "on";
    }
    return "unknown";
}

DECLINLINE(const char *) vboxNetLwfWinSupportedText(ULONG uSupported)
{
    switch (uSupported)
    {
        case NDIS_OFFLOAD_NOT_SUPPORTED: return "not supported";
        case NDIS_OFFLOAD_SUPPORTED: return "supported";
    }
    return "unknown";
}

static void vboxNetLwfWinDumpSetOffloadSettings(PNDIS_OFFLOAD pOffloadConfig)
{
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv4Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv4Transmit.Encapsulation);
    Log5(("   Checksum.IPv4Transmit.IpOptionsSupported          = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpOptionsSupported         = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpChecksum                 = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv4Transmit.UdpChecksum                 = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.UdpChecksum)));
    Log5(("   Checksum.IPv4Transmit.IpChecksum                  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv4Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv4Receive.Encapsulation);
    Log5(("   Checksum.IPv4Receive.IpOptionsSupported           = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpOptionsSupported          = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpChecksum                  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpChecksum)));
    Log5(("   Checksum.IPv4Receive.UdpChecksum                  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.UdpChecksum)));
    Log5(("   Checksum.IPv4Receive.IpChecksum                   = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv6Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv6Transmit.Encapsulation);
    Log5(("   Checksum.IPv6Transmit.IpExtensionHeadersSupported = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpOptionsSupported         = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpChecksum                 = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv6Transmit.UdpChecksum                 = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.UdpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv6Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv6Receive.Encapsulation);
    Log5(("   Checksum.IPv6Receive.IpExtensionHeadersSupported  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Receive.TcpOptionsSupported          = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Receive.TcpChecksum                  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpChecksum)));
    Log5(("   Checksum.IPv6Receive.UdpChecksum                  = %s\n", vboxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.UdpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   LsoV1.IPv4.Encapsulation                          =", pOffloadConfig->LsoV1.IPv4.Encapsulation);
    Log5(("   LsoV1.IPv4.TcpOptions                             = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.TcpOptions)));
    Log5(("   LsoV1.IPv4.IpOptions                              = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.IpOptions)));
    vboxNetLwfWinDumpEncapsulation("   LsoV2.IPv4.Encapsulation                          =", pOffloadConfig->LsoV2.IPv4.Encapsulation);
    vboxNetLwfWinDumpEncapsulation("   LsoV2.IPv6.Encapsulation                          =", pOffloadConfig->LsoV2.IPv6.Encapsulation);
    Log5(("   LsoV2.IPv6.IpExtensionHeadersSupported            = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.IpExtensionHeadersSupported)));
    Log5(("   LsoV2.IPv6.TcpOptionsSupported                    = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.TcpOptionsSupported)));
}

static void vboxNetLwfWinDumpOffloadSettings(PNDIS_OFFLOAD pOffloadConfig)
{
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv4Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv4Transmit.Encapsulation);
    Log5(("   Checksum.IPv4Transmit.IpOptionsSupported          = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpOptionsSupported         = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpChecksum                 = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv4Transmit.UdpChecksum                 = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.UdpChecksum)));
    Log5(("   Checksum.IPv4Transmit.IpChecksum                  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv4Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv4Receive.Encapsulation);
    Log5(("   Checksum.IPv4Receive.IpOptionsSupported           = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpOptionsSupported          = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpChecksum                  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpChecksum)));
    Log5(("   Checksum.IPv4Receive.UdpChecksum                  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.UdpChecksum)));
    Log5(("   Checksum.IPv4Receive.IpChecksum                   = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv6Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv6Transmit.Encapsulation);
    Log5(("   Checksum.IPv6Transmit.IpExtensionHeadersSupported = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpOptionsSupported         = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpChecksum                 = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv6Transmit.UdpChecksum                 = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.UdpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   Checksum.IPv6Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv6Receive.Encapsulation);
    Log5(("   Checksum.IPv6Receive.IpExtensionHeadersSupported  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Receive.TcpOptionsSupported          = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Receive.TcpChecksum                  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpChecksum)));
    Log5(("   Checksum.IPv6Receive.UdpChecksum                  = %s\n", vboxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.UdpChecksum)));
    vboxNetLwfWinDumpEncapsulation("   LsoV1.IPv4.Encapsulation                          =", pOffloadConfig->LsoV1.IPv4.Encapsulation);
    Log5(("   LsoV1.IPv4.TcpOptions                             = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.TcpOptions)));
    Log5(("   LsoV1.IPv4.IpOptions                              = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.IpOptions)));
    vboxNetLwfWinDumpEncapsulation("   LsoV2.IPv4.Encapsulation                          =", pOffloadConfig->LsoV2.IPv4.Encapsulation);
    vboxNetLwfWinDumpEncapsulation("   LsoV2.IPv6.Encapsulation                          =", pOffloadConfig->LsoV2.IPv6.Encapsulation);
    Log5(("   LsoV2.IPv6.IpExtensionHeadersSupported            = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.IpExtensionHeadersSupported)));
    Log5(("   LsoV2.IPv6.TcpOptionsSupported                    = %s\n", vboxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.TcpOptionsSupported)));
}

static const char *vboxNetLwfWinStateToText(uint32_t enmState)
{
    switch (enmState)
    {
        case LwfState_Detached: return "Detached";
        case LwfState_Attaching: return "Attaching";
        case LwfState_Paused: return "Paused";
        case LwfState_Restarting: return "Restarting";
        case LwfState_Running: return "Running";
        case LwfState_Pausing: return "Pausing";
    }
    return "invalid";
}

#else /* !DEBUG */
#define vboxNetLwfWinDumpFilterTypes(uFlags)
#define vboxNetLwfWinDumpOffloadSettings(p)
#define vboxNetLwfWinDumpSetOffloadSettings(p)
#endif /* DEBUG */

DECLINLINE(bool) vboxNetLwfWinChangeState(PVBOXNETLWF_MODULE pModuleCtx, uint32_t enmNew, uint32_t enmOld = LwfState_32BitHack)
{
    AssertReturn(pModuleCtx, false);

    bool fSuccess = true;
    if (enmOld != LwfState_32BitHack)
    {
        fSuccess = ASMAtomicCmpXchgU32(&pModuleCtx->enmState, enmNew, enmOld);
        if (fSuccess)
            Log((__FUNCTION__": state change %s -> %s\n",
                 vboxNetLwfWinStateToText(enmOld),
                 vboxNetLwfWinStateToText(enmNew)));
        else
            Log((__FUNCTION__": failed state change %s (actual=%s) -> %s\n",
                 vboxNetLwfWinStateToText(enmOld),
                 vboxNetLwfWinStateToText(ASMAtomicReadU32(&pModuleCtx->enmState)),
                 vboxNetLwfWinStateToText(enmNew)));
        Assert(fSuccess);
    }
    else
    {
        uint32_t enmPrevState = ASMAtomicXchgU32(&pModuleCtx->enmState, enmNew);
        Log((__FUNCTION__": state change %s -> %s\n",
             vboxNetLwfWinStateToText(enmPrevState),
             vboxNetLwfWinStateToText(enmNew)));
    }
    return fSuccess;
}

DECLINLINE(void) vboxNetLwfWinInitOidRequest(PVBOXNETLWF_OIDREQ pRequest)
{
    NdisZeroMemory(pRequest, sizeof(VBOXNETLWF_OIDREQ));

    NdisInitializeEvent(&pRequest->Event);

    pRequest->Request.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
    pRequest->Request.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
    pRequest->Request.Header.Size = NDIS_SIZEOF_OID_REQUEST_REVISION_1;

    pRequest->Request.RequestId = (PVOID)VBOXNETLWF_REQ_ID;
}

static NDIS_STATUS vboxNetLwfWinSyncOidRequest(PVBOXNETLWF_MODULE pModuleCtx, PVBOXNETLWF_OIDREQ pRequest)
{
    NDIS_STATUS Status = NdisFOidRequest(pModuleCtx->hFilter, &pRequest->Request);
    if (Status == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pRequest->Event, 0);
        Status = pRequest->Status;
    }
    return Status;
}

DECLINLINE(void) vboxNetLwfWinCopyOidRequestResults(PNDIS_OID_REQUEST pFrom, PNDIS_OID_REQUEST pTo)
{
    switch (pFrom->RequestType)
    {
        case NdisRequestSetInformation:
            pTo->DATA.SET_INFORMATION.BytesRead   = pFrom->DATA.SET_INFORMATION.BytesRead;
            pTo->DATA.SET_INFORMATION.BytesNeeded = pFrom->DATA.SET_INFORMATION.BytesNeeded;
            break;
        case NdisRequestMethod:
            pTo->DATA.METHOD_INFORMATION.OutputBufferLength = pFrom->DATA.METHOD_INFORMATION.OutputBufferLength;
            pTo->DATA.METHOD_INFORMATION.BytesWritten       = pFrom->DATA.METHOD_INFORMATION.BytesWritten;
            pTo->DATA.METHOD_INFORMATION.BytesRead          = pFrom->DATA.METHOD_INFORMATION.BytesRead;
            pTo->DATA.METHOD_INFORMATION.BytesNeeded        = pFrom->DATA.METHOD_INFORMATION.BytesNeeded;
            break;
        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
        default:
            pTo->DATA.QUERY_INFORMATION.BytesWritten = pFrom->DATA.QUERY_INFORMATION.BytesWritten;
            pTo->DATA.QUERY_INFORMATION.BytesNeeded  = pFrom->DATA.QUERY_INFORMATION.BytesNeeded;
    }
}

NDIS_STATUS vboxNetLwfWinOidRequest(IN NDIS_HANDLE hModuleCtx,
                                    IN PNDIS_OID_REQUEST pOidRequest)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    vboxNetCmnWinDumpOidRequest(__FUNCTION__, pOidRequest);
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    PNDIS_OID_REQUEST pClone = NULL;
    NDIS_STATUS Status = NdisAllocateCloneOidRequest(pModuleCtx->hFilter,
                                                     pOidRequest,
                                                     VBOXNETLWF_MEM_TAG,
                                                     &pClone);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        /* Save the pointer to the original */
        *((PNDIS_OID_REQUEST*)(pClone->SourceReserved)) = pOidRequest;

        pClone->RequestId = pOidRequest->RequestId;
        /* We are not supposed to get another request until we are through with the one we "postponed" */
        PNDIS_OID_REQUEST pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, pClone, PNDIS_OID_REQUEST);
        Assert(pPrev == NULL);
        pModuleCtx->pPendingRequest = pClone;
        if (pOidRequest->RequestType == NdisRequestSetInformation
            && pOidRequest->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER)
        {
            ASMAtomicWriteU32((uint32_t*)&pModuleCtx->uPacketFilter, *(ULONG*)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
            Log((__FUNCTION__": updated cached packet filter value to:\n"));
            vboxNetLwfWinDumpFilterTypes(*(ULONG*)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
        }
        if (pOidRequest->RequestType == NdisRequestSetInformation
            && pOidRequest->DATA.SET_INFORMATION.Oid == OID_TCP_OFFLOAD_CURRENT_CONFIG)
        {
            Log5((__FUNCTION__": offloading set to:\n"));
            vboxNetLwfWinDumpSetOffloadSettings((PNDIS_OFFLOAD)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
        }

        /* Forward the clone to underlying filters/miniport */
        Status = NdisFOidRequest(pModuleCtx->hFilter, pClone);
        if (Status != NDIS_STATUS_PENDING)
        {
            /* Synchronous completion */
            pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, NULL, PNDIS_OID_REQUEST);
            Assert(pPrev == pClone);
            vboxNetLwfWinCopyOidRequestResults(pClone, pOidRequest);
            NdisFreeCloneOidRequest(pModuleCtx->hFilter, pClone);
        }
        /* In case of async completion we do the rest in vboxNetLwfWinOidRequestComplete() */
    }
    else
    {
        Log((__FUNCTION__": NdisAllocateCloneOidRequest failed with 0x%x\n", Status));
    }
    LogFlow(("<=="__FUNCTION__": Status=0x%x\n", Status));
    return Status;
}

VOID vboxNetLwfWinOidRequestComplete(IN NDIS_HANDLE hModuleCtx,
                                     IN PNDIS_OID_REQUEST pRequest,
                                     IN NDIS_STATUS Status)
{
    LogFlow(("==>"__FUNCTION__": module=%p req=%p status=0x%x\n", hModuleCtx, pRequest, Status));
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    PNDIS_OID_REQUEST pOriginal = *((PNDIS_OID_REQUEST*)(pRequest->SourceReserved));
    if (pOriginal)
    {
        /* NDIS is supposed to serialize requests */
        PNDIS_OID_REQUEST pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, NULL, PNDIS_OID_REQUEST);
        Assert(pPrev == pRequest);

        vboxNetLwfWinCopyOidRequestResults(pRequest, pOriginal);
        NdisFreeCloneOidRequest(pModuleCtx->hFilter, pRequest);
        NdisFOidRequestComplete(pModuleCtx->hFilter, pOriginal, Status);
    }
    else
    {
        /* This is not a clone, we originated it */
        Log((__FUNCTION__": locally originated request (%p) completed, status=0x%x\n", pRequest, Status));
        PVBOXNETLWF_OIDREQ pRqWrapper = RT_FROM_MEMBER(pRequest, VBOXNETLWF_OIDREQ, Request);
        pRqWrapper->Status = Status;
        NdisSetEvent(&pRqWrapper->Event);
    }
    LogFlow(("<=="__FUNCTION__"\n"));
}


static bool vboxNetLwfWinIsPromiscuous(PVBOXNETLWF_MODULE pModuleCtx)
{
    return !!(pModuleCtx->uPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS);
}

#if 0
static NDIS_STATUS vboxNetLwfWinGetPacketFilter(PVBOXNETLWF_MODULE pModuleCtx)
{
    LogFlow(("==>"__FUNCTION__"\n"));
    VBOXNETLWF_OIDREQ Rq;
    vboxNetLwfWinInitOidRequest(&Rq);
    Rq.Request.RequestType = NdisRequestQueryInformation;
    Rq.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBuffer = &pModuleCtx->uPacketFilter;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(pModuleCtx->uPacketFilter);
    NDIS_STATUS Status = vboxNetLwfWinSyncOidRequest(pModuleCtx, &Rq);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        Log((__FUNCTION__": vboxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed with 0x%x\n", Status));
        return FALSE;
    }
    if (Rq.Request.DATA.QUERY_INFORMATION.BytesWritten != sizeof(pModuleCtx->uPacketFilter))
    {
        Log((__FUNCTION__": vboxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed to write neccessary amount (%d bytes), actually written %d bytes\n", sizeof(pModuleCtx->uPacketFilter), Rq.Request.DATA.QUERY_INFORMATION.BytesWritten));
    }

    Log5((__FUNCTION__": OID_GEN_CURRENT_PACKET_FILTER query returned the following filters:\n"));
    vboxNetLwfWinDumpFilterTypes(pModuleCtx->uPacketFilter);

    LogFlow(("<=="__FUNCTION__": status=0x%x\n", Status));
    return Status;
}
#endif

static NDIS_STATUS vboxNetLwfWinSetPacketFilter(PVBOXNETLWF_MODULE pModuleCtx, bool fPromisc)
{
    LogFlow(("==>"__FUNCTION__": module=%p %s\n", pModuleCtx, fPromisc ? "promiscuous" : "normal"));
    VBOXNETLWF_OIDREQ Rq;
    vboxNetLwfWinInitOidRequest(&Rq);
    ULONG uFilter = ASMAtomicReadU32((uint32_t*)&pModuleCtx->uPacketFilter);
    if (fPromisc)
        uFilter |= NDIS_PACKET_TYPE_PROMISCUOUS;
    Rq.Request.RequestType = NdisRequestSetInformation;
    Rq.Request.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Rq.Request.DATA.SET_INFORMATION.InformationBuffer = &uFilter;
    Rq.Request.DATA.SET_INFORMATION.InformationBufferLength = sizeof(uFilter);
    NDIS_STATUS Status = vboxNetLwfWinSyncOidRequest(pModuleCtx, &Rq);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        Log((__FUNCTION__": vboxNetLwfWinSyncOidRequest(set, OID_GEN_CURRENT_PACKET_FILTER, vvv below vvv) failed with 0x%x\n", Status));
        vboxNetLwfWinDumpFilterTypes(uFilter);
    }
    LogFlow(("<=="__FUNCTION__": status=0x%x\n", Status));
    return Status;
}


static NTSTATUS vboxNetLwfWinDevDispatch(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    PIO_STACK_LOCATION pIrpSl = IoGetCurrentIrpStackLocation(pIrp);;
    NTSTATUS Status = STATUS_SUCCESS;

    switch (pIrpSl->MajorFunction)
    {
        case IRP_MJ_DEVICE_CONTROL:
            Status = STATUS_NOT_SUPPORTED;
            break;
        case IRP_MJ_CREATE:
        case IRP_MJ_CLEANUP:
        case IRP_MJ_CLOSE:
            break;
        default:
            Assert(0);
            break;
    }

    pIrp->IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return Status;
}

/** @todo So far we had no use for device, should we even bother to create it? */
static NDIS_STATUS vboxNetLwfWinDevCreate(PVBOXNETLWFGLOBALS pGlobals)
{
    NDIS_STRING DevName, LinkName;
    PDRIVER_DISPATCH aMajorFunctions[IRP_MJ_MAXIMUM_FUNCTION+1];
    NdisInitUnicodeString(&DevName, VBOXNETLWF_NAME_DEVICE);
    NdisInitUnicodeString(&LinkName, VBOXNETLWF_NAME_LINK);

    Assert(!pGlobals->hDevice);
    Assert(!pGlobals->pDevObj);
    NdisZeroMemory(aMajorFunctions, sizeof (aMajorFunctions));
    aMajorFunctions[IRP_MJ_CREATE] = vboxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLEANUP] = vboxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLOSE] = vboxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_DEVICE_CONTROL] = vboxNetLwfWinDevDispatch;

    NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceAttributes;
    NdisZeroMemory(&DeviceAttributes, sizeof(DeviceAttributes));
    DeviceAttributes.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
    DeviceAttributes.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttributes.Header.Size = sizeof(DeviceAttributes);
    DeviceAttributes.DeviceName = &DevName;
    DeviceAttributes.SymbolicName = &LinkName;
    DeviceAttributes.MajorFunctions = aMajorFunctions;
    //DeviceAttributes.ExtensionSize = sizeof(FILTER_DEVICE_EXTENSION);

    NDIS_STATUS Status = NdisRegisterDeviceEx(pGlobals->hFilterDriver,
                                              &DeviceAttributes,
                                              &pGlobals->pDevObj,
                                              &pGlobals->hDevice);
    Log(("vboxNetLwfWinDevCreate: NdisRegisterDeviceEx returned 0x%x\n", Status));
    Assert(Status == NDIS_STATUS_SUCCESS);
#if 0
    if (Status == NDIS_STATUS_SUCCESS)
    {
        PFILTER_DEVICE_EXTENSION pExtension;
        pExtension = NdisGetDeviceReservedExtension(pGlobals->pDevObj);
        pExtension->Signature = VBOXNETLWF_MEM_TAG;
        pExtension->Handle = pGlobals->hFilterDriver;
    }
#endif
    return Status;
}

static void vboxNetLwfWinDevDestroy(PVBOXNETLWFGLOBALS pGlobals)
{
    Assert(pGlobals->hDevice);
    Assert(pGlobals->pDevObj);
    NdisDeregisterDeviceEx(pGlobals->hDevice);
    pGlobals->hDevice = NULL;
    pGlobals->pDevObj = NULL;
}


static NDIS_STATUS vboxNetLwfWinAttach(IN NDIS_HANDLE hFilter, IN NDIS_HANDLE hDriverCtx,
                                       IN PNDIS_FILTER_ATTACH_PARAMETERS pParameters)
{
    LogFlow(("==>"__FUNCTION__": filter=%p\n", hFilter));

    PVBOXNETLWFGLOBALS pGlobals = (PVBOXNETLWFGLOBALS)hDriverCtx;
    AssertReturn(pGlobals, NDIS_STATUS_FAILURE);

    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)NdisAllocateMemoryWithTagPriority(hFilter,
                                                                      sizeof(VBOXNETLWF_MODULE),
                                                                      VBOXNETLWF_MEM_TAG,
                                                                      LowPoolPriority);
    if (!pModuleCtx)
        return NDIS_STATUS_RESOURCES;
    Log4((__FUNCTION__ ": allocated module context 0x%p\n", pModuleCtx));

    NdisZeroMemory(pModuleCtx, sizeof(VBOXNETLWF_MODULE));

    /* We use the miniport name to associate this filter module with the netflt instance */
    NTSTATUS rc = RtlUnicodeStringToAnsiString(&pModuleCtx->strMiniportName,
                                               pParameters->BaseMiniportName,
                                               TRUE);
    if (rc != STATUS_SUCCESS)
    {
        Log(("ERROR! vboxNetLwfWinAttach: RtlUnicodeStringToAnsiString(%ls) failed with 0x%x\n",
             pParameters->BaseMiniportName, rc));
        NdisFreeMemory(pModuleCtx, 0, 0);
        return NDIS_STATUS_FAILURE;
    }
    Assert(pParameters->MacAddressLength == sizeof(RTMAC));
    NdisMoveMemory(&pModuleCtx->MacAddr, pParameters->CurrentMacAddress, RT_MIN(sizeof(RTMAC), pParameters->MacAddressLength));
    if (pParameters->DefaultOffloadConfiguration)
    {
        pModuleCtx->SavedOffloadConfig = *pParameters->DefaultOffloadConfiguration;
        pModuleCtx->fOffloadConfigValid = true;
    }

    pModuleCtx->pGlobals = pGlobals;
    pModuleCtx->hFilter  = hFilter;
    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Attaching);
    /* Insert into module chain */
    NdisAcquireSpinLock(&pGlobals->Lock);
    RTListPrepend(&pGlobals->listModules, &pModuleCtx->node);
    NdisReleaseSpinLock(&pGlobals->Lock);
    /* Initialize transmission mutex and events */
    NDIS_INIT_MUTEX(&pModuleCtx->InTransmit);
#ifdef VBOXNETLWF_SYNC_SEND
    KeInitializeEvent(&pModuleCtx->EventWire, SynchronizationEvent, FALSE);
    KeInitializeEvent(&pModuleCtx->EventHost, SynchronizationEvent, FALSE);
#else /* !VBOXNETLWF_SYNC_SEND */
    NdisInitializeEvent(&pModuleCtx->EventSendComplete);
    pModuleCtx->cPendingBuffers = 0;
#endif /* !VBOXNETLWF_SYNC_SEND */
    /* Allocate buffer pools */
    NET_BUFFER_LIST_POOL_PARAMETERS PoolParams;
    NdisZeroMemory(&PoolParams, sizeof(PoolParams));
    PoolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    PoolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    PoolParams.Header.Size = sizeof(PoolParams);
    PoolParams.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
    PoolParams.fAllocateNetBuffer = TRUE;
    PoolParams.ContextSize = 0; /** @todo Do we need to consider underlying drivers? I think not. */
    PoolParams.PoolTag = VBOXNETLWF_MEM_TAG;
#ifndef VBOXNETLWF_SYNC_SEND
    PoolParams.DataSize = 2048; /** @todo figure out the optimal size, use several pools if necessary, make configurable, etc */
#endif /* !VBOXNETLWF_SYNC_SEND */

    pModuleCtx->hPool = NdisAllocateNetBufferListPool(hFilter, &PoolParams);
    if (!pModuleCtx->hPool)
    {
        Log(("ERROR! "__FUNCTION__": NdisAllocateNetBufferListPool failed\n"));
        RtlFreeAnsiString(&pModuleCtx->strMiniportName);
        NdisFreeMemory(pModuleCtx, 0, 0);
        return NDIS_STATUS_RESOURCES;
    }
    Log4((__FUNCTION__ ": allocated NBL+NB pool 0x%p\n", pModuleCtx->hPool));

    NDIS_FILTER_ATTRIBUTES Attributes;
    NdisZeroMemory(&Attributes, sizeof(Attributes));
    Attributes.Header.Revision = NDIS_FILTER_ATTRIBUTES_REVISION_1;
    Attributes.Header.Size = sizeof(Attributes);
    Attributes.Header.Type = NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES;
    Attributes.Flags = 0;
    NDIS_STATUS Status = NdisFSetAttributes(hFilter, pModuleCtx, &Attributes);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        Log(("ERROR! vboxNetLwfWinAttach: NdisFSetAttributes failed with 0x%x\n", Status));
        NdisFreeNetBufferListPool(pModuleCtx->hPool);
        Log4((__FUNCTION__ ": freed NBL+NB pool 0x%p\n", pModuleCtx->hPool));
        RtlFreeAnsiString(&pModuleCtx->strMiniportName);
        NdisFreeMemory(pModuleCtx, 0, 0);
        return NDIS_STATUS_RESOURCES;
    }

    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Paused);

    /// @todo Somehow the packet filter is 0 at this point: Status = vboxNetLwfWinGetPacketFilter(pModuleCtx);
    /// @todo We actually update it later in status handler, perhaps we should not do anything here.

    LogFlow(("<=="__FUNCTION__": Status = 0x%x\n", Status));
    return Status;
}

static VOID vboxNetLwfWinDetach(IN NDIS_HANDLE hModuleCtx)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Detached, LwfState_Paused);

    /* Remove from module chain */
    NdisAcquireSpinLock(&pModuleCtx->pGlobals->Lock);
    RTListNodeRemove(&pModuleCtx->node);
    NdisReleaseSpinLock(&pModuleCtx->pGlobals->Lock);

    PVBOXNETFLTINS pNetFltIns = pModuleCtx->pNetFlt; /// @todo Atomic?
    if (pNetFltIns && vboxNetFltTryRetainBusyNotDisconnected(pNetFltIns))
    {
        /*
         * Set hModuleCtx to null now in order to prevent filter restart,
         * OID requests and other stuff associated with NetFlt deactivation.
         */
        pNetFltIns->u.s.WinIf.hModuleCtx = NULL;
        /* Notify NetFlt that we are going down */
        pNetFltIns->pSwitchPort->pfnDisconnect(pNetFltIns->pSwitchPort, &pNetFltIns->MyPort, vboxNetFltPortReleaseBusy);
        /* We do not 'release' netflt instance since it has been done by pfnDisconnect */
    }
    pModuleCtx->pNetFlt = NULL;

    /*
     * We have to make sure that all NET_BUFFER_LIST structures have been freed by now, but
     * it does not require us to do anything here since it has already been taken care of
     * by vboxNetLwfWinPause().
     */
    if (pModuleCtx->hPool)
    {
        NdisFreeNetBufferListPool(pModuleCtx->hPool);
        Log4((__FUNCTION__ ": freed NBL+NB pool 0x%p\n", pModuleCtx->hPool));
    }
    RtlFreeAnsiString(&pModuleCtx->strMiniportName);
    NdisFreeMemory(hModuleCtx, 0, 0);
    Log4((__FUNCTION__ ": freed module context 0x%p\n", pModuleCtx));
    LogFlow(("<=="__FUNCTION__"\n"));
}


static NDIS_STATUS vboxNetLwfWinPause(IN NDIS_HANDLE hModuleCtx, IN PNDIS_FILTER_PAUSE_PARAMETERS pParameters)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Pausing, LwfState_Running);
    /* Wait for pending send/indication operations to complete. */
    NDIS_WAIT_FOR_MUTEX(&pModuleCtx->InTransmit);
#ifndef VBOXNETLWF_SYNC_SEND
    NdisWaitEvent(&pModuleCtx->EventSendComplete, 1000 /* ms */);
#endif /* !VBOXNETLWF_SYNC_SEND */
    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Paused, LwfState_Pausing);
    NDIS_RELEASE_MUTEX(&pModuleCtx->InTransmit);
    LogFlow(("<=="__FUNCTION__"\n"));
    return NDIS_STATUS_SUCCESS; /* Failure is not an option */
}


static void vboxNetLwfWinIndicateOffload(PVBOXNETLWF_MODULE pModuleCtx, PNDIS_OFFLOAD pOffload)
{
    Log5((__FUNCTION__": offload config changed to:\n"));
    vboxNetLwfWinDumpOffloadSettings(pOffload);
    NDIS_STATUS_INDICATION OffloadingIndication;
    NdisZeroMemory(&OffloadingIndication, sizeof(OffloadingIndication));
    OffloadingIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    OffloadingIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    OffloadingIndication.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    OffloadingIndication.SourceHandle = pModuleCtx->hFilter;
    OffloadingIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
    OffloadingIndication.StatusBuffer = pOffload;
    OffloadingIndication.StatusBufferSize = sizeof(NDIS_OFFLOAD);
    NdisFIndicateStatus(pModuleCtx->hFilter, &OffloadingIndication);
}


static NDIS_STATUS vboxNetLwfWinRestart(IN NDIS_HANDLE hModuleCtx, IN PNDIS_FILTER_RESTART_PARAMETERS pParameters)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Restarting, LwfState_Paused);
#if 1
    if (pModuleCtx->fOffloadConfigValid)
    {
        if (ASMAtomicReadBool(&pModuleCtx->fActive))
        {
            /* Disable offloading temporarily by indicating offload config change. */
            /** @todo Be sure to revise this when implementing offloading support! */
            NDIS_OFFLOAD OffloadConfig;
            OffloadConfig = pModuleCtx->SavedOffloadConfig;
            OffloadConfig.Checksum.IPv4Transmit.Encapsulation               = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Transmit.IpOptionsSupported          = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Transmit.TcpOptionsSupported         = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Transmit.TcpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Transmit.UdpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Transmit.IpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.Encapsulation                = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.IpOptionsSupported           = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.TcpOptionsSupported          = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.TcpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.UdpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv4Receive.IpChecksum                   = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Transmit.Encapsulation               = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Transmit.TcpOptionsSupported         = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Transmit.TcpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Transmit.UdpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Receive.Encapsulation                = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Receive.IpExtensionHeadersSupported  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Receive.TcpOptionsSupported          = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Receive.TcpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.Checksum.IPv6Receive.UdpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.LsoV1.IPv4.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.LsoV1.IPv4.TcpOptions                             = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.LsoV1.IPv4.IpOptions                              = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.LsoV2.IPv4.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.LsoV2.IPv6.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
            OffloadConfig.LsoV2.IPv6.IpExtensionHeadersSupported            = NDIS_OFFLOAD_NOT_SUPPORTED;
            OffloadConfig.LsoV2.IPv6.TcpOptionsSupported                    = NDIS_OFFLOAD_NOT_SUPPORTED;
            vboxNetLwfWinIndicateOffload(pModuleCtx, &OffloadConfig);
            Log((__FUNCTION__": set offloading off\n"));
        }
        else
        {
            /* The filter is inactive -- restore offloading configuration. */
            vboxNetLwfWinIndicateOffload(pModuleCtx, &pModuleCtx->SavedOffloadConfig);
            Log((__FUNCTION__": restored offloading config\n"));
        }
    }
#endif

    vboxNetLwfWinChangeState(pModuleCtx, LwfState_Running, LwfState_Restarting);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("<=="__FUNCTION__": Status = 0x%x\n", Status));
    return Status;
}


static void vboxNetLwfWinDumpPackets(const char *pszMsg, PNET_BUFFER_LIST pBufLists)
{
    for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
    {
        for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
        {
            Log(("%s packet: cb=%d\n", pszMsg, NET_BUFFER_DATA_LENGTH(pBuf)));
        }
    }
}

DECLINLINE(const char *) vboxNetLwfWinEthTypeStr(uint16_t uType)
{
    switch (uType)
    {
        case RTNET_ETHERTYPE_IPV4: return "IP";
        case RTNET_ETHERTYPE_IPV6: return "IPv6";
        case RTNET_ETHERTYPE_ARP:  return "ARP";
    }
    return "unknown";
}

#define VBOXNETLWF_PKTDMPSIZE 0x50

/**
 * Dump a packet to debug log.
 *
 * @param   cpPacket    The packet.
 * @param   cb          The size of the packet.
 * @param   cszText     A string denoting direction of packet transfer.
 */
DECLINLINE(void) vboxNetLwfWinDumpPacket(PCINTNETSG pSG, const char *cszText)
{
    uint8_t bPacket[VBOXNETLWF_PKTDMPSIZE];

    uint32_t cb = pSG->cbTotal < VBOXNETLWF_PKTDMPSIZE ? pSG->cbTotal : VBOXNETLWF_PKTDMPSIZE;
    IntNetSgReadEx(pSG, 0, cb, bPacket);

    AssertReturnVoid(cb >= 14);

    uint8_t *pHdr = bPacket;
    uint8_t *pEnd = bPacket + cb;
    AssertReturnVoid(pEnd - pHdr >= 14);
    uint16_t uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+12));
    Log2(("NetLWF: %s (%d bytes), %RTmac => %RTmac, EthType=%s(0x%x)\n",
          cszText, cb, pHdr+6, pHdr, vboxNetLwfWinEthTypeStr(uEthType), uEthType));
    pHdr += sizeof(RTNETETHERHDR);
    if (uEthType == RTNET_ETHERTYPE_VLAN)
    {
        AssertReturnVoid(pEnd - pHdr >= 4);
        uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+2));
        Log2((" + VLAN: id=%d EthType=%s(0x%x)\n", RT_N2H_U16(*(uint16_t*)(pHdr)) & 0xFFF,
              vboxNetLwfWinEthTypeStr(uEthType), uEthType));
        pHdr += 2 * sizeof(uint16_t);
    }
    uint8_t uProto = 0xFF;
    switch (uEthType)
    {
        case RTNET_ETHERTYPE_IPV6:
            AssertReturnVoid(pEnd - pHdr >= 40);
            uProto = pHdr[6];
            Log2((" + IPv6: %RTnaipv6 => %RTnaipv6\n", pHdr+8, pHdr+24));
            pHdr += 40;
            break;
        case RTNET_ETHERTYPE_IPV4:
            AssertReturnVoid(pEnd - pHdr >= 20);
            uProto = pHdr[9];
            Log2((" + IP: %RTnaipv4 => %RTnaipv4\n", *(uint32_t*)(pHdr+12), *(uint32_t*)(pHdr+16)));
            pHdr += (pHdr[0] & 0xF) * 4;
            break;
        case RTNET_ETHERTYPE_ARP:
            AssertReturnVoid(pEnd - pHdr >= 28);
            AssertReturnVoid(RT_N2H_U16(*(uint16_t*)(pHdr+2)) == RTNET_ETHERTYPE_IPV4);
            switch (RT_N2H_U16(*(uint16_t*)(pHdr+6)))
            {
                case 1: /* ARP request */
                    Log2((" + ARP-REQ: who-has %RTnaipv4 tell %RTnaipv4\n",
                          *(uint32_t*)(pHdr+24), *(uint32_t*)(pHdr+14)));
                    break;
                case 2: /* ARP reply */
                    Log2((" + ARP-RPL: %RTnaipv4 is-at %RTmac\n",
                          *(uint32_t*)(pHdr+14), pHdr+8));
                    break;
                default:
                    Log2((" + ARP: unknown op %d\n", RT_N2H_U16(*(uint16_t*)(pHdr+6))));
                    break;
            }
            break;
        /* There is no default case as uProto is initialized with 0xFF */
    }
    while (uProto != 0xFF)
    {
        switch (uProto)
        {
            case 0:  /* IPv6 Hop-by-Hop option*/
            case 60: /* IPv6 Destination option*/
            case 43: /* IPv6 Routing option */
            case 44: /* IPv6 Fragment option */
                Log2((" + IPv6 option (%d): <not implemented>\n", uProto));
                uProto = pHdr[0];
                pHdr += pHdr[1] * 8 + 8; /* Skip to the next extension/protocol */
                break;
            case 51: /* IPv6 IPsec AH */
                Log2((" + IPv6 IPsec AH: <not implemented>\n"));
                uProto = pHdr[0];
                pHdr += (pHdr[1] + 2) * 4; /* Skip to the next extension/protocol */
                break;
            case 50: /* IPv6 IPsec ESP */
                /* Cannot decode IPsec, fall through */
                Log2((" + IPv6 IPsec ESP: <not implemented>\n"));
                uProto = 0xFF;
                break;
            case 59: /* No Next Header */
                Log2((" + IPv6 No Next Header\n"));
                uProto = 0xFF;
                break;
            case 58: /* IPv6-ICMP */
                switch (pHdr[0])
                {
                    case 1:   Log2((" + IPv6-ICMP: destination unreachable, code %d\n", pHdr[1])); break;
                    case 128: Log2((" + IPv6-ICMP: echo request\n")); break;
                    case 129: Log2((" + IPv6-ICMP: echo reply\n")); break;
                    default:  Log2((" + IPv6-ICMP: unknown type %d, code %d\n", pHdr[0], pHdr[1])); break;
                }
                uProto = 0xFF;
                break;
            case 1: /* ICMP */
                switch (pHdr[0])
                {
                    case 0:  Log2((" + ICMP: echo reply\n")); break;
                    case 8:  Log2((" + ICMP: echo request\n")); break;
                    case 3:  Log2((" + ICMP: destination unreachable, code %d\n", pHdr[1])); break;
                    default: Log2((" + ICMP: unknown type %d, code %d\n", pHdr[0], pHdr[1])); break;
                }
                uProto = 0xFF;
                break;
            case 6: /* TCP */
                Log2((" + TCP: src=%d dst=%d seq=%x ack=%x\n",
                      RT_N2H_U16(*(uint16_t*)(pHdr)), RT_N2H_U16(*(uint16_t*)(pHdr+2)),
                      RT_N2H_U32(*(uint32_t*)(pHdr+4)), RT_N2H_U32(*(uint32_t*)(pHdr+8))));
                uProto = 0xFF;
                break;
            case 17: /* UDP */
                Log2((" + UDP: src=%d dst=%d\n",
                      RT_N2H_U16(*(uint16_t*)(pHdr)), RT_N2H_U16(*(uint16_t*)(pHdr+2))));
                uProto = 0xFF;
                break;
            default:
                Log2((" + Unknown: proto=0x%x\n", uProto));
                uProto = 0xFF;
                break;
        }
    }
    Log3(("%.*Rhxd\n", cb, bPacket));
}

static void vboxNetLwfWinDestroySG(PINTNETSG pSG)
{
    NdisFreeMemory(pSG, 0, 0);
    Log4((__FUNCTION__ ": freed SG 0x%p\n", pSG));
}

DECLINLINE(ULONG) vboxNetLwfWinCalcSegments(PNET_BUFFER pNetBuf)
{
    ULONG cSegs = 0;
    for (PMDL pMdl = NET_BUFFER_CURRENT_MDL(pNetBuf); pMdl; pMdl = NDIS_MDL_LINKAGE(pMdl))
        cSegs++;
    return cSegs;
}

DECLINLINE(void) vboxNetLwfWinFreeMdlChain(PMDL pMdl)
{
#ifdef VBOXNETLWF_SYNC_SEND
    PMDL pMdlNext;
    while (pMdl)
    {
        pMdlNext = pMdl->Next;
        NdisFreeMdl(pMdl);
        Log4((__FUNCTION__ ": freed MDL 0x%p\n", pMdl));
        pMdl = pMdlNext;
    }
#endif /* VBOXNETLWF_SYNC_SEND */
}

/** @todo
 * 1) Copy data from SG to MDL (if we decide to complete asynchronously).
 * 2) Provide context/backfill space. Nobody does it, should we?
 * 3) We always get a single segment from intnet. Simplify?
 */
static PNET_BUFFER_LIST vboxNetLwfWinSGtoNB(PVBOXNETLWF_MODULE pModule, PINTNETSG pSG)
{
    AssertReturn(pSG->cSegsUsed >= 1, NULL);
    LogFlow(("==>"__FUNCTION__": segments=%d\n", pSG->cSegsUsed));

#ifdef VBOXNETLWF_SYNC_SEND
    PINTNETSEG pSeg = pSG->aSegs;
    PMDL pMdl = NdisAllocateMdl(pModule->hFilter, pSeg->pv, pSeg->cb);
    if (!pMdl)
    {
        Log(("ERROR! "__FUNCTION__": failed to allocate an MDL\n"));
        LogFlow(("<=="__FUNCTION__": return NULL\n"));
        return NULL;
    }
    Log4((__FUNCTION__ ": allocated Mdl 0x%p\n", pMdl));
    PMDL pMdlCurr = pMdl;
    for (int i = 1; i < pSG->cSegsUsed; i++)
    {
        pSeg = &pSG->aSegs[i];
        pMdlCurr->Next = NdisAllocateMdl(pModule->hFilter, pSeg->pv, pSeg->cb);
        if (!pMdlCurr->Next)
        {
            Log(("ERROR! "__FUNCTION__": failed to allocate an MDL\n"));
            /* Tear down all MDL we chained so far */
            vboxNetLwfWinFreeMdlChain(pMdl);
            return NULL;
        }
        pMdlCurr = pMdlCurr->Next;
        Log4((__FUNCTION__ ": allocated Mdl 0x%p\n", pMdlCurr));
    }
    PNET_BUFFER_LIST pBufList = NdisAllocateNetBufferAndNetBufferList(pModule->hPool,
                                                                      0 /* ContextSize */,
                                                                      0 /* ContextBackFill */,
                                                                      pMdl,
                                                                      0 /* DataOffset */,
                                                                      pSG->cbTotal);
    if (pBufList)
    {
        Log4((__FUNCTION__ ": allocated NBL+NB 0x%p\n", pBufList));
        pBufList->SourceHandle = pModule->hFilter;
        /** @todo Do we need to initialize anything else? */
    }
    else
    {
        Log(("ERROR! "__FUNCTION__": failed to allocate an NBL+NB\n"));
        vboxNetLwfWinFreeMdlChain(pMdl);
    }
#else /* !VBOXNETLWF_SYNC_SEND */
    AssertReturn(pSG->cbTotal < 2048, NULL);
    PNET_BUFFER_LIST pBufList = NdisAllocateNetBufferList(pModule->hPool,
                                                          0 /** @todo ContextSize */,
                                                          0 /** @todo ContextBackFill */);
    NET_BUFFER_LIST_NEXT_NBL(pBufList) = NULL; /** @todo Is it even needed? */
    NET_BUFFER *pBuffer = NET_BUFFER_LIST_FIRST_NB(pBufList);
    NDIS_STATUS Status = NdisRetreatNetBufferDataStart(pBuffer, pSG->cbTotal, 0 /** @todo DataBackfill */, NULL);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        uint8_t *pDst = (uint8_t*)NdisGetDataBuffer(pBuffer, pSG->cbTotal, NULL, 1, 0);
        if (pDst)
        {
            for (int i = 0; i < pSG->cSegsUsed; i++)
            {
                NdisMoveMemory(pDst, pSG->aSegs[i].pv, pSG->aSegs[i].cb);
                pDst += pSG->aSegs[i].cb;
            }
            Log4((__FUNCTION__ ": allocated NBL+NB+MDL+Data 0x%p\n", pBufList));
            pBufList->SourceHandle = pModule->hFilter;
            /** @todo Do we need to initialize anything else? */
        }
        else
        {
            Log((__FUNCTION__": failed to obtain the buffer pointer (size=%u)\n", pSG->cbTotal));
            NdisAdvanceNetBufferDataStart(pBuffer, pSG->cbTotal, false, NULL); /** @todo why bother? */
            NdisFreeNetBufferList(pBufList);
            pBufList = NULL;
        }
    }
    else
    {
        Log((__FUNCTION__": NdisRetreatNetBufferDataStart failed with 0x%x (size=%u)\n", Status, pSG->cbTotal));
        NdisFreeNetBufferList(pBufList);
        pBufList = NULL;
    }
#endif /* !VBOXNETLWF_SYNC_SEND */
    LogFlow(("<=="__FUNCTION__": return %p\n", pBufList));
    return pBufList;
}

static PINTNETSG vboxNetLwfWinNBtoSG(PVBOXNETLWF_MODULE pModule, PNET_BUFFER pNetBuf)
{
    ULONG cbPacket = NET_BUFFER_DATA_LENGTH(pNetBuf);
    UINT cSegs = vboxNetLwfWinCalcSegments(pNetBuf);
    /* Allocate and initialize SG */
    PINTNETSG pSG = (PINTNETSG)NdisAllocateMemoryWithTagPriority(pModule->hFilter,
                                                                 RT_OFFSETOF(INTNETSG, aSegs[cSegs]),
                                                                 VBOXNETLWF_MEM_TAG,
                                                                 NormalPoolPriority);
    AssertReturn(pSG, pSG);
    Log4((__FUNCTION__ ": allocated SG 0x%p\n", pSG));
    IntNetSgInitTempSegs(pSG, cbPacket /*cbTotal*/, cSegs, cSegs /*cSegsUsed*/);

    int rc = NDIS_STATUS_SUCCESS;
    ULONG uOffset = NET_BUFFER_CURRENT_MDL_OFFSET(pNetBuf);
    cSegs = 0;
    for (PMDL pMdl = NET_BUFFER_CURRENT_MDL(pNetBuf);
         pMdl != NULL && cbPacket > 0;
         pMdl = NDIS_MDL_LINKAGE(pMdl))
    {
        PUCHAR pSrc = (PUCHAR)MmGetSystemAddressForMdlSafe(pMdl, LowPagePriority);
        if (!pSrc)
        {
            rc = NDIS_STATUS_RESOURCES;
            break;
        }
        ULONG cbSrc = MmGetMdlByteCount(pMdl);
        if (uOffset)
        {
            Assert(uOffset < cbSrc);
            pSrc  += uOffset;
            cbSrc -= uOffset;
            uOffset = 0;
        }

        if (cbSrc > cbPacket)
            cbSrc = cbPacket;

        pSG->aSegs[cSegs].pv = pSrc;
        pSG->aSegs[cSegs].cb = cbSrc;
        pSG->aSegs[cSegs].Phys = NIL_RTHCPHYS;
        cSegs++;
        cbPacket -= cbSrc;
    }

    Assert(cSegs <= pSG->cSegsAlloc);

    if (RT_FAILURE(rc))
    {
        vboxNetLwfWinDestroySG(pSG);
        pSG = NULL;
    }
    else
    {
        Assert(cbPacket == 0);
        Assert(pSG->cSegsUsed == cSegs);
    }
    return pSG;
}

VOID vboxNetLwfWinStatus(IN NDIS_HANDLE hModuleCtx, IN PNDIS_STATUS_INDICATION pIndication)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)hModuleCtx;
    Log((__FUNCTION__"Status indication: %s\n", vboxNetLwfWinStatusToText(pIndication->StatusCode)));
    switch (pIndication->StatusCode)
    {
        case NDIS_STATUS_PACKET_FILTER:
            vboxNetLwfWinDumpFilterTypes(*(ULONG*)pIndication->StatusBuffer);
            break;
        case NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG:
            Log5((__FUNCTION__": offloading currently set to:\n"));
            vboxNetLwfWinDumpOffloadSettings((PNDIS_OFFLOAD)pIndication->StatusBuffer);
            break;
    }
    NdisFIndicateStatus(pModule->hFilter, pIndication);
    LogFlow(("<=="__FUNCTION__"\n"));
}

static bool vboxNetLwfWinForwardToIntNet(PVBOXNETLWF_MODULE pModuleCtx, PNET_BUFFER_LIST pBufLists, uint32_t fSrc)
{
    /* We must not forward anything to the trunk unless it is ready to receive. */
    if (!ASMAtomicReadBool(&pModuleCtx->fActive))
    {
        Log((__FUNCTION__": trunk is inactive, won't forward\n"));
        return false;
    }

    AssertReturn(pModuleCtx->pNetFlt, false);
    AssertReturn(pModuleCtx->pNetFlt->pSwitchPort, false);
    AssertReturn(pModuleCtx->pNetFlt->pSwitchPort->pfnRecv, false);
    LogFlow(("==>"__FUNCTION__": module=%p\n", pModuleCtx));
    Assert(pBufLists);                                                   /* The chain must contain at least one list */
    Assert(NET_BUFFER_LIST_NEXT_NBL(pBufLists) == NULL); /* The caller is supposed to unlink the list from the chain */
    /*
     * Even if NBL contains more than one buffer we are prepared to deal with it.
     * When any of buffers should not be dropped we keep the whole list. It is
     * better to leak some "unexpected" packets to the wire/host than to loose any.
     */
    bool fDropIt   = false;
    bool fDontDrop = false;
    int nLists = 0;
    for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
    {
        int nBuffers = 0;
        nLists++;
        for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
        {
            nBuffers++;
            PINTNETSG pSG = vboxNetLwfWinNBtoSG(pModuleCtx, pBuf);
            if (pSG)
            {
                vboxNetLwfWinDumpPacket(pSG, (fSrc & INTNETTRUNKDIR_WIRE)?"intnet <-- wire":"intnet <-- host");
                /* A bit paranoid, but we do not use any locks, so... */
                if (ASMAtomicReadBool(&pModuleCtx->fActive))
                    if (pModuleCtx->pNetFlt->pSwitchPort->pfnRecv(pModuleCtx->pNetFlt->pSwitchPort, NULL, pSG, fSrc))
                        fDropIt = true;
                    else
                        fDontDrop = true;
                vboxNetLwfWinDestroySG(pSG);
            }
        }
        Log((__FUNCTION__": list=%d buffers=%d\n", nLists, nBuffers));
    }
    Log((__FUNCTION__": lists=%d drop=%s don't=%s\n", nLists, fDropIt ? "true":"false", fDontDrop ? "true":"false"));
    LogFlow(("<=="__FUNCTION__": return '%s'\n",
             fDropIt ? (fDontDrop ? "do not drop (some)" : "drop it") : "do not drop (any)"));
    return fDropIt && !fDontDrop; /* Drop the list if ALL its buffers are being dropped! */
}

DECLINLINE(bool) vboxNetLwfWinIsRunning(PVBOXNETLWF_MODULE pModule)
{
    Log((__FUNCTION__": state=%d\n", ASMAtomicReadU32(&pModule->enmState)));
    return ASMAtomicReadU32(&pModule->enmState) == LwfState_Running;
}

VOID vboxNetLwfWinSendNetBufferLists(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN NDIS_PORT_NUMBER nPort, IN ULONG fFlags)
{
    size_t cb = 0;
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)hModuleCtx;
    if (vboxNetLwfWinIsRunning(pModule))
    {
        PNET_BUFFER_LIST pNext     = NULL;
        PNET_BUFFER_LIST pDropHead = NULL;
        PNET_BUFFER_LIST pDropTail = NULL;
        PNET_BUFFER_LIST pPassHead = NULL;
        PNET_BUFFER_LIST pPassTail = NULL;
        for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = pNext)
        {
            pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
            NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink */
            if (vboxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_HOST))
            {
                NET_BUFFER_LIST_STATUS(pList) = NDIS_STATUS_SUCCESS;
                if (pDropHead)
                {
                    NET_BUFFER_LIST_NEXT_NBL(pDropTail) = pList;
                    pDropTail = pList;
                }
                else
                    pDropHead = pDropTail = pList;
            }
            else
            {
                if (pPassHead)
                {
                    NET_BUFFER_LIST_NEXT_NBL(pPassTail) = pList;
                    pPassTail = pList;
                }
                else
                    pPassHead = pPassTail = pList;
            }
        }
        Assert((pBufLists == pPassHead) || (pBufLists == pDropHead));
        if (pPassHead)
        {
            vboxNetLwfWinDumpPackets(__FUNCTION__": passing down", pPassHead);
            NdisFSendNetBufferLists(pModule->hFilter, pBufLists, nPort, fFlags);
        }
        if (pDropHead)
        {
            vboxNetLwfWinDumpPackets(__FUNCTION__": consumed", pDropHead);
            NdisFSendNetBufferListsComplete(pModule->hFilter, pDropHead,
                                            fFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
        }
    }
    else
    {
        for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
        {
            NET_BUFFER_LIST_STATUS(pList) = NDIS_STATUS_PAUSED;
        }
        vboxNetLwfWinDumpPackets(__FUNCTION__": consumed", pBufLists);
        NdisFSendNetBufferListsComplete(pModule->hFilter, pBufLists,
                                        fFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);

    }
    LogFlow(("<=="__FUNCTION__"\n"));
}

VOID vboxNetLwfWinSendNetBufferListsComplete(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN ULONG fFlags)
{
    size_t cb = 0;
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)hModuleCtx;
    PNET_BUFFER_LIST pList = pBufLists;
    PNET_BUFFER_LIST pNextList;
    PNET_BUFFER_LIST pPrevList = NULL;
    while (pList)
    {
        pNextList = NET_BUFFER_LIST_NEXT_NBL(pList);
        if (pList->SourceHandle == pModule->hFilter)
        {
            /* We allocated this NET_BUFFER_LIST, let's free it up */
            Assert(NET_BUFFER_LIST_FIRST_NB(pList));
            Assert(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /*
             * All our NBLs hold a single NB each, no need to iterate over a list.
             * There is no need to free an associated NB explicitly either, as it was
             * preallocated with NBL structure.
             */
            Assert(!NET_BUFFER_NEXT_NB(NET_BUFFER_LIST_FIRST_NB(pList)));
            vboxNetLwfWinFreeMdlChain(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /* Unlink this list from the chain */
            if (pPrevList)
                NET_BUFFER_LIST_NEXT_NBL(pPrevList) = pNextList;
            else
                pBufLists = pNextList;
            Log((__FUNCTION__": our list %p, next=%p, previous=%p, head=%p\n", pList, pNextList, pPrevList, pBufLists));
            NdisFreeNetBufferList(pList);
#ifdef VBOXNETLWF_SYNC_SEND
            Log4((__FUNCTION__ ": freed NBL+NB 0x%p\n", pList));
            KeSetEvent(&pModule->EventWire, 0, FALSE);
#else /* !VBOXNETLWF_SYNC_SEND */
            Log4((__FUNCTION__ ": freed NBL+NB+MDL+Data 0x%p\n", pList));
            Assert(ASMAtomicReadS32(&pModule->cPendingBuffers) > 0);
            if (ASMAtomicDecS32(&pModule->cPendingBuffers) == 0)
                NdisSetEvent(&pModule->EventSendComplete);
#endif /* !VBOXNETLWF_SYNC_SEND */
        }
        else
        {
            pPrevList = pList;
            Log((__FUNCTION__": passing list %p, next=%p, previous=%p, head=%p\n", pList, pNextList, pPrevList, pBufLists));
        }
        pList = pNextList;
    }
    if (pBufLists)
    {
        /* There are still lists remaining in the chain, pass'em up */
        NdisFSendNetBufferListsComplete(pModule->hFilter, pBufLists, fFlags);
    }
    LogFlow(("<=="__FUNCTION__"\n"));
}

VOID vboxNetLwfWinReceiveNetBufferLists(IN NDIS_HANDLE hModuleCtx,
                                        IN PNET_BUFFER_LIST pBufLists,
                                        IN NDIS_PORT_NUMBER nPort,
                                        IN ULONG nBufLists,
                                        IN ULONG fFlags)
{
    /// @todo Do we need loopback handling?
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)hModuleCtx;
    if (vboxNetLwfWinIsRunning(pModule))
    {
        if (NDIS_TEST_RECEIVE_CANNOT_PEND(fFlags))
        {
            /* We do not own NBLs so we do not need to return them */
            /* First we need to scan through the list to see if some packets must be dropped */
            bool bDropIt = false;
            for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
            {
                PNET_BUFFER_LIST pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
                NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink temporarily */
                if (vboxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_WIRE))
                    bDropIt = true;
                NET_BUFFER_LIST_NEXT_NBL(pList) = pNext; /* Restore the link */
            }
            if (bDropIt)
            {
                /* Some NBLs must be dropped, indicate selectively one by one */
                for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
                {
                    PNET_BUFFER_LIST pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
                    NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink temporarily */
                    vboxNetLwfWinDumpPackets(__FUNCTION__": passing up", pList);
                    NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pList, nPort, nBufLists, fFlags);
                    NET_BUFFER_LIST_NEXT_NBL(pList) = pNext; /* Restore the link */
                }
            }
            else
            {
                /* All NBLs must be indicated, do it in bulk. */
                vboxNetLwfWinDumpPackets(__FUNCTION__": passing up", pBufLists);
                NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pBufLists, nPort, nBufLists, fFlags);
            }
        }
        else
        {
            /* We collect dropped NBLs in a separate list in order to "return" them. */
            PNET_BUFFER_LIST pNext     = NULL;
            PNET_BUFFER_LIST pDropHead = NULL;
            PNET_BUFFER_LIST pDropTail = NULL;
            PNET_BUFFER_LIST pPassHead = NULL;
            PNET_BUFFER_LIST pPassTail = NULL;
            ULONG nDrop = 0, nPass = 0;
            for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = pNext)
            {
                pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
                NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink */
                if (vboxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_WIRE))
                {
                    if (nDrop++)
                    {
                        NET_BUFFER_LIST_NEXT_NBL(pDropTail) = pList;
                        pDropTail = pList;
                    }
                    else
                        pDropHead = pDropTail = pList;
                }
                else
                {
                    if (nPass++)
                    {
                        NET_BUFFER_LIST_NEXT_NBL(pPassTail) = pList;
                        pPassTail = pList;
                    }
                    else
                        pPassHead = pPassTail = pList;
                }
            }
            Assert((pBufLists == pPassHead) || (pBufLists == pDropHead));
            Assert(nDrop + nPass == nBufLists);
            if (pPassHead)
            {
                vboxNetLwfWinDumpPackets(__FUNCTION__": passing up", pPassHead);
                NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pPassHead, nPort, nPass, fFlags);
            }
            if (pDropHead)
            {
                vboxNetLwfWinDumpPackets(__FUNCTION__": consumed", pDropHead);
                NdisFReturnNetBufferLists(pModule->hFilter, pDropHead,
                                          fFlags & NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL ? NDIS_RETURN_FLAGS_DISPATCH_LEVEL : 0);
            }
        }

    }
    else
    {
        vboxNetLwfWinDumpPackets(__FUNCTION__": consumed", pBufLists);
        if ((fFlags & NDIS_RECEIVE_FLAGS_RESOURCES) == 0)
            NdisFReturnNetBufferLists(pModule->hFilter, pBufLists,
                                      fFlags & NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL ? NDIS_RETURN_FLAGS_DISPATCH_LEVEL : 0);
    }
    LogFlow(("<=="__FUNCTION__"\n"));
}

VOID vboxNetLwfWinReturnNetBufferLists(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN ULONG fFlags)
{
    size_t cb = 0;
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)hModuleCtx;
    PNET_BUFFER_LIST pList = pBufLists;
    PNET_BUFFER_LIST pNextList;
    PNET_BUFFER_LIST pPrevList = NULL;
    /** @todo Move common part into a separate function to be used by vboxNetLwfWinSendNetBufferListsComplete() as well */
    while (pList)
    {
        pNextList = NET_BUFFER_LIST_NEXT_NBL(pList);
        if (pList->SourceHandle == pModule->hFilter)
        {
            /* We allocated this NET_BUFFER_LIST, let's free it up */
            Assert(NET_BUFFER_LIST_FIRST_NB(pList));
            Assert(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /*
             * All our NBLs hold a single NB each, no need to iterate over a list.
             * There is no need to free an associated NB explicitly either, as it was
             * preallocated with NBL structure.
             */
            vboxNetLwfWinFreeMdlChain(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /* Unlink this list from the chain */
            if (pPrevList)
                NET_BUFFER_LIST_NEXT_NBL(pPrevList) = pNextList;
            else
                pBufLists = pNextList;
            NdisFreeNetBufferList(pList);
#ifdef VBOXNETLWF_SYNC_SEND
            Log4((__FUNCTION__ ": freed NBL+NB 0x%p\n", pList));
            KeSetEvent(&pModule->EventHost, 0, FALSE);
#else /* !VBOXNETLWF_SYNC_SEND */
            Log4((__FUNCTION__ ": freed NBL+NB+MDL+Data 0x%p\n", pList));
            Assert(ASMAtomicReadS32(&pModule->cPendingBuffers) > 0);
            if (ASMAtomicDecS32(&pModule->cPendingBuffers) == 0)
                NdisSetEvent(&pModule->EventSendComplete);
#endif /* !VBOXNETLWF_SYNC_SEND */
        }
        else
            pPrevList = pList;
        pList = pNextList;
    }
    if (pBufLists)
    {
        /* There are still lists remaining in the chain, pass'em up */
        NdisFReturnNetBufferLists(pModule->hFilter, pBufLists, fFlags);
    }
    LogFlow(("<=="__FUNCTION__"\n"));
}

NDIS_STATUS vboxNetLwfWinSetModuleOptions(IN NDIS_HANDLE hModuleCtx)
{
    LogFlow(("==>"__FUNCTION__": module=%p\n", hModuleCtx));
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)hModuleCtx;
    NDIS_FILTER_PARTIAL_CHARACTERISTICS PChars;

    NdisZeroMemory(&PChars, sizeof(PChars));

    PChars.Header.Type = NDIS_OBJECT_TYPE_FILTER_PARTIAL_CHARACTERISTICS;
    PChars.Header.Size = NDIS_SIZEOF_FILTER_PARTIAL_CHARACTERISTICS_REVISION_1;
    PChars.Header.Revision = NDIS_FILTER_PARTIAL_CHARACTERISTICS_REVISION_1;

    if (ASMAtomicReadBool(&pModuleCtx->fActive))
    {
        Log((__FUNCTION__": active mode\n"));
        PChars.SendNetBufferListsHandler = vboxNetLwfWinSendNetBufferLists;
        PChars.SendNetBufferListsCompleteHandler = vboxNetLwfWinSendNetBufferListsComplete;
        PChars.ReceiveNetBufferListsHandler = vboxNetLwfWinReceiveNetBufferLists;
        PChars.ReturnNetBufferListsHandler = vboxNetLwfWinReturnNetBufferLists;
    }
    else
    {
        Log((__FUNCTION__": bypass mode\n"));
    }
    NDIS_STATUS Status = NdisSetOptionalHandlers(pModuleCtx->hFilter,
                                                 (PNDIS_DRIVER_OPTIONAL_HANDLERS)&PChars);
    LogFlow(("<=="__FUNCTION__": status=0x%x\n", Status));
    return Status;
}

/**
 * register the filter driver
 */
DECLHIDDEN(NDIS_STATUS) vboxNetLwfWinRegister(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr)
{
    NDIS_FILTER_DRIVER_CHARACTERISTICS FChars;
    NDIS_STRING FriendlyName;
    NDIS_STRING UniqueName;
    NDIS_STRING ServiceName;

    NdisInitUnicodeString(&FriendlyName, VBOXNETLWF_NAME_FRIENDLY);
    NdisInitUnicodeString(&UniqueName, VBOXNETLWF_NAME_UNIQUE);
    NdisInitUnicodeString(&ServiceName, VBOXNETLWF_NAME_SERVICE);

    NdisZeroMemory(&FChars, sizeof (FChars));

    FChars.Header.Type = NDIS_OBJECT_TYPE_FILTER_DRIVER_CHARACTERISTICS;
    FChars.Header.Size = sizeof(NDIS_FILTER_DRIVER_CHARACTERISTICS);
    FChars.Header.Revision = NDIS_FILTER_CHARACTERISTICS_REVISION_1;

    FChars.MajorNdisVersion = VBOXNETLWF_VERSION_NDIS_MAJOR;
    FChars.MinorNdisVersion = VBOXNETLWF_VERSION_NDIS_MINOR;

    FChars.FriendlyName = FriendlyName;
    FChars.UniqueName = UniqueName;
    FChars.ServiceName = ServiceName;

    /* Mandatory functions */
    FChars.AttachHandler = vboxNetLwfWinAttach;
    FChars.DetachHandler = vboxNetLwfWinDetach;
    FChars.RestartHandler = vboxNetLwfWinRestart;
    FChars.PauseHandler = vboxNetLwfWinPause;

    /* Optional functions, non changeble at run-time */
    FChars.OidRequestHandler = vboxNetLwfWinOidRequest;
    FChars.OidRequestCompleteHandler = vboxNetLwfWinOidRequestComplete;
    //FChars.CancelOidRequestHandler = vboxNetLwfWinCancelOidRequest;
    FChars.StatusHandler = vboxNetLwfWinStatus;
    //FChars.NetPnPEventHandler = vboxNetLwfWinPnPEvent;
    FChars.SetFilterModuleOptionsHandler = vboxNetLwfWinSetModuleOptions;

    /* Optional functions */
    FChars.SendNetBufferListsHandler = vboxNetLwfWinSendNetBufferLists;
    FChars.SendNetBufferListsCompleteHandler = vboxNetLwfWinSendNetBufferListsComplete;
    FChars.ReceiveNetBufferListsHandler = vboxNetLwfWinReceiveNetBufferLists;
    FChars.ReturnNetBufferListsHandler = vboxNetLwfWinReturnNetBufferLists;

    pDriverObject->DriverUnload = vboxNetLwfWinUnloadDriver;

    NDIS_STATUS Status;
    g_VBoxNetLwfGlobals.hFilterDriver = NULL;
    Log(("vboxNetLwfWinRegister: registering filter driver...\n"));
    Status = NdisFRegisterFilterDriver(pDriverObject,
                                       (NDIS_HANDLE)&g_VBoxNetLwfGlobals,
                                       &FChars,
                                       &g_VBoxNetLwfGlobals.hFilterDriver);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Log(("vboxNetLwfWinRegister: successfully registered filter driver; registering device...\n"));
        Status = vboxNetLwfWinDevCreate(&g_VBoxNetLwfGlobals);
        Assert(Status == STATUS_SUCCESS);
        Log(("vboxNetLwfWinRegister: vboxNetLwfWinDevCreate() returned 0x%x\n", Status));
    }
    else
    {
        Log(("ERROR! vboxNetLwfWinRegister: failed to register filter driver, status=0x%x", Status));
    }
    return Status;
}

static int vboxNetLwfWinStartInitIdcThread()
{
    int rc = VERR_INVALID_STATE;

    if (ASMAtomicCmpXchgU32(&g_VBoxNetLwfGlobals.enmIdcState, LwfIdcState_Connecting, LwfIdcState_Disconnected))
    {
        Log((__FUNCTION__": IDC state change Diconnected -> Connecting\n"));

        NTSTATUS Status = PsCreateSystemThread(&g_VBoxNetLwfGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS,
                                               NULL,
                                               NULL,
                                               NULL,
                                               vboxNetLwfWinInitIdcWorker,
                                               &g_VBoxNetLwfGlobals);
        Log((__FUNCTION__": create IDC initialization thread, status=0x%x\n", Status));
        if (Status != STATUS_SUCCESS)
        {
            LogRel(("NETLWF: IDC initialization failed (system thread creation, status=0x%x)\n", Status));
            /*
             * We failed to init IDC and there will be no second chance.
             */
            Log((__FUNCTION__": IDC state change Connecting -> Diconnected\n"));
            ASMAtomicWriteU32(&g_VBoxNetLwfGlobals.enmIdcState, LwfIdcState_Disconnected);
        }
        rc = RTErrConvertFromNtStatus(Status);
    }
    return rc;
}

static void vboxNetLwfWinStopInitIdcThread()
{
}


RT_C_DECLS_BEGIN

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);

RT_C_DECLS_END

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int rc;

    /* the idc registration is initiated via IOCTL since our driver
     * can be loaded when the VBoxDrv is not in case we are a Ndis IM driver */
    rc = vboxNetLwfWinInitBase();
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        NdisZeroMemory(&g_VBoxNetLwfGlobals, sizeof (g_VBoxNetLwfGlobals));
        RTListInit(&g_VBoxNetLwfGlobals.listModules);
        NdisAllocateSpinLock(&g_VBoxNetLwfGlobals.Lock);
        /*
         * We choose to ignore IDC initialization errors here because if we fail to load
         * our filter the upper protocols won't bind to the associated adapter, causing
         * network failure at the host. Better to have non-working filter than broken
         * networking on the host.
         */
        rc = vboxNetLwfWinStartInitIdcThread();
        AssertRC(rc);

        Status = vboxNetLwfWinRegister(pDriverObject, pRegistryPath);
        Assert(Status == STATUS_SUCCESS);
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Log(("NETLWF: started successfully\n"));
            return STATUS_SUCCESS;
        }
        NdisFreeSpinLock(&g_VBoxNetLwfGlobals.Lock);
        vboxNetLwfWinFini();
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    return Status;
}


static VOID vboxNetLwfWinUnloadDriver(IN PDRIVER_OBJECT pDriver)
{
    LogFlow(("==>"__FUNCTION__": driver=%p\n", pDriver));
    vboxNetLwfWinDevDestroy(&g_VBoxNetLwfGlobals);
    NdisFDeregisterFilterDriver(g_VBoxNetLwfGlobals.hFilterDriver);
    NdisFreeSpinLock(&g_VBoxNetLwfGlobals.Lock);
    LogFlow(("<=="__FUNCTION__"\n"));
    vboxNetLwfWinFini();
}

static const char *vboxNetLwfWinIdcStateToText(uint32_t enmState)
{
    switch (enmState)
    {
        case LwfIdcState_Disconnected: return "Disconnected";
        case LwfIdcState_Connecting: return "Connecting";
        case LwfIdcState_Connected: return "Connected";
        case LwfIdcState_Stopping: return "Stopping";
    }
    return "Unknown";
}

static VOID vboxNetLwfWinInitIdcWorker(PVOID pvContext)
{
    int rc;
    PVBOXNETLWFGLOBALS pGlobals = (PVBOXNETLWFGLOBALS)pvContext;

    while (ASMAtomicReadU32(&pGlobals->enmIdcState) == LwfIdcState_Connecting)
    {
        rc = vboxNetFltInitIdc(&g_VBoxNetFltGlobals);
        if (RT_SUCCESS(rc))
        {
            if (!ASMAtomicCmpXchgU32(&pGlobals->enmIdcState, LwfIdcState_Connected, LwfIdcState_Connecting))
            {
                /* The state has been changed (the only valid transition is to "Stopping"), undo init */
                rc = vboxNetFltTryDeleteIdc(&g_VBoxNetFltGlobals);
                Log((__FUNCTION__": state change (Connecting -> %s) while initializing IDC, deleted IDC, rc=0x%x\n",
                     vboxNetLwfWinIdcStateToText(ASMAtomicReadU32(&pGlobals->enmIdcState)), rc));
            }
            else
            {
                Log((__FUNCTION__": IDC state change Connecting -> Connected\n"));
            }
        }
        else
        {
            LARGE_INTEGER WaitIn100nsUnits;
            WaitIn100nsUnits.QuadPart = -(LONGLONG)10000000; /* 1 sec */
            KeDelayExecutionThread(KernelMode, FALSE /* non-alertable */, &WaitIn100nsUnits);
        }
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static int vboxNetLwfWinTryFiniIdc()
{
    int rc = VINF_SUCCESS;
    NTSTATUS Status;
    PKTHREAD pThread = NULL;
    uint32_t enmPrevState = ASMAtomicXchgU32(&g_VBoxNetLwfGlobals.enmIdcState, LwfIdcState_Stopping);

    Log((__FUNCTION__": IDC state change %s -> Stopping\n", vboxNetLwfWinIdcStateToText(enmPrevState)));

    switch (enmPrevState)
    {
        case LwfIdcState_Disconnected:
            /* Have not even attempted to connect -- nothing to do. */
            break;
        case LwfIdcState_Stopping:
            /* Impossible, but another thread is alreading doing FiniIdc, bail out */
            Log(("ERROR: "__FUNCTION__"() called in 'Stopping' state\n"));
            rc = VERR_INVALID_STATE;
            break;
        case LwfIdcState_Connecting:
            /* the worker thread is running, let's wait for it to stop */
            Status = ObReferenceObjectByHandle(g_VBoxNetLwfGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS, NULL, KernelMode,
                                               (PVOID*)&pThread, NULL);
            if (Status == STATUS_SUCCESS)
            {
                KeWaitForSingleObject(pThread, Executive, KernelMode, FALSE, NULL);
                ObDereferenceObject(pThread);
            }
            else
            {
                Log(("ERROR in "__FUNCTION__": ObReferenceObjectByHandle(%p) failed with 0x%x\n",
                     g_VBoxNetLwfGlobals.hInitIdcThread, Status));
            }
            rc = RTErrConvertFromNtStatus(Status);
            break;
        case LwfIdcState_Connected:
            /* the worker succeeded in IDC init and terminated */
            rc = vboxNetFltTryDeleteIdc(&g_VBoxNetFltGlobals);
            Log((__FUNCTION__": deleted IDC, rc=0x%x\n", rc));
            break;
    }
    return rc;
}

static void vboxNetLwfWinFiniBase()
{
    vboxNetFltDeleteGlobals(&g_VBoxNetFltGlobals);

    /*
     * Undo the work done during start (in reverse order).
     */
    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));

    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));

    RTR0Term();
}

static int vboxNetLwfWinInitBase()
{
    int rc = RTR0Init(0);
    if (!RT_SUCCESS(rc))
        return rc;

    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));
    rc = vboxNetFltInitGlobals(&g_VBoxNetFltGlobals);
    if (!RT_SUCCESS(rc))
        RTR0Term();

    return rc;
}

static int vboxNetLwfWinFini()
{
    int rc = vboxNetLwfWinTryFiniIdc();
    if (RT_SUCCESS(rc))
    {
        vboxNetLwfWinFiniBase();
    }
    return rc;
}


/*
 *
 * The OS specific interface definition
 *
 */


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    LogFlow(("==>"__FUNCTION__": instance=%p\n", pThis));
    LogFlow(("<=="__FUNCTION__": return %RTbool\n", !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost)));
    /* AttachToInterface true if disconnected */
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}

int vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    int rc = VINF_SUCCESS;

    PVBOXNETLWF_MODULE pModule = (PVBOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>"__FUNCTION__": instance=%p module=%p\n", pThis, pModule));
    if (!pModule)
    {
        LogFlow(("<=="__FUNCTION__": pModule is null, return %d\n", VERR_INTERNAL_ERROR));
        return VERR_INTERNAL_ERROR;
    }
    /* Prevent going into "paused" state until all transmissions have been completed. */
    NDIS_WAIT_FOR_MUTEX(&pModule->InTransmit);
    /* Ignore all sends if the stack is paused or being paused, etc... */
    if (!vboxNetLwfWinIsRunning(pModule))
    {
        NDIS_RELEASE_MUTEX(&pModule->InTransmit);
        return VINF_SUCCESS;
    }

    const char *pszDir = (fDst & INTNETTRUNKDIR_WIRE) ?
        ( (fDst & INTNETTRUNKDIR_HOST) ? "intnet --> all" : "intnet --> wire" ) : "intnet --> host";
    vboxNetLwfWinDumpPacket(pSG, pszDir);
    /*
     * There are two possible strategies to deal with incoming SGs:
     * 1) make a copy of data and complete asynchronously;
     * 2) complete synchronously using the original data buffers.
     * Before we consider implementing (1) it is quite interesting to see
     * how well (2) performs. So we block until our requests are complete.
     * Actually there is third possibility -- to use SG retain/release
     * callbacks, but those seem not be fully implemented yet.
     * Note that ansynchronous completion will require different implementation
     * of vboxNetLwfWinPause(), not relying on InTransmit mutex.
     */
#ifdef VBOXNETLWF_SYNC_SEND
    PVOID aEvents[2]; /* To wire and to host */
    ULONG nEvents = 0;
    LARGE_INTEGER timeout;
    timeout.QuadPart = -(LONGLONG)10000000; /* 1 sec */
#endif /* VBOXNETLWF_SYNC_SEND */
    if (fDst & INTNETTRUNKDIR_WIRE)
    {
        PNET_BUFFER_LIST pBufList = vboxNetLwfWinSGtoNB(pModule, pSG);
        if (pBufList)
        {
#ifdef VBOXNETLWF_SYNC_SEND
            aEvents[nEvents++] = &pModule->EventWire;
#else /* !VBOXNETLWF_SYNC_SEND */
            if (ASMAtomicIncS32(&pModule->cPendingBuffers) == 1)
                NdisResetEvent(&pModule->EventSendComplete);
#endif /* !VBOXNETLWF_SYNC_SEND */
            NdisFSendNetBufferLists(pModule->hFilter, pBufList, NDIS_DEFAULT_PORT_NUMBER, 0); /** @todo sendFlags! */
        }
    }
    if (fDst & INTNETTRUNKDIR_HOST)
    {
        PNET_BUFFER_LIST pBufList = vboxNetLwfWinSGtoNB(pModule, pSG);
        if (pBufList)
        {
#ifdef VBOXNETLWF_SYNC_SEND
            aEvents[nEvents++] = &pModule->EventHost;
#else /* !VBOXNETLWF_SYNC_SEND */
            if (ASMAtomicIncS32(&pModule->cPendingBuffers) == 1)
                NdisResetEvent(&pModule->EventSendComplete);
#endif /* !VBOXNETLWF_SYNC_SEND */
            NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pBufList, NDIS_DEFAULT_PORT_NUMBER, 1, 0);
        }
    }
#ifdef VBOXNETLWF_SYNC_SEND
    if (nEvents)
    {
        NTSTATUS Status = KeWaitForMultipleObjects(nEvents, aEvents, WaitAll, Executive, KernelMode, FALSE, &timeout, NULL);
        if (Status != STATUS_SUCCESS)
        {
            Log(("ERROR! "__FUNCTION__": KeWaitForMultipleObjects() failed with 0x%x\n", Status));
            if (Status == STATUS_TIMEOUT)
                rc = VERR_TIMEOUT;
            else
                rc = RTErrConvertFromNtStatus(Status);
        }
    }
#endif /* VBOXNETLWF_SYNC_SEND */
    NDIS_RELEASE_MUTEX(&pModule->InTransmit);

    LogFlow(("<=="__FUNCTION__": return %d\n", rc));
    return rc;
}

void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>"__FUNCTION__": instance=%p module=%p fActive=%RTbool\n", pThis, pModuleCtx, fActive));
    if (!pModuleCtx)
    {
        LogFlow(("<=="__FUNCTION__": pModuleCtx is null\n"));
        return;
    }

    NDIS_STATUS Status = STATUS_SUCCESS;
    bool fOldActive = ASMAtomicXchgBool(&pModuleCtx->fActive, fActive);
    if (fOldActive != fActive)
    {
        /// @todo Shouldn't we wait for traffic to cease here? Probably not.
        /* Schedule restart to enable/disable bypass mode */
        NdisFRestartFilter(pModuleCtx->hFilter);
        Status = vboxNetLwfWinSetPacketFilter(pModuleCtx, fActive);
        LogFlow(("<=="__FUNCTION__": vboxNetLwfWinSetPacketFilter() returned 0x%x\n", Status));
    }
    else
        LogFlow(("<=="__FUNCTION__": no change, remain %sactive\n", fActive ? "":"in"));
}

int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow(("==>"__FUNCTION__": instance=%p\n", pThis));
    LogFlow(("<=="__FUNCTION__": return 0\n"));
    return VINF_SUCCESS;
}

int vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow(("==>"__FUNCTION__": instance=%p\n", pThis));
    LogFlow(("<=="__FUNCTION__": return 0\n"));
    return VINF_SUCCESS;
}

void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    PVBOXNETLWF_MODULE pModuleCtx = (PVBOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>"__FUNCTION__": instance=%p module=%p\n", pThis, pModuleCtx));
    /* Technically it is possible that the module has already been gone by now. */
    if (pModuleCtx)
    {
        Assert(!pModuleCtx->fActive); /* Deactivation ensures bypass mode */
        pModuleCtx->pNetFlt = NULL;
        pThis->u.s.WinIf.hModuleCtx = NULL;
    }
    LogFlow(("<=="__FUNCTION__"\n"));
}

static void vboxNetLwfWinReportCapabilities(PVBOXNETFLTINS pThis, PVBOXNETLWF_MODULE pModuleCtx)
{
    if (pThis->pSwitchPort
        && vboxNetFltTryRetainBusyNotDisconnected(pThis))
    {
        pThis->pSwitchPort->pfnReportMacAddress(pThis->pSwitchPort, &pModuleCtx->MacAddr);
        pThis->pSwitchPort->pfnReportPromiscuousMode(pThis->pSwitchPort,
                                                     vboxNetLwfWinIsPromiscuous(pModuleCtx));
        pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort, 0,
                                                     INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST);
        pThis->pSwitchPort->pfnReportNoPreemptDsts(pThis->pSwitchPort, 0 /* none */);
        vboxNetFltRelease(pThis, true /*fBusy*/);
    }
}

int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    LogFlow(("==>"__FUNCTION__": instance=%p context=%p\n", pThis, pvContext));
    AssertReturn(pThis, VERR_INVALID_PARAMETER);
    Log((__FUNCTION__": trunk name=%s\n", pThis->szName));
    ANSI_STRING strInst;
    RtlInitAnsiString(&strInst, pThis->szName);
    PVBOXNETLWF_MODULE pModuleCtx = NULL;
    RTListForEach(&g_VBoxNetLwfGlobals.listModules, pModuleCtx, VBOXNETLWF_MODULE, node)
    {
        DbgPrint(__FUNCTION__": evaluating module, name=%Z\n", pModuleCtx->strMiniportName);
        if (RtlEqualString(&strInst, &pModuleCtx->strMiniportName, TRUE))
        {
            Log((__FUNCTION__": found matching module, name=%s\n", pThis->szName));
            pThis->u.s.WinIf.hModuleCtx = pModuleCtx;
            pModuleCtx->pNetFlt = pThis;
            vboxNetLwfWinReportCapabilities(pThis, pModuleCtx);
            LogFlow(("<=="__FUNCTION__": return 0\n"));
            return VINF_SUCCESS;
        }
    }
    LogFlow(("<=="__FUNCTION__": return VERR_INTNET_FLT_IF_NOT_FOUND\n"));
    return VERR_INTNET_FLT_IF_NOT_FOUND;
}

int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    LogFlow(("==>"__FUNCTION__": instance=%p\n", pThis));
    pThis->u.s.WinIf.hModuleCtx = 0;
    LogFlow(("<=="__FUNCTION__": return 0\n"));
    return VINF_SUCCESS;
}

void vboxNetFltPortOsNotifyMacAddress(PVBOXNETFLTINS pThis, void *pvIfData, PCRTMAC pMac)
{
    LogFlow(("==>"__FUNCTION__": instance=%p data=%p mac=%RTmac\n", pThis, pvIfData, pMac));
    LogFlow(("<=="__FUNCTION__"\n"));
}

int vboxNetFltPortOsConnectInterface(PVBOXNETFLTINS pThis, void *pvIf, void **ppvIfData)
{
    LogFlow(("==>"__FUNCTION__": instance=%p if=%p data=%p\n", pThis, pvIf, ppvIfData));
    LogFlow(("<=="__FUNCTION__": return 0\n"));
    /* Nothing to do */
    return VINF_SUCCESS;
}

int vboxNetFltPortOsDisconnectInterface(PVBOXNETFLTINS pThis, void *pvIfData)
{
    LogFlow(("==>"__FUNCTION__": instance=%p data=%p\n", pThis, pvIfData));
    LogFlow(("<=="__FUNCTION__": return 0\n"));
    /* Nothing to do */
    return VINF_SUCCESS;
}

