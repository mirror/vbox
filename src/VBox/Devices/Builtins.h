/** @file
 *
 * Built-in drivers & devices (part 1) headers
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __Builtins_h__
#define __Builtins_h__

#include <VBox/pdm.h>

__BEGIN_DECLS

__BEGIN_DECLS
/** The default BIOS logo data. */
extern const unsigned char  g_abPcDefBiosLogo[];
/** The size of the default BIOS logo data. */
extern const unsigned       g_cbPcDefBiosLogo;
__END_DECLS


extern const PDMDEVREG g_DevicePCI;
extern const PDMDEVREG g_DevicePcArch;
extern const PDMDEVREG g_DevicePcBios;
extern const PDMDEVREG g_DevicePS2KeyboardMouse;
extern const PDMDEVREG g_DeviceI8254;
extern const PDMDEVREG g_DeviceI8259;
extern const PDMDEVREG g_DeviceMC146818;
extern const PDMDEVREG g_DevicePIIX3IDE;
extern const PDMDEVREG g_DeviceFloppyController;
extern const PDMDEVREG g_DeviceVga;
extern const PDMDEVREG g_DeviceVMMDev;
extern const PDMDEVREG g_DevicePCNet;
extern const PDMDEVREG g_DeviceNE2000;
extern const PDMDEVREG g_DeviceICHAC97;
extern const PDMDEVREG g_DeviceAudioSniffer;
extern const PDMDEVREG g_DeviceUSBBackend;
extern const PDMDEVREG g_DeviceOHCI;
extern const PDMDEVREG g_DeviceACPI;
extern const PDMDEVREG g_DeviceDMA;
extern const PDMDEVREG g_DeviceFloppyController;
extern const PDMDEVREG g_DeviceSerialPort;

extern const PDMDRVREG g_DrvMouseQueue;
extern const PDMDRVREG g_DrvKeyboardQueue;
extern const PDMDRVREG g_DrvBlock;
extern const PDMDRVREG g_DrvVBoxHDD;
extern const PDMDRVREG g_DrvHostDVD;
extern const PDMDRVREG g_DrvHostFloppy;
extern const PDMDRVREG g_DrvMediaISO;
extern const PDMDRVREG g_DrvRawImage;
extern const PDMDRVREG g_DrvISCSI;
extern const PDMDRVREG g_DrvISCSITransportTcp;
extern const PDMDRVREG g_DrvHostInterface;
extern const PDMDRVREG g_DrvIntNet;
extern const PDMDRVREG g_DrvNAT;
extern const PDMDRVREG g_DrvNetSniffer;
extern const PDMDRVREG g_DrvAUDIO;
extern const PDMDRVREG g_DrvACPI;
extern const PDMDRVREG g_DrvVUSBRootHub;

__END_DECLS

#endif
