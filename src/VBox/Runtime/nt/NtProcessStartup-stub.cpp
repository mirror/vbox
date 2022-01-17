/* $Id$ */
/** @file
 * IPRT - NtProcessStartup stub to make the link happy.
 */

/*
 * Copyright (C) 2009-2022 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>


/**
 * This is the entrypoint the linker picks, however it is never called for
 * ring-0 binaries.
 */
extern "C" void __cdecl NtProcessStartup(void *pvIgnored);
extern "C" void __cdecl NtProcessStartup(void *pvIgnored)
{
    ASMBreakpoint();
    NOREF(pvIgnored);
}


#ifdef IN_RING0
/**
 * This dummy entry point is required for using BufferOverflowK.lib and
 * /guard:cf and /GS.  It is never called.
 */
extern "C" long __stdcall DriverEntry(void *pvDrvObjIgn, void *pvRegPathIgn);
extern "C" long __stdcall DriverEntry(void *pvDrvObjIgn, void *pvRegPathIgn)
{
    ASMBreakpoint();
    RT_NOREF(pvDrvObjIgn, pvRegPathIgn);
    return UINT32_C(0xc0000022);
}
#endif

