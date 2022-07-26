/* $Id$ */
/** @file
 * IPRT - No-CRT - Basic allocators, Windows.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include "internal/iprt.h"
#include "internal/compiler-vcc.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Very like some remnant from the 8086, 286 and 386 days of DOS, OS/2 and
 * similar, where you could link with different floating point libs.  My guess
 * would be that this constant indicates to the compiled code which floating
 * point support the library provides, probably especially as it comes to
 * printf and scanf conversion but probably also emulation/real hw.
 *
 * Found some old 16-bit and 32-bit MSC C libraries (probably around v6.0)
 * which all seems to define it as 0x9876.  They also have a whole bunch of
 * external dependencies on what seems to be mostly conversion helpers.
 */
unsigned _fltused = 0x9875;
