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
    return (msiGetMessageControl(pDev) & VBOX_PCI_MSI_FLAGS_64BIT) != 0;
}

DECLINLINE(uint32_t*) msiGetMaskBits(PPCIDEVICE pDev)
{
    uint8_t iOff = msiIs64Bit(pDev) ? VBOX_MSI_CAP_MASK_BITS_64 : VBOX_MSI_CAP_MASK_BITS_32;
    iOff += pDev->Int.s.u8MsiCapOffset;
    return (uint32_t*)(pDev->config + iOff);
}

DECLINLINE(uint32_t*) msiGetPendingBits(PPCIDEVICE pDev)
{
    uint8_t iOff = msiIs64Bit(pDev) ? VBOX_MSI_CAP_PENDING_BITS_64 : VBOX_MSI_CAP_PENDING_BITS_32;
    iOff += pDev->Int.s.u8MsiCapOffset;
    return (uint32_t*)(pDev->config + iOff);
}

DECLINLINE(bool) msiIsEnabled(PPCIDEVICE pDev)
{
    return (msiGetMessageControl(pDev) & VBOX_PCI_MSI_FLAGS_ENABLE) != 0;
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

DECLINLINE(bool) msiBitJustCleared(uint32_t u32OldValue,
                                   uint32_t u32NewValue,
                                   uint32_t u32Mask)
{
    return (!!(u32OldValue & u32Mask) && !(u32NewValue & u32Mask));
}


void     MSIPciConfigWrite(PPDMDEVINS pDevIns, PPCIDEVICE pDev, uint32_t u32Address, uint32_t val, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsiCapOffset;
    Assert(iOff >= 0 && (PCIIsMsiCapable(pDev) && iOff < pDev->Int.s.u8MsiCapSize));

    Log2(("MSIPciConfigWrite: %d <- %x (%d)\n", iOff, val, len));

    uint32_t uAddr = u32Address;
    bool f64Bit = msiIs64Bit(pDev);

    for (uint32_t i = 0; i < len; i++)
    {
        uint32_t reg = i + iOff;
        switch (reg)
        {
            case 0: /* Capability ID, ro */
            case 1: /* Next pointer,  ro */
                break;
            case VBOX_MSI_CAP_MESSAGE_CONTROL:
                /* don't change read-only bits: 1-3,7 */
                val &= UINT32_C(~0x8e);
                pDev->config[uAddr] &= ~val;
                break;
            case VBOX_MSI_CAP_MESSAGE_CONTROL + 1:
                /* don't change read-only bit 8, and reserved 9-15 */
                break;
            default:
                if (pDev->config[uAddr] != val)
                {
                    int32_t maskUpdated = -1;

                    /* If we're enabling masked vector, and have pending messages
                       for this vector, we have to send this message now */
                    if (    !f64Bit
                         && (reg >= VBOX_MSI_CAP_MASK_BITS_32)
                         && (reg < VBOX_MSI_CAP_MASK_BITS_32 + 4)
                        )
                    {
                        maskUpdated = reg - VBOX_MSI_CAP_MASK_BITS_32;
                    }
                    if (    f64Bit
                         && (reg >= VBOX_MSI_CAP_MASK_BITS_64)
                         && (reg < VBOX_MSI_CAP_MASK_BITS_64 + 4)
                       )
                    {
                        maskUpdated = reg - VBOX_MSI_CAP_MASK_BITS_64;
                    }

                    if (maskUpdated != -1 && msiIsEnabled(pDev))
                    {
                        for (int iBitNum = 0; i<8; i++)
                        {
                            int32_t iBit = 1 << iBitNum;
                            if (msiBitJustCleared(pDev->config[uAddr], val, iBit))
                            {
                                /* To ensure that we're no longer masked */
                                pDev->config[uAddr] &= ~iBit;
                                MSINotify(pDevIns, pDev, maskUpdated*8 + iBitNum);
                            }
                        }
                    }

                    pDev->config[uAddr] = val;
                }
        }
        uAddr++;
        val >>= 8;
    }
}

uint32_t MSIPciConfigRead (PPDMDEVINS pDevIns, PPCIDEVICE pDev, uint32_t u32Address, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsiCapOffset;

    Assert(iOff >= 0 && (PCIIsMsiCapable(pDev) && iOff < pDev->Int.s.u8MsiCapSize));
    uint32_t rv = 0;

    switch (len)
    {
        case 1:
            rv = PCIDevGetByte(pDev, u32Address);
            break;
        case 2:
            rv = PCIDevGetWord(pDev, u32Address);
            break;
        case 4:
            rv = PCIDevGetDWord(pDev, u32Address);
            break;
        default:
            Assert(false);
    }

    Log2(("MSIPciConfigRead: %d (%d) -> %x\n", iOff, len, rv));

    return rv;
}


int MSIInit(PPCIDEVICE pDev, PPDMMSIREG pMsiReg)
{
    uint16_t   cVectors    = pMsiReg->cVectors;
    uint8_t    iCapOffset  = pMsiReg->iCapOffset;
    uint8_t    iNextOffset = pMsiReg->iNextOffset;
    uint16_t   iMsiFlags   = pMsiReg->iMsiFlags;

    Assert(cVectors > 0);

    if (cVectors != 1)
        /* We cannot handle multiple vectors yet */
        return VERR_TOO_MUCH_DATA;

    if (cVectors > VBOX_MSI_MAX_ENTRIES)
        return VERR_TOO_MUCH_DATA;

    Assert(iCapOffset != 0 && iCapOffset < 0xff && iNextOffset < 0xff);

    bool f64bit = (iMsiFlags & VBOX_PCI_MSI_FLAGS_64BIT) != 0;
    /* We always support per-vector masking */
    iMsiFlags |= VBOX_PCI_MSI_FLAGS_MASKBIT;

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

    uint32_t   uMask = *msiGetMaskBits(pDev);
    uint32_t*  upPending = msiGetPendingBits(pDev);

    if ((uMask & (1<<iVector)) != 0)
    {
        *upPending |= (1<<iVector);
        return;
    }

    RTGCPHYS   GCAddr = msiGetMsiAddress(pDev);
    uint32_t   u32Value = msiGetMsiData(pDev, iVector);

    *upPending &= ~(1<<iVector);

    PDMDevHlpPhysWrite(pDevIns, GCAddr, &u32Value, sizeof(u32Value));
}
