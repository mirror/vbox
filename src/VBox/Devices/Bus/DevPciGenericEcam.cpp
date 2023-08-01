/* $Id$ */
/** @file
 * DevPciGeneric - Generic host to PCIe bridge emulation.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
#define PDMPCIDEV_INCLUDE_PRIVATE  /* Hack to get pdmpcidevint.h included at the right point. */
#include <VBox/vmm/pdmpcidev.h>

#include <VBox/AssertGuest.h>
#include <VBox/msi.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/mm.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/uuid.h>
#endif

#include "PciInline.h"
#include "VBoxDD.h"
#include "MsiCommon.h"
#include "DevPciInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @todo As this shares a lot of code with the ICH9 PCI device we have to also keep the saved state version in sync. */
/** Saved state version of the generic ECAM PCI bus device. */
#define VBOX_PCIGENECAM_SAVED_STATE_VERSION             VBOX_ICH9PCI_SAVED_STATE_VERSION_4KB_CFG_SPACE
/** 4KB config space */
#define VBOX_ICH9PCI_SAVED_STATE_VERSION_4KB_CFG_SPACE  4


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Returns the interrupt pin for a given device slot on the root port
 * due to swizzeling.
 *
 * @returns Interrupt pin on the root port.
 * @param   uDevFn      The device.
 * @param   uPin        The interrupt pin on the device.
 */
DECLINLINE(uint8_t) pciGenEcamGetPirq(uint8_t uDevFn, uint8_t uPin)
{
    uint8_t uSlot = (uDevFn >> 3) - 1;
    return (uPin + uSlot) & 3;
}


/**
 * Returns whether the interrupt line is asserted on the PCI root for the given pin.
 *
 * @returns Flag whther the interrupt line is asserted (true) or not (false).
 * @param   pPciRoot    The PCI root bus.
 * @param   u8IrqPin    The IRQ pin being checked.
 */
DECLINLINE(bool) pciGenEcamGetIrqLvl(PDEVPCIROOT pPciRoot, uint8_t u8IrqPin)
{
    return (pPciRoot->u.GenericEcam.auPciIrqLevels[u8IrqPin] != 0);
}


/**
 * @interface_method_impl{PDMPCIBUSREGCC,pfnSetIrqR3}
 */
static DECLCALLBACK(void) pciGenEcamSetIrq(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDEVPCIROOT pPciRoot = PDMINS_2_DATA(pDevIns, PDEVPCIROOT);
    PDEVPCIBUS  pBus = &pPciRoot->PciBus;
    PDEVPCIBUSCC pBusCC = PDMINS_2_DATA_CC(pDevIns, PDEVPCIBUSCC);
    uint8_t uDevFn = pPciDev->uDevFn;
    uint16_t const uBusDevFn = PCIBDF_MAKE(pBus->iBus, uDevFn);

    LogFlowFunc(("invoked by %p/%d: iIrq=%d iLevel=%d uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, iIrq, iLevel, uTagSrc));

    /* If MSI or MSI-X is enabled, PCI INTx# signals are disabled regardless of the PCI command
     * register interrupt bit state.
     * PCI 3.0 (section 6.8) forbids MSI and MSI-X to be enabled at the same time and makes
     * that undefined behavior. We check for MSI first, then MSI-X.
     */
    if (MsiIsEnabled(pPciDev))
    {
        Assert(!MsixIsEnabled(pPciDev));    /* Not allowed -- see note above. */
        LogFlowFunc(("PCI Dev %p : MSI\n", pPciDev));
        MsiNotify(pDevIns, pBusCC->CTX_SUFF(pPciHlp), pPciDev, iIrq, iLevel, uTagSrc);
        return;
    }

    if (MsixIsEnabled(pPciDev))
    {
        LogFlowFunc(("PCI Dev %p : MSI-X\n", pPciDev));
        MsixNotify(pDevIns, pBusCC->CTX_SUFF(pPciHlp), pPciDev, iIrq, iLevel, uTagSrc);
        return;
    }

    LogFlowFunc(("PCI Dev %p : IRQ\n", pPciDev));

    /* Check if the state changed. */
    if (pPciDev->Int.s.uIrqPinState != iLevel)
    {
        pPciDev->Int.s.uIrqPinState = (iLevel & PDM_IRQ_LEVEL_HIGH);

        /* Get the pin. */
        uint8_t uIrqPin = devpciR3GetByte(pPciDev, VBOX_PCI_INTERRUPT_PIN);
        uint8_t uIrq = pciGenEcamGetPirq(pPciDev->uDevFn, uIrqPin);

        if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_HIGH)
            ASMAtomicIncU32(&pPciRoot->u.GenericEcam.auPciIrqLevels[uIrq]);
        else if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_LOW)
            ASMAtomicDecU32(&pPciRoot->u.GenericEcam.auPciIrqLevels[uIrq]);

        bool fIrqLvl = pciGenEcamGetIrqLvl(pPciRoot, uIrq);
        uint32_t u32IrqNr = pPciRoot->u.GenericEcam.auPciIrqNr[uIrq];

        Log3Func(("%s: uIrqPin=%u uIrqRoot=%u fIrqLvl=%RTbool uIrqNr=%u\n",
              R3STRING(pPciDev->pszNameR3), uIrqPin, uIrq, fIrqLvl, u32IrqNr));
        pBusCC->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pDevIns, uBusDevFn, u32IrqNr, fIrqLvl, uTagSrc);

        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
            ASMAtomicDecU32(&pPciRoot->u.GenericEcam.auPciIrqLevels[uIrq]);
            pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
            fIrqLvl = pciGenEcamGetIrqLvl(pPciRoot, uIrq);
            Log3Func(("%s: uIrqPin=%u uIrqRoot=%u fIrqLvl=%RTbool uIrqNr=%u\n",
                  R3STRING(pPciDev->pszNameR3), uIrqPin, uIrq, fIrqLvl, u32IrqNr));
            pBusCC->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pDevIns, uBusDevFn, u32IrqNr, fIrqLvl, uTagSrc);
        }
    }
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE,
 * Emulates writes to PIO space.}
 */
static DECLCALLBACK(VBOXSTRICTRC) pciHostR3MmioPioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    Log2Func(("%RGp LB %d\n", off, cb));
    RT_NOREF(pvUser);

    AssertReturn(off < _64K, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= 4, VERR_INVALID_PARAMETER);

    /* Get the value. */
    uint32_t u32;
    switch (cb)
    {
        case 1:
            u32 = *(uint8_t const *)pv;
            break;
        case 2:
            u32 = *(uint16_t const *)pv;
            break;
        case 4:
            u32 = *(uint32_t const *)pv;
            break;
        default:
            ASSERT_GUEST_MSG_FAILED(("cb=%u off=%RGp\n", cb, off)); /** @todo how the heck should this work? Split it, right? */
            u32 = 0;
            break;
    }

    return PDMDevHlpIoPortWrite(pDevIns, (RTIOPORT)off, u32, cb);
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE,
 * Emulates reads from PIO space.}
 */
static DECLCALLBACK(VBOXSTRICTRC) pciHostR3MmioPioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    LogFlowFunc(("%RGp LB %u\n", off, cb));
    RT_NOREF(pvUser);

    AssertReturn(off < _64K, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= 4, VERR_INVALID_PARAMETER);

    /* Perform PIO space read */
    uint32_t     u32Value = 0;
    VBOXSTRICTRC rcStrict = PDMDevHlpIoPortRead(pDevIns, (RTIOPORT)off, &u32Value, cb);

    if (RT_SUCCESS(rcStrict))
    {
        switch (cb)
        {
            case 1:
                *(uint8_t *)pv   = (uint8_t)u32Value;
                break;
            case 2:
                *(uint16_t *)pv  = (uint16_t)u32Value;
                break;
            case 4:
                *(uint32_t *)pv  = u32Value;
                break;
            default:
                ASSERT_GUEST_MSG_FAILED(("cb=%u off=%RGp\n", cb, off)); /** @todo how the heck should this work? Split it, right? */
                break;
        }
    }

    return rcStrict;
}


#ifdef IN_RING3

/* -=-=-=-=-=- PCI Config Space -=-=-=-=-=- */


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) pciGenEcamR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE  pCfg)
{
    RT_NOREF1(iInstance);
    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    PDEVPCIBUSCC    pBusCC   = PDMINS_2_DATA_CC(pDevIns, PDEVPCIBUSCC);
    PDEVPCIROOT     pPciRoot = PDMINS_2_DATA(pDevIns, PDEVPCIROOT);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PDEVPCIBUS      pBus     = &pPciRoot->PciBus;

    /*
     * Validate and read configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "MmioEcamBase"
                                           "|MmioEcamLength"
                                           "|MmioPioBase"
                                           "|MmioPioSize"
                                           "|IntPinA"
                                           "|IntPinB"
                                           "|IntPinC"
                                           "|IntPinD", "");

    int rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioEcamBase", &pPciRoot->u64PciConfigMMioAddress, 0);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"McfgBase\"")));

    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioEcamLength", &pPciRoot->u64PciConfigMMioLength, 0);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"McfgLength\"")));

    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioPioBase", &pPciRoot->GCPhysMmioPioEmuBase, 0);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"MmioPioBase\"")));

    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioPioSize", &pPciRoot->GCPhysMmioPioEmuSize, 0);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"MmioPioSize\"")));

    rc = pHlp->pfnCFGMQueryU32(pCfg, "IntPinA", &pPciRoot->u.GenericEcam.auPciIrqNr[0]);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"IntPinA\"")));

    rc = pHlp->pfnCFGMQueryU32(pCfg, "IntPinB", &pPciRoot->u.GenericEcam.auPciIrqNr[1]);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"IntPinB\"")));

    rc = pHlp->pfnCFGMQueryU32(pCfg, "IntPinC", &pPciRoot->u.GenericEcam.auPciIrqNr[2]);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"IntPinC\"")));

    rc = pHlp->pfnCFGMQueryU32(pCfg, "IntPinD", &pPciRoot->u.GenericEcam.auPciIrqNr[3]);
    AssertRCReturn(rc, PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"IntPinD\"")));

    Log(("PCI: fUseIoApic=%RTbool McfgBase=%#RX64 McfgLength=%#RX64 fR0Enabled=%RTbool fRCEnabled=%RTbool\n", pPciRoot->fUseIoApic,
         pPciRoot->u64PciConfigMMioAddress, pPciRoot->u64PciConfigMMioLength, pDevIns->fR0Enabled, pDevIns->fRCEnabled));
    Log(("PCI: IntPinA=%u IntPinB=%u IntPinC=%u IntPinD=%u\n", pPciRoot->u.GenericEcam.auPciIrqNr[0],
         pPciRoot->u.GenericEcam.auPciIrqNr[1], pPciRoot->u.GenericEcam.auPciIrqNr[2], pPciRoot->u.GenericEcam.auPciIrqNr[3]));

    /*
     * Init data.
     */
    /* And fill values */
    pBusCC->pDevInsR3               = pDevIns;
    pPciRoot->hIoPortAddress        = NIL_IOMIOPORTHANDLE;
    pPciRoot->hIoPortData           = NIL_IOMIOPORTHANDLE;
    pPciRoot->hIoPortMagic          = NIL_IOMIOPORTHANDLE;
    pPciRoot->hMmioMcfg             = NIL_IOMMMIOHANDLE;
    pPciRoot->hMmioPioEmu           = NIL_IOMMMIOHANDLE;
    pPciRoot->PciBus.enmType        = DEVPCIBUSTYPE_GENERIC_ECAM;
    pPciRoot->PciBus.fPureBridge    = false;
    pPciRoot->PciBus.papBridgesR3   = (PPDMPCIDEV *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPDMPCIDEV) * RT_ELEMENTS(pPciRoot->PciBus.apDevices));
    AssertLogRelReturn(pPciRoot->PciBus.papBridgesR3, VERR_NO_MEMORY);

    /*
     * Disable default device locking.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register bus
     */
    PDMPCIBUSREGCC PciBusReg;
    PciBusReg.u32Version                 = PDM_PCIBUSREGCC_VERSION;
    PciBusReg.pfnRegisterR3              = devpciR3CommonRegisterDevice;
    PciBusReg.pfnRegisterMsiR3           = NULL;
    PciBusReg.pfnIORegionRegisterR3      = devpciR3CommonIORegionRegister;
    PciBusReg.pfnInterceptConfigAccesses = devpciR3CommonInterceptConfigAccesses;
    PciBusReg.pfnConfigRead              = devpciR3CommonConfigRead;
    PciBusReg.pfnConfigWrite             = devpciR3CommonConfigWrite;
    PciBusReg.pfnSetIrqR3                = pciGenEcamSetIrq;
    PciBusReg.u32EndVersion              = PDM_PCIBUSREGCC_VERSION;
    rc = PDMDevHlpPCIBusRegister(pDevIns, &PciBusReg, &pBusCC->pPciHlpR3, &pBus->iBus);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to register ourselves as a PCI Bus"));
    Assert(pBus->iBus == 0);
    if (pBusCC->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                   pBusCC->pPciHlpR3->u32Version, PDM_PCIHLPR3_VERSION);

    /*
     * Fill in PCI configs and add them to the bus.
     */
#if 0
    /* Host bridge device */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    AssertPtr(pPciDev);
    PDMPciDevSetVendorId(  pPciDev, 0x8086); /** @todo Intel */
    PDMPciDevSetDeviceId(  pPciDev, 0x29e0); /** @todo Desktop */
    PDMPciDevSetRevisionId(pPciDev,   0x01); /* rev. 01 */
    PDMPciDevSetClassBase( pPciDev,   0x06); /* bridge */
    PDMPciDevSetClassSub(  pPciDev,   0x00); /* Host/PCI bridge */
    PDMPciDevSetClassProg( pPciDev,   0x00); /* Host/PCI bridge */
    PDMPciDevSetHeaderType(pPciDev,   0x00); /* bridge */
    PDMPciDevSetWord(pPciDev,  VBOX_PCI_SEC_STATUS, 0x0280);  /* secondary status */

    rc = PDMDevHlpPCIRegisterEx(pDevIns, pPciDev, 0 /*fFlags*/, 0 /*uPciDevNo*/, 0 /*uPciFunNo*/, "Host");
    AssertLogRelRCReturn(rc, rc);
#endif

    /*
     * MMIO handlers.
     */
    if (pPciRoot->u64PciConfigMMioAddress != 0)
    {
        rc = PDMDevHlpMmioCreateAndMap(pDevIns, pPciRoot->u64PciConfigMMioAddress, pPciRoot->u64PciConfigMMioLength,
                                       devpciCommonMcfgMmioWrite, devpciCommonMcfgMmioRead,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                       "ECAM window", &pPciRoot->hMmioMcfg);
        AssertMsgRCReturn(rc, ("rc=%Rrc %#RX64/%#RX64\n", rc,  pPciRoot->u64PciConfigMMioAddress, pPciRoot->u64PciConfigMMioLength), rc);
    }

    if (pPciRoot->GCPhysMmioPioEmuBase != 0)
    {
        rc = PDMDevHlpMmioCreateAndMap(pDevIns, pPciRoot->GCPhysMmioPioEmuBase, pPciRoot->GCPhysMmioPioEmuSize,
                                       pciHostR3MmioPioWrite, pciHostR3MmioPioRead,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                       "PIO range", &pPciRoot->hMmioPioEmu);
        AssertMsgRCReturn(rc, ("rc=%Rrc %#RGp/%#RGp\n", rc,  pPciRoot->GCPhysMmioPioEmuBase, pPciRoot->GCPhysMmioPioEmuSize), rc);
    }

    /*
     * Saved state and info handlers.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VBOX_PCIGENECAM_SAVED_STATE_VERSION,
                                sizeof(*pBus) + 16*128, "pgm",
                                NULL, NULL, NULL,
                                NULL, devpciR3CommonSaveExec, NULL,
                                NULL, devpciR3CommonLoadExec, NULL);
    AssertRCReturn(rc, rc);

    PDMDevHlpDBGFInfoRegister(pDevIns, "pci",
                              "Display PCI bus status. Recognizes 'basic' or 'verbose' as arguments, defaults to 'basic'.",
                              devpciR3InfoPci);
    PDMDevHlpDBGFInfoRegister(pDevIns, "pciirq", "Display PCI IRQ state. (no arguments)", devpciR3InfoPciIrq);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) pciGenEcamR3Destruct(PPDMDEVINS pDevIns)
{
    PDEVPCIROOT pPciRoot = PDMINS_2_DATA(pDevIns, PDEVPCIROOT);
    if (pPciRoot->PciBus.papBridgesR3)
    {
        PDMDevHlpMMHeapFree(pDevIns, pPciRoot->PciBus.papBridgesR3);
        pPciRoot->PciBus.papBridgesR3 = NULL;
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) pciGenEcamR3Reset(PPDMDEVINS pDevIns)
{
    /* Reset everything under the root bridge. */
    devpciR3CommonResetBridge(pDevIns);
}

#else  /* !IN_RING3 */

/**
 * @interface_method_impl{PDMDEVREGR0,pfnConstruct}
 */
DECLCALLBACK(int) pciGenEcamRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVPCIROOT  pPciRoot = PDMINS_2_DATA(pDevIns, PDEVPCIROOT);
    PDEVPCIBUSCC pBusCC   = PDMINS_2_DATA_CC(pDevIns, PDEVPCIBUSCC);

    /* Mirror the ring-3 device lock disabling: */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Set up the RZ PCI bus callbacks: */
    PDMPCIBUSREGCC PciBusReg;
    PciBusReg.u32Version    = PDM_PCIBUSREGCC_VERSION;
    PciBusReg.iBus          = pPciRoot->PciBus.iBus;
    PciBusReg.pfnSetIrq     = pciGenEcamSetIrq;
    PciBusReg.u32EndVersion = PDM_PCIBUSREGCC_VERSION;
    rc = PDMDevHlpPCIBusSetUpContext(pDevIns, &PciBusReg, &pBusCC->CTX_SUFF(pPciHlp));
    AssertRCReturn(rc, rc);

    /* Set up MMIO callbacks: */
    if (pPciRoot->hMmioMcfg != NIL_IOMMMIOHANDLE)
    {
        rc = PDMDevHlpMmioSetUpContext(pDevIns, pPciRoot->hMmioMcfg, devpciCommonMcfgMmioWrite, devpciCommonMcfgMmioRead, NULL /*pvUser*/);
        AssertLogRelRCReturn(rc, rc);
    }

    return rc;
}

#endif /* !IN_RING3 */

/**
 * The PCI bus device registration structure.
 */
const PDMDEVREG g_DevicePciGenericEcam =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "pci-generic-ecam",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_BUS_PCI,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVPCIROOT),
    /* .cbInstanceCC = */           sizeof(CTX_SUFF(DEVPCIBUS)),
    /* .cbInstanceRC = */           sizeof(DEVPCIBUSRC),
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic PCI host bridge (working with pci-host-ecam-generic driver)",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           pciGenEcamR3Construct,
    /* .pfnDestruct = */            pciGenEcamR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               pciGenEcamR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           pciGenEcamRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           pciGenEcamRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};
