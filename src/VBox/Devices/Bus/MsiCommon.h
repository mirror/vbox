/* $Id$ */
/** @file
 * Header for MSI support routines.
 */
/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Maybe belongs to types.h */
#ifdef IN_RING3
typedef PCPDMPCIHLPR3 PCPDMPCIHLP;
#endif

#ifdef IN_RING0
typedef PCPDMPCIHLPR0 PCPDMPCIHLP;
#endif

#ifdef IN_RC
typedef PCPDMPCIHLPRC PCPDMPCIHLP;
#endif

/* Init MSI support in the device. */
int      MsiInit(PPCIDEVICE pDev, PPDMMSIREG pMsiReg);

/* If MSI is enabled, so that MSINotify() shall be used for notifications.  */
bool     MsiIsEnabled(PPCIDEVICE pDev);

/* Device notification (aka interrupt). */
void     MsiNotify(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, int iVector);

/* PCI config space accessors for MSI registers */
void     MsiPciConfigWrite(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, uint32_t u32Address, uint32_t val, unsigned len);
uint32_t MsiPciConfigRead (PPDMDEVINS pDevIns, PPCIDEVICE pDev, uint32_t u32Address, unsigned len);
