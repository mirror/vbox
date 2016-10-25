/* $Id$ */
/** @file
 * DevPCI - Common Internal Header.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___Bus_DevPciInternal_h___
#define ___Bus_DevPciInternal_h___

#ifndef PDMPCIDEV_INCLUDE_PRIVATE
# define PDMPCIDEV_INCLUDE_PRIVATE /* Hack to get pdmpcidevint.h included at the right point. */
#endif
#include <VBox/vmm/pdmdev.h>


/**
 * PCI bus instance (common to both).
 */
typedef struct DEVPCIBUS
{
    /** Bus number. */
    int32_t                 iBus;
    /** Number of bridges attached to the bus. */
    uint32_t                cBridges;
    /** Start device number - always zero (only for DevPCI source compat). */
    uint32_t                iDevSearch;
    /** Size alignemnt padding. */
    uint32_t                u32Alignment0;

    /** R3 pointer to the device instance. */
    PPDMDEVINSR3            pDevInsR3;
    /** Pointer to the PCI R3  helpers. */
    PCPDMPCIHLPR3           pPciHlpR3;

    /** R0 pointer to the device instance. */
    PPDMDEVINSR0            pDevInsR0;
    /** Pointer to the PCI R0 helpers. */
    PCPDMPCIHLPR0           pPciHlpR0;

    /** RC pointer to the device instance. */
    PPDMDEVINSRC            pDevInsRC;
    /** Pointer to the PCI RC helpers. */
    PCPDMPCIHLPRC           pPciHlpRC;

    /** Array of bridges attached to the bus. */
    R3PTRTYPE(PPDMPCIDEV *) papBridgesR3;
#if HC_ARCH_BITS == 32
    uint32_t                au32Alignment1[5]; /**< Cache line align apDevices. */
#endif
    /** Array of PCI devices. We assume 32 slots, each with 8 functions. */
    R3PTRTYPE(PPDMPCIDEV)   apDevices[256];

    /** The PCI device for the PCI bridge. */
    PDMPCIDEV               PciDev;
} DEVPCIBUS;
/** Pointer to a PCI bus instance.   */
typedef DEVPCIBUS *PDEVPCIBUS;


#endif

