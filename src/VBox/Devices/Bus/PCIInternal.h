/** @file
 *
 * PCI Internal header - Only for hiding bits of PCIDEVICE.
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
    HCPTRTYPE(PFNPCIIOREGIONMAP)    map_func;
} PCIIOREGION, PCIIORegion;
/** Pointer to PCI I/O region. */
typedef PCIIOREGION *PPCIIOREGION;

/** Pointer to pci_default_read_config. */
typedef uint32_t    (*PFNPCICONFIGREAD)(PPCIDEVICE pPciDev, uint32_t u32CfgAddress, int cb);
/** Pointer to pci_default_write_config. */
typedef void        (*PFNPCICONFIGWRITE)(PPCIDEVICE pPciDev, uint32_t u32CfgAddress, uint32_t u32Data, int cb);

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
