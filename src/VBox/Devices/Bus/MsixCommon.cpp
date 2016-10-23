/* $Id$ */
/** @file
 * MSI-X support routines
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
#define PDMPCIDEV_INCLUDE_PRIVATE  /* Hack to get pdmpcidevint.h included at the right point. */
#include <VBox/pci.h>
#include <VBox/msi.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/vmm/mm.h>

#include <iprt/assert.h>

#include "MsiCommon.h"
#include "PciInline.h"

#pragma pack(1)
typedef struct
{
    uint32_t  u32MsgAddressLo;
    uint32_t  u32MsgAddressHi;
    uint32_t  u32MsgData;
    uint32_t  u32VectorControl;
} MsixTableRecord;
AssertCompileSize(MsixTableRecord, VBOX_MSIX_ENTRY_SIZE);
#pragma pack()

/** @todo use accessors so that raw PCI devices work correctly with MSI-X. */
DECLINLINE(uint16_t)  msixGetMessageControl(PPCIDEVICE pDev)
{
    return PCIDevGetWord(pDev, pDev->Int.s.u8MsixCapOffset + VBOX_MSIX_CAP_MESSAGE_CONTROL);
}

DECLINLINE(bool)      msixIsEnabled(PPCIDEVICE pDev)
{
    return (msixGetMessageControl(pDev) & VBOX_PCI_MSIX_FLAGS_ENABLE) != 0;
}

DECLINLINE(bool)      msixIsMasked(PPCIDEVICE pDev)
{
    return (msixGetMessageControl(pDev) & VBOX_PCI_MSIX_FLAGS_FUNCMASK) != 0;
}

DECLINLINE(uint16_t)  msixTableSize(PPCIDEVICE pDev)
{
    return (msixGetMessageControl(pDev) & 0x3ff) + 1;
}

DECLINLINE(uint8_t*)  msixGetPageOffset(PPCIDEVICE pDev, uint32_t off)
{
    return (uint8_t*)pDev->Int.s.CTX_SUFF(pMsixPage) + off;
}

DECLINLINE(MsixTableRecord*) msixGetVectorRecord(PPCIDEVICE pDev, uint32_t iVector)
{
    return (MsixTableRecord*)msixGetPageOffset(pDev, iVector * VBOX_MSIX_ENTRY_SIZE);
}

DECLINLINE(RTGCPHYS)  msixGetMsiAddress(PPCIDEVICE pDev, uint32_t iVector)
{
    MsixTableRecord* pRec = msixGetVectorRecord(pDev, iVector);
    return RT_MAKE_U64(pRec->u32MsgAddressLo & ~UINT32_C(0x3), pRec->u32MsgAddressHi);
}

DECLINLINE(uint32_t)  msixGetMsiData(PPCIDEVICE pDev, uint32_t iVector)
{
    return msixGetVectorRecord(pDev, iVector)->u32MsgData;
}

DECLINLINE(uint32_t)  msixIsVectorMasked(PPCIDEVICE pDev, uint32_t iVector)
{
    return (msixGetVectorRecord(pDev, iVector)->u32VectorControl & 0x1) != 0;
}

DECLINLINE(uint8_t*)  msixPendingByte(PPCIDEVICE pDev, uint32_t iVector)
{
    return msixGetPageOffset(pDev, 0x800 + iVector / 8);
}

DECLINLINE(void)      msixSetPending(PPCIDEVICE pDev, uint32_t iVector)
{
    *msixPendingByte(pDev, iVector) |= (1 << (iVector & 0x7));
}

DECLINLINE(void)      msixClearPending(PPCIDEVICE pDev, uint32_t iVector)
{
    *msixPendingByte(pDev, iVector) &= ~(1 << (iVector & 0x7));
}

DECLINLINE(bool)      msixIsPending(PPCIDEVICE pDev, uint32_t iVector)
{
    return (*msixPendingByte(pDev, iVector) & (1 << (iVector & 0x7))) != 0;
}

static void msixCheckPendingVector(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, uint32_t iVector)
{
    if (msixIsPending(pDev, iVector) && !msixIsVectorMasked(pDev, iVector))
        MsixNotify(pDevIns, pPciHlp, pDev, iVector, 1 /* iLevel */, 0 /*uTagSrc*/);
}

#ifdef IN_RING3

PDMBOTHCBDECL(int) msixMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    /// @todo qword accesses?
    NOREF(pDevIns);
    AssertMsgReturn(cb == 4,
                    ("MSI-X must be accessed with 4-byte reads"),
                    VERR_INTERNAL_ERROR);

    uint32_t off = (uint32_t)(GCPhysAddr & 0xfff);
    PPCIDEVICE pPciDev = (PPCIDEVICE)pvUser;

    *(uint32_t*)pv = *(uint32_t*)msixGetPageOffset(pPciDev, off);

    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) msixMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    /// @todo qword accesses?
    AssertMsgReturn(cb == 4,
                    ("MSI-X must be accessed with 4-byte reads"),
                    VERR_INTERNAL_ERROR);
    PPCIDEVICE pPciDev = (PPCIDEVICE)pvUser;

    uint32_t off = (uint32_t)(GCPhysAddr & 0xfff);

    AssertMsgReturn(off < 0x800, ("Trying to write to PBA\n"), VINF_SUCCESS);

    *(uint32_t*)msixGetPageOffset(pPciDev, off) = *(uint32_t*)pv;

    msixCheckPendingVector(pDevIns, (PCPDMPCIHLP)pPciDev->Int.s.pPciBusPtrR3, pPciDev, off / VBOX_MSIX_ENTRY_SIZE);

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) msixMap(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, uint32_t iRegion,
                                 RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    NOREF(iRegion); NOREF(enmType);

    int rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, pPciDev,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   msixMMIOWrite, msixMMIORead, "MSI-X tables");

    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

int MsixInit(PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, PPDMMSIREG pMsiReg)
{
    if (pMsiReg->cMsixVectors == 0)
         return VINF_SUCCESS;

     /* We cannot init MSI-X on raw devices yet. */
    Assert(!pciDevIsPassthrough(pDev));

    uint16_t   cVectors    = pMsiReg->cMsixVectors;
    uint8_t    iCapOffset  = pMsiReg->iMsixCapOffset;
    uint8_t    iNextOffset = pMsiReg->iMsixNextOffset;
    uint8_t    iBar        = pMsiReg->iMsixBar;

    if (cVectors > VBOX_MSIX_MAX_ENTRIES)
    {
        AssertMsgFailed(("Too many MSI-X vectors: %d\n", cVectors));
        return VERR_TOO_MUCH_DATA;
    }

    if (iBar > 5)
    {
        AssertMsgFailed(("Using wrong BAR for MSI-X: %d\n", iBar));
        return VERR_INVALID_PARAMETER;
    }

    Assert(iCapOffset != 0 && iCapOffset < 0xff && iNextOffset < 0xff);

    int rc = VINF_SUCCESS;

    /* If device is passthrough, BAR is registered using common mechanism. */
    if (!pciDevIsPassthrough(pDev))
    {
        rc = PDMDevHlpPCIIORegionRegister(pDev->pDevIns, iBar, 0x1000, PCI_ADDRESS_SPACE_MEM, msixMap);
        if (RT_FAILURE (rc))
            return rc;
    }

    pDev->Int.s.u8MsixCapOffset = iCapOffset;
    pDev->Int.s.u8MsixCapSize   = VBOX_MSIX_CAP_SIZE;
    PVM pVM = PDMDevHlpGetVM(pDev->pDevIns);

    pDev->Int.s.pMsixPageR3     = NULL;

    rc = MMHyperAlloc(pVM, 0x1000, 1, MM_TAG_PDM_DEVICE_USER, (void **)&pDev->Int.s.pMsixPageR3);
    if (RT_FAILURE(rc) || (pDev->Int.s.pMsixPageR3 == NULL))
        return VERR_NO_VM_MEMORY;
    RT_BZERO(pDev->Int.s.pMsixPageR3, 0x1000);
    pDev->Int.s.pMsixPageR0     = MMHyperR3ToR0(pVM, pDev->Int.s.pMsixPageR3);
    pDev->Int.s.pMsixPageRC     = MMHyperR3ToRC(pVM, pDev->Int.s.pMsixPageR3);

    /* R3 PCI helper */
    pDev->Int.s.pPciBusPtrR3    = pPciHlp;

    PCIDevSetByte(pDev,  iCapOffset + 0, VBOX_PCI_CAP_ID_MSIX);
    PCIDevSetByte(pDev,  iCapOffset + 1, iNextOffset); /* next */
    PCIDevSetWord(pDev,  iCapOffset + VBOX_MSIX_CAP_MESSAGE_CONTROL, cVectors - 1);

    uint32_t offTable = 0, offPBA = 0x800;

    PCIDevSetDWord(pDev,  iCapOffset + VBOX_MSIX_TABLE_BIROFFSET, offTable | iBar);
    PCIDevSetDWord(pDev,  iCapOffset + VBOX_MSIX_PBA_BIROFFSET,   offPBA   | iBar);

    pciDevSetMsixCapable(pDev);

    return VINF_SUCCESS;
}
#endif

bool     MsixIsEnabled(PPCIDEVICE pDev)
{
    return pciDevIsMsixCapable(pDev) && msixIsEnabled(pDev);
}

void MsixNotify(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, int iVector, int iLevel, uint32_t uTagSrc)
{
    AssertMsg(msixIsEnabled(pDev), ("Must be enabled to use that"));

    Assert(pPciHlp->pfnIoApicSendMsi != NULL);

    /* We only trigger MSI-X on level up */
    if ((iLevel & PDM_IRQ_LEVEL_HIGH) == 0)
    {
        return;
    }

    // if this vector is somehow disabled
    if (msixIsMasked(pDev) || msixIsVectorMasked(pDev, iVector))
    {
        // mark pending bit
        msixSetPending(pDev, iVector);
        return;
    }

    // clear pending bit
    msixClearPending(pDev, iVector);

    RTGCPHYS   GCAddr = msixGetMsiAddress(pDev, iVector);
    uint32_t   u32Value = msixGetMsiData(pDev, iVector);

    pPciHlp->pfnIoApicSendMsi(pDevIns, GCAddr, u32Value, uTagSrc);
}

DECLINLINE(bool) msixBitJustCleared(uint32_t uOldValue,
                                    uint32_t uNewValue,
                                    uint32_t uMask)
{
    return (!!(uOldValue & uMask) && !(uNewValue & uMask));
}

static void msixCheckPendingVectors(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev)
{
    for (uint32_t i = 0; i < msixTableSize(pDev); i++)
        msixCheckPendingVector(pDevIns, pPciHlp, pDev, i);
}


void MsixPciConfigWrite(PPDMDEVINS pDevIns, PCPDMPCIHLP pPciHlp, PPCIDEVICE pDev, uint32_t u32Address, uint32_t val, unsigned len)
{
    int32_t iOff = u32Address - pDev->Int.s.u8MsixCapOffset;
    Assert(iOff >= 0 && (pciDevIsMsixCapable(pDev) && iOff < pDev->Int.s.u8MsixCapSize));

    Log2(("MsixPciConfigWrite: %d <- %x (%d)\n", iOff, val, len));

    uint32_t uAddr = u32Address;
    uint8_t u8NewVal;
    bool fJustEnabled = false;

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
            {
                /* don't change read-only bits 8-13 */
                u8NewVal = (u8Val & UINT8_C(~0x3f)) | (pDev->config[uAddr] & UINT8_C(0x3f));
                /* If just enabled globally - check pending vectors */
                fJustEnabled |= msixBitJustCleared(pDev->config[uAddr], u8NewVal, VBOX_PCI_MSIX_FLAGS_ENABLE >> 8);
                fJustEnabled |= msixBitJustCleared(pDev->config[uAddr], u8NewVal, VBOX_PCI_MSIX_FLAGS_FUNCMASK >> 8);
                pDev->config[uAddr] = u8NewVal;
                break;
        }
            default:
                /* other fields read-only too */
                break;
        }
        uAddr++;
        val >>= 8;
    }

    if (fJustEnabled)
        msixCheckPendingVectors(pDevIns, pPciHlp, pDev);
}


uint32_t MsixPciConfigRead(PPDMDEVINS pDevIns, PPCIDEVICE pDev, uint32_t u32Address, unsigned len)
{
    NOREF(pDevIns);
#if defined(LOG_ENABLED) || defined(VBOX_STRICT)
    int32_t iOff = u32Address - pDev->Int.s.u8MsixCapOffset;
    Assert(iOff >= 0 && (pciDevIsMsixCapable(pDev) && iOff < pDev->Int.s.u8MsixCapSize));
#endif
    uint32_t rv;
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
            AssertFailed();
            rv = 0;
    }

    Log2(("MsixPciConfigRead: %d (%d) -> %x\n", iOff, len, rv));
    return rv;
}

