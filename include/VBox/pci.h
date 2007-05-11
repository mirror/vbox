/** @file
 * PCI - The PCI Controller And Devices.
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


#ifndef __VBox_pci_h__
#define __VBox_pci_h__


#include <VBox/cdefs.h>
#include <VBox/types.h>

/** @defgroup grp_pci       PCI - The PCI Controller.
 * @{
 */

/** Pointer to a PCI device. */
typedef struct PCIDevice *PPCIDEVICE;


/**
 * PCI configuration word 4 (command) and word 6 (status).
 */
typedef enum PCICONFIGCOMMAND
{
    /** Supports/uses memory accesses. */
    PCI_COMMAND_IOACCESS = 0x0001,
    PCI_COMMAND_MEMACCESS = 0x0002,
    PCI_COMMAND_BUSMASTER = 0x0004
} PCICONFIGCOMMAND;


/**
 * PCI Address space specification.
 * This is used when registering a I/O region.
 */
typedef enum PCIADDRESSSPACE
{
    /** Memory. */
    PCI_ADDRESS_SPACE_MEM = 0x00,
    /** I/O space. */
    PCI_ADDRESS_SPACE_IO = 0x01,
    /** Prefetch memory. */
    PCI_ADDRESS_SPACE_MEM_PREFETCH = 0x08
} PCIADDRESSSPACE;


/**
 * Callback function for mapping an PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
typedef DECLCALLBACK(int) FNPCIIOREGIONMAP(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType);
/** Pointer to a FNPCIIOREGIONMAP() function. */
typedef FNPCIIOREGIONMAP *PFNPCIIOREGIONMAP;

/** 
 * Callback function for reading from the PCI configuration space.
 *
 * @returns The register value.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   Address         The configuration space register address. [0..255]
 * @param   cb              The register size. [1,2,4]
 */
typedef DECLCALLBACK(uint32_t) FNPCICONFIGREAD(PPCIDEVICE pPciDev, uint32_t Address, unsigned cb);
/** Pointer to a FNPCICONFIGREAD() function. */
typedef FNPCICONFIGREAD *PFNPCICONFIGREAD;
/** Pointer to a PFNPCICONFIGREAD. */
typedef PFNPCICONFIGREAD *PPFNPCICONFIGREAD;

/** 
 * Callback function for writing to the PCI configuration space.
 *
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   Address         The configuration space register address. [0..255]
 * @param   u32Value        The value that's being written. The number of bits actually used from 
 *                          this value is determined by the cb parameter.
 * @param   cb              The register size. [1,2,4]
 */
typedef DECLCALLBACK(void) FNPCICONFIGWRITE(PPCIDEVICE pPciDev, uint32_t Address, uint32_t u32Value, unsigned cb);
/** Pointer to a FNPCICONFIGWRITE() function. */
typedef FNPCICONFIGWRITE *PFNPCICONFIGWRITE;
/** Pointer to a PFNPCICONFIGWRITE. */
typedef PFNPCICONFIGWRITE *PPFNPCICONFIGWRITE;

/** Fixed I/O region number for ROM. */
#define PCI_ROM_SLOT 6
/** Max number of I/O regions. */
#define PCI_NUM_REGIONS 7

/*
 * Hack to include the PCIDEVICEINT structure at the right place
 * to avoid duplications of FNPCIIOREGIONMAP and PCI_NUM_REGIONS.
 */
#ifdef PCI_INCLUDE_PRIVATE
# include "PCIInternal.h"
#endif

/**
 * PCI Device structure.
 */
typedef struct PCIDevice
{
    /** PCI config space. */
    uint8_t                 config[256];

    /** Internal data. */
    union
    {
#ifdef __PCIDEVICEINT_DECLARED__
        PCIDEVICEINT        s;
#endif
        char                padding[224];
    } Int;

    /** Read only data.
     * @{
     */
    /** PCI device number on the pci bus. */
    int32_t                 devfn;
    uint32_t                Alignment0; /**< Alignment. */
    /** Device name. */
    R3PTRTYPE(const char *) name;
    /** Pointer to the device instance which registered the device. */
    PPDMDEVINSR3            pDevIns;
    /**  @} */
} PCIDEVICE;


/** @} */

#endif
