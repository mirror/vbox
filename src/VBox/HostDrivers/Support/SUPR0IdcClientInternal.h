/* $Id$ */
/** @file
 * VirtualBox Support Driver - Internal header for the IDC client library.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___SUPR0IdcClientInternal_h__
#define ___SUPR0IdcClientInternal_h__

#include <VBox/types.h>
#include <iprt/assert.h>

#ifdef RT_OS_WINDOWS
# if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#  include <iprt/asm.h>
#  define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#  define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#  define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#  define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#  pragma warning(disable : 4163)
__BEGIN_DECLS
#  include <ntddk.h>
__END_DECLS
#  pragma warning(default : 4163)
#  undef  _InterlockedExchange
#  undef  _InterlockedExchangeAdd
#  undef  _InterlockedCompareExchange
#  undef  _InterlockedAddLargeStatistic
# else
__BEGIN_DECLS
#  include <ntddk.h>
__END_DECLS
# endif
#endif /* RT_OS_WINDOWS */


/**
 * The hidden part of SUPDRVIDCHANDLE.
 */
struct SUPDRVIDCHANDLEPRIVATE
{
    /** Pointer to the session handle. */
    PSUPDRVSESSION  pSession;
# ifdef RT_OS_WINDOWS
    /** Pointer to the NT device object. */
    PDEVICE_OBJECT  pDeviceObject;
    /** Pointer to the NT file object. */
    PFILE_OBJECT    pFileObject;
# endif
};
/** Indicate that the structure is present. */
#define SUPDRVIDCHANDLEPRIVATE_DECLARED 1

#include <VBox/sup.h>
#include "SUPDrvIDC.h"
AssertCompile(RT_SIZEOFMEMB(SUPDRVIDCHANDLE, apvPadding) >= sizeof(struct SUPDRVIDCHANDLEPRIVATE));

__BEGIN_DECLS
PSUPDRVIDCHANDLE supR0IdcGetHandleFromSession(PSUPDRVSESSION pSession);
int VBOXCALL supR0IdcNativeOpen(PSUPDRVIDCHANDLE pHandle, PSUPDRVIDCREQCONNECT pReq);
int VBOXCALL supR0IdcNativeClose(PSUPDRVIDCHANDLE pHandle, PSUPDRVIDCREQHDR pReq);
int VBOXCALL supR0IdcNativeCall(PSUPDRVIDCHANDLE pHandle, uint32_t iReq, PSUPDRVIDCREQHDR pReq);
__END_DECLS

#endif

