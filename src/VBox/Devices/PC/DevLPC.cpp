/* $Id$ */
/** @file
 * DevLPC - LPC device emulation
 */
/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 *  Low Pin Count emulation
 *
 *  Copyright (c) 2007 Alexander Graf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * *****************************************************************
 *
 * This driver emulates an ICH-7 LPC partially. The LPC is basically the
 * same as the ISA-bridge in the existing PIIX implementation, but
 * more recent and includes support for HPET and Power Management.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_LPC
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "../Builtins.h"

#define RCBA_BASE                0xFED1C000

typedef struct
{
    /** PCI device structure. */
    PCIDEVICE      dev;

    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3   pDevIns;

    /* So far, not much of a state */
} LPCState;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


static uint32_t rcba_ram_readl(LPCState* s, RTGCPHYS addr)
{
    Log(("rcba_read at %llx\n", (uint64_t)addr));
    int32_t iIndex = (addr - RCBA_BASE);
    uint32_t value = 0;

    /* This is the HPET config pointer, HPAS in DSDT */
    switch (iIndex)
    {
       case 0x3404:
          Log(("rcba_read HPET_CONFIG_POINTER\n"));
          value = 0xf0; /*  enabled at 0xfed00000 */
          break;
       case 0x3410:
          /* This is the HPET config pointer */
          Log(("rcba_read GCS\n"));
          value = 0;
          break;
       default:
          Log(("Unknown RCBA read\n"));
          break;
    }

    return value;
}

static void rcba_ram_writel(LPCState* s, RTGCPHYS addr, uint32_t value)
{
    Log(("rcba_write %llx = %#x\n", (uint64_t)addr, value));
    int32_t iIndex = (addr - RCBA_BASE);

    switch (iIndex)
    {
       case 0x3410:
          Log(("rcba_write GCS\n"));
          break;
       default:
          Log(("Unknown RCBA write\n"));
          break;
    }
}

/**
 * I/O handler for memory-mapped read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 * @thread  EMT
 */
PDMBOTHCBDECL(int)  lpcMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    LPCState *s = PDMINS_2_DATA(pDevIns, LPCState*);
    switch (cb)
    {
        case 1:
        case 2:
            break;

        case 4:
        {
            *(uint32_t*)pv = rcba_ram_readl(s, GCPhysAddr);
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

/**
 * Memory mapped I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the value.
 * @param   cb          Number of bytes to write.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) lpcMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    LPCState *s = PDMINS_2_DATA(pDevIns, LPCState*);

    switch (cb)
    {
        case 1:
        case 2:
            break;
        case 4:
        {
            /** @todo: locking? */
            rcba_ram_writel(s, GCPhysAddr, *(uint32_t *)pv);
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) lpcReset(PPDMDEVINS pDevIns)
{
    LPCState *pThis = PDMINS_2_DATA(pDevIns, LPCState *);
    LogFlow(("lpcReset: \n"));
}

/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) lpcInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    LPCState   *pThis = PDMINS_2_DATA(pDevIns, LPCState *);
    LogFlow(("lpcInfo: \n"));
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) lpcConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    LPCState   *pThis = PDMINS_2_DATA(pDevIns, LPCState *);
    int         rc;
    Assert(iInstance == 0);

    pThis->pDevIns = pDevIns;

    /*
     * Register the PCI device.
     */
    PCIDevSetVendorId         (&pThis->dev, 0x8086); /* Intel */
    PCIDevSetDeviceId         (&pThis->dev, 0x27b9);
    PCIDevSetCommand          (&pThis->dev, 0x0007); /* master, memory and I/O */
    PCIDevSetRevisionId       (&pThis->dev, 0x02);
    PCIDevSetClassSub         (&pThis->dev, 0x01); /* PCI-to-ISA Bridge */
    PCIDevSetClassBase        (&pThis->dev, 0x06); /* Bridge */
    PCIDevSetHeaderType       (&pThis->dev, 0xf0); /* ??? */
    PCIDevSetSubSystemVendorId(&pThis->dev, 0x8086);
    PCIDevSetSubSystemId      (&pThis->dev, 0x7270);
    PCIDevSetInterruptPin     (&pThis->dev, 0x03);
    PCIDevSetStatus           (&pThis->dev, 0x0200); /* PCI_status_devsel_medium */

    /** @todo: rewrite using PCI accessors */
    pThis->dev.config[0x40] = 0x01;
    pThis->dev.config[0x41] = 0x0b;

    pThis->dev.config[0x4c] = 0x4d;
    pThis->dev.config[0x4e] = 0x03;
    pThis->dev.config[0x4f] = 0x00;

    pThis->dev.config[0x60] = 0x0a; /* PCI A -> IRQ 10 */
    pThis->dev.config[0x61] = 0x0a; /* PCI B -> IRQ 10 */
    pThis->dev.config[0x62] = 0x0b; /* PCI C -> IRQ 11 */
    pThis->dev.config[0x63] = 0x0b; /* PCI D -> IRQ 11 */

    pThis->dev.config[0x69] = 0x02;
    pThis->dev.config[0x70] = 0x80;
    pThis->dev.config[0x76] = 0x0c;
    pThis->dev.config[0x77] = 0x0c;
    pThis->dev.config[0x78] = 0x02;
    pThis->dev.config[0x79] = 0x00;
    pThis->dev.config[0x80] = 0x00;
    pThis->dev.config[0x82] = 0x00;
    pThis->dev.config[0xa0] = 0x08;
    pThis->dev.config[0xa2] = 0x00;
    pThis->dev.config[0xa3] = 0x00;
    pThis->dev.config[0xa4] = 0x00;
    pThis->dev.config[0xa5] = 0x00;
    pThis->dev.config[0xa6] = 0x00;
    pThis->dev.config[0xa7] = 0x00;
    pThis->dev.config[0xa8] = 0x0f;
    pThis->dev.config[0xaa] = 0x00;
    pThis->dev.config[0xab] = 0x00;
    pThis->dev.config[0xac] = 0x00;
    pThis->dev.config[0xae] = 0x00;


    /* We need to allow direct config reading from this address */
    pThis->dev.config[0xf0] = (uint8_t)(RCBA_BASE | 1); /* enabled */
    pThis->dev.config[0xf1] = (uint8_t)(RCBA_BASE >> 8);
    pThis->dev.config[0xf2] = (uint8_t)(RCBA_BASE >> 16);
    pThis->dev.config[0xf3] = (uint8_t)(RCBA_BASE >> 24);

    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the MMIO regions.
     */
    rc = PDMDevHlpMMIORegister(pDevIns, RCBA_BASE, 0x4000, pThis,
                               lpcMMIOWrite, lpcMMIORead, NULL, "LPC Memory");
    if (RT_FAILURE(rc))
        return rc;

    /* No state in the LPC right now */

    /*
     * Initialize the device state.
     */
    lpcReset(pDevIns);

    /**
     * @todo: Register statistics.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "lpc", "Display LPC status. (no arguments)", lpcInfo);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceLPC =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "lpc",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    " Low Pin Count (LPC) Bus",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(LPCState),
    /* pfnConstruct */
    lpcConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    lpcReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
