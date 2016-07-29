/* $Id$ */
/** @file
 * VBoxNetAdp-win.cpp - NDIS6 Host-only Networking Driver, Windows-specific code.
 */
/*
 * Copyright (C) 2014-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV

#include <VBox/log.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>

#include <iprt/nt/ndis.h>

#include "VBoxNetAdp-win.h"
#include "VBox/VBoxNetCmn-win.h"

/*
 * By default the link speed reported to be 1Gbps. We may wish to lower
 * it to 100Mbps to work around issues with multi-cast traffic on the host.
 * See @bugref{6379}.
 */
#define VBOXNETADPWIN_LINK_SPEED 1000000000ULL

/* Forward declarations */
MINIPORT_INITIALIZE                vboxNetAdpWinInitializeEx;
MINIPORT_HALT                      vboxNetAdpWinHaltEx;
MINIPORT_UNLOAD                    vboxNetAdpWinUnload;
MINIPORT_PAUSE                     vboxNetAdpWinPause;
MINIPORT_RESTART                   vboxNetAdpWinRestart;
MINIPORT_OID_REQUEST               vboxNetAdpWinOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS     vboxNetAdpWinSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS   vboxNetAdpWinReturnNetBufferLists;
MINIPORT_CANCEL_SEND               vboxNetAdpWinCancelSend;
MINIPORT_CHECK_FOR_HANG            vboxNetAdpWinCheckForHangEx;
MINIPORT_RESET                     vboxNetAdpWinResetEx;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY   vboxNetAdpWinDevicePnPEventNotify;
MINIPORT_SHUTDOWN                  vboxNetAdpWinShutdownEx;
MINIPORT_CANCEL_OID_REQUEST        vboxNetAdpWinCancelOidRequest;


typedef struct _VBOXNETADPGLOBALS
{
    /** ndis device */
    NDIS_HANDLE hDevice;
    /** device object */
    PDEVICE_OBJECT pDevObj;
    /** our miniport driver handle */
    NDIS_HANDLE hMiniportDriver;
    /** power management capabilities, shared by all instances, do not change after init */
    NDIS_PNP_CAPABILITIES PMCaps;
} VBOXNETADPGLOBALS, *PVBOXNETADPGLOBALS;

/* win-specific global data */
VBOXNETADPGLOBALS g_VBoxNetAdpGlobals;


typedef struct _VBOXNETADP_ADAPTER {
    NDIS_HANDLE hAdapter;
    PVBOXNETADPGLOBALS pGlobals;
    RTMAC MacAddr;
} VBOXNETADP_ADAPTER;
typedef VBOXNETADP_ADAPTER *PVBOXNETADP_ADAPTER;


static NTSTATUS vboxNetAdpWinDevDispatch(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    PIO_STACK_LOCATION pIrpSl = IoGetCurrentIrpStackLocation(pIrp);;
    NTSTATUS Status = STATUS_SUCCESS;

    switch (pIrpSl->MajorFunction)
    {
        case IRP_MJ_DEVICE_CONTROL:
            Status = STATUS_NOT_SUPPORTED; // TODO: add/remove ioctls
            break;
        case IRP_MJ_CREATE:
        case IRP_MJ_CLEANUP:
        case IRP_MJ_CLOSE:
            break;
        default:
            AssertFailed();
            break;
    }

    pIrp->IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return Status;
}

static NDIS_STATUS vboxNetAdpWinDevCreate(PVBOXNETADPGLOBALS pGlobals)
{
    NDIS_STRING DevName, LinkName;
    PDRIVER_DISPATCH aMajorFunctions[IRP_MJ_MAXIMUM_FUNCTION+1];
    NdisInitUnicodeString(&DevName, VBOXNETADP_NAME_DEVICE);
    NdisInitUnicodeString(&LinkName, VBOXNETADP_NAME_LINK);

    Assert(!pGlobals->hDevice);
    Assert(!pGlobals->pDevObj);
    NdisZeroMemory(aMajorFunctions, sizeof (aMajorFunctions));
    aMajorFunctions[IRP_MJ_CREATE] = vboxNetAdpWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLEANUP] = vboxNetAdpWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLOSE] = vboxNetAdpWinDevDispatch;
    aMajorFunctions[IRP_MJ_DEVICE_CONTROL] = vboxNetAdpWinDevDispatch;

    NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceAttributes;
    NdisZeroMemory(&DeviceAttributes, sizeof(DeviceAttributes));
    DeviceAttributes.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
    DeviceAttributes.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttributes.Header.Size = sizeof(DeviceAttributes);
    DeviceAttributes.DeviceName = &DevName;
    DeviceAttributes.SymbolicName = &LinkName;
    DeviceAttributes.MajorFunctions = aMajorFunctions;

    NDIS_STATUS Status = NdisRegisterDeviceEx(pGlobals->hMiniportDriver,
                                              &DeviceAttributes,
                                              &pGlobals->pDevObj,
                                              &pGlobals->hDevice);
    Log(("vboxNetAdpWinDevCreate: NdisRegisterDeviceEx returned 0x%x\n", Status));
    Assert(Status == NDIS_STATUS_SUCCESS);
    return Status;
}

static void vboxNetAdpWinDevDestroy(PVBOXNETADPGLOBALS pGlobals)
{
    Assert(pGlobals->hDevice);
    Assert(pGlobals->pDevObj);
    NdisDeregisterDeviceEx(pGlobals->hDevice);
    pGlobals->hDevice = NULL;
    pGlobals->pDevObj = NULL;
}





NDIS_OID g_SupportedOids[] =
{
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_INTERRUPT_MODERATION,
    OID_GEN_LINK_PARAMETERS,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_RCV_OK,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_STATISTICS,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_VENDOR_ID,
    OID_GEN_XMIT_OK,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_PNP_CAPABILITIES,
    OID_PNP_QUERY_POWER,
    OID_PNP_SET_POWER
};

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinAllocAdapter(NDIS_HANDLE hAdapter, PVBOXNETADP_ADAPTER *ppAdapter, ULONG uIfIndex)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PVBOXNETADP_ADAPTER pAdapter = NULL;

    LogFlow(("==>vboxNetAdpWinAllocAdapter: adapter handle=%p\n", hAdapter));

    *ppAdapter = NULL;

    pAdapter = (PVBOXNETADP_ADAPTER)NdisAllocateMemoryWithTagPriority(g_VBoxNetAdpGlobals.hMiniportDriver,
                                                                         sizeof(VBOXNETADP_ADAPTER),
                                                                         VBOXNETADPWIN_TAG,
                                                                         NormalPoolPriority);
    if (!pAdapter)
    {
        Status = NDIS_STATUS_RESOURCES;
        Log(("vboxNetAdpWinAllocAdapter: Out of memory while allocating adapter context (size=%d)\n", sizeof(VBOXNETADP_ADAPTER)));
    }
    else
    {
        NdisZeroMemory(pAdapter, sizeof(VBOXNETADP_ADAPTER));
        pAdapter->hAdapter = hAdapter;
        pAdapter->pGlobals = &g_VBoxNetAdpGlobals;
        // TODO: Use netadp structure instead!
    /* Use a locally administered version of the OUI we use for the guest NICs. */
    pAdapter->MacAddr.au8[0] = 0x08 | 2;
    pAdapter->MacAddr.au8[1] = 0x00;
    pAdapter->MacAddr.au8[2] = 0x27;

    pAdapter->MacAddr.au8[3] = (uIfIndex >> 16) & 0xFF;
    pAdapter->MacAddr.au8[4] = (uIfIndex >> 8) & 0xFF;
    pAdapter->MacAddr.au8[5] = uIfIndex & 0xFF;

        //TODO: Statistics?

        *ppAdapter = pAdapter;
    }
    LogFlow(("<==vboxNetAdpWinAllocAdapter: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(void) vboxNetAdpWinFreeAdapter(PVBOXNETADP_ADAPTER pAdapter)
{
    NdisFreeMemory(pAdapter, 0, 0);
}

DECLINLINE(NDIS_MEDIA_CONNECT_STATE) vboxNetAdpWinGetConnectState(PVBOXNETADP_ADAPTER pAdapter)
{
    return MediaConnectStateConnected;
}


DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinInitializeEx(IN NDIS_HANDLE NdisMiniportHandle,
                                                  IN NDIS_HANDLE MiniportDriverContext,
                                                  IN PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters)
{
    PVBOXNETADP_ADAPTER pAdapter = NULL;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    LogFlow(("==>vboxNetAdpWinInitializeEx: miniport=0x%x\n", NdisMiniportHandle));

    do
    {
        NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES RAttrs = {0};
        NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES GAttrs = {0};

        Status = vboxNetAdpWinAllocAdapter(NdisMiniportHandle, &pAdapter, MiniportInitParameters->IfIndex);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vboxNetAdpWinInitializeEx: Failed to allocate the adapter context with 0x%x\n", Status));
            break;
        }

        RAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
        RAttrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        RAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        RAttrs.MiniportAdapterContext = pAdapter;
        RAttrs.AttributeFlags = VBOXNETADPWIN_ATTR_FLAGS; // NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM
        RAttrs.CheckForHangTimeInSeconds = VBOXNETADPWIN_HANG_CHECK_TIME;
        RAttrs.InterfaceType = NdisInterfaceInternal;

        Status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                                            (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&RAttrs);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vboxNetAdpWinInitializeEx: NdisMSetMiniportAttributes(registration) failed with 0x%x\n", Status));
            break;
        }

        // TODO: Registry?

        // TODO: WDM stack?

        // TODO: DPC?

        GAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
        GAttrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
        GAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;

        GAttrs.MediaType = NdisMedium802_3;
        GAttrs.PhysicalMediumType = NdisPhysicalMediumUnspecified;
        GAttrs.MtuSize = 1500; //TODO
        GAttrs.MaxXmitLinkSpeed = VBOXNETADPWIN_LINK_SPEED;
        GAttrs.XmitLinkSpeed = VBOXNETADPWIN_LINK_SPEED;
        GAttrs.MaxRcvLinkSpeed = VBOXNETADPWIN_LINK_SPEED;
        GAttrs.RcvLinkSpeed = VBOXNETADPWIN_LINK_SPEED;
        GAttrs.MediaConnectState = vboxNetAdpWinGetConnectState(pAdapter);
        GAttrs.MediaDuplexState = MediaDuplexStateFull;
        GAttrs.LookaheadSize = 1500; //TODO
        GAttrs.MacOptions = VBOXNETADP_MAC_OPTIONS;
        GAttrs.SupportedPacketFilters = VBOXNETADP_SUPPORTED_FILTERS;
        GAttrs.MaxMulticastListSize = 32; //TODO

        GAttrs.MacAddressLength = ETH_LENGTH_OF_ADDRESS;
        Assert(GAttrs.MacAddressLength == sizeof(pAdapter->MacAddr));
        memcpy(GAttrs.PermanentMacAddress, pAdapter->MacAddr.au8, GAttrs.MacAddressLength);
        memcpy(GAttrs.CurrentMacAddress, pAdapter->MacAddr.au8, GAttrs.MacAddressLength);

        GAttrs.RecvScaleCapabilities = NULL;
        GAttrs.AccessType = NET_IF_ACCESS_BROADCAST;
        GAttrs.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
        GAttrs.ConnectionType = NET_IF_CONNECTION_DEDICATED;
        GAttrs.IfType = IF_TYPE_ETHERNET_CSMACD;
        GAttrs.IfConnectorPresent = false;
        GAttrs.SupportedStatistics = VBOXNETADPWIN_SUPPORTED_STATISTICS;
        GAttrs.SupportedPauseFunctions = NdisPauseFunctionsUnsupported;
        GAttrs.DataBackFillSize = 0;
        GAttrs.ContextBackFillSize = 0;
        GAttrs.SupportedOidList = g_SupportedOids;
        GAttrs.SupportedOidListLength = sizeof(g_SupportedOids);
        GAttrs.AutoNegotiationFlags = NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED;
        GAttrs.PowerManagementCapabilities = &g_VBoxNetAdpGlobals.PMCaps;

        Status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                                            (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&GAttrs);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vboxNetAdpWinInitializeEx: NdisMSetMiniportAttributes(general) failed with 0x%x\n", Status));
            break;
        }
    } while (false);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        if (pAdapter)
            vboxNetAdpWinFreeAdapter(pAdapter);
    }

    LogFlow(("<==vboxNetAdpWinInitializeEx: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vboxNetAdpWinHaltEx(IN NDIS_HANDLE MiniportAdapterContext,
                                     IN NDIS_HALT_ACTION HaltAction)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinHaltEx\n"));
    // TODO: Stop something?
    if (pAdapter)
        vboxNetAdpWinFreeAdapter(pAdapter);
    LogFlow(("<==vboxNetAdpWinHaltEx\n"));
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinPause(IN NDIS_HANDLE MiniportAdapterContext,
                                           IN PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vboxNetAdpWinPause\n"));
    LogFlow(("<==vboxNetAdpWinPause: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinRestart(IN NDIS_HANDLE MiniportAdapterContext,
                                             IN PNDIS_MINIPORT_RESTART_PARAMETERS MiniportRestartParameters)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vboxNetAdpWinRestart\n"));
    LogFlow(("<==vboxNetAdpWinRestart: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinOidRqQuery(PVBOXNETADP_ADAPTER pAdapter,
                                                PNDIS_OID_REQUEST pRequest)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    struct _NDIS_OID_REQUEST::_REQUEST_DATA::_QUERY *pQuery = &pRequest->DATA.QUERY_INFORMATION;

    LogFlow(("==>vboxNetAdpWinOidRqQuery\n"));

    uint64_t u64Tmp = 0;
    ULONG ulTmp = 0;
    PVOID pInfo = &ulTmp;
    ULONG cbInfo = sizeof(ulTmp);

    switch (pQuery->Oid)
    {
        case OID_GEN_INTERRUPT_MODERATION:
        {
            PNDIS_INTERRUPT_MODERATION_PARAMETERS pParams =
                (PNDIS_INTERRUPT_MODERATION_PARAMETERS)pQuery->InformationBuffer;
            cbInfo = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            if (cbInfo > pQuery->InformationBufferLength)
                break;
            pParams->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            pParams->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            pParams->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            pParams->Flags = 0;
            pParams->InterruptModeration = NdisInterruptModerationNotSupported;
            pInfo = NULL; /* Do not copy */
            break;
        }
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
            ulTmp = VBOXNETADP_MAX_FRAME_SIZE;
            break;
        case OID_GEN_RCV_OK:
        case OID_GEN_XMIT_OK:
            u64Tmp = 0;
            pInfo = &u64Tmp;
            cbInfo = sizeof(u64Tmp);
            break;
        case OID_GEN_RECEIVE_BUFFER_SPACE:
        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            // TODO: Make configurable
            ulTmp = VBOXNETADP_MAX_FRAME_SIZE * 40;
            break;
        case OID_GEN_STATISTICS:
        {
            PNDIS_STATISTICS_INFO pStats =
                (PNDIS_STATISTICS_INFO)pQuery->InformationBuffer;
            cbInfo = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
            if (cbInfo > pQuery->InformationBufferLength)
                break;
            pInfo = NULL; /* Do not copy */
            memset(pStats, 0, cbInfo);
            pStats->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            pStats->Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;
            pStats->Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
            // TODO: We need some stats, don't we?
            break;
        }
        case OID_GEN_VENDOR_DESCRIPTION:
            pInfo = VBOXNETADP_VENDOR_NAME;
            cbInfo = sizeof(VBOXNETADP_VENDOR_NAME);
            break;
        case OID_GEN_VENDOR_DRIVER_VERSION:
            ulTmp = (VBOXNETADP_VERSION_NDIS_MAJOR << 16) | VBOXNETADP_VERSION_NDIS_MINOR;
            break;
        case OID_GEN_VENDOR_ID:
            ulTmp = VBOXNETADP_VENDOR_ID;
            break;
        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
            pInfo = &pAdapter->MacAddr;
            cbInfo = sizeof(pAdapter->MacAddr);
            break;
            //case OID_802_3_MULTICAST_LIST:
        case OID_802_3_MAXIMUM_LIST_SIZE:
            ulTmp = VBOXNETADP_MCAST_LIST_SIZE;
            break;
        case OID_PNP_CAPABILITIES:
            pInfo = &pAdapter->pGlobals->PMCaps;
            cbInfo = sizeof(pAdapter->pGlobals->PMCaps);
            break;
        case OID_PNP_QUERY_POWER:
            pInfo = NULL; /* Do not copy */
            cbInfo = 0;
            break;
        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (cbInfo > pQuery->InformationBufferLength)
        {
            pQuery->BytesNeeded = cbInfo;
            Status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
        else
        {
            if (pInfo)
                NdisMoveMemory(pQuery->InformationBuffer, pInfo, cbInfo);
            pQuery->BytesWritten = cbInfo;
        }
    }

    LogFlow(("<==vboxNetAdpWinOidRqQuery: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinOidRqSet(PVBOXNETADP_ADAPTER pAdapter,
                                              PNDIS_OID_REQUEST pRequest)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    struct _NDIS_OID_REQUEST::_REQUEST_DATA::_SET *pSet = &pRequest->DATA.SET_INFORMATION;

    LogFlow(("==>vboxNetAdpWinOidRqSet\n"));

    switch (pSet->Oid)
    {
        case OID_GEN_CURRENT_LOOKAHEAD:
            if (pSet->InformationBufferLength != sizeof(ULONG))
            {
                pSet->BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            // TODO: For the time being we simply ignore lookahead settings.
            pSet->BytesRead = sizeof(ULONG);
            Status = NDIS_STATUS_SUCCESS;
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            if (pSet->InformationBufferLength != sizeof(ULONG))
            {
                pSet->BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            // TODO: For the time being we simply ignore packet filter settings.
            pSet->BytesRead = pSet->InformationBufferLength;
            Status = NDIS_STATUS_SUCCESS;
            break;

        case OID_GEN_INTERRUPT_MODERATION:
            pSet->BytesNeeded = 0;
            pSet->BytesRead = 0;
            Status = NDIS_STATUS_INVALID_DATA;
            break;

        case OID_PNP_SET_POWER:
            if (pSet->InformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE))
            {
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            pSet->BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
            Status = NDIS_STATUS_SUCCESS;
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    LogFlow(("<==vboxNetAdpWinOidRqSet: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinOidRequest(IN NDIS_HANDLE MiniportAdapterContext,
                                                IN PNDIS_OID_REQUEST NdisRequest)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinOidRequest\n"));
    vboxNetCmnWinDumpOidRequest(__FUNCTION__, NdisRequest);

    switch (NdisRequest->RequestType)
    {
#if 0
        case NdisRequestMethod:
            Status = vboxNetAdpWinOidRqMethod(pAdapter, NdisRequest);
            break;
#endif

        case NdisRequestSetInformation:
            Status = vboxNetAdpWinOidRqSet(pAdapter, NdisRequest);
            break;

        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
            Status = vboxNetAdpWinOidRqQuery(pAdapter, NdisRequest);
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }
    LogFlow(("<==vboxNetAdpWinOidRequest: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vboxNetAdpWinSendNetBufferLists(IN NDIS_HANDLE MiniportAdapterContext,
                                                 IN PNET_BUFFER_LIST NetBufferLists,
                                                 IN NDIS_PORT_NUMBER PortNumber,
                                                 IN ULONG SendFlags)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinSendNetBufferLists\n"));
    PNET_BUFFER_LIST pNbl = NetBufferLists;
    for (pNbl = NetBufferLists; pNbl; pNbl = NET_BUFFER_LIST_NEXT_NBL(pNbl))
        NET_BUFFER_LIST_STATUS(pNbl) = NDIS_STATUS_SUCCESS;
    NdisMSendNetBufferListsComplete(pAdapter->hAdapter, NetBufferLists,
                                    (SendFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL) ?
                                    NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
    LogFlow(("<==vboxNetAdpWinSendNetBufferLists\n"));
}

DECLHIDDEN(VOID) vboxNetAdpWinReturnNetBufferLists(IN NDIS_HANDLE MiniportAdapterContext,
                                                   IN PNET_BUFFER_LIST NetBufferLists,
                                                   IN ULONG ReturnFlags)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinReturnNetBufferLists\n"));
    Log(("vboxNetAdpWinReturnNetBufferLists: We should not be here!\n"));
    LogFlow(("<==vboxNetAdpWinReturnNetBufferLists\n"));
}

DECLHIDDEN(VOID) vboxNetAdpWinCancelSend(IN NDIS_HANDLE MiniportAdapterContext,
                                         IN PVOID CancelId)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinCancelSend\n"));
    Log(("vboxNetAdpWinCancelSend: We should not be here!\n"));
    LogFlow(("<==vboxNetAdpWinCancelSend\n"));
}


DECLHIDDEN(BOOLEAN) vboxNetAdpWinCheckForHangEx(IN NDIS_HANDLE MiniportAdapterContext)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinCheckForHangEx\n"));
    LogFlow(("<==vboxNetAdpWinCheckForHangEx return false\n"));
    return FALSE;
}

DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinResetEx(IN NDIS_HANDLE MiniportAdapterContext,
                                             OUT PBOOLEAN AddressingReset)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vboxNetAdpWinResetEx\n"));
    LogFlow(("<==vboxNetAdpWinResetEx: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vboxNetAdpWinDevicePnPEventNotify(IN NDIS_HANDLE MiniportAdapterContext,
                                                   IN PNET_DEVICE_PNP_EVENT NetDevicePnPEvent)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinDevicePnPEventNotify\n"));
    Log(("vboxNetAdpWinDevicePnPEventNotify: PnP event=%d\n", NetDevicePnPEvent->DevicePnPEvent));
    LogFlow(("<==vboxNetAdpWinDevicePnPEventNotify\n"));
}


DECLHIDDEN(VOID) vboxNetAdpWinShutdownEx(IN NDIS_HANDLE MiniportAdapterContext,
                                         IN NDIS_SHUTDOWN_ACTION ShutdownAction)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinShutdownEx\n"));
    Log(("vboxNetAdpWinShutdownEx: action=%d\n", ShutdownAction));
    LogFlow(("<==vboxNetAdpWinShutdownEx\n"));
}

DECLHIDDEN(VOID) vboxNetAdpWinCancelOidRequest(IN NDIS_HANDLE MiniportAdapterContext,
                                               IN PVOID RequestId)
{
    PVBOXNETADP_ADAPTER pAdapter = (PVBOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vboxNetAdpWinCancelOidRequest\n"));
    Log(("vboxNetAdpWinCancelOidRequest: req id=%p\n", RequestId));
    LogFlow(("<==vboxNetAdpWinCancelOidRequest\n"));
}



DECLHIDDEN(VOID) vboxNetAdpWinUnload(IN PDRIVER_OBJECT DriverObject)
{
    LogFlow(("==>vboxNetAdpWinUnload\n"));
    //vboxNetAdpWinDevDestroy(&g_VBoxNetAdpGlobals);
    if (g_VBoxNetAdpGlobals.hMiniportDriver)
        NdisMDeregisterMiniportDriver(g_VBoxNetAdpGlobals.hMiniportDriver);
    //NdisFreeSpinLock(&g_VBoxNetAdpGlobals.Lock);
    LogFlow(("<==vboxNetAdpWinUnload\n"));
    RTR0Term();
}


/**
 * register the miniport driver
 */
DECLHIDDEN(NDIS_STATUS) vboxNetAdpWinRegister(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr)
{
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS MChars;

    NdisZeroMemory(&MChars, sizeof (MChars));

    MChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    MChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    MChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

    MChars.MajorNdisVersion = VBOXNETADP_VERSION_NDIS_MAJOR;
    MChars.MinorNdisVersion = VBOXNETADP_VERSION_NDIS_MINOR;

    MChars.MajorDriverVersion = VBOXNETADP_VERSION_MAJOR;
    MChars.MinorDriverVersion = VBOXNETADP_VERSION_MINOR;

    MChars.InitializeHandlerEx         = vboxNetAdpWinInitializeEx;
    MChars.HaltHandlerEx               = vboxNetAdpWinHaltEx;
    MChars.UnloadHandler               = vboxNetAdpWinUnload;
    MChars.PauseHandler                = vboxNetAdpWinPause;
    MChars.RestartHandler              = vboxNetAdpWinRestart;
    MChars.OidRequestHandler           = vboxNetAdpWinOidRequest;
    MChars.SendNetBufferListsHandler   = vboxNetAdpWinSendNetBufferLists;
    MChars.ReturnNetBufferListsHandler = vboxNetAdpWinReturnNetBufferLists;
    MChars.CancelSendHandler           = vboxNetAdpWinCancelSend;
    MChars.CheckForHangHandlerEx       = vboxNetAdpWinCheckForHangEx;
    MChars.ResetHandlerEx              = vboxNetAdpWinResetEx;
    MChars.DevicePnPEventNotifyHandler = vboxNetAdpWinDevicePnPEventNotify;
    MChars.ShutdownHandlerEx           = vboxNetAdpWinShutdownEx;
    MChars.CancelOidRequestHandler     = vboxNetAdpWinCancelOidRequest;

    NDIS_STATUS Status;
    g_VBoxNetAdpGlobals.hMiniportDriver = NULL;
    Log(("vboxNetAdpWinRegister: registering miniport driver...\n"));
    Status = NdisMRegisterMiniportDriver(pDriverObject,
                                         pRegistryPathStr,
                                         (NDIS_HANDLE)&g_VBoxNetAdpGlobals,
                                         &MChars,
                                         &g_VBoxNetAdpGlobals.hMiniportDriver);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Log(("vboxNetAdpWinRegister: successfully registered miniport driver; registering device...\n"));
        //Status = vboxNetAdpWinDevCreate(&g_VBoxNetAdpGlobals);
        //Assert(Status == STATUS_SUCCESS);
        //Log(("vboxNetAdpWinRegister: vboxNetAdpWinDevCreate() returned 0x%x\n", Status));
    }
    else
    {
        Log(("ERROR! vboxNetAdpWinRegister: failed to register miniport driver, status=0x%x", Status));
    }
    return Status;
}


RT_C_DECLS_BEGIN

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);

RT_C_DECLS_END

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int rc;


    rc = RTR0Init(0);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        NdisZeroMemory(&g_VBoxNetAdpGlobals, sizeof (g_VBoxNetAdpGlobals));
        //NdisAllocateSpinLock(&g_VBoxNetAdpGlobals.Lock);
        //g_VBoxNetAdpGlobals.PMCaps.WakeUpCapabilities.Flags = NDIS_DEVICE_WAKE_UP_ENABLE;
        g_VBoxNetAdpGlobals.PMCaps.WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
        g_VBoxNetAdpGlobals.PMCaps.WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateUnspecified;

        Status = vboxNetAdpWinRegister(pDriverObject, pRegistryPath);
        Assert(Status == STATUS_SUCCESS);
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Log(("NETADP: started successfully\n"));
            return STATUS_SUCCESS;
        }
        //NdisFreeSpinLock(&g_VBoxNetAdpGlobals.Lock);
        RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
        RTLogDestroy(RTLogSetDefaultInstance(NULL));

        RTR0Term();
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    return Status;
}

