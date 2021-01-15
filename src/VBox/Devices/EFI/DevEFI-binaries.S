/* $Id$ */
/** @file
 * DevEFI - firmware binaries.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifdef RT_OS_DARWIN
#define NAME(a_Name) _##a_Name
#else
#define NAME(a_Name) a_Name
#endif

.section __TEXT,__const

; 32-bit firmware:
.p2align 6
.globl NAME(g_abEfiFirmware32)
NAME(g_abEfiFirmware32):
        .incbin "VBoxEFI32.fd"
end_32_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmware32)
NAME(g_cbEfiFirmware32):
        .long end_32_firmware - NAME(g_abEfiFirmware32)


; 64-bit firmware:
.p2align 6
.globl NAME(g_abEfiFirmware64)
NAME(g_abEfiFirmware64):
        .incbin "VBoxEFI64.fd"
end_64_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmware64)
NAME(g_cbEfiFirmware64):
        .long  end_64_firmware - NAME(g_abEfiFirmware64)

#ifdef ASM_FORMAT_ELF
.size g_abEfiFirmware32 end_32_firmware - NAME(g_abEfiFirmware32)
.type g_abEfiFirmware32 object
.size g_cbEfiFirmware32 4
.type g_cbEfiFirmware32 object

.size g_abEfiFirmware64 end_64_firmware - NAME(g_abEfiFirmware64)
.type g_abEfiFirmware64 object
.size g_cbEfiFirmware64 4
.type g_cbEfiFirmware64 object
#endif
