; $Id$
;; @file
; IPRT - ASMCpuIdExSlow().
;

;
; Copyright (C) 2012-2016 Oracle Corporation
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; CPUID with EAX and ECX inputs, returning ALL output registers.
;
; @param    uOperator   x86:ebp+8   gcc:rdi      msc:rcx
; @param    uInitEBX    x86:ebp+c   gcc:rsi      msc:rdx
; @param    uInitECX    x86:ebp+10  gcc:rdx      msc:r8
; @param    uInitEDX    x86:ebp+14  gcc:rcx      msc:r9
; @param    pvEAX       x86:ebp+18  gcc:r8       msc:rbp+30h
; @param    pvEBX       x86:ebp+1c  gcc:r9       msc:rbp+38h
; @param    pvECX       x86:ebp+20  gcc:rbp+10h  msc:rbp+40h
; @param    pvEDX       x86:ebp+24  gcc:rbp+18h  msc:rbp+48h
;
; @returns  EAX
;
BEGINPROC_EXPORTED ASMCpuIdExSlow
        push    xBP
        mov     xBP, xSP
        push    xBX
%ifdef RT_ARCH_X86
        push    edi
%endif

%ifdef ASM_CALL64_MSC
        mov     eax, ecx
        mov     ebx, edx
        mov     ecx, r8d
        mov     edx, r9d
        mov     r8,  [rbp + 30h]
        mov     r9,  [rbp + 38h]
        mov     r10, [rbp + 40h]
        mov     r11, [rbp + 48h]
%elifdef ASM_CALL64_GCC
        mov     eax, edi
        mov     ebx, esi
        xchg    ecx, edx
        mov     r10, [rbp + 10h]
        mov     r11, [rbp + 18h]
%elifdef RT_ARCH_X86
        mov     eax, [ebp + 08h]
        mov     ebx, [ebp + 0ch]
        mov     ecx, [ebp + 10h]
        mov     edx, [ebp + 14h]
        mov     edi, [ebp + 18h]
%else
 %error unsupported arch
%endif

        cpuid

%ifdef RT_ARCH_AMD64
        test    r8, r8
        jz      .store_ebx
        mov     [r8], eax
%else
        test    edi, edi
        jz      .store_ebx
        mov     [edi], eax
%endif
.store_ebx:

%ifdef RT_ARCH_AMD64
        test    r9, r9
        jz      .store_ecx
        mov     [r9], ebx
%else
        mov     edi, [ebp + 1ch]
        test    edi, edi
        jz      .store_ecx
        mov     [edi], ebx
%endif
.store_ecx:

%ifdef RT_ARCH_AMD64
        test    r10, r10
        jz      .store_edx
        mov     [r10], ecx
%else
        mov     edi, [ebp + 20h]
        test    edi, edi
        jz      .store_edx
        mov     [edi], ecx
%endif
.store_edx:

%ifdef RT_ARCH_AMD64
        test    r11, r11
        jz      .done
        mov     [r11], edx
%else
        mov     edi, [ebp + 24h]
        test    edi, edi
        jz      .done
        mov     [edi], edx
%endif
.done:

%ifdef RT_ARCH_X86
        pop     edi
%endif
        pop     xBX
        leave
        ret
ENDPROC ASMCpuIdExSlow

