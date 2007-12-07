/** @file
 *
 * PCI Internal header - Only for hiding bits of PCIDEVICE.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __PCIInternal_h__
#define __PCIInternal_h__

/**
 * PCI I/O region.
 */
typedef struct PCIIOREGION
{
    /** Current PCI mapping address.
     * -1 means not mapped. Memory addresses are relative to pci_mem_base. */
    uint32_t                        addr;
    uint32_t                        size;
    uint8_t                         type; /* PCIADDRESSSPACE */
    uint8_t                         padding[HC_ARCH_BITS == 32 ? 3 : 7];
    /** Callback called when the region is mapped. */
    R3PTRTYPE(PFNPCIIOREGIONMAP)    map_func;
} PCIIOREGION, PCIIORegion;
/** Pointer to PCI I/O region. */
typedef PCIIOREGION *PPCIIOREGION;

/**
 * PCI Device - Internal data.
 */
typedef struct PCIDEVICEINT
{
    /** I/O regions. */
    PCIIOREGION                     aIORegions[PCI_NUM_REGIONS];
    /** Pointer to the PCI bus of the device. */
    R3PTRTYPE(struct PCIBus *)      pBus;
    /** Read config callback. */
    R3PTRTYPE(PFNPCICONFIGREAD)     pfnConfigRead;
    /** Write config callback. */
    R3PTRTYPE(PFNPCICONFIGWRITE)    pfnConfigWrite;
    /** The irq assigned to the device. */
    int32_t                         iIrq;
    /** Set if the specific device fun was requested by PDM.
     * If clear the device and it's functions can be relocated to satisfy the slot request of another device. */
    bool                            fRequestedDevFn;
} PCIDEVICEINT;

/* Indicate that PCIDEVICE::Int.s can be declared. */
#define __PCIDEVICEINT_DECLARED__

#endif
