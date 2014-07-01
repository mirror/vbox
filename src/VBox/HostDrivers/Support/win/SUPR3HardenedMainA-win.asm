; $Id$
;; @file
; VirtualBox Support Library - Hardened main(), Windows assembly bits.
;

;
; Copyright (C) 2012-2014 Oracle Corporation
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


; External data.
extern NAME(g_pfnNtCreateSectionJmpBack)


BEGINCODE

;
; 64-bit
;
%ifdef RT_ARCH_AMD64
 %macro supR3HardenedJmpBack_NtCreateSection_Xxx 1
 BEGINPROC supR3HardenedJmpBack_NtCreateSection_ %+ %1
        ; The code we replaced.
        mov     r10, rcx
        mov     eax, %1

        ; Jump back to the original code.
        jmp     [NAME(g_pfnNtCreateSectionJmpBack) wrt RIP]
 ENDPROC   supR3HardenedJmpBack_NtCreateSection_ %+ %1
 %endm
 %define SYSCALL(a_Num) supR3HardenedJmpBack_NtCreateSection_Xxx a_Num
 %include "NtCreateSection-template-amd64-syscall-type-1.h"

%endif


;
; 32-bit.
;
%ifdef RT_ARCH_X86
 %macro supR3HardenedJmpBack_NtCreateSection_Xxx 1
 BEGINPROC supR3HardenedJmpBack_NtCreateSection_ %+ %1
        ; The code we replaced.
        mov     eax, %1

        ; Jump back to the original code.
        jmp     [NAME(g_pfnNtCreateSectionJmpBack)]
 ENDPROC   supR3HardenedJmpBack_NtCreateSection_ %+ %1
 %endm
 %define SYSCALL(a_Num) supR3HardenedJmpBack_NtCreateSection_Xxx a_Num
 %include "NtCreateSection-template-x86-syscall-type-1.h"

%endif

