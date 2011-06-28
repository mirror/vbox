/* $Id$ */
/** @file
 * VBoxManage - The 'showvminfo' command and helper routines.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#ifdef VBOX_WITH_PCI_PASSTHROUGH
#include <VBox/pci.h>
#endif

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
        RTPrintf("SnapshotUUID%lS=\"%s\"\n", prefix.raw(), Utf8Str(uuid).c_str());
    }
    else
    {
        /* print with indentation */
        bool fCurrent = (rootSnapshot == currentSnapshot);
        RTPrintf("   %lSName: %lS (UUID: %s)%s\n",
                 prefix.raw(),
                 name.raw(),
                 Utf8Str(uuid).c_str(),
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

static void makeTimeStr(char *s, int cb, int64_t millies)
{
    RTTIME t;
    RTTIMESPEC ts;

    RTTimeSpecSetMilli(&ts, millies);

    RTTimeExplode(&t, &ts);

    RTStrPrintf(s, cb, "%04d/%02d/%02d %02d:%02d:%02d UTC",
                        t.i32Year, t.u8Month, t.u8MonthDay,
                        t.u8Hour, t.u8Minute, t.u8Second);
}

const char *machineStateToName(MachineState_T machineState, bool fShort)
{
    switch (machineState)
    {
        case MachineState_PoweredOff:
            return fShort ? "poweroff"             : "powered off";
        case MachineState_Saved:
            return "saved";
        case MachineState_Aborted:
            return "aborted";
        case MachineState_Teleported:
            return "teleported";
        case MachineState_Running:
            return "running";
        case MachineState_Paused:
            return "paused";
        case MachineState_Stuck:
            return fShort ? "gurumeditation"       : "guru meditation";
        case MachineState_LiveSnapshotting:
            return fShort ? "livesnapshotting"     : "live snapshotting";
        case MachineState_Teleporting:
            return "teleporting";
        case MachineState_Starting:
            return "starting";
        case MachineState_Stopping:
            return "stopping";
        case MachineState_Saving:
            return "saving";
        case MachineState_Restoring:
            return "restoring";
        case MachineState_TeleportingPausedVM:
            return fShort ? "teleportingpausedvm"  : "teleporting paused vm";
        case MachineState_TeleportingIn:
            return fShort ? "teleportingin"        : "teleporting (incoming)";
        case MachineState_RestoringSnapshot:
            return fShort ? "restoringsnapshot"    : "restoring snapshot";
        case MachineState_DeletingSnapshot:
            return fShort ? "deletingsnapshot"     : "deleting snapshot";
        case MachineState_DeletingSnapshotOnline:
            return fShort ? "deletingsnapshotlive" : "deleting snapshot live";
        case MachineState_DeletingSnapshotPaused:
            return fShort ? "deletingsnapshotlivepaused" : "deleting snapshot live paused";
        case MachineState_SettingUp:
            return fShort ? "settingup"           : "setting up";
        default:
            break;
    }
    return "unknown";
}

const char *facilityStateToName(AdditionsFacilityStatus_T faStatus, bool fShort)
{
    switch (faStatus)
    {
        case AdditionsFacilityStatus_Inactive:
            return fShort ? "inactive" : "not active";
        case AdditionsFacilityStatus_Paused:
            return "paused";
        case AdditionsFacilityStatus_PreInit:
            return fShort ? "preinit" : "pre-initializing";
        case AdditionsFacilityStatus_Init:
            return fShort ? "init"    : "initializing";
        case AdditionsFacilityStatus_Active:
            return fShort ? "active"  : "active/running";
        case AdditionsFacilityStatus_Terminating:
            return "terminating";
        case AdditionsFacilityStatus_Terminated:
            return "terminated";
        case AdditionsFacilityStatus_Failed:
            return "failed";
        case AdditionsFacilityStatus_Unknown:
        default:
            break;
    }
    return "unknown";
}

/* Disable global optimizations for MSC 8.0/64 to make it compile in reasonable
   time. MSC 7.1/32 doesn't have quite as much trouble with it, but still
   sufficient to qualify for this hack as well since this code isn't performance
   critical and probably won't gain much from the extra optimizing in real life. */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

HRESULT showVMInfo(ComPtr<IVirtualBox> virtualBox,
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
    if (FAILED(rc)) return rc;

    Bstr uuid;
    rc = machine->COMGETTER(Id)(uuid.asOutParam());

    if (!accessible)
    {
        if (details == VMINFO_COMPACT)
            RTPrintf("\"<inaccessible>\" {%s}\n", Utf8Str(uuid).c_str());
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("name=\"<inaccessible>\"\n");
            else
                RTPrintf("Name:            <inaccessible!>\n");
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("UUID=\"%s\"\n", Utf8Str(uuid).c_str());
            else
                RTPrintf("UUID:            %s\n", Utf8Str(uuid).c_str());
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
        RTPrintf("\"%lS\" {%s}\n", machineName.raw(), Utf8Str(uuid).c_str());
        return S_OK;
    }

    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("name=\"%lS\"\n", machineName.raw());
    else
        RTPrintf("Name:            %lS\n", machineName.raw());

    Bstr osTypeId;
    rc = machine->COMGETTER(OSTypeId)(osTypeId.asOutParam());
    ComPtr<IGuestOSType> osType;
    rc = virtualBox->GetGuestOSType(osTypeId.raw(), osType.asOutParam());
    Bstr osName;
    rc = osType->COMGETTER(Description)(osName.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("ostype=\"%lS\"\n", osTypeId.raw());
    else
        RTPrintf("Guest OS:        %lS\n", osName.raw());

    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("UUID=\"%s\"\n", Utf8Str(uuid).c_str());
    else
        RTPrintf("UUID:            %s\n", Utf8Str(uuid).c_str());

    Bstr settingsFilePath;
    rc = machine->COMGETTER(SettingsFilePath)(settingsFilePath.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("CfgFile=\"%lS\"\n", settingsFilePath.raw());
    else
        RTPrintf("Config file:     %lS\n", settingsFilePath.raw());

    Bstr snapshotFolder;
    rc = machine->COMGETTER(SnapshotFolder)(snapshotFolder.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("SnapFldr=\"%lS\"\n", snapshotFolder.raw());
    else
        RTPrintf("Snapshot folder: %lS\n", snapshotFolder.raw());

    Bstr logFolder;
    rc = machine->COMGETTER(LogFolder)(logFolder.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("LogFldr=\"%lS\"\n", logFolder.raw());
    else
        RTPrintf("Log folder:      %lS\n", logFolder.raw());

    Bstr strHardwareUuid;
    rc = machine->COMGETTER(HardwareUUID)(strHardwareUuid.asOutParam());
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hardwareuuid=\"%lS\"\n", strHardwareUuid.raw());
    else
        RTPrintf("Hardware UUID:   %lS\n", strHardwareUuid.raw());

    ULONG memorySize;
    rc = machine->COMGETTER(MemorySize)(&memorySize);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("memory=%u\n", memorySize);
    else
        RTPrintf("Memory size:     %uMB\n", memorySize);

    BOOL fPageFusionEnabled;
    rc = machine->COMGETTER(PageFusionEnabled)(&fPageFusionEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("pagefusion=\"%s\"\n", fPageFusionEnabled ? "on" : "off");
    else
        RTPrintf("Page Fusion:     %s\n", fPageFusionEnabled ? "on" : "off");

    ULONG vramSize;
    rc = machine->COMGETTER(VRAMSize)(&vramSize);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("vram=%u\n", vramSize);
    else
        RTPrintf("VRAM size:       %uMB\n", vramSize);

    ULONG cpuCap;
    rc = machine->COMGETTER(CPUExecutionCap)(&cpuCap);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("cpuexecutioncap=%u\n", cpuCap);
    else
        RTPrintf("CPU exec cap:    %u%%\n", cpuCap);

    BOOL fHpetEnabled;
    machine->COMGETTER(HpetEnabled)(&fHpetEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hpet=\"%s\"\n", fHpetEnabled ? "on" : "off");
    else
        RTPrintf("HPET:            %s\n", fHpetEnabled ? "on" : "off");

    ChipsetType_T chipsetType = ChipsetType_Null;
    const char *pszChipsetType = NULL;
    machine->COMGETTER(ChipsetType)(&chipsetType);
    switch (chipsetType)
    {
        case ChipsetType_Null:
            pszChipsetType = "invalid";
            break;
        case ChipsetType_PIIX3:
            pszChipsetType = "piix3";
            break;
        case ChipsetType_ICH9:
            pszChipsetType = "ich9";
            break;
        default:
            Assert(false);
            pszChipsetType = "unknown";
    }
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("chipset=\"%s\"\n", pszChipsetType);
    else
        RTPrintf("Chipset:         %s\n", pszChipsetType);

    FirmwareType_T firmwareType = FirmwareType_BIOS;
    const char *pszFirmwareType = NULL;
    machine->COMGETTER(FirmwareType)(&firmwareType);
    switch (firmwareType)
    {
        case FirmwareType_BIOS:
            pszFirmwareType = "BIOS";
            break;
        case FirmwareType_EFI:
            pszFirmwareType = "EFI";
            break;
        case FirmwareType_EFI32:
            pszFirmwareType = "EFI32";
            break;
        case FirmwareType_EFI64:
            pszFirmwareType = "EFI64";
            break;
        case FirmwareType_EFIDUAL:
            pszFirmwareType = "EFIDUAL";
            break;
        default:
            Assert(false);
            pszFirmwareType = "unknown";
    }
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("firmware=\"%s\"\n", pszFirmwareType);
    else
        RTPrintf("Firmware:        %s\n", pszFirmwareType);


    ULONG numCpus;
    rc = machine->COMGETTER(CPUCount)(&numCpus);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("cpus=%u\n", numCpus);
    else
        RTPrintf("Number of CPUs:  %u\n", numCpus);

    BOOL fSyntheticCpu;
    machine->GetCPUProperty(CPUPropertyType_Synthetic, &fSyntheticCpu);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("synthcpu=\"%s\"\n", fSyntheticCpu ? "on" : "off");
    else
        RTPrintf("Synthetic Cpu:   %s\n", fSyntheticCpu ? "on" : "off");

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("CPUID overrides: ");
    ULONG cFound = 0;
    static uint32_t const s_auCpuIdRanges[] =
    {
        UINT32_C(0x00000000), UINT32_C(0x0000000a),
        UINT32_C(0x80000000), UINT32_C(0x8000000a)
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_auCpuIdRanges); i += 2)
        for (uint32_t uLeaf = s_auCpuIdRanges[i]; uLeaf < s_auCpuIdRanges[i + 1]; uLeaf++)
        {
            ULONG uEAX, uEBX, uECX, uEDX;
            rc = machine->GetCPUIDLeaf(uLeaf, &uEAX, &uEBX, &uECX, &uEDX);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("cpuid=%08x,%08x,%08x,%08x,%08x", uLeaf, uEAX, uEBX, uECX, uEDX);
                else
                {
                    if (!cFound)
                        RTPrintf("Leaf no.  EAX      EBX      ECX      EDX\n");
                    RTPrintf("                 %08x  %08x %08x %08x %08x\n", uLeaf, uEAX, uEBX, uECX, uEDX);
                }
                cFound++;
            }
        }
    if (!cFound && details != VMINFO_MACHINEREADABLE)
        RTPrintf("None\n");

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
    machine->GetCPUProperty(CPUPropertyType_PAE, &PAEEnabled);
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

    BOOL RTCUseUTC;
    machine->COMGETTER(RTCUseUTC)(&RTCUseUTC);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("rtcuseutc=\"%s\"\n", RTCUseUTC ? "on" : "off");
    else
        RTPrintf("RTC:             %s\n", RTCUseUTC ? "UTC" : "local time");

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

    BOOL HWVirtExLargePagesEnabled;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_LargePages, &HWVirtExLargePagesEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("largepages=\"%s\"\n", HWVirtExLargePagesEnabled ? "on" : "off");
    else
        RTPrintf("Large Pages:     %s\n", HWVirtExLargePagesEnabled ? "on" : "off");

    BOOL HWVirtExVPIDEnabled;
    machine->GetHWVirtExProperty(HWVirtExPropertyType_VPID, &HWVirtExVPIDEnabled);
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("vtxvpid=\"%s\"\n", HWVirtExVPIDEnabled ? "on" : "off");
    else
        RTPrintf("VT-x VPID:       %s\n", HWVirtExVPIDEnabled ? "on" : "off");

    MachineState_T machineState;
    rc = machine->COMGETTER(State)(&machineState);
    const char *pszState = machineStateToName(machineState, details == VMINFO_MACHINEREADABLE /*=fShort*/);

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
    CHECK_ERROR(machine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(storageCtls)));
    for (size_t i = 0; i < storageCtls.size(); ++ i)
    {
        ComPtr<IStorageController> storageCtl = storageCtls[i];
        StorageControllerType_T    enmCtlType = StorageControllerType_Null;
        const char *pszCtl = NULL;
        ULONG ulValue = 0;
        BOOL  fBootable = FALSE;
        Bstr storageCtlName;

        storageCtl->COMGETTER(Name)(storageCtlName.asOutParam());
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollername%u=\"%lS\"\n", i, storageCtlName.raw());
        else
            RTPrintf("Storage Controller Name (%u):            %lS\n", i, storageCtlName.raw());

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
            RTPrintf("storagecontrollertype%u=\"%s\"\n", i, pszCtl);
        else
            RTPrintf("Storage Controller Type (%u):            %s\n", i, pszCtl);

        storageCtl->COMGETTER(Instance)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerinstance%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Instance Number (%u): %lu\n", i, ulValue);

        storageCtl->COMGETTER(MaxPortCount)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollermaxportcount%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Max Port Count (%u):  %lu\n", i, ulValue);

        storageCtl->COMGETTER(PortCount)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerportcount%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Port Count (%u):      %lu\n", i, ulValue);

        storageCtl->COMGETTER(Bootable)(&fBootable);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerbootable%u=\"%s\"\n", i, fBootable ? "on" : "off");
        else
            RTPrintf("Storage Controller Bootable (%u):        %s\n", i, fBootable ? "on" : "off");
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
                rc = machine->GetMedium(storageCtlName.raw(), i, k,
                                        medium.asOutParam());
                if (SUCCEEDED(rc) && medium)
                {
                    BOOL fPassthrough;
                    ComPtr<IMediumAttachment> mediumAttach;

                    rc = machine->GetMediumAttachment(storageCtlName.raw(),
                                                      i, k,
                                                      mediumAttach.asOutParam());
                    if (SUCCEEDED(rc) && mediumAttach)
                        mediumAttach->COMGETTER(Passthrough)(&fPassthrough);

                    medium->COMGETTER(Location)(filePath.asOutParam());
                    medium->COMGETTER(Id)(uuid.asOutParam());

                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        RTPrintf("\"%lS-%d-%d\"=\"%lS\"\n", storageCtlName.raw(),
                                 i, k, filePath.raw());
                        RTPrintf("\"%lS-ImageUUID-%d-%d\"=\"%s\"\n",
                                 storageCtlName.raw(), i, k, Utf8Str(uuid).c_str());
                        if (fPassthrough)
                            RTPrintf("\"%lS-dvdpassthrough\"=\"%s\"\n", storageCtlName.raw(),
                                     fPassthrough ? "on" : "off");
                    }
                    else
                    {
                        RTPrintf("%lS (%d, %d): %lS (UUID: %s)",
                                 storageCtlName.raw(), i, k, filePath.raw(),
                                 Utf8Str(uuid).c_str());
                        if (fPassthrough)
                            RTPrintf(" (passthrough enabled)");
                        RTPrintf("\n");
                    }
                }
                else if (SUCCEEDED(rc))
                {
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("\"%lS-%d-%d\"=\"emptydrive\"\n", storageCtlName.raw(), i, k);
                    else
                        RTPrintf("%lS (%d, %d): Empty\n", storageCtlName.raw(), i, k);
                }
                else
                {
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("\"%lS-%d-%d\"=\"none\"\n", storageCtlName.raw(), i, k);
                }
            }
        }
    }

    /* get the maximum amount of NICS */
    ULONG maxNICs = getMaxNics(virtualBox, machine);

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
                Utf8Str strNatSettings = "";
                Utf8Str strNatForwardings = "";
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
                        ComPtr<INATEngine> driver;
                        nic->COMGETTER(NatDriver)(driver.asOutParam());
                        driver->COMGETTER(Network)(strNetwork.asOutParam());
                        com::SafeArray<BSTR> forwardings;
                        driver->COMGETTER(Redirects)(ComSafeArrayAsOutParam(forwardings));
                        strNatForwardings = "";
                        for (size_t i = 0; i < forwardings.size(); ++i)
                        {
                            bool fSkip = false;
                            uint16_t port = 0;
                            BSTR r = forwardings[i];
                            Utf8Str utf = Utf8Str(r);
                            Utf8Str strName;
                            Utf8Str strProto;
                            Utf8Str strHostPort;
                            Utf8Str strHostIP;
                            Utf8Str strGuestPort;
                            Utf8Str strGuestIP;
                            size_t pos, ppos;
                            pos = ppos = 0;
                            #define ITERATE_TO_NEXT_TERM(res, str, pos, ppos)   \
                            do {                                                \
                                pos = str.find(",", ppos);                      \
                                if (pos == Utf8Str::npos)                       \
                                {                                               \
                                    Log(( #res " extracting from %s is failed\n", str.c_str())); \
                                    fSkip = true;                               \
                                }                                               \
                                res = str.substr(ppos, pos - ppos);             \
                                Log2((#res " %s pos:%d, ppos:%d\n", res.c_str(), pos, ppos)); \
                                ppos = pos + 1;                                 \
                            } while (0)
                            ITERATE_TO_NEXT_TERM(strName, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strProto, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strHostIP, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strHostPort, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strGuestIP, utf, pos, ppos);
                            if (fSkip) continue;
                            strGuestPort = utf.substr(ppos, utf.length() - ppos);
                            #undef ITERATE_TO_NEXT_TERM
                            switch (strProto.toUInt32())
                            {
                                case NATProtocol_TCP:
                                    strProto = "tcp";
                                    break;
                                case NATProtocol_UDP:
                                    strProto = "udp";
                                    break;
                                default:
                                    strProto = "unk";
                                    break;
                            }
                            if (details == VMINFO_MACHINEREADABLE)
                            {
                                strNatForwardings = Utf8StrFmt("%sForwarding(%d)=\"%s,%s,%s,%s,%s,%s\"\n",
                                    strNatForwardings.c_str(), i, strName.c_str(), strProto.c_str(),
                                    strHostIP.c_str(), strHostPort.c_str(),
                                    strGuestIP.c_str(), strGuestPort.c_str());
                            }
                            else
                            {
                                strNatForwardings = Utf8StrFmt("%sNIC %d Rule(%d):   name = %s, protocol = %s,"
                                    " host ip = %s, host port = %s, guest ip = %s, guest port = %s\n",
                                    strNatForwardings.c_str(), currentNIC + 1, i, strName.c_str(), strProto.c_str(),
                                    strHostIP.c_str(), strHostPort.c_str(),
                                    strGuestIP.c_str(), strGuestPort.c_str());
                            }
                        }
                        ULONG mtu = 0;
                        ULONG sockSnd = 0;
                        ULONG sockRcv = 0;
                        ULONG tcpSnd = 0;
                        ULONG tcpRcv = 0;
                        driver->GetNetworkSettings(&mtu, &sockSnd, &sockRcv, &tcpSnd, &tcpRcv);

                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("natnet%d=\"%lS\"\n", currentNIC + 1, strNetwork.length() ? strNetwork.raw(): Bstr("nat").raw());
                            strAttachment = "nat";
                            strNatSettings = Utf8StrFmt("mtu=\"%d\"\nsockSnd=\"%d\"\nsockRcv=\"%d\"\ntcpWndSnd=\"%d\"\ntcpWndRcv=\"%d\"\n",
                                mtu, sockSnd ? sockSnd : 64, sockRcv ? sockRcv : 64 , tcpSnd ? tcpSnd : 64, tcpRcv ? tcpRcv : 64);
                        }
                        else
                        {
                            strAttachment = "NAT";
                            strNatSettings = Utf8StrFmt("NIC %d Settings:  MTU: %d, Socket (send: %d, receive: %d), TCP Window (send:%d, receive: %d)\n",
                                currentNIC + 1, mtu, sockSnd ? sockSnd : 64, sockRcv ? sockRcv : 64 , tcpSnd ? tcpSnd : 64, tcpRcv ? tcpRcv : 64);
                        }
                        break;
                    }

                    case NetworkAttachmentType_Bridged:
                    {
                        Bstr strBridgeAdp;
                        nic->COMGETTER(BridgedInterface)(strBridgeAdp.asOutParam());
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
                            strAttachment = Utf8StrFmt("Internal Network '%s'", Utf8Str(strNetwork).c_str());
                        break;
                    }

                    case NetworkAttachmentType_HostOnly:
                    {
                        Bstr strHostonlyAdp;
                        nic->COMGETTER(HostOnlyInterface)(strHostonlyAdp.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("hostonlyadapter%d=\"%lS\"\n", currentNIC + 1, strHostonlyAdp.raw());
                            strAttachment = "hostonly";
                        }
                        else
                            strAttachment = Utf8StrFmt("Host-only Interface '%lS'", strHostonlyAdp.raw());
                        break;
                    }
                    case NetworkAttachmentType_Generic:
                    {
                        Bstr strGenericDriver;
                        nic->COMGETTER(GenericDriver)(strGenericDriver.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("generic%d=\"%lS\"\n", currentNIC + 1, strGenericDriver.raw());
                            strAttachment = "Generic";
                        }
                        else
                        {
                            strAttachment = Utf8StrFmt("Generic '%lS'", strGenericDriver.raw());

                            // show the generic properties
                            com::SafeArray<BSTR> aProperties;
                            com::SafeArray<BSTR> aValues;
                            rc = nic->GetProperties(NULL,
                                                    ComSafeArrayAsOutParam(aProperties),
                                                    ComSafeArrayAsOutParam(aValues));
                            if (SUCCEEDED(rc))
                            {
                                strAttachment += " { ";
                                for (unsigned i = 0; i < aProperties.size(); ++i)
                                    strAttachment += Utf8StrFmt(!i ? "%lS='%lS'" : ", %lS='%lS'",
                                                                aProperties[i], aValues[i]);
                                strAttachment += " }";
                            }
                        }
                        break;
                    }
                    default:
                        strAttachment = "unknown";
                        break;
                }

                /* cable connected */
                BOOL fConnected;
                nic->COMGETTER(CableConnected)(&fConnected);

                /* promisc policy */
                NetworkAdapterPromiscModePolicy_T enmPromiscModePolicy;
                CHECK_ERROR2_RET(nic, COMGETTER(PromiscModePolicy)(&enmPromiscModePolicy), hrcCheck);
                const char *pszPromiscuousGuestPolicy;
                switch (enmPromiscModePolicy)
                {
                    case NetworkAdapterPromiscModePolicy_Deny:          pszPromiscuousGuestPolicy = "deny"; break;
                    case NetworkAdapterPromiscModePolicy_AllowNetwork:  pszPromiscuousGuestPolicy = "allow-vms"; break;
                    case NetworkAdapterPromiscModePolicy_AllowAll:      pszPromiscuousGuestPolicy = "allow-all"; break;
                    default: AssertFailedReturn(VERR_INTERNAL_ERROR_4);
                }

                /* trace stuff */
                BOOL fTraceEnabled;
                nic->COMGETTER(TraceEnabled)(&fTraceEnabled);
                Bstr traceFile;
                nic->COMGETTER(TraceFile)(traceFile.asOutParam());

                /* NIC type */
                NetworkAdapterType_T NICType;
                nic->COMGETTER(AdapterType)(&NICType);
                const char *pszNICType;
                switch (NICType)
                {
                    case NetworkAdapterType_Am79C970A:  pszNICType = "Am79C970A";   break;
                    case NetworkAdapterType_Am79C973:   pszNICType = "Am79C973";    break;
#ifdef VBOX_WITH_E1000
                    case NetworkAdapterType_I82540EM:   pszNICType = "82540EM";     break;
                    case NetworkAdapterType_I82543GC:   pszNICType = "82543GC";     break;
                    case NetworkAdapterType_I82545EM:   pszNICType = "82545EM";     break;
#endif
#ifdef VBOX_WITH_VIRTIO
                    case NetworkAdapterType_Virtio:     pszNICType = "virtio";      break;
#endif
                    default: AssertFailed();            pszNICType = "unknown";     break;
                }

                /* reported line speed */
                ULONG ulLineSpeed;
                nic->COMGETTER(LineSpeed)(&ulLineSpeed);

                /* boot priority of the adapter */
                ULONG ulBootPriority;
                nic->COMGETTER(BootPriority)(&ulBootPriority);

                if (details == VMINFO_MACHINEREADABLE)
                {
                    RTPrintf("macaddress%d=\"%lS\"\n", currentNIC + 1, strMACAddress.raw());
                    RTPrintf("cableconnected%d=\"%s\"\n", currentNIC + 1, fConnected ? "on" : "off");
                    RTPrintf("nic%d=\"%s\"\n", currentNIC + 1, strAttachment.c_str());
                }
                else
                    RTPrintf("NIC %u:           MAC: %lS, Attachment: %s, Cable connected: %s, Trace: %s (file: %lS), Type: %s, Reported speed: %d Mbps, Boot priority: %d, Promisc Policy: %s\n",
                             currentNIC + 1, strMACAddress.raw(), strAttachment.c_str(),
                             fConnected ? "on" : "off",
                             fTraceEnabled ? "on" : "off",
                             traceFile.isEmpty() ? Bstr("none").raw() : traceFile.raw(),
                             pszNICType,
                             ulLineSpeed / 1000,
                             (int)ulBootPriority,
                             pszPromiscuousGuestPolicy);
                if (strNatSettings.length())
                    RTPrintf(strNatSettings.c_str());
                if (strNatForwardings.length())
                    RTPrintf(strNatForwardings.c_str());
            }
        }
    }

    /* Pointing device information */
    PointingHidType_T aPointingHid;
    const char *pszHid = "Unknown";
    const char *pszMrHid = "unknown";
    machine->COMGETTER(PointingHidType)(&aPointingHid);
    switch (aPointingHid)
    {
        case PointingHidType_None:
            pszHid = "None";
            pszMrHid = "none";
            break;
        case PointingHidType_PS2Mouse:
            pszHid = "PS/2 Mouse";
            pszMrHid = "ps2mouse";
            break;
        case PointingHidType_USBMouse:
            pszHid = "USB Mouse";
            pszMrHid = "usbmouse";
            break;
        case PointingHidType_USBTablet:
            pszHid = "USB Tablet";
            pszMrHid = "usbtablet";
            break;
        case PointingHidType_ComboMouse:
            pszHid = "USB Tablet and PS/2 Mouse";
            pszMrHid = "combomouse";
            break;
        default:
            break;
    }
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hidpointing=\"%s\"\n", pszMrHid);
    else
        RTPrintf("Pointing Device: %s\n", pszHid);

    /* Keyboard device information */
    KeyboardHidType_T aKeyboardHid;
    machine->COMGETTER(KeyboardHidType)(&aKeyboardHid);
    pszHid = "Unknown";
    pszMrHid = "unknown";
    switch (aKeyboardHid)
    {
        case KeyboardHidType_None:
            pszHid = "None";
            pszMrHid = "none";
            break;
        case KeyboardHidType_PS2Keyboard:
            pszHid = "PS/2 Keyboard";
            pszMrHid = "ps2kbd";
            break;
        case KeyboardHidType_USBKeyboard:
            pszHid = "USB Keyboard";
            pszMrHid = "usbkbd";
            break;
        case KeyboardHidType_ComboKeyboard:
            pszHid = "USB and PS/2 Keyboard";
            pszMrHid = "combokbd";
            break;
        default:
            break;
    }
    if (details == VMINFO_MACHINEREADABLE)
        RTPrintf("hidkeyboard=\"%s\"\n", pszMrHid);
    else
        RTPrintf("Keyboard Device: %s\n", pszHid);

    /* get the maximum amount of UARTs */
    ComPtr<ISystemProperties> sysProps;
    virtualBox->COMGETTER(SystemProperties)(sysProps.asOutParam());

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
                    RTPrintf("UART %d:          I/O base: %#06x, IRQ: %d",
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
                case AudioControllerType_HDA:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "hda";
                    else
                        pszCtrl = "HDA";
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
            rc = display->GetScreenResolution(0, &xRes, &yRes, &bpp);
            if (rc == E_ACCESSDENIED)
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::ErrorInfo info(display, COM_IIDOF(IDisplay));
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
     * Remote Desktop
     */
    ComPtr<IVRDEServer> vrdeServer;
    rc = machine->COMGETTER(VRDEServer)(vrdeServer.asOutParam());
    if (SUCCEEDED(rc) && vrdeServer)
    {
        BOOL fEnabled = false;
        vrdeServer->COMGETTER(Enabled)(&fEnabled);
        if (fEnabled)
        {
            LONG currentPort = -1;
            Bstr ports;
            vrdeServer->GetVRDEProperty(Bstr("TCP/Ports").raw(), ports.asOutParam());
            Bstr address;
            vrdeServer->GetVRDEProperty(Bstr("TCP/Address").raw(), address.asOutParam());
            BOOL fMultiCon;
            vrdeServer->COMGETTER(AllowMultiConnection)(&fMultiCon);
            BOOL fReuseCon;
            vrdeServer->COMGETTER(ReuseSingleConnection)(&fReuseCon);
            Bstr videoChannel;
            vrdeServer->GetVRDEProperty(Bstr("VideoChannel/Enabled").raw(), videoChannel.asOutParam());
            BOOL fVideoChannel =    (videoChannel.compare(Bstr("true"), Bstr::CaseInsensitive)== 0)
                                 || (videoChannel == "1");
            Bstr videoChannelQuality;
            vrdeServer->GetVRDEProperty(Bstr("VideoChannel/Quality").raw(), videoChannelQuality.asOutParam());
            AuthType_T authType;
            const char *strAuthType;
            vrdeServer->COMGETTER(AuthType)(&authType);
            switch (authType)
            {
                case AuthType_Null:
                    strAuthType = "null";
                    break;
                case AuthType_External:
                    strAuthType = "external";
                    break;
                case AuthType_Guest:
                    strAuthType = "guest";
                    break;
                default:
                    strAuthType = "unknown";
                    break;
            }
            if (console)
            {
                ComPtr<IVRDEServerInfo> vrdeServerInfo;
                CHECK_ERROR_RET(console, COMGETTER(VRDEServerInfo)(vrdeServerInfo.asOutParam()), rc);
                rc = vrdeServerInfo->COMGETTER(Port)(&currentPort);
                if (rc == E_ACCESSDENIED)
                {
                    currentPort = -1; /* VM not powered up */
                }
                if (FAILED(rc))
                {
                    com::ErrorInfo info(vrdeServerInfo, COM_IIDOF(IVRDEServerInfo));
                    GluePrintErrorInfo(info);
                    return rc;
                }
            }
            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("vrde=\"on\"\n");
                RTPrintf("vrdeport=%d\n", currentPort);
                RTPrintf("vrdeports=\"%lS\"\n", ports.raw());
                RTPrintf("vrdeaddress=\"%lS\"\n", address.raw());
                RTPrintf("vrdeauthtype=\"%s\"\n", strAuthType);
                RTPrintf("vrdemulticon=\"%s\"\n", fMultiCon ? "on" : "off");
                RTPrintf("vrdereusecon=\"%s\"\n", fReuseCon ? "on" : "off");
                RTPrintf("vrdevideochannel=\"%s\"\n", fVideoChannel ? "on" : "off");
                if (fVideoChannel)
                    RTPrintf("vrdevideochannelquality=\"%lS\"\n", videoChannelQuality.raw());
            }
            else
            {
                if (address.isEmpty())
                    address = "0.0.0.0";
                RTPrintf("VRDE:            enabled (Address %lS, Ports %lS, MultiConn: %s, ReuseSingleConn: %s, Authentication type: %s)\n", address.raw(), ports.raw(), fMultiCon ? "on" : "off", fReuseCon ? "on" : "off", strAuthType);
                if (console && currentPort != -1 && currentPort != 0)
                   RTPrintf("VRDE port:       %d\n", currentPort);
                if (fVideoChannel)
                    RTPrintf("Video redirection: enabled (Quality %lS)\n", videoChannelQuality.raw());
                else
                    RTPrintf("Video redirection: disabled\n");
            }
            com::SafeArray<BSTR> aProperties;
            if (SUCCEEDED(vrdeServer->COMGETTER(VRDEProperties)(ComSafeArrayAsOutParam(aProperties))))
            {
                unsigned i;
                for (i = 0; i < aProperties.size(); ++i)
                {
                    Bstr value;
                    vrdeServer->GetVRDEProperty(aProperties[i], value.asOutParam());
                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        if (value.isEmpty())
                            RTPrintf("vrdeproperty[%lS]=<not set>\n", aProperties[i]);
                        else
                            RTPrintf("vrdeproperty[%lS]=\"%lS\"\n", aProperties[i], value.raw());
                    }
                    else
                    {
                        if (value.isEmpty())
                            RTPrintf("VRDE property: %-10lS = <not set>\n", aProperties[i]);
                        else
                            RTPrintf("VRDE property: %-10lS = \"%lS\"\n", aProperties[i], value.raw());
                    }
                }
            }
        }
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("vrde=\"off\"\n");
            else
                RTPrintf("VRDE:            disabled\n");
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

        SafeIfaceArray <IUSBDeviceFilter> Coll;
        rc = USBCtl->COMGETTER(DeviceFilters)(ComSafeArrayAsOutParam(Coll));
        if (SUCCEEDED(rc))
        {
        if (details != VMINFO_MACHINEREADABLE)
            RTPrintf("\nUSB Device Filters:\n\n");

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
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Active)(&bActive), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterActive%zu=\"%s\"\n", index + 1, bActive ? "on" : "off");
                    else
                        RTPrintf("Active:           %s\n", bActive ? "yes" : "no");

                    Bstr bstr;
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Name)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterName%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Name:             %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(VendorId)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterVendorId%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("VendorId:         %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(ProductId)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterProductId%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("ProductId:        %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Revision)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterRevision%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Revision:         %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Manufacturer)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Manufacturer:     %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Product)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Product:          %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(Remote)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterRemote%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Remote:           %lS\n", bstr.raw());
                    CHECK_ERROR_RET(DevPtr, COMGETTER(SerialNumber)(bstr.asOutParam()), rc);
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("USBFilterSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                    else
                        RTPrintf("Serial Number:    %lS\n", bstr.raw());
                    if (details != VMINFO_MACHINEREADABLE)
                    {
                        ULONG fMaskedIfs;
                        CHECK_ERROR_RET(DevPtr, COMGETTER(MaskedInterfaces)(&fMaskedIfs), rc);
                        if (fMaskedIfs)
                            RTPrintf("Masked Interfaces: %#010x\n", fMaskedIfs);
                        RTPrintf("\n");
                    }
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
                CHECK_ERROR_RET(console, COMGETTER(RemoteUSBDevices)(ComSafeArrayAsOutParam(coll)), rc);

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
                        CHECK_ERROR_RET(dev, COMGETTER(Id)(id.asOutParam()), rc);
                        USHORT usVendorId;
                        CHECK_ERROR_RET(dev, COMGETTER(VendorId)(&usVendorId), rc);
                        USHORT usProductId;
                        CHECK_ERROR_RET(dev, COMGETTER(ProductId)(&usProductId), rc);
                        USHORT bcdRevision;
                        CHECK_ERROR_RET(dev, COMGETTER(Revision)(&bcdRevision), rc);

                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("USBRemoteUUID%zu=\"%S\"\n"
                                     "USBRemoteVendorId%zu=\"%#06x\"\n"
                                     "USBRemoteProductId%zu=\"%#06x\"\n"
                                     "USBRemoteRevision%zu=\"%#04x%02x\"\n",
                                     index + 1, Utf8Str(id).c_str(),
                                     index + 1, usVendorId,
                                     index + 1, usProductId,
                                     index + 1, bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTPrintf("UUID:               %S\n"
                                     "VendorId:           %#06x (%04X)\n"
                                     "ProductId:          %#06x (%04X)\n"
                                     "Revision:           %u.%u (%02u%02u)\n",
                                     Utf8Str(id).c_str(),
                                     usVendorId, usVendorId, usProductId, usProductId,
                                     bcdRevision >> 8, bcdRevision & 0xff,
                                     bcdRevision >> 8, bcdRevision & 0xff);

                        /* optional stuff. */
                        Bstr bstr;
                        CHECK_ERROR_RET(dev, COMGETTER(Manufacturer)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Manufacturer:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(Product)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Product:            %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(SerialNumber)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBRemoteSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("SerialNumber:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(Address)(bstr.asOutParam()), rc);
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
                    RTPrintf("Currently Attached USB Devices:\n\n");

                SafeIfaceArray <IUSBDevice> coll;
                CHECK_ERROR_RET(console, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)), rc);

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
                        CHECK_ERROR_RET(dev, COMGETTER(Id)(id.asOutParam()), rc);
                        USHORT usVendorId;
                        CHECK_ERROR_RET(dev, COMGETTER(VendorId)(&usVendorId), rc);
                        USHORT usProductId;
                        CHECK_ERROR_RET(dev, COMGETTER(ProductId)(&usProductId), rc);
                        USHORT bcdRevision;
                        CHECK_ERROR_RET(dev, COMGETTER(Revision)(&bcdRevision), rc);

                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("USBAttachedUUID%zu=\"%S\"\n"
                                     "USBAttachedVendorId%zu=\"%#06x\"\n"
                                     "USBAttachedProductId%zu=\"%#06x\"\n"
                                     "USBAttachedRevision%zu=\"%#04x%02x\"\n",
                                     index + 1, Utf8Str(id).c_str(),
                                     index + 1, usVendorId,
                                     index + 1, usProductId,
                                     index + 1, bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTPrintf("UUID:               %S\n"
                                     "VendorId:           %#06x (%04X)\n"
                                     "ProductId:          %#06x (%04X)\n"
                                     "Revision:           %u.%u (%02u%02u)\n",
                                     Utf8Str(id).c_str(),
                                     usVendorId, usVendorId, usProductId, usProductId,
                                     bcdRevision >> 8, bcdRevision & 0xff,
                                     bcdRevision >> 8, bcdRevision & 0xff);

                        /* optional stuff. */
                        Bstr bstr;
                        CHECK_ERROR_RET(dev, COMGETTER(Manufacturer)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedManufacturer%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Manufacturer:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(Product)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedProduct%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("Product:            %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(SerialNumber)(bstr.asOutParam()), rc);
                        if (!bstr.isEmpty())
                        {
                            if (details == VMINFO_MACHINEREADABLE)
                                RTPrintf("USBAttachedSerialNumber%zu=\"%lS\"\n", index + 1, bstr.raw());
                            else
                                RTPrintf("SerialNumber:       %lS\n", bstr.raw());
                        }
                        CHECK_ERROR_RET(dev, COMGETTER(Address)(bstr.asOutParam()), rc);
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

#ifdef VBOX_WITH_PCI_PASSTHROUGH
    /* Host PCI passthrough devices */
    {
         SafeIfaceArray <IPciDeviceAttachment> assignments;
         rc = machine->COMGETTER(PciDeviceAssignments)(ComSafeArrayAsOutParam(assignments));
         if (SUCCEEDED(rc))
         {
             if (assignments.size() > 0 && (details != VMINFO_MACHINEREADABLE))
             {
                 RTPrintf("\nAttached physical PCI devices:\n\n");
             }

             for (size_t index = 0; index < assignments.size(); ++index)
             {
                 ComPtr<IPciDeviceAttachment> Assignment = assignments[index];
                 char szHostPciAddress[32], szGuestPciAddress[32];
                 LONG iHostPciAddress = -1, iGuestPciAddress = -1;
                 Bstr DevName;

                 Assignment->COMGETTER(Name)(DevName.asOutParam());
                 Assignment->COMGETTER(HostAddress)(&iHostPciAddress);
                 Assignment->COMGETTER(GuestAddress)(&iGuestPciAddress);
                 PciBusAddress().fromLong(iHostPciAddress).format(szHostPciAddress, sizeof(szHostPciAddress));
                 PciBusAddress().fromLong(iGuestPciAddress).format(szGuestPciAddress, sizeof(szGuestPciAddress));

                 if (details == VMINFO_MACHINEREADABLE)
                     RTPrintf("AttachedHostPci=%s,%s\n", szHostPciAddress, szGuestPciAddress);
                 else
                     RTPrintf("   Host device %lS at %s attached as %s\n", DevName.raw(), szHostPciAddress, szGuestPciAddress);
             }

             if (assignments.size() > 0 && (details != VMINFO_MACHINEREADABLE))
             {
                 RTPrintf("\n");
             }
         }
    }
    /* Host PCI passthrough devices */
#endif

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
         * Live VRDE info.
         */
        ComPtr<IVRDEServerInfo> vrdeServerInfo;
        CHECK_ERROR_RET(console, COMGETTER(VRDEServerInfo)(vrdeServerInfo.asOutParam()), rc);
        BOOL    Active;
        ULONG   NumberOfClients;
        LONG64  BeginTime;
        LONG64  EndTime;
        LONG64  BytesSent;
        LONG64  BytesSentTotal;
        LONG64  BytesReceived;
        LONG64  BytesReceivedTotal;
        Bstr    User;
        Bstr    Domain;
        Bstr    ClientName;
        Bstr    ClientIP;
        ULONG   ClientVersion;
        ULONG   EncryptionStyle;

        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(Active)(&Active), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(NumberOfClients)(&NumberOfClients), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BeginTime)(&BeginTime), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(EndTime)(&EndTime), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesSent)(&BytesSent), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesSentTotal)(&BytesSentTotal), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesReceived)(&BytesReceived), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesReceivedTotal)(&BytesReceivedTotal), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(User)(User.asOutParam()), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(Domain)(Domain.asOutParam()), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientName)(ClientName.asOutParam()), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientIP)(ClientIP.asOutParam()), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientVersion)(&ClientVersion), rc);
        CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(EncryptionStyle)(&EncryptionStyle), rc);

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("VRDEActiveConnection=\"%s\"\n", Active ? "on": "off");
        else
            RTPrintf("VRDE Connection:    %s\n", Active? "active": "not active");

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("VRDEClients=%d\n", NumberOfClients);
        else
            RTPrintf("Clients so far:     %d\n", NumberOfClients);

        if (NumberOfClients > 0)
        {
            char timestr[128];

            if (Active)
            {
                makeTimeStr(timestr, sizeof(timestr), BeginTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDEStartTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Start time:         %s\n", timestr);
            }
            else
            {
                makeTimeStr(timestr, sizeof(timestr), BeginTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDELastStartTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Last started:       %s\n", timestr);
                makeTimeStr(timestr, sizeof(timestr), EndTime);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("VRDELastEndTime=\"%s\"\n", timestr);
                else
                    RTPrintf("Last ended:         %s\n", timestr);
            }

            int64_t ThroughputSend = 0;
            int64_t ThroughputReceive = 0;
            if (EndTime != BeginTime)
            {
                ThroughputSend = (BytesSent * 1000) / (EndTime - BeginTime);
                ThroughputReceive = (BytesReceived * 1000) / (EndTime - BeginTime);
            }

            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("VRDEBytesSent=%lld\n", BytesSent);
                RTPrintf("VRDEThroughputSend=%lld\n", ThroughputSend);
                RTPrintf("VRDEBytesSentTotal=%lld\n", BytesSentTotal);

                RTPrintf("VRDEBytesReceived=%lld\n", BytesReceived);
                RTPrintf("VRDEThroughputReceive=%lld\n", ThroughputReceive);
                RTPrintf("VRDEBytesReceivedTotal=%lld\n", BytesReceivedTotal);
            }
            else
            {
                RTPrintf("Sent:               %lld Bytes\n", BytesSent);
                RTPrintf("Average speed:      %lld B/s\n", ThroughputSend);
                RTPrintf("Sent total:         %lld Bytes\n", BytesSentTotal);

                RTPrintf("Received:           %lld Bytes\n", BytesReceived);
                RTPrintf("Speed:              %lld B/s\n", ThroughputReceive);
                RTPrintf("Received total:     %lld Bytes\n", BytesReceivedTotal);
            }

            if (Active)
            {
                if (details == VMINFO_MACHINEREADABLE)
                {
                    RTPrintf("VRDEUserName=\"%lS\"\n", User.raw());
                    RTPrintf("VRDEDomain=\"%lS\"\n", Domain.raw());
                    RTPrintf("VRDEClientName=\"%lS\"\n", ClientName.raw());
                    RTPrintf("VRDEClientIP=\"%lS\"\n", ClientIP.raw());
                    RTPrintf("VRDEClientVersion=%d\n",  ClientVersion);
                    RTPrintf("VRDEEncryption=\"%s\"\n", EncryptionStyle == 0? "RDP4": "RDP5 (X.509)");
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


    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Guest:\n\n");

    ULONG guestVal;
    rc = machine->COMGETTER(MemoryBalloonSize)(&guestVal);
    if (SUCCEEDED(rc))
    {
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("GuestMemoryBalloon=%d\n", guestVal);
        else
            RTPrintf("Configured memory balloon size:      %d MB\n", guestVal);
    }

    if (console)
    {
        ComPtr<IGuest> guest;
        rc = console->COMGETTER(Guest)(guest.asOutParam());
        if (SUCCEEDED(rc))
        {
            Bstr guestString;
            rc = guest->COMGETTER(OSTypeId)(guestString.asOutParam());
            if (   SUCCEEDED(rc)
                && !guestString.isEmpty())
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("GuestOSType=\"%lS\"\n", guestString.raw());
                else
                    RTPrintf("OS type:                             %lS\n", guestString.raw());
            }

            AdditionsRunLevelType_T guestRunLevel; /** @todo Add a runlevel-to-string (e.g. 0 = "None") method? */
            rc = guest->COMGETTER(AdditionsRunLevel)(&guestRunLevel);
            if (SUCCEEDED(rc))
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("GuestAdditionsRunLevel=%u\n", guestRunLevel);
                else
                    RTPrintf("Additions run level:                 %u\n", guestRunLevel);
            }

            rc = guest->COMGETTER(AdditionsVersion)(guestString.asOutParam());
            if (   SUCCEEDED(rc)
                && !guestString.isEmpty())
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("GuestAdditionsVersion=\"%lS\"\n", guestString.raw());
                else
                    RTPrintf("Additions version:                   %lS\n\n", guestString.raw());
            }

            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("\nGuest Facilities:\n\n");

            /* Print information about known Guest Additions facilities: */
            SafeIfaceArray <IAdditionsFacility> collFac;
            CHECK_ERROR_RET(guest, COMGETTER(Facilities)(ComSafeArrayAsOutParam(collFac)), rc);
            LONG64 lLastUpdatedMS;
            char szLastUpdated[32];
            AdditionsFacilityStatus_T curStatus;
            for (size_t index = 0; index < collFac.size(); ++index)
            {
                ComPtr<IAdditionsFacility> fac = collFac[index];
                if (fac)
                {
                    CHECK_ERROR_RET(fac, COMGETTER(Name)(guestString.asOutParam()), rc);
                    if (!guestString.isEmpty())
                    {
                        CHECK_ERROR_RET(fac, COMGETTER(Status)(&curStatus), rc);
                        CHECK_ERROR_RET(fac, COMGETTER(LastUpdated)(&lLastUpdatedMS), rc);
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("GuestAdditionsFacility_%lS=%u,%lld\n",
                                     guestString.raw(), curStatus, lLastUpdatedMS);
                        else
                        {
                            makeTimeStr(szLastUpdated, sizeof(szLastUpdated), lLastUpdatedMS);
                            RTPrintf("Facility \"%lS\": %s (last update: %s)\n",
                                     guestString.raw(), facilityStateToName(curStatus, false /* No short naming */), szLastUpdated);
                        }
                    }
                    else
                        AssertMsgFailed(("Facility with undefined name retrieved!\n"));
                }
                else
                    AssertMsgFailed(("Invalid facility returned!\n"));
            }
            if (!collFac.size() && details != VMINFO_MACHINEREADABLE)
                RTPrintf("No active facilities.\n");
        }
    }

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");

    /*
     * snapshots
     */
    ComPtr<ISnapshot> snapshot;
    rc = machine->FindSnapshot(Bstr().raw(), snapshot.asOutParam());
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
    { "--machinereadable",  'M', RTGETOPT_REQ_NOTHING },
    { "-machinereadable",   'M', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--log",              'l', RTGETOPT_REQ_UINT32 },
};

int handleShowVMInfo(HandlerArg *a)
{
    HRESULT rc;
    const char *VMNameOrUuid = NULL;
    bool fLog = false;
    uint32_t uLogIdx = 0;
    bool fDetails = false;
    bool fMachinereadable = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aShowVMInfoOptions, RT_ELEMENTS(g_aShowVMInfoOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'D':   // --details
                fDetails = true;
                break;

            case 'M':   // --machinereadable
                fMachinereadable = true;
                break;

            case 'l':   // --log
                fLog = true;
                uLogIdx = ValueUnion.u32;
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
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(VMNameOrUuid).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return 1;

    /* Printing the log is exclusive. */
    if (fLog && (fMachinereadable || fDetails))
        return errorSyntax(USAGE_SHOWVMINFO, "Option --log is exclusive");

    if (fLog)
    {
        ULONG64 uOffset = 0;
        SafeArray<BYTE> aLogData;
        ULONG cbLogData;
        while (true)
        {
            /* Reset the array */
            aLogData.setNull();
            /* Fetch a chunk of the log file */
            CHECK_ERROR_BREAK(machine, ReadLog(uLogIdx, uOffset, _1M,
                                               ComSafeArrayAsOutParam(aLogData)));
            cbLogData = aLogData.size();
            if (cbLogData == 0)
                break;
            /* aLogData has a platform dependent line ending, standardize on
             * Unix style, as RTStrmWrite does the LF -> CR/LF replacement on
             * Windows. Otherwise we end up with CR/CR/LF on Windows. */
            ULONG cbLogDataPrint = cbLogData;
            for (BYTE *s = aLogData.raw(), *d = s;
                 s - aLogData.raw() < (ssize_t)cbLogData;
                 s++, d++)
            {
                if (*s == '\r')
                {
                    /* skip over CR, adjust destination */
                    d--;
                    cbLogDataPrint--;
                }
                else if (s != d)
                    *d = *s;
            }
            RTStrmWrite(g_pStdOut, aLogData.raw(), cbLogDataPrint);
            uOffset += cbLogData;
        }
    }
    else
    {
        /* 2nd option can be -details or -argdump */
        VMINFO_DETAILS details = VMINFO_NONE;
        if (fMachinereadable)
            details = VMINFO_MACHINEREADABLE;
        else if (fDetails)
            details = VMINFO_FULL;
        else
            details = VMINFO_STANDARD;

        ComPtr<IConsole> console;

        /* open an existing session for the VM */
        rc = machine->LockMachine(a->session, LockType_Shared);
        if (SUCCEEDED(rc))
            /* get the session machine */
            rc = a->session->COMGETTER(Machine)(machine.asOutParam());
        if (SUCCEEDED(rc))
            /* get the session console */
            rc = a->session->COMGETTER(Console)(console.asOutParam());

        rc = showVMInfo(a->virtualBox, machine, details, console);

        if (console)
            a->session->UnlockMachine();
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
