/* $Id$ */
/** @file
 * VBoxManage - The 'list' command.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>

#include <VBox/log.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>

#include "VBoxManage.h"
using namespace com;

#ifdef VBOX_WITH_HOSTNETIF_API
static const char *getHostIfMediumTypeText(HostNetworkInterfaceMediumType_T enmType)
{
    switch (enmType)
    {
        case HostNetworkInterfaceMediumType_Ethernet: return "Ethernet";
        case HostNetworkInterfaceMediumType_PPP: return "PPP";
        case HostNetworkInterfaceMediumType_SLIP: return "SLIP";
    }
    return "Unknown";
}

static const char *getHostIfStatusText(HostNetworkInterfaceStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case HostNetworkInterfaceStatus_Up: return "Up";
        case HostNetworkInterfaceStatus_Down: return "Down";
    }
    return "Unknown";
}
#endif /* VBOX_WITH_HOSTNETIF_API */

static const char*getDeviceTypeText(DeviceType_T enmType)
{
    switch (enmType)
    {
        case DeviceType_HardDisk: return "HardDisk";
        case DeviceType_DVD: return "DVD";
        case DeviceType_Floppy: return "Floppy";
    }
    return "Unknown";
}

static void listMedia(const ComPtr<IVirtualBox> aVirtualBox,
                      const com::SafeIfaceArray<IMedium> &aMedia,
                      const char *pszParentUUIDStr)
{
    HRESULT rc;
    for (size_t i = 0; i < aMedia.size(); ++i)
    {
        ComPtr<IMedium> pMedium = aMedia[i];
        Bstr uuid;
        pMedium->COMGETTER(Id)(uuid.asOutParam());
        RTPrintf("UUID:        %s\n", Utf8Str(uuid).c_str());
        if (pszParentUUIDStr)
            RTPrintf("Parent UUID: %s\n", pszParentUUIDStr);
        Bstr format;
        pMedium->COMGETTER(Format)(format.asOutParam());
        RTPrintf("Format:      %ls\n", format.raw());
        Bstr filepath;
        pMedium->COMGETTER(Location)(filepath.asOutParam());
        RTPrintf("Location:    %ls\n", filepath.raw());

        MediumState_T enmState;
        pMedium->RefreshState(&enmState);
        const char *stateStr = "unknown";
        switch (enmState)
        {
            case MediumState_NotCreated:
                stateStr = "not created";
                break;
            case MediumState_Created:
                stateStr = "created";
                break;
            case MediumState_LockedRead:
                stateStr = "locked read";
                break;
            case MediumState_LockedWrite:
                stateStr = "locked write";
                break;
            case MediumState_Inaccessible:
                stateStr = "inaccessible";
                break;
            case MediumState_Creating:
                stateStr = "creating";
                break;
            case MediumState_Deleting:
                stateStr = "deleting";
                break;
        }
        RTPrintf("State:       %s\n", stateStr);

        MediumType_T type;
        pMedium->COMGETTER(Type)(&type);
        const char *typeStr = "unknown";
        switch (type)
        {
            case MediumType_Normal:
                typeStr = "normal";
                break;
            case MediumType_Immutable:
                typeStr = "immutable";
                break;
            case MediumType_Writethrough:
                typeStr = "writethrough";
                break;
            case MediumType_Shareable:
                typeStr = "shareable";
                break;
            case MediumType_Readonly:
                typeStr = "readonly";
                break;
            case MediumType_MultiAttach:
                typeStr = "multiattach";
                break;
        }
        RTPrintf("Type:        %s\n", typeStr);

        com::SafeArray<BSTR> machineIds;
        pMedium->COMGETTER(MachineIds)(ComSafeArrayAsOutParam(machineIds));
        for (size_t j = 0; j < machineIds.size(); ++j)
        {
            ComPtr<IMachine> machine;
            CHECK_ERROR(aVirtualBox, FindMachine(machineIds[j], machine.asOutParam()));
            ASSERT(machine);
            Bstr name;
            machine->COMGETTER(Name)(name.asOutParam());
            RTPrintf("%s%ls (UUID: %ls)",
                    j == 0 ? "Usage:       " : "             ",
                    name.raw(), machineIds[j]);
            com::SafeArray<BSTR> snapshotIds;
            pMedium->GetSnapshotIds(machineIds[j],
                                    ComSafeArrayAsOutParam(snapshotIds));
            for (size_t k = 0; k < snapshotIds.size(); ++k)
            {
                ComPtr<ISnapshot> snapshot;
                machine->FindSnapshot(snapshotIds[k], snapshot.asOutParam());
                if (snapshot)
                {
                    Bstr snapshotName;
                    snapshot->COMGETTER(Name)(snapshotName.asOutParam());
                    RTPrintf(" [%ls (UUID: %ls)]", snapshotName.raw(), snapshotIds[k]);
                }
            }
            RTPrintf("\n");
        }
        RTPrintf("\n");

        com::SafeIfaceArray<IMedium> children;
        CHECK_ERROR(pMedium, COMGETTER(Children)(ComSafeArrayAsOutParam(children)));
        if (children.size() > 0)
        {
            // depth first listing of child media
            listMedia(aVirtualBox, children, Utf8Str(uuid).c_str());
        }
    }
}


/**
 * List extension packs.
 *
 * @returns See produceList.
 * @param   rptrVirtualBox      Reference to the IVirtualBox smart pointer.
 */
static HRESULT listExtensionPacks(const ComPtr<IVirtualBox> &rptrVirtualBox)
{
    ComObjPtr<IExtPackManager> ptrExtPackMgr;
    CHECK_ERROR2_RET(rptrVirtualBox, COMGETTER(ExtensionPackManager)(ptrExtPackMgr.asOutParam()), hrcCheck);

    SafeIfaceArray<IExtPack> extPacks;
    CHECK_ERROR2_RET(ptrExtPackMgr, COMGETTER(InstalledExtPacks)(ComSafeArrayAsOutParam(extPacks)), hrcCheck);
    RTPrintf("Extension Packs: %u\n", extPacks.size());

    HRESULT hrc = S_OK;
    for (size_t i = 0; i < extPacks.size(); i++)
    {
        /* Read all the properties. */
        Bstr    bstrName;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Name)(bstrName.asOutParam()),          hrc = hrcCheck; bstrName.setNull());
        Bstr    bstrDesc;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Description)(bstrDesc.asOutParam()),   hrc = hrcCheck; bstrDesc.setNull());
        Bstr    bstrVersion;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Version)(bstrVersion.asOutParam()),    hrc = hrcCheck; bstrVersion.setNull());
        ULONG   uRevision;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Revision)(&uRevision),                 hrc = hrcCheck; uRevision = 0);
        Bstr    bstrEdition;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Edition)(bstrEdition.asOutParam()),    hrc = hrcCheck; bstrEdition.setNull());
        Bstr    bstrVrdeModule;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(VRDEModule)(bstrVrdeModule.asOutParam()),hrc=hrcCheck; bstrVrdeModule.setNull());
        BOOL    fUsable;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(Usable)(&fUsable),                     hrc = hrcCheck; fUsable = FALSE);
        Bstr    bstrWhy;
        CHECK_ERROR2_STMT(extPacks[i], COMGETTER(WhyUnusable)(bstrWhy.asOutParam()),    hrc = hrcCheck; bstrWhy.setNull());

        /* Display them. */
        if (i)
            RTPrintf("\n");
        RTPrintf("Pack no.%2zu:   %ls\n"
                 "Version:      %ls\n"
                 "Revision:     %u\n"
                 "Edition:      %ls\n"
                 "Description:  %ls\n"
                 "VRDE Module:  %ls\n"
                 "Usable:       %RTbool\n"
                 "Why unusable: %ls\n",
                 i, bstrName.raw(),
                 bstrVersion.raw(),
                 uRevision,
                 bstrEdition.raw(),
                 bstrDesc.raw(),
                 bstrVrdeModule.raw(),
                 fUsable != FALSE,
                 bstrWhy.raw());

        /* Query plugins and display them. */
    }
    return hrc;
}


/**
 * List machine groups.
 *
 * @returns See produceList.
 * @param   rptrVirtualBox      Reference to the IVirtualBox smart pointer.
 */
static HRESULT listGroups(const ComPtr<IVirtualBox> &rptrVirtualBox)
{
    SafeArray<BSTR> groups;
    CHECK_ERROR2_RET(rptrVirtualBox, COMGETTER(MachineGroups)(ComSafeArrayAsOutParam(groups)), hrcCheck);

    for (size_t i = 0; i < groups.size(); i++)
    {
        RTPrintf("\"%ls\"\n", groups[i]);
    }
    return S_OK;
}


/**
 * The type of lists we can produce.
 */
enum enmListType
{
    kListNotSpecified = 1000,
    kListVMs,
    kListRunningVMs,
    kListOsTypes,
    kListHostDvds,
    kListHostFloppies,
    kListBridgedInterfaces,
#if defined(VBOX_WITH_NETFLT)
    kListHostOnlyInterfaces,
#endif
    kListHostCpuIDs,
    kListHostInfo,
    kListHddBackends,
    kListHdds,
    kListDvds,
    kListFloppies,
    kListUsbHost,
    kListUsbFilters,
    kListSystemProperties,
    kListDhcpServers,
    kListExtPacks,
    kListGroups
};


/**
 * Produces the specified listing.
 *
 * @returns S_OK or some COM error code that has been reported in full.
 * @param   enmList             The list to produce.
 * @param   fOptLong            Long (@c true) or short list format.
 * @param   rptrVirtualBox      Reference to the IVirtualBox smart pointer.
 */
static HRESULT produceList(enum enmListType enmCommand, bool fOptLong, const ComPtr<IVirtualBox> &rptrVirtualBox)
{
    HRESULT rc = S_OK;
    switch (enmCommand)
    {
        case kListNotSpecified:
            AssertFailed();
            return E_FAIL;

        case kListVMs:
        {
            /*
             * Get the list of all registered VMs
             */
            com::SafeIfaceArray<IMachine> machines;
            rc = rptrVirtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
            if (SUCCEEDED(rc))
            {
                /*
                 * Iterate through the collection
                 */
                for (size_t i = 0; i < machines.size(); ++i)
                {
                    if (machines[i])
                        rc = showVMInfo(rptrVirtualBox, machines[i], fOptLong ? VMINFO_STANDARD : VMINFO_COMPACT);
                }
            }
            break;
        }

        case kListRunningVMs:
        {
            /*
             * Get the list of all _running_ VMs
             */
            com::SafeIfaceArray<IMachine> machines;
            rc = rptrVirtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
            com::SafeArray<MachineState_T> states;
            if (SUCCEEDED(rc))
                rc = rptrVirtualBox->GetMachineStates(ComSafeArrayAsInParam(machines), ComSafeArrayAsOutParam(states));
            if (SUCCEEDED(rc))
            {
                /*
                 * Iterate through the collection
                 */
                for (size_t i = 0; i < machines.size(); ++i)
                {
                    if (machines[i])
                    {
                        MachineState_T machineState = states[i];
                        switch (machineState)
                        {
                            case MachineState_Running:
                            case MachineState_Teleporting:
                            case MachineState_LiveSnapshotting:
                            case MachineState_Paused:
                            case MachineState_TeleportingPausedVM:
                                rc = showVMInfo(rptrVirtualBox, machines[i], fOptLong ? VMINFO_STANDARD : VMINFO_COMPACT);
                                break;
                        }
                    }
                }
            }
            break;
        }

        case kListOsTypes:
        {
            com::SafeIfaceArray<IGuestOSType> coll;
            rc = rptrVirtualBox->COMGETTER(GuestOSTypes)(ComSafeArrayAsOutParam(coll));
            if (SUCCEEDED(rc))
            {
                /*
                 * Iterate through the collection.
                 */
                for (size_t i = 0; i < coll.size(); ++i)
                {
                    ComPtr<IGuestOSType> guestOS;
                    guestOS = coll[i];
                    Bstr guestId;
                    guestOS->COMGETTER(Id)(guestId.asOutParam());
                    RTPrintf("ID:          %ls\n", guestId.raw());
                    Bstr guestDescription;
                    guestOS->COMGETTER(Description)(guestDescription.asOutParam());
                    RTPrintf("Description: %ls\n", guestDescription.raw());
                    Bstr familyId;
                    guestOS->COMGETTER(FamilyId)(familyId.asOutParam());
                    RTPrintf("Family ID:   %ls\n", familyId.raw());
                    Bstr familyDescription;
                    guestOS->COMGETTER(FamilyDescription)(familyDescription.asOutParam());
                    RTPrintf("Family Desc: %ls\n", familyDescription.raw());
                    BOOL is64Bit;
                    guestOS->COMGETTER(Is64Bit)(&is64Bit);
                    RTPrintf("64 bit:      %RTbool\n", is64Bit);
                    RTPrintf("\n");
                }
            }
            break;
        }

        case kListHostDvds:
        {
            ComPtr<IHost> host;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(Host)(host.asOutParam()));
            com::SafeIfaceArray<IMedium> coll;
            CHECK_ERROR(host, COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(coll)));
            if (SUCCEEDED(rc))
            {
                for (size_t i = 0; i < coll.size(); ++i)
                {
                    ComPtr<IMedium> dvdDrive = coll[i];
                    Bstr uuid;
                    dvdDrive->COMGETTER(Id)(uuid.asOutParam());
                    RTPrintf("UUID:         %s\n", Utf8Str(uuid).c_str());
                    Bstr location;
                    dvdDrive->COMGETTER(Location)(location.asOutParam());
                    RTPrintf("Name:         %ls\n\n", location.raw());
                }
            }
            break;
        }

        case kListHostFloppies:
        {
            ComPtr<IHost> host;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(Host)(host.asOutParam()));
            com::SafeIfaceArray<IMedium> coll;
            CHECK_ERROR(host, COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(coll)));
            if (SUCCEEDED(rc))
            {
                for (size_t i = 0; i < coll.size(); ++i)
                {
                    ComPtr<IMedium> floppyDrive = coll[i];
                    Bstr uuid;
                    floppyDrive->COMGETTER(Id)(uuid.asOutParam());
                    RTPrintf("UUID:         %s\n", Utf8Str(uuid).c_str());
                    Bstr location;
                    floppyDrive->COMGETTER(Location)(location.asOutParam());
                    RTPrintf("Name:         %ls\n\n", location.raw());
                }
            }
            break;
        }

        /** @todo function. */
        case kListBridgedInterfaces:
#if defined(VBOX_WITH_NETFLT)
        case kListHostOnlyInterfaces:
#endif
        {
            ComPtr<IHost> host;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(Host)(host.asOutParam()));
            com::SafeIfaceArray<IHostNetworkInterface> hostNetworkInterfaces;
#if defined(VBOX_WITH_NETFLT)
            if (enmCommand == kListBridgedInterfaces)
                CHECK_ERROR(host, FindHostNetworkInterfacesOfType(HostNetworkInterfaceType_Bridged,
                                                                  ComSafeArrayAsOutParam(hostNetworkInterfaces)));
            else
                CHECK_ERROR(host, FindHostNetworkInterfacesOfType(HostNetworkInterfaceType_HostOnly,
                                                                  ComSafeArrayAsOutParam(hostNetworkInterfaces)));
#else
            CHECK_ERROR(host, COMGETTER(NetworkInterfaces)(ComSafeArrayAsOutParam(hostNetworkInterfaces)));
#endif
            for (size_t i = 0; i < hostNetworkInterfaces.size(); ++i)
            {
                ComPtr<IHostNetworkInterface> networkInterface = hostNetworkInterfaces[i];
#ifndef VBOX_WITH_HOSTNETIF_API
                Bstr interfaceName;
                networkInterface->COMGETTER(Name)(interfaceName.asOutParam());
                RTPrintf("Name:        %ls\n", interfaceName.raw());
                Guid interfaceGuid;
                networkInterface->COMGETTER(Id)(interfaceGuid.asOutParam());
                RTPrintf("GUID:        %ls\n\n", Bstr(interfaceGuid.toString()).raw());
#else /* VBOX_WITH_HOSTNETIF_API */
                Bstr interfaceName;
                networkInterface->COMGETTER(Name)(interfaceName.asOutParam());
                RTPrintf("Name:            %ls\n", interfaceName.raw());
                Bstr interfaceGuid;
                networkInterface->COMGETTER(Id)(interfaceGuid.asOutParam());
                RTPrintf("GUID:            %ls\n", interfaceGuid.raw());
                BOOL bDhcpEnabled;
                networkInterface->COMGETTER(DhcpEnabled)(&bDhcpEnabled);
                RTPrintf("Dhcp:            %s\n", bDhcpEnabled ? "Enabled" : "Disabled");

                Bstr IPAddress;
                networkInterface->COMGETTER(IPAddress)(IPAddress.asOutParam());
                RTPrintf("IPAddress:       %ls\n", IPAddress.raw());
                Bstr NetworkMask;
                networkInterface->COMGETTER(NetworkMask)(NetworkMask.asOutParam());
                RTPrintf("NetworkMask:     %ls\n", NetworkMask.raw());
                Bstr IPV6Address;
                networkInterface->COMGETTER(IPV6Address)(IPV6Address.asOutParam());
                RTPrintf("IPV6Address:     %ls\n", IPV6Address.raw());
                ULONG IPV6NetworkMaskPrefixLength;
                networkInterface->COMGETTER(IPV6NetworkMaskPrefixLength)(&IPV6NetworkMaskPrefixLength);
                RTPrintf("IPV6NetworkMaskPrefixLength: %d\n", IPV6NetworkMaskPrefixLength);
                Bstr HardwareAddress;
                networkInterface->COMGETTER(HardwareAddress)(HardwareAddress.asOutParam());
                RTPrintf("HardwareAddress: %ls\n", HardwareAddress.raw());
                HostNetworkInterfaceMediumType_T Type;
                networkInterface->COMGETTER(MediumType)(&Type);
                RTPrintf("MediumType:      %s\n", getHostIfMediumTypeText(Type));
                HostNetworkInterfaceStatus_T Status;
                networkInterface->COMGETTER(Status)(&Status);
                RTPrintf("Status:          %s\n", getHostIfStatusText(Status));
                Bstr netName;
                networkInterface->COMGETTER(NetworkName)(netName.asOutParam());
                RTPrintf("VBoxNetworkName: %ls\n\n", netName.raw());
#endif
            }
            break;
        }

        /** @todo function. */
        case kListHostInfo:
        {
            ComPtr<IHost> Host;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(Host)(Host.asOutParam()));

            RTPrintf("Host Information:\n\n");

            LONG64      u64UtcTime = 0;
            CHECK_ERROR(Host, COMGETTER(UTCTime)(&u64UtcTime));
            RTTIMESPEC  timeSpec;
            char        szTime[32];
            RTPrintf("Host time: %s\n", RTTimeSpecToString(RTTimeSpecSetMilli(&timeSpec, u64UtcTime), szTime, sizeof(szTime)));

            ULONG processorOnlineCount = 0;
            CHECK_ERROR(Host, COMGETTER(ProcessorOnlineCount)(&processorOnlineCount));
            RTPrintf("Processor online count: %lu\n", processorOnlineCount);
            ULONG processorCount = 0;
            CHECK_ERROR(Host, COMGETTER(ProcessorCount)(&processorCount));
            RTPrintf("Processor count: %lu\n", processorCount);
            ULONG processorSpeed = 0;
            Bstr processorDescription;
            for (ULONG i = 0; i < processorCount; i++)
            {
                CHECK_ERROR(Host, GetProcessorSpeed(i, &processorSpeed));
                if (processorSpeed)
                    RTPrintf("Processor#%u speed: %lu MHz\n", i, processorSpeed);
                else
                    RTPrintf("Processor#%u speed: unknown\n", i, processorSpeed);
                CHECK_ERROR(Host, GetProcessorDescription(i, processorDescription.asOutParam()));
                RTPrintf("Processor#%u description: %ls\n", i, processorDescription.raw());
            }

            ULONG memorySize = 0;
            CHECK_ERROR(Host, COMGETTER(MemorySize)(&memorySize));
            RTPrintf("Memory size: %lu MByte\n", memorySize);

            ULONG memoryAvailable = 0;
            CHECK_ERROR(Host, COMGETTER(MemoryAvailable)(&memoryAvailable));
            RTPrintf("Memory available: %lu MByte\n", memoryAvailable);

            Bstr operatingSystem;
            CHECK_ERROR(Host, COMGETTER(OperatingSystem)(operatingSystem.asOutParam()));
            RTPrintf("Operating system: %ls\n", operatingSystem.raw());

            Bstr oSVersion;
            CHECK_ERROR(Host, COMGETTER(OSVersion)(oSVersion.asOutParam()));
            RTPrintf("Operating system version: %ls\n", oSVersion.raw());
            break;
        }

        case kListHostCpuIDs:
        {
            ComPtr<IHost> Host;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(Host)(Host.asOutParam()));

            RTPrintf("Host CPUIDs:\n\nLeaf no.  EAX      EBX      ECX      EDX\n");
            ULONG uCpuNo = 0; /* ASSUMES that CPU#0 is online. */
            static uint32_t const s_auCpuIdRanges[] =
            {
                UINT32_C(0x00000000), UINT32_C(0x0000007f),
                UINT32_C(0x80000000), UINT32_C(0x8000007f),
                UINT32_C(0xc0000000), UINT32_C(0xc000007f)
            };
            for (unsigned i = 0; i < RT_ELEMENTS(s_auCpuIdRanges); i += 2)
            {
                ULONG uEAX, uEBX, uECX, uEDX, cLeafs;
                CHECK_ERROR(Host, GetProcessorCPUIDLeaf(uCpuNo, s_auCpuIdRanges[i], 0, &cLeafs, &uEBX, &uECX, &uEDX));
                if (cLeafs < s_auCpuIdRanges[i] || cLeafs > s_auCpuIdRanges[i+1])
                    continue;
                cLeafs++;
                for (ULONG iLeaf = s_auCpuIdRanges[i]; iLeaf <= cLeafs; iLeaf++)
                {
                    CHECK_ERROR(Host, GetProcessorCPUIDLeaf(uCpuNo, iLeaf, 0, &uEAX, &uEBX, &uECX, &uEDX));
                    RTPrintf("%08x  %08x %08x %08x %08x\n", iLeaf, uEAX, uEBX, uECX, uEDX);
                }
            }
            break;
        }

        /** @todo function. */
        case kListHddBackends:
        {
            ComPtr<ISystemProperties>           systemProperties;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(SystemProperties)(systemProperties.asOutParam()));
            com::SafeIfaceArray<IMediumFormat>  mediumFormats;
            CHECK_ERROR(systemProperties, COMGETTER(MediumFormats)(ComSafeArrayAsOutParam(mediumFormats)));

            RTPrintf("Supported hard disk backends:\n\n");
            for (size_t i = 0; i < mediumFormats.size(); ++i)
            {
                /* General information */
                Bstr id;
                CHECK_ERROR(mediumFormats[i], COMGETTER(Id)(id.asOutParam()));

                Bstr description;
                CHECK_ERROR(mediumFormats[i],
                            COMGETTER(Id)(description.asOutParam()));

                ULONG caps;
                CHECK_ERROR(mediumFormats[i],
                            COMGETTER(Capabilities)(&caps));

                RTPrintf("Backend %u: id='%ls' description='%ls' capabilities=%#06x extensions='",
                        i, id.raw(), description.raw(), caps);

                /* File extensions */
                com::SafeArray <BSTR> fileExtensions;
                com::SafeArray <DeviceType_T> deviceTypes;
                CHECK_ERROR(mediumFormats[i],
                            DescribeFileExtensions(ComSafeArrayAsOutParam(fileExtensions), ComSafeArrayAsOutParam(deviceTypes)));
                for (size_t j = 0; j < fileExtensions.size(); ++j)
                {
                    RTPrintf("%ls (%s)", Bstr(fileExtensions[j]).raw(), getDeviceTypeText(deviceTypes[j]));
                    if (j != fileExtensions.size()-1)
                        RTPrintf(",");
                }
                RTPrintf("'");

                /* Configuration keys */
                com::SafeArray <BSTR> propertyNames;
                com::SafeArray <BSTR> propertyDescriptions;
                com::SafeArray <DataType_T> propertyTypes;
                com::SafeArray <ULONG> propertyFlags;
                com::SafeArray <BSTR> propertyDefaults;
                CHECK_ERROR(mediumFormats[i],
                            DescribeProperties(ComSafeArrayAsOutParam(propertyNames),
                                                ComSafeArrayAsOutParam(propertyDescriptions),
                                                ComSafeArrayAsOutParam(propertyTypes),
                                                ComSafeArrayAsOutParam(propertyFlags),
                                                ComSafeArrayAsOutParam(propertyDefaults)));

                RTPrintf(" properties=(");
                if (propertyNames.size() > 0)
                {
                    for (size_t j = 0; j < propertyNames.size(); ++j)
                    {
                        RTPrintf("\n  name='%ls' desc='%ls' type=",
                                Bstr(propertyNames[j]).raw(), Bstr(propertyDescriptions[j]).raw());
                        switch (propertyTypes[j])
                        {
                            case DataType_Int32: RTPrintf("int"); break;
                            case DataType_Int8: RTPrintf("byte"); break;
                            case DataType_String: RTPrintf("string"); break;
                        }
                        RTPrintf(" flags=%#04x", propertyFlags[j]);
                        RTPrintf(" default='%ls'", Bstr(propertyDefaults[j]).raw());
                        if (j != propertyNames.size()-1)
                            RTPrintf(", ");
                    }
                }
                RTPrintf(")\n");
            }
            break;
        }

        case kListHdds:
        {
            com::SafeIfaceArray<IMedium> hdds;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(HardDisks)(ComSafeArrayAsOutParam(hdds)));
            listMedia(rptrVirtualBox, hdds, "base");
            break;
        }

        case kListDvds:
        {
            com::SafeIfaceArray<IMedium> dvds;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(DVDImages)(ComSafeArrayAsOutParam(dvds)));
            listMedia(rptrVirtualBox, dvds, NULL);
            break;
        }

        case kListFloppies:
        {
            com::SafeIfaceArray<IMedium> floppies;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(FloppyImages)(ComSafeArrayAsOutParam(floppies)));
            listMedia(rptrVirtualBox, floppies, NULL);
            break;
        }

        /** @todo function. */
        case kListUsbHost:
        {
            ComPtr<IHost> Host;
            CHECK_ERROR_RET(rptrVirtualBox, COMGETTER(Host)(Host.asOutParam()), 1);

            SafeIfaceArray<IHostUSBDevice> CollPtr;
            CHECK_ERROR_RET(Host, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(CollPtr)), 1);

            RTPrintf("Host USB Devices:\n\n");

            if (CollPtr.size() == 0)
            {
                RTPrintf("<none>\n\n");
            }
            else
            {
                for (size_t i = 0; i < CollPtr.size(); ++i)
                {
                    ComPtr <IHostUSBDevice> dev = CollPtr[i];

                    /* Query info. */
                    Bstr id;
                    CHECK_ERROR_RET(dev, COMGETTER(Id)(id.asOutParam()), 1);
                    USHORT usVendorId;
                    CHECK_ERROR_RET(dev, COMGETTER(VendorId)(&usVendorId), 1);
                    USHORT usProductId;
                    CHECK_ERROR_RET(dev, COMGETTER(ProductId)(&usProductId), 1);
                    USHORT bcdRevision;
                    CHECK_ERROR_RET(dev, COMGETTER(Revision)(&bcdRevision), 1);
                    USHORT usPort;
                    CHECK_ERROR_RET(dev, COMGETTER(Port)(&usPort), 1);
                    USHORT usVersion;
                    CHECK_ERROR_RET(dev, COMGETTER(Version)(&usVersion), 1);
                    USHORT usPortVersion;
                    CHECK_ERROR_RET(dev, COMGETTER(PortVersion)(&usPortVersion), 1);

                    RTPrintf("UUID:               %s\n"
                             "VendorId:           %#06x (%04X)\n"
                             "ProductId:          %#06x (%04X)\n"
                             "Revision:           %u.%u (%02u%02u)\n"
                             "Port:               %u\n"
                             "USB version/speed:  %u/%u\n",
                             Utf8Str(id).c_str(),
                             usVendorId, usVendorId, usProductId, usProductId,
                             bcdRevision >> 8, bcdRevision & 0xff,
                             bcdRevision >> 8, bcdRevision & 0xff,
                             usPort, usVersion, usPortVersion);

                    /* optional stuff. */
                    Bstr bstr;
                    CHECK_ERROR_RET(dev, COMGETTER(Manufacturer)(bstr.asOutParam()), 1);
                    if (!bstr.isEmpty())
                        RTPrintf("Manufacturer:       %ls\n", bstr.raw());
                    CHECK_ERROR_RET(dev, COMGETTER(Product)(bstr.asOutParam()), 1);
                    if (!bstr.isEmpty())
                        RTPrintf("Product:            %ls\n", bstr.raw());
                    CHECK_ERROR_RET(dev, COMGETTER(SerialNumber)(bstr.asOutParam()), 1);
                    if (!bstr.isEmpty())
                        RTPrintf("SerialNumber:       %ls\n", bstr.raw());
                    CHECK_ERROR_RET(dev, COMGETTER(Address)(bstr.asOutParam()), 1);
                    if (!bstr.isEmpty())
                        RTPrintf("Address:            %ls\n", bstr.raw());

                    /* current state  */
                    USBDeviceState_T state;
                    CHECK_ERROR_RET(dev, COMGETTER(State)(&state), 1);
                    const char *pszState = "?";
                    switch (state)
                    {
                        case USBDeviceState_NotSupported:
                            pszState = "Not supported";
                            break;
                        case USBDeviceState_Unavailable:
                            pszState = "Unavailable";
                            break;
                        case USBDeviceState_Busy:
                            pszState = "Busy";
                            break;
                        case USBDeviceState_Available:
                            pszState = "Available";
                            break;
                        case USBDeviceState_Held:
                            pszState = "Held";
                            break;
                        case USBDeviceState_Captured:
                            pszState = "Captured";
                            break;
                        default:
                            ASSERT(false);
                            break;
                    }
                    RTPrintf("Current State:      %s\n\n", pszState);
                }
            }
            break;
        }

        /** @todo function. */
        case kListUsbFilters:
        {
            RTPrintf("Global USB Device Filters:\n\n");

            ComPtr<IHost> host;
            CHECK_ERROR_RET(rptrVirtualBox, COMGETTER(Host)(host.asOutParam()), 1);

            SafeIfaceArray<IHostUSBDeviceFilter> coll;
            CHECK_ERROR_RET(host, COMGETTER(USBDeviceFilters)(ComSafeArrayAsOutParam(coll)), 1);

            if (coll.size() == 0)
            {
                RTPrintf("<none>\n\n");
            }
            else
            {
                for (size_t index = 0; index < coll.size(); ++index)
                {
                    ComPtr<IHostUSBDeviceFilter> flt = coll[index];

                    /* Query info. */

                    RTPrintf("Index:            %zu\n", index);

                    BOOL active = FALSE;
                    CHECK_ERROR_RET(flt, COMGETTER(Active)(&active), 1);
                    RTPrintf("Active:           %s\n", active ? "yes" : "no");

                    USBDeviceFilterAction_T action;
                    CHECK_ERROR_RET(flt, COMGETTER(Action)(&action), 1);
                    const char *pszAction = "<invalid>";
                    switch (action)
                    {
                        case USBDeviceFilterAction_Ignore:
                            pszAction = "Ignore";
                            break;
                        case USBDeviceFilterAction_Hold:
                            pszAction = "Hold";
                            break;
                        default:
                            break;
                    }
                    RTPrintf("Action:           %s\n", pszAction);

                    Bstr bstr;
                    CHECK_ERROR_RET(flt, COMGETTER(Name)(bstr.asOutParam()), 1);
                    RTPrintf("Name:             %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(VendorId)(bstr.asOutParam()), 1);
                    RTPrintf("VendorId:         %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(ProductId)(bstr.asOutParam()), 1);
                    RTPrintf("ProductId:        %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(Revision)(bstr.asOutParam()), 1);
                    RTPrintf("Revision:         %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(Manufacturer)(bstr.asOutParam()), 1);
                    RTPrintf("Manufacturer:     %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(Product)(bstr.asOutParam()), 1);
                    RTPrintf("Product:          %ls\n", bstr.raw());
                    CHECK_ERROR_RET(flt, COMGETTER(SerialNumber)(bstr.asOutParam()), 1);
                    RTPrintf("Serial Number:    %ls\n\n", bstr.raw());
                }
            }
            break;
        }

        /** @todo function. */
        case kListSystemProperties:
        {
            ComPtr<ISystemProperties> systemProperties;
            rptrVirtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

            Bstr str;
            ULONG ulValue;
            LONG64 i64Value;

            rptrVirtualBox->COMGETTER(APIVersion)(str.asOutParam());
            RTPrintf("API version:                     %ls\n", str.raw());

            systemProperties->COMGETTER(MinGuestRAM)(&ulValue);
            RTPrintf("Minimum guest RAM size:          %u Megabytes\n", ulValue);
            systemProperties->COMGETTER(MaxGuestRAM)(&ulValue);
            RTPrintf("Maximum guest RAM size:          %u Megabytes\n", ulValue);
            systemProperties->COMGETTER(MinGuestVRAM)(&ulValue);
            RTPrintf("Minimum video RAM size:          %u Megabytes\n", ulValue);
            systemProperties->COMGETTER(MaxGuestVRAM)(&ulValue);
            RTPrintf("Maximum video RAM size:          %u Megabytes\n", ulValue);
            systemProperties->COMGETTER(MinGuestCPUCount)(&ulValue);
            RTPrintf("Minimum guest CPU count:         %u\n", ulValue);
            systemProperties->COMGETTER(MaxGuestCPUCount)(&ulValue);
            RTPrintf("Maximum guest CPU count:         %u\n", ulValue);
            systemProperties->COMGETTER(InfoVDSize)(&i64Value);
            RTPrintf("Virtual disk limit (info):       %lld Bytes\n", i64Value);
            systemProperties->COMGETTER(SerialPortCount)(&ulValue);
            RTPrintf("Maximum Serial Port count:       %u\n", ulValue);
            systemProperties->COMGETTER(ParallelPortCount)(&ulValue);
            RTPrintf("Maximum Parallel Port count:     %u\n", ulValue);
            systemProperties->COMGETTER(MaxBootPosition)(&ulValue);
            RTPrintf("Maximum Boot Position:           %u\n", ulValue);
            systemProperties->GetMaxNetworkAdapters(ChipsetType_PIIX3, &ulValue);
            RTPrintf("Maximum PIIX3 Network Adapter count:   %u\n", ulValue);
            systemProperties->GetMaxNetworkAdapters(ChipsetType_ICH9,  &ulValue);
            RTPrintf("Maximum ICH9 Network Adapter count:   %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_PIIX3, StorageBus_IDE, &ulValue);
            RTPrintf("Maximum PIIX3 IDE Controllers:   %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_ICH9, StorageBus_IDE, &ulValue);
            RTPrintf("Maximum ICH9 IDE Controllers:    %u\n", ulValue);
            systemProperties->GetMaxPortCountForStorageBus(StorageBus_IDE, &ulValue);
            RTPrintf("Maximum IDE Port count:          %u\n", ulValue);
            systemProperties->GetMaxDevicesPerPortForStorageBus(StorageBus_IDE, &ulValue);
            RTPrintf("Maximum Devices per IDE Port:    %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_PIIX3, StorageBus_SATA, &ulValue);
            RTPrintf("Maximum PIIX3 SATA Controllers:  %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_ICH9, StorageBus_SATA, &ulValue);
            RTPrintf("Maximum ICH9 SATA Controllers:   %u\n", ulValue);
            systemProperties->GetMaxPortCountForStorageBus(StorageBus_SATA, &ulValue);
            RTPrintf("Maximum SATA Port count:         %u\n", ulValue);
            systemProperties->GetMaxDevicesPerPortForStorageBus(StorageBus_SATA, &ulValue);
            RTPrintf("Maximum Devices per SATA Port:   %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_PIIX3, StorageBus_SCSI, &ulValue);
            RTPrintf("Maximum PIIX3 SCSI Controllers:  %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_ICH9, StorageBus_SCSI, &ulValue);
            RTPrintf("Maximum ICH9 SCSI Controllers:   %u\n", ulValue);
            systemProperties->GetMaxPortCountForStorageBus(StorageBus_SCSI, &ulValue);
            RTPrintf("Maximum SCSI Port count:         %u\n", ulValue);
            systemProperties->GetMaxDevicesPerPortForStorageBus(StorageBus_SCSI, &ulValue);
            RTPrintf("Maximum Devices per SCSI Port:   %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_PIIX3, StorageBus_SAS, &ulValue);
            RTPrintf("Maximum SAS PIIX3 Controllers:   %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_ICH9, StorageBus_SAS, &ulValue);
            RTPrintf("Maximum SAS ICH9 Controllers:    %u\n", ulValue);
            systemProperties->GetMaxPortCountForStorageBus(StorageBus_SAS, &ulValue);
            RTPrintf("Maximum SAS Port count:          %u\n", ulValue);
            systemProperties->GetMaxDevicesPerPortForStorageBus(StorageBus_SAS, &ulValue);
            RTPrintf("Maximum Devices per SAS Port:    %u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_PIIX3, StorageBus_Floppy, &ulValue);
            RTPrintf("Maximum PIIX3 Floppy Controllers:%u\n", ulValue);
            systemProperties->GetMaxInstancesOfStorageBus(ChipsetType_ICH9, StorageBus_Floppy, &ulValue);
            RTPrintf("Maximum ICH9 Floppy Controllers: %u\n", ulValue);
            systemProperties->GetMaxPortCountForStorageBus(StorageBus_Floppy, &ulValue);
            RTPrintf("Maximum Floppy Port count:       %u\n", ulValue);
            systemProperties->GetMaxDevicesPerPortForStorageBus(StorageBus_Floppy, &ulValue);
            RTPrintf("Maximum Devices per Floppy Port: %u\n", ulValue);
            systemProperties->COMGETTER(DefaultMachineFolder)(str.asOutParam());
            RTPrintf("Default machine folder:          %ls\n", str.raw());
            systemProperties->COMGETTER(VRDEAuthLibrary)(str.asOutParam());
            RTPrintf("VRDE auth library:               %ls\n", str.raw());
            systemProperties->COMGETTER(WebServiceAuthLibrary)(str.asOutParam());
            RTPrintf("Webservice auth. library:        %ls\n", str.raw());
            systemProperties->COMGETTER(DefaultVRDEExtPack)(str.asOutParam());
            RTPrintf("Remote desktop ExtPack:          %ls\n", str.raw());
            systemProperties->COMGETTER(LogHistoryCount)(&ulValue);
            RTPrintf("Log history count:               %u\n", ulValue);
            break;
        }

        case kListDhcpServers:
        {
            com::SafeIfaceArray<IDHCPServer> svrs;
            CHECK_ERROR(rptrVirtualBox, COMGETTER(DHCPServers)(ComSafeArrayAsOutParam(svrs)));
            for (size_t i = 0; i < svrs.size(); ++i)
            {
                ComPtr<IDHCPServer> svr = svrs[i];
                Bstr netName;
                svr->COMGETTER(NetworkName)(netName.asOutParam());
                RTPrintf("NetworkName:    %ls\n", netName.raw());
                Bstr ip;
                svr->COMGETTER(IPAddress)(ip.asOutParam());
                RTPrintf("IP:             %ls\n", ip.raw());
                Bstr netmask;
                svr->COMGETTER(NetworkMask)(netmask.asOutParam());
                RTPrintf("NetworkMask:    %ls\n", netmask.raw());
                Bstr lowerIp;
                svr->COMGETTER(LowerIP)(lowerIp.asOutParam());
                RTPrintf("lowerIPAddress: %ls\n", lowerIp.raw());
                Bstr upperIp;
                svr->COMGETTER(UpperIP)(upperIp.asOutParam());
                RTPrintf("upperIPAddress: %ls\n", upperIp.raw());
                BOOL fEnabled;
                svr->COMGETTER(Enabled)(&fEnabled);
                RTPrintf("Enabled:        %s\n", fEnabled ? "Yes" : "No");
                RTPrintf("\n");
            }
            break;
        }

        case kListExtPacks:
            rc = listExtensionPacks(rptrVirtualBox);
            break;

        case kListGroups:
            rc = listGroups(rptrVirtualBox);
            break;

        /* No default here, want gcc warnings. */

    } /* end switch */

    return rc;
}

/**
 * Handles the 'list' command.
 *
 * @returns Appropriate exit code.
 * @param   a                   Handler argument.
 */
int handleList(HandlerArg *a)
{
    bool                fOptLong      = false;
    bool                fOptMultiple  = false;
    enum enmListType    enmOptCommand = kListNotSpecified;

    static const RTGETOPTDEF s_aListOptions[] =
    {
        { "--long",             'l',                     RTGETOPT_REQ_NOTHING },
        { "--multiple",         'm',                     RTGETOPT_REQ_NOTHING }, /* not offical yet */
        { "vms",                kListVMs,                RTGETOPT_REQ_NOTHING },
        { "runningvms",         kListRunningVMs,         RTGETOPT_REQ_NOTHING },
        { "ostypes",            kListOsTypes,            RTGETOPT_REQ_NOTHING },
        { "hostdvds",           kListHostDvds,           RTGETOPT_REQ_NOTHING },
        { "hostfloppies",       kListHostFloppies,       RTGETOPT_REQ_NOTHING },
        { "hostifs",            kListBridgedInterfaces,  RTGETOPT_REQ_NOTHING }, /* backward compatibility */
        { "bridgedifs",         kListBridgedInterfaces,  RTGETOPT_REQ_NOTHING },
#if defined(VBOX_WITH_NETFLT)
        { "hostonlyifs",        kListHostOnlyInterfaces, RTGETOPT_REQ_NOTHING },
#endif
        { "hostinfo",           kListHostInfo,           RTGETOPT_REQ_NOTHING },
        { "hostcpuids",         kListHostCpuIDs,         RTGETOPT_REQ_NOTHING },
        { "hddbackends",        kListHddBackends,        RTGETOPT_REQ_NOTHING },
        { "hdds",               kListHdds,               RTGETOPT_REQ_NOTHING },
        { "dvds",               kListDvds,               RTGETOPT_REQ_NOTHING },
        { "floppies",           kListFloppies,           RTGETOPT_REQ_NOTHING },
        { "usbhost",            kListUsbHost,            RTGETOPT_REQ_NOTHING },
        { "usbfilters",         kListUsbFilters,         RTGETOPT_REQ_NOTHING },
        { "systemproperties",   kListSystemProperties,   RTGETOPT_REQ_NOTHING },
        { "dhcpservers",        kListDhcpServers,        RTGETOPT_REQ_NOTHING },
        { "extpacks",           kListExtPacks,           RTGETOPT_REQ_NOTHING },
        { "groups",             kListGroups,             RTGETOPT_REQ_NOTHING },
    };

    int                 ch;
    RTGETOPTUNION       ValueUnion;
    RTGETOPTSTATE       GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aListOptions, RT_ELEMENTS(s_aListOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'l':  /* --long */
                fOptLong = true;
                break;

            case 'm':
                fOptMultiple = true;
                if (enmOptCommand == kListNotSpecified)
                    break;
                ch = enmOptCommand;
                /* fall thru */

            case kListVMs:
            case kListRunningVMs:
            case kListOsTypes:
            case kListHostDvds:
            case kListHostFloppies:
            case kListBridgedInterfaces:
#if defined(VBOX_WITH_NETFLT)
            case kListHostOnlyInterfaces:
#endif
            case kListHostInfo:
            case kListHostCpuIDs:
            case kListHddBackends:
            case kListHdds:
            case kListDvds:
            case kListFloppies:
            case kListUsbHost:
            case kListUsbFilters:
            case kListSystemProperties:
            case kListDhcpServers:
            case kListExtPacks:
            case kListGroups:
                enmOptCommand = (enum enmListType)ch;
                if (fOptMultiple)
                {
                    HRESULT hrc = produceList((enum enmListType)ch, fOptLong, a->virtualBox);
                    if (FAILED(hrc))
                        return 1;
                }
                break;

            case VINF_GETOPT_NOT_OPTION:
                return errorSyntax(USAGE_LIST, "Unknown subcommand \"%s\".", ValueUnion.psz);

            default:
                return errorGetOpt(USAGE_LIST, ch, &ValueUnion);
        }
    }

    /*
     * If not in multiple list mode, we have to produce the list now.
     */
    if (enmOptCommand == kListNotSpecified)
        return errorSyntax(USAGE_LIST, "Missing subcommand for \"list\" command.\n");
    if (!fOptMultiple)
    {
        HRESULT hrc = produceList(enmOptCommand, fOptLong, a->virtualBox);
        if (FAILED(hrc))
            return 1;
    }

    return 0;
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
