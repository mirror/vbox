/* $Id$ */

/** @file
 *
 * VirtualBox COM global definitions
 *
 * NOTE: This file is part of both VBoxC.dll and VBoxSVC.exe.
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

#include "Global.h"

#include <iprt/assert.h>

/* static */
const Global::OSType Global::sOSTypes[SchemaDefs::OSTypeId_COUNT] =
{
    /* NOTE1: we assume that unknown is always the first entry!
     * NOTE2: please use powers of 2 when specifying the size of harddisks since
     *        '2GB' looks better than '1.95GB' (= 2000MB) */
    { "Other",   "Other",             SchemaDefs_OSTypeId_Other,           "Other/Unknown",
      VBOXOSTYPE_Unknown,         VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows31,       "Windows 3.1",
      VBOXOSTYPE_Win31,           VBOXOSHINT_NONE,  32,   4,  1 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows95,       "Windows 95",
      VBOXOSTYPE_Win95,           VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows98,       "Windows 98",
      VBOXOSTYPE_Win98,           VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsMe,       "Windows Me",
      VBOXOSTYPE_WinMe,           VBOXOSHINT_NONE,  64,   4,  4 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsNT4,      "Windows NT 4",
      VBOXOSTYPE_WinNT4,          VBOXOSHINT_NONE, 128,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows2000,     "Windows 2000",
      VBOXOSTYPE_Win2k,           VBOXOSHINT_NONE, 168,  12,  4 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsXP,       "Windows XP",
      VBOXOSTYPE_WinXP,           VBOXOSHINT_NONE, 192,  12, 10 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsXP_64,    "Windows XP (64 bit)",
      VBOXOSTYPE_WinXP_x64,       VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  192,  12, 10 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows2003,     "Windows 2003",
      VBOXOSTYPE_Win2k3,          VBOXOSHINT_NONE, 256,  12, 20 * _1K, NetworkAdapterType_Am79C973 },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows2003_64,  "Windows 2003 (64 bit)",
      VBOXOSTYPE_Win2k3_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsVista,    "Windows Vista",
      VBOXOSTYPE_WinVista,        VBOXOSHINT_NONE, 512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsVista_64, "Windows Vista (64 bit)",
      VBOXOSTYPE_WinVista_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows2008,     "Windows 2008",
      VBOXOSTYPE_Win2k8,          VBOXOSHINT_NONE, 512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows2008_64,  "Windows 2008 (64 bit)",
      VBOXOSTYPE_Win2k8_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows7,        "Windows 7",
      VBOXOSTYPE_Win7,            VBOXOSHINT_NONE, 512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_Windows7_64,  "Windows 7 (64 bit)",
      VBOXOSTYPE_Win7_x64,        VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  512,  12, 20 * _1K, NetworkAdapterType_I82540EM },
    { "Windows", "Microsoft Windows", SchemaDefs_OSTypeId_WindowsNT,       "Other Windows",
      VBOXOSTYPE_WinNT,           VBOXOSHINT_NONE, 512,  12, 20 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux22,         "Linux 2.2",
      VBOXOSTYPE_Linux22,         VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux24,         "Linux 2.4",
      VBOXOSTYPE_Linux24,         VBOXOSHINT_NONE, 128,   4,  4 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux24_64,      "Linux 2.4 (64 bit)",
      VBOXOSTYPE_Linux24_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  128,   4,  4 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux26,         "Linux 2.6",
      VBOXOSTYPE_Linux26,         VBOXOSHINT_NONE, 256,   4,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux26_64,      "Linux 2.6 (64 bit)",
      VBOXOSTYPE_Linux26_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,   4,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_ArchLinux,       "Arch Linux",
      VBOXOSTYPE_ArchLinux,       VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_ArchLinux_64,    "Arch Linux (64 bit)",
      VBOXOSTYPE_ArchLinux_x64,   VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Debian,          "Debian",
      VBOXOSTYPE_Debian,          VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Debian_64,       "Debian (64 bit)",
      VBOXOSTYPE_Debian_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_OpenSUSE,        "openSUSE",
      VBOXOSTYPE_OpenSUSE,        VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_OpenSUSE_64,     "openSUSE (64 bit)",
      VBOXOSTYPE_OpenSUSE_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Fedora,          "Fedora",
      VBOXOSTYPE_FedoraCore,      VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Fedora_64,       "Fedora (64 bit)",
      VBOXOSTYPE_FedoraCore_x64,  VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Gentoo,          "Gentoo",
      VBOXOSTYPE_Gentoo,          VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Gentoo_64,       "Gentoo (64 bit)",
      VBOXOSTYPE_Gentoo_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Mandriva,        "Mandriva",
      VBOXOSTYPE_Mandriva,        VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Mandriva_64,     "Mandriva (64 bit)",
      VBOXOSTYPE_Mandriva_x64,    VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_RedHat,          "Red Hat",
      VBOXOSTYPE_RedHat,          VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_RedHat_64,       "Red Hat (64 bit)",
      VBOXOSTYPE_RedHat_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Ubuntu,          "Ubuntu",
      VBOXOSTYPE_Ubuntu,          VBOXOSHINT_NONE, 384,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Ubuntu_64,       "Ubuntu (64 bit)",
      VBOXOSTYPE_Ubuntu_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  384,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Xandros,         "Xandros",
      VBOXOSTYPE_Xandros,         VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Xandros_64,      "Xandros (64 bit)",
      VBOXOSTYPE_Xandros_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  256,  12,  8 * _1K, NetworkAdapterType_I82540EM },
    { "Linux",   "Linux",             SchemaDefs_OSTypeId_Linux,           "Other Linux",
      VBOXOSTYPE_Linux,           VBOXOSHINT_NONE, 256,  12,  8 * _1K, NetworkAdapterType_Am79C973 },
    { "Solaris", "Solaris",           SchemaDefs_OSTypeId_Solaris,         "Solaris",
      VBOXOSTYPE_Solaris,         VBOXOSHINT_NONE, 512,  12, 16 * _1K, NetworkAdapterType_I82540EM },
    { "Solaris", "Solaris",           SchemaDefs_OSTypeId_Solaris_64,      "Solaris (64 bit)",
      VBOXOSTYPE_Solaris_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  512,  12, 16 * _1K, NetworkAdapterType_I82540EM },
    { "Solaris", "Solaris",           SchemaDefs_OSTypeId_OpenSolaris,     "OpenSolaris",
      VBOXOSTYPE_OpenSolaris,     VBOXOSHINT_NONE, 512,  12, 16 * _1K, NetworkAdapterType_I82540EM },
    { "Solaris", "Solaris",           SchemaDefs_OSTypeId_OpenSolaris_64,  "OpenSolaris (64 bit)",
      VBOXOSTYPE_OpenSolaris_x64, VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  512,  12, 16 * _1K, NetworkAdapterType_I82540EM },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_FreeBSD,         "FreeBSD",
      VBOXOSTYPE_FreeBSD,         VBOXOSHINT_NONE, 128,   4,  2 * _1K, NetworkAdapterType_I82540EM },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_FreeBSD_64,      "FreeBSD (64 bit)",
      VBOXOSTYPE_FreeBSD_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,  128,   4,  2 * _1K, NetworkAdapterType_I82540EM },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_OpenBSD,         "OpenBSD",
      VBOXOSTYPE_OpenBSD,         VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_OpenBSD_64,      "OpenBSD (64 bit)",
      VBOXOSTYPE_OpenBSD_x64,     VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,   64,   4,  2 * _1K, NetworkAdapterType_I82540EM },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_NetBSD,          "NetBSD",
      VBOXOSTYPE_NetBSD,          VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "BSD",     "BSD",               SchemaDefs_OSTypeId_NetBSD_64,       "NetBSD (64 bit)",
      VBOXOSTYPE_NetBSD_x64,      VBOXOSHINT_64BIT | VBOXOSHINT_HWVIRTEX | VBOXOSHINT_IOAPIC,   64,   4,  2 * _1K, NetworkAdapterType_I82540EM },
    { "OS2",     "IBM OS/2",          SchemaDefs_OSTypeId_OS2Warp3,        "OS/2 Warp 3",
      VBOXOSTYPE_OS2Warp3,        VBOXOSHINT_HWVIRTEX,   48,   4,  1 * _1K, NetworkAdapterType_Am79C973 },
    { "OS2",     "IBM OS/2",          SchemaDefs_OSTypeId_OS2Warp4,        "OS/2 Warp 4",
      VBOXOSTYPE_OS2Warp4,        VBOXOSHINT_HWVIRTEX,   64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "OS2",     "IBM OS/2",          SchemaDefs_OSTypeId_OS2Warp45,       "OS/2 Warp 4.5",
      VBOXOSTYPE_OS2Warp45,       VBOXOSHINT_HWVIRTEX,   96,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "OS2",     "IBM OS/2",          SchemaDefs_OSTypeId_OS2eCS,          "eComStation",
      VBOXOSTYPE_ECS,             VBOXOSHINT_HWVIRTEX,   96,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "OS2",     "IBM OS/2",          SchemaDefs_OSTypeId_OS2,             "Other OS/2",
      VBOXOSTYPE_OS2,             VBOXOSHINT_HWVIRTEX,   96,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Other",   "Other",             SchemaDefs_OSTypeId_DOS,             "DOS",
      VBOXOSTYPE_DOS,             VBOXOSHINT_NONE,  32,   4,      512, NetworkAdapterType_Am79C973 },
    { "Other",   "Other",             SchemaDefs_OSTypeId_Netware,         "Netware",
      VBOXOSTYPE_Netware,         VBOXOSHINT_NONE, 512,   4,  4 * _1K, NetworkAdapterType_Am79C973 },
    { "Other",   "Other",             SchemaDefs_OSTypeId_L4,              "L4",
      VBOXOSTYPE_L4,              VBOXOSHINT_NONE,  64,   4,  2 * _1K, NetworkAdapterType_Am79C973 },
    { "Other",   "Other",             SchemaDefs_OSTypeId_QNX,             "QNX",
      VBOXOSTYPE_QNX,             VBOXOSHINT_HWVIRTEX,  512,   4,  4 * _1K, NetworkAdapterType_Am79C973 }
};

/**
 * Returns an OS Type ID for the given VBOXOSTYPE value.
 *
 * The returned ID will correspond to the IGuestOSType::id value of one of the
 * objects stored in the IVirtualBox::guestOSTypes
 * (VirtualBoxImpl::COMGETTER(GuestOSTypes)) collection.
 */
/* static */
const char *Global::OSTypeId (VBOXOSTYPE aOSType)
{
    for (size_t i = 0; i < RT_ELEMENTS (sOSTypes); ++ i)
    {
        if (sOSTypes [i].osType == aOSType)
            return sOSTypes [i].id;
    }

    AssertMsgFailed (("No record for VBOXOSTYPE %d\n", aOSType));
    return sOSTypes [0].id;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
