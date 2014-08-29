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
%define RT_ASM_WITH_SEH64
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
        SEH64_END_PROLOGUE
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

;;
; Composes a standard call name.
%ifdef RT_ARCH_X86
 %define SUPHNTIMP_STDCALL_NAME(a,b) _ %+ a %+ @ %+ b
%else
 %define SUPHNTIMP_STDCALL_NAME(a,b) NAME(a)
%endif


;;
; Import data and code for an API call.
;
; @param 1  The plain API name.
; @param 2  The parameter frame size on x86. Multiple of dword.
; @param 3  Non-zero expression if system call.
;
%define SUPHNTIMP_SYSCALL 1
%macro SupHardNtImport 3
        ;
        ; The data.
        ;
BEGINDATA
global __imp_ %+ SUPHNTIMP_STDCALL_NAME(%1,%2)  ; The import name used via dllimport.
__imp_ %+ SUPHNTIMP_STDCALL_NAME(%1,%2):
GLOBALNAME g_pfn %+ %1                          ; The name we like to refer to.
        RTCCPTR_DEF 0
%if %3
GLOBALNAME g_uApiNo %+ %1
        RTCCPTR_DEF 0
%endif

        ;
        ; The code: First a call stub.
        ;
BEGINCODE
global SUPHNTIMP_STDCALL_NAME(%1, %2)
SUPHNTIMP_STDCALL_NAME(%1, %2):
        jmp     RTCCPTR_PRE [NAME(g_pfn %+ %1) xWrtRIP]

%if %3
        ;
        ; Make system calls.
        ;
 %ifdef RT_ARCH_AMD64
BEGINPROC %1 %+ _SyscallType1
        SEH64_END_PROLOGUE
        mov     eax, [NAME(g_uApiNo %+ %1) xWrtRIP]
        mov     r10, rcx
        syscall
        ret
ENDPROC %1 %+ _SyscallType1
 %else
BEGINPROC %1 %+ _SyscallType1
        mov     edx, 07ffe0300h         ; SharedUserData!SystemCallStub
        mov     eax, [NAME(g_uApiNo %+ %1) xWrtRIP]
        call    dword [edx]
        ret     %2
ENDPROC %1 %+ _SyscallType1
BEGINPROC %1 %+ _SyscallType2
        push    .return
        mov     edx, esp
        mov     eax, [NAME(g_uApiNo %+ %1) xWrtRIP]
        sysenter
        add     esp, 4
.return:
        ret     %2
ENDPROC %1 %+ _SyscallType2
  %endif
%endif
%endmacro

%define SUPHARNT_COMMENT(a_Comment)
%define SUPHARNT_IMPORT_SYSCALL(a_Name, a_cbParamsX86) SupHardNtImport a_Name, a_cbParamsX86, SUPHNTIMP_SYSCALL
%define SUPHARNT_IMPORT_STDCALL(a_Name, a_cbParamsX86) SupHardNtImport a_Name, a_cbParamsX86, 0
%include "import-template-ntdll.h"
%include "import-template-kernel32.h"


;
; For simplified LdrLoadDll patching we define a special writable, readable and
; exectuable section of 4KB where we can put jump back code.
;
section .rwxpg bss execute read write align=4096
GLOBALNAME g_abSupHardReadWriteExecPage
        resb    4096

