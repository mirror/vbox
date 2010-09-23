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
#ifndef ___VBoxDispD3DCmn_h___
#define ___VBoxDispD3DCmn_h___

#include <windows.h>
#include <d3d9types.h>
//#include <d3dtypes.h>
#include <D3dumddi.h>
#include <d3dhal.h>

#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxDispD3DIf.h"
#include "../../Miniport/wddm/VBoxVideoIf.h"
#include "VBoxDispCm.h"
#include "VBoxDispD3D.h"

#ifdef DEBUG
# define VBOXWDDMDISP_DEBUG
# define VBOXWDDMDISP_DEBUG_FLOW
# define VBOXWDDMDISP_DEBUG_DUMPSURFDATA
#endif

#if defined(VBOXWDDMDISP_DEBUG) || defined(VBOX_WDDMDISP_WITH_PROFILE)
# define VBOXWDDMDISP_DEBUG_PRINT
#endif

#if 0
# ifdef Assert
#  undef Assert
#  define Assert(_a) do{}while(0)
# endif
# ifdef AssertBreakpoint
#  undef AssertBreakpoint
#  define AssertBreakpoint() do{}while(0)
# endif
# ifdef AssertFailed
#  undef AssertFailed
#  define AssertFailed() do{}while(0)
# endif
#endif

#ifdef VBOXWDDMDISP_DEBUG_PRINT
VOID vboxVDbgDoMpPrintF(const PVBOXWDDMDISP_DEVICE pDevice, LPCSTR szString, ...);
VOID vboxVDbgDoPrint(LPCSTR szString, ...);
#endif

#ifdef VBOXWDDMDISP_DEBUG
extern bool g_VDbgTstDumpEnable;
extern bool g_VDbgTstDumpOnSys2VidSameSizeEnable;

VOID vboxVDbgDoDumpSurfData(const PVBOXWDDMDISP_DEVICE pDevice, const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const RECT *pRect, IDirect3DSurface9 *pSurf, const char* pSuffix);
void vboxVDbgDoMpPrintRect(const PVBOXWDDMDISP_DEVICE pDevice, const char * pPrefix, const RECT *pRect, const char * pSuffix);
void vboxVDbgDoMpPrintAlloc(const PVBOXWDDMDISP_DEVICE pDevice, const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const char * pSuffix);
void vboxVDbgVEHandlerRegister();
void vboxVDbgVEHandlerUnregister();

#define vboxVDbgBreak() AssertBreakpoint()
#define vboxVDbgPrint(_m) \
    do { \
        vboxVDbgDoPrint _m ; \
    } while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#define vboxVDbgMpPrint(_m) \
    do { \
        vboxVDbgDoMpPrintF _m ; \
    } while (0)
#define vboxVDbgMpPrintRect(_m) \
    do { \
        vboxVDbgDoMpPrintRect _m ; \
    } while (0)
#define vboxVDbgMpPrintAlloc(_m) \
    do { \
        vboxVDbgDoMpPrintAlloc _m ; \
    } while (0)
#ifdef VBOXWDDMDISP_DEBUG_DUMPSURFDATA
#define vboxVDbgDumpSurfData(_m) \
    do { \
        vboxVDbgDoDumpSurfData _m ; \
    } while (0)
#else
#define vboxVDbgDumpSurfData(_m) do {} while (0)
#endif
#ifdef VBOXWDDMDISP_DEBUG_FLOW
# define vboxVDbgPrintF  vboxVDbgPrint
#else
# define vboxVDbgPrintF(_m)  do {} while (0)
#endif
#else
#define vboxVDbgMpPrint(_m) do {} while (0)
#define vboxVDbgMpPrintRect(_m) do {} while (0)
#define vboxVDbgMpPrintAlloc(_m) do {} while (0)
#define vboxVDbgDumpSurfData(_m) do {} while (0)
#define vboxVDbgBreak() do {} while (0)
#define vboxVDbgPrint(_m)  do {} while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#define vboxVDbgPrintF vboxVDbgPrint
#endif

#endif /* #ifndef ___VBoxDispD3DCmn_h___ */
