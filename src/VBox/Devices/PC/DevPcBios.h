/** @file
 *
 * VBox basic PC devices:
 * PC BIOS device
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef DEV_PCBIOS_H
#define DEV_PCBIOS_H

#define VBOX_DMI_TABLE_ENTR          2
#define VBOX_DMI_TABLE_SIZE          160
#define VBOX_DMI_TABLE_BASE          0xe1000
#define VBOX_DMI_TABLE_VER           0x23

#define VBOX_MPS_TABLE_BASE          0xe1100

#define VBOX_LANBOOT_SEG             0xca00

#endif
