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
    MODIFYVM_HDA,
    MODIFYVM_HDB,
    MODIFYVM_HDD,
    MODIFYVM_IDECONTROLLER,
    MODIFYVM_SATAIDEEMULATION,
    MODIFYVM_SATAPORTCOUNT,
    MODIFYVM_SATAPORT,
    MODIFYVM_SATA,
    MODIFYVM_SCSIPORT,
    MODIFYVM_SCSITYPE,
    MODIFYVM_SCSI,
    MODIFYVM_DVDPASSTHROUGH,
    MODIFYVM_DVD,
    MODIFYVM_FLOPPY,
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
    MODIFYVM_TELEPORTER_PASSWORD
};

static const RTGETOPTDEF g_aModifyVMOptions[] =
{
    { "--name",                     MODIFYVM_NAME,                      RTGETOPT_REQ_STRING },
    { "--ostype",                   MODIFYVM_OSTYPE,                    RTGETOPT_REQ_STRING },
    { "--memory",                   MODIFYVM_MEMORY,                    RTGETOPT_REQ_UINT32 },
    { "--vram",                     MODIFYVM_VRAM,                      RTGETOPT_REQ_UINT32 },
    { "--firmware",                 MODIFYVM_FIRMWARE,                  RTGETOPT_REQ_STRING },
    { "--acpi",                     MODIFYVM_ACPI,                      RTGETOPT_REQ_STRING },
    { "--ioapic",                   MODIFYVM_IOAPIC,                    RTGETOPT_REQ_STRING },
    { "--pae",                      MODIFYVM_PAE,                       RTGETOPT_REQ_STRING },
    { "--synthcpu",                 MODIFYVM_SYNTHCPU,                  RTGETOPT_REQ_STRING },
    { "--hwvirtex",                 MODIFYVM_HWVIRTEX,                  RTGETOPT_REQ_STRING },
    { "--hwvirtexexcl",             MODIFYVM_HWVIRTEXEXCLUSIVE,         RTGETOPT_REQ_STRING },
    { "--nestedpaging",             MODIFYVM_NESTEDPAGING,              RTGETOPT_REQ_STRING },
    { "--vtxvpid",                  MODIFYVM_VTXVPID,                   RTGETOPT_REQ_STRING },
    { "--cpus",                     MODIFYVM_CPUS,                      RTGETOPT_REQ_UINT32 },
    { "--monitorcount",             MODIFYVM_MONITORCOUNT,              RTGETOPT_REQ_UINT32 },
    { "--accelerate3d",             MODIFYVM_ACCELERATE3D,              RTGETOPT_REQ_STRING },
    { "--accelerate2dvideo",        MODIFYVM_ACCELERATE2DVIDEO,         RTGETOPT_REQ_STRING },
    { "--bioslogofadein",           MODIFYVM_BIOSLOGOFADEIN,            RTGETOPT_REQ_STRING },
    { "--bioslogofadeout",          MODIFYVM_BIOSLOGOFADEOUT,           RTGETOPT_REQ_STRING },
    { "--bioslogodisplaytime",      MODIFYVM_BIOSLOGODISPLAYTIME,       RTGETOPT_REQ_UINT64 },
    { "--bioslogoimagepath",        MODIFYVM_BIOSLOGOIMAGEPATH,         RTGETOPT_REQ_STRING },
    { "--biosbootmenu",             MODIFYVM_BIOSBOOTMENU,              RTGETOPT_REQ_STRING },
    { "--biossystemtimeoffset",     MODIFYVM_BIOSSYSTEMTIMEOFFSET,      RTGETOPT_REQ_UINT64 },
    { "--biospxedebug",             MODIFYVM_BIOSPXEDEBUG,              RTGETOPT_REQ_STRING },
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
    { "--nictrace",                 MODIFYVM_NICTRACE,                  RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--nictype",                  MODIFYVM_NICTYPE,                   RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--nicspeed",                 MODIFYVM_NICSPEED,                  RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
    { "--nic",                      MODIFYVM_NIC,                       RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
    { "--cableconnected",           MODIFYVM_CABLECONNECTED,            RTGETOPT_REQ_STRING | RTGETOPT_FLAG_INDEX },
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
    { "--vrdpmulticon",             MODIFYVM_VRDPMULTICON,              RTGETOPT_REQ_STRING },
    { "--vrdpreusecon",             MODIFYVM_VRDPREUSECON,              RTGETOPT_REQ_STRING },
    { "--vrdp",                     MODIFYVM_VRDP,                      RTGETOPT_REQ_STRING },
    { "--usbehci",                  MODIFYVM_USBEHCI,                   RTGETOPT_REQ_STRING },
    { "--usb",                      MODIFYVM_USB,                       RTGETOPT_REQ_STRING },
    { "--snapshotfolder",           MODIFYVM_SNAPSHOTFOLDER,            RTGETOPT_REQ_STRING },
    { "--teleporterenabled",        MODIFYVM_TELEPORTER_ENABLED,        RTGETOPT_REQ_STRING },
    { "--teleporterport",           MODIFYVM_TELEPORTER_PORT,           RTGETOPT_REQ_UINT32 },
    { "--teleporteraddress",        MODIFYVM_TELEPORTER_ADDRESS,        RTGETOPT_REQ_STRING },
    { "--teleporterpassword",       MODIFYVM_TELEPORTER_PASSWORD,       RTGETOPT_REQ_STRING },
};

int handleModifyVM(HandlerArg *a)
{
    int c;
    HRESULT rc;
    Bstr name;
    Bstr machineuuid (a->argv[0]);
    RTGETOPTUNION pValueUnion;
    RTGETOPTSTATE pGetState;
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
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(NetworkAdapterCount) (&NetworkAdapterCount), 1);
    }
    ULONG SerialPortCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(SerialPortCount) (&SerialPortCount), 1);
    }

    /* try to find the given machine */
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR_RET (a->virtualBox, GetMachine (machineuuid, machine.asOutParam()), 1);
    }
    else
    {
        CHECK_ERROR_RET (a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()), 1);
        machine->COMGETTER(Id)(machineuuid.asOutParam());
    }

    /* open a session for the VM */
    CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, machineuuid), 1);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());
    machine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());

    RTGetOptInit (&pGetState, a->argc, a->argv, g_aModifyVMOptions,
                  RT_ELEMENTS(g_aModifyVMOptions), 1, 0 /* fFlags */);

    while (   SUCCEEDED (rc)
           && (c = RTGetOpt(&pGetState, &pValueUnion)))
    {
        switch (c)
        {
            case MODIFYVM_NAME:
            {
                if (pValueUnion.psz)
                    CHECK_ERROR (machine, COMSETTER(Name)(Bstr(pValueUnion.psz)));
                break;
            }
            case MODIFYVM_OSTYPE:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IGuestOSType> guestOSType;
                    CHECK_ERROR (a->virtualBox, GetGuestOSType(Bstr(pValueUnion.psz), guestOSType.asOutParam()));
                    if (SUCCEEDED(rc) && guestOSType)
                    {
                        CHECK_ERROR (machine, COMSETTER(OSTypeId)(Bstr(pValueUnion.psz)));
                    }
                    else
                    {
                        errorArgument("Invalid guest OS type '%s'", Utf8Str(pValueUnion.psz).raw());
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_MEMORY:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(MemorySize)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_VRAM:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(VRAMSize)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_FIRMWARE:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "efi"))
                    {
                        CHECK_ERROR (machine, COMSETTER(FirmwareType)(FirmwareType_EFI));
                    }
                    else if (!strcmp(pValueUnion.psz, "bios"))
                    {
                        CHECK_ERROR (machine, COMSETTER(FirmwareType)(FirmwareType_BIOS));
                    }
                    else
                    {
                        errorArgument("Invalid --firmware argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_ACPI:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(ACPIEnabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(ACPIEnabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --acpi argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_IOAPIC:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(IOAPICEnabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(IOAPICEnabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --ioapic argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_PAE:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetCpuProperty(CpuPropertyType_PAE, true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetCpuProperty(CpuPropertyType_PAE, false));
                    }
                    else
                    {
                        errorArgument("Invalid --pae argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_SYNTHCPU:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetCpuProperty(CpuPropertyType_Synthetic, true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetCpuProperty(CpuPropertyType_Synthetic, false));
                    }
                    else
                    {
                        errorArgument("Invalid --synthcpu argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_HWVIRTEX:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_Enabled, TRUE));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_Enabled, FALSE));
                    }
                    else
                    {
                        errorArgument("Invalid --hwvirtex argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_HWVIRTEXEXCLUSIVE:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_Exclusive, TRUE));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_Exclusive, FALSE));
                    }
                    else
                    {
                        errorArgument("Invalid --hwvirtex argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_NESTEDPAGING:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, TRUE));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, FALSE));
                    }
                    else
                    {
                        errorArgument("Invalid --nestedpaging argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_VTXVPID:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_VPID, TRUE));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, SetHWVirtExProperty(HWVirtExPropertyType_VPID, FALSE));
                    }
                    else
                    {
                        errorArgument("Invalid --vtxvpid argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_CPUS:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(CPUCount)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_MONITORCOUNT:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(MonitorCount)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_ACCELERATE3D:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, COMSETTER(Accelerate3DEnabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, COMSETTER(Accelerate3DEnabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --accelerate3d argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_ACCELERATE2DVIDEO:
            {
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (machine, COMSETTER(Accelerate2DVideoEnabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (machine, COMSETTER(Accelerate2DVideoEnabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --accelerate2dvideo argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
#endif
                break;
            }

            case MODIFYVM_BIOSLOGOFADEIN:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(LogoFadeIn)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(LogoFadeIn)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --bioslogofadein argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_BIOSLOGOFADEOUT:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(LogoFadeOut)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(LogoFadeOut)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --bioslogofadeout argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                        break;
                    }
                }
                break;
            }

            case MODIFYVM_BIOSLOGODISPLAYTIME:
            {
                if (pValueUnion.u64 > 0)
                    CHECK_ERROR (biosSettings, COMSETTER(LogoDisplayTime)(pValueUnion.u64));
                break;
            }

            case MODIFYVM_BIOSLOGOIMAGEPATH:
            {
                if (pValueUnion.psz)
                    CHECK_ERROR (biosSettings, COMSETTER(LogoImagePath)(Bstr(pValueUnion.psz)));
                break;
            }

            case MODIFYVM_BIOSBOOTMENU:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "disabled"))
                        CHECK_ERROR (biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_Disabled));
                    else if (!strcmp(pValueUnion.psz, "menuonly"))
                        CHECK_ERROR (biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MenuOnly));
                    else if (!strcmp(pValueUnion.psz, "messageandmenu"))
                        CHECK_ERROR (biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MessageAndMenu));
                    else
                    {
                        errorArgument("Invalid --biosbootmenu argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_BIOSSYSTEMTIMEOFFSET:
            {
                if (pValueUnion.u64 > 0)
                    CHECK_ERROR (biosSettings, COMSETTER(TimeOffset)(pValueUnion.u64));
                break;
            }

            case MODIFYVM_BIOSPXEDEBUG:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(PXEDebugEnabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (biosSettings, COMSETTER(PXEDebugEnabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --biospxedebug argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_BOOT:
            {
                if ((pGetState.uIndex < 1) && (pGetState.uIndex > 4))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid boot slot number in '%s'",
                                       pGetState.pDef->pszLong);

                if (!strcmp(pValueUnion.psz, "none"))
                {
                    CHECK_ERROR (machine, SetBootOrder (pGetState.uIndex, DeviceType_Null));
                }
                else if (!strcmp(pValueUnion.psz, "floppy"))
                {
                    CHECK_ERROR (machine, SetBootOrder (pGetState.uIndex, DeviceType_Floppy));
                }
                else if (!strcmp(pValueUnion.psz, "dvd"))
                {
                    CHECK_ERROR (machine, SetBootOrder (pGetState.uIndex, DeviceType_DVD));
                }
                else if (!strcmp(pValueUnion.psz, "disk"))
                {
                    CHECK_ERROR (machine, SetBootOrder (pGetState.uIndex, DeviceType_HardDisk));
                }
                else if (!strcmp(pValueUnion.psz, "net"))
                {
                    CHECK_ERROR (machine, SetBootOrder (pGetState.uIndex, DeviceType_Network));
                }
                else
                    return errorArgument("Invalid boot device '%s'", pValueUnion.psz);

                break;
            }

            case MODIFYVM_HDA:
            {
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 0, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(pValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(pValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR (a->virtualBox,
                                         OpenHardDisk(Bstr(pValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR (machine, AttachDevice(Bstr("IDE Controller"), 0, 0, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_HDB:
            {
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 0, 1);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(pValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(pValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR (a->virtualBox,
                                         OpenHardDisk(Bstr(pValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR (machine, AttachDevice(Bstr("IDE Controller"), 0, 1, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_HDD:
            {
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("IDE Controller"), 1, 1);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(pValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(pValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR (a->virtualBox,
                                         OpenHardDisk(Bstr(pValueUnion.psz),
                                                      AccessMode_ReadWrite, false, Bstr(""),
                                                      false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR (machine, AttachDevice(Bstr("IDE Controller"), 1, 1, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_IDECONTROLLER:
            {
                ComPtr<IStorageController> storageController;
                CHECK_ERROR (machine, GetStorageControllerByName(Bstr("IDE Controller"),
                                                                 storageController.asOutParam()));

                if (!RTStrICmp(pValueUnion.psz, "PIIX3"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
                }
                else if (!RTStrICmp(pValueUnion.psz, "PIIX4"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
                }
                else if (!RTStrICmp(pValueUnion.psz, "ICH6"))
                {
                    CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_ICH6));
                }
                else
                {
                    errorArgument("Invalid --idecontroller argument '%s'", pValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SATAIDEEMULATION:
            {
                ComPtr<IStorageController> SataCtl;
                CHECK_ERROR (machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > 4))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA boot slot number in '%s'",
                                       pGetState.pDef->pszLong);

                if ((pValueUnion.u32 < 1) && (pValueUnion.u32 > 30))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA port number in '%s'",
                                       pGetState.pDef->pszLong);

                if (SUCCEEDED(rc))
                    CHECK_ERROR(SataCtl, SetIDEEmulationPort(pGetState.uIndex, pValueUnion.u32));

                break;
            }

            case MODIFYVM_SATAPORTCOUNT:
            {
                ComPtr<IStorageController> SataCtl;
                CHECK_ERROR (machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));

                if (SUCCEEDED(rc) && pValueUnion.u32 > 0)
                    CHECK_ERROR(SataCtl, COMSETTER(PortCount)(pValueUnion.u32));

                break;
            }

            case MODIFYVM_SATAPORT:
            {
                if ((pGetState.uIndex < 1) && (pGetState.uIndex > 30))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SATA port number in '%s'",
                                       pGetState.pDef->pszLong);

                if (!strcmp(pValueUnion.psz, "none"))
                {
                    machine->DetachDevice(Bstr("SATA"), pGetState.uIndex, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(pValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc =  a->virtualBox->FindHardDisk(Bstr(pValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR (a->virtualBox,
                                         OpenHardDisk(Bstr(pValueUnion.psz), AccessMode_ReadWrite,
                                                      false, Bstr(""), false,
                                                      Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR (machine,
                                     AttachDevice(Bstr("SATA"), pGetState.uIndex,
                                                  0, DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SATA:
            {
                if (!strcmp(pValueUnion.psz, "on") || !strcmp(pValueUnion.psz, "enable"))
                {
                    ComPtr<IStorageController> ctl;
                    CHECK_ERROR (machine, AddStorageController(Bstr("SATA"), StorageBus_SATA, ctl.asOutParam()));
                    CHECK_ERROR (ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
                }
                else if (!strcmp(pValueUnion.psz, "off") || !strcmp(pValueUnion.psz, "disable"))
                    CHECK_ERROR (machine, RemoveStorageController(Bstr("SATA")));
                else
                    return errorArgument("Invalid --usb argument '%s'", pValueUnion.psz);
                break;
            }

            case MODIFYVM_SCSIPORT:
            {
                if ((pGetState.uIndex < 1) && (pGetState.uIndex > 16))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid SCSI port number in '%s'",
                                       pGetState.pDef->pszLong);

                if (!strcmp(pValueUnion.psz, "none"))
                {
                    rc = machine->DetachDevice(Bstr("LsiLogic"), pGetState.uIndex, 0);
                    if (FAILED(rc))
                        CHECK_ERROR(machine, DetachDevice(Bstr("BusLogic"), pGetState.uIndex, 0));
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Bstr uuid(pValueUnion.psz);
                    ComPtr<IMedium> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        rc = a->virtualBox->FindHardDisk(Bstr(pValueUnion.psz), hardDisk.asOutParam());
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR (a->virtualBox,
                                         OpenHardDisk(Bstr(pValueUnion.psz),
                                                           AccessMode_ReadWrite, false, Bstr(""),
                                                           false, Bstr(""), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        rc = machine->AttachDevice(Bstr("LsiLogic"), pGetState.uIndex, 0, DeviceType_HardDisk, uuid);
                        if (FAILED(rc))
                            CHECK_ERROR (machine,
                                         AttachDevice(Bstr("BusLogic"),
                                                      pGetState.uIndex, 0,
                                                      DeviceType_HardDisk, uuid));
                    }
                    else
                        rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_SCSITYPE:
            {
                ComPtr<IStorageController> ctl;

                if (!RTStrICmp(pValueUnion.psz, "LsiLogic"))
                {
                    rc = machine->RemoveStorageController(Bstr("BusLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR (machine, RemoveStorageController(Bstr("LsiLogic")));

                    CHECK_ERROR (machine,
                                 AddStorageController(Bstr("LsiLogic"),
                                                      StorageBus_SCSI,
                                                      ctl.asOutParam()));

                    if (SUCCEEDED(rc))
                        CHECK_ERROR (ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
                else if (!RTStrICmp(pValueUnion.psz, "BusLogic"))
                {
                    rc = machine->RemoveStorageController(Bstr("LsiLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR (machine, RemoveStorageController(Bstr("BusLogic")));

                    CHECK_ERROR (machine,
                                 AddStorageController(Bstr("BusLogic"),
                                                      StorageBus_SCSI,
                                                      ctl.asOutParam()));

                    if (SUCCEEDED(rc))
                        CHECK_ERROR (ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else
                    return errorArgument("Invalid --scsitype argument '%s'", pValueUnion.psz);
                break;
            }

            case MODIFYVM_SCSI:
            {
                if (!strcmp(pValueUnion.psz, "on") || !strcmp(pValueUnion.psz, "enable"))
                {
                    ComPtr<IStorageController> ctl;

                    CHECK_ERROR (machine, AddStorageController(Bstr("BusLogic"), StorageBus_SCSI, ctl.asOutParam()));
                    if (SUCCEEDED(rc))
                        CHECK_ERROR (ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else if (!strcmp(pValueUnion.psz, "off") || !strcmp(pValueUnion.psz, "disable"))
                {
                    rc = machine->RemoveStorageController(Bstr("BusLogic"));
                    if (FAILED(rc))
                        CHECK_ERROR (machine, RemoveStorageController(Bstr("LsiLogic")));
                }
                break;
            }

            case MODIFYVM_DVDPASSTHROUGH:
            {
                ComPtr<IMediumAttachment> dvdAttachment;
                machine->GetMediumAttachment(Bstr("IDE Controller"), 1, 0, dvdAttachment.asOutParam());
                ASSERT(dvdAttachment);

                CHECK_ERROR (dvdAttachment, COMSETTER(Passthrough)(!strcmp(pValueUnion.psz, "on")));
                break;
            }

            case MODIFYVM_DVD:
            {
                ComPtr<IMedium> dvdMedium;
                Bstr uuid(pValueUnion.psz);

                /* unmount? */
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    /* nothing to do, NULL object will cause unmount */
                }
                /* host drive? */
                else if (!strncmp(pValueUnion.psz, "host:", 5))
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR (a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    rc = host->FindHostDVDDrive(Bstr(pValueUnion.psz + 5), dvdMedium.asOutParam());
                    if (!dvdMedium)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(pValueUnion.psz + 5, szPathReal, sizeof(szPathReal))))
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pValueUnion.psz + 5);
                            rc = E_FAIL;
                            break;
                        }
                        rc = host->FindHostDVDDrive(Bstr(szPathReal), dvdMedium.asOutParam());
                        if (!dvdMedium)
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pValueUnion.psz + 5);
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
                        rc = a->virtualBox->FindDVDImage(Bstr(pValueUnion.psz), dvdMedium.asOutParam());
                        /* not registered, do that on the fly */
                        if (!dvdMedium)
                        {
                            Bstr emptyUUID;
                            CHECK_ERROR (a->virtualBox, OpenDVDImage(Bstr(pValueUnion.psz),
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
                CHECK_ERROR (machine, MountMedium(Bstr("IDE Controller"), 1, 0, uuid));
                break;
            }

            case MODIFYVM_FLOPPY:
            {
                Bstr uuid(pValueUnion.psz);
                ComPtr<IMedium> floppyMedium;
                ComPtr<IMediumAttachment> floppyAttachment;
                machine->GetMediumAttachment(Bstr("Floppy Controller"), 0, 0, floppyAttachment.asOutParam());

                /* disable? */
                if (!strcmp(pValueUnion.psz, "disabled"))
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
                    if (    !strcmp(pValueUnion.psz, "none")
                        ||  !strcmp(pValueUnion.psz, "empty"))   // deprecated
                    {
                        /* nothing to do, NULL object will cause unmount */
                    }
                    /* host drive? */
                    else if (!strncmp(pValueUnion.psz, "host:", 5))
                    {
                        ComPtr<IHost> host;
                        CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                        rc = host->FindHostFloppyDrive(Bstr(pValueUnion.psz + 5), floppyMedium.asOutParam());
                        if (!floppyMedium)
                        {
                            errorArgument("Invalid host floppy drive name \"%s\"", pValueUnion.psz + 5);
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
                            rc = a->virtualBox->FindFloppyImage(Bstr(pValueUnion.psz), floppyMedium.asOutParam());
                            /* not registered, do that on the fly */
                            if (!floppyMedium)
                            {
                                Bstr emptyUUID;
                                CHECK_ERROR (a->virtualBox,
                                             OpenFloppyImage(Bstr(pValueUnion.psz),
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
                    CHECK_ERROR (machine, MountMedium(Bstr("Floppy Controller"), 0, 0, uuid));
                }
                break;
            }

            case MODIFYVM_NICTRACEFILE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR (nic, COMSETTER(TraceFile)(Bstr(pValueUnion.psz)));
                break;
            }

            case MODIFYVM_NICTRACE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(pValueUnion.psz, "on"))
                {
                    CHECK_ERROR (nic, COMSETTER(TraceEnabled)(TRUE));
                }
                else if (!strcmp(pValueUnion.psz, "off"))
                {
                    CHECK_ERROR (nic, COMSETTER(TraceEnabled)(FALSE));
                }
                else
                {
                    errorArgument("Invalid --nictrace%lu argument '%s'", pGetState.uIndex, pValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_NICTYPE:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(pValueUnion.psz, "Am79C970A"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C970A));
                }
                else if (!strcmp(pValueUnion.psz, "Am79C973"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C973));
                }
#ifdef VBOX_WITH_E1000
                else if (!strcmp(pValueUnion.psz, "82540EM"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_I82540EM));
                }
                else if (!strcmp(pValueUnion.psz, "82543GC"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_I82543GC));
                }
                else if (!strcmp(pValueUnion.psz, "82545EM"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_I82545EM));
                }
#endif
#ifdef VBOX_WITH_VIRTIO
                else if (!strcmp(pValueUnion.psz, "virtio"))
                {
                    CHECK_ERROR (nic, COMSETTER(AdapterType)(NetworkAdapterType_Virtio));
                }
#endif /* VBOX_WITH_VIRTIO */
                else
                {
                    errorArgument("Invalid NIC type '%s' specified for NIC %lu", pValueUnion.psz, pGetState.uIndex);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_NICSPEED:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                if ((pValueUnion.u32 < 1000) && (pValueUnion.u32 > 4000000))
                {
                    errorArgument("Invalid --nicspeed%lu argument '%u'", pGetState.uIndex, pValueUnion.u32);
                    rc = E_FAIL;
                    break;
                }

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR (nic, COMSETTER(LineSpeed)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_NIC:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(pValueUnion.psz, "none"))
                {
                    CHECK_ERROR (nic, COMSETTER(Enabled) (FALSE));
                }
                else if (!strcmp(pValueUnion.psz, "null"))
                {
                    CHECK_ERROR (nic, COMSETTER(Enabled) (TRUE));
                    CHECK_ERROR (nic, Detach());
                }
                else if (!strcmp(pValueUnion.psz, "nat"))
                {
                    CHECK_ERROR (nic, COMSETTER(Enabled) (TRUE));
                    CHECK_ERROR (nic, AttachToNAT());
                }
                else if (  !strcmp(pValueUnion.psz, "bridged")
                        || !strcmp(pValueUnion.psz, "hostif")) /* backward compatibility */
                {
                    CHECK_ERROR (nic, COMSETTER(Enabled) (TRUE));
                    CHECK_ERROR (nic, AttachToBridgedInterface());
                }
                else if (!strcmp(pValueUnion.psz, "intnet"))
                {
                    CHECK_ERROR (nic, COMSETTER(Enabled) (TRUE));
                    CHECK_ERROR (nic, AttachToInternalNetwork());
                }
#if defined(VBOX_WITH_NETFLT)
                else if (!strcmp(pValueUnion.psz, "hostonly"))
                {

                    CHECK_ERROR (nic, COMSETTER(Enabled) (TRUE));
                    CHECK_ERROR (nic, AttachToHostOnlyInterface());
                }
#endif
                else
                {
                    errorArgument("Invalid type '%s' specfied for NIC %lu", pValueUnion.psz, pGetState.uIndex);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_CABLECONNECTED:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                if (!strcmp(pValueUnion.psz, "on"))
                {
                    CHECK_ERROR (nic, COMSETTER(CableConnected)(TRUE));
                }
                else if (!strcmp(pValueUnion.psz, "off"))
                {
                    CHECK_ERROR (nic, COMSETTER(CableConnected)(FALSE));
                }
                else
                {
                    errorArgument("Invalid --cableconnected%lu argument '%s'", pGetState.uIndex, pValueUnion.psz);
                    rc = E_FAIL;
                }
                break;
            }

            case MODIFYVM_BRIDGEADAPTER:
            case MODIFYVM_HOSTONLYADAPTER:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* remove it? */
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    CHECK_ERROR (nic, COMSETTER(HostInterface)(NULL));
                }
                else
                {
                    CHECK_ERROR (nic, COMSETTER(HostInterface)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_INTNET:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* remove it? */
                if (!strcmp(pValueUnion.psz, "none"))
                {
                    CHECK_ERROR (nic, COMSETTER(InternalNetwork)(NULL));
                }
                else
                {
                    CHECK_ERROR (nic, COMSETTER(InternalNetwork)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_NATNET:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                CHECK_ERROR (nic, COMSETTER(NATNetwork)(Bstr(pValueUnion.psz)));

                break;
            }

            case MODIFYVM_MACADDRESS:
            {
                ComPtr<INetworkAdapter> nic;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > NetworkAdapterCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid NIC slot number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetNetworkAdapter (pGetState.uIndex - 1, nic.asOutParam()));
                ASSERT(nic);

                /* generate one? */
                if (!strcmp(pValueUnion.psz, "auto"))
                {
                    CHECK_ERROR (nic, COMSETTER(MACAddress)(NULL));
                }
                else
                {
                    CHECK_ERROR (nic, COMSETTER(MACAddress)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_UARTMODE:
            {
                ComPtr<ISerialPort> uart;
                char *pszIRQ = NULL;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > SerialPortCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid Serial Port number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetSerialPort (pGetState.uIndex - 1, uart.asOutParam()));
                ASSERT(uart);

                if (!strcmp(pValueUnion.psz, "disconnected"))
                {
                    CHECK_ERROR (uart, COMSETTER(HostMode) (PortMode_Disconnected));
                }
                else if (   !strcmp(pValueUnion.psz, "server")
                         || !strcmp(pValueUnion.psz, "client")
                         || !strcmp(pValueUnion.psz, "file"))
                {
                    char *pszIRQ = NULL;

                    /**
                     * @todo: not the right way to get the second value for
                     * the argument, change this when RTGetOpt can return
                     * multiple values
                     */
                    pszIRQ = pGetState.argv[pGetState.iNext];
                    pGetState.iNext++;

                    if (!pszIRQ)
                        return errorSyntax(USAGE_MODIFYVM,
                                           "Missing or Invalid argument to '%s'",
                                           pGetState.pDef->pszLong);

                    CHECK_ERROR (uart, COMSETTER(Path) (Bstr(pszIRQ)));

                    if (!strcmp(pValueUnion.psz, "server"))
                    {
                        CHECK_ERROR (uart, COMSETTER(HostMode) (PortMode_HostPipe));
                        CHECK_ERROR (uart, COMSETTER(Server) (TRUE));
                    }
                    else if (!strcmp(pValueUnion.psz, "client"))
                    {
                        CHECK_ERROR (uart, COMSETTER(HostMode) (PortMode_HostPipe));
                        CHECK_ERROR (uart, COMSETTER(Server) (FALSE));
                    }
                    else if (!strcmp(pValueUnion.psz, "file"))
                    {
                        CHECK_ERROR (uart, COMSETTER(HostMode) (PortMode_RawFile));
                    }
                }
                else
                {
                    CHECK_ERROR (uart, COMSETTER(Path) (Bstr(pValueUnion.psz)));
                    CHECK_ERROR (uart, COMSETTER(HostMode) (PortMode_HostDevice));
                }
                break;
            }

            case MODIFYVM_UART:
            {
                ComPtr<ISerialPort> uart;

                if ((pGetState.uIndex < 1) && (pGetState.uIndex > SerialPortCount))
                    return errorSyntax(USAGE_MODIFYVM,
                                       "Missing or Invalid Serial Port number in '%s'",
                                       pGetState.pDef->pszLong);

                CHECK_ERROR_BREAK (machine, GetSerialPort (pGetState.uIndex - 1, uart.asOutParam()));
                ASSERT(uart);

                if (!strcmp(pValueUnion.psz, "off") || !strcmp(pValueUnion.psz, "disable"))
                    CHECK_ERROR (uart, COMSETTER(Enabled) (FALSE));
                else
                {
                    char *pszIRQ = NULL;
                    uint32_t uVal = 0;
                    int vrc;

                    /**
                     * @todo: not the right way to get the second value for
                     * the argument, change this when RTGetOpt can return
                     * multiple values
                     */
                    pszIRQ = pGetState.argv[pGetState.iNext];
                    pGetState.iNext++;

                    if (!pszIRQ)
                        return errorSyntax(USAGE_MODIFYVM,
                                           "Missing or Invalid argument to '%s'",
                                           pGetState.pDef->pszLong);

                    vrc = RTStrToUInt32Ex(pszIRQ, NULL, 0, &uVal);
                    if (vrc != VINF_SUCCESS || uVal == 0)
                        return errorArgument("Error parsing UART I/O base '%s'", pszIRQ);

                    CHECK_ERROR (uart, COMSETTER(IRQ) (uVal));

                    vrc = RTStrToUInt32Ex(pValueUnion.psz, NULL, 0, &uVal);
                    if (vrc != VINF_SUCCESS || uVal == 0)
                        return errorArgument("Error parsing UART I/O base '%s'", pszIRQ);
                    CHECK_ERROR (uart, COMSETTER(IOBase) (uVal));

                    CHECK_ERROR (uart, COMSETTER(Enabled) (TRUE));
                }
                break;
            }

            case MODIFYVM_GUESTSTATISTICSINTERVAL:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(StatisticsUpdateInterval)(pValueUnion.u32));
                break;
            }

#ifdef VBOX_WITH_MEM_BALLOONING
            case MODIFYVM_GUESTMEMORYBALLOON:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR (machine, COMSETTER(MemoryBalloonSize)(pValueUnion.u32));
                break;
            }
#endif

            case MODIFYVM_AUDIOCONTROLLER:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IAudioAdapter> audioAdapter;
                    machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
                    ASSERT(audioAdapter);

                    if (!strcmp(pValueUnion.psz, "sb16"))
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioController)(AudioControllerType_SB16));
                    else if (!strcmp(pValueUnion.psz, "ac97"))
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioController)(AudioControllerType_AC97));
                    else
                    {
                        errorArgument("Invalid --audiocontroller argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_AUDIO:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IAudioAdapter> audioAdapter;
                    machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
                    ASSERT(audioAdapter);

                    /* disable? */
                    if (!strcmp(pValueUnion.psz, "none"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(false));
                    }
                    else if (!strcmp(pValueUnion.psz, "null"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Null));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
                    else if (!strcmp(pValueUnion.psz, "winmm"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_WinMM));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
#endif
                    else if (!strcmp(pValueUnion.psz, "dsound"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_DirectSound));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_LINUX
                    else if (!strcmp(pValueUnion.psz, "oss"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
# ifdef VBOX_WITH_ALSA
                    else if (!strcmp(pValueUnion.psz, "alsa"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_ALSA));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
# endif
# ifdef VBOX_WITH_PULSE
                    else if (!strcmp(pValueUnion.psz, "pulse"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Pulse));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
# endif
#endif /* !RT_OS_LINUX */
#ifdef RT_OS_SOLARIS
                    else if (!strcmp(pValueUnion.psz, "solaudio"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_SolAudio));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }

# ifdef VBOX_WITH_SOLARIS_OSS
                    else if (!strcmp(pValueUnion.psz, "oss"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }
# endif

#endif /* !RT_OS_SOLARIS */
#ifdef RT_OS_DARWIN
                    else if (!strcmp(pValueUnion.psz, "coreaudio"))
                    {
                        CHECK_ERROR (audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_CoreAudio));
                        CHECK_ERROR (audioAdapter, COMSETTER(Enabled)(true));
                    }

#endif /* !RT_OS_DARWIN */
                    else
                    {
                        errorArgument("Invalid --audio argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_CLIPBOARD:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "disabled"))
                    {
                        CHECK_ERROR (machine, COMSETTER(ClipboardMode)(ClipboardMode_Disabled));
                    }
                    else if (!strcmp(pValueUnion.psz, "hosttoguest"))
                    {
                        CHECK_ERROR (machine, COMSETTER(ClipboardMode)(ClipboardMode_HostToGuest));
                    }
                    else if (!strcmp(pValueUnion.psz, "guesttohost"))
                    {
                        CHECK_ERROR (machine, COMSETTER(ClipboardMode)(ClipboardMode_GuestToHost));
                    }
                    else if (!strcmp(pValueUnion.psz, "bidirectional"))
                    {
                        CHECK_ERROR (machine, COMSETTER(ClipboardMode)(ClipboardMode_Bidirectional));
                    }
                    else
                    {
                        errorArgument("Invalid --clipboard argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

#ifdef VBOX_WITH_VRDP
            case MODIFYVM_VRDPPORT:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    if (!strcmp(pValueUnion.psz, "default"))
                        CHECK_ERROR (vrdpServer, COMSETTER(Ports)(Bstr("0")));
                    else
                        CHECK_ERROR (vrdpServer, COMSETTER(Ports)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_VRDPADDRESS:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    CHECK_ERROR (vrdpServer, COMSETTER(NetAddress)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_VRDPAUTHTYPE:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    if (!strcmp(pValueUnion.psz, "null"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Null));
                    }
                    else if (!strcmp(pValueUnion.psz, "external"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(AuthType)(VRDPAuthType_External));
                    }
                    else if (!strcmp(pValueUnion.psz, "guest"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Guest));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpauthtype argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_VRDPMULTICON:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(AllowMultiConnection)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(AllowMultiConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpmulticon argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_VRDPREUSECON:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(ReuseSingleConnection)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(ReuseSingleConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpreusecon argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }

            case MODIFYVM_VRDP:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IVRDPServer> vrdpServer;
                    machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
                    ASSERT(vrdpServer);

                    if (!strcmp(pValueUnion.psz, "on"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(Enabled)(true));
                    }
                    else if (!strcmp(pValueUnion.psz, "off"))
                    {
                        CHECK_ERROR (vrdpServer, COMSETTER(Enabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdp argument '%s'", pValueUnion.psz);
                        rc = E_FAIL;
                    }
                }
                break;
            }
#endif /* VBOX_WITH_VRDP */

            case MODIFYVM_USBEHCI:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IUSBController> UsbCtl;
                    CHECK_ERROR (machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        if (!strcmp(pValueUnion.psz, "on") || !strcmp(pValueUnion.psz, "enable"))
                            CHECK_ERROR (UsbCtl, COMSETTER(EnabledEhci)(true));
                        else if (!strcmp(pValueUnion.psz, "off") || !strcmp(pValueUnion.psz, "disable"))
                            CHECK_ERROR (UsbCtl, COMSETTER(EnabledEhci)(false));
                        else
                            return errorArgument("Invalid --usbehci argument '%s'", pValueUnion.psz);
                    }
                }
                break;
            }

            case MODIFYVM_USB:
            {
                if (pValueUnion.psz)
                {
                    ComPtr<IUSBController> UsbCtl;
                    CHECK_ERROR (machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        if (!strcmp(pValueUnion.psz, "on") || !strcmp(pValueUnion.psz, "enable"))
                            CHECK_ERROR (UsbCtl, COMSETTER(Enabled)(true));
                        else if (!strcmp(pValueUnion.psz, "off") || !strcmp(pValueUnion.psz, "disable"))
                            CHECK_ERROR (UsbCtl, COMSETTER(Enabled)(false));
                        else
                            return errorArgument("Invalid --usb argument '%s'", pValueUnion.psz);
                    }
                }
                break;
            }

            case MODIFYVM_SNAPSHOTFOLDER:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "default"))
                        CHECK_ERROR (machine, COMSETTER(SnapshotFolder)(NULL));
                    else
                        CHECK_ERROR (machine, COMSETTER(SnapshotFolder)(Bstr(pValueUnion.psz)));
                }
                break;
            }

            case MODIFYVM_TELEPORTER_ENABLED:
            {
                if (pValueUnion.psz)
                {
                    if (!strcmp(pValueUnion.psz, "on"))
                        CHECK_ERROR (machine, COMSETTER(TeleporterEnabled)(1));
                    else if (!strcmp(pValueUnion.psz, "off"))
                        CHECK_ERROR (machine, COMSETTER(TeleporterEnabled)(0));
                    else
                        return errorArgument("Invalid --teleporterenabled value '%s'", pValueUnion.psz);
                }
                break;
            }

            case MODIFYVM_TELEPORTER_PORT:
            {
                if (pValueUnion.u32 > 0)
                    CHECK_ERROR(machine, COMSETTER(TeleporterPort)(pValueUnion.u32));
                break;
            }

            case MODIFYVM_TELEPORTER_ADDRESS:
            {
                if (pValueUnion.psz)
                    CHECK_ERROR(machine, COMSETTER(TeleporterAddress)(Bstr(pValueUnion.psz)));
                break;
            }

            case MODIFYVM_TELEPORTER_PASSWORD:
            {
                if (pValueUnion.psz)
                    CHECK_ERROR(machine, COMSETTER(TeleporterPassword)(Bstr(pValueUnion.psz)));
                break;
            }

            default:
            {
                errorGetOpt(USAGE_MODIFYVM, c, &pValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR (machine, SaveSettings());

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
