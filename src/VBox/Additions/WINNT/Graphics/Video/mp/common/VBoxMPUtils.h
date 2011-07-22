/* $Id$ */

/** @file
 * VBox Miniport common utils header
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXMPUTILS_H
#define VBOXMPUTILS_H

/*Sanity check*/
#if defined(VBOX_XPDM_MINIPORT)==defined(VBOX_WDDM_MINIPORT)
#error One of the VBOX_XPDM_MINIPORT or VBOX_WDDM_MINIPORT should be defined!
#endif

#include <iprt/cdefs.h>
#define LOG_GROUP LOG_GROUP_DRV_MINIPORT
#include <VBox/log.h>
#define VBOX_VIDEO_LOG_NAME "VBoxMP"
#include "common/VBoxVideoLog.h"
#include <iprt/err.h>
#include <iprt/assert.h>

RT_C_DECLS_BEGIN
#ifdef VBOX_XPDM_MINIPORT
#  include <dderror.h>
#  include <devioctl.h>
#else
#  ifdef PAGE_SIZE
#    undef PAGE_SIZE
#  endif
#  ifdef PAGE_SHIFT
#    undef PAGE_SHIFT
#  endif
#  define VBOX_WITH_WORKAROUND_MISSING_PACK
#  if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#    define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#    define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#    define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#    define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#    define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#    define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#    define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#    define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#    pragma warning(disable : 4163)
#    ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#      pragma warning(disable : 4103)
#    endif
#    include <ntddk.h>
#    pragma warning(default : 4163)
#    ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#      pragma pack()
#      pragma warning(default : 4103)
#    endif
#    undef  _InterlockedExchange
#    undef  _InterlockedExchangeAdd
#    undef  _InterlockedCompareExchange
#    undef  _InterlockedAddLargeStatistic
#    undef  _interlockedbittestandset
#    undef  _interlockedbittestandreset
#    undef  _interlockedbittestandset64
#    undef  _interlockedbittestandreset64
#  else
#    include <ntddk.h>
#  endif
#  include <dispmprt.h>
#  include <ntddvdeo.h>
#  include <dderror.h>
#endif
RT_C_DECLS_END

/*Windows version identifier*/
typedef enum
{
    UNKNOWN_WINVERSION = 0,
    WINNT4    = 1,
    WIN2K     = 2,
    WINXP     = 3,
    WINVISTA  = 4,
    WIN7      = 5
} vboxWinVersion_t;

RT_C_DECLS_BEGIN
vboxWinVersion_t VBoxQueryWinVersion();
uint32_t VBoxGetHeightReduction();
bool VBoxLikesVideoMode(uint32_t display, uint32_t width, uint32_t height, uint32_t bpp);
bool VBoxQueryDisplayRequest(uint32_t *xres, uint32_t *yres, uint32_t *bpp, uint32_t *pDisplayId);
bool VBoxQueryHostWantsAbsolute();
bool VBoxQueryPointerPos(uint16_t *pPosX, uint16_t *pPosY);
RT_C_DECLS_END

#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES 4*_1M

#define VBOXMP_WARN_VPS_NOBP(_vps)     \
if ((_vps) != NO_ERROR)           \
{                                 \
    WARN_NOBP(("vps(%#x)!=NO_ERROR", _vps)); \
}

#define VBOXMP_WARN_VPS(_vps)     \
if ((_vps) != NO_ERROR)           \
{                                 \
    WARN(("vps(%#x)!=NO_ERROR", _vps)); \
}


#define VBOXMP_CHECK_VPS_BREAK(_vps)    \
if ((_vps) != NO_ERROR)                 \
{                                       \
    break;                              \
}

#ifdef DEBUG_misha
/* specifies whether the vboxVDbgBreakF should break in the debugger
 * windbg seems to have some issues when there is a lot ( >~50) of sw breakpoints defined
 * to simplify things we just insert breaks for the case of intensive debugging WDDM driver*/
extern bool g_bVBoxVDbgBreakF;
extern bool g_bVBoxVDbgBreakFv;
#define vboxVDbgBreakF() do { if (g_bVBoxVDbgBreakF) AssertBreakpoint(); } while (0)
#define vboxVDbgBreakFv() do { if (g_bVBoxVDbgBreakFv) AssertBreakpoint(); } while (0)
#else
#define vboxVDbgBreakF() do { } while (0)
#define vboxVDbgBreakFv() do { } while (0)
#endif

#endif /*VBOXMPUTILS_H*/
