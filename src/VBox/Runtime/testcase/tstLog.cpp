/* $Id$ */
/** @file
 * Incredibly Portable Runtime Testcase - Log Formatting.
 */

/*
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/log.h>
#include <iprt/runtime.h>
#include <iprt/err.h>

#include <stdio.h>

int main()
{
    RTR3Init();
    printf("tstLog: Requires manual inspection of the log output!\n");
    RTLogPrintf("%%Vrc %d: %Vrc\n", VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER);
    RTLogPrintf("%%Vrs %d: %Vrs\n", VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER);
    RTLogPrintf("%%Vrf %d: %Vrf\n", VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER);
    RTLogPrintf("%%Vra %d: %Vra\n", VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER);

    RTLogPrintf("%%Vt: %Vt\n");

    static uint8_t au8Hex[256];
    for (unsigned iHex = 0; iHex < sizeof(au8Hex); iHex++)
        au8Hex[iHex] = (uint8_t)iHex;
    RTLogPrintf("%%Vhxs   : %Vhxs\n", &au8Hex[0]);
    RTLogPrintf("%%.32Vhxs: %.32Vhxs\n", &au8Hex[0]);

    RTLogPrintf("%%Vhxd   :\n%Vhxd\n", &au8Hex[0]);
    RTLogPrintf("%%.64Vhxd:\n%.64Vhxd\n", &au8Hex[0]);
    RTLogPrintf("%%.*Vhxd:\n%.*Vhxd\n", 64, &au8Hex[0]);
    RTLogPrintf("%%32.256Vhxd : \n%32.256Vhxd\n", &au8Hex[0]);
    RTLogPrintf("%%32.*Vhxd : \n%32.*Vhxd\n", 256, &au8Hex[0]);
    RTLogPrintf("%%7.32Vhxd : \n%7.32Vhxd\n", &au8Hex[0]);
    RTLogPrintf("%%7.*Vhxd : \n%7.*Vhxd\n", 32, &au8Hex[0]);
    RTLogPrintf("%%*.*Vhxd : \n%*.*Vhxd\n", 7, 32, &au8Hex[0]);

    RTLogPrintf("%%VGp: %VGp\n", (RTGCPHYS)0x87654321);
    RTLogPrintf("%%VGv: %VGv\n", (RTGCPTR)0x87654321);
    RTLogPrintf("%%VHp: %VHp\n", (RTGCPHYS)0x87654321);
    RTLogPrintf("%%VHv: %VHv\n", (RTGCPTR)0x87654321);

    RTLogPrintf("%%VI8 : %VI8\n", (uint8_t)808);
    RTLogPrintf("%%VI16: %VI16\n", (uint16_t)16016);
    RTLogPrintf("%%VI32: %VI32\n", _1G);
    RTLogPrintf("%%VI64: %VI64\n", _1E);

    RTLogPrintf("%%VU8 : %VU8\n", (uint8_t)808);
    RTLogPrintf("%%VU16: %VU16\n", (uint16_t)16016);
    RTLogPrintf("%%VU32: %VU32\n", _2G32);
    RTLogPrintf("%%VU64: %VU64\n", _2E);

    RTLogPrintf("%%VX8 : %VX8 %#VX8\n",   (uint8_t)808, (uint8_t)808);
    RTLogPrintf("%%VX16: %VX16 %#VX16\n", (uint16_t)16016, (uint16_t)16016);
    RTLogPrintf("%%VX32: %VX32 %#VX32\n", _2G32, _2G32);
    RTLogPrintf("%%VX64: %VX64 %#VX64\n", _2E, _2E);

    RTLogFlush(NULL);

    return 0;
}

