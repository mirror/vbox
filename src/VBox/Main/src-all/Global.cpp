/* $Id$ */
/** @file
 * VirtualBox COM global definitions
 *
 * NOTE: This file is part of both VBoxC.dll and VBoxSVC.exe.
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Global.h"

#include <iprt/assert.h>
#include <iprt/string.h>
#include <VBox/err.h>

/* static */
const Global::OSType Global::sOSTypes[] =
{
    /* NOTE1: we assume that unknown is always the first entry!
     * NOTE2: please use powers of 2 when specifying the size of harddisks since
     *        '2GB' looks better than '1.95GB' (= 2000MB) */
    { "Other",   "Other",             "Other",              "Other/Unknown",
      VBOXOSTYPE_Unknown,         VBOXOSHINT_NONE,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
      StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97 },
    { "Windows", "Microsoft Windows", "Windows31",          "Windows 3.1",
      VBOXOSTYPE_Win31,           VBOXOSHINT_FLOPPY,
        32,   4,  1 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "Windows", "Microsoft Windows", "Windows95",          "Windows 95",
      VBOXOSTYPE_Win95,           VBOXOSHINT_FLOPPY,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "Windows", "Microsoft Windows", "Windows98",          "Windows 98",
      VBOXOSTYPE_Win98,           VBOXOSHINT_FLOPPY,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "Windows", "Microsoft Windows", "WindowsMe",          "Windows ME",
      VBOXOSTYPE_WinMe,           VBOXOSHINT_FLOPPY | VBOXOSHINT_USBTABLET,
        128,  4,  4 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Windows", "Microsoft Windows", "WindowsNT4",         "Windows NT 4",
      VBOXOSTYPE_WinNT4,          VBOXOSHINT_NONE,
       128,  16,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "Windows", "Microsoft Windows", "Windows2000",        "Windows 2000",
      VBOXOSTYPE_Win2k,            VBOXOSHINT_USBTABLET,
       168,  16,  4 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Windows", "Microsoft Windows", "WindowsXP",          "Windows XP",
      VBOXOSTYPE_WinXP,            VBOXOSHINT_USBTABLET,
       192,  16, 10 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Windows", "Microsoft Windows", "WindowsXP_64",       "Windows XP (64 bit)",
      VBOXOSTYPE_WinXP_x64,       VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
       192,  16, 10 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Windows", "Microsoft Windows", "Windows2003",        "Windows 2003",
      VBOXOSTYPE_Win2k3,           VBOXOSHINT_USBTABLET,
       256,  16, 20 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Windows", "Microsoft Windows", "Windows2003_64",     "Windows 2003 (64 bit)",
      VBOXOSTYPE_Win2k3_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
       256,  16, 20 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "WindowsVista",       "Windows Vista",
      VBOXOSTYPE_WinVista,         VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "WindowsVista_64",    "Windows Vista (64 bit)",
      VBOXOSTYPE_WinVista_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows2008",        "Windows 2008",
      VBOXOSTYPE_Win2k8,           VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows2008_64",     "Windows 2008 (64 bit)",
      VBOXOSTYPE_Win2k8_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows7",           "Windows 7",
      VBOXOSTYPE_Win7,             VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows7_64",        "Windows 7 (64 bit)",
      VBOXOSTYPE_Win7_x64,        VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
       512,  16, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows8",           "Windows 8",
      VBOXOSTYPE_Win8,             VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET | VBOXOSHINT_ACCEL3D | VBOXOSHINT_PAE,
       1024,128, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "Windows8_64",        "Windows 8 (64 bit)",
      VBOXOSTYPE_Win8_x64,        VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET | VBOXOSHINT_ACCEL3D,
       2048,128, 25 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_HDA  },
    { "Windows", "Microsoft Windows", "WindowsNT",          "Other Windows",
      VBOXOSTYPE_WinNT,           VBOXOSHINT_NONE,
       512,  16, 20 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux22",            "Linux 2.2",
      VBOXOSTYPE_Linux22,         VBOXOSHINT_RTCUTC,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux24",            "Linux 2.4",
      VBOXOSTYPE_Linux24,         VBOXOSHINT_RTCUTC,
       128,   4,  4 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux24_64",         "Linux 2.4 (64 bit)",
      VBOXOSTYPE_Linux24_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC,
       128,   4,  4 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux26",            "Linux 2.6",
      VBOXOSTYPE_Linux26,         VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,   4,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux26_64",         "Linux 2.6 (64 bit)",
      VBOXOSTYPE_Linux26_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,   4,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "ArchLinux",          "Arch Linux",
      VBOXOSTYPE_ArchLinux,       VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "ArchLinux_64",       "Arch Linux (64 bit)",
      VBOXOSTYPE_ArchLinux_x64,   VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Debian",             "Debian",
      VBOXOSTYPE_Debian,          VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       384,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Debian_64",          "Debian (64 bit)",
      VBOXOSTYPE_Debian_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       384,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97},
    { "Linux",   "Linux",             "OpenSUSE",           "openSUSE",
      VBOXOSTYPE_OpenSUSE,        VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "OpenSUSE_64",        "openSUSE (64 bit)",
      VBOXOSTYPE_OpenSUSE_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Fedora",             "Fedora",
      VBOXOSTYPE_FedoraCore,      VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       768,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Fedora_64",          "Fedora (64 bit)",
      VBOXOSTYPE_FedoraCore_x64,  VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       768,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Gentoo",             "Gentoo",
      VBOXOSTYPE_Gentoo,          VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Gentoo_64",          "Gentoo (64 bit)",
      VBOXOSTYPE_Gentoo_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Mandriva",           "Mandriva",
      VBOXOSTYPE_Mandriva,        VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Mandriva_64",        "Mandriva (64 bit)",
      VBOXOSTYPE_Mandriva_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "RedHat",             "Red Hat",
      VBOXOSTYPE_RedHat,          VBOXOSHINT_RTCUTC | VBOXOSHINT_PAE,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "RedHat_64",          "Red Hat (64 bit)",
      VBOXOSTYPE_RedHat_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_PAE | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Turbolinux",         "Turbolinux",
      VBOXOSTYPE_Turbolinux,      VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       384,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Turbolinux_64",      "Turbolinux (64 bit)",
      VBOXOSTYPE_Turbolinux_x64,  VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       384,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Ubuntu",             "Ubuntu",
      VBOXOSTYPE_Ubuntu,          VBOXOSHINT_RTCUTC | VBOXOSHINT_PAE | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Ubuntu_64",          "Ubuntu (64 bit)",
      VBOXOSTYPE_Ubuntu_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       512,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Xandros",            "Xandros",
      VBOXOSTYPE_Xandros,         VBOXOSHINT_RTCUTC,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Xandros_64",         "Xandros (64 bit)",
      VBOXOSTYPE_Xandros_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC,
       256,  12,  8 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Oracle",             "Oracle",
      VBOXOSTYPE_Oracle,          VBOXOSHINT_RTCUTC | VBOXOSHINT_PAE,
       512,  12, 12 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Oracle_64",          "Oracle (64 bit)",
      VBOXOSTYPE_Oracle_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_PAE | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_RTCUTC,
       512,  12, 12 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Linux",   "Linux",             "Linux",              "Other Linux",
      VBOXOSTYPE_Linux,           VBOXOSHINT_RTCUTC | VBOXOSHINT_USBTABLET,
       256,  12,  8 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Solaris", "Solaris",           "Solaris",            "Oracle Solaris 10 5/09 and earlier",
      VBOXOSTYPE_Solaris,         VBOXOSHINT_NONE,
       768,  12, 16 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Solaris", "Solaris",           "Solaris_64",         "Oracle Solaris 10 5/09 and earlier (64 bit)",
      VBOXOSTYPE_Solaris_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,
      1536,  12, 16 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Solaris", "Solaris",           "OpenSolaris",        "Oracle Solaris 10 10/09 and later",
      VBOXOSTYPE_OpenSolaris,     VBOXOSHINT_USBTABLET,
       768,  12, 16 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_IntelAhci, StorageBus_SATA,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Solaris", "Solaris",           "OpenSolaris_64",     "Oracle Solaris 10 10/09 and later (64 bit)",
      VBOXOSTYPE_OpenSolaris_x64, VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
      1536,  12, 16 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_IntelAhci, StorageBus_SATA,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Solaris", "Solaris",           "Solaris11_64",       "Oracle Solaris 11 (64 bit)",
      VBOXOSTYPE_Solaris11_x64, VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_USBTABLET,
      1536,  12, 16 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_IntelAhci, StorageBus_SATA,
        StorageControllerType_IntelAhci, StorageBus_SATA, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "FreeBSD",            "FreeBSD",
      VBOXOSTYPE_FreeBSD,         VBOXOSHINT_NONE,
       128,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "FreeBSD_64",         "FreeBSD (64 bit)",
      VBOXOSTYPE_FreeBSD_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,
       128,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "OpenBSD",            "OpenBSD",
      VBOXOSTYPE_OpenBSD,         VBOXOSHINT_HWVIRTEX,
        64,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "OpenBSD_64",         "OpenBSD (64 bit)",
      VBOXOSTYPE_OpenBSD_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,
        64,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "NetBSD",             "NetBSD",
      VBOXOSTYPE_NetBSD,          VBOXOSHINT_NONE,
        64,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "BSD",     "BSD",               "NetBSD_64",          "NetBSD (64 bit)",
      VBOXOSTYPE_NetBSD_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,
        64,   4,  2 * _1G64, NetworkAdapterType_I82540EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "OS2",     "IBM OS/2",          "OS2Warp3",           "OS/2 Warp 3",
      VBOXOSTYPE_OS2Warp3,        VBOXOSHINT_HWVIRTEX | VBOXOSHINT_FLOPPY,
        48,   4,  1 * _1G64, NetworkAdapterType_Am79C973, 1, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "OS2",     "IBM OS/2",          "OS2Warp4",           "OS/2 Warp 4",
      VBOXOSTYPE_OS2Warp4,        VBOXOSHINT_HWVIRTEX | VBOXOSHINT_FLOPPY,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 1, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "OS2",     "IBM OS/2",          "OS2Warp45",          "OS/2 Warp 4.5",
      VBOXOSTYPE_OS2Warp45,       VBOXOSHINT_HWVIRTEX | VBOXOSHINT_FLOPPY,
        128,  4,  2 * _1G64, NetworkAdapterType_Am79C973, 1, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "OS2",     "IBM OS/2",          "OS2eCS",             "eComStation",
      VBOXOSTYPE_ECS,             VBOXOSHINT_HWVIRTEX,
        256,  4,  2 * _1G64, NetworkAdapterType_Am79C973, 1, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "OS2",     "IBM OS/2",          "OS2",                "Other OS/2",
      VBOXOSTYPE_OS2,             VBOXOSHINT_HWVIRTEX | VBOXOSHINT_FLOPPY | VBOXOSHINT_NOUSB,
        96,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 1, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "MacOS",   "Mac OS X",          "MacOS",              "Mac OS X",
      VBOXOSTYPE_MacOS,           VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_EFI | VBOXOSHINT_PAE | VBOXOSHINT_USBHID | VBOXOSHINT_HPET | VBOXOSHINT_USBTABLET,
      1024,   4, 20 * _1G64, NetworkAdapterType_I82543GC, 0,
       StorageControllerType_ICH6, StorageBus_IDE, StorageControllerType_IntelAhci, StorageBus_SATA,
      ChipsetType_ICH9, AudioControllerType_HDA  },
    { "MacOS",   "Mac OS X",          "MacOS_64",           "Mac OS X (64 bit)",
      VBOXOSTYPE_MacOS_x64,       VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_EFI | VBOXOSHINT_PAE |  VBOXOSHINT_64BIT | VBOXOSHINT_USBHID | VBOXOSHINT_HPET | VBOXOSHINT_USBTABLET,
      1024,   4, 20 * _1G64, NetworkAdapterType_I82543GC, 0,
      StorageControllerType_ICH6, StorageBus_IDE, StorageControllerType_IntelAhci, StorageBus_SATA,
      ChipsetType_ICH9, AudioControllerType_HDA  },
    { "Other",   "Other",             "DOS",                "DOS",
      VBOXOSTYPE_DOS,             VBOXOSHINT_FLOPPY | VBOXOSHINT_NOUSB,
        32,   4,  500 * _1M, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_SB16  },
    { "Other",   "Other",             "Netware",            "Netware",
      VBOXOSTYPE_Netware,         VBOXOSHINT_HWVIRTEX,
       512,   4,  4 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Other",   "Other",             "L4",                 "L4",
      VBOXOSTYPE_L4,              VBOXOSHINT_NONE,
        64,   4,  2 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Other",   "Other",             "QNX",                "QNX",
      VBOXOSTYPE_QNX,             VBOXOSHINT_HWVIRTEX,
       512,   4,  4 * _1G64, NetworkAdapterType_Am79C973, 0, StorageControllerType_PIIX4, StorageBus_IDE,
      StorageControllerType_PIIX4, StorageBus_IDE, ChipsetType_PIIX3, AudioControllerType_AC97  },
    { "Other",   "Other",             "JRockitVE",          "JRockitVE",
        VBOXOSTYPE_JRockitVE,     VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC | VBOXOSHINT_PAE,
        1024, 4,  8 * _1G64, NetworkAdapterType_I82545EM, 0, StorageControllerType_PIIX4, StorageBus_IDE,
        StorageControllerType_BusLogic, StorageBus_SCSI, ChipsetType_PIIX3, AudioControllerType_AC97  },
};

uint32_t Global::cOSTypes = RT_ELEMENTS(Global::sOSTypes);

/**
 * Returns an OS Type ID for the given VBOXOSTYPE value.
 *
 * The returned ID will correspond to the IGuestOSType::id value of one of the
 * objects stored in the IVirtualBox::guestOSTypes
 * (VirtualBoxImpl::COMGETTER(GuestOSTypes)) collection.
 */
/* static */
const char *Global::OSTypeId(VBOXOSTYPE aOSType)
{
    for (size_t i = 0; i < RT_ELEMENTS(sOSTypes); ++i)
    {
        if (sOSTypes[i].osType == aOSType)
            return sOSTypes[i].id;
    }

    return sOSTypes[0].id;
}

/*static*/ uint32_t Global::getMaxNetworkAdapters(ChipsetType_T aChipsetType)
{
    switch (aChipsetType)
    {
        case ChipsetType_PIIX3:
            return 8;
        case ChipsetType_ICH9:
            return 36;
        default:
            return 0;
    }
}

/*static*/ const char *
Global::stringifyMachineState(MachineState_T aState)
{
    switch (aState)
    {
        case MachineState_Null:                 return "Null";
        case MachineState_PoweredOff:           return "PoweredOff";
        case MachineState_Saved:                return "Saved";
        case MachineState_Teleported:           return "Teleported";
        case MachineState_Aborted:              return "Aborted";
        case MachineState_Running:              return "Running";
        case MachineState_Paused:               return "Paused";
        case MachineState_Stuck:                return "GuruMeditation";
        case MachineState_Teleporting:          return "Teleporting";
        case MachineState_LiveSnapshotting:     return "LiveSnapshotting";
        case MachineState_Starting:             return "Starting";
        case MachineState_Stopping:             return "Stopping";
        case MachineState_Saving:               return "Saving";
        case MachineState_Restoring:            return "Restoring";
        case MachineState_TeleportingPausedVM:  return "TeleportingPausedVM";
        case MachineState_TeleportingIn:        return "TeleportingIn";
        case MachineState_FaultTolerantSyncing: return "FaultTolerantSyncing";
        case MachineState_DeletingSnapshotOnline: return "DeletingSnapshotOnline";
        case MachineState_DeletingSnapshotPaused: return "DeletingSnapshotPaused";
        case MachineState_RestoringSnapshot:    return "RestoringSnapshot";
        case MachineState_DeletingSnapshot:     return "DeletingSnapshot";
        case MachineState_SettingUp:            return "SettingUp";
        default:
        {
            AssertMsgFailed(("%d (%#x)\n", aState, aState));
            static char s_szMsg[48];
            RTStrPrintf(s_szMsg, sizeof(s_szMsg), "InvalidState-0x%08x\n", aState);
            return s_szMsg;
        }
    }
}

/*static*/ const char *
Global::stringifySessionState(SessionState_T aState)
{
    switch (aState)
    {
        case SessionState_Null:         return "Null";
        case SessionState_Unlocked:     return "Unlocked";
        case SessionState_Locked:       return "Locked";
        case SessionState_Spawning:     return "Spawning";
        case SessionState_Unlocking:    return "Unlocking";
        default:
        {
            AssertMsgFailed(("%d (%#x)\n", aState, aState));
            static char s_szMsg[48];
            RTStrPrintf(s_szMsg, sizeof(s_szMsg), "InvalidState-0x%08x\n", aState);
            return s_szMsg;
        }
    }
}

/*static*/ const char *
Global::stringifyDeviceType(DeviceType_T aType)
{
    switch (aType)
    {
        case DeviceType_Null:         return "Null";
        case DeviceType_Floppy:       return "Floppy";
        case DeviceType_DVD:          return "DVD";
        case DeviceType_HardDisk:     return "HardDisk";
        case DeviceType_Network:      return "Network";
        case DeviceType_USB:          return "USB";
        case DeviceType_SharedFolder: return "ShardFolder";
        default:
        {
            AssertMsgFailed(("%d (%#x)\n", aType, aType));
            static char s_szMsg[48];
            RTStrPrintf(s_szMsg, sizeof(s_szMsg), "InvalidType-0x%08x\n", aType);
            return s_szMsg;
        }
    }
}

/*static*/ int
Global::vboxStatusCodeFromCOM(HRESULT aComStatus)
{
    switch (aComStatus)
    {
        case S_OK:                              return VINF_SUCCESS;

        /* Standard COM status codes. See also RTErrConvertFromDarwinCOM */
        case E_UNEXPECTED:                      return VERR_COM_UNEXPECTED;
        case E_NOTIMPL:                         return VERR_NOT_IMPLEMENTED;
        case E_OUTOFMEMORY:                     return VERR_NO_MEMORY;
        case E_INVALIDARG:                      return VERR_INVALID_PARAMETER;
        case E_NOINTERFACE:                     return VERR_NOT_SUPPORTED;
        case E_POINTER:                         return VERR_INVALID_POINTER;
#ifdef E_HANDLE
        case E_HANDLE:                          return VERR_INVALID_HANDLE;
#endif
        case E_ABORT:                           return VERR_CANCELLED;
        case E_FAIL:                            return VERR_GENERAL_FAILURE;
        case E_ACCESSDENIED:                    return VERR_ACCESS_DENIED;

        /* VirtualBox status codes */
        case VBOX_E_OBJECT_NOT_FOUND:           return VERR_COM_OBJECT_NOT_FOUND;
        case VBOX_E_INVALID_VM_STATE:           return VERR_COM_INVALID_VM_STATE;
        case VBOX_E_VM_ERROR:                   return VERR_COM_VM_ERROR;
        case VBOX_E_FILE_ERROR:                 return VERR_COM_FILE_ERROR;
        case VBOX_E_IPRT_ERROR:                 return VERR_COM_IPRT_ERROR;
        case VBOX_E_PDM_ERROR:                  return VERR_COM_PDM_ERROR;
        case VBOX_E_INVALID_OBJECT_STATE:       return VERR_COM_INVALID_OBJECT_STATE;
        case VBOX_E_HOST_ERROR:                 return VERR_COM_HOST_ERROR;
        case VBOX_E_NOT_SUPPORTED:              return VERR_COM_NOT_SUPPORTED;
        case VBOX_E_XML_ERROR:                  return VERR_COM_XML_ERROR;
        case VBOX_E_INVALID_SESSION_STATE:      return VERR_COM_INVALID_SESSION_STATE;
        case VBOX_E_OBJECT_IN_USE:              return VERR_COM_OBJECT_IN_USE;

        default:
            if (SUCCEEDED(aComStatus))
                return VINF_SUCCESS;
            /** @todo Check for the win32 facility and use the
             *        RTErrConvertFromWin32 function on windows. */
            return VERR_UNRESOLVED_ERROR;
    }
}


/*static*/ HRESULT
Global::vboxStatusCodeToCOM(int aVBoxStatus)
{
    switch (aVBoxStatus)
    {
        case VINF_SUCCESS:                      return S_OK;

        /* Standard COM status codes. */
        case VERR_COM_UNEXPECTED:               return E_UNEXPECTED;
        case VERR_NOT_IMPLEMENTED:              return E_NOTIMPL;
        case VERR_NO_MEMORY:                    return E_OUTOFMEMORY;
        case VERR_INVALID_PARAMETER:            return E_INVALIDARG;
        case VERR_NOT_SUPPORTED:                return E_NOINTERFACE;
        case VERR_INVALID_POINTER:              return E_POINTER;
#ifdef E_HANDLE
        case VERR_INVALID_HANDLE:               return E_HANDLE;
#endif
        case VERR_CANCELLED:                    return E_ABORT;
        case VERR_GENERAL_FAILURE:              return E_FAIL;
        case VERR_ACCESS_DENIED:                return E_ACCESSDENIED;

        /* VirtualBox COM status codes */
        case VERR_COM_OBJECT_NOT_FOUND:         return VBOX_E_OBJECT_NOT_FOUND;
        case VERR_COM_INVALID_VM_STATE:         return VBOX_E_INVALID_VM_STATE;
        case VERR_COM_VM_ERROR:                 return VBOX_E_VM_ERROR;
        case VERR_COM_FILE_ERROR:               return VBOX_E_FILE_ERROR;
        case VERR_COM_IPRT_ERROR:               return VBOX_E_IPRT_ERROR;
        case VERR_COM_PDM_ERROR:                return VBOX_E_PDM_ERROR;
        case VERR_COM_INVALID_OBJECT_STATE:     return VBOX_E_INVALID_OBJECT_STATE;
        case VERR_COM_HOST_ERROR:               return VBOX_E_HOST_ERROR;
        case VERR_COM_NOT_SUPPORTED:            return VBOX_E_NOT_SUPPORTED;
        case VERR_COM_XML_ERROR:                return VBOX_E_XML_ERROR;
        case VERR_COM_INVALID_SESSION_STATE:    return VBOX_E_INVALID_SESSION_STATE;
        case VERR_COM_OBJECT_IN_USE:            return VBOX_E_OBJECT_IN_USE;

        /* Other errors. */
        case VERR_UNRESOLVED_ERROR:             return E_FAIL;

        default:
            AssertMsgFailed(("%Rrc\n", aVBoxStatus));
            if (RT_SUCCESS(aVBoxStatus))
                return S_OK;

            /* try categorize it */
            if (   aVBoxStatus < 0
                && (   aVBoxStatus > -1000
                    || (aVBoxStatus < -22000 && aVBoxStatus > -32766) )
               )
                return VBOX_E_IPRT_ERROR;
            if (    aVBoxStatus <  VERR_PDM_NO_SUCH_LUN / 100 * 10
                &&  aVBoxStatus >  VERR_PDM_NO_SUCH_LUN / 100 * 10 - 100)
                return VBOX_E_PDM_ERROR;
            if (    aVBoxStatus <= -1000
                &&  aVBoxStatus >  -5000 /* wrong, but so what... */)
                return VBOX_E_VM_ERROR;

            return E_FAIL;
    }
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
