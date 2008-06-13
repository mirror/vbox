/* $Id$ */
/** @file
 * PC BIOS Device Header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef DEV_PCBIOS_H
#define DEV_PCBIOS_H

/** @def VBOX_DMI_TABLE_BASE */
#define VBOX_DMI_TABLE_BASE          0xe1000
#define VBOX_DMI_TABLE_VER           0x25
#define VBOX_DMI_TABLE_ENTR          3
#define VBOX_DMI_TABLE_SIZE          0x100


/** @def MPS_TABLE_BASE
 *
 * Must be located in the same page as the DMI table.
 */
#define VBOX_MPS_TABLE_BASE          0xe1100

#define VBOX_SMBIOS_MAJOR_VER        2
#define VBOX_SMBIOS_MINOR_VER        5
#define VBOX_SMBIOS_MAXSS            0xff   /* Not very accurate */


/** @def VBOX_VMI_TABLE_BASE
 *
 * Must be located between 0xC0000 and 0xDEFFF, otherwise it will not be
 * recognized as regular BIOS.
 */
#define VBOX_VMI_BIOS_BASE           0xdf000


/** @def VBOX_LANBOOT_SEG
 */
#define VBOX_LANBOOT_SEG             0xe200

#endif

