/** @file
 *
 * VirtualBox Windows NT/2000/XP guest video driver
 *
 * Display driver screen DirectDraw entry points.
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#ifdef VBOX_WITH_VIDEOHWACCEL
DWORD APIENTRY DdSetColorKey(PDD_SETCOLORKEYDATA  lpSetColorKey);
DWORD APIENTRY DdAddAttachedSurface(PDD_ADDATTACHEDSURFACEDATA  lpAddAttachedSurface);
DWORD APIENTRY DdBlt(PDD_BLTDATA  lpBlt);
DWORD APIENTRY DdDestroySurface(PDD_DESTROYSURFACEDATA  lpDestroySurface);
DWORD APIENTRY DdFlip(PDD_FLIPDATA  lpFlip);
DWORD APIENTRY DdGetBltStatus(PDD_GETBLTSTATUSDATA  lpGetBltStatus);
DWORD APIENTRY DdGetFlipStatus(PDD_GETFLIPSTATUSDATA  lpGetFlipStatus);
DWORD APIENTRY DdSetOverlayPosition(PDD_SETOVERLAYPOSITIONDATA  lpSetOverlayPosition);
DWORD APIENTRY DdUpdateOverlay(PDD_UPDATEOVERLAYDATA  lpUpdateOverlay);
#endif

#endif /* __VBOX_DD_H__*/
