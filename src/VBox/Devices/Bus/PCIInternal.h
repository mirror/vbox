/* $Id$ */
/** @file
 * DevPCI - PCI Internal header - Only for hiding bits of PCIDEVICE.
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
 * Callback function for reading from the PCI configuration space.
 *
 * @returns The register value.
 * @param   pDevIns         Pointer to the device instance of the PCI bus.
 * @param   iBus            The bus number this device is on.
 * @param   iDevice         The number of the device on the bus.
 * @param   Address         The configuration space register address. [0..255]
 * @param   cb              The register size. [1,2,4]
 */
typedef DECLCALLBACK(uint32_t) FNPCIBRIDGECONFIGREAD(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, unsigned cb);
/** Pointer to a FNPCICONFIGREAD() function. */
typedef FNPCIBRIDGECONFIGREAD *PFNPCIBRIDGECONFIGREAD;
/** Pointer to a PFNPCICONFIGREAD. */
typedef PFNPCIBRIDGECONFIGREAD *PPFNPCIBRIDGECONFIGREAD;

/**
 * Callback function for writing to the PCI configuration space.
 *
 * @param   pDevIns         Pointer to the device instance of the PCI bus.
 * @param   iBus            The bus number this device is on.
 * @param   iDevice         The number of the device on the bus.
 * @param   Address         The configuration space register address. [0..255]
 * @param   u32Value        The value that's being written. The number of bits actually used from
 *                          this value is determined by the cb parameter.
 * @param   cb              The register size. [1,2,4]
 */
typedef DECLCALLBACK(void) FNPCIBRIDGECONFIGWRITE(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, uint32_t u32Value, unsigned cb);
/** Pointer to a FNPCICONFIGWRITE() function. */
typedef FNPCIBRIDGECONFIGWRITE *PFNPCIBRIDGECONFIGWRITE;
/** Pointer to a PFNPCICONFIGWRITE. */
typedef PFNPCIBRIDGECONFIGWRITE *PPFNPCIBRIDGECONFIGWRITE;

/**
 * PCI Device - Internal data.
 */
typedef struct PCIDEVICEINT
{
    /** I/O regions. */
    PCIIOREGION                     aIORegions[PCI_NUM_REGIONS];
    /** Pointer to the PCI bus of the device. - R3 ptr */
    R3PTRTYPE(struct PCIBus *)      pBusR3;
    /** Pointer to the PCI bus of the device. - R0 ptr */
    R0PTRTYPE(struct PCIBus *)      pBusR0;
    /** Pointer to the PCI bus of the device. - RC ptr */
    RCPTRTYPE(struct PCIBus *)      pBusRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                         Alignment0;
#endif

    /** Read config callback. */
    R3PTRTYPE(PFNPCICONFIGREAD)     pfnConfigRead;
    /** Write config callback. */
    R3PTRTYPE(PFNPCICONFIGWRITE)    pfnConfigWrite;

    /** Set if the specific device fun was requested by PDM.
     * If clear the device and it's functions can be relocated to satisfy the slot request of another device. */
    bool                            fRequestedDevFn;
    /** Flag whether the device is a pci-to-pci bridge.
     * This is set prior to device registration.  */
    bool                            fPciToPciBridge;
    /** Current state of the IRQ pin of the device. */
    int32_t                         uIrqPinState;

    /** Read config callback for PCI bridges to pass requests
     *  to devices on another bus.
     */
    R3PTRTYPE(PFNPCIBRIDGECONFIGREAD) pfnBridgeConfigRead;
    /** Write config callback for PCI bridges to pass requests
     *  to devices on another bus.
     */
    R3PTRTYPE(PFNPCIBRIDGECONFIGWRITE) pfnBridgeConfigWrite;

} PCIDEVICEINT;

/* Indicate that PCIDEVICE::Int.s can be declared. */
#define PCIDEVICEINT_DECLARED

#endif
