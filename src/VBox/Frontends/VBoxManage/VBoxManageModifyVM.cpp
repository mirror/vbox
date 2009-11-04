/* $Id$ */
/** @file
 * VBoxManage - Implementation of modifyvm command.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <vector>
#include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/getopt.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;


/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

enum
{
    MODIFYVM_NAME = 1000,
    MODIFYVM_OSTYPE,
    MODIFYVM_MEMORY,
    MODIFYVM_VRAM,
    MODIFYVM_FIRMWARE,
    MODIFYVM_ACPI,
    MODIFYVM_IOAPIC,
    MODIFYVM_PAE,
    MODIFYVM_SYNTHCPU,
    MODIFYVM_HWVIRTEX,
    MODIFYVM_HWVIRTEXEXCLUSIVE,
    MODIFYVM_NESTEDPAGING,
    MODIFYVM_VTXVPID,
    MODIFYVM_CPUS,
    MODIFYVM_SETCPUID,
    MODIFYVM_DELCPUID,
    MODIFYVM_DELALLCPUID,
    MODIFYVM_MONITORCOUNT,
    MODIFYVM_ACCELERATE3D,
    MODIFYVM_ACCELERATE2DVIDEO,
    MODIFYVM_BIOSLOGOFADEIN,
    MODIFYVM_BIOSLOGOFADEOUT,
    MODIFYVM_BIOSLOGODISPLAYTIME,
    MODIFYVM_BIOSLOGOIMAGEPATH,
    MODIFYVM_BIOSBOOTMENU,
    MODIFYVM_BIOSSYSTEMTIMEOFFSET,
    MODIFYVM_BIOSPXEDEBUG,
    MODIFYVM_BOOT,
    MODIFYVM_HDA,                // deprecated
    MODIFYVM_HDB,                // deprecated
    MODIFYVM_HDD,                // deprecated
    MODIFYVM_IDECONTROLLER,      // deprecated
    MODIFYVM_SATAIDEEMULATION,   // deprecated
    MODIFYVM_SATAPORTCOUNT,      // deprecated
    MODIFYVM_SATAPORT,           // deprecated
    MODIFYVM_SATA,               // deprecated
    MODIFYVM_SCSIPORT,           // deprecated
    MODIFYVM_SCSITYPE,           // deprecated
    MODIFYVM_SCSI,               // deprecated
    MODIFYVM_DVDPASSTHROUGH,     // deprecated
    MODIFYVM_DVD,                // deprecated
    MODIFYVM_FLOPPY,             // deprecated
    MODIFYVM_NICTRACEFILE,
    MODIFYVM_NICTRACE,
    MODIFYVM_NICTYPE,
    MODIFYVM_NICSPEED,
    MODIFYVM_NIC,
    MODIFYVM_CABLECONNECTED,
    MODIFYVM_BRIDGEADAPTER,
    MODIFYVM_HOSTONLYADAPTER,
    MODIFYVM_INTNET,
    MODIFYVM_NATNET,
    MODIFYVM_MACADDRESS,
    MODIFYVM_UARTMODE,
    MODIFYVM_UART,
    MODIFYVM_GUESTSTATISTICSINTERVAL,
    MODIFYVM_GUESTMEMORYBALLOON,
    MODIFYVM_AUDIOCONTROLLER,
    MODIFYVM_AUDIO,
    MODIFYVM_CLIPBOARD,
    MODIFYVM_VRDPPORT,
    MODIFYVM_VRDPADDRESS,
    MODIFYVM_VRDPAUTHTYPE,
    MODIFYVM_VRDPMULTICON,
    MODIFYVM_VRDPREUSECON,
    MODIFYVM_VRDP,
    MODIFYVM_USBEHCI,
    MODIFYVM_USB,
    MODIFYVM_SNAPSHOTFOLDER,
    MODIFYVM_TELEPORTER_ENABLED,
    MODIFYVM_TELEPORTER_PORT,
    MODIFYVM_TELEPORTER_ADDRESS,
    MODIFYVM_TELEPORTER_PASSWORD,
    MODIFYVM_HARDWARE_UUID
};

static const RTGETOPTDEF g_aModifyVMOptions[] =
{
    { "--name",                     MODIFYVM_NAME,                      RTGETOPT_REQ_STRING },
    { "--ostype",                   MODIFYVM_OSTYPE,                    RTGETOPT_REQ_STRING },
    { "--memory",                   MODIFYVM_MEMORY,                    RTGETOPT_REQ_UINT32 },
    { "--vram",                     MODIFYVM_VRAM,                      RTGETOPT_REQ_UINT32 },
    { "--firmware",                 MODIFYVM_FIRMWARE,                  RTGETOPT_REQ_STRING },
    { "--acpi",                     MODIFYVM_ACPI,                      RTGETOPT_REQ_BOOL_ONOFF },
    { "--ioapic",                   MODIFYVM_IOAPIC,                    RTGETOPT_REQ_BOOL_ONOFF },
    { "--pae",                      MODIFYVM_PAE,                       RTGETOPT_REQ_BOOL_ONOFF },
    { "--synthcpu",                 MODIFYVM_SYNTHCPU,                  RTGETOPT_REQ_BOOL_ONOFF },
    { "--hwvirtex",                 MODIFYVM_HWVIRTEX,                  RTGETOPT_REQ_BOOL_ONOFF },
    { "--hwvirtexexcl",             MODIFYVM_HWVIRTEXEXCLUSIVE,         RTGETOPT_REQ_BOOL_ONOFF },
    { "--nestedpaging",             MODIFYVM_NESTEDPAGING,              RTGETOPT_REQ_BOOL_ONOFF },
    { "--vtxvpid",                  MODIFYVM_VTXVPID,                   RTGETOPT_REQ_BOOL_ONOFF },
    { "--cpuidset",                 MODIFYVM_SETCPUID,                  RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_HEX},
    { "--cpuidremove",              MODIFYVM_DELCPUID,                  RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_HEX},
    { "--cpuidremoveall",           MODIFYVM_DELALLCPUID,               RTGETOPT_REQ_NOTHING},
    { "--cpus",                     MODIFYVM_CPUS,                      RTGETOPT_REQ_UINT32 },
    { "--monitorcount",             MODIFYVM_MONITORCOUNT,              RTGETOPT_REQ_UINT32 },
    { "--accelerate3d",             MODIFYVM_ACCELERATE3D,              RTGETOPT_REQ_BOOL_ONOFF },
    { "--accelerate2dvideo",        MODIFYVM_ACCELERATE2DVIDEO,         RTGETOPT_REQ_BOOL_ONOFF },
    { "--bioslogofadein",           MODIFYVM_BIOSLOGOFADEIN,            RTGETOPT_REQ_BOOL_ONOFF },
    { "--bioslogofadeout",          MODIFYVM_BIOSLOGOFADEOUT,           RTGETOPT_REQ_BOOL_ONOFF },
    { "--bioslogodisplaytime",      MODIFYVM_BIOSLOGODISPLAYTIME,       RTGETOPT_REQ_UINT64 },
    { "--bioslogoimagepath",        MODIFYVM_BIOSLOGOIMAGEPATH,         RTGETOPT_REQ_STRING },
    { "--biosbootmenu",             MODIFYVM_BIOSBOOTMENU,              RTGETOPT_REQ_STRING },
    { "--biossystemtimeoffset",     MODIFYVM_BIOSSYSTEMTIMEOFFSET,      RTGETOPT_REQ_UINT64 },
    { "--biospxedebug",             MODIFYVM_BIOSPXEDEBUG,              RTGETOPT_REQ_BOOL_ONOFF },
    { "--boot",                     MODIFYVM_BOOT,                      RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--hda",                      MODIFYVM_HDA,                       RTGETOPT_REQ_STRING },
    { "--hdb",                      MODIFYVM_HDB,                       RTGETOPT_REQ_STRING },
    { "--hdd",                      MODIFYVM_HDD,                       RTGETOPT_REQ_STRING },
    { "--idecontroller",            MODIFYVM_IDECONTROLLER,             RTGETOPT_REQ_STRING },
    { "--sataideemulation",         MODIFYVM_SATAIDEEMULATION,          RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
    { "--sataportcount",            MODIFYVM_SATAPORTCOUNT,             RTGETOPT_REQ_UINT32 },
    { "--sataport",                 MODIFYVM_SATAPORT,                  RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--sata",                     MODIFYVM_SATA,                      RTGETOPT_REQ_STRING },
    { "--scsiport",                 MODIFYVM_SCSIPORT,                  RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--scsitype",                 MODIFYVM_SCSITYPE,                  RTGETOPT_REQ_STRING },
    { "--scsi",                     MODIFYVM_SCSI,                      RTGETOPT_REQ_STRING },
    { "--dvdpassthrough",           MODIFYVM_DVDPASSTHROUGH,            RTGETOPT_REQ_STRING },
    { "--dvd",                      MODIFYVM_DVD,                       RTGETOPT_REQ_STRING },
    { "--floppy",                   MODIFYVM_FLOPPY,                    RTGETOPT_REQ_STRING },
    { "--nictracefile",             MODIFYVM_NICTRACEFILE,              RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--nictrace",                 MODIFYVM_NICTRACE,                  RTGETOPT_REQ_BOOL_ONOFF | RTGETOPT_FLAG_INDEX },
    { "--nictype",                  MODIFYVM_NICTYPE,                   RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--nicspeed",                 MODIFYVM_NICSPEED,                  RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
    { "--nic",                      MODIFYVM_NIC,                       RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--cableconnected",           MODIFYVM_CABLECONNECTED,            RTGETOPT_REQ_BOOL_ONOFF | RTGETOPT_FLAG_INDEX },
    { "--bridgeadapter",            MODIFYVM_BRIDGEADAPTER,             RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--hostonlyadapter",          MODIFYVM_HOSTONLYADAPTER,           RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--intnet",                   MODIFYVM_INTNET,                    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--natnet",                   MODIFYVM_NATNET,                    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--macaddress",               MODIFYVM_MACADDRESS,                RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--uartmode",                 MODIFYVM_UARTMODE,                  RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--uart",                     MODIFYVM_UART,                      RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--gueststatisticsinterval",  MODIFYVM_GUESTSTATISTICSINTERVAL,   RTGETOPT_REQ_UINT32 },
    { "--guestmemoryballoon",       MODIFYVM_GUESTMEMORYBALLOON,        RTGETOPT_REQ_UINT32 },
    { "--audiocontroller",          MODIFYVM_AUDIOCONTROLLER,           RTGETOPT_REQ_STRING },
    { "--audio",                    MODIFYVM_AUDIO,                     RTGETOPT_REQ_STRING },
    { "--clipboard",                MODIFYVM_CLIPBOARD,                 RTGETOPT_REQ_STRING },
    { "--vrdpport",                 MODIFYVM_VRDPPORT,                  RTGETOPT_REQ_STRING },
    { "--vrdpaddress",              MODIFYVM_VRDPADDRESS,               RTGETOPT_REQ_STRING },
    { "--vrdpauthtype",             MODIFYVM_VRDPAUTHTYPE,              RTGETOPT_REQ_STRING },
    { "--vrdpmulticon",             MODIFYVM_VRDPMULTICON,              RTGETOPT_REQ_BOOL_ONOFF },
    { "--vrdpreusecon",             MODIFYVM_VRDPREUSECON,              RTGETOPT_REQ_BOOL_ONOFF },
    { "--vrdp",                     MODIFYVM_VRDP,                      RTGETOPT_REQ_BOOL_ONOFF },
    { "--usbehci",                  MODIFYVM_USBEHCI,                   RTGETOPT_REQ_BOOL_ONOFF },
    { "--usb",                      MODIFYVM_USB,                       RTGETOPT_REQ_BOOL_ONOFF },
    { "--snapshotfolder",           MODIFYVM_SNAPSHOTFOLDER,            RTGETOPT_REQ_STRING },
    { "--teleporterenabled",        MODIFYVM_TELEPORTER_ENABLED,        RTGETOPT_REQ_BOOL_ONOFF },
    { "--teleporterport",           MODIFYVM_TELEPORTER_PORT,           RTGETOPT_REQ_UINT32 },
    { "--teleporteraddress",        MODIFYVM_TELEPORTER_ADDRESS,        RTGETOPT_REQ_STRING },
    { "--teleporterpassword",       MODIFYVM_TELEPORTER_PASSWORD,       RTGETOPT_REQ_STRING },
    { "--hardwareuuid",             MODIFYVM_HARDWARE_UUID,             RTGETOPT_REQ_STRING },
};

int handleModifyVM(HandlerArg *a)
{
    int c;
    HRESULT rc;
    Bstr name;
    Bstr machineuuid (a->argv[0]);
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetOptState;
    ComPtr <IMachine> machine;
    ComPtr <IBIOSSettings> biosSettings;

    /* VM ID + at least one parameter. Parameter arguments are checked
     * individually. */
    if (a->argc < 2)
        return errorSyntax(USAGE_MODIFYVM, "Not enough parameters");

    /* Get the number of network adapters */
    ULONG NetworkAdapterCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()), 1);
        CHECK_ERROR_RET(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount), 1);
    }
    ULONG SerialPortCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()), 1);
        CHECK_ERROR_RET(info, COMGETTER(SerialPortCount)(&SerialPortCount), 1);
    }

    /* try to find the given machine */
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR_RET(a->virtualBox, GetMachine(machineuuid, machine.asOutParam()), 1);
    }
    else
    {
        CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()), 1);
        machine->COMGETTER(Id)(machineuuid.asOutParam());
    }

    /* open a session for the VM */
    CHECK_ERROR_RET(a->virtualBox, OpenSession(a->session, machineuuid), 1);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());
    machine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());

    RTGetOptInit(&GetOptState, a->argc, a->argv, g_aModifyVMOptions,
                 RT_ELEMENTS(g_aModifyVMOptions), 1, 0 /* fFlags */);

    while (   SUCCEEDED (rc)
           && (c = RTGetOpt(&GetOptState, &ValueUnion)))
    {
        switch (c)
        {
            case MODIFYVM_NAME:
            {
                CHECK_ERROR(machine, COMSETTER(Name)(Bstr(ValueUnion.psz)));
                break;
            }
            case MODIFYVM_OSTYPE:
            {
                ComPtr<IGuestOSType> guestOSType;
                CHECK_ERROR(a->virtualBox, GetGuestOSType(Bstr(ValueUnion.psz), guestOSType.asOutParam()));
                if (SUCCEEDED(rc) && guestOSType)
                {
                    CHECK_ERROR(machine, COMSETTER(OSTypeId)(Bstr(ValueUnion.psz)));
                }
                else
                {
                    errorArgument("Invalid guest OS type '%s'", Utf8Str(ValueUnion.psz).raw());
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_MEMORY:
            {
                CHECK_ERROR(machine, COMSETTER(MemorySize)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_VRAM:
            {
                CHECK_ERROR(machine, COMSETTER(VRAMSize)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_FIRMWARE:
            {
                if (!strcmp(ValueUnion.psz, "efi"))
                {
                    CHECK_ERROR(machine, COMSETTER(FirmwareType)(FirmwareType_EFI));
                }
                else if (!strcmp(ValueUnion.psz, "bios"))
                {
                    CHECK_ERROR(machine, COMSETTER(FirmwareType)(FirmwareType_BIOS));
                }
                else
                {
                    errorArgument("Invalid --firmware argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_ACPI:
            {
                CHECK_ERROR(biosSettings, COMSETTER(ACPIEnabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_IOAPIC:
            {
                CHECK_ERROR(biosSettings, COMSETTER(IOAPICEnabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_PAE:
            {
                CHECK_ERROR(machine, SetCpuProperty(CpuPropertyType_PAE, ValueUnion.f));
                break;
            }

            case MODIFYVM_SYNTHCPU:
            {
                CHECK_ERROR(machine, SetCpuProperty(CpuPropertyType_Synthetic, ValueUnion.f));
                break;
            }

            case MODIFYVM_HWVIRTEX:
            {
                CHECK_ERROR(machine, SetHWVirtExProperty(HWVirtExPropertyType_Enabled, ValueUnion.f));
                break;
            }

            case MODIFYVM_HWVIRTEXEXCLUSIVE:
            {
                CHECK_ERROR(machine, SetHWVirtExProperty(HWVirtExPropertyType_Exclusive, ValueUnion.f));
                break;
            }

            case MODIFYVM_SETCPUID:
            {
                uint32_t id = ValueUnion.u32;
                uint32_t aValue[4];

                for (unsigned i = 0 ; i < 4 ; i++)
                {
                    int vrc = RTGetOptFetchValue(&GetOptState, &ValueUnion, RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_HEX);
                    if (RT_FAILURE(vrc))
                        return errorSyntax(USAGE_MODIFYVM,
                                           "Missing or Invalid argument to '%s'",
                                           GetOptState.pDef->pszLong);
                    aValue[i] = ValueUnion.u32;
                }
                CHECK_ERROR(machine, SetCpuIdLeaf(id, aValue[0], aValue[1], aValue[2], aValue[3]));
                break;
            }

            case MODIFYVM_DELCPUID:
            {
                CHECK_ERROR(machine, RemoveCpuIdLeaf(ValueUnion.u32));
                break;
            }

            case MODIFYVM_DELALLCPUID:
            {
                CHECK_ERROR(machine, RemoveAllCpuIdLeafs());
                break;
            }

            case MODIFYVM_NESTEDPAGING:
            {
                CHECK_ERROR(machine, SetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, ValueUnion.f));
                break;
            }

            case MODIFYVM_VTXVPID:
            {
                CHECK_ERROR(machine, SetHWVirtExProperty(HWVirtExPropertyType_VPID, ValueUnion.f));
                break;
            }

            case MODIFYVM_CPUS:
            {
                CHECK_ERROR(machine, COMSETTER(CPUCount)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_MONITORCOUNT:
            {
                CHECK_ERROR(machine, COMSETTER(MonitorCount)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_ACCELERATE3D:
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate3DEnabled)(ValueUnion.f));
                break;
            }

#ifdef VBOX_WITH_VIDEOHWACCEL
            case MODIFYVM_ACCELERATE2DVIDEO:
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate2DVideoEnabled)(ValueUnion.f));
                break;
            }
#endif

            case MODIFYVM_BIOSLOGOFADEIN:
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeIn)(ValueUnion.f));
                break;
            }

            case MODIFYVM_BIOSLOGOFADEOUT:
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeOut)(ValueUnion.f));
                break;
            }

            case MODIFYVM_BIOSLOGODISPLAYTIME:
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoDisplayTime)(ValueUnion.u64));
                break;
            }

            case MODIFYVM_BIOSLOGOIMAGEPATH:
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoImagePath)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_BIOSBOOTMENU:
            {
                if (!strcmp(ValueUnion.psz, "disabled"))
                {
                    CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_Disabled));
                }
                else if (!strcmp(ValueUnion.psz, "menuonly"))
                {
                    CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MenuOnly));
                }
                else if (!strcmp(ValueUnion.psz, "messageandmenu"))
                {
                    CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MessageAndMenu));
                }
                else
                {
                    errorArgument("Invalid --biosbootmenu argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_BIOSSYSTEMTIMEOFFSET:
            {
                CHECK_ERROR(biosSettings, COMSETTER(TimeOffset)(ValueUnion.u64));
                break;
            }

            case MODIFYVM_BIOSPXEDEBUG:
            {
                CHECK_ERROR(biosSettings, COMSETTER(PXEDebugEnabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_BOOT:
            {
                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > 4))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid boot slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                if (!strcmp(ValueUnion.psz, "none"))
                {
                    CHECK_ERROR(machine, SetBootOrder(GetOptState.uIndex, DeviceType_Null));
                }
                else if (!strcmp(ValueUnion.psz, "floppy"))
                {
                    CHECK_ERROR(machine, SetBootOrder(GetOptState.uIndex, DeviceType_Floppy));
                }
                else if (!strcmp(ValueUnion.psz, "dvd"))
                {
                    CHECK_ERROR(machine, SetBootOrder(GetOptState.uIndex, DeviceType_DVD));
                }
                else if (!strcmp(ValueUnion.psz, "disk"))
                {
                    CHECK_ERROR(machine, SetBootOrder(GetOptState.uIndex, DeviceType_HardDisk));
                }
                else if (!strcmp(ValueUnion.psz, "net"))
                {
                    CHECK_ERROR(machine, SetBootOrder(GetOptState.uIndex, DeviceType_Network));
                }
                else
                    return errorArgument("Invalid boot device '%s'", ValueUnion.psz);

                break;
            }

            case MODIFYVM_HDA: // deprecated
            {
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 0, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(ValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(ValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox,
                                         OpenHardDisk(Bstr(ValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine, AttachDevice(Bstr("IDE Controller"), 0, 0, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_HDB: // deprecated
            {
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 0, 1);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(ValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(ValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox,
                                         OpenHardDisk(Bstr(ValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine, AttachDevice(Bstr("IDE Controller"), 0, 1, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_HDD: // deprecated
            {
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 1, 1);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(ValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(ValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox,
                                         OpenHardDisk(Bstr(ValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine, AttachDevice(Bstr("IDE Controller"), 1, 1, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_IDECONTROLLER: // deprecated
            {
                ComPtr<IStorageController> storageController;
                CHECK_ERROR(machine, GetStorageControllerByName(Bstr("IDE Controller"),
                                                                 storageController.asOutParam()));

                if (!RTStrICmp(ValueUnion.psz, "PIIX3"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
                }
                else if (!RTStrICmp(ValueUnion.psz, "PIIX4"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
                }
                else if (!RTStrICmp(ValueUnion.psz, "ICH6"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_ICH6));
                }
                else
                {
                    errorArgument("Invalid --idecontroller argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SATAIDEEMULATION: // deprecated
            {
                ComPtr<IStorageController> SataCtl;
                CHECK_ERROR(machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > 4))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA boot slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                if ((ValueUnion.u32 < 1) && (ValueUnion.u32 > 30))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA port number in '%s'",
                                       GetOptState.pDef->pszLong);

                if (SUCCEEDED(rc))
                    CHECK_ERROR(SataCtl, SetIDEEmulationPort(GetOptState.uIndex, ValueUnion.u32));

                break;
            }

            case MODIFYVM_SATAPORTCOUNT: // deprecated
            {
                ComPtr<IStorageController> SataCtl;
                CHECK_ERROR(machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));

                if (SUCCEEDED(rc) && ValueUnion.u32 > 0)
                    CHECK_ERROR(SataCtl, COMSETTER(PortCount)(ValueUnion.u32));

                break;
            }

            case MODIFYVM_SATAPORT: // deprecated
            {
                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > 30))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA port number in '%s'",
                                       GetOptState.pDef->pszLong);

                if (!strcmp(ValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("SATA"), GetOptState.uIndex, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(ValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc =  a->virtualBox->FindHardDisk(Bstr(ValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox,
                                         OpenHardDisk(Bstr(ValueUnion.psz), AccessMode_ReadWrite,
                                                      false, Bstr(""), false,
                                                      Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine,
                                     AttachDevice(Bstr("SATA"), GetOptState.uIndex,
                                                  0, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SATA: // deprecated
            {
                if (!strcmp(ValueUnion.psz, "on") || !strcmp(ValueUnion.psz, "enable"))
                {
                    ComPtr<IStorageController> ctl;
                    CHECK_ERROR(machine, AddStorageController(Bstr("SATA"), StorageBus_SATA, ctl.asOutParam()));
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
                }
                else if (!strcmp(ValueUnion.psz, "off") || !strcmp(ValueUnion.psz, "disable"))
                    CHECK_ERROR(machine, RemoveStorageController(Bstr("SATA")));
                else
                    return errorArgument("Invalid --usb argument '%s'", ValueUnion.psz);
                break;
            }

            case MODIFYVM_SCSIPORT: // deprecated
            {
                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > 16))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SCSI port number in '%s'",
                                       GetOptState.pDef->pszLong);

                if (!strcmp(ValueUnion.psz, "none"))
                {
                    rc = machine->DetachDevice(Bstr("LsiLogic"), GetOptState.uIndex, 0);
                    if (FAILED(rc))
                        CHECK_ERROR(machine, DetachDevice(Bstr("BusLogic"), GetOptState.uIndex, 0));
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(ValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(ValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox,
                                         OpenHardDisk(Bstr(ValueUnion.psz),
                                                           AccessMode_ReadWrite, false, Bstr(""),
                                                           false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        rc = machine->AttachDevice(Bstr("LsiLogic"), GetOptState.uIndex, 0, DeviceType_HardDisk, uuid);
                        if (FAILED(rc))
                            CHECK_ERROR(machine,
                                         AttachDevice(Bstr("BusLogic"),
                                                      GetOptState.uIndex, 0,
                                                      DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SCSITYPE: // deprecated
            {
                ComPtr<IStorageController> ctl;

                if (!RTStrICmp(ValueUnion.psz, "LsiLogic"))
                {
                    rc = machine->RemoveStorageController(Bstr("BusLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR(machine, RemoveStorageController(Bstr("LsiLogic")));

                    CHECK_ERROR(machine,
                                 AddStorageController(Bstr("LsiLogic"),
                                                      StorageBus_SCSI,
                                                      ctl.asOutParam()));

                    if (SUCCEEDED(rc))
                        CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
                else if (!RTStrICmp(ValueUnion.psz, "BusLogic"))
                {
                    rc = machine->RemoveStorageController(Bstr("LsiLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR(machine, RemoveStorageController(Bstr("BusLogic")));

                    CHECK_ERROR(machine,
                                 AddStorageController(Bstr("BusLogic"),
                                                      StorageBus_SCSI,
                                                      ctl.asOutParam()));

                    if (SUCCEEDED(rc))
                        CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else
                    return errorArgument("Invalid --scsitype argument '%s'", ValueUnion.psz);
                break;
            }

            case MODIFYVM_SCSI: // deprecated
            {
                if (!strcmp(ValueUnion.psz, "on") || !strcmp(ValueUnion.psz, "enable"))
                {
                    ComPtr<IStorageController> ctl;

                    CHECK_ERROR(machine, AddStorageController(Bstr("BusLogic"), StorageBus_SCSI, ctl.asOutParam()));
                    if (SUCCEEDED(rc))
                        CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else if (!strcmp(ValueUnion.psz, "off") || !strcmp(ValueUnion.psz, "disable"))
                {
                    rc = machine->RemoveStorageController(Bstr("BusLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR(machine, RemoveStorageController(Bstr("LsiLogic")));
                }
                break;
            }

            case MODIFYVM_DVDPASSTHROUGH: // deprecated
            {
                CHECK_ERROR(machine, PassthroughDevice(Bstr("IDE Controller"), 1, 0, !strcmp(ValueUnion.psz, "on")));
                break;
            }

            case MODIFYVM_DVD: // deprecated
            {
                ComPtr<IMedium> dvdMedium;
                Bstr uuid(ValueUnion.psz);

                /* unmount? */
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    /* nothing to do, NULL object will cause unmount */
                }
                /* host drive? */
                else if (!strncmp(ValueUnion.psz, "host:", 5))
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    rc = host->FindHostDVDDrive(Bstr(ValueUnion.psz + 5), dvdMedium.asOutParam());
                    if (!dvdMedium)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(ValueUnion.psz + 5, szPathReal, sizeof(szPathReal))))
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", ValueUnion.psz + 5);
                            rc = E_FAIL;
                            break;
                        }
                        rc = host->FindHostDVDDrive(Bstr(szPathReal), dvdMedium.asOutParam());
                        if (!dvdMedium)
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", ValueUnion.psz + 5);
                            rc = E_FAIL;
                            break;
                        }
                    }
                }
                else
                {
                    /* first assume it's a UUID */
                    rc = a->virtualBox->GetDVDImage(uuid, dvdMedium.asOutParam());
                    if (FAILED(rc) || !dvdMedium)
                    {
                        /* must be a filename, check if it's in the collection */
                        rc = a->virtualBox->FindDVDImage(Bstr(ValueUnion.psz), dvdMedium.asOutParam());
                        /* not registered, do that on the fly */
                        if (!dvdMedium)
                        {
                            Bstr emptyUUID;
                            CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(ValueUnion.psz),
                                         emptyUUID, dvdMedium.asOutParam()));
                        }
                    }
                    if (!dvdMedium)
                    {
                        rc = E_FAIL;
                        break;
                    }
                }

                /** @todo generalize this, allow arbitrary number of DVD drives
                 * and as a consequence multiple attachments and different
                 * storage controllers. */
                if (dvdMedium)
                    dvdMedium->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(machine, MountMedium(Bstr("IDE Controller"), 1, 0, uuid));
                break;
            }

            case MODIFYVM_FLOPPY: // deprecated
            {
                Bstr uuid(ValueUnion.psz);
                ComPtr<IMedium> floppyMedium;
                ComPtr<IMediumAttachment> floppyAttachment;
                machine->GetMediumAttachment(Bstr("Floppy Controller"), 0, 0, floppyAttachment.asOutParam());

                /* disable? */
                if (!strcmp(ValueUnion.psz, "disabled"))
                {
                    /* disable the controller */
                    if (floppyAttachment)
                        CHECK_ERROR(machine, DetachDevice(Bstr("Floppy Controller"), 0, 0));
                }
                else
                {
                    /* enable the controller */
                    if (!floppyAttachment)
                        CHECK_ERROR(machine, AttachDevice(Bstr("Floppy Controller"), 0, 0, DeviceType_Floppy, NULL));

                    /* unmount? */
                    if (    !strcmp(ValueUnion.psz, "none")
                        ||  !strcmp(ValueUnion.psz, "empty"))   // deprecated
                    {
                        /* nothing to do, NULL object will cause unmount */
                    }
                    /* host drive? */
                    else if (!strncmp(ValueUnion.psz, "host:", 5))
                    {
                        ComPtr<IHost> host;
                        CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                        rc = host->FindHostFloppyDrive(Bstr(ValueUnion.psz + 5), floppyMedium.asOutParam());
                        if (!floppyMedium)
                        {
                            errorArgument("Invalid host floppy drive name \"%s\"", ValueUnion.psz + 5);
                            rc = E_FAIL;
                            break;
                        }
                    }
                    else
                    {
                        /* first assume it's a UUID */
                        rc = a->virtualBox->GetFloppyImage(uuid, floppyMedium.asOutParam());
                        if (FAILED(rc) || !floppyMedium)
                        {
                            /* must be a filename, check if it's in the collection */
                            rc = a->virtualBox->FindFloppyImage(Bstr(ValueUnion.psz), floppyMedium.asOutParam());
                            /* not registered, do that on the fly */
                            if (!floppyMedium)
                            {
                                Bstr emptyUUID;
                                CHECK_ERROR(a->virtualBox,
                                             OpenFloppyImage(Bstr(ValueUnion.psz),
                                                             emptyUUID,
                                                             floppyMedium.asOutParam()));
                            }
                        }
                        if (!floppyMedium)
                        {
                            rc = E_FAIL;
                            break;
                        }
                    }
                    floppyMedium->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, MountMedium(Bstr("Floppy Controller"), 0, 0, uuid));
                }
                break;
            }

            case MODIFYVM_NICTRACEFILE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR(nic, COMSETTER(TraceFile)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_NICTRACE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR(nic, COMSETTER(TraceEnabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_NICTYPE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(ValueUnion.psz, "Am79C970A"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C970A));
                }
                else if (!strcmp(ValueUnion.psz, "Am79C973"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C973));
                }
#ifdef VBOX_WITH_E1000
                else if (!strcmp(ValueUnion.psz, "82540EM"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82540EM));
                }
                else if (!strcmp(ValueUnion.psz, "82543GC"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82543GC));
                }
                else if (!strcmp(ValueUnion.psz, "82545EM"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82545EM));
                }
#endif
#ifdef VBOX_WITH_VIRTIO
                else if (!strcmp(ValueUnion.psz, "virtio"))
                {
                    CHECK_ERROR(nic, COMSETTER(AdapterType)(NetworkAdapterType_Virtio));
                }
#endif /* VBOX_WITH_VIRTIO */
                else
                {
                    errorArgument("Invalid NIC type '%s' specified for NIC %lu", ValueUnion.psz, GetOptState.uIndex);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_NICSPEED:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                if ((ValueUnion.u32 < 1000) && (ValueUnion.u32 > 4000000))
                {
                    errorArgument("Invalid --nicspeed%lu argument '%u'", GetOptState.uIndex, ValueUnion.u32);
                    rc = E_FAIL;
                    break;
                }

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR(nic, COMSETTER(LineSpeed)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_NIC:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(ValueUnion.psz, "none"))
                {
                    CHECK_ERROR(nic, COMSETTER(Enabled)(FALSE));
                }
                else if (!strcmp(ValueUnion.psz, "null"))
                {
                    CHECK_ERROR(nic, COMSETTER(Enabled)(TRUE));
                    CHECK_ERROR(nic, Detach());
                }
                else if (!strcmp(ValueUnion.psz, "nat"))
                {
                    CHECK_ERROR(nic, COMSETTER(Enabled)(TRUE));
                    CHECK_ERROR(nic, AttachToNAT());
                }
                else if (  !strcmp(ValueUnion.psz, "bridged")
                        || !strcmp(ValueUnion.psz, "hostif")) /* backward compatibility */
                {
                    CHECK_ERROR(nic, COMSETTER(Enabled)(TRUE));
                    CHECK_ERROR(nic, AttachToBridgedInterface());
                }
                else if (!strcmp(ValueUnion.psz, "intnet"))
                {
                    CHECK_ERROR(nic, COMSETTER(Enabled)(TRUE));
                    CHECK_ERROR(nic, AttachToInternalNetwork());
                }
#if defined(VBOX_WITH_NETFLT)
                else if (!strcmp(ValueUnion.psz, "hostonly"))
                {

                    CHECK_ERROR(nic, COMSETTER(Enabled)(TRUE));
                    CHECK_ERROR(nic, AttachToHostOnlyInterface());
                }
#endif
                else
                {
                    errorArgument("Invalid type '%s' specfied for NIC %lu", ValueUnion.psz, GetOptState.uIndex);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_CABLECONNECTED:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR(nic, COMSETTER(CableConnected)(ValueUnion.f));
                break;
            }

            case MODIFYVM_BRIDGEADAPTER:
            case MODIFYVM_HOSTONLYADAPTER:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* remove it? */
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    CHECK_ERROR(nic, COMSETTER(HostInterface)(NULL));
                }
                else
                {
                    CHECK_ERROR(nic, COMSETTER(HostInterface)(Bstr(ValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_INTNET:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* remove it? */
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    CHECK_ERROR(nic, COMSETTER(InternalNetwork)(NULL));
                }
                else
                {
                    CHECK_ERROR(nic, COMSETTER(InternalNetwork)(Bstr(ValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_NATNET:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR(nic, COMSETTER(NATNetwork)(Bstr(ValueUnion.psz)));

                break;
            }

            case MODIFYVM_MACADDRESS:
            {
                ComPtr<INetworkAdapter> nic;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetNetworkAdapter(GetOptState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* generate one? */
                if (!strcmp(ValueUnion.psz, "auto"))
                {
                    CHECK_ERROR(nic, COMSETTER(MACAddress)(NULL));
                }
                else
                {
                    CHECK_ERROR(nic, COMSETTER(MACAddress)(Bstr(ValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_UARTMODE:
            {
                ComPtr<ISerialPort> uart;
                char *pszIRQ = NULL;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > SerialPortCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid Serial Port number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetSerialPort(GetOptState.uIndex - 1, uart.asOutParam()));
                ASSERT(uart);

                if (!strcmp(ValueUnion.psz, "disconnected"))
                {
                    CHECK_ERROR(uart, COMSETTER(HostMode)(PortMode_Disconnected));
                }
                else if (   !strcmp(ValueUnion.psz, "server")
                         || !strcmp(ValueUnion.psz, "client")
                         || !strcmp(ValueUnion.psz, "file"))
                {
                    const char *pszMode = ValueUnion.psz;

                    int vrc = RTGetOptFetchValue(&GetOptState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_FAILURE(vrc))
                        return errorSyntax(USAGE_MODIFYVM,
                                           "Missing or Invalid argument to '%s'",
                                           GetOptState.pDef->pszLong);

                    CHECK_ERROR(uart, COMSETTER(Path)(Bstr(ValueUnion.psz)));

                    if (!strcmp(pszMode, "server"))
                    {
                        CHECK_ERROR(uart, COMSETTER(HostMode)(PortMode_HostPipe));
                        CHECK_ERROR(uart, COMSETTER(Server)(TRUE));
                    }
                    else if (!strcmp(pszMode, "client"))
                    {
                        CHECK_ERROR(uart, COMSETTER(HostMode)(PortMode_HostPipe));
                        CHECK_ERROR(uart, COMSETTER(Server)(FALSE));
                    }
                    else if (!strcmp(pszMode, "file"))
                    {
                        CHECK_ERROR(uart, COMSETTER(HostMode)(PortMode_RawFile));
                    }
                }
                else
                {
                    CHECK_ERROR(uart, COMSETTER(Path)(Bstr(ValueUnion.psz)));
                    CHECK_ERROR(uart, COMSETTER(HostMode)(PortMode_HostDevice));
                }
                break;
            }

            case MODIFYVM_UART:
            {
                ComPtr<ISerialPort> uart;

                if ((GetOptState.uIndex < 1) && (GetOptState.uIndex > SerialPortCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid Serial Port number in '%s'",
                                       GetOptState.pDef->pszLong);

                CHECK_ERROR_BREAK(machine, GetSerialPort(GetOptState.uIndex - 1, uart.asOutParam()));
                ASSERT(uart);

                if (!strcmp(ValueUnion.psz, "off") || !strcmp(ValueUnion.psz, "disable"))
                    CHECK_ERROR(uart, COMSETTER(Enabled)(FALSE));
                else
                {
                    const char *pszIOBase = ValueUnion.psz;
                    uint32_t uVal = 0;

                    int vrc = RTGetOptFetchValue(&GetOptState, &ValueUnion, RTGETOPT_REQ_UINT32) != MODIFYVM_UART;
                    if (RT_FAILURE(vrc))
                        return errorSyntax(USAGE_MODIFYVM,
                                           "Missing or Invalid argument to '%s'",
                                           GetOptState.pDef->pszLong);

                    CHECK_ERROR(uart, COMSETTER(IRQ)(ValueUnion.u32));

                    vrc = RTStrToUInt32Ex(pszIOBase, NULL, 0, &uVal);
                    if (vrc != VINF_SUCCESS || uVal == 0)
                        return errorArgument("Error parsing UART I/O base '%s'", pszIOBase);
                    CHECK_ERROR(uart, COMSETTER(IOBase)(uVal));

                    CHECK_ERROR(uart, COMSETTER(Enabled)(TRUE));
                }
                break;
            }

            case MODIFYVM_GUESTSTATISTICSINTERVAL:
            {
                CHECK_ERROR(machine, COMSETTER(StatisticsUpdateInterval)(ValueUnion.u32));
                break;
            }

#ifdef VBOX_WITH_MEM_BALLOONING
            case MODIFYVM_GUESTMEMORYBALLOON:
            {
                CHECK_ERROR(machine, COMSETTER(MemoryBalloonSize)(ValueUnion.u32));
                break;
            }
#endif

            case MODIFYVM_AUDIOCONTROLLER:
            {
                ComPtr<IAudioAdapter> audioAdapter;
                machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
                ASSERT(audioAdapter);

                if (!strcmp(ValueUnion.psz, "sb16"))
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_SB16));
                else if (!strcmp(ValueUnion.psz, "ac97"))
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_AC97));
                else
                {
                    errorArgument("Invalid --audiocontroller argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_AUDIO:
            {
                ComPtr<IAudioAdapter> audioAdapter;
                machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
                ASSERT(audioAdapter);

                /* disable? */
                if (!strcmp(ValueUnion.psz, "none"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(false));
                }
                else if (!strcmp(ValueUnion.psz, "null"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Null));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
                else if (!strcmp(ValueUnion.psz, "winmm"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_WinMM));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif
                else if (!strcmp(ValueUnion.psz, "dsound"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_DirectSound));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_LINUX
                else if (!strcmp(ValueUnion.psz, "oss"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# ifdef VBOX_WITH_ALSA
                else if (!strcmp(ValueUnion.psz, "alsa"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_ALSA));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
# ifdef VBOX_WITH_PULSE
                else if (!strcmp(ValueUnion.psz, "pulse"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Pulse));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
#endif /* !RT_OS_LINUX */
#ifdef RT_OS_SOLARIS
                else if (!strcmp(ValueUnion.psz, "solaudio"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_SolAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

# ifdef VBOX_WITH_SOLARIS_OSS
                else if (!strcmp(ValueUnion.psz, "oss"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif

#endif /* !RT_OS_SOLARIS */
#ifdef RT_OS_DARWIN
                else if (!strcmp(ValueUnion.psz, "coreaudio"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_CoreAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

#endif /* !RT_OS_DARWIN */
                else
                {
                    errorArgument("Invalid --audio argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_CLIPBOARD:
            {
                if (!strcmp(ValueUnion.psz, "disabled"))
                {
                    CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Disabled));
                }
                else if (!strcmp(ValueUnion.psz, "hosttoguest"))
                {
                    CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_HostToGuest));
                }
                else if (!strcmp(ValueUnion.psz, "guesttohost"))
                {
                    CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_GuestToHost));
                }
                else if (!strcmp(ValueUnion.psz, "bidirectional"))
                {
                    CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Bidirectional));
                }
                else
                {
                    errorArgument("Invalid --clipboard argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

#ifdef VBOX_WITH_VRDP
            case MODIFYVM_VRDPPORT:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                if (!strcmp(ValueUnion.psz, "default"))
                    CHECK_ERROR(vrdpServer, COMSETTER(Ports)(Bstr("0")));
                else
                    CHECK_ERROR(vrdpServer, COMSETTER(Ports)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_VRDPADDRESS:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                CHECK_ERROR(vrdpServer, COMSETTER(NetAddress)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_VRDPAUTHTYPE:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                if (!strcmp(ValueUnion.psz, "null"))
                {
                    CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Null));
                }
                else if (!strcmp(ValueUnion.psz, "external"))
                {
                    CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_External));
                }
                else if (!strcmp(ValueUnion.psz, "guest"))
                {
                    CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Guest));
                }
                else
                {
                    errorArgument("Invalid --vrdpauthtype argument '%s'", ValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_VRDPMULTICON:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                CHECK_ERROR(vrdpServer, COMSETTER(AllowMultiConnection)(ValueUnion.f));
                break;
            }

            case MODIFYVM_VRDPREUSECON:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                CHECK_ERROR(vrdpServer, COMSETTER(ReuseSingleConnection)(ValueUnion.f));
                break;
            }

            case MODIFYVM_VRDP:
            {
                ComPtr<IVRDPServer> vrdpServer;
                machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                ASSERT(vrdpServer);

                CHECK_ERROR(vrdpServer, COMSETTER(Enabled)(ValueUnion.f));
                break;
            }
#endif /* VBOX_WITH_VRDP */

            case MODIFYVM_USBEHCI:
            {
                ComPtr<IUSBController> UsbCtl;
                CHECK_ERROR(machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
                if (SUCCEEDED(rc))
                    CHECK_ERROR(UsbCtl, COMSETTER(EnabledEhci)(ValueUnion.f));
                break;
            }

            case MODIFYVM_USB:
            {
                ComPtr<IUSBController> UsbCtl;
                CHECK_ERROR(machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
                if (SUCCEEDED(rc))
                    CHECK_ERROR(UsbCtl, COMSETTER(Enabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_SNAPSHOTFOLDER:
            {
                if (!strcmp(ValueUnion.psz, "default"))
                    CHECK_ERROR(machine, COMSETTER(SnapshotFolder)(NULL));
                else
                    CHECK_ERROR(machine, COMSETTER(SnapshotFolder)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_TELEPORTER_ENABLED:
            {
                CHECK_ERROR(machine, COMSETTER(TeleporterEnabled)(ValueUnion.f));
                break;
            }

            case MODIFYVM_TELEPORTER_PORT:
            {
                CHECK_ERROR(machine, COMSETTER(TeleporterPort)(ValueUnion.u32));
                break;
            }

            case MODIFYVM_TELEPORTER_ADDRESS:
            {
                CHECK_ERROR(machine, COMSETTER(TeleporterAddress)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_TELEPORTER_PASSWORD:
            {
                CHECK_ERROR(machine, COMSETTER(TeleporterPassword)(Bstr(ValueUnion.psz)));
                break;
            }

            case MODIFYVM_HARDWARE_UUID:
            {
                CHECK_ERROR(machine, COMSETTER(HardwareUUID)(Bstr(ValueUnion.psz)));
                break;
            }

            default:
            {
                errorGetOpt(USAGE_MODIFYVM, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
