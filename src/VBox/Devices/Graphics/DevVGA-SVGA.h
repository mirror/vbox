/** @file
 * VMware SVGA device
 */
/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef __DEVVGA_SVGA_H__
#define __DEVVGA_SVGA_H__


/** Default FIFO size. */
#define VMSVGA_FIFO_SIZE                0x20000
/** Default scratch region size. */
#define VMSVGA_SCRATCH_SIZE             0x100
/** Surface memory available to the guest. */
#define VMSVGA_SURFACE_SIZE             (512*1024*1024)
/** Maximum GMR pages. */
#define VMSVGA_MAX_GMR_PAGES            0x100000
/** Maximum nr of GMR ids. */
#define VMSVGA_MAX_GMR_IDS              0x100

#define VMSVGA_VAL_UNINITIALIZED        (unsigned)-1

/** For validating X and width values.
 * The code assumes it's at least an order of magnitude less than UINT32_MAX. */
#define VMSVGA_MAX_X                    _1M
/** For validating Y and height values.
 * The code assumes it's at least an order of magnitude less than UINT32_MAX. */
#define VMSVGA_MAX_Y                    _1M

/* u32ActionFlags */
#define VMSVGA_ACTION_CHANGEMODE_BIT    0
#define VMSVGA_ACTION_CHANGEMODE        RT_BIT(VMSVGA_ACTION_CHANGEMODE_BIT)

DECLCALLBACK(int) vmsvgaR3IORegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType);

DECLCALLBACK(void) vmsvgaPortSetViewport(PPDMIDISPLAYPORT pInterface, uint32_t uScreenId, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);

int vmsvgaInit(PPDMDEVINS pDevIns);
int vmsvgaReset(PPDMDEVINS pDevIns);
int vmsvgaDestruct(PPDMDEVINS pDevIns);
int vmsvgaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
int vmsvgaLoadDone(PPDMDEVINS pDevIns);
int vmsvgaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
DECLCALLBACK(void) vmsvgaR3PowerOn(PPDMDEVINS pDevIns);
DECLCALLBACK(void) vmsvgaR3PowerOff(PPDMDEVINS pDevIns);

#endif  /* __DEVVGA_SVGA_H__ */
