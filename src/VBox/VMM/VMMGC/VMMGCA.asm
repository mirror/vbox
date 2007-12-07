; $Id$
;; @file
; VMMGC - Guest Context Virtual Machine Monitor assembly routines.
;

; Copyright (C) 2006-2007 innotek GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/x86.mac"


;*******************************************************************************
;*   Defined Constants And Macros                                              *
;*******************************************************************************
;; save all registers before loading special values for the faulting.
%macro SaveAndLoadAll 0
    pushad
    push    ds
    push    es
    push    fs
    push    gs
    call    NAME(vmmGCTestLoadRegs)
%endmacro

;; restore all registers after faulting.
%macro RestoreAll 0
    pop     gs
    pop     fs
    pop     es
    pop     ds
    popad
%endmacro


;*******************************************************************************
;* External Symbols                                                            *
;*******************************************************************************
extern IMPNAME(g_Logger)
extern IMPNAME(g_RelLogger)
extern NAME(RTLogLogger)


BEGINCODE

;/**
; * Internal GC logger worker: Logger wrapper.
; */
;VMMGCDECL(void) vmmGCLoggerWrapper(const char *pszFormat, ...);
EXPORTEDNAME vmmGCLoggerWrapper
%ifdef __YASM__
%ifdef ASM_FORMAT_ELF
    push    dword IMP(g_Logger)         ; YASM BUG #67! YASMCHECK!
%else
    push    IMP(g_Logger)
%endif
%else
    push    IMP(g_Logger)
%endif
    call    NAME(RTLogLogger)
    add     esp, byte 4
    ret
ENDPROC vmmGCLoggerWrapper


;/**
; * Internal GC logger worker: Logger (release) wrapper.
; */
;VMMGCDECL(void) vmmGCRelLoggerWrapper(const char *pszFormat, ...);
EXPORTEDNAME vmmGCRelLoggerWrapper
%ifdef __YASM__
%ifdef ASM_FORMAT_ELF
    push    dword IMP(g_RelLogger)         ; YASM BUG #67! YASMCHECK!
%else
    push    IMP(g_RelLogger)
%endif
%else
    push    IMP(g_RelLogger)
%endif
    call    NAME(RTLogLogger)
    add     esp, byte 4
    ret
ENDPROC vmmGCRelLoggerWrapper


;;
; Enables write protection.
BEGINPROC vmmGCEnableWP
    push    eax
    mov     eax, cr0
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
    pop     eax
    ret
ENDPROC vmmGCEnableWP


;;
; Disables write protection.
BEGINPROC vmmGCDisableWP
    push    eax
    mov     eax, cr0
    and     eax, ~X86_CR0_WRITE_PROTECT
    mov     cr0, eax
    pop     eax
    ret
ENDPROC vmmGCDisableWP


;;
; Load special register set expected upon faults.
; All registers are changed.
BEGINPROC vmmGCTestLoadRegs
    mov     eax, ss
    mov     ds, eax
    mov     es, eax
    mov     fs, eax
    mov     gs, eax
    mov     edi, 001234567h
    mov     esi, 042000042h
    mov     ebp, 0ffeeddcch
    mov     ebx, 089abcdefh
    mov     ecx, 0ffffaaaah
    mov     edx, 077778888h
    mov     eax, 0f0f0f0f0h
    ret
ENDPROC vmmGCTestLoadRegs


;;
; A Trap 3 testcase.
GLOBALNAME vmmGCTestTrap3
    SaveAndLoadAll

    int     3
EXPORTEDNAME vmmGCTestTrap3_FaultEIP

    RestoreAll
    mov     eax, 0ffffffffh
    ret
ENDPROC vmmGCTestTrap3


;;
; A Trap 8 testcase.
GLOBALNAME vmmGCTestTrap8
    SaveAndLoadAll

    sub     esp, byte 8
    sidt    [esp]
    mov     word [esp], 111 ; make any #PF double fault.
    lidt    [esp]
    add     esp, byte 8

    COM_S_CHAR '!'

    xor     eax, eax
EXPORTEDNAME vmmGCTestTrap8_FaultEIP
    mov     eax, [eax]


    COM_S_CHAR '2'

    RestoreAll
    mov     eax, 0ffffffffh
    ret
ENDPROC vmmGCTestTrap8


;;
; A simple Trap 0d testcase.
GLOBALNAME vmmGCTestTrap0d
    SaveAndLoadAll

    push    ds
EXPORTEDNAME vmmGCTestTrap0d_FaultEIP
    ltr     [esp]
    pop     eax

    RestoreAll
    mov     eax, 0ffffffffh
    ret
ENDPROC vmmGCTestTrap0d


;;
; A simple Trap 0e testcase.
GLOBALNAME vmmGCTestTrap0e
    SaveAndLoadAll

    xor     eax, eax
EXPORTEDNAME vmmGCTestTrap0e_FaultEIP
    mov     eax, [eax]

    RestoreAll
    mov     eax, 0ffffffffh
    ret

EXPORTEDNAME vmmGCTestTrap0e_ResumeEIP
    RestoreAll
    xor     eax, eax
    ret
ENDPROC vmmGCTestTrap0e

