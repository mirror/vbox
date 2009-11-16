/* $Id$ */
/** @file
 * DevFwCommon - shared header for code common between different firmware types (EFI, BIOS).   
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef DEV_FWCOMMON_H
#define DEV_FWCOMMON_H

#include "DevPcBios.h"

/** @def VBOX_MPS_TABLE_BASE
 *
 * Must be located in the same page as the DMI table.
 */
#define VBOX_MPS_TABLE_BASE          0xe1100

/* Plant DMI table */
int sharedfwPlantDMITable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, PRTUUID pUuid, PCFGMNODE pCfgHandle);

/* Plant MPS table */
void sharedfwPlantMpsTable(PPDMDEVINS pDevIns, uint8_t *pTable, uint16_t numCpus);

#endif
