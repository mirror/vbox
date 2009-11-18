/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Miniport edge of ndis filter driver
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

#include "VBoxNetFltCommon-win.h"

#ifdef VBOX_NETFLT_ONDEMAND_BIND
# error "unsupported (VBOX_NETFLT_ONDEMAND_BIND)"
#else

/** driver handle */
static NDIS_HANDLE         g_hDriverHandle = NULL;
/** Ndis wrapper handle */
static NDIS_HANDLE        g_hNdisWrapperHandle;
/** device handle for ioctl interface this is not used currently and should be removed soon */
static NDIS_HANDLE     g_hNdisDeviceHandle = NULL;
/** device object used for ioctl interface this is not used currently and should be removed soon */
static PDEVICE_OBJECT  g_pControlDeviceObject = NULL;
/** ioctl device ref count */
static LONG               g_cControlDeviceRefs = 0;
/** true if control device needs to be dereferenced before destroying */
static bool g_bControlDeviceReferenced = false;

enum _DEVICE_STATE
{
    /** ready for create/delete */
    PS_DEVICE_STATE_READY = 0,
    /** create operation in progress */
    PS_DEVICE_STATE_CREATING,
    /** delete operation in progress */
    PS_DEVICE_STATE_DELETING
} g_eControlDeviceState = PS_DEVICE_STATE_READY;

/*
 * miniport
 */
typedef struct {
    PVOID aBuffer[8];
}OUR_SECURITY_DESCRIPTOR_BUFFER;

static PUCHAR vboxNetFltWinMpDbgGetOidName(ULONG oid);

NTSYSAPI
NTSTATUS
NTAPI
ZwSetSecurityObject(IN HANDLE hHandle,
        IN SECURITY_INFORMATION SInfo,
        IN PSECURITY_DESCRIPTOR pSDescriptor);
/*
 * makes our device object usable/accessible for non-privileged users
 */
static NTSTATUS vboxNetFltWinSetSecurity(PNDIS_STRING pDevName)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    IO_STATUS_BLOCK IoStatus;
    HANDLE hFile;
    OUR_SECURITY_DESCRIPTOR_BUFFER SecurityDes;

    /*obtain the handle first*/
    NdisZeroMemory(&ObjAttr, sizeof(ObjAttr));
    InitializeObjectAttributes(&ObjAttr, pDevName,
                OBJ_KERNEL_HANDLE /* ULONG Attributes */,
                NULL /*HANDLE  RootDirectory*/,
                NULL /*PSECURITY_DESCRIPTOR  SecurityDescriptor */
               );

    NdisZeroMemory(&IoStatus, sizeof(IoStatus));
    Status = ZwOpenFile(&hFile /* PHANDLE  FileHandle*/,
                WRITE_DAC /*ACCESS_MASK  DesiredAccess - we want to change the ACL */,
                &ObjAttr /*POBJECT_ATTRIBUTES */,
                &IoStatus /*PIO_STATUS_BLOCK */,
                0 /*ULONG  ShareAccess*/,
                0 /*ULONG  OpenOptions*/
                );
    Assert(Status == STATUS_SUCCESS);
    if(Status == STATUS_SUCCESS)
    {
        /* create and set security descriptor */
        NdisZeroMemory(&SecurityDes, sizeof(SecurityDes));
        Status = RtlCreateSecurityDescriptor(&SecurityDes, SECURITY_DESCRIPTOR_REVISION);
        Assert(Status == STATUS_SUCCESS);
        if(Status == STATUS_SUCCESS)
        {
            Status = ZwSetSecurityObject(hFile, DACL_SECURITY_INFORMATION, &SecurityDes);
            Assert(Status == STATUS_SUCCESS);
            if(Status != STATUS_SUCCESS)
            {
                LogRel(("ZwSetSecurityObject error: Status (0x%x)\n", Status));
                DBGPRINT(("ZwSetSecurityObject error: Status (0x%x)\n", Status));
            }
        }
        else
        {
            LogRel(("RtlCreateSecurityDescriptor error: Status (0x%x)\n", Status));
            DBGPRINT(("RtlCreateSecurityDescriptor error: Status (0x%x)\n", Status));
        }

        {
            NTSTATUS Tmp = ZwClose(hFile);
            Assert(Tmp == STATUS_SUCCESS);
            if(Tmp != STATUS_SUCCESS)
            {
                LogRel(("ZwClose error: Status (0x%x), ignoring\n", Status));
                DBGPRINT(("ZwClose error: Status (0x%x), ignoring\n", Status));
            }
        }
    }
    else
    {
        LogRel(("ZwOpenFile error: Status (0x%x)\n", Status));
        DBGPRINT(("ZwOpenFile error: Status (0x%x)\n", Status));
    }

    return Status;
}
/**
 * Register an ioctl interface - a device object to be used for this
 * purpose is created by NDIS when we call NdisMRegisterDevice.
 *
 * This routine is called whenever a new miniport instance is
 * initialized. However, we only create one global device object,
 * when the first miniport instance is initialized. This routine
 * handles potential race conditions with vboxNetFltWinPtDeregisterDevice via
 * the g_eControlDeviceState and g_cControlDeviceRefs variables.
 *
 * NOTE: do not call this from DriverEntry; it will prevent the driver
 * from being unloaded (e.g. on uninstall).
 *
 * @return NDIS_STATUS_SUCCESS if we successfully register a device object. */
static NDIS_STATUS
vboxNetFltWinPtRegisterDevice(
    VOID
    )
{
    NDIS_STATUS            Status = NDIS_STATUS_SUCCESS;
    UNICODE_STRING         DeviceName;
    UNICODE_STRING         DeviceLinkUnicodeString;
    PDRIVER_DISPATCH       DispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1];

    LogFlow(("==>vboxNetFltWinPtRegisterDevice\n"));

    NdisAcquireSpinLock(&g_GlobalLock);

    ++g_cControlDeviceRefs;

    if (1 == g_cControlDeviceRefs)
    {
        Assert(g_eControlDeviceState != PS_DEVICE_STATE_CREATING);

        /* Another thread could be running vboxNetFltWinPtDeregisterDevice on
         * behalf of another miniport instance. If so, wait for
         * it to exit. */
        while (g_eControlDeviceState != PS_DEVICE_STATE_READY)
        {
            NdisReleaseSpinLock(&g_GlobalLock);
            NdisMSleep(1);
            NdisAcquireSpinLock(&g_GlobalLock);
        }

        g_eControlDeviceState = PS_DEVICE_STATE_CREATING;

        NdisReleaseSpinLock(&g_GlobalLock);


        NdisZeroMemory(DispatchTable, (IRP_MJ_MAXIMUM_FUNCTION+1) * sizeof(PDRIVER_DISPATCH));

        DispatchTable[IRP_MJ_CREATE] = vboxNetFltWinPtDispatch;
        DispatchTable[IRP_MJ_CLEANUP] = vboxNetFltWinPtDispatch;
        DispatchTable[IRP_MJ_CLOSE] = vboxNetFltWinPtDispatch;
        DispatchTable[IRP_MJ_DEVICE_CONTROL] = vboxNetFltWinPtDispatch;


        NdisInitUnicodeString(&DeviceName, NTDEVICE_STRING);
        NdisInitUnicodeString(&DeviceLinkUnicodeString, LINKNAME_STRING);

        /* Create a device object and register our dispatch handlers */

        Status = NdisMRegisterDevice(
                    g_hNdisWrapperHandle,
                    &DeviceName,
                    &DeviceLinkUnicodeString,
                    &DispatchTable[0],
                    &g_pControlDeviceObject,
                    &g_hNdisDeviceHandle
                    );

        Assert(Status == NDIS_STATUS_SUCCESS);
        if(Status == NDIS_STATUS_SUCCESS)
        {
            /* NdisMRegisterDevice does not offers us the ability to set security attributes */
            /* need to do this "manualy" for the device to be accessible by the non-privileged users */
            Status = vboxNetFltWinSetSecurity(&DeviceLinkUnicodeString);
            Assert(Status == STATUS_SUCCESS);
            if(Status != STATUS_SUCCESS)
            {
                LogRel(("Failed to set security attributes for netflt control device, status (0x%x), ignoring\n", Status));
                /* ignore the failure */
                Status = NDIS_STATUS_SUCCESS;
            }

            Status = ObReferenceObjectByPointer(g_pControlDeviceObject, FILE_READ_DATA, *IoFileObjectType, KernelMode);
            Assert(Status == NDIS_STATUS_SUCCESS);
            if(Status == NDIS_STATUS_SUCCESS)
            {
                g_bControlDeviceReferenced = true;
            }
            else
            {
                LogRel(("Failed to reference netflt control device, status (0x%x), ignoring\n", Status));
                /* ignore the failure */
                Status = NDIS_STATUS_SUCCESS;
                g_bControlDeviceReferenced = false;
            }
        }

        NdisAcquireSpinLock(&g_GlobalLock);

        g_eControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&g_GlobalLock);

    LogFlow(("<==vboxNetFltWinPtRegisterDevice: %x\n", Status));

    return (Status);
}

/**
 * Deregister the ioctl interface. This is called whenever a miniport
 * instance is halted. When the last miniport instance is halted, we
 * request NDIS to delete the device object
 *
 * @return NDIS_STATUS_SUCCESS if everything worked ok
 * */
static NDIS_STATUS
vboxNetFltWinPtDeregisterDevice(
    VOID
    )
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    LogFlow(("==>NetFltDeregisterDevice\n"));

    NdisAcquireSpinLock(&g_GlobalLock);

    Assert(g_cControlDeviceRefs > 0);

    --g_cControlDeviceRefs;

    if (0 == g_cControlDeviceRefs)
    {
        /* All miniport instances have been halted. Deregister
         * the control device. */

        Assert(g_eControlDeviceState == PS_DEVICE_STATE_READY);

        /* Block vboxNetFltWinPtRegisterDevice() while we release the control
         * device lock and deregister the device. */

        g_eControlDeviceState = PS_DEVICE_STATE_DELETING;

        NdisReleaseSpinLock(&g_GlobalLock);

        if (g_hNdisDeviceHandle != NULL)
        {
            if(g_bControlDeviceReferenced)
            {
                g_bControlDeviceReferenced = false;
                ObDereferenceObject(g_pControlDeviceObject);
            }

            Status = NdisMDeregisterDevice(g_hNdisDeviceHandle);
            g_hNdisDeviceHandle = NULL;
        }

        NdisAcquireSpinLock(&g_GlobalLock);
        g_eControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&g_GlobalLock);

    LogFlow(("<== NetFltDeregisterDevice: %x\n", Status));
    return Status;

}
#ifndef VBOXNETADP
/**
 * This is the initialize handler which gets called as a result of
 * the BindAdapter handler calling NdisIMInitializeDeviceInstanceEx.
 * The context parameter which we pass there is the adapter structure
 * which we retrieve here.
 *
 * @param OpenErrorStatus            Not used by us.
 * @param SelectedMediumIndex        Place-holder for what media we are using
 * @param MediumArray                Array of ndis media passed down to us to pick from
 * @param MediumArraySize            Size of the array
 * @param MiniportAdapterHandle    The handle NDIS uses to refer to us
 * @param WrapperConfigurationContext    For use by NdisOpenConfiguration
 * @return NDIS_STATUS_SUCCESS unless something goes wrong
 * */
static NDIS_STATUS vboxNetFltWinMpInitialize(
    OUT PNDIS_STATUS             OpenErrorStatus,
    OUT PUINT                    SelectedMediumIndex,
    IN  PNDIS_MEDIUM             MediumArray,
    IN  UINT                     MediumArraySize,
    IN  NDIS_HANDLE              MiniportAdapterHandle,
    IN  NDIS_HANDLE              WrapperConfigurationContext
    )
{
    UINT            i;
    PADAPT          pAdapt;
    NDIS_STATUS     Status = NDIS_STATUS_FAILURE;
    NDIS_MEDIUM     Medium;

    UNREFERENCED_PARAMETER(WrapperConfigurationContext);

    do
    {
        /*
         * Start off by retrieving our adapter context and storing
         * the Miniport handle in it.
         */
        pAdapt = (PADAPT)NdisIMGetDeviceContext(MiniportAdapterHandle);
        pAdapt->hMiniportHandle = MiniportAdapterHandle;

        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initializing);
        /* the MP state should be already set to kVBoxNetDevOpState_Initializing, just a paranoya
         * in case NDIS for some reason calls us in some unregular way */
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initializing);

        LogFlow(("==> Miniport Initialize: Adapt %p\n", pAdapt));

        /*
         * Usually we export the medium type of the adapter below as our
         * virtual miniport's medium type. However if the adapter below us
         * is a WAN device, then we claim to be of medium type 802.3.
         */
        Medium = pAdapt->Medium;

        if (Medium == NdisMediumWan)
        {
            Medium = NdisMedium802_3;
        }

        for (i = 0; i < MediumArraySize; i++)
        {
            if (MediumArray[i] == Medium)
            {
                *SelectedMediumIndex = i;
                break;
            }
        }

        if (i == MediumArraySize)
        {
            Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
            break;
        }


        /*
         * Set the attributes now. NDIS_ATTRIBUTE_DESERIALIZE enables us
         * to make up-calls to NDIS without having to call NdisIMSwitchToMiniport
         * or NdisIMQueueCallBack. This also forces us to protect our data using
         * spinlocks where appropriate. Also in this case NDIS does not queue
         * packets on our behalf. Since this is a very simple pass-thru
         * miniport, we do not have a need to protect anything. However in
         * a general case there will be a need to use per-adapter spin-locks
         * for the packet queues at the very least.
         */
        NdisMSetAttributesEx(MiniportAdapterHandle,
                             pAdapt,
                             0,                                        /* CheckForHangTimeInSeconds */
                             NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT    |
                                NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT|
                                NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER |
                                NDIS_ATTRIBUTE_DESERIALIZE |
                                NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
                             (NDIS_INTERFACE_TYPE)0);

        /*
         * Initialize LastIndicatedStatus to be NDIS_STATUS_MEDIA_CONNECT
         */
        pAdapt->LastIndicatedStatus = NDIS_STATUS_MEDIA_CONNECT;

        /*
         * Initialize the power states for both the lower binding (PTDeviceState)
         * and our miniport edge to Powered On.
         */
        Assert(vboxNetFltWinGetPowerState(&pAdapt->MPState) == NdisDeviceStateD3);
        vboxNetFltWinSetPowerState(&pAdapt->MPState, NdisDeviceStateD0);
        Assert(pAdapt->MPState.OpState == kVBoxNetDevOpState_Initializing);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initialized);

        Status = NDIS_STATUS_SUCCESS;
    }
    while (FALSE);

    /*
     * If we had received an UnbindAdapter notification on the underlying
     * adapter, we would have blocked that thread waiting for the IM Init
     * process to complete. Wake up any such thread.
     */
    if(Status != NDIS_STATUS_SUCCESS)
    {
        Assert(vboxNetFltWinGetPowerState(&pAdapt->MPState) == NdisDeviceStateD3);
        Assert(pAdapt->MPState.OpState == kVBoxNetDevOpState_Initializing);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
    }
    NdisSetEvent(&pAdapt->MiniportInitEvent);

    LogFlow(("<== Miniport Initialize: Adapt %p, Status %x\n", pAdapt, Status));

    *OpenErrorStatus = Status;

    return Status;
}

/**
 * process the packet send in a "passthru" mode
 */
static NDIS_STATUS
vboxNetFltWinSendPassThru(
    IN PADAPT             pAdapt,
    IN PNDIS_PACKET           pPacket
    )
{
    PNDIS_PACKET           pMyPacket;
    NDIS_STATUS fStatus;

    fStatus = vboxNetFltWinPrepareSendPacket(pAdapt, pPacket, &pMyPacket/*, false*/);

    Assert(fStatus == NDIS_STATUS_SUCCESS);
    if (fStatus == NDIS_STATUS_SUCCESS)
    {
#if !defined(VBOX_LOOPBACK_USEFLAGS) /* || defined(DEBUG_NETFLT_PACKETS) */
        /* no need for the loop enqueue & check in a passthru mode , ndis will do everything for us */
#endif
        NdisSend(&fStatus,
                 pAdapt->hBindingHandle,
                 pMyPacket);
        if (fStatus != NDIS_STATUS_PENDING)
        {
#ifndef WIN9X
            NdisIMCopySendCompletePerPacketInfo (pPacket, pMyPacket);
#endif
            NdisFreePacket(pMyPacket);
        }
    }
    return fStatus;
}

#else /* defined VBOXNETADP */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDoDeinitialization(PADAPT pAdapt)
{
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    uint64_t NanoTS = RTTimeSystemNanoTS();
    int cPPUsage;

    Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
    /*
     * Set the flag that the miniport below is unbinding, so the request handlers will
     * fail any request comming later
     */
    RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

    ASMAtomicUoWriteBool(&pNetFlt->fDisconnectedFromHost, true);
    ASMAtomicUoWriteBool(&pNetFlt->fRediscoveryPending, false);
    ASMAtomicUoWriteU64(&pNetFlt->NanoTSLastRediscovery, NanoTS);

    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitializing);

    RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

    vboxNetFltWinWaitDereference(&pAdapt->MPState);

    /* check packet pool is empty */
    cPPUsage = NdisPacketPoolUsage(pAdapt->hRecvPacketPoolHandle);
    Assert(cPPUsage == 0);
    /* for debugging only, ignore the err in release */
    NOREF(cPPUsage);

    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);

//    pAdapt->hMiniportHandle = NULL;

    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS vboxNetFltWinMpReadApplyConfig(PADAPT pAdapt, NDIS_HANDLE hMiniportAdapter, NDIS_HANDLE hWrapperConfigurationContext)
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    NDIS_HANDLE     hConfiguration;
    PNDIS_CONFIGURATION_PARAMETER pParameterValue;
    NDIS_STRING strMAC = NDIS_STRING_CONST("MAC");
    PVBOXNETFLTINS pThis = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTMAC mac;

    //
    // Open the registry for this adapter to read advanced
    // configuration parameters stored by the INF file.
    //
    NdisOpenConfiguration(
        &Status,
        &hConfiguration,
        hWrapperConfigurationContext);
    Assert(Status == NDIS_STATUS_SUCCESS);
    if(Status == NDIS_STATUS_SUCCESS)
    {
        do
        {
            int rc;
            NDIS_CONFIGURATION_PARAMETER param;
            WCHAR MacBuf[13];


            NdisReadConfiguration(&Status,
                                  &pParameterValue,
                                  hConfiguration,
                                  &strMAC,
                                  NdisParameterString);
//            Assert(Status == NDIS_STATUS_SUCCESS);
            if(Status == NDIS_STATUS_SUCCESS)
            {

                rc = vboxNetFltWinMACFromNdisString(&mac, &pParameterValue->ParameterData.StringData);
                Assert(RT_SUCCESS(rc));
                if(RT_SUCCESS(rc))
                {
                    break;
                }
            }

            vboxNetFltWinGenerateMACAddress(&mac);
            param.ParameterType = NdisParameterString;
            param.ParameterData.StringData.Buffer = MacBuf;
            param.ParameterData.StringData.MaximumLength = sizeof(MacBuf);

            rc = vboxNetFltWinMAC2NdisString(&mac, &param.ParameterData.StringData);
            Assert(RT_SUCCESS(rc));
            if(RT_SUCCESS(rc))
            {
                NdisWriteConfiguration(&Status,
                        hConfiguration,
                        &strMAC,
                        &param);
                Assert(Status == NDIS_STATUS_SUCCESS);
                if(Status != NDIS_STATUS_SUCCESS)
                {
                    /* ignore the failure */
                    Status = NDIS_STATUS_SUCCESS;
                }
            }
        }while(0);

        NdisCloseConfiguration(hConfiguration);
    }
    else
    {
        vboxNetFltWinGenerateMACAddress(&mac);
    }

    pThis->u.s.Mac = mac;

    return NDIS_STATUS_SUCCESS;
}

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDoInitialization(PADAPT pAdapt, NDIS_HANDLE hMiniportAdapter, NDIS_HANDLE hWrapperConfigurationContext)
{
    NDIS_STATUS Status;
    pAdapt->hMiniportHandle = hMiniportAdapter;

    LogFlow(("==> vboxNetFltWinMpDoInitialization: Adapt %p\n", pAdapt));

    Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initializing);

    vboxNetFltWinMpReadApplyConfig(pAdapt, hMiniportAdapter, hWrapperConfigurationContext);

    NdisMSetAttributesEx(hMiniportAdapter,
                         pAdapt,
                         0,                                        /* CheckForHangTimeInSeconds */
                            NDIS_ATTRIBUTE_DESERIALIZE |
                            NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
                            NdisInterfaceInternal/*(NDIS_INTERFACE_TYPE)0*/);

    /*
     * Initialize the power states for both the lower binding (PTDeviceState)
     * and our miniport edge to Powered On.
     */
    Assert(vboxNetFltWinGetPowerState(&pAdapt->MPState) == NdisDeviceStateD3);
    vboxNetFltWinSetPowerState(&pAdapt->MPState, NdisDeviceStateD0);
    Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initializing);
    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initialized);

    Status = NDIS_STATUS_SUCCESS;


//    *OpenErrorStatus = Status;

    LogFlow(("<== vboxNetFltWinMpDoInitialization: Adapt %p, Status %x\n", pAdapt, Status));

    return Status;
}

/**
 * This is the initialize handler which gets called as a result of
 * the BindAdapter handler calling NdisIMInitializeDeviceInstanceEx.
 * The context parameter which we pass there is the adapter structure
 * which we retrieve here.
 *
 * @param OpenErrorStatus            Not used by us.
 * @param SelectedMediumIndex        Place-holder for what media we are using
 * @param MediumArray                Array of ndis media passed down to us to pick from
 * @param MediumArraySize            Size of the array
 * @param MiniportAdapterHandle    The handle NDIS uses to refer to us
 * @param WrapperConfigurationContext    For use by NdisOpenConfiguration
 * @return NDIS_STATUS_SUCCESS unless something goes wrong
 * */
static NDIS_STATUS vboxNetFltWinMpInitialize(
    OUT PNDIS_STATUS             OpenErrorStatus,
    OUT PUINT                    SelectedMediumIndex,
    IN  PNDIS_MEDIUM             MediumArray,
    IN  UINT                     MediumArraySize,
    IN  NDIS_HANDLE              MiniportAdapterHandle,
    IN  NDIS_HANDLE              WrapperConfigurationContext
    )
{
    UINT            i;
    PADAPT          pAdapt;
    NDIS_STATUS     Status = NDIS_STATUS_FAILURE;
    NDIS_MEDIUM     Medium;

    UNREFERENCED_PARAMETER(WrapperConfigurationContext);

    Medium = NdisMedium802_3;

    if (Medium == NdisMediumWan)
    {
        Medium = NdisMedium802_3;
    }

    for (i = 0; i < MediumArraySize; i++)
    {
        if (MediumArray[i] == Medium)
        {
            *SelectedMediumIndex = i;
            break;
        }
    }

    if (i != MediumArraySize)
    {
        PDEVICE_OBJECT pPdo, pFdo;
#define KEY_PREFIX L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\"
        UCHAR Buf[512];
        PUCHAR pSuffix;
        ULONG  cbBuf;
        NDIS_STRING RtlStr;

        wcscpy((WCHAR*)Buf, KEY_PREFIX);
        pSuffix = Buf + (sizeof(KEY_PREFIX)-2);

        NdisMGetDeviceProperty(MiniportAdapterHandle,
                           &pPdo,
                           &pFdo,
                           NULL, //Next Device Object
                           NULL,
                           NULL);

        Status = IoGetDeviceProperty (pPdo,
                                      DevicePropertyDriverKeyName,
                                      sizeof(Buf) - (sizeof(KEY_PREFIX)-2),
                                      pSuffix,
                                      &cbBuf);
        if(Status == STATUS_SUCCESS)
        {
            OBJECT_ATTRIBUTES ObjAttr;
            HANDLE hDrvKey;
            RtlStr.Buffer=(WCHAR*)Buf;
            RtlStr.Length=(USHORT)cbBuf - 2 + sizeof(KEY_PREFIX) - 2;
            RtlStr.MaximumLength=sizeof(Buf);

            InitializeObjectAttributes(&ObjAttr, &RtlStr, OBJ_CASE_INSENSITIVE, NULL, NULL);

            Status = ZwOpenKey(&hDrvKey, KEY_READ, &ObjAttr);
            if(Status == STATUS_SUCCESS)
            {
                static UNICODE_STRING NetCfgInstanceIdValue = NDIS_STRING_CONST("NetCfgInstanceId");
//                UCHAR valBuf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + RTUUID_STR_LENGTH*2 + 10];
//                ULONG cLength = sizeof(valBuf);
#define NAME_PREFIX L"\\DEVICE\\"
                PKEY_VALUE_PARTIAL_INFORMATION pInfo = (PKEY_VALUE_PARTIAL_INFORMATION)Buf;
                Status = ZwQueryValueKey(hDrvKey,
                            &NetCfgInstanceIdValue,
                            KeyValuePartialInformation,
                            pInfo,
                            sizeof(Buf),
                            &cbBuf);
                if(Status == STATUS_SUCCESS)
                {
                    if(pInfo->Type == REG_SZ && pInfo->DataLength > 2)
                    {
                        WCHAR *pName;
                        Status = vboxNetFltWinMemAlloc(&pName, pInfo->DataLength + sizeof(NAME_PREFIX));
                        if(Status == STATUS_SUCCESS)
                        {
                            wcscpy(pName, NAME_PREFIX);
                            wcscpy(pName+(sizeof(NAME_PREFIX)-2)/2, (WCHAR*)pInfo->Data);
                            RtlStr.Buffer=pName;
                            RtlStr.Length = (USHORT)pInfo->DataLength - 2 + sizeof(NAME_PREFIX) - 2;
                            RtlStr.MaximumLength = (USHORT)pInfo->DataLength + sizeof(NAME_PREFIX);

                            Status = vboxNetFltWinPtInitBind(&pAdapt, MiniportAdapterHandle, &RtlStr, WrapperConfigurationContext);

                            if(Status == STATUS_SUCCESS)
                            {
                                Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
                                vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initialized);
#if 0
                                NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                                         NDIS_STATUS_MEDIA_CONNECT,
                                                         (PVOID)NULL,
                                                         0);
#endif
                            }
                            else
                            {
                                Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
                                vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
                            }

                            vboxNetFltWinMemFree(pName);

                        }
                    }
                    else
                    {
                        Status = NDIS_STATUS_FAILURE;
                    }
                }
            }
        }
    }
    else
    {
        Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

    if(Status != NDIS_STATUS_SUCCESS)
    {
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
    }

    /* TODO: */
    *OpenErrorStatus = Status;

    return Status;
}
#endif


/**
 * Send Packet Array handler. Either this or our SendPacket handler is called
 * based on which one is enabled in our Miniport Characteristics.
 *
 * @param MiniportAdapterContext     Pointer to our adapter
 * @param PacketArray                Set of packets to send
 * @param NumberOfPackets            Self-explanatory
 * @return none */
static VOID
vboxNetFltWinMpSendPackets(
    IN NDIS_HANDLE             fMiniportAdapterContext,
    IN PPNDIS_PACKET           pPacketArray,
    IN UINT                    cNumberOfPackets
    )
{
    PADAPT pAdapt = (PADAPT)fMiniportAdapterContext;
    NDIS_STATUS         fStatus;
    UINT                i;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    bool bNetFltActive;

    Assert(cNumberOfPackets);

    if(vboxNetFltWinIncReferenceAdaptNetFlt(pNetFlt, pAdapt, cNumberOfPackets, &bNetFltActive))
    {
        uint32_t cAdaptRefs = cNumberOfPackets;
        uint32_t cNetFltRefs;
        uint32_t cPassThruRefs;
        if(bNetFltActive)
        {
        	cNetFltRefs = cNumberOfPackets;
        	cPassThruRefs = 0;
        }
        else
        {
        	cPassThruRefs = cNumberOfPackets;
        	cNetFltRefs = 0;
        }

        for (i = 0; i < cNumberOfPackets; i++)
        {
            PNDIS_PACKET    pPacket;

            pPacket = pPacketArray[i];

            if(!cNetFltRefs
                    || (fStatus = vboxNetFltWinQuEnqueuePacket(pNetFlt, pPacket, PACKET_SRC_HOST)) != NDIS_STATUS_SUCCESS)
            {
#ifndef VBOXNETADP
                fStatus = vboxNetFltWinSendPassThru(pAdapt, pPacket);
#else
                if(!cNetFltRefs)
                {
# ifdef VBOXNETADP_REPORT_DISCONNECTED
                    fStatus = NDIS_STATUS_MEDIA_DISCONNECT;
                    STATISTIC_INCREASE(pAdapt->cTxError);
# else
                    fStatus = NDIS_STATUS_SUCCESS;
# endif
                }
#endif

                if (fStatus != NDIS_STATUS_PENDING)
                {
                    NdisMSendComplete(pAdapt->hMiniportHandle,
                                      pPacket,
                                      fStatus);
                }
                else
                {
                    cAdaptRefs--;
                }
            }
            else
            {
                cAdaptRefs--;
                cNetFltRefs--;
            }
        }

        if(cNetFltRefs)
        {
            vboxNetFltWinDecReferenceNetFlt(pNetFlt, cNetFltRefs);
        }
        else if(cPassThruRefs)
        {
            vboxNetFltWinDecReferenceModePassThru(pNetFlt, cPassThruRefs);
        }
        if(cAdaptRefs)
        {
            vboxNetFltWinDecReferenceAdapt(pAdapt, cAdaptRefs);
        }
    }
    else
    {
        NDIS_HANDLE h = pAdapt->hMiniportHandle;
        Assert(0);
        if(h)
        {
            for (i = 0; i < cNumberOfPackets; i++)
            {
                PNDIS_PACKET    pPacket;
                pPacket = pPacketArray[i];
                NdisMSendComplete(h,
                                    pPacket,
                                    NDIS_STATUS_FAILURE);
            }
        }
    }
}
#ifndef VBOXNETADP
/**
 * Entry point called by NDIS to query for the value of the specified OID.
 * Typical processing is to forward the query down to the underlying miniport.
 *
 * The following OIDs are filtered here:
 * OID_PNP_QUERY_POWER - return success right here
 * OID_GEN_SUPPORTED_GUIDS - do not forward, otherwise we will show up
 * multiple instances of private GUIDs supported by the underlying miniport.
 * OID_PNP_CAPABILITIES - we do send this down to the lower miniport, but
 * the values returned are postprocessed before we complete this request;
 * see vboxNetFltWinPtRequestComplete.
 *
 * NOTE on OID_TCP_TASK_OFFLOAD - if this IM driver modifies the contents
 * of data it passes through such that a lower miniport may not be able
 * to perform TCP task offload, then it should not forward this OID down,
 * but fail it here with the status NDIS_STATUS_NOT_SUPPORTED. This is to
 * avoid performing incorrect transformations on data.
 *
 * If our miniport edge (upper edge) is at a low-power state, fail the request.
 * If our protocol edge (lower edge) has been notified of a low-power state,
 * we pend this request until the miniport below has been set to D0. Since
 * requests to miniports are serialized always, at most a single request will
 * be pended.
 *
 * @param MiniportAdapterContext    Pointer to the adapter structure
 * @param Oid                       Oid for this query
 * @param InformationBuffer         Buffer for information
 * @param InformationBufferLength   Size of this buffer
 * @param BytesWritten              Specifies how much info is written
 * @param BytesNeeded               In case the buffer is smaller than what we need, tell them how much is needed
 * @return    Return code from the NdisRequest below.
 * */
static NDIS_STATUS
vboxNetFltWinMpQueryInformation(
    IN NDIS_HANDLE                MiniportAdapterContext,
    IN NDIS_OID                   Oid,
    IN PVOID                      InformationBuffer,
    IN ULONG                      InformationBufferLength,
    OUT PULONG                    BytesWritten,
    OUT PULONG                    BytesNeeded
    )
{
    PADAPT        pAdapt = (PADAPT)MiniportAdapterContext;
    NDIS_STATUS   Status = NDIS_STATUS_FAILURE;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    do
    {
        if (Oid == OID_PNP_QUERY_POWER)
        {
            /*
             *  Do not forward this.
             */
            Status = NDIS_STATUS_SUCCESS;
            break;
        }

        if (Oid == OID_GEN_SUPPORTED_GUIDS)
        {
            /*
             *  Do not forward this, otherwise we will end up with multiple
             *  instances of private GUIDs that the underlying miniport
             *  supports.
             */
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        if (Oid == OID_TCP_TASK_OFFLOAD)
        {
            /* we want to receive packets with checksums calculated
             * since we are passing them to IntNet
             */
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }

        /*
         * If the miniport below is unbinding, just fail any request
         */
        if (vboxNetFltWinGetOpState(&pAdapt->PTState) > kVBoxNetDevOpState_Initialized) /* protocol unbind in progress */
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        /*
         * All other queries are failed, if the miniport is not at D0,
         */
        if (vboxNetFltWinGetPowerState(&pAdapt->MPState) > NdisDeviceStateD0)
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        pAdapt->Request.RequestType = NdisRequestQueryInformation;
        pAdapt->Request.DATA.QUERY_INFORMATION.Oid = Oid;
        pAdapt->Request.DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
        pAdapt->Request.DATA.QUERY_INFORMATION.InformationBufferLength = InformationBufferLength;
        pAdapt->BytesNeeded = BytesNeeded;
        pAdapt->BytesReadOrWritten = BytesWritten;

        /*
         * If the miniport below is binding, fail the request
         */
        RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

        if (vboxNetFltWinGetOpState(&pAdapt->PTState) > kVBoxNetDevOpState_Initialized)
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        /*
         * If the Protocol device state is OFF, mark this request as being
         * pended. We queue this until the device state is back to D0.
         */
        if ((vboxNetFltWinGetPowerState(&pAdapt->PTState) > NdisDeviceStateD0)
                && (pAdapt->bStandingBy == FALSE))
        {
            pAdapt->bQueuedRequest = TRUE;
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_PENDING;
            break;
        }
        /*
         * This is in the process of powering down the system, always fail the request
         */
        if (pAdapt->bStandingBy == TRUE)
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        pAdapt->bOutstandingRequests = TRUE;

        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
        if(Oid == OID_GEN_CURRENT_PACKET_FILTER && VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
        {
            bool fNetFltActive;
            const bool fAdaptActive = vboxNetFltWinReferenceAdaptNetFlt(pNetFlt, pAdapt, &fNetFltActive);

            Assert(InformationBuffer);
            Assert(!pAdapt->fProcessingPacketFilter);

            if(fNetFltActive)
            {
                /* netflt is active, simply return the cached value */
                *((PULONG)InformationBuffer) = pAdapt->fUpperProtocolSetFilter;

                Status = NDIS_STATUS_SUCCESS;
                vboxNetFltWinDereferenceNetFlt(pNetFlt);
                vboxNetFltWinDereferenceAdapt(pAdapt);

                RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);
                pAdapt->bOutstandingRequests = FALSE;
                RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
                break;
            }
            else if(fAdaptActive)
            {
            	pAdapt->fProcessingPacketFilter = VBOXNETFLT_PFP_PASSTHRU;
                /* we're cleaning it in RequestComplete */
            }
        }

        /*
         * default case, most requests will be passed to the miniport below
         */
        NdisRequest(&Status,
                    pAdapt->hBindingHandle,
                    &pAdapt->Request);


        if (Status != NDIS_STATUS_PENDING)
        {
            vboxNetFltWinPtRequestComplete(pAdapt, &pAdapt->Request, Status);
            Status = NDIS_STATUS_PENDING;
        }

    } while (FALSE);

    return(Status);

}
/**
 * Postprocess a request for OID_PNP_CAPABILITIES that was forwarded
 * down to the underlying miniport, and has been completed by it.
 *
 * @param pAdapt - Pointer to the adapter structure
 * @param pStatus - Place to return final status
 * @return  None. */
DECLHIDDEN(VOID)
vboxNetFltWinMpQueryPNPCapabilities(
    IN OUT PADAPT            pAdapt,
    OUT PNDIS_STATUS         pStatus
    )
{
    PNDIS_PNP_CAPABILITIES           pPNPCapabilities;
    PNDIS_PM_WAKE_UP_CAPABILITIES    pPMstruct;

    if (pAdapt->Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(NDIS_PNP_CAPABILITIES))
    {
        pPNPCapabilities = (PNDIS_PNP_CAPABILITIES)(pAdapt->Request.DATA.QUERY_INFORMATION.InformationBuffer);

        /*
         * The following fields must be overwritten by an IM driver.
         */
        pPMstruct= & pPNPCapabilities->WakeUpCapabilities;
        pPMstruct->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
        pPMstruct->MinPatternWakeUp = NdisDeviceStateUnspecified;
        pPMstruct->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
        *pAdapt->BytesReadOrWritten = sizeof(NDIS_PNP_CAPABILITIES);
        *pAdapt->BytesNeeded = 0;


        *pStatus = NDIS_STATUS_SUCCESS;
    }
    else
    {
        *pAdapt->BytesNeeded= sizeof(NDIS_PNP_CAPABILITIES);
        *pStatus = NDIS_STATUS_RESOURCES;
    }
}

#endif /* ifndef VBOXNETADP*/

/**
 * This routine does all the procssing for a request with a SetPower Oid
 * The miniport shoud accept  the Set Power and transition to the new state
 *
 * The Set Power should not be passed to the miniport below
 *
 * If the IM miniport is going into a low power state, then there is no guarantee if it will ever
 * be asked go back to D0, before getting halted. No requests should be pended or queued.
 *
 * @param pNdisStatus           - Status of the operation
 * @param pAdapt                - The Adapter structure
 * @param InformationBuffer     - The New DeviceState
 * @param InformationBufferLength
 * @param BytesRead             - No of bytes read
 * @param BytesNeeded           -  No of bytes needed
 * @return    Status  - NDIS_STATUS_SUCCESS if all the wait events succeed. */
static VOID
vboxNetFltWinMpProcessSetPowerOid(
    IN OUT PNDIS_STATUS          pNdisStatus,
    IN PADAPT                    pAdapt,
    IN PVOID                     InformationBuffer,
    IN ULONG                     InformationBufferLength,
    OUT PULONG                   BytesRead,
    OUT PULONG                   BytesNeeded
    )
{


    NDIS_DEVICE_POWER_STATE NewDeviceState;

    LogFlow(("==>vboxNetFltWinMpProcessSetPowerOid: Adapt %p\n", pAdapt));

    Assert (InformationBuffer != NULL);

    *pNdisStatus = NDIS_STATUS_FAILURE;

    do
    {
        /*
         * Check for invalid length
         */
        if (InformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE))
        {
            *pNdisStatus = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        NewDeviceState = (*(PNDIS_DEVICE_POWER_STATE)InformationBuffer);

        /*
         * Check for invalid device state
         */
        if ((vboxNetFltWinGetPowerState(&pAdapt->MPState) > NdisDeviceStateD0) && (NewDeviceState != NdisDeviceStateD0))
        {
            /*
             * If the miniport is in a non-D0 state, the miniport can only receive a Set Power to D0
             */
            Assert (!(vboxNetFltWinGetPowerState(&pAdapt->MPState) > NdisDeviceStateD0) && (NewDeviceState != NdisDeviceStateD0));

            *pNdisStatus = NDIS_STATUS_FAILURE;
            break;
        }

#ifndef VBOXNETADP
        /*
         * Is the miniport transitioning from an On (D0) state to an Low Power State (>D0)
         * If so, then set the bStandingBy Flag - (Block all incoming requests)
         */
        if (vboxNetFltWinGetPowerState(&pAdapt->MPState) == NdisDeviceStateD0 && NewDeviceState > NdisDeviceStateD0)
        {
            pAdapt->bStandingBy = TRUE;
        }

        /*
         * If the miniport is transitioning from a low power state to ON (D0), then clear the bStandingBy flag
         * All incoming requests will be pended until the physical miniport turns ON.
         */
        if (vboxNetFltWinGetPowerState(&pAdapt->MPState) > NdisDeviceStateD0 &&  NewDeviceState == NdisDeviceStateD0)
        {
            pAdapt->bStandingBy = FALSE;
        }
#endif
        /*
         * Now update the state in the pAdapt structure;
         */
        vboxNetFltWinSetPowerState(&pAdapt->MPState, NewDeviceState);
#ifndef VBOXNETADP
        if(NewDeviceState != NdisDeviceStateD0)
        {
            vboxNetFltWinPtFlushReceiveQueue(pAdapt,
                    true ); /* just return */
        }
#endif
        *pNdisStatus = NDIS_STATUS_SUCCESS;


    } while (FALSE);

    if (*pNdisStatus == NDIS_STATUS_SUCCESS)
    {
#ifndef VBOXNETADP
        /*
         * The miniport resume from low power state
         */
        if (pAdapt->bStandingBy == FALSE)
        {
            /*
             * If we need to indicate the media connect state
             */
            if (pAdapt->LastIndicatedStatus != pAdapt->LatestUnIndicateStatus)
            {
               NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                        pAdapt->LatestUnIndicateStatus,
                                        (PVOID)NULL,
                                        0);
               NdisMIndicateStatusComplete(pAdapt->hMiniportHandle);
               pAdapt->LastIndicatedStatus = pAdapt->LatestUnIndicateStatus;
            }
        }
        else
        {
            /*
             * Initialize LatestUnIndicatedStatus
             */
            pAdapt->LatestUnIndicateStatus = pAdapt->LastIndicatedStatus;
        }
#endif
        *BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
        *BytesNeeded = 0;
    }
    else
    {
        *BytesRead = 0;
        *BytesNeeded = sizeof (NDIS_DEVICE_POWER_STATE);
    }

    LogFlow(("<==vboxNetFltWinMpProcessSetPowerOid: Adapt %p\n", pAdapt));
}
#ifndef VBOXNETADP
/**
 * Miniport SetInfo handler.
 *
 * In the case of OID_PNP_SET_POWER, record the power state and return the OID.
 * Do not pass below
 * If the device is suspended, do not block the SET_POWER_OID
 * as it is used to reactivate the NetFlt miniport
 *
 * PM- If the MP is not ON (DeviceState > D0) return immediately  (except for 'query power' and 'set power')
 *       If MP is ON, but the PT is not at D0, then queue the queue the request for later processing
 *
 * Requests to miniports are always serialized
 *
 * @param MiniportAdapterContext    Pointer to the adapter structure
 * @param Oid                       Oid for this query
 * @param InformationBuffer         Buffer for information
 * @param InformationBufferLength   Size of this buffer
 * @param BytesRead                 Specifies how much info is read
 * @param BytesNeeded               In case the buffer is smaller than what we need, tell them how much is needed
 * @return Return code from the NdisRequest below. */
static NDIS_STATUS
vboxNetFltWinMpSetInformation(
    IN NDIS_HANDLE             MiniportAdapterContext,
    IN NDIS_OID                Oid,
    IN PVOID                   InformationBuffer,
    IN ULONG                   InformationBufferLength,
    OUT PULONG                 BytesRead,
    OUT PULONG                 BytesNeeded
    )
{
    PADAPT        pAdapt = (PADAPT)MiniportAdapterContext;
    NDIS_STATUS   Status;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    Status = NDIS_STATUS_FAILURE;

    do
    {
        /*
         * The Set Power should not be sent to the miniport below the NetFlt, but is handled internally
         */
        if (Oid == OID_PNP_SET_POWER)
        {
            vboxNetFltWinMpProcessSetPowerOid(&Status,
                                 pAdapt,
                                 InformationBuffer,
                                 InformationBufferLength,
                                 BytesRead,
                                 BytesNeeded);
            break;

        }

        /*
         * If the miniport below is unbinding, fail the request
         */
        if (vboxNetFltWinGetOpState(&pAdapt->PTState) > kVBoxNetDevOpState_Initialized)
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        /*
         * All other Set Information requests are failed, if the miniport is
         * not at D0 or is transitioning to a device state greater than D0.
         */
        if (vboxNetFltWinGetPowerState(&pAdapt->MPState) > NdisDeviceStateD0)
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        /* Set up the Request and return the result */
        pAdapt->Request.RequestType = NdisRequestSetInformation;
        pAdapt->Request.DATA.SET_INFORMATION.Oid = Oid;
        pAdapt->Request.DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
        pAdapt->Request.DATA.SET_INFORMATION.InformationBufferLength = InformationBufferLength;
        pAdapt->BytesNeeded = BytesNeeded;
        pAdapt->BytesReadOrWritten = BytesRead;

        /*
         * If the miniport below is unbinding, fail the request
         */
        RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);
        if (vboxNetFltWinGetOpState(&pAdapt->PTState) > kVBoxNetDevOpState_Initialized)
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        /*
         * If the device below is at a low power state, we cannot send it the
         * request now, and must pend it.
         */
        if ((vboxNetFltWinGetPowerState(&pAdapt->PTState) > NdisDeviceStateD0)
                && (pAdapt->bStandingBy == FALSE))
        {
            pAdapt->bQueuedRequest = TRUE;
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_PENDING;
            break;
        }
        /*
         * This is in the process of powering down the system, always fail the request
         */
        if (pAdapt->bStandingBy == TRUE)
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        pAdapt->bOutstandingRequests = TRUE;

        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

        if(Oid == OID_GEN_CURRENT_PACKET_FILTER && VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
        {
            /* need to disable cleaning promiscuous here ?? */
        	bool fNetFltActive;
            const bool fAdaptActive = vboxNetFltWinReferenceAdaptNetFlt(pNetFlt, pAdapt, &fNetFltActive);

            Assert(InformationBuffer);
            Assert(!pAdapt->fProcessingPacketFilter);

            if(fNetFltActive)
            {
                Assert(fAdaptActive);

                /* netflt is active, update the cached value */
                /* TODO: in case we are are not in promiscuous now, we are issuing a request.
                 * what should we do in case of a failure?
                 * i.e. should we update the fUpperProtocolSetFilter in completion routine in this case? etc. */
                pAdapt->fUpperProtocolSetFilter = *((PULONG)InformationBuffer);
                pAdapt->bUpperProtSetFilterInitialized = true;

                if(!(pAdapt->fOurSetFilter & NDIS_PACKET_TYPE_PROMISCUOUS))
                {
                    pAdapt->fSetFilterBuffer = NDIS_PACKET_TYPE_PROMISCUOUS;
                    pAdapt->Request.DATA.SET_INFORMATION.InformationBuffer = &pAdapt->fSetFilterBuffer;
                    pAdapt->Request.DATA.SET_INFORMATION.InformationBufferLength = sizeof(pAdapt->fSetFilterBuffer);
                    pAdapt->fProcessingPacketFilter = VBOXNETFLT_PFP_NETFLT;
                    /* we'll do dereferencing in request complete */
                }
                else
                {
                    Status = NDIS_STATUS_SUCCESS;
                    vboxNetFltWinDereferenceNetFlt(pNetFlt);
                    vboxNetFltWinDereferenceAdapt(pAdapt);

                    RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);
                    pAdapt->bOutstandingRequests = FALSE;
                    RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
                    break;
                }
            }
            else if(fAdaptActive)
            {
                pAdapt->fProcessingPacketFilter = VBOXNETFLT_PFP_PASSTHRU;
                /* dereference on completion */
            }
        }

        /*
         * Forward the request to the device below.
         */
        NdisRequest(&Status,
                    pAdapt->hBindingHandle,
                    &pAdapt->Request);

        if (Status != NDIS_STATUS_PENDING)
        {
            vboxNetFltWinPtRequestComplete(pAdapt, &pAdapt->Request, Status);
        }

    } while (FALSE);

    return(Status);
}
#else
static NDIS_OID g_vboxNetFltWinMpSupportedOids[] =
{
        OID_GEN_SUPPORTED_LIST,
        OID_GEN_HARDWARE_STATUS,
        OID_GEN_MEDIA_SUPPORTED,
        OID_GEN_MEDIA_IN_USE,
        OID_GEN_MAXIMUM_LOOKAHEAD,
        OID_GEN_MAXIMUM_FRAME_SIZE,
        OID_GEN_LINK_SPEED,
        OID_GEN_TRANSMIT_BUFFER_SPACE,
        OID_GEN_RECEIVE_BUFFER_SPACE,
        OID_GEN_TRANSMIT_BLOCK_SIZE,
        OID_GEN_RECEIVE_BLOCK_SIZE,
        OID_GEN_VENDOR_ID,
        OID_GEN_VENDOR_DESCRIPTION,
        OID_GEN_VENDOR_DRIVER_VERSION,
        OID_GEN_CURRENT_PACKET_FILTER,
        OID_GEN_CURRENT_LOOKAHEAD,
        OID_GEN_DRIVER_VERSION,
        OID_GEN_MAXIMUM_TOTAL_SIZE,
        OID_GEN_PROTOCOL_OPTIONS,
        OID_GEN_MAC_OPTIONS,
        OID_GEN_MEDIA_CONNECT_STATUS,
        OID_GEN_MAXIMUM_SEND_PACKETS,
        OID_GEN_XMIT_OK,
        OID_GEN_RCV_OK,
        OID_GEN_XMIT_ERROR,
        OID_GEN_RCV_ERROR,
        OID_GEN_RCV_NO_BUFFER,
        OID_GEN_RCV_CRC_ERROR,
        OID_GEN_TRANSMIT_QUEUE_LENGTH,
        OID_802_3_PERMANENT_ADDRESS,
        OID_802_3_CURRENT_ADDRESS,
        OID_802_3_MULTICAST_LIST,
        OID_802_3_MAC_OPTIONS,
        OID_802_3_MAXIMUM_LIST_SIZE,
        OID_802_3_RCV_ERROR_ALIGNMENT,
        OID_802_3_XMIT_ONE_COLLISION,
        OID_802_3_XMIT_MORE_COLLISIONS,
        OID_802_3_XMIT_DEFERRED,
        OID_802_3_XMIT_MAX_COLLISIONS,
        OID_802_3_RCV_OVERRUN,
        OID_802_3_XMIT_UNDERRUN,
        OID_802_3_XMIT_HEARTBEAT_FAILURE,
        OID_802_3_XMIT_TIMES_CRS_LOST,
        OID_802_3_XMIT_LATE_COLLISIONS
#ifndef INTERFACE_WITH_NDISPROT
        ,
        OID_PNP_CAPABILITIES,
        OID_PNP_SET_POWER,
        OID_PNP_QUERY_POWER
# if 0
        ,
        OID_PNP_ADD_WAKE_UP_PATTERN,
        OID_PNP_REMOVE_WAKE_UP_PATTERN,
        OID_PNP_ENABLE_WAKE_UP
# endif
#endif
};

static NDIS_STATUS
vboxNetFltWinMpQueryInformation(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN ULONG        InformationBufferLength,
    OUT PULONG      BytesWritten,
    OUT PULONG      BytesNeeded)
/*++

Routine Description:

    Entry point called by NDIS to query for the value of the specified OID.
    MiniportQueryInformation runs at IRQL = DISPATCH_LEVEL.

Arguments:

    MiniportAdapterContext      Pointer to the adapter structure
    Oid                         Oid for this query
    InformationBuffer           Buffer for information
    InformationBufferLength     Size of this buffer
    BytesWritten                Specifies how much info is written
    BytesNeeded                 In case the buffer is smaller than
                                what we need, tell them how much is needed


Return Value:

    Return code from the NdisRequest below.

Notes: Read "Minimizing Miniport Driver Initialization Time" in the DDK
    for more info on how to handle certain OIDs that affect the init of
    a miniport.

--*/
{
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    PADAPT             pAdapt = (PADAPT)MiniportAdapterContext;
    NDIS_HARDWARE_STATUS    HardwareStatus = NdisHardwareStatusReady;
    NDIS_MEDIUM             Medium = NdisMedium802_3;
    UCHAR                   VendorDesc[] = VBOXNETADP_VENDOR_DESC;
    ULONG                   ulInfo = 0;
    USHORT                  usInfo = 0;
    ULONG64                 ulInfo64 = 0;
    PVOID                   pInfo = (PVOID) &ulInfo;
    ULONG                   ulInfoLen = sizeof(ulInfo);
    NDIS_PNP_CAPABILITIES   PMCaps;

    LogFlow(("==> vboxNetFltWinMpQueryInformation %s\n", vboxNetFltWinMpDbgGetOidName(Oid)));

    // Initialize the result
    *BytesWritten = 0;
    *BytesNeeded = 0;

    switch(Oid)
    {
        case OID_GEN_SUPPORTED_LIST:
            //
            // The OID_GEN_SUPPORTED_LIST OID specifies an array of OIDs
            // for objects that the underlying driver or its NIC supports.
            // Objects include general, media-specific, and implementation-
            // specific objects. NDIS forwards a subset of the returned
            // list to protocols that make this query. That is, NDIS filters
            // any supported statistics OIDs out of the list because
            // protocols never make statistics queries.
            //
            pInfo = (PVOID) g_vboxNetFltWinMpSupportedOids;
            ulInfoLen = sizeof(g_vboxNetFltWinMpSupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:
            //
            // Specify the current hardware status of the underlying NIC as
            // one of the following NDIS_HARDWARE_STATUS-type values.
            //
            pInfo = (PVOID) &HardwareStatus;
            ulInfoLen = sizeof(NDIS_HARDWARE_STATUS);
            break;

        case OID_GEN_MEDIA_SUPPORTED:
            //
            // Specify the media types that the NIC can support but not
            // necessarily the media types that the NIC currently uses.
            // fallthrough:
        case OID_GEN_MEDIA_IN_USE:
            //
            // Specifiy a complete list of the media types that the NIC
            // currently uses.
            //
            pInfo = (PVOID) &Medium;
            ulInfoLen = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_MAXIMUM_LOOKAHEAD:
            //
            // If the miniport driver indicates received data by calling
            // NdisXxxIndicateReceive, it should respond to OID_GEN_MAXIMUM_LOOKAHEAD
            // with the maximum number of bytes the NIC can provide as
            // lookahead data. If that value is different from the size of the
            // lookahead buffer supported by bound protocols, NDIS will call
            // MiniportSetInformation to set the size of the lookahead buffer
            // provided by the miniport driver to the minimum of the miniport
            // driver and protocol(s) values. If the driver always indicates
            // up full packets with NdisMIndicateReceivePacket, it should
            // set this value to the maximum total packet size, which
            // excludes the header.
            // Upper-layer drivers examine lookahead data to determine whether
            // a packet that is associated with the lookahead data is intended
            // for one or more of their clients. If the underlying driver
            // supports multipacket receive indications, bound protocols are
            // given full net packets on every indication. Consequently,
            // this value is identical to that returned for
            // OID_GEN_RECEIVE_BLOCK_SIZE.
            //
            ulInfo = VBOXNETADP_MAX_LOOKAHEAD_SIZE;
            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:
            //
            // Specifiy the maximum network packet size, in bytes, that the
            // NIC supports excluding the header. A NIC driver that emulates
            // another medium type for binding to a transport must ensure that
            // the maximum frame size for a protocol-supplied net packet does
            // not exceed the size limitations for the true network medium.
            //
            ulInfo = VBOXNETADP_MAX_PACKET_SIZE - VBOXNETADP_HEADER_SIZE;
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            //
            // Specify the maximum total packet length, in bytes, the NIC
            // supports including the header. A protocol driver might use
            // this returned length as a gauge to determine the maximum
            // size packet that a NIC driver could forward to the
            // protocol driver. The miniport driver must never indicate
            // up to the bound protocol driver packets received over the
            // network that are longer than the packet size specified by
            // OID_GEN_MAXIMUM_TOTAL_SIZE.
            //
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
            //
            // The OID_GEN_TRANSMIT_BLOCK_SIZE OID specifies the minimum
            // number of bytes that a single net packet occupies in the
            // transmit buffer space of the NIC. For example, a NIC that
            // has a transmit space divided into 256-byte pieces would have
            // a transmit block size of 256 bytes. To calculate the total
            // transmit buffer space on such a NIC, its driver multiplies
            // the number of transmit buffers on the NIC by its transmit
            // block size. In our case, the transmit block size is
            // identical to its maximum packet size.

        case OID_GEN_RECEIVE_BLOCK_SIZE:
            //
            // The OID_GEN_RECEIVE_BLOCK_SIZE OID specifies the amount of
            // storage, in bytes, that a single packet occupies in the receive
            // buffer space of the NIC.
            //
            ulInfo = (ULONG) VBOXNETADP_MAX_PACKET_SIZE;
            break;

        case OID_GEN_MAC_OPTIONS:
            //
            // Specify a bitmask that defines optional properties of the NIC.
            // This miniport indicates receive with NdisMIndicateReceivePacket
            // function. It has no MiniportTransferData function. Such a driver
            // should set this NDIS_MAC_OPTION_TRANSFERS_NOT_PEND flag.
            //
            // NDIS_MAC_OPTION_NO_LOOPBACK tells NDIS that NIC has no internal
            // loopback support so NDIS will manage loopbacks on behalf of
            // this driver.
            //
            // NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA tells the protocol that
            // our receive buffer is not on a device-specific card. If
            // NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA is not set, multi-buffer
            // indications are copied to a single flat buffer.
            //
            ulInfo = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                NDIS_MAC_OPTION_NO_LOOPBACK;
            break;

        case OID_GEN_LINK_SPEED:
            //
            // Specify the maximum speed of the NIC in kbps.
            // The unit of measurement is 100 bps
            //
            ulInfo = VBOXNETADP_LINK_SPEED;
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            //
            // Specify the amount of memory, in bytes, on the NIC that
            // is available for buffering transmit data. A protocol can
            // use this OID as a guide for sizing the amount of transmit
            // data per send.
            //
            ulInfo = VBOXNETADP_MAX_PACKET_SIZE * PACKET_INFO_POOL_SIZE;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            //
            // Specify the amount of memory on the NIC that is available
            // for buffering receive data. A protocol driver can use this
            // OID as a guide for advertising its receive window after it
            // establishes sessions with remote nodes.
            //

            ulInfo = VBOXNETADP_MAX_PACKET_SIZE * PACKET_INFO_POOL_SIZE;
            break;

        case OID_GEN_VENDOR_ID:
            //
            // Specify a three-byte IEEE-registered vendor code, followed
            // by a single byte that the vendor assigns to identify a
            // particular NIC. The IEEE code uniquely identifies the vendor
            // and is the same as the three bytes appearing at the beginning
            // of the NIC hardware address. Vendors without an IEEE-registered
            // code should use the value 0xFFFFFF.
            //
            ulInfo = VBOXNETADP_VENDOR_ID;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            //
            // Specify a zero-terminated string describing the NIC vendor.
            //
            pInfo = VendorDesc;
            ulInfoLen = sizeof(VendorDesc);
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            //
            // Specify the vendor-assigned version number of the NIC driver.
            // The low-order half of the return value specifies the minor
            // version; the high-order half specifies the major version.
            //
            ulInfo = VBOXNETADP_VENDOR_DRIVER_VERSION;
            break;

        case OID_GEN_DRIVER_VERSION:
            //
            // Specify the NDIS version in use by the NIC driver. The high
            // byte is the major version number; the low byte is the minor
            // version number.
            //
            usInfo = (USHORT) (VBOXNETFLT_MAJOR_NDIS_VERSION<<8) + VBOXNETFLT_MINOR_NDIS_VERSION;
            pInfo = (PVOID) &usInfo;
            ulInfoLen = sizeof(USHORT);
            break;

        case OID_GEN_MAXIMUM_SEND_PACKETS:
            //
            // If a miniport driver registers a MiniportSendPackets function,
            // MiniportQueryInformation will be called with the
            // OID_GEN_MAXIMUM_SEND_PACKETS request. The miniport driver must
            // respond with the maximum number of packets it is prepared to
            // handle on a single send request. The miniport driver should
            // pick a maximum that minimizes the number of packets that it
            // has to queue internally because it has no resources
            // (its device is full). A miniport driver for a bus-master DMA
            // NIC should attempt to pick a value that keeps its NIC filled
            // under anticipated loads.
            //
            ulInfo = PACKET_INFO_POOL_SIZE;
            break;

        case OID_GEN_MEDIA_CONNECT_STATUS:
            //
            // Return the connection status of the NIC on the network as one
            // of the following system-defined values: NdisMediaStateConnected
            // or NdisMediaStateDisconnected.
            //
#ifdef VBOXNETADP_REPORT_DISCONNECTED
            {
                PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
                bool bNetFltActive;
                bool bActive = vboxNetFltWinReferenceAdaptNetFltFromAdapt(pNetFlt, pAdapt, bNetFltActive);
                if(bActive && bNetFltActive)
                {
                    ulInfo = NdisMediaStateConnected;
                }
                else
                {
                    ulInfo = NdisMediaStateDisconnected;
                }

                if(bActive)
                {
                    vboxNetFltWinDereferenceAdapt(pAdapt);
                }
                if(bNetFltActive)
                {
                    vboxNetFltWinDereferenceNetFlt(pNetFlt);
                }
                else
                {
                    vboxNetFltWinDereferenceModePassThru(pNetFlt);
                }
            }
#else
            ulInfo = NdisMediaStateConnected;
#endif
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            //
            // Specifiy the types of net packets such as directed, broadcast
            // multicast, for which a protocol receives indications from a
            // NIC driver. After NIC is initialized, a protocol driver
            // can send a set OID_GEN_CURRENT_PACKET_FILTER to a non-zero value,
            // thereby enabling the miniport driver to indicate receive packets
            // to that protocol.
            //
            ulInfo = (
                      NDIS_PACKET_TYPE_BROADCAST
                    | NDIS_PACKET_TYPE_DIRECTED
                    | NDIS_PACKET_TYPE_ALL_FUNCTIONAL
                    | NDIS_PACKET_TYPE_ALL_LOCAL
                    | NDIS_PACKET_TYPE_GROUP
                    | NDIS_PACKET_TYPE_MULTICAST
                    );
            break;

        case OID_PNP_CAPABILITIES:
            //
            // Return the wake-up capabilities of its NIC. If you return
            // NDIS_STATUS_NOT_SUPPORTED, NDIS considers the miniport driver
            // to be not Power management aware and doesn't send any power
            // or wake-up related queries such as
            // OID_PNP_SET_POWER, OID_PNP_QUERY_POWER,
            // OID_PNP_ADD_WAKE_UP_PATTERN, OID_PNP_REMOVE_WAKE_UP_PATTERN,
            // OID_PNP_ENABLE_WAKE_UP. Here, we are expecting the driver below
            // us to do the right thing.
            //
            RtlZeroMemory (&PMCaps, sizeof(NDIS_PNP_CAPABILITIES));
            ulInfoLen = sizeof (NDIS_PNP_CAPABILITIES);
            pInfo = (PVOID) &PMCaps;
            PMCaps.WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
            PMCaps.WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateUnspecified;
            break;

        case OID_PNP_QUERY_POWER:
            Status = NDIS_STATUS_SUCCESS;
            break;

            //
            // Following 4 OIDs are for querying Ethernet Operational
            // Characteristics.
            //
        case OID_802_3_PERMANENT_ADDRESS:
            //
            // Return the MAC address of the NIC burnt in the hardware.
            //
            {
                PVBOXNETFLTINS pNetFlt = (PADAPT_2_PVBOXNETFLTINS(pAdapt));
                pInfo = &pNetFlt->u.s.Mac;
                ulInfoLen = VBOXNETADP_ETH_ADDRESS_LENGTH;
            }
            break;

        case OID_802_3_CURRENT_ADDRESS:
            //
            // Return the MAC address the NIC is currently programmed to
            // use. Note that this address could be different from the
            // permananent address as the user can override using
            // registry. Read NdisReadNetworkAddress doc for more info.
            //
            {
                PVBOXNETFLTINS pNetFlt = (PADAPT_2_PVBOXNETFLTINS(pAdapt));
                pInfo = &pNetFlt->u.s.Mac;
                ulInfoLen = VBOXNETADP_ETH_ADDRESS_LENGTH;
            }
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:
            //
            // The maximum number of multicast addresses the NIC driver
            // can manage. This list is global for all protocols bound
            // to (or above) the NIC. Consequently, a protocol can receive
            // NDIS_STATUS_MULTICAST_FULL from the NIC driver when
            // attempting to set the multicast address list, even if
            // the number of elements in the given list is less than
            // the number originally returned for this query.
            //
            ulInfo = VBOXNETADP_MAX_MCAST_LIST;
            break;

        case OID_802_3_MAC_OPTIONS:
            //
            // A protocol can use this OID to determine features supported
            // by the underlying driver such as NDIS_802_3_MAC_OPTION_PRIORITY.
            // Return zero indicating that it supports no options.
            //
            ulInfo = 0;
            break;

            //
            // Following list  consists of both general and Ethernet
            // specific statistical OIDs.
            //

        case OID_GEN_XMIT_OK:
            ulInfo64 = pAdapt->cTxSuccess;
            pInfo = &ulInfo64;
            if (InformationBufferLength >= sizeof(ULONG64) ||
                InformationBufferLength == 0)
            {
                ulInfoLen = sizeof(ULONG64);
            }
            else
            {
                ulInfoLen = sizeof(ULONG);
            }
            // We should always report that 8 bytes are required to keep ndistest happy
            *BytesNeeded =  sizeof(ULONG64);
            break;

        case OID_GEN_RCV_OK:
            ulInfo64 = pAdapt->cRxSuccess;
            pInfo = &ulInfo64;
            if (InformationBufferLength >= sizeof(ULONG64) ||
                InformationBufferLength == 0)
            {
                ulInfoLen = sizeof(ULONG64);
            }
            else
            {
                ulInfoLen = sizeof(ULONG);
            }
            // We should always report that 8 bytes are required to keep ndistest happy
            *BytesNeeded =  sizeof(ULONG64);
            break;

        case OID_GEN_XMIT_ERROR:

            ulInfo = pAdapt->cTxError;
            break;

        case OID_GEN_RCV_ERROR:
            ulInfo = pAdapt->cRxError;
            break;

        case OID_GEN_RCV_NO_BUFFER:
            ulInfo = 0;
            break;

        case OID_GEN_RCV_CRC_ERROR:
            ulInfo = 0;
            break;

        case OID_GEN_TRANSMIT_QUEUE_LENGTH:
            ulInfo = PACKET_INFO_POOL_SIZE;
            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_ONE_COLLISION:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_DEFERRED:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_MAX_COLLISIONS:
            ulInfo = 0;
            break;

        case OID_802_3_RCV_OVERRUN:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_UNDERRUN:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_HEARTBEAT_FAILURE:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_TIMES_CRS_LOST:
            ulInfo = 0;
            break;

        case OID_802_3_XMIT_LATE_COLLISIONS:
            ulInfo = 0;
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if(Status == NDIS_STATUS_SUCCESS)
    {
        if(ulInfoLen <= InformationBufferLength)
        {
            // Copy result into InformationBuffer
            *BytesWritten = ulInfoLen;
            if(ulInfoLen)
            {
                NdisMoveMemory(InformationBuffer, pInfo, ulInfoLen);
            }
        }
        else
        {
            // too short
            *BytesNeeded = ulInfoLen;
            Status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
    }


    LogFlow(("<== vboxNetFltWinMpQueryInformation Status = 0x%08x\n",
                                                Status));

    return(Status);
}

NDIS_STATUS
vboxNetFltWinMpSetMulticastList(
    IN PADAPT              pAdapt,
    IN PVOID               InformationBuffer,
    IN ULONG               InformationBufferLength,
    OUT PULONG             pBytesRead,
    OUT PULONG             pBytesNeeded
    )
/*++
Routine Description:
    This routine will set up the adapter for a specified multicast
    address list.

Arguments:
    IN PMP_ADAPTER Adapter - Pointer to adapter block
    InformationBuffer       - Buffer for information
    InformationBufferLength   Size of this buffer
    pBytesRead                Specifies how much info is read
    BytesNeeded               In case the buffer is smaller than

Return Value:

    NDIS_STATUS

--*/
{
    NDIS_STATUS            Status = NDIS_STATUS_SUCCESS;
#if 0
    ULONG                  index;
#endif

    LogFlow(("==> vboxNetFltWinMpSetMulticastList\n"));

    //
    // Initialize.
    //
    *pBytesNeeded = 0;
    *pBytesRead = InformationBufferLength;

    do
    {
        if (InformationBufferLength % VBOXNETADP_ETH_ADDRESS_LENGTH)
        {
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        if (InformationBufferLength > (VBOXNETADP_MAX_MCAST_LIST * VBOXNETADP_ETH_ADDRESS_LENGTH))
        {
            Status = NDIS_STATUS_MULTICAST_FULL;
            *pBytesNeeded = VBOXNETADP_MAX_MCAST_LIST * VBOXNETADP_ETH_ADDRESS_LENGTH;
            break;
        }
#if 0
        //
        // Protect the list update with a lock if it can be updated by
        // another thread simultaneously.
        //

        NdisZeroMemory(pAdapt->aMCList,
                       sizeof(pAdapt->aMCList));

        NdisMoveMemory(pAdapt->aMCList,
                       InformationBuffer,
                       InformationBufferLength);

        pAdapt->cMCList =    InformationBufferLength / ETH_LENGTH_OF_ADDRESS;
#endif

    }
    while (FALSE);

    //
    // Program the hardware to add suport for these muticast addresses
    //

    LogFlow(("<== vboxNetFltWinMpSetMulticastList\n"));

    return(Status);

}


static NDIS_STATUS
vboxNetFltWinMpSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded)
/*++

Routine Description:

    This is the handler for an OID set operation.
    MiniportSetInformation runs at IRQL = DISPATCH_LEVEL.

Arguments:

    MiniportAdapterContext      Pointer to the adapter structure
    Oid                         Oid for this query
    InformationBuffer           Buffer for information
    InformationBufferLength     Size of this buffer
    BytesRead                   Specifies how much info is read
    BytesNeeded                 In case the buffer is smaller than what
                                we need, tell them how much is needed

Return Value:

    Return code from the NdisRequest below.

--*/
{
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    PADAPT                  pAdapt = (PADAPT) MiniportAdapterContext;
    PNDIS_PM_PACKET_PATTERN pPmPattern = NULL;

    LogFlow(("==> vboxNetFltWinMpSetInformation %s\n", vboxNetFltWinMpDbgGetOidName(Oid)));

    *BytesRead = 0;
    *BytesNeeded = 0;

    switch(Oid)
    {
        case OID_802_3_MULTICAST_LIST:
            //
            // Set the multicast address list on the NIC for packet reception.
            // The NIC driver can set a limit on the number of multicast
            // addresses bound protocol drivers can enable simultaneously.
            // NDIS returns NDIS_STATUS_MULTICAST_FULL if a protocol driver
            // exceeds this limit or if it specifies an invalid multicast
            // address.
            //
            Status = vboxNetFltWinMpSetMulticastList(
                            pAdapt,
                            InformationBuffer,
                            InformationBufferLength,
                            BytesRead,
                            BytesNeeded);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            //
            // Program the hardware to indicate the packets
            // of certain filter types.
            //
            if(InformationBufferLength != sizeof(ULONG))
            {
                *BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            *BytesRead = InformationBufferLength;

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            //
            // A protocol driver can set a suggested value for the number
            // of bytes to be used in its binding; however, the underlying
            // NIC driver is never required to limit its indications to
            // the value set.
            //
            if(InformationBufferLength != sizeof(ULONG)){
                *BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            break;

        case OID_PNP_SET_POWER:
            //
            // This OID notifies a miniport driver that its NIC will be
            // transitioning to the device power state specified in the
            // InformationBuffer. The miniport driver must always return
            // NDIS_STATUS_SUCCESS to an OID_PNP_SET_POWER request. An
            // OID_PNP_SET_POWER request may or may not be preceded by an
            // OID_PNP_QUERY_POWER request.
            //
            if (InformationBufferLength != sizeof(NDIS_DEVICE_POWER_STATE ))
            {
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            vboxNetFltWinMpProcessSetPowerOid(&Status, pAdapt, InformationBuffer, InformationBufferLength, BytesRead, BytesNeeded);
            break;
/*
        case OID_PNP_ADD_WAKE_UP_PATTERN:
            //
            // This OID is sent by a protocol driver to a miniport driver to
            // specify a wake-up pattern. The wake-up pattern, along with its mask,
            // is described by an NDIS_PM_PACKET_PATTERN structure.
            //
            pPmPattern = (PNDIS_PM_PACKET_PATTERN) InformationBuffer;
            if (InformationBufferLength < sizeof(NDIS_PM_PACKET_PATTERN))
            {
                Status = NDIS_STATUS_BUFFER_TOO_SHORT;

                *BytesNeeded = sizeof(NDIS_PM_PACKET_PATTERN);
                break;
            }
            if (InformationBufferLength < pPmPattern->PatternOffset + pPmPattern->PatternSize)
            {
                Status = NDIS_STATUS_BUFFER_TOO_SHORT;

                *BytesNeeded = pPmPattern->PatternOffset + pPmPattern->PatternSize;
                break;
            }
            *BytesRead = pPmPattern->PatternOffset + pPmPattern->PatternSize;
            Status = NDIS_STATUS_SUCCESS;
            bForwardRequest = TRUE;
            break;

        case OID_PNP_REMOVE_WAKE_UP_PATTERN:
            //
            // This OID requests the miniport driver to delete a wake-up pattern
            // that it previously received in an OID_PNP_ADD_WAKE_UP_PATTERN request.
            // The wake-up pattern, along with its mask, is described by an
            // NDIS_PM_PACKET_PATTERN structure.
            //
            pPmPattern = (PNDIS_PM_PACKET_PATTERN) InformationBuffer;
            if (InformationBufferLength < sizeof(NDIS_PM_PACKET_PATTERN))
            {
                Status = NDIS_STATUS_BUFFER_TOO_SHORT;

                *BytesNeeded = sizeof(NDIS_PM_PACKET_PATTERN);
                break;
            }
            if (InformationBufferLength < pPmPattern->PatternOffset + pPmPattern->PatternSize)
            {
                Status = NDIS_STATUS_BUFFER_TOO_SHORT;

                *BytesNeeded = pPmPattern->PatternOffset + pPmPattern->PatternSize;
                break;
            }
            *BytesRead = pPmPattern->PatternOffset + pPmPattern->PatternSize;
            Status = NDIS_STATUS_SUCCESS;
            bForwardRequest = TRUE;

            break;

        case OID_PNP_ENABLE_WAKE_UP:
            //
            // This OID specifies which wake-up capabilities a miniport
            // driver should enable in its NIC. Before the miniport
            // transitions to a low-power state (that is, before NDIS
            // sends the miniport driver an OID_PNP_SET_POWER request),
            // NDIS sends the miniport an OID_PNP_ENABLE_WAKE_UP request to
            // enable the appropriate wake-up capabilities.
            //
            DEBUGP(MP_INFO, ("--> OID_PNP_ENABLE_WAKE_UP\n"));
            if(InformationBufferLength != sizeof(ULONG))
            {
                *BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            *BytesRead = sizeof(ULONG);
            Status = NDIS_STATUS_SUCCESS;
            bForwardRequest = TRUE;
            break;
*/
        default:
            Status = NDIS_STATUS_INVALID_OID;
            break;

    }


    LogFlow(("<== vboxNetFltWinMpSetInformation Status = 0x%08x\n", Status));

    return(Status);
}

static PUCHAR vboxNetFltWinMpDbgGetOidName(ULONG oid)
{
    PCHAR oidName;

    switch (oid){

        #undef MAKECASE
        #define MAKECASE(oidx) case oidx: oidName = #oidx; break;

        MAKECASE(OID_GEN_SUPPORTED_LIST)
        MAKECASE(OID_GEN_HARDWARE_STATUS)
        MAKECASE(OID_GEN_MEDIA_SUPPORTED)
        MAKECASE(OID_GEN_MEDIA_IN_USE)
        MAKECASE(OID_GEN_MAXIMUM_LOOKAHEAD)
        MAKECASE(OID_GEN_MAXIMUM_FRAME_SIZE)
        MAKECASE(OID_GEN_LINK_SPEED)
        MAKECASE(OID_GEN_TRANSMIT_BUFFER_SPACE)
        MAKECASE(OID_GEN_RECEIVE_BUFFER_SPACE)
        MAKECASE(OID_GEN_TRANSMIT_BLOCK_SIZE)
        MAKECASE(OID_GEN_RECEIVE_BLOCK_SIZE)
        MAKECASE(OID_GEN_VENDOR_ID)
        MAKECASE(OID_GEN_VENDOR_DESCRIPTION)
        MAKECASE(OID_GEN_CURRENT_PACKET_FILTER)
        MAKECASE(OID_GEN_CURRENT_LOOKAHEAD)
        MAKECASE(OID_GEN_DRIVER_VERSION)
        MAKECASE(OID_GEN_MAXIMUM_TOTAL_SIZE)
        MAKECASE(OID_GEN_PROTOCOL_OPTIONS)
        MAKECASE(OID_GEN_MAC_OPTIONS)
        MAKECASE(OID_GEN_MEDIA_CONNECT_STATUS)
        MAKECASE(OID_GEN_MAXIMUM_SEND_PACKETS)
        MAKECASE(OID_GEN_VENDOR_DRIVER_VERSION)
        MAKECASE(OID_GEN_SUPPORTED_GUIDS)
        MAKECASE(OID_GEN_NETWORK_LAYER_ADDRESSES)
        MAKECASE(OID_GEN_TRANSPORT_HEADER_OFFSET)
        MAKECASE(OID_GEN_MEDIA_CAPABILITIES)
        MAKECASE(OID_GEN_PHYSICAL_MEDIUM)
        MAKECASE(OID_GEN_XMIT_OK)
        MAKECASE(OID_GEN_RCV_OK)
        MAKECASE(OID_GEN_XMIT_ERROR)
        MAKECASE(OID_GEN_RCV_ERROR)
        MAKECASE(OID_GEN_RCV_NO_BUFFER)
        MAKECASE(OID_GEN_DIRECTED_BYTES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_BYTES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_BYTES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_BYTES_RCV)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_RCV)
        MAKECASE(OID_GEN_MULTICAST_BYTES_RCV)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_RCV)
        MAKECASE(OID_GEN_BROADCAST_BYTES_RCV)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_RCV)
        MAKECASE(OID_GEN_RCV_CRC_ERROR)
        MAKECASE(OID_GEN_TRANSMIT_QUEUE_LENGTH)
        MAKECASE(OID_GEN_GET_TIME_CAPS)
        MAKECASE(OID_GEN_GET_NETCARD_TIME)
        MAKECASE(OID_GEN_NETCARD_LOAD)
        MAKECASE(OID_GEN_DEVICE_PROFILE)
        MAKECASE(OID_GEN_INIT_TIME_MS)
        MAKECASE(OID_GEN_RESET_COUNTS)
        MAKECASE(OID_GEN_MEDIA_SENSE_COUNTS)
        MAKECASE(OID_PNP_CAPABILITIES)
        MAKECASE(OID_PNP_SET_POWER)
        MAKECASE(OID_PNP_QUERY_POWER)
        MAKECASE(OID_PNP_ADD_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_REMOVE_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_ENABLE_WAKE_UP)
        MAKECASE(OID_802_3_PERMANENT_ADDRESS)
        MAKECASE(OID_802_3_CURRENT_ADDRESS)
        MAKECASE(OID_802_3_MULTICAST_LIST)
        MAKECASE(OID_802_3_MAXIMUM_LIST_SIZE)
        MAKECASE(OID_802_3_MAC_OPTIONS)
        MAKECASE(OID_802_3_RCV_ERROR_ALIGNMENT)
        MAKECASE(OID_802_3_XMIT_ONE_COLLISION)
        MAKECASE(OID_802_3_XMIT_MORE_COLLISIONS)
        MAKECASE(OID_802_3_XMIT_DEFERRED)
        MAKECASE(OID_802_3_XMIT_MAX_COLLISIONS)
        MAKECASE(OID_802_3_RCV_OVERRUN)
        MAKECASE(OID_802_3_XMIT_UNDERRUN)
        MAKECASE(OID_802_3_XMIT_HEARTBEAT_FAILURE)
        MAKECASE(OID_802_3_XMIT_TIMES_CRS_LOST)
        MAKECASE(OID_802_3_XMIT_LATE_COLLISIONS)

        default:
            oidName = "<** UNKNOWN OID **>";
            break;
    }

    return oidName;
}
#endif

/**
 * NDIS Miniport entry point called whenever protocols are done with
 * a packet that we had indicated up and they had queued up for returning
 * later.
 *
 * @param MiniportAdapterContext    - pointer to ADAPT structure
 * @param Packet    - packet being returned.
 * @return None. */
DECLHIDDEN(VOID)
vboxNetFltWinMpReturnPacket(
    IN NDIS_HANDLE             MiniportAdapterContext,
    IN PNDIS_PACKET            Packet
    )
{
    PADAPT            pAdapt = (PADAPT)MiniportAdapterContext;

    {
        /*
         * This is a packet allocated from this IM's receive packet pool.
         * Reclaim our packet, and return the original to the driver below.
         */

        PNDIS_PACKET    MyPacket;
        PRECV_RSVD      RecvRsvd;

        RecvRsvd = (PRECV_RSVD)(Packet->MiniportReserved);
        MyPacket = RecvRsvd->pOriginalPkt;

        if(MyPacket)
        {
            /* the packet was sent from underlying miniport */
            NdisFreePacket(Packet);
            NdisReturnPackets(&MyPacket, 1);
        }
        else
        {
            PVOID pBufToFree = RecvRsvd->pBufToFree;

            /* the packet was sent from NetFlt */
            vboxNetFltWinFreeSGNdisPacket(Packet, !pBufToFree);
            if(pBufToFree)
            {
                vboxNetFltWinMemFree(pBufToFree);
            }
        }

        vboxNetFltWinDereferenceAdapt(pAdapt);
    }
}

/** Miniport's transfer data handler.
 *
 * @param Packet                    Destination packet
 * @param BytesTransferred          Place-holder for how much data was copied
 * @param MiniportAdapterContext    Pointer to the adapter structure
 * @param MiniportReceiveContext    Context
 * @param ByteOffset                Offset into the packet for copying data
 * @param BytesToTransfer           How much to copy.
 * @return    Status of transfer */
static NDIS_STATUS
vboxNetFltWinMpTransferData(
    OUT PNDIS_PACKET            Packet,
    OUT PUINT                   BytesTransferred,
    IN NDIS_HANDLE              MiniportAdapterContext,
    IN NDIS_HANDLE              MiniportReceiveContext,
    IN UINT                     ByteOffset,
    IN UINT                     BytesToTransfer
    )
{
#ifndef VBOXNETADP

    PADAPT        pAdapt = (PADAPT)MiniportAdapterContext;
    NDIS_STATUS   Status;

    /*
     * Return, if the device is OFF
     */
    if (vboxNetFltWinGetPowerState(&pAdapt->PTState) != NdisDeviceStateD0
            || vboxNetFltWinGetPowerState(&pAdapt->MPState) != NdisDeviceStateD0)
    {
        return NDIS_STATUS_FAILURE;
    }

    NdisTransferData(&Status,
                     pAdapt->hBindingHandle,
                     MiniportReceiveContext,
                     ByteOffset,
                     BytesToTransfer,
                     Packet,
                     BytesTransferred);

    return(Status);
#else
    /* should never be here */
    Assert(0);
    return NDIS_STATUS_FAILURE;
#endif
}

/**
 * Halt handler. All the hard-work for clean-up is done here.
 *
 * @param MiniportAdapterContext    Pointer to the Adapter
 * @return    None. */
static VOID
vboxNetFltWinMpHalt(
    IN NDIS_HANDLE                MiniportAdapterContext
    )
{
    PADAPT             pAdapt = (PADAPT)MiniportAdapterContext;
#ifndef VBOXNETADP
    NDIS_STATUS        Status;
#endif

    LogFlow(("==>MiniportHalt: Adapt %p\n", pAdapt));

#ifndef VBOXNETADP
//    Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitializing);
//    if(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitializing)
    if(vboxNetFltWinGetAdaptState(pAdapt) == kVBoxAdaptState_Disconnecting)
    {
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitializing);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitializing);
        /* we're called from protocolUnbinAdapter, do our job */
        /*
         * If we have a valid bind, close the miniport below the protocol
         */
        vboxNetFltWinPtCloseAdapter(pAdapt, &Status);

        Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitializing);
        vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
    }
    else
#endif
    {
        /* we're NOT called from protocolUnbinAdapter, perform a full disconnect */
        NDIS_STATUS Status;

        Assert(/*vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initializing
                ||*/ vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
#ifndef VBOXNETADP
        AssertBreakpoint();
#endif
        Status = vboxNetFltWinDetachFromInterface(pAdapt, false);
        Assert(Status == NDIS_STATUS_SUCCESS);
/* you can not access the pAdapt after closure
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
#ifndef VBOXNETADP
        Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitialized);
        vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
#endif
*/
    }

    LogFlow(("<== MiniportHalt: pAdapt %p\n", pAdapt));
}

/**
 * register the miniport edge
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinMpRegister(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    )
{
    NDIS_MINIPORT_CHARACTERISTICS      MChars;
    NDIS_STATUS Status;

    NdisMInitializeWrapper(&g_hNdisWrapperHandle, DriverObject, RegistryPath, NULL);

    /*
     * Register the miniport with NDIS. Note that it is the miniport
     * which was started as a driver and not the protocol. Also the miniport
     * must be registered prior to the protocol since the protocol's BindAdapter
     * handler can be initiated anytime and when it is, it must be ready to
     * start driver instances.
     */

    NdisZeroMemory(&MChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

    MChars.MajorNdisVersion = VBOXNETFLT_MAJOR_NDIS_VERSION;
    MChars.MinorNdisVersion = VBOXNETFLT_MINOR_NDIS_VERSION;

    MChars.InitializeHandler = vboxNetFltWinMpInitialize;
    MChars.QueryInformationHandler = vboxNetFltWinMpQueryInformation;
    MChars.SetInformationHandler = vboxNetFltWinMpSetInformation;
    MChars.ResetHandler = NULL;
    MChars.TransferDataHandler = vboxNetFltWinMpTransferData;
    MChars.HaltHandler = vboxNetFltWinMpHalt;

    /*
     * We will disable the check for hang timeout so we do not
     * need a check for hang handler!
     */
    MChars.CheckForHangHandler = NULL;
    MChars.ReturnPacketHandler = vboxNetFltWinMpReturnPacket;

    /*
     * Either the Send or the SendPackets handler should be specified.
     * If SendPackets handler is specified, SendHandler is ignored
     */
    MChars.SendHandler = NULL;    /* vboxNetFltWinMpSend; */
    MChars.SendPacketsHandler = vboxNetFltWinMpSendPackets;

#ifndef VBOXNETADP
    Status = NdisIMRegisterLayeredMiniport(g_hNdisWrapperHandle,
                                              &MChars,
                                              sizeof(MChars),
                                              &g_hDriverHandle);
#else
    Status = NdisMRegisterMiniport(
                    g_hNdisWrapperHandle,
                    &MChars,
                    sizeof(MChars));
#endif
    if(Status == NDIS_STATUS_SUCCESS)
    {
# ifndef WIN9X
        NdisMRegisterUnloadHandler(g_hNdisWrapperHandle, vboxNetFltWinUnload);
# endif
    }

    return Status;
}

/**
 * deregister the miniport edge
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinMpDeregister()
{
#ifndef VBOXNETADP
    NdisIMDeregisterLayeredMiniport(g_hDriverHandle);
#else
    /* don't need to do anything here */
#endif
    NdisTerminateWrapper(g_hNdisWrapperHandle, NULL);
    return NDIS_STATUS_SUCCESS;
}

#ifndef VBOXNETADP
/**
 * return the miniport edge handle
 */
DECLHIDDEN(NDIS_HANDLE) vboxNetFltWinMpGetHandle()
{
    return g_hDriverHandle;
}

/**
 * initialize the instance of a device used for ioctl handling
 */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpInitializeDevideInstance(PADAPT pAdapt)
{
    NDIS_STATUS Status;
    /*
     * Now ask NDIS to initialize our miniport (upper) edge.
     * Set the flag below to synchronize with a possible call
     * to our protocol Unbind handler that may come in before
     * our miniport initialization happens.
     */
    Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initializing);
    /* this is doe in vboxNetFltWinPtInitPADAPT*/
    /*        NdisInitializeEvent(&pAdapt->MiniportInitEvent); */

    Status = NdisIMInitializeDeviceInstanceEx(g_hDriverHandle,
                                       &pAdapt->DeviceName,
                                       pAdapt);
    /* ensure we're taking into account the pAdapt->Status if our miniport halt handler was called */
    if(Status == NDIS_STATUS_SUCCESS)
    {
        if(pAdapt->Status == NDIS_STATUS_SUCCESS)
        {
//            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
//            vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Initialized);
            return NDIS_STATUS_SUCCESS;
        }
        AssertBreakpoint();
        vboxNetFltWinMpDeInitializeDevideInstance(pAdapt, &Status);
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
        return pAdapt->Status;
    }
    else
    {
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
    }
    return Status;
}

/**
 * deinitialize the instance of a device used for ioctl handling
 */
DECLHIDDEN(bool) vboxNetFltWinMpDeInitializeDevideInstance(PADAPT pAdapt, PNDIS_STATUS pStatus)
{
# ifndef WIN9X
    NDIS_STATUS LocalStatus;
    /*
     * Check if we had called NdisIMInitializeDeviceInstanceEx and
     * we are awaiting a call to MiniportInitialize.
     */
    if (vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initializing)
    {
        /*
         * Try to cancel the pending IMInit process.
         */
        LocalStatus = NdisIMCancelInitializeDeviceInstance(
                        g_hDriverHandle,
                        &pAdapt->DeviceName);

        if (LocalStatus == NDIS_STATUS_SUCCESS)
        {
            /*
             * Successfully cancelled IM Initialization; our
             * Miniport Initialize routine will not be called
             * for this device.
             */
            Assert(pAdapt->hMiniportHandle == NULL);
        }
        else
        {
            /*
             * Our Miniport Initialize routine will be called
             * (may be running on another thread at this time).
             * Wait for it to finish.
             */
            NdisWaitEvent(&pAdapt->MiniportInitEvent, 0);
        }

        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
    }
#endif

    /*
     * Call NDIS to remove our device-instance. We do most of the work
     * inside the HaltHandler.
     *
     * The Handle will be NULL if our miniport Halt Handler has been called or
     * if the IM device was never initialized
     */

    if (vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized)
    {
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitializing);

        *pStatus = NdisIMDeInitializeDeviceInstance(pAdapt->hMiniportHandle);

        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);

        if (*pStatus != NDIS_STATUS_SUCCESS)
        {
            *pStatus = NDIS_STATUS_FAILURE;
        }

        return true;
    }

    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);

    return false;
}
#endif

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpReferenceControlDevice()
{
    return vboxNetFltWinPtRegisterDevice();
}

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDereferenceControlDevice()
{
    return vboxNetFltWinPtDeregisterDevice();
}

#endif /* #ifndef VBOX_NETFLT_ONDEMAND_BIND*/
