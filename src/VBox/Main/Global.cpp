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
const Global::OSType Global::sOSTypes [SchemaDefs::OSTypeId_COUNT] =
{
    /* NOTE1: we assume that unknown is always the first entry!
     * NOTE2: please use powers of 2 when specifying the size of harddisks since
     *        '2GB' looks better than '1.95GB' (= 2000MB) */
    { SchemaDefs_OSTypeId_unknown,     "Other/Unknown",       VBOXOSTYPE_Unknown,      64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_dos,         "DOS",                 VBOXOSTYPE_DOS,          32,   4,      512 },
    { SchemaDefs_OSTypeId_win31,       "Windows 3.1",         VBOXOSTYPE_Win31,        32,   4,  1 * _1K },
    { SchemaDefs_OSTypeId_win95,       "Windows 95",          VBOXOSTYPE_Win95,        64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_win98,       "Windows 98",          VBOXOSTYPE_Win98,        64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_winme,       "Windows Me",          VBOXOSTYPE_WinMe,        64,   4,  4 * _1K },
    { SchemaDefs_OSTypeId_winnt4,      "Windows NT 4",        VBOXOSTYPE_WinNT4,      128,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_win2k,       "Windows 2000",        VBOXOSTYPE_Win2k,       168,  12,  4 * _1K },
    { SchemaDefs_OSTypeId_winxp,       "Windows XP",          VBOXOSTYPE_WinXP,       192,  12, 10 * _1K },
    { SchemaDefs_OSTypeId_win2k3,      "Windows Server 2003", VBOXOSTYPE_Win2k3,      256,  12, 20 * _1K },
    { SchemaDefs_OSTypeId_winvista,    "Windows Vista",       VBOXOSTYPE_WinVista,    512,  12, 20 * _1K },
    { SchemaDefs_OSTypeId_win2k8,      "Windows Server 2008", VBOXOSTYPE_Win2k8,      256,  12, 20 * _1K },
    { SchemaDefs_OSTypeId_os2warp3,    "OS/2 Warp 3",         VBOXOSTYPE_OS2Warp3,     48,   4,  1 * _1K },
    { SchemaDefs_OSTypeId_os2warp4,    "OS/2 Warp 4",         VBOXOSTYPE_OS2Warp4,     64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_os2warp45,   "OS/2 Warp 4.5",       VBOXOSTYPE_OS2Warp45,    96,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_ecs,         "eComStation",         VBOXOSTYPE_ECS,          96,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_linux22,     "Linux 2.2",           VBOXOSTYPE_Linux22,      64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_linux24,     "Linux 2.4",           VBOXOSTYPE_Linux24,     128,   4,  4 * _1K },
    { SchemaDefs_OSTypeId_linux26,     "Linux 2.6",           VBOXOSTYPE_Linux26,     256,   4,  8 * _1K },
    { SchemaDefs_OSTypeId_archlinux,   "Arch Linux",          VBOXOSTYPE_ArchLinux,   256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_debian,      "Debian",              VBOXOSTYPE_Debian,      256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_opensuse,    "openSUSE",            VBOXOSTYPE_OpenSUSE,    256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_fedoracore,  "Fedora",              VBOXOSTYPE_FedoraCore,  256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_gentoo,      "Gentoo Linux",        VBOXOSTYPE_Gentoo,      256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_mandriva,    "Mandriva",            VBOXOSTYPE_Mandriva,    256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_redhat,      "Red Hat",             VBOXOSTYPE_RedHat,      256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_ubuntu,      "Ubuntu",              VBOXOSTYPE_Ubuntu,      256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_xandros,     "Xandros",             VBOXOSTYPE_Xandros,     256,  12,  8 * _1K },
    { SchemaDefs_OSTypeId_freebsd,     "FreeBSD",             VBOXOSTYPE_FreeBSD,      64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_openbsd,     "OpenBSD",             VBOXOSTYPE_OpenBSD,      64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_netbsd,      "NetBSD",              VBOXOSTYPE_NetBSD,       64,   4,  2 * _1K },
    { SchemaDefs_OSTypeId_netware,     "Netware",             VBOXOSTYPE_Netware,     512,   4,  4 * _1K },
    { SchemaDefs_OSTypeId_solaris,     "Solaris",             VBOXOSTYPE_Solaris,     512,  12, 16 * _1K },
    { SchemaDefs_OSTypeId_opensolaris, "OpenSolaris",         VBOXOSTYPE_OpenSolaris, 512,  12, 16 * _1K },
    { SchemaDefs_OSTypeId_l4,          "L4",                  VBOXOSTYPE_L4,           64,   4,  2 * _1K }
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

