/** @file
 * PCI - The PCI Controller And Devices.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_pci_h
#define ___VBox_pci_h

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
/** Note: There are all sorts of dirty dependencies on the values in the
 *  pci device. Be careful when changing this.
 *  @todo we should introduce 32 & 64 bits physical address types
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


/** @name PCI Configuration Space Registers
 * @{ */
#define VBOX_PCI_VENDOR_ID              0x00    /**< 16-bit RO */
#define VBOX_PCI_DEVICE_ID              0x02    /**< 16-bit RO */
#define VBOX_PCI_COMMAND                0x04    /**< 16-bit RW */
#define VBOX_PCI_STATUS                 0x06    /**< 16-bit RW */
#define VBOX_PCI_REVISION_ID            0x08    /**<  8-bit RO */
#define VBOX_PCI_CLASS_PROG             0x09    /**<  8-bit RO */
#define VBOX_PCI_CLASS_DEVICE           0x0a    /**<  8-bit ?? */
#define VBOX_PCI_CACHE_LINE_SIZE        0x0c    /**<  8-bit ?? */
#define VBOX_PCI_LATENCY_TIMER          0x0d    /**<  8-bit ?? */
#define VBOX_PCI_HEADER_TYPE            0x0e    /**<  8-bit ?? */
#define VBOX_PCI_BIST                   0x0f    /**<  8-bit ?? */
#define VBOX_PCI_BASE_ADDRESS_0         0x10    /**< 32-bit RW */
#define VBOX_PCI_BASE_ADDRESS_1         0x14    /**< 32-bit RW */
#define VBOX_PCI_BASE_ADDRESS_2         0x18    /**< 32-bit RW */
#define VBOX_PCI_PRIMARY_BUS            0x18    /**<  8-bit ?? - bridge - primary bus number. */
#define VBOX_PCI_SECONDARY_BUS          0x19    /**<  8-bit ?? - bridge - secondary bus number. */
#define VBOX_PCI_SUBORDINATE_BUS        0x1a    /**<  8-bit ?? - bridge - highest subordinate bus number. (behind the bridge) */
#define VBOX_PCI_SEC_LATENCY_TIMER      0x1b    /**<  8-bit ?? - bridge - secondary latency timer. */
#define VBOX_PCI_BASE_ADDRESS_3         0x1c    /**< 32-bit RW */
#define VBOX_PCI_IO_BASE                0x1c    /**<  8-bit ?? - bridge - I/O range base. */
#define VBOX_PCI_IO_LIMIT               0x1d    /**<  8-bit ?? - bridge - I/O range limit. */
#define VBOX_PCI_SEC_STATUS             0x1e    /**< 16-bit ?? - bridge - secondary status register. */
#define VBOX_PCI_BASE_ADDRESS_4         0x20    /**< 32-bit RW */
#define VBOX_PCI_MEMORY_BASE            0x20    /**< 16-bit ?? - bridge - memory range base. */
#define VBOX_PCI_MEMORY_LIMIT           0x22    /**< 16-bit ?? - bridge - memory range limit. */
#define VBOX_PCI_BASE_ADDRESS_5         0x24    /**< 32-bit RW */
#define VBOX_PCI_PREF_MEMORY_BASE       0x24    /**< 16-bit ?? - bridge - Prefetchable memory range base. */
#define VBOX_PCI_PREF_MEMORY_LIMIT      0x26    /**< 16-bit ?? - bridge - Prefetchable memory range limit. */
#define VBOX_PCI_CARDBUS_CIS            0x28    /**< 32-bit ?? */
#define VBOX_PCI_PREF_BASE_UPPER32      0x28    /**< 32-bit ?? - bridge - Prefetchable memory range high base.*/
#define VBOX_PCI_PREF_LIMIT_UPPER32     0x2c    /**< 32-bit ?? - bridge - Prefetchable memory range high limit. */
#define VBOX_PCI_SUBSYSTEM_VENDOR_ID    0x2c    /**< 16-bit ?? */
#define VBOX_PCI_SUBSYSTEM_ID           0x2e    /**< 16-bit ?? */
#define VBOX_PCI_ROM_ADDRESS            0x30    /**< 32-bit ?? */
#define VBOX_PCI_IO_BASE_UPPER16        0x30    /**< 16-bit ?? - bridge - memory range high base. */
#define VBOX_PCI_IO_LIMIT_UPPER16       0x32    /**< 16-bit ?? - bridge - memory range high limit. */
#define VBOX_PCI_CAPABILITY_LIST        0x34    /**< 8-bit? ?? */
#define VBOX_PCI_ROM_ADDRESS_BR         0x38    /**< 32-bit ?? - bridge */
#define VBOX_PCI_INTERRUPT_LINE         0x3c    /**<  8-bit RW - Interrupt line. */
#define VBOX_PCI_INTERRUPT_PIN          0x3d    /**<  8-bit RO - Interrupt pin.  */
#define VBOX_PCI_MIN_GNT                0x3e    /**<  8-bit ?? */
#define VBOX_PCI_BRIDGE_CONTROL         0x3e    /**< 8-bit? ?? - bridge */
#define VBOX_PCI_MAX_LAT                0x3f    /**<  8-bit ?? */
/** @} */


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


/**
 * Sets the vendor id config register.
 * @param   pPciDev         The PCI device.
 * @param   u16VendorId     The vendor id.
 */
DECLINLINE(void) PCIDevSetVendorId(PPCIDEVICE pPciDev, uint16_t u16VendorId)
{
    u16VendorId = RT_H2LE_U16(u16VendorId);
    pPciDev->config[VBOX_PCI_VENDOR_ID]     = u16VendorId & 0xff;
    pPciDev->config[VBOX_PCI_VENDOR_ID + 1] = u16VendorId >> 8;
}

/**
 * Gets the vendor id config register.
 * @returns the vendor id.
 * @param   pPciDev         The PCI device.
 */
DECLINLINE(uint16_t) PCIDevGetVendorId(PPCIDEVICE pPciDev)
{
    return RT_LE2H_U16(RT_MAKE_U16(pPciDev->config[VBOX_PCI_VENDOR_ID], pPciDev->config[VBOX_PCI_VENDOR_ID + 1]));
}

/**
 * Sets the device id config register.
 * @param   pPciDev         The PCI device.
 * @param   u16DeviceId     The device id.
 */
DECLINLINE(void) PCIDevSetDeviceId(PPCIDEVICE pPciDev, uint16_t u16DeviceId)
{
    u16DeviceId = RT_H2LE_U16(u16DeviceId);
    pPciDev->config[VBOX_PCI_DEVICE_ID]     = u16DeviceId & 0xff;
    pPciDev->config[VBOX_PCI_DEVICE_ID + 1] = u16DeviceId >> 8;
}

/**
 * Gets the device id config register.
 * @returns the device id.
 * @param   pPciDev         The PCI device.
 */
DECLINLINE(uint16_t) PCIDevGetDeviceId(PPCIDEVICE pPciDev)
{
    return RT_LE2H_U16(RT_MAKE_U16(pPciDev->config[VBOX_PCI_DEVICE_ID], pPciDev->config[VBOX_PCI_DEVICE_ID + 1]));
}

/**
 * Gets the sub-system vendor id config register.
 * @returns the sub-system vendor id.
 * @param   pPciDev         The PCI device.
 */
DECLINLINE(uint16_t) PCIDevGetSubSystemVendorId(PPCIDEVICE pPciDev)
{
    return RT_LE2H_U16(RT_MAKE_U16(pPciDev->config[VBOX_PCI_SUBSYSTEM_VENDOR_ID], pPciDev->config[VBOX_PCI_SUBSYSTEM_VENDOR_ID + 1]));
}

/**
 * Gets the sub-system id config register.
 * @returns the sub-system id.
 * @param   pPciDev         The PCI device.
 */
DECLINLINE(uint16_t) PCIDevGetSubSystemId(PPCIDEVICE pPciDev)
{
    return RT_LE2H_U16(RT_MAKE_U16(pPciDev->config[VBOX_PCI_SUBSYSTEM_ID], pPciDev->config[VBOX_PCI_SUBSYSTEM_ID + 1]));
}


/** @} */

#endif
