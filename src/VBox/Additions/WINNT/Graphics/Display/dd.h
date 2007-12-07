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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef __VBOX_DD_H__
#define __VBOX_DD_H__

#include <winddi.h>

BOOL APIENTRY DrvGetDirectDrawInfo(DHPDEV dhpdev, DD_HALINFO *pHalInfo, DWORD *pdwNumHeaps, VIDEOMEMORY *pvmList, DWORD *pdwNumFourCCCodes, DWORD *pdwFourCC);
BOOL APIENTRY DrvEnableDirectDraw(DHPDEV dhpdev, DD_CALLBACKS *pCallBacks, DD_SURFACECALLBACKS *pSurfaceCallBacks, DD_PALETTECALLBACKS *pPaletteCallBacks);
VOID APIENTRY DrvDisableDirectDraw(DHPDEV dhpdev);
DWORD CALLBACK DdGetDriverInfo(DD_GETDRIVERINFODATA *lpData);
DWORD APIENTRY DdCreateSurface(PDD_CREATESURFACEDATA  lpCreateSurface);
DWORD APIENTRY DdCanCreateSurface(PDD_CANCREATESURFACEDATA lpCanCreateSurface);
DWORD APIENTRY DdDestroySurface(PDD_DESTROYSURFACEDATA lpDestroySurface);
DWORD APIENTRY DdLock(PDD_LOCKDATA lpLock);
DWORD APIENTRY DdUnlock(PDD_UNLOCKDATA lpUnlock);
DWORD APIENTRY DdFreeDriverMemory(PDD_FREEDRIVERMEMORYDATA lpFreeDriverMemory);
DWORD APIENTRY DdSetExclusiveMode(PDD_SETEXCLUSIVEMODEDATA lpSetExclusiveMode);
DWORD APIENTRY DdFlipToGDISurface(PDD_FLIPTOGDISURFACEDATA lpFlipToGDISurface);
HBITMAP DrvDeriveSurface(DD_DIRECTDRAW_GLOBAL*  pDirectDraw, DD_SURFACE_LOCAL* pSurface);
DWORD CALLBACK DdMapMemory(PDD_MAPMEMORYDATA lpMapMemory);

#endif /* __VBOX_DD_H__*/
