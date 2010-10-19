/* $Id$ */
/** @file
 * MSI-X support routines
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

DECLINLINE(uint16_t) msixGetMessageControl(PPCIDEVICE pDev)
{
    return PCIDevGetWord(pDev, pDev->Int.s.u8MsixCapOffset + VBOX_MSIX_CAP_MESSAGE_CONTROL);
}

DECLINLINE(bool) msixIsEnabled(PPCIDEVICE pDev)
{
    return (msixGetMessageControl(pDev) & VBOX_PCI_MSIX_FLAGS_ENABLE) != 0;
}


int MsixInit(PPCIDEVICE pDev, PPDMMSIREG pMsiReg)
{
    if (pMsiReg->cMsixVectors == 0)
         return VINF_SUCCESS;

    uint16_t   cVectors    = pMsiReg->cMsixVectors;
    uint8_t    iCapOffset  = pMsiReg->iMsixCapOffset;
    uint8_t    iNextOffset = pMsiReg->iMsixNextOffset;
    uint16_t   iFlags      = pMsiReg->iMsixFlags;
    
    if (cVectors != 1)
        /* We cannot handle multiple vectors yet */
        return VERR_TOO_MUCH_DATA;
    
    if (cVectors > VBOX_MSIX_MAX_ENTRIES)
        return VERR_TOO_MUCH_DATA;
    
    Assert(iCapOffset != 0 && iCapOffset < 0xff && iNextOffset < 0xff);
    
    pDev->Int.s.u8MsixCapOffset = iCapOffset;
    pDev->Int.s.u8MsixCapSize   = VBOX_MSIX_CAP_SIZE;
    
    PCIDevSetByte(pDev,  iCapOffset + 0, VBOX_PCI_CAP_ID_MSIX);
    PCIDevSetByte(pDev,  iCapOffset + 1, iNextOffset); /* next */
    PCIDevSetWord(pDev,  iCapOffset + VBOX_MSIX_CAP_MESSAGE_CONTROL, iFlags);

    PCISetMsixCapable(pDev);

    return VINF_SUCCESS;
}


bool     MsixIsEnabled(PPCIDEVICE pDev)
{
    return PCIIsMsixCapable(pDev) && msixIsEnabled(pDev);
}

void MsixNotify(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, int iVector, int iLevel)
{
    AssertMsg(msixIsEnabled(pDev), ("Must be enabled to use that"));

    Assert(pPciHlp->pfnIoApicSendMsi != NULL);
}


void     MsixPciConfigWrite(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, uint32_t u32Address, uint32_t val, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsixCapOffset;
    Assert(iOff >= 0 && (PCIIsMsixCapable(pDev) && iOff < pDev->Int.s.u8MsixCapSize));

    Log2(("MsixPciConfigWrite: %d <- %x (%d)\n", iOff, val, len));

    uint32_t uAddr = u32Address;

    for (uint32_t i = 0; i < len; i++)
    {
        uint32_t reg = i + iOff;
        uint8_t u8Val = (uint8_t)val;
        switch (reg)
        {
            case 0: /* Capability ID, ro */
            case 1: /* Next pointer,  ro */
                break;
            case VBOX_MSIX_CAP_MESSAGE_CONTROL:
                /* don't change read-only bits: 0-7 */
                break;
            case VBOX_MSIX_CAP_MESSAGE_CONTROL + 1:
                /* don't change read-only bits 8-13 */
                pDev->config[uAddr] = (u8Val & UINT8_C(~0x3f)) | (pDev->config[uAddr] & UINT8_C(0x3f));
                break;
            default:
                /* other fields read-only too */
                break;
        }
        uAddr++;
        val >>= 8;
    }
}
uint32_t MsixPciConfigRead (PPDMDEVINS pDevIns, PPCIDEVICE pDev, uint32_t u32Address, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsixCapOffset;

    Assert(iOff >= 0 && (PCIIsMsixCapable(pDev) && iOff < pDev->Int.s.u8MsixCapSize));
    uint32_t rv = 0;

    switch (len)
    {
        case 1:
            rv = PCIDevGetByte(pDev,  u32Address);
            break;
        case 2:
            rv = PCIDevGetWord(pDev,  u32Address);
            break;
        case 4:
            rv = PCIDevGetDWord(pDev, u32Address);
            break;
        default:
            Assert(false);
    }

    Log2(("MsixPciConfigRead: %d (%d) -> %x\n", iOff, len, rv));

    return rv;
}
