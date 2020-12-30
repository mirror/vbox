; $Id$
;; @file
; IPRT - Zero Memory, mach-o version (for arm).
;

;
; Copyright (C) 2013-2020 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;


; Putting it in the code segment/section for now.
#ifdef ASM_FORMAT_MACHO
        .section    __TEXT,__text,regular,pure_instructions
        .section    __TEXT,__const
# define NAME(a) _##a
#else
# error "PORT ME!"
#endif

;;
; 64KB of zero memory with various sized labels.
        .globl      NAME(g_abRTZeroPage)
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZeroPage),16384
#endif
NAME(g_abRTZeroPage):
        .globl      NAME(g_abRTZero4K)
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZero4K),4096
#endif
NAME(g_abRTZero4K):
        .globl      NAME(g_abRTZero8K)
NAME(g_abRTZero8K):
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZero8K),8192
#endif
        .globl      NAME(g_abRTZero16K)
NAME(g_abRTZero16K):
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZero16K),16384
#endif
        .globl      NAME(g_abRTZero32K)
NAME(g_abRTZero32K):
        .globl      NAME(g_abRTZero64K)
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZero32K),32768
#endif
NAME(g_abRTZero64K):
        .space      65536
#ifdef ASM_FORMAT_ELF
        .size       NAME(g_abRTZero64K),65536
#endif

