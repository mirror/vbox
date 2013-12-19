/** @file
 * VMware SVGA device
 */
/*
 * Copyright (C) 2013 Oracle Corporation
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
#define VMSVGA_MAX_GMR_PAGES            0x2000
/** Maximum nr of GMR ids. */
#define VMSVGA_MAX_GMR_IDS              0x100
/** Size of the region to backup when switching into svga mode. */
#define VMSVGA_FRAMEBUFFER_BACKUP_SIZE  (32*1024)

/* u32ActionFlags */
#define VMSVGA_ACTION_CHANGEMODE_BIT    0
#define VMSVGA_ACTION_CHANGEMODE        RT_BIT(VMSVGA_ACTION_CHANGEMODE_BIT)

DECLCALLBACK(int) vmsvgaR3IORegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType);

int vmsvgaInit(PPDMDEVINS pDevIns);
int vmsvgaDestruct(PPDMDEVINS pDevIns);
int vmsvgaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
int vmsvgaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
DECLCALLBACK(void) vmsvgaR3PowerOn(PPDMDEVINS pDevIns);

#endif  /* __DEVVGA_SVGA_H__ */

