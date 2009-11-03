/* $Id$ */
/** @file
 * VBoxManage - The 'showvminfo' command and helper routines.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////

void showSnapshots(ComPtr<ISnapshot> &rootSnapshot,
                   ComPtr<ISnapshot> &currentSnapshot,
                   VMINFO_DETAILS details,
                   const Bstr &prefix /* = ""*/,
                   int level /*= 0*/)
{
    /* start with the root */
    Bstr name;
    Bstr uuid;
    rootSnapshot->COMGETTER(Name)(name.asOutParam());
    rootSnapshot->COMGETTER(Id)(uuid.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
    {
        /* print with hierarchical numbering */
        RTPrintf("SnapshotName%lS=\"%lS\"\n", prefix.raw(), name.raw());
        RTPrintf("SnapshotUUID%lS=\"%s\"\n", prefix.raw(), Utf8Str(uuid).raw());
    }
    else
    {
        /* print with indentation */
        bool fCurrent = (rootSnapshot == currentSnapshot);
        RTPrintf("   %lSName: %lS (UUID: %s)%s\n",
                 prefix.raw(),
                 name.raw(),
                 Utf8Str(uuid).raw(),
                 (fCurrent) ? " *" : "");
    }

    /* get the children */
    SafeIfaceArray <ISnapshot> coll;
    rootSnapshot->COMGETTER(Children)(ComSafeArrayAsOutParam(coll));
    if (!coll.isNull())
    {
        for (size_t index = 0; index < coll.size(); ++index)
        {
            ComPtr<ISnapshot> snapshot = coll[index];
            if (snapshot)
            {
                Bstr newPrefix;
                if (details == VMINFO_MACHINEREADABLE)
                    newPrefix = Utf8StrFmt("%lS-%d", prefix.raw(), index + 1);
                else
                {
                    newPrefix = Utf8StrFmt("%lS   ", prefix.raw());
                }

                /* recursive call */
                showSnapshots(snapshot, currentSnapshot, details, newPrefix, level + 1);
            }
        }
    }
}

static void makeTimeStr (char *s, int cb, int64_t millies)
{
    RTTIME t;
    RTTIMESPEC ts;

    RTTimeSpecSetMilli(&ts, millies);

    RTTimeExplode (&t, &ts);

    RTStrPrintf(s, cb, "%04d/%02d/%02d %02d:%02d:%02d UTC",
                        t.i32Year, t.u8Month, t.u8MonthDay,
                        t.u8Hour, t.u8Minute, t.u8Second);
}

/* Disable global optimizations for MSC 8.0/64 to make it compile in reasonable
   time. MSC 7.1/32 doesn't have quite as much trouble with it, but still
   sufficient to qualify for this hack as well since this code isn't performance
   critical and probably won't gain much from the extra optimizing in real life. */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

HRESULT showVMInfo (ComPtr<IVirtualBox> virtualBox,
                    ComPtr<IMachine> machine,
                    VMINFO_DETAILS details /*= VMINFO_NONE*/,
                    ComPtr<IConsole> console /*= ComPtr <IConsole> ()*/)
{
    HRESULT rc;

    /*
     * The rules for output in -argdump format:
     * 1) the key part (the [0-9a-zA-Z_]+ string before the '=' delimiter)
     *    is all lowercase for "VBoxManage modifyvm" parameters. Any
     *    other values printed are in CamelCase.
     * 2) strings (anything non-decimal) are printed surrounded by
     *    double quotes '"'. If the strings themselves contain double
     *    quotes, these characters are escaped by '\'. Any '\' character
     *    in the original string is also escaped by '\'.
     * 3) numbers (containing just [0-9\-]) are written out unchanged.
     */

    /** @todo the quoting is not yet implemented! */
    /** @todo error checking! */

    BOOL accessible = FALSE;
    CHECK_ERROR(machine, COMGETTER(Accessible)(&accessible));
    CheckComRCReturnRC(rc);

    Bstr uuid;
    rc = machine->COMGETTER(Id)(uuid.asOutParam());

    if (!accessible)
    {
        if (details == VMINFO_COMPACT)
            RTPrintf("\"<inaccessible>\" {%s}\n", Utf8Str(uuid).raw());
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("name=\"<inaccessible>\"\n");
            else
                RTPrintf("Name:            <inaccessible!>\n");
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("UUID=\"%s\"\n", Utf8Str(uuid).raw());
            else
                RTPrintf("UUID:            %s\n", Utf8Str(uuid).raw());
            if (details != VMINFO_MACHINEREADABLE)
            {
                Bstr settingsFilePath;
                rc = machine->COMGETTER(SettingsFilePath)(settingsFilePath.asOutParam());
                RTPrintf("Config file:     %lS\n", settingsFilePath.raw());
                ComPtr<IVirtualBoxErrorInfo> accessError;
                rc = machine->COMGETTER(AccessError)(accessError.asOutParam());
                RTPrintf("Access error details:\n");
                ErrorInfo ei(accessError);
                GluePrintErrorInfo(ei);
                RTPrintf("\n");
            }
        }
        return S_OK;
    }

    Bstr machineName;
    rc = machine->COMGETTER(Name)(machineName.asOutParam());

    if (details == VMINFO_COMPACT)
    {
        RTPrintf("\"%lS\" {%s}\n", machineName.raw(), Utf8Str(uuid).raw());
        return S_OK;
    }

    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("name=\"%lS\"\n", machineName.raw());
    else
        RTPrintf("Name:            %lS\n", machineName.raw());

    Bstr osTypeId;
    rc = machine->COMGETTER(OSTypeId)(osTypeId.asOutParam());
    ComPtr<IGuestOSType> osType;
    rc = virtualBox->GetGuestOSType (osTypeId, osType.asOutParam());
    Bstr osName;
    rc = osType->COMGETTER(Description)(osName.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("ostype=\"%lS\"\n", osTypeId.raw());
    else
        RTPrintf("Guest OS:        %lS\n", osName.raw());

    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("UUID=\"%s\"\n", Utf8Str(uuid).raw());
    else
        RTPrintf("UUID:            %s\n", Utf8Str(uuid).raw());

    Bstr settingsFilePath;
    rc = machine->COMGETTER(SettingsFilePath)(settingsFilePath.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("CfgFile=\"%lS\"\n", settingsFilePath.raw());
    else
        RTPrintf("Config file:     %lS\n", settingsFilePath.raw());

    Bstr strHardwareUuid;
    rc = machine->COMGETTER(HardwareUUID)(strHardwareUuid.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("HardwareUUID=\"%s\"\n", strHardwareUuid.raw());
    else
        RTPrintf("Hardware UUID:   %lS\n", strHardwareUuid.raw());

    ULONG memorySize;
    rc = machine->COMGETTER(MemorySize)(&memorySize);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("memory=%u\n", memorySize);
    else
        RTPrintf("Memory size:     %uMB\n", memorySize);

    ULONG vramSize;
    rc = machine->COMGETTER(VRAMSize)(&vramSize);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("vram=%u\n", vramSize);
    else
        RTPrintf("VRAM size:       %uMB\n", vramSize);

    ULONG numCpus;
    rc = machine->COMGETTER(CPUCount)(&numCpus);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("cpus=%u\n", numCpus);
    else
        RTPrintf("Number of CPUs:  %u\n", numCpus);

    BOOL fSyntheticCpu;
    machine->GetCpuProperty(CpuPropertyType_Synthetic, &fSyntheticCpu);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("synthcpu=\"%s\"\n", fSyntheticCpu ? "on" : "off");
    else
        RTPrintf("Synthetic Cpu:   %s\n", fSyntheticCpu ? "on" : "off");

    ComPtr <IBIOSSettings> biosSettings;
    machine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());

    BIOSBootMenuMode_T bootMenuMode;
    biosSettings->COMGETTER(BootMenuMode)(&bootMenuMode);
    const char *pszBootMenu = NULL;
    switch (bootMenuMode)
    {
        case BIOSBootMenuMode_Disabled:
            pszBootMenu = "disabled";
            break;
        case BIOSBootMenuMode_MenuOnly:
            if (details == VMINFO_MACHINEREADABLE)
                pszBootMenu = "menuonly";
            else
                pszBootMenu = "menu only";
            break;
        default:
            if (details == VMINFO_MACHINEREADABLE)
                pszBootMenu = "messageandmenu";
            else
                pszBootMenu = "message and menu";
    }
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("bootmenu=\"%s\"\n", pszBootMenu);
    else
        RTPrintf("Boot menu mode:  %s\n", pszBootMenu);

    ULONG maxBootPosition = 0;
    ComPtr<ISystemProperties> systemProperties;
    virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    systemProperties->COMGETTER(MaxBootPosition)(&maxBootPosition);
    for (ULONG i = 1; i <= maxBootPosition; i++)
    {
        DeviceType_T bootOrder;
        machine->GetBootOrder(i, &bootOrder);
        if (bootOrder == DeviceType_Floppy)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"floppy\"\n", i);
            else
                RTPrintf("Boot Device (%d): Floppy\n", i);
        }
        else if (bootOrder == DeviceType_DVD)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"dvd\"\n", i);
            else
                RTPrintf("Boot Device (%d): DVD\n", i);
        }
        else if (bootOrder == DeviceType_HardDisk)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"disk\"\n", i);
            else
                RTPrintf("Boot Device (%d): HardDisk\n", i);
        }
        else if (bootOrder == DeviceType_Network)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"net\"\n", i);
            else
                RTPrintf("Boot Device (%d): Network\n", i);
        }
        else if (bootOrder == DeviceType_USB)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"usb\"\n", i);
            else
                RTPrintf("Boot Device (%d): USB\n", i);
        }
        else if (bootOrder == DeviceType_SharedFolder)
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"sharedfolder\"\n", i);
            else
                RTPrintf("Boot Device (%d): Shared Folder\n", i);
        }
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("boot%d=\"none\"\n", i);
            else
                RTPrintf("Boot Device (%d): Not Assigned\n", i);
        }
    }

    BOOL acpiEnabled;
    biosSettings->COMGETTER(ACPIEnabled)(&acpiEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("acpi=\"%s\"\n", acpiEnabled ? "on" : "off");
    else
        RTPrintf("ACPI:            %s\n", acpiEnabled ? "on" : "off");

    BOOL ioapicEnabled;
    biosSettings->COMGETTER(IOAPICEnabled)(&ioapicEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("ioapic=\"%s\"\n", ioapicEnabled ? "on" : "off");
    else
        RTPrintf("IOAPIC:          %s\n", ioapicEnabled ? "on" : "off");

    BOOL PAEEnabled;
    machine->GetCpuProperty(CpuPropertyType_PAE, &PAEEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("pae=\"%s\"\n", PAEEnabled ? "on" : "off");
    else
        RTPrintf("PAE:             %s\n", PAEEnabled ? "on" : "off");

    LONG64 timeOffset;
    biosSettings->COMGETTER(TimeOffset)(&timeOffset);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("biossystemtimeoffset=%lld\n", timeOffset);
    else
        RTPrintf("Time offset:     %lld ms\n", timeOffset);

    BOOL hwVirtExEnabled;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_Enabled, &hwVirtExEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hwvirtex=\"%s\"\n", hwVirtExEnabled ? "on" : "off");
    else
        RTPrintf("Hardw. virt.ext: %s\n", hwVirtExEnabled ? "on" : "off");

    BOOL hwVirtExExclusive;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_Exclusive, &hwVirtExExclusive);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hwvirtexexcl=\"%s\"\n", hwVirtExExclusive ? "on" : "off");
    else
        RTPrintf("Hardw. virt.ext exclusive: %s\n", hwVirtExExclusive ? "on" : "off");

    BOOL HWVirtExNestedPagingEnabled;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, &HWVirtExNestedPagingEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("nestedpaging=\"%s\"\n", HWVirtExNestedPagingEnabled ? "on" : "off");
    else
        RTPrintf("Nested Paging:   %s\n", HWVirtExNestedPagingEnabled ? "on" : "off");

    BOOL HWVirtExVPIDEnabled;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_VPID, &HWVirtExVPIDEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("vtxvpid=\"%s\"\n", HWVirtExVPIDEnabled ? "on" : "off");
    else
        RTPrintf("VT-x VPID:       %s\n", HWVirtExVPIDEnabled ? "on" : "off");

    MachineState_T machineState;
    const char *pszState = NULL;
    rc = machine->COMGETTER(State)(&machineState);
    switch (machineState)
    {
        case MachineState_PoweredOff:
            pszState = details == VMINFO_MACHINEREADABLE ? "poweroff"            : "powered off";
            break;
        case MachineState_Saved:
            pszState = "saved";
            break;
        case MachineState_Aborted:
            pszState = "aborted";
            break;
        case MachineState_Teleported:
            pszState = "teleported";
            break;
        case MachineState_Running:
            pszState = "running";
            break;
        case MachineState_Paused:
            pszState = "paused";
            break;
        case MachineState_Stuck:
            pszState = details == VMINFO_MACHINEREADABLE ? "gurumeditation"      : "guru meditation";
            break;
        case MachineState_LiveSnapshotting:
            pszState = details == VMINFO_MACHINEREADABLE ? "livesnapshotting"    : "live snapshotting";
            break;
        case MachineState_Teleporting:
            pszState = "teleporting";
            break;
        case MachineState_Starting:
            pszState = "starting";
            break;
        case MachineState_Stopping:
            pszState = "stopping";
            break;
        case MachineState_Saving:
            pszState = "saving";
            break;
        case MachineState_Restoring:
            pszState = "restoring";
            break;
        case MachineState_TeleportingPausedVM:
            pszState = details == VMINFO_MACHINEREADABLE ? "teleportingpausedvm" : "teleporting paused vm";
            break;
        case MachineState_TeleportingIn:
            pszState = details == VMINFO_MACHINEREADABLE ? "teleportingin"       : "teleporting (incoming)";
            break;
        case MachineState_RestoringSnapshot:
            pszState = details == VMINFO_MACHINEREADABLE ? "restoringsnapshot"   : "restoring snapshot";
        case MachineState_DeletingSnapshot:
            pszState = details == VMINFO_MACHINEREADABLE ? "deletingsnapshot"    : "deleting snapshot";
        case MachineState_SettingUp:
            pszState = details == VMINFO_MACHINEREADABLE ? "settingup"           : "setting up";
            break;
        default:
            pszState = "unknown";
            break;
    }
    LONG64 stateSince;
    machine->COMGETTER(LastStateChange)(&stateSince);
    RTTIMESPEC timeSpec;
    RTTimeSpecSetMilli(&timeSpec, stateSince);
    char pszTime[30] = {0};
    RTTimeSpecToString(&timeSpec, pszTime, sizeof(pszTime));
    Bstr stateFile;
    machine->COMGETTER(StateFilePath)(stateFile.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
    {
        RTPrintf("VMState=\"%s\"\n", pszState);
        RTPrintf("VMStateChangeTime=\"%s\"\n", pszTime);
        if (!stateFile.isEmpty())
            RTPrintf("VMStateFile=\"%lS\"\n", stateFile.raw());
    }
    else
        RTPrintf("State:           %s (since %s)\n", pszState, pszTime);

    ULONG numMonitors;
    machine->COMGETTER(MonitorCount)(&numMonitors);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("monitorcount=%d\n", numMonitors);
    else
        RTPrintf("Monitor count:   %d\n", numMonitors);

    BOOL accelerate3d;
    machine->COMGETTER(Accelerate3DEnabled)(&accelerate3d);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("accelerate3d=\"%s\"\n", accelerate3d ? "on" : "off");
    else
        RTPrintf("3D Acceleration: %s\n", accelerate3d ? "on" : "off");

#ifdef VBOX_WITH_VIDEOHWACCEL
    BOOL accelerate2dVideo;
    machine->COMGETTER(Accelerate2DVideoEnabled)(&accelerate2dVideo);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("accelerate2dvideo=\"%s\"\n", accelerate2dVideo ? "on" : "off");
    else
        RTPrintf("2D Video Acceleration: %s\n", accelerate2dVideo ? "on" : "off");
#endif

    BOOL teleporterEnabled;
    machine->COMGETTER(TeleporterEnabled)(&teleporterEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("teleporterenabled=\"%s\"\n", teleporterEnabled ? "on" : "off");
    else
        RTPrintf("Teleporter Enabled: %s\n", teleporterEnabled ? "on" : "off");

    ULONG teleporterPort;
    machine->COMGETTER(TeleporterPort)(&teleporterPort);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("teleporterport=%u\n", teleporterPort);
    else
        RTPrintf("Teleporter Port: %u\n", teleporterPort);

    Bstr teleporterAddress;
    machine->COMGETTER(TeleporterAddress)(teleporterAddress.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("teleporteraddress=\"%lS\"\n", teleporterAddress.raw());
    else
        RTPrintf("Teleporter Address: %lS\n", teleporterAddress.raw());

    Bstr teleporterPassword;
    machine->COMGETTER(TeleporterPassword)(teleporterPassword.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("teleporterpassword=\"%lS\"\n", teleporterPassword.raw());
    else
        RTPrintf("Teleporter Password: %lS\n", teleporterPassword.raw());

    /*
     * Storage Controllers and their attached Mediums.
     */
    com::SafeIfaceArray<IStorageController> storageCtls;
    CHECK_ERROR(machine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam (storageCtls)));
    for (size_t i = 0; i < storageCtls.size(); ++ i)
    {
        ComPtr<IStorageController> storageCtl = storageCtls[i];
        StorageControllerType_T    enmCtlType = StorageControllerType_Null;
        const char *pszCtl = NULL;
        Bstr storageCtlName;

        storageCtl->COMGETTER(Name)(storageCtlName.asOutParam());

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontroller%u:\"%lS\"\n", i, storageCtlName.raw());
        else
            RTPrintf("Storage Controller      (%u): %lS\n", i, storageCtlName.raw());

        storageCtl->COMGETTER(ControllerType)(&enmCtlType);

        switch (enmCtlType)
        {
            case StorageControllerType_LsiLogic:
                pszCtl = "LsiLogic";
                break;
            case StorageControllerType_BusLogic:
                pszCtl = "BusLogic";
                break;
            case StorageControllerType_IntelAhci:
                pszCtl = "IntelAhci";
                break;
            case StorageControllerType_PIIX3:
                pszCtl = "PIIX3";
                break;
            case StorageControllerType_PIIX4:
                pszCtl = "PIIX4";
                break;
            case StorageControllerType_ICH6:
                pszCtl = "ICH6";
                break;
            case StorageControllerType_I82078:
                pszCtl = "I82078";
                break;

            default:
                pszCtl = "unknown";
        }
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollertype%u:=\"%s\"\n", i, pszCtl);
        else
            RTPrintf("Storage Controller Type (%u): %s\n", i, pszCtl);
    }

    for (size_t j = 0; j < storageCtls.size(); ++ j)
    {
        ComPtr<IStorageController> storageCtl = storageCtls[j];
        ComPtr<IMedium> medium;
        Bstr storageCtlName;
        Bstr filePath;
        ULONG cDevices;
        ULONG cPorts;

        storageCtl->COMGETTER(Name)(storageCtlName.asOutParam());
        storageCtl->COMGETTER(MaxDevicesPerPortCount)(&cDevices);
        storageCtl->COMGETTER(PortCount)(&cPorts);

        for (ULONG i = 0; i < cPorts; ++ i)
        {
            for (ULONG k = 0; k < cDevices; ++ k)
            {
                rc = machine->GetMedium(storageCtlName, i, k, medium.asOutParam());
                if (SUCCEEDED(rc) && medium)
                {
                    BOOL fPassthrough;
                    ComPtr<IMediumAttachment> mediumAttach;

                    rc = machine->GetMediumAttachment(storageCtlName, i, k, mediumAttach.asOutParam());
                    if (SUCCEEDED(rc) && mediumAttach)
                        mediumAttach->COMGETTER(Passthrough)(&fPassthrough);

                    medium->COMGETTER(Location)(filePath.asOutParam());
                    medium->COMGETTER(Id)(uuid.asOutParam());

                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        RTPrintf("\"%lS\"-%d-%d=\"%lS\"\n", storageCtlName.raw(),
                                 i, k, filePath.raw());
                        RTPrintf("\"%lS\"-ImageUUID-%d-%d=\"%s\"\n",
                                 storageCtlName.raw(), i, k, Utf8Str(uuid).raw());
                        if (fPassthrough)
                            RTPrintf("\"%lS\"-dvdpassthrough=\"%s\"\n", storageCtlName.raw(),
                                     fPassthrough ? "on" : "off");
                    }
                    else
                    {
                        RTPrintf("%lS (%d, %d): %lS (UUID: %s)",
                                 storageCtlName.raw(), i, k, filePath.raw(),
                                 Utf8Str(uuid).raw());
                        if (fPassthrough)
                            RTPrintf(" (passthrough enabled)");
                        RTPrintf("\n");
                    }
                }
                else
                {
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("\"%lS\"-%d-%d=\"none\"\n", storageCtlName.raw(), i, k);
                }
            }
        }
    }

    /* get the maximum amount of NICS */
    ComPtr<ISystemProperties> sysProps;
    virtualBox->COMGETTER(SystemProperties)(sysProps.asOutParam());
    ULONG maxNICs = 0;
    sysProps->COMGETTER(NetworkAdapterCount)(&maxNICs);
    for (ULONG currentNIC = 0; currentNIC < maxNICs; currentNIC++)
    {
        ComPtr<INetworkAdapter> nic;
        rc = machine->GetNetworkAdapter(currentNIC, nic.asOutParam());
        if (SUCCEEDED(rc) && nic)
        {
            BOOL fEnabled;
            nic->COMGETTER(Enabled)(&fEnabled);
            if (!fEnabled)
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("nic%d=\"none\"\n", currentNIC + 1);
                else
                    RTPrintf("NIC %d:           disabled\n", currentNIC + 1);
            }
            else
            {
                Bstr strMACAddress;
                nic->COMGETTER(MACAddress)(strMACAddress.asOutParam());
                Utf8Str strAttachment;
                NetworkAttachmentType_T attachment;
                nic->COMGETTER(AttachmentType)(&attachment);
                switch (attachment)
                {
                    case NetworkAttachmentType_Null:
                        if (details == VMINFO_MACHINEREADABLE)
                            strAttachment = "null";
                        else
                            strAttachment = "none";
                        break;
                    case NetworkAttachmentType_NAT:
                    {
                        Bstr strNetwork;
                        nic->COMGETTER(NATNetwork)(strNetwork.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("natnet%d=\"%lS\"\n", currentNIC + 1, strNetwork.raw());
                            strAttachment = "nat";
                        }
                        else if (!strNetwork.isEmpty())
                            strAttachment = Utf8StrFmt("NAT (%lS)", strNetwork.raw());
                        else
                            strAttachment = "NAT";
                        break;
                    }
                    case NetworkAttachmentType_Bridged:
                    {
                        Bstr strBridgeAdp;
                        nic->COMGETTER(HostInterface)(strBridgeAdp.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("bridgeadapter%d=\"%lS\"\n", currentNIC + 1, strBridgeAdp.raw());
                            strAttachment = "bridged";
                        }
                        else
                            strAttachment = Utf8StrFmt("Bridged Interface '%lS'", strBridgeAdp.raw());
                        break;
                    }
                    case NetworkAttachmentType_Internal:
                    {
                        Bstr strNetwork;
                        nic->COMGETTER(InternalNetwork)(strNetwork.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("intnet%d=\"%lS\"\n", currentNIC + 1, strNetwork.raw());
                            strAttachment = "intnet";
                        }
                        else
                            strAttachment = Utf8StrFmt("Internal Network '%s'", Utf8Str(strNetwork).raw());
                        break;
                    }
#if defined(VBOX_WITH_NETFLT)
                    case NetworkAttachmentType_HostOnly:
                    {
                        Bstr strHostonlyAdp;
                        nic->COMGETTER(HostInterface)(strHostonlyAdp.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("hostonlyadapter%d=\"%lS\"\n", currentNIC + 1, strHostonlyAdp.raw());
                            strAttachment = "hostonly";
                        }
                        else
                            strAttachment = Utf8StrFmt("Host-only Interface '%lS'", strHostonlyAdp.raw());
                        break;
                    }
#endif
                    default:
                        strAttachment = "unknown";
                        break;
                }

                /* cable connected */
                BOOL fConnected;
                nic->COMGETTER(CableConnected)(&fConnected);

                /* trace stuff */
                BOOL fTraceEnabled;
                nic->COMGETTER(TraceEnabled)(&fTraceEnabled);
                Bstr traceFile;
                nic->COMGETTER(TraceFile)(traceFile.asOutParam());

                /* NIC type */
                Utf8Str strNICType;
                NetworkAdapterType_T NICType;
                nic->COMGETTER(AdapterType)(&NICType);
                switch (NICType) {
                case NetworkAdapterType_Am79C970A:
                    strNICType = "Am79C970A";
                    break;
                case NetworkAdapterType_Am79C973:
                    strNICType = "Am79C973";
                    break;
#ifdef VBOX_WITH_E1000
                case NetworkAdapterType_I82540EM:
                    strNICType = "82540EM";
                    break;
                case NetworkAdapterType_I82543GC:
                    strNICType = "82543GC";
                    break;
                case NetworkAdapterType_I82545EM:
                    strNICType = "82545EM";
                    break;
#endif
#ifdef VBOX_WITH_VIRTIO
                case NetworkAdapterType_Virtio:
                    strNICType = "virtio";
                    break;
#endif /* VBOX_WITH_VIRTIO */
                default:
                    strNICType = "unknown";
                    break;
                }

                /* reported line speed */
                ULONG ulLineSpeed;
                nic->COMGETTER(LineSpeed)(&ulLineSpeed);

                if (details == VMINFO_MACHINEREADABLE)
                {
                    RTPrintf("macaddress%d=\"%lS\"\n", currentNIC + 1, strMACAddress.raw());
                    RTPrintf("cableconnected%d=\"%s\"\n", currentNIC + 1, fConnected ? "on" : "off");
                    RTPrintf("nic%d=\"%s\"\n", currentNIC + 1, strAttachment.raw());
                }
                else
                    RTPrintf("NIC %d:           MAC: %lS, Attachment: %s, Cable connected: %s, Trace: %s (file: %lS), Type: %s, Reported speed: %d Mbps\n",
                             currentNIC + 1, strMACAddress.raw(), strAttachment.raw(),
                             fConnected ? "on" : "off",
                             fTraceEnabled ? "on" : "off",
                             traceFile.isEmpty() ? Bstr("none").raw() : traceFile.raw(),
                             strNICType.raw(),
                             ulLineSpeed / 1000);
            }
        }
    }

    /* get the maximum amount of UARTs */
    ULONG maxUARTs = 0;
    sysProps->COMGETTER(SerialPortCount)(&maxUARTs);
    for (ULONG currentUART = 0; currentUART < maxUARTs; currentUART++)
    {
        ComPtr<ISerialPort> uart;
        rc = machine->GetSerialPort(currentUART, uart.asOutParam());
        if (SUCCEEDED(rc) && uart)
        {
            BOOL fEnabled;
            uart->COMGETTER(Enabled)(&fEnabled);
            if (!fEnabled)
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("uart%d=\"off\"\n", currentUART + 1);
                else
                    RTPrintf("UART %d:          disabled\n", currentUART + 1);
            }
            else
            {
                ULONG ulIRQ, ulIOBase;
                PortMode_T HostMode;
                Bstr path;
                BOOL fServer;
                uart->COMGETTER(IRQ)(&ulIRQ);
                uart->COMGETTER(IOBase)(&ulIOBase);
                uart->COMGETTER(Path)(path.asOutParam());
                uart->COMGETTER(Server)(&fServer);
                uart->COMGETTER(HostMode)(&HostMode);

                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("uart%d=\"%#06x,%d\"\n", currentUART + 1,
                             ulIOBase, ulIRQ);
                else
                    RTPrintf("UART %d:          I/O base: 0x%04x, IRQ: %d",
                             currentUART + 1, ulIOBase, ulIRQ);
                switch (HostMode)
                {
                    default:
                    case PortMode_Disconnected:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"disconnected\"\n", currentUART + 1);
                        else
                            RTPrintf(", disconnected\n");
                        break;
                    case PortMode_RawFile:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%lS\"\n", currentUART + 1,
                                     path.raw());
                        else
                            RTPrintf(", attached to raw file '%lS'\n",
                                     path.raw());
                        break;
                    case PortMode_HostPipe:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%s,%lS\"\n", currentUART + 1,
                                     fServer ? "server" : "client", path.raw());
                        else
                            RTPrintf(", attached to pipe (%s) '%lS'\n",
                                     fServer ? "server" : "client", path.raw());
                        break;
                    case PortMode_HostDevice:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%lS\"\n", currentUART + 1,
                                     path.raw());
                        else
                            RTPrintf(", attached to device '%lS'\n", path.raw());
                        break;
                }
            }
        }
    }

    ComPtr<IAudioAdapter> AudioAdapter;
    rc = machine->COMGETTER(AudioAdapter)(AudioAdapter.asOutParam());
    if (SUCCEEDED(rc))
    {
        const char *pszDrv  = "Unknown";
        const char *pszCtrl = "Unknown";
        BOOL fEnabled;
        rc = AudioAdapter->COMGETTER(Enabled)(&fEnabled);
        if (SUCCEEDED(rc) && fEnabled)
        {
            AudioDriverType_T enmDrvType;
            rc = AudioAdapter->COMGETTER(AudioDriver)(&enmDrvType);
            switch (enmDrvType)
            {
                case AudioDriverType_Null:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "null";
                    else
                        pszDrv = "Null";
                    break;
                case AudioDriverType_WinMM:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "winmm";
                    else
                        pszDrv = "WINMM";
                    break;
                case AudioDriverType_DirectSound:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "dsound";
                    else
                        pszDrv = "DSOUND";
                    break;
                case AudioDriverType_OSS:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "oss";
                    else
                        pszDrv = "OSS";
                    break;
                case AudioDriverType_ALSA:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "alsa";
                    else
                        pszDrv = "ALSA";
                    break;
                case AudioDriverType_Pulse:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "pulse";
                    else
                        pszDrv = "PulseAudio";
                    break;
                case AudioDriverType_CoreAudio:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "coreaudio";
                    else
                        pszDrv = "CoreAudio";
                    break;
                case AudioDriverType_SolAudio:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "solaudio";
                    else
                        pszDrv = "SolAudio";
                    break;
                default:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "unknown";
                    break;
            }
            AudioControllerType_T enmCtrlType;
            rc = AudioAdapter->COMGETTER(AudioController)(&enmCtrlType);
            switch (enmCtrlType)
            {
                case AudioControllerType_AC97:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "ac97";
                    else
                        pszCtrl = "AC97";
                    break;
                case AudioControllerType_SB16:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "sb16";
                    else
                        pszCtrl = "SB16";
                    break;
            }
        }
        else
            fEnabled = FALSE;
        if (details == VMINFO_MACHINEREADABLE)
        {
            if (fEnabled)
                RTPrintf("audio=\"%s\"\n", pszDrv);
            else
                RTPrintf("audio=\"none\"\n");
        }
        else
        {
            RTPrintf("Audio:           %s",
                    fEnabled ? "enabled" : "disabled");
            if (fEnabled)
                RTPrintf(" (Driver: %s, Controller: %s)",
                    pszDrv, pszCtrl);
            RTPrintf("\n");
        }
    }

    /* Shared clipboard */
    {
        const char *psz = "Unknown";
        ClipboardMode_T enmMode;
        rc = machine->COMGETTER(ClipboardMode)(&enmMode);
        switch (enmMode)
        {
            case ClipboardMode_Disabled:
                if (details == VMINFO_MACHINEREADABLE)
                    psz = "disabled";
                else
                    psz = "disabled";
                break;
            case ClipboardMode_HostToGuest:
                if (details == VMINFO_MACHINEREADABLE)
                    psz = "hosttoguest";
                else
                    psz = "HostToGuest";
                break;
            case ClipboardMode_GuestToHost:
                if (details == VMINFO_MACHINEREADABLE)
                    psz = "guesttohost";
                else
                    psz = "GuestToHost";
                break;
            case ClipboardMode_Bidirectional:
                if (details == VMINFO_MACHINEREADABLE)
                    psz = "bidirectional";
                else
                    psz = "Bidirectional";
                break;
            default:
                if (details == VMINFO_MACHINEREADABLE)
                    psz = "unknown";
                break;
        }
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("clipboard=\"%s\"\n", psz);
        else
            RTPrintf("Clipboard Mode:  %s\n", psz);
    }

    if (console)
    {
        ComPtr<IDisplay> display;
        CHECK_ERROR_RET(console, COMGETTER(Display)(display.asOutParam()), rc);
        do
        {
            ULONG xRes, yRes, bpp;
            rc = display->COMGETTER(Width)(&xRes);
            if (rc == E_ACCESSDENIED)
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::ErrorInfo info (display);
                GluePrintErrorInfo(info);
                return rc;
            }
            rc = display->COMGETTER(Height)(&yRes);
            if (rc == E_ACCESSDENIED)
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::ErrorInfo info (display);
                GluePrintErrorInfo(info);
                return rc;
            }
            rc = display->COMGETTER(BitsPerPixel)(&bpp);
            if (rc == E_ACCESSDENIED)
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::ErrorInfo info (display);
                GluePrintErrorInfo(info);
                return rc;
            }
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("VideoMode=\"%d,%d,%d\"\n", xRes, yRes, bpp);
            else
                RTPrintf("Video mode:      %dx%dx%d\n", xRes, yRes, bpp);
        }
        while (0);
    }

    /*
     * VRDP
     */
    ComPtr<IVRDPServer> vrdpServer;
    rc = machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
    if (SUCCEEDED(rc) && vrdpServer)
    {
        BOOL fEnabled = false;
        vrdpServer->COMGETTER(Enabled)(&fEnabled);
        if (fEnabled)
        {
            LONG vrdpPort = -1;
            Bstr ports;
            vrdpServer->COMGETTER(Ports)(ports.asOutParam());
            Bstr address;
            vrdpServer->COMGETTER(NetAddress)(address.asOutParam());
            BOOL fMultiCon;
            vrdpServer->COMGETTER(AllowMultiConnection)(&fMultiCon);
            BOOL fReuseCon;
            vrdpServer->COMGETTER(ReuseSingleConnection)(&fReuseCon);
            VRDPAuthType_T vrdpAuthType;
            const char *strAuthType;
            vrdpServer->COMGETTER(AuthType)(&vrdpAuthType);
            switch (vrdpAuthType)
            {
                case VRDPAuthType_Null:
                    strAuthType = "null";
                    break;
                case VRDPAuthType_External:
                    strAuthType = "external";
                    break;
                case VRDPAuthType_Guest:
                    strAuthType = "guest";
                    break;
                default:
                    strAuthType = "unknown";
                    break;
            }
            if (console)
            {
                ComPtr<IRemoteDisplayInfo> remoteDisplayInfo;
                CHECK_ERROR_RET(console, COMGETTER(RemoteDisplayInfo)(remoteDisplayInfo.asOutParam()), rc);
                rc = remoteDisplayInfo->COMGETTER(Port)(&vrdpPort);
                if (rc == E_ACCESSDENIED)
                {
                    vrdpPort = -1; /* VM not powered up */
                }
                if (FAILED(rc))
                {
                    com::ErrorInfo info (remoteDisplayInfo);
                    GluePrintErrorInfo(info);
                    return rc;
                }
            }
            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("vrdp=\"on\"\n");
                RTPrintf("vrdpport=%d\n", vrdpPort);
                RTPrintf("vrdpports=\"%lS\"\n", ports.raw());
                RTPrintf("vrdpaddress=\"%lS\"\n", address.raw());
                RTPrintf("vrdpauthtype=\"%s\"\n", strAuthType);
                RTPrintf("vrdpmulticon=\"%s\"\n", fMultiCon ? "on" : "off");
                RTPrintf("vrdpreusecon=\"%s\"\n", fReuseCon ? "on" : "off");
            }
            else
            {
                if (address.isEmpty())
                    address = "0.0.0.0";
                RTPrintf("VRDP:            enabled (Address %lS, Ports %lS, MultiConn: %s, ReuseSingleConn: %s, Authentication type: %s)\n", address.raw(), ports.raw(), fMultiCon ? "on" : "off", fReuseCon ? "on" : "off", strAuthType);
                if (console && vrdpPort != -1 && vrdpPort != 0)
                   RTPrintf("VRDP port:       %d\n", vrdpPort);
            }
        }
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("vrdp=\"off\"\n");
            else
                RTPrintf("VRDP:            disabled\n");
        }
    }

    /*
     * USB.
     */
    ComPtr<IUSBController> USBCtl;
    rc = machine->COMGETTER(USBController)(USBCtl.asOutParam());
    if (SUCCEEDED(rc))
    {
        BOOL fEnabled;
        rc = USBCtl->COMGETTER(Enabled)(&fEnabled);
        if (FAILED(rc))
            fEnabled = false;
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("usb=\"%s\"\n", fEnabled ? "on" : "off");
        else
            RTPrintf("USB:             %s\n", fEnabled ? "enabled" : "disabled");

        if (details != VMINFO_MACHINEREADABLE)
            RTPrintf("\nUSB Device Filters:\n\n");

        SafeIfaceArray <IUSBDeviceFilter> Coll;
        CHECK_ERROR_RET (USBCtl, COMGETTER(DeviceFilters)(ComSafeArrayAsOutParam(Coll)), rc);

        if (Coll.size() == 0)
        {
            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("<none>\n\n");
        }
        else
        {
            for (size_t index = 0; index < Coll.size(); ++index)
            {
                ComPtr<IUSBDeviceFilter> DevPtr = Coll[index];

                /* Query info. */

                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf("Index:            %zu\n", index);

                BOOL bActive = FALSE;
                CHECK_ERROR_RET (DevPtr, COMGETTER (Active) (&bActive), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterActive%zu=\"%s\"\n", index + 1, bActive ? "on" : "off");
                else
                    RTPrintf("Active:           %s\n", bActive ? "yes" : "no");

                Bstr bstr;
                CHECK_ERROR_RET (DevPtr, COMGETTER (Name) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterName%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Name:             %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (VendorId) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterVendorId%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("VendorId:         %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (ProductId) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterProductId%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("ProductId:        %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (Revision) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterRevision%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Revision:         %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (Manufacturer) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Manufacturer:     %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (Product) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Product:          %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (Remote) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterRemote%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Remote:           %lS\n", bstr.raw());
                CHECK_ERROR_RET (DevPtr, COMGETTER (SerialNumber) (bstr.asOutParam()), rc);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("USBFilterSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                else
                    RTPrintf("Serial Number:    %lS\n", bstr.raw());
                if (details != VMINFO_MACHINEREADABLE)
                {
                    ULONG fMaskedIfs;
                    CHECK_ERROR_RET (DevPtr, COMGETTER (MaskedInterfaces) (&fMaskedIfs), rc);
                    if (fMaskedIfs)
                        RTPrintf("Masked Interfaces: 0x%08x\n", fMaskedIfs);
                    RTPrintf("\n");
                }
            }
        }

        if (console)
        {
            /* scope */
            {
                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf("Available remote USB devices:\n\n");

                SafeIfaceArray <IHostUSBDevice> coll;
                CHECK_ERROR_RET (console, COMGETTER(RemoteUSBDevices) (ComSafeArrayAsOutParam(coll)), rc);

                if (coll.size() == 0)
                {
                    if (details != VMINFO_MACHINEREADABLE)
                        RTPrintf("<none>\n\n");
                }
                else
                {
                    for (size_t index = 0; index < coll.size(); ++index)
                    {
                        ComPtr <IHostUSBDevice> dev = coll[index];

                        /* Query info. */
                        Bstr id;
                        CHECK_ERROR_RET (dev, COMGETTER(Id)(id.asOutParam()), rc);
                        USHORT usVendorId;
                        CHECK_ERROR_RET (dev, COMGETTER(VendorId)(&usVendorId), rc);
                        USHORT usProductId;
                        CHECK_ERROR_RET (dev, COMGETTER(ProductId)(&usProductId), rc);
                        USHORT bcdRevision;
                        CHECK_ERROR_RET (dev, COMGETTER(Revision)(&bcdRevision), rc);

                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("USBRemoteUUID%zu=\"%S\"\n"
                                     "USBRemoteVendorId%zu=\"%#06x\"\n"
                                     "USBRemoteProductId%zu=\"%#06x\"\n"
                                     "USBRemoteRevision%zu=\"%#04x%02x\"\n",
                                     index + 1, Utf8Str(id).raw(),
                                     index + 1, usVendorId,
                                     index + 1, usProductId,
                                     index + 1, bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTPrintf("UUID:               %S\n"
                                     "VendorId:           0x%04x (%04X)\n"
                                     "ProductId:          0x%04x (%04X)\n"
                                     "Revision:           %u.%u (%02u%02u)\n",
                                     Utf8Str(id).raw(),
                                     usVendorId, usVendorId, usProductId, usProductId,
                                     bcdRevision >> 8, bcdRevision & 0xff,
                                     bcdRevision >> 8, bcdRevision & 0xff);

                        /* optional stuff. */
                        Bstr bstr;
                        CHECK_ERROR_RET (dev, COMGETTER(Manufacturer)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Manufacturer:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(Product)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Product:            %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(SerialNumber)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("SerialNumber:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(Address)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteAddress%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Address:            %lS\n", bstr.raw());
                        }

                        if (details != VMINFO_MACHINEREADABLE)
                            RTPrintf("\n");
                    }
                }
            }

            /* scope */
            {
                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf ("Currently Attached USB Devices:\n\n");

                SafeIfaceArray <IUSBDevice> coll;
                CHECK_ERROR_RET (console, COMGETTER(USBDevices) (ComSafeArrayAsOutParam(coll)), rc);

                if (coll.size() == 0)
                {
                    if (details != VMINFO_MACHINEREADABLE)
                        RTPrintf("<none>\n\n");
                }
                else
                {
                    for (size_t index = 0; index < coll.size(); ++index)
                    {
                        ComPtr <IUSBDevice> dev = coll[index];

                        /* Query info. */
                        Bstr id;
                        CHECK_ERROR_RET (dev, COMGETTER(Id)(id.asOutParam()), rc);
                        USHORT usVendorId;
                        CHECK_ERROR_RET (dev, COMGETTER(VendorId)(&usVendorId), rc);
                        USHORT usProductId;
                        CHECK_ERROR_RET (dev, COMGETTER(ProductId)(&usProductId), rc);
                        USHORT bcdRevision;
                        CHECK_ERROR_RET (dev, COMGETTER(Revision)(&bcdRevision), rc);

                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("USBAttachedUUID%zu=\"%S\"\n"
                                     "USBAttachedVendorId%zu=\"%#06x\"\n"
                                     "USBAttachedProductId%zu=\"%#06x\"\n"
                                     "USBAttachedRevision%zu=\"%#04x%02x\"\n",
                                     index + 1, Utf8Str(id).raw(),
                                     index + 1, usVendorId,
                                     index + 1, usProductId,
                                     index + 1, bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTPrintf("UUID:               %S\n"
                                     "VendorId:           0x%04x (%04X)\n"
                                     "ProductId:          0x%04x (%04X)\n"
                                     "Revision:           %u.%u (%02u%02u)\n",
                                     Utf8Str(id).raw(),
                                     usVendorId, usVendorId, usProductId, usProductId,
                                     bcdRevision >> 8, bcdRevision & 0xff,
                                     bcdRevision >> 8, bcdRevision & 0xff);

                        /* optional stuff. */
                        Bstr bstr;
                        CHECK_ERROR_RET (dev, COMGETTER(Manufacturer)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Manufacturer:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(Product)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Product:            %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(SerialNumber)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("SerialNumber:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET (dev, COMGETTER(Address)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedAddress%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Address:            %lS\n", bstr.raw());
                        }

                        if (details != VMINFO_MACHINEREADABLE)
                            RTPrintf("\n");
                    }
                }
            }
        }
    } /* USB */

    /*
     * Shared folders
     */
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Shared folders:  ");
    uint32_t numSharedFolders = 0;
#if 0 // not yet implemented
    /* globally shared folders first */
    {
        SafeIfaceArray <ISharedFolder> sfColl;
        CHECK_ERROR_RET(virtualBox, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(sfColl)), rc);
        for (size_t i = 0; i < sfColl.size(); ++i)
        {
            ComPtr<ISharedFolder> sf = sfColl[i];
            Bstr name, hostPath;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            RTPrintf("Name: '%lS', Host path: '%lS' (global mapping)\n", name.raw(), hostPath.raw());
            ++numSharedFolders;
        }
    }
#endif
    /* now VM mappings */
    {
        com::SafeIfaceArray <ISharedFolder> folders;

        CHECK_ERROR_RET(machine, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders)), rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr <ISharedFolder> sf = folders[i];

            Bstr name, hostPath;
            BOOL writable;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            sf->COMGETTER(Writable)(&writable);
            if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
                RTPrintf("\n\n");
            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("SharedFolderNameMachineMapping%zu=\"%lS\"\n", i + 1,
                         name.raw());
                RTPrintf("SharedFolderPathMachineMapping%zu=\"%lS\"\n", i + 1,
                         hostPath.raw());
            }
            else
                RTPrintf("Name: '%lS', Host path: '%lS' (machine mapping), %s\n",
                         name.raw(), hostPath.raw(), writable ? "writable" : "readonly");
            ++numSharedFolders;
        }
    }
    /* transient mappings */
    if (console)
    {
        com::SafeIfaceArray <ISharedFolder> folders;

        CHECK_ERROR_RET(console, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders)), rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr <ISharedFolder> sf = folders[i];

            Bstr name, hostPath;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
                RTPrintf("\n\n");
            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("SharedFolderNameTransientMapping%zu=\"%lS\"\n", i + 1,
                         name.raw());
                RTPrintf("SharedFolderPathTransientMapping%zu=\"%lS\"\n", i + 1,
                         hostPath.raw());
            }
            else
                RTPrintf("Name: '%lS', Host path: '%lS' (transient mapping)\n", name.raw(), hostPath.raw());
            ++numSharedFolders;
        }
    }
    if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
        RTPrintf("<none>\n");
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");

    if (console)
    {
        /*
         * Live VRDP info.
         */
        ComPtr<IRemoteDisplayInfo> remoteDisplayInfo;
        CHECK_ERROR_RET(console, COMGETTER(RemoteDisplayInfo)(remoteDisplayInfo.asOutParam()), rc);
        BOOL    Active;
        ULONG   NumberOfClients;
        LONG64  BeginTime;
        LONG64  EndTime;
        ULONG64 BytesSent;
        ULONG64 BytesSentTotal;
        ULONG64 BytesReceived;
        ULONG64 BytesReceivedTotal;
        Bstr    User;
        Bstr    Domain;
        Bstr    ClientName;
        Bstr    ClientIP;
        ULONG   ClientVersion;
        ULONG   EncryptionStyle;

        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(Active)             (&Active), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(NumberOfClients)    (&NumberOfClients), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(BeginTime)          (&BeginTime), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(EndTime)            (&EndTime), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(BytesSent)          (&BytesSent), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(BytesSentTotal)     (&BytesSentTotal), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(BytesReceived)      (&BytesReceived), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(BytesReceivedTotal) (&BytesReceivedTotal), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(User)               (User.asOutParam ()), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(Domain)             (Domain.asOutParam ()), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(ClientName)         (ClientName.asOutParam ()), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(ClientIP)           (ClientIP.asOutParam ()), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(ClientVersion)      (&ClientVersion), rc);
        CHECK_ERROR_RET(remoteDisplayInfo, COMGETTER(EncryptionStyle)    (&EncryptionStyle), rc);

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("VRDPActiveConnection=\"%s\"\n", Active ? "on": "off");
        else
            RTPrintf("VRDP Connection:    %s\n", Active? "active": "not active");

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("VRDPClients=%d\n", NumberOfClients);
        else
            RTPrintf("Clients so far:     %d\n", NumberOfClients);

        if (NumberOfClients > 0)
        {
            char timestr[128];

            if (Active)
            {
                makeTimeStr (timestr, sizeof (timestr), BeginTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDPStartTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Start time:         %s\n", timestr);
            }
            else
            {
                makeTimeStr (timestr, sizeof (timestr), BeginTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDPLastStartTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Last started:       %s\n", timestr);
                makeTimeStr (timestr, sizeof (timestr), EndTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDPLastEndTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Last ended:         %s\n", timestr);
            }

            uint64_t ThroughputSend = 0;
            uint64_t ThroughputReceive = 0;
            if (EndTime != BeginTime)
            {
                ThroughputSend = (BytesSent * 1000) / (EndTime - BeginTime);
                ThroughputReceive = (BytesReceived * 1000) / (EndTime - BeginTime);
            }

            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("VRDPBytesSent=%llu\n", BytesSent);
                RTPrintf("VRDPThroughputSend=%llu\n", ThroughputSend);
                RTPrintf("VRDPBytesSentTotal=%llu\n", BytesSentTotal);

                RTPrintf("VRDPBytesReceived=%llu\n", BytesReceived);
                RTPrintf("VRDPThroughputReceive=%llu\n", ThroughputReceive);
                RTPrintf("VRDPBytesReceivedTotal=%llu\n", BytesReceivedTotal);
            }
            else
            {
                RTPrintf("Sent:               %llu Bytes\n", BytesSent);
                RTPrintf("Average speed:      %llu B/s\n", ThroughputSend);
                RTPrintf("Sent total:         %llu Bytes\n", BytesSentTotal);

                RTPrintf("Received:           %llu Bytes\n", BytesReceived);
                RTPrintf("Speed:              %llu B/s\n", ThroughputReceive);
                RTPrintf("Received total:     %llu Bytes\n", BytesReceivedTotal);
            }

            if (Active)
            {
                if (details == VMINFO_MACHINEREADABLE)
                {
                    RTPrintf("VRDPUserName=\"%lS\"\n", User.raw());
                    RTPrintf("VRDPDomain=\"%lS\"\n", Domain.raw());
                    RTPrintf("VRDPClientName=\"%lS\"\n", ClientName.raw());
                    RTPrintf("VRDPClientIP=\"%lS\"\n", ClientIP.raw());
                    RTPrintf("VRDPClientVersion=%d\n",  ClientVersion);
                    RTPrintf("VRDPEncryption=\"%s\"\n", EncryptionStyle == 0? "RDP4": "RDP5 (X.509)");
                }
                else
                {
                    RTPrintf("User name:          %lS\n", User.raw());
                    RTPrintf("Domain:             %lS\n", Domain.raw());
                    RTPrintf("Client name:        %lS\n", ClientName.raw());
                    RTPrintf("Client IP:          %lS\n", ClientIP.raw());
                    RTPrintf("Client version:     %d\n",  ClientVersion);
                    RTPrintf("Encryption:         %s\n", EncryptionStyle == 0? "RDP4": "RDP5 (X.509)");
                }
            }
        }

        if (details != VMINFO_MACHINEREADABLE)
            RTPrintf("\n");
    }

    if (    details == VMINFO_STANDARD
        ||  details == VMINFO_FULL
        ||  details == VMINFO_MACHINEREADABLE)
    {
        Bstr description;
        machine->COMGETTER(Description)(description.asOutParam());
        if (!description.isEmpty())
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("description=\"%lS\"\n", description.raw());
            else
                RTPrintf("Description:\n%lS\n", description.raw());
        }
    }

    ULONG guestVal;
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Guest:\n\n");

#ifdef VBOX_WITH_MEM_BALLOONING
    rc = machine->COMGETTER(MemoryBalloonSize)(&guestVal);
    if (SUCCEEDED(rc))
    {
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("GuestMemoryBalloon=%d\n", guestVal);
        else
            RTPrintf("Configured memory balloon size:      %d MB\n", guestVal);
    }
#endif
    rc = machine->COMGETTER(StatisticsUpdateInterval)(&guestVal);
    if (SUCCEEDED(rc))
    {
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("GuestStatisticsUpdateInterval=%d\n", guestVal);
        else
        {
            if (guestVal == 0)
                RTPrintf("Statistics update:                   disabled\n");
            else
                RTPrintf("Statistics update interval:          %d seconds\n", guestVal);
        }
    }
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");

    if (    console
        &&  (   details == VMINFO_STATISTICS
             || details == VMINFO_FULL
             || details == VMINFO_MACHINEREADABLE))
    {
        ComPtr <IGuest> guest;

        rc = console->COMGETTER(Guest)(guest.asOutParam());
        if (SUCCEEDED(rc))
        {
            ULONG statVal;

            rc = guest->GetStatistic(0, GuestStatisticType_SampleNumber, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestSample=%d\n", statVal);
                else
                    RTPrintf("Guest statistics for sample %d:\n\n", statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_CPULoad_Idle, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestLoadIdleCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: CPU Load Idle          %-3d%%\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_CPULoad_Kernel, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestLoadKernelCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: CPU Load Kernel        %-3d%%\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_CPULoad_User, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestLoadUserCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: CPU Load User          %-3d%%\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_Threads, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestThreadsCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Threads                %d\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_Processes, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestProcessesCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Processes              %d\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_Handles, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestHandlesCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Handles                %d\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_MemoryLoad, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryLoadCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Memory Load            %d%%\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_PhysMemTotal, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryTotalPhysCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Total physical memory  %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_PhysMemAvailable, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryFreePhysCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Free physical memory   %-4d MB\n", 0, statVal);
            }

#ifdef VBOX_WITH_MEM_BALLOONING
            rc = guest->GetStatistic(0, GuestStatisticType_PhysMemBalloon, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryBalloonCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Memory balloon size    %-4d MB\n", 0, statVal);
            }
#endif
            rc = guest->GetStatistic(0, GuestStatisticType_MemCommitTotal, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryCommittedCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Committed memory       %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_MemKernelTotal, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryTotalKernelCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Total kernel memory    %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_MemKernelPaged, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryPagedKernelCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Paged kernel memory    %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_MemKernelNonpaged, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestMemoryNonpagedKernelCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Nonpaged kernel memory %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_MemSystemCache, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestSystemCacheSizeCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: System cache size      %-4d MB\n", 0, statVal);
            }

            rc = guest->GetStatistic(0, GuestStatisticType_PageFileSize, &statVal);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("StatGuestPageFileSizeCPU%d=%d\n", 0, statVal);
                else
                    RTPrintf("CPU%d: Page file size         %-4d MB\n", 0, statVal);
            }

            RTPrintf("\n");
        }
        else
        {
            if (details != VMINFO_MACHINEREADABLE)
            {
                RTPrintf("[!] FAILED calling console->getGuest at line %d!\n", __LINE__);
                GluePrintRCMessage(rc);
            }
        }
    }

    /*
     * snapshots
     */
    ComPtr<ISnapshot> snapshot;
    rc = machine->GetSnapshot(Bstr(), snapshot.asOutParam());
    if (SUCCEEDED(rc) && snapshot)
    {
        ComPtr<ISnapshot> currentSnapshot;
        rc = machine->COMGETTER(CurrentSnapshot)(currentSnapshot.asOutParam());
        if (SUCCEEDED(rc))
        {
            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("Snapshots:\n\n");
            showSnapshots(snapshot, currentSnapshot, details);
        }
    }

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");
    return S_OK;
}

#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

static const RTGETOPTDEF g_aShowVMInfoOptions[] =
{
    { "--details",          'D', RTGETOPT_REQ_NOTHING },
    { "-details",           'D', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--statistics",       'S', RTGETOPT_REQ_NOTHING },
    { "-statistics",        'S', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--machinereadable",  'M', RTGETOPT_REQ_NOTHING },
    { "-machinereadable",   'M', RTGETOPT_REQ_NOTHING },    // deprecated
};

int handleShowVMInfo(HandlerArg *a)
{
    HRESULT rc;
    const char *VMNameOrUuid = NULL;
    bool fDetails = false;
    bool fStatistics = false;
    bool fMachinereadable = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aShowVMInfoOptions, RT_ELEMENTS(g_aShowVMInfoOptions), 0, 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'D':   // --details
                fDetails = true;
                break;

            case 'S':   // --statistics
                fStatistics = true;
                break;

            case 'M':   // --machinereadable
                fMachinereadable = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!VMNameOrUuid)
                    VMNameOrUuid = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_SHOWVMINFO, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_SHOWVMINFO, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_SHOWVMINFO, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_SHOWVMINFO, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_SHOWVMINFO, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_SHOWVMINFO, "error: %Rrs", c);
        }
    }

    /* check for required options */
    if (!VMNameOrUuid)
        return errorSyntax(USAGE_SHOWVMINFO, "VM name or UUID required");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Bstr uuid (VMNameOrUuid);
    if (!Guid (VMNameOrUuid).isEmpty())
    {
        CHECK_ERROR (a->virtualBox, GetMachine (uuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR (a->virtualBox, FindMachine (Bstr(VMNameOrUuid), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id) (uuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* 2nd option can be -details, -statistics or -argdump */
    VMINFO_DETAILS details = VMINFO_NONE;
    if (fMachinereadable)
        details = VMINFO_MACHINEREADABLE;
    else
    if (fDetails && fStatistics)
        details = VMINFO_FULL;
    else
    if (fDetails)
        details = VMINFO_STANDARD;
    else
    if (fStatistics)
        details = VMINFO_STATISTICS;

    ComPtr <IConsole> console;

    /* open an existing session for the VM */
    rc = a->virtualBox->OpenExistingSession (a->session, uuid);
    if (SUCCEEDED(rc))
        /* get the session machine */
        rc = a->session->COMGETTER(Machine)(machine.asOutParam());
    if (SUCCEEDED(rc))
        /* get the session console */
        rc = a->session->COMGETTER(Console)(console.asOutParam());

    rc = showVMInfo(a->virtualBox, machine, details, console);

    if (console)
        a->session->Close();

    return SUCCEEDED (rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
