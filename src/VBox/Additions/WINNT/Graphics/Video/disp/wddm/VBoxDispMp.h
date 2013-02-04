/* $Id$ */

/** @file
 * VBoxVideo Display external interface
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include "../../common/wddm/VBoxMPIf.h"

typedef struct VBOXDISPMP_REGIONS
{
    HWND hWnd;
    PVBOXVIDEOCM_CMD_RECTS pRegions;
} VBOXDISPMP_REGIONS, *PVBOXDISPMP_REGIONS;

typedef DECLCALLBACK(HRESULT) FNVBOXDISPMP_ENABLEEVENTS();
typedef FNVBOXDISPMP_ENABLEEVENTS *PFNVBOXDISPMP_ENABLEEVENTS;

typedef DECLCALLBACK(HRESULT) FNVBOXDISPMP_DISABLEEVENTS();
typedef FNVBOXDISPMP_DISABLEEVENTS *PFNVBOXDISPMP_DISABLEEVENTS;

typedef DECLCALLBACK(HRESULT) FNVBOXDISPMP_DISABLEEVENTS();
typedef FNVBOXDISPMP_DISABLEEVENTS *PFNVBOXDISPMP_DISABLEEVENTS;

typedef DECLCALLBACK(HRESULT) FNVBOXDISPMP_GETREGIONS(PVBOXDISPMP_REGIONS pRegions, DWORD dwMilliseconds);
typedef FNVBOXDISPMP_GETREGIONS *PFNVBOXDISPMP_GETREGIONS;

typedef DECLCALLBACK(void) FNVBOXDISPMP_LOG(LPCSTR pszMsg);
typedef FNVBOXDISPMP_LOG *PFNVBOXDISPMP_LOG;


typedef struct VBOXDISPMP_CALLBACKS
{
    PFNVBOXDISPMP_ENABLEEVENTS pfnEnableEvents;
    PFNVBOXDISPMP_DISABLEEVENTS pfnDisableEvents;
    /**
     * if events are enabled - blocks until dirty region is available or timeout occurs
     * in the former case S_OK is returned on event, in the latter case WAIT_TIMEOUT is returned
     * if events are disabled - returns S_FALSE
     */
    PFNVBOXDISPMP_GETREGIONS pfnGetRegions;
} VBOXDISPMP_CALLBACKS, *PVBOXDISPMP_CALLBACKS;

/** @def VBOXNETCFGWIN_DECL
 * The usual declaration wrapper.
 */

/* enable this in case we include this in a dll*/
# ifdef VBOXWDDMDISP
#  define VBOXDISPMP_DECL(_type) DECLEXPORT(_type) VBOXCALL
# else
#  define VBOXDISPMP_DECL(_type) DECLIMPORT(_type) VBOXCALL
# endif

#define VBOXDISPMP_IFVERSION 3
#define VBOXDISPMP_VERSION (VBOXVIDEOIF_VERSION | (VBOXDISPMP_IFVERSION < 16))
/**
 *  VBoxDispMpGetCallbacks export
 *
 *  @param u32Version - must be set to VBOXDISPMP_VERSION
 *  @param pCallbacks - callbacks structure
 */
typedef VBOXDISPMP_DECL(HRESULT) FNVBOXDISPMP_GETCALLBACKS(uint32_t u32Version, PVBOXDISPMP_CALLBACKS pCallbacks);
typedef FNVBOXDISPMP_GETCALLBACKS *PFNVBOXDISPMP_GETCALLBACKS;

#endif /* #ifndef ___VBoxDispMp_h___ */
