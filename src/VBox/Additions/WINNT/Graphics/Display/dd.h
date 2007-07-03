/** @file
 *
 * VirtualBox Windows NT/2000/XP guest video driver
 *
 * Display driver screen DirectDraw entry points.
 *
 * Copyright (C) 2006-2007 innotek GmbH
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
 *
 */
#ifndef __VBOX_DD_H__
#define __VBOX_DD_H__

#include <winddi.h>

BOOL APIENTRY DrvGetDirectDrawInfo(DHPDEV dhpdev, DD_HALINFO *pHalInfo, DWORD *pdwNumHeaps, VIDEOMEMORY *pvmList, DWORD *pdwNumFourCCCodes, DWORD *pdwFourCC);
BOOL APIENTRY DrvEnableDirectDraw(DHPDEV dhpdev, DD_CALLBACKS *pCallBacks, DD_SURFACECALLBACKS *pSurfaceCallBacks, DD_PALETTECALLBACKS *pPaletteCallBacks);
VOID APIENTRY DrvDisableDirectDraw(DHPDEV dhpdev);


#endif /* __VBOX_DD_H__*/
