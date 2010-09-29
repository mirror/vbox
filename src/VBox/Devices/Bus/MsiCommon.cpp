/* $Id$ */
/** @file
 * MSI support routines
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
#define LOG_GROUP LOG_GROUP_DEV_PCI
/* Hack to get PCIDEVICEINT declare at the right point - include "PCIInternal.h". */
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/msi.h>
#include <VBox/pdmdev.h>
#include <VBox/log.h>

#include "MsiCommon.h"

DECLINLINE(uint16_t) msiGetMessageControl(PPCIDEVICE pDev)
{
    return PCIDevGetWord(pDev, pDev->Int.s.u8MsiCapOffset + VBOX_MSI_CAP_MESSAGE_CONTROL);
}

DECLINLINE(bool) msiIs64Bit(PPCIDEVICE pDev)
{
    return (msiGetMessageControl(pDev) & VBOX_PCI_MSI_FLAGS_64BIT != 0);
}

DECLINLINE(bool) msiIsEnabled(PPCIDEVICE pDev)
{
    return msiGetMessageControl(pDev) & VBOX_PCI_MSI_FLAGS_ENABLE;
}

DECLINLINE(bool) msiIsMME(PPCIDEVICE pDev)
{
    return (msiGetMessageControl(pDev) & VBOX_PCI_MSI_FLAGS_QSIZE) != 0;
}

DECLINLINE(RTGCPHYS) msiGetMsiAddress(PPCIDEVICE pDev)
{
    if (msiIs64Bit(pDev))
    {
        uint32_t lo = PCIDevGetDWord(pDev, pDev->Int.s.u8MsiCapOffset + VBOX_MSI_CAP_MESSAGE_ADDRESS_LO);
        uint32_t hi = PCIDevGetDWord(pDev, pDev->Int.s.u8MsiCapOffset + VBOX_MSI_CAP_MESSAGE_ADDRESS_HI);
        return RT_MAKE_U64(lo, hi);
    }
    else
    {
        return PCIDevGetDWord(pDev, pDev->Int.s.u8MsiCapOffset + VBOX_MSI_CAP_MESSAGE_ADDRESS_32);
    }
}

DECLINLINE(uint32_t) msiGetMsiData(PPCIDEVICE pDev, int32_t iVector)
{
    int16_t  iOff = msiIs64Bit(pDev) ? VBOX_MSI_CAP_MESSAGE_DATA_64 : VBOX_MSI_CAP_MESSAGE_DATA_32;
    uint16_t lo = PCIDevGetWord(pDev, pDev->Int.s.u8MsiCapOffset + iOff);

    /// @todo: vector encoding into lower bits of message data, for Multiple Message Enable
    Assert(!msiIsMME(pDev));

    return RT_MAKE_U32(lo, 0);
}

void     MSIPciConfigWrite(PPCIDEVICE pDev, uint32_t u32Address, uint32_t val, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsiCapOffset;
    Assert(iOff >= 0 && (PCIIsMsiCapable(pDev) && iOff < pDev->Int.s.u8MsiCapSize));

    Log2(("MSIPciConfigWrite: %d <- %x (%d)\n", iOff, val, len));

    uint32_t uAddr = u32Address;
    for (uint32_t i = 0; i < len; i++)
    {
        switch (i + iOff)
        {
            case 0: /* Capability ID, ro */
            case 1: /* Next pointer,  ro */
                break;
            case 2:
                /* don't change read-only bits: 1-3,7 */
                val &= UINT32_C(~0x8e);
                pDev->config[uAddr] &= ~val;
                break;
            case 3:
                /* don't change read-only bit 8, and reserved 9-15 */
                break;
            default:
                pDev->config[uAddr] = val;
        }
        uAddr++;
        val >>= 8;
    }
}

uint32_t MSIPciConfigRead (PPCIDEVICE pDev, uint32_t u32Address, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsiCapOffset;

    Log2(("MSIPciConfigRead: %d (%d)\n", iOff, len));

    Assert(iOff >= 0 && (PCIIsMsiCapable(pDev) && iOff < pDev->Int.s.u8MsiCapSize));

    switch (len)
    {
        case 1:
            return PCIDevGetByte(pDev, u32Address);
        case 2:
            return PCIDevGetWord(pDev, u32Address);
        default:
        case 4:
            return PCIDevGetDWord(pDev, u32Address);
    }
}


int MSIInit(PPCIDEVICE pDev, PPDMMSIREG pMsiReg)
{
    uint16_t   cVectors    = pMsiReg->cVectors;
    uint8_t    iCapOffset  = pMsiReg->iCapOffset;
    uint8_t    iNextOffset = pMsiReg->iNextOffset;
    uint16_t   iMsiFlags   = pMsiReg->iMsiFlags;

    Assert(cVectors > 0);

    if (cVectors > VBOX_MSI_MAX_ENTRIES)
        return VERR_TOO_MUCH_DATA;

    Assert(iCapOffset != 0 && iCapOffset < 0xff && iNextOffset < 0xff);

    bool f64bit = (iMsiFlags & VBOX_PCI_MSI_FLAGS_64BIT) != 0;

    pDev->Int.s.u8MsiCapOffset = iCapOffset;
    pDev->Int.s.u8MsiCapSize   = f64bit ? VBOX_MSI_CAP_SIZE_64 : VBOX_MSI_CAP_SIZE_32;

    PCIDevSetByte(pDev,  iCapOffset + 0, VBOX_PCI_CAP_ID_MSI);
    PCIDevSetByte(pDev,  iCapOffset + 1, iNextOffset); /* next */
    PCIDevSetWord(pDev,  iCapOffset + VBOX_MSI_CAP_MESSAGE_CONTROL, iMsiFlags);

    PCISetMsiCapable(pDev);

    return VINF_SUCCESS;
}


bool     MSIIsEnabled(PPCIDEVICE pDev)
{
    return PCIIsMsiCapable(pDev) && msiIsEnabled(pDev);
}

void MSINotify(PPDMDEVINS pDevIns, PPCIDEVICE pDev, int iVector)
{
    Log2(("MSINotify: %d\n", iVector));

    AssertMsg(msiIsEnabled(pDev), ("Must be enabled to use that"));

    RTGCPHYS   GCAddr = msiGetMsiAddress(pDev);
    uint32_t   u32Value = msiGetMsiData(pDev, iVector);

    PDMDevHlpPhysWrite(pDevIns, GCAddr, &u32Value, sizeof(u32Value));
}
