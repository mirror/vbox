/** @file
 * PCI - The PCI Controller And Devices.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_pci_h
#define ___VBox_pci_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

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
 * @param   GCPhysAddress   Physical address of the region. If enmType is PCI_ADDRESS_SPACE_IO, this
 *                          is an I/O port, otherwise it's a physical address.
 *
 *                          NIL_RTGCPHYS indicates that a MMIO2 mapping is about to be unmapped and
 *                          that the device deregister access handlers for it and update its internal
 *                          state to reflect this.
 *
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 *
 * @remarks The address is *NOT* relative to pci_mem_base.
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
#define VBOX_PCI_CLASS_PROG             0x09    /**<  8-bit RO - - register-level programming class code (device specific). */
#define VBOX_PCI_CLASS_SUB              0x0a    /**<  8-bit RO - - sub-class code. */
#define VBOX_PCI_CLASS_DEVICE           VBOX_PCI_CLASS_SUB
#define VBOX_PCI_CLASS_BASE             0x0b    /**<  8-bit RO - - base class code. */
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
 * Sets the command config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u16Command      The command register value.
 */
DECLINLINE(void) PCIDevSetCommand(PPCIDEVICE pPciDev, uint16_t u16Command)
{
    u16Command = RT_H2LE_U16(u16Command);
    pPciDev->config[VBOX_PCI_COMMAND]     = u16Command & 0xff;
    pPciDev->config[VBOX_PCI_COMMAND + 1] = u16Command >> 8;
}


/**
 * Sets the status config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u16Status       The status register value.
 */
DECLINLINE(void) PCIDevSetStatus(PPCIDEVICE pPciDev, uint16_t u16Status)
{
    u16Status = RT_H2LE_U16(u16Status);
    pPciDev->config[VBOX_PCI_STATUS]     = u16Status & 0xff;
    pPciDev->config[VBOX_PCI_STATUS + 1] = u16Status >> 8;
}


/**
 * Sets the revision id config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8RevisionId    The revision id.
 */
DECLINLINE(void) PCIDevSetRevisionId(PPCIDEVICE pPciDev, uint8_t u8RevisionId)
{
    pPciDev->config[VBOX_PCI_REVISION_ID] = u8RevisionId;
}


/**
 * Sets the register level programming class config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8ClassProg     The new value.
 */
DECLINLINE(void) PCIDevSetClassProg(PPCIDEVICE pPciDev, uint8_t u8ClassProg)
{
    pPciDev->config[VBOX_PCI_CLASS_PROG] = u8ClassProg;
}


/**
 * Sets the sub-class (aka device class) config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8SubClass      The sub-class.
 */
DECLINLINE(void) PCIDevSetClassSub(PPCIDEVICE pPciDev, uint8_t u8SubClass)
{
    pPciDev->config[VBOX_PCI_CLASS_SUB] = u8SubClass;
}


/**
 * Sets the base class config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8BaseClass     The base class.
 */
DECLINLINE(void) PCIDevSetClassBase(PPCIDEVICE pPciDev, uint8_t u8BaseClass)
{
    pPciDev->config[VBOX_PCI_CLASS_BASE] = u8BaseClass;
}


/**
 * Sets the header type config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8HdrType       The header type.
 */
DECLINLINE(void) PCIDevSetHeaderType(PPCIDEVICE pPciDev, uint8_t u8HdrType)
{
    pPciDev->config[VBOX_PCI_HEADER_TYPE] = u8HdrType;
}


/**
 * Sets a base address config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   fIOSpace        Whether it's I/O (true) or memory (false) space.
 * @param   fPrefetchable   Whether the memory is prefetachable. Must be false if fIOSpace == true.
 * @param   f64Bit          Whether the memory can be mapped anywhere in the 64-bit address space. Otherwise restrict to 32-bit.
 * @param   u32Addr         The address value.
 */
DECLINLINE(void) PCIDevSetBaseAddress(PPCIDEVICE pPciDev, uint8_t iReg, bool fIOSpace, bool fPrefetchable, bool f64Bit, uint32_t u32Addr)
{
    if (fIOSpace)
    {
        Assert(!(u32Addr & 0x3)); Assert(!fPrefetchable); Assert(!f64Bit);
        u32Addr |= RT_BIT_32(0);
    }
    else
    {
        Assert(!(u32Addr & 0xf));
        if (fPrefetchable)
            u32Addr |= RT_BIT_32(3);
        if (f64Bit)
            u32Addr |= 0x2 << 1;
    }
    switch (iReg)
    {
        case 0: iReg = VBOX_PCI_BASE_ADDRESS_0; break;
        case 1: iReg = VBOX_PCI_BASE_ADDRESS_1; break;
        case 2: iReg = VBOX_PCI_BASE_ADDRESS_2; break;
        case 3: iReg = VBOX_PCI_BASE_ADDRESS_3; break;
        case 4: iReg = VBOX_PCI_BASE_ADDRESS_4; break;
        case 5: iReg = VBOX_PCI_BASE_ADDRESS_5; break;
        default: AssertFailedReturnVoid();
    }

    u32Addr = RT_H2LE_U32(u32Addr);
    pPciDev->config[iReg]     = u32Addr         & 0xff;
    pPciDev->config[iReg + 1] = (u32Addr >> 16) & 0xff;
    pPciDev->config[iReg + 2] = (u32Addr >> 16) & 0xff;
    pPciDev->config[iReg + 3] = (u32Addr >> 24) & 0xff;
}


/**
 * Sets the sub-system vendor id config register.
 *
 * @param   pPciDev             The PCI device.
 * @param   u16SubSysVendorId   The sub-system vendor id.
 */
DECLINLINE(void) PCIDevSetSubSystemVendorId(PPCIDEVICE pPciDev, uint16_t u16SubSysVendorId)
{
    u16SubSysVendorId = RT_H2LE_U16(u16SubSysVendorId);
    pPciDev->config[VBOX_PCI_SUBSYSTEM_VENDOR_ID]     = u16SubSysVendorId & 0xff;
    pPciDev->config[VBOX_PCI_SUBSYSTEM_VENDOR_ID + 1] = u16SubSysVendorId >> 8;
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
 * Sets the sub-system id config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u16SubSystemId  The sub-system id.
 */
DECLINLINE(void) PCIDevSetSubSystemId(PPCIDEVICE pPciDev, uint16_t u16SubSystemId)
{
    u16SubSystemId = RT_H2LE_U16(u16SubSystemId);
    pPciDev->config[VBOX_PCI_SUBSYSTEM_ID]     = u16SubSystemId & 0xff;
    pPciDev->config[VBOX_PCI_SUBSYSTEM_ID + 1] = u16SubSystemId >> 8;
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


/**
 * Sets the interrupt line config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8Line          The interrupt line.
 */
DECLINLINE(void) PCIDevSetInterruptLine(PPCIDEVICE pPciDev, uint8_t u8Line)
{
    pPciDev->config[VBOX_PCI_INTERRUPT_LINE] = u8Line;
}


/**
 * Sets the interrupt pin config register.
 *
 * @param   pPciDev         The PCI device.
 * @param   u8Pin           The interrupt pin.
 */
DECLINLINE(void) PCIDevSetInterruptPin(PPCIDEVICE pPciDev, uint8_t u8Pin)
{
    pPciDev->config[VBOX_PCI_INTERRUPT_PIN] = u8Pin;
}


/** @} */

#endif
