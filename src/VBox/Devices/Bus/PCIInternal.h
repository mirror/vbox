/* $Id$ */
/** @file
 * DevPCI - PCI Internal header - Only for hiding bits of PCIDEVICE.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
    /** Current PCI mapping address, 0xffffffff means not mapped. */
    uint64_t                        addr;
    uint64_t                        size;
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

/* Forward declaration */
struct PCIBus;

enum {
    /** Set if the specific device function was requested by PDM.
     * If clear the device and it's functions can be relocated to satisfy the slot request of another device. */
    PCIDEV_FLAG_REQUESTED_DEVFUNC  = 1<<0,
    /** Flag whether the device is a pci-to-pci bridge.
     * This is set prior to device registration.  */
    PCIDEV_FLAG_PCI_TO_PCI_BRIDGE  = 1<<1,
    /** Flag whether the device is a PCI Express device.
     * This is set prior to device registration.  */
    PCIDEV_FLAG_PCI_EXPRESS_DEVICE = 1<<2,
    /** Flag whether the device is capable of MSI.
     * This one is set by MsiInit().  */
    PCIDEV_FLAG_MSI_CAPABLE        = 1<<3,
    /** Flag whether the device is capable of MSI-X.
     * This one is set by MsixInit().  */
    PCIDEV_FLAG_MSIX_CAPABLE       = 1<<4,
    /** Flag if device represents real physical device in passthrough mode. */
    PCIDEV_FLAG_PASSTHROUGH        = 1<<5,
    /** Flag whether the device is capable of MSI using 64-bit address.  */
    PCIDEV_FLAG_MSI64_CAPABLE      = 1<<6

};

/**
 * PCI Device - Internal data.
 */
typedef struct PCIDEVICEINT
{
    /** I/O regions. */
    PCIIOREGION                     aIORegions[PCI_NUM_REGIONS];
    /** Pointer to the PCI bus of the device. (R3 ptr) */
    R3PTRTYPE(struct PCIBus *)      pBusR3;
    /** Pointer to the PCI bus of the device. (R0 ptr) */
    R0PTRTYPE(struct PCIBus *)      pBusR0;
    /** Pointer to the PCI bus of the device. (RC ptr) */
    RCPTRTYPE(struct PCIBus *)      pBusRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                         Alignment0;
#endif

    /** Page used for MSI-X state.             (R3 ptr) */
    R3PTRTYPE(void*)                pMsixPageR3;
    /** Page used for MSI-X state.             (R0 ptr) */
    R0PTRTYPE(void*)                pMsixPageR0;
    /** Page used for MSI-X state.             (RC ptr) */
    RCPTRTYPE(void*)                pMsixPageRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                         Alignment1;
#endif


    /** Read config callback. */
    R3PTRTYPE(PFNPCICONFIGREAD)     pfnConfigRead;
    /** Write config callback. */
    R3PTRTYPE(PFNPCICONFIGWRITE)    pfnConfigWrite;

    /** Flags of this PCI device, see PCIDEV_FLAG_XXX constants. */
    uint32_t                        fFlags;
    /** Current state of the IRQ pin of the device. */
    int32_t                         uIrqPinState;

    /** Offset of MSI PCI capability in config space, or 0. */
    uint8_t                         u8MsiCapOffset;
    /** Size of MSI PCI capability in config space, or 0. */
    uint8_t                         u8MsiCapSize;
    /** Offset of MSI-X PCI capability in config space, or 0. */
    uint8_t                         u8MsixCapOffset;
    /** Size of MSI-X PCI capability in config space, or 0. */
    uint8_t                         u8MsixCapSize;

    uint32_t                        Alignment2;

    /** Pointer to bus specific data.                 (R3 ptr) */
    R3PTRTYPE(const void*)          pPciBusPtrR3;

    /** Read config callback for PCI bridges to pass requests
     *  to devices on another bus.
     */
    R3PTRTYPE(PFNPCIBRIDGECONFIGREAD) pfnBridgeConfigRead;
    /** Write config callback for PCI bridges to pass requests
     *  to devices on another bus.
     */
    R3PTRTYPE(PFNPCIBRIDGECONFIGWRITE) pfnBridgeConfigWrite;

} PCIDEVICEINT;

/** Indicate that PCIDEVICE::Int.s can be declared. */
#define PCIDEVICEINT_DECLARED

#endif
