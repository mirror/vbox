/* $Id$ */
/** @file
 * FwCommon - Shared firmware code, header.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_PC_DevFwCommon_h
#define VBOX_INCLUDED_SRC_PC_DevFwCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DevPcBios.h"

/* Plant DMI table */
int FwCommonPlantDMITable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, PCRTUUID pUuid, PCFGMNODE pCfg, uint16_t cCpus, uint16_t *pcbDmiTables, uint16_t *pcNumDmiTables);
void FwCommonPlantSmbiosAndDmiHdrs(PPDMDEVINS pDevIns, uint8_t *pHdr, uint16_t cbDmiTables, uint16_t cNumDmiTables);

/* Plant MPS table */
void FwCommonPlantMpsTable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, uint16_t cCpus);
void FwCommonPlantMpsFloatPtr(PPDMDEVINS pDevIns, uint32_t u32MpTableAddr);

#endif /* !VBOX_INCLUDED_SRC_PC_DevFwCommon_h */
