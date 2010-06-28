/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
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
#ifndef ___VBoxDispMp_h___
#define ___VBoxDispMp_h___

#include <windows.h>
#include <d3d9types.h>
#include <D3dumddi.h>
#include <d3dhal.h>
#include "../../Miniport/wddm/VBoxVideoIf.h"

HRESULT vboxDispMpEnable();
HRESULT vboxDispMpDisable();

typedef struct VBOXDISPMP_REGIONS
{
    HWND hWnd;
    PVBOXVIDEOCM_CMD_RECTS pRegions;
} VBOXDISPMP_REGIONS, *PVBOXDISPMP_REGIONS;

/*
 * the VBOXDISPMP_REGIONS::pRegions returned is valid until the next vboxDispMpGetRegions call or vboxDispMpDisable call
 */
HRESULT vboxDispMpGetRegions(PVBOXDISPMP_REGIONS pRegions, DWORD dwMilliseconds);

#endif /* #ifndef ___VBoxDispMp_h___ */
