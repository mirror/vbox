; $Id$
;; @file
; VMM - R0 assembly routines.
;

;
; Copyright (C) 2006-2007 Sun Microsystems, Inc.
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VMMInternal.mac"
%include "iprt/err.mac"


%ifdef RT_ARCH_X86      ; The other architecture(s) use(s) C99 variadict macros.
extern NAME(RTLogLogger)
%endif

%ifdef RT_OS_DARWIN
 %define VMM_R0_SWITCH_STACK
%endif


BEGINCODE


;;
; The setjmp variant used for calling Ring-3.
;
; This differs from the normal setjmp in that it will resume VMMR0CallHost if we're
; in the middle of a ring-3 call. Another differences is the function pointer and
; argument. This has to do with resuming code and the stack frame of the caller.
;
; @returns  VINF_SUCCESS on success or whatever is passed to vmmR0CallHostLongJmp.
; @param    pJmpBuf msc:rcx gcc:rdi x86:[esp+0x04]     Our jmp_buf.
; @param    pfn     msc:rdx gcc:rsi x86:[esp+0x08]     The function to be called when not resuming.
; @param    pvUser1 msc:r8  gcc:rdx x86:[esp+0x0c]     The argument of that function.
; @param    pvUser2 msc:r9  gcc:rcx x86:[esp+0x10]     The argument of that function.
;
BEGINPROC vmmR0CallHostSetJmp
GLOBALNAME vmmR0CallHostSetJmpEx
%ifdef RT_ARCH_X86
    ;
    ; Save the registers.
    ;
    mov     edx, [esp + 4h]             ; pJmpBuf
    mov     [edx + VMMR0JMPBUF.ebx], ebx
    mov     [edx + VMMR0JMPBUF.esi], esi
    mov     [edx + VMMR0JMPBUF.edi], edi
    mov     [edx + VMMR0JMPBUF.ebp], ebp
    mov     eax, [esp]
    mov     [edx + VMMR0JMPBUF.eip], eax
    lea     ecx, [esp + 4]              ; (used in resume)
    mov     [edx + VMMR0JMPBUF.esp], ecx

    ;
    ; If we're not in a ring-3 call, call pfn and return.
    ;
    test    byte [edx + VMMR0JMPBUF.fInRing3Call], 1
    jnz     .resume

    mov     ebx, edx                    ; pJmpBuf -> ebx (persistent reg)
%ifdef VMM_R0_SWITCH_STACK
    mov     esi, [ebx + VMMR0JMPBUF.pvSavedStack]
    test    esi, esi
    jz      .entry_error
 %ifdef VBOX_STRICT
    mov      edx, esi
    mov      edi, esi
    mov      ecx, 2048
    mov      eax, 0eeeeeeeeh
    repne stosd
 %endif
    lea     esi, [esi + 8192 - 32]
    mov     [esi + 1ch], dword 0deadbeefh ; Marker 1.
    mov     [esi + 18h], ebx            ; Save pJmpBuf pointer.
    mov     [esi + 14h], dword 00c00ffeeh ; Marker 2.
    mov     [esi + 10h], dword 0f00dbeefh ; Marker 3.
    mov     edx, [esp + 10h]            ; pvArg2
    mov     [esi + 04h], edx
    mov     ecx, [esp + 0ch]            ; pvArg1
    mov     [esi      ], ecx
    mov     eax, [esp + 08h]            ; pfn
    mov     esp, esi                    ; Switch stack!
    call    eax
    and     dword [esi + 1ch], byte 0   ; clear marker.

%else  ; !VMM_R0_SWITCH_STACK
    mov     ecx, [esp + 0ch]            ; pvArg1
    mov     edx, [esp + 10h]            ; pvArg2
    mov     eax, [esp + 08h]            ; pfn
    sub     esp, 12                     ; align the stack on a 16-byte boundrary.
    mov     [esp      ], ecx
    mov     [esp + 04h], edx
    call    eax
%endif ; !VMM_R0_SWITCH_STACK
    mov     edx, ebx                    ; pJmpBuf -> edx (volatile reg)

    ;
    ; Return like in the long jump but clear eip, no short cuts here.
    ;
.proper_return:
    mov     ebx, [edx + VMMR0JMPBUF.ebx]
    mov     esi, [edx + VMMR0JMPBUF.esi]
    mov     edi, [edx + VMMR0JMPBUF.edi]
    mov     ebp, [edx + VMMR0JMPBUF.ebp]
    mov     ecx, [edx + VMMR0JMPBUF.eip]
    and     dword [edx + VMMR0JMPBUF.eip], byte 0 ; used for valid check.
    mov     esp, [edx + VMMR0JMPBUF.esp]
    jmp     ecx

.entry_error:
    mov     eax, VERR_INTERNAL_ERROR_2
    jmp     .proper_return

    ;
    ; Resume VMMR0CallHost the call.
    ;
.resume:
%ifdef VMM_R0_SWITCH_STACK
    ; Switch stack.
    mov     esp, [edx + VMMR0JMPBUF.SpResume]
%else  ; !VMM_R0_SWITCH_STACK
    ; Sanity checks.
    cmp     ecx, [edx + VMMR0JMPBUF.SpCheck]
    je      .espCheck_ok
.bad:
    and     dword [edx + VMMR0JMPBUF.eip], byte 0 ; used for valid check.
    mov     edi, [edx + VMMR0JMPBUF.edi]
    mov     esi, [edx + VMMR0JMPBUF.esi]
    mov     ebx, [edx + VMMR0JMPBUF.ebx]
    mov     eax, VERR_INTERNAL_ERROR_3    ; todo better return code!
    ret

.espCheck_ok:
    mov     ecx, [edx + VMMR0JMPBUF.cbSavedStack]
    cmp     ecx, 8192
    ja      .bad
    test    ecx, 3
    jnz     .bad
    mov     edi, [edx + VMMR0JMPBUF.esp]
    sub     edi, [edx + VMMR0JMPBUF.SpResume]
    cmp     ecx, edi
    jne     .bad

    ;
    ; Restore the stack.
    ;
    mov     ecx, [edx + VMMR0JMPBUF.cbSavedStack]
    shr     ecx, 2
    mov     esi, [edx + VMMR0JMPBUF.pvSavedStack]
    mov     edi, [edx + VMMR0JMPBUF.SpResume]
    mov     esp, edi
    rep movsd
%endif ; !VMM_R0_SWITCH_STACK
    mov     byte [edx + VMMR0JMPBUF.fInRing3Call], 0

    ;
    ; Continue where we left off.
    ;
%ifdef VBOX_STRICT
    pop     eax                         ; magic
    cmp     eax, 0f00dbed0h
    je      .magic_ok
    mov     ecx, 0123h
    mov     [ecx], edx
.magic_ok:
%endif
    popf
    pop     ebx
    pop     esi
    pop     edi
    pop     ebp
    xor     eax, eax                    ; VINF_SUCCESS
    ret
%endif ; RT_ARCH_X86

%ifdef RT_ARCH_AMD64
    ;
    ; Save the registers.
    ;
    push    rbp
    mov     rbp, rsp
 %ifdef ASM_CALL64_MSC
    sub     rsp, 30h
    mov     r11, rdx                    ; pfn
    mov     rdx, rcx                    ; pJmpBuf;
 %else
    sub     rsp, 10h
    mov     r8, rdx                     ; pvUser1 (save it like MSC)
    mov     r9, rcx                     ; pvUser2 (save it like MSC)
    mov     r11, rsi                    ; pfn
    mov     rdx, rdi                    ; pJmpBuf
 %endif
    mov     [rdx + VMMR0JMPBUF.rbx], rbx
 %ifdef ASM_CALL64_MSC
    mov     [rdx + VMMR0JMPBUF.rsi], rsi
    mov     [rdx + VMMR0JMPBUF.rdi], rdi
 %endif
    mov     r10, [rbp]
    mov     [rdx + VMMR0JMPBUF.rbp], r10
    mov     [rdx + VMMR0JMPBUF.r12], r12
    mov     [rdx + VMMR0JMPBUF.r13], r13
    mov     [rdx + VMMR0JMPBUF.r14], r14
    mov     [rdx + VMMR0JMPBUF.r15], r15
    mov     rax, [rbp + 8]
    mov     [rdx + VMMR0JMPBUF.rip], rax
    lea     r10, [rbp + 10h]            ; (used in resume)
    mov     [rdx + VMMR0JMPBUF.rsp], r10

    ;
    ; If we're not in a ring-3 call, call pfn and return.
    ;
    test    byte [rdx + VMMR0JMPBUF.fInRing3Call], 1
    jnz     .resume

 %ifdef VMM_R0_SWITCH_STACK
    mov     r15, [rdx + VMMR0JMPBUF.pvSavedStack]
    test    r15, r15
    jz      .entry_error
  %ifdef VBOX_STRICT
    mov     rdi, r15
    mov     rcx, 1024
    mov     rax, 00eeeeeeeffeeeeeeeh
    repne stosq
    mov     [rdi - 10h], rbx
  %endif
    lea     r15, [r15 + 8192 - 40h]
    mov     rsp, r15                    ; Switch stack!
 %endif ; VMM_R0_SWITCH_STACK

    mov     r12, rdx                    ; Save pJmpBuf.
 %ifdef ASM_CALL64_MSC
    mov     rcx, r8                     ; pvUser -> arg0
    mov     rdx, r9
 %else
    mov     rdi, r8                     ; pvUser -> arg0
    mov     rsi, r9
 %endif
    call    r11
    mov     rdx, r12                    ; Restore pJmpBuf

    ;
    ; Return like in the long jump but clear eip, no short cuts here.
    ;
.proper_return:
    mov     rbx, [rdx + VMMR0JMPBUF.rbx]
 %ifdef ASM_CALL64_MSC
    mov     rsi, [rdx + VMMR0JMPBUF.rsi]
    mov     rdi, [rdx + VMMR0JMPBUF.rdi]
 %endif
    mov     r12, [rdx + VMMR0JMPBUF.r12]
    mov     r13, [rdx + VMMR0JMPBUF.r13]
    mov     r14, [rdx + VMMR0JMPBUF.r14]
    mov     r15, [rdx + VMMR0JMPBUF.r15]
    mov     rbp, [rdx + VMMR0JMPBUF.rbp]
    mov     rcx, [rdx + VMMR0JMPBUF.rip]
    and     qword [rdx + VMMR0JMPBUF.rip], byte 0 ; used for valid check.
    mov     rsp, [rdx + VMMR0JMPBUF.rsp]
    jmp     rcx

.entry_error:
    mov     eax, VERR_INTERNAL_ERROR_2
    jmp     .proper_return

    ;
    ; Resume VMMR0CallHost the call.
    ;
.resume:
 %ifdef VMM_R0_SWITCH_STACK
    ; Switch stack.
    mov     rsp, [rdx + VMMR0JMPBUF.SpResume]
 %else  ; !VMM_R0_SWITCH_STACK
    ; Sanity checks.
    cmp     r10, [rdx + VMMR0JMPBUF.SpCheck]
    je      .rspCheck_ok
.bad:
    and     qword [rdx + VMMR0JMPBUF.rip], byte 0 ; used for valid check.
    mov     rbx, [rdx + VMMR0JMPBUF.rbx]
  %ifdef ASM_CALL64_MSC
    mov     rsi, [rdx + VMMR0JMPBUF.rsi]
    mov     rdi, [rdx + VMMR0JMPBUF.rdi]
  %endif
    mov     r12, [rdx + VMMR0JMPBUF.r12]
    mov     r13, [rdx + VMMR0JMPBUF.r13]
    mov     r14, [rdx + VMMR0JMPBUF.r14]
    mov     r15, [rdx + VMMR0JMPBUF.r15]
    mov     eax, VERR_INTERNAL_ERROR_2
    leave
    ret

.rspCheck_ok:
    mov     ecx, [rdx + VMMR0JMPBUF.cbSavedStack]
    cmp     rcx, 8192
    ja      .bad
    test    rcx, 3
    jnz     .bad
    mov     rdi, [rdx + VMMR0JMPBUF.rsp]
    sub     rdi, [rdx + VMMR0JMPBUF.SpResume]
    cmp     rcx, rdi
    jne     .bad

    ;
    ; Restore the stack.
    ;
    mov     ecx, [rdx + VMMR0JMPBUF.cbSavedStack]
    shr     ecx, 3
    mov     rsi, [rdx + VMMR0JMPBUF.pvSavedStack]
    mov     rdi, [rdx + VMMR0JMPBUF.SpResume]
    mov     rsp, rdi
    rep movsq
 %endif ; !VMM_R0_SWITCH_STACK
    mov     byte [rdx + VMMR0JMPBUF.fInRing3Call], 0

    ;
    ; Continue where we left off.
    ;
    popf
    pop     rbx
 %ifdef ASM_CALL64_MSC
    pop     rsi
    pop     rdi
 %endif
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    pop     rbp
    xor     eax, eax                    ; VINF_SUCCESS
    ret
%endif
ENDPROC vmmR0CallHostSetJmp


;;
; Worker for VMMR0CallHost.
; This will save the stack and registers.
;
; @param    pJmpBuf msc:rcx gcc:rdi x86:[ebp+8]     Pointer to the jump buffer.
; @param    rc      msc:rdx gcc:rsi x86:[ebp+c]     The return code.
;
BEGINPROC vmmR0CallHostLongJmp
%ifdef RT_ARCH_X86
    ;
    ; Save the registers on the stack.
    ;
    push    ebp
    mov     ebp, esp
    push    edi
    push    esi
    push    ebx
    pushf
%ifdef VBOX_STRICT
    push    dword 0f00dbed0h
%endif

    ;
    ; Load parameters.
    ;
    mov     edx, [ebp + 08h]            ; pJmpBuf
    mov     eax, [ebp + 0ch]            ; rc

    ;
    ; Is the jump buffer armed?
    ;
    cmp     dword [edx + VMMR0JMPBUF.eip], byte 0
    je      .nok

    ;
    ; Sanity checks.
    ;
    mov     edi, [edx + VMMR0JMPBUF.pvSavedStack]
    test    edi, edi                    ; darwin may set this to 0.
    jz      .nok
    mov     [edx + VMMR0JMPBUF.SpResume], esp
%ifndef VMM_R0_SWITCH_STACK
    mov     esi, esp
    mov     ecx, [edx + VMMR0JMPBUF.esp]
    sub     ecx, esi

    ; two sanity checks on the size.
    cmp     ecx, 8192                   ; check max size.
    jnbe    .nok

    ;
    ; Copy the stack.
    ;
    test    ecx, 3                      ; check alignment
    jnz     .nok
    mov     [edx + VMMR0JMPBUF.cbSavedStack], ecx
    shr     ecx, 2
    rep movsd
%endif ; !VMM_R0_SWITCH_STACK

    ; store the last pieces of info.
    mov     ecx, [edx + VMMR0JMPBUF.esp]
    mov     [edx + VMMR0JMPBUF.SpCheck], ecx
    mov     byte [edx + VMMR0JMPBUF.fInRing3Call], 1

    ;
    ; Do the long jump.
    ;
    mov     ebx, [edx + VMMR0JMPBUF.ebx]
    mov     esi, [edx + VMMR0JMPBUF.esi]
    mov     edi, [edx + VMMR0JMPBUF.edi]
    mov     ebp, [edx + VMMR0JMPBUF.ebp]
    mov     ecx, [edx + VMMR0JMPBUF.eip]
    mov     esp, [edx + VMMR0JMPBUF.esp]
    jmp     ecx

    ;
    ; Failure
    ;
.nok:
%ifdef VBOX_STRICT
    pop     eax                         ; magic
    cmp     eax, 0f00dbed0h
    je      .magic_ok
    mov     ecx, 0123h
    mov     [ecx], edx
.magic_ok:
%endif
    popf
    pop     ebx
    pop     esi
    pop     edi
    mov     eax, VERR_INTERNAL_ERROR_4
    leave
    ret
%endif ; RT_ARCH_X86

%ifdef RT_ARCH_AMD64
    ;
    ; Save the registers on the stack.
    ;
    push    rbp
    mov     rbp, rsp
    push    r15
    push    r14
    push    r13
    push    r12
 %ifdef ASM_CALL64_MSC
    push    rdi
    push    rsi
 %endif
    push    rbx
    pushf

    ;
    ; Normalize the parameters.
    ;
 %ifdef ASM_CALL64_MSC
    mov     eax, edx                    ; rc
    mov     rdx, rcx                    ; pJmpBuf
 %else
    mov     rdx, rdi                    ; pJmpBuf
    mov     eax, esi                    ; rc
 %endif

    ;
    ; Is the jump buffer armed?
    ;
    cmp     qword [rdx + VMMR0JMPBUF.rip], byte 0
    je      .nok

    ;
    ; Sanity checks.
    ;
    mov     rdi, [rdx + VMMR0JMPBUF.pvSavedStack]
    test    rdi, rdi                    ; darwin may set this to 0.
    jz      .nok
    mov     [rdx + VMMR0JMPBUF.SpResume], rsp
 %ifndef VMM_R0_SWITCH_STACK
    mov     rsi, rsp
    mov     rcx, [rdx + VMMR0JMPBUF.rsp]
    sub     rcx, rsi

    ; two sanity checks on the size.
    cmp     rcx, 8192                   ; check max size.
    jnbe    .nok

    ;
    ; Copy the stack
    ;
    test    ecx, 7                      ; check alignment
    jnz     .nok
    mov     [rdx + VMMR0JMPBUF.cbSavedStack], ecx
    shr     ecx, 3
    rep movsq

 %endif ; !VMM_R0_SWITCH_STACK

    ; store the last pieces of info.
    mov     rcx, [rdx + VMMR0JMPBUF.rsp]
    mov     [rdx + VMMR0JMPBUF.SpCheck], rcx
    mov     byte [rdx + VMMR0JMPBUF.fInRing3Call], 1

    ;
    ; Do the long jump.
    ;
    mov     rbx, [rdx + VMMR0JMPBUF.rbx]
 %ifdef ASM_CALL64_MSC
    mov     rsi, [rdx + VMMR0JMPBUF.rsi]
    mov     rdi, [rdx + VMMR0JMPBUF.rdi]
 %endif
    mov     r12, [rdx + VMMR0JMPBUF.r12]
    mov     r13, [rdx + VMMR0JMPBUF.r13]
    mov     r14, [rdx + VMMR0JMPBUF.r14]
    mov     r15, [rdx + VMMR0JMPBUF.r15]
    mov     rbp, [rdx + VMMR0JMPBUF.rbp]
    mov     rcx, [rdx + VMMR0JMPBUF.rip]
    mov     rsp, [rdx + VMMR0JMPBUF.rsp]
    jmp     rcx

    ;
    ; Failure
    ;
.nok:
    mov     eax, VERR_INTERNAL_ERROR_4
    popf
    pop     rbx
 %ifdef ASM_CALL64_MSC
    pop     rsi
    pop     rdi
 %endif
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    leave
    ret

%endif
ENDPROC vmmR0CallHostLongJmp


;;
; Internal R0 logger worker: Logger wrapper.
;
; @cproto VMMR0DECL(void) vmmR0LoggerWrapper(const char *pszFormat, ...)
;
EXPORTEDNAME vmmR0LoggerWrapper
%ifdef RT_ARCH_X86      ; The other architecture(s) use(s) C99 variadict macros.
    push    0                           ; assumes we're the wrapper for a default instance.
    call    NAME(RTLogLogger)
    add     esp, byte 4
    ret
%else
    int3
    int3
    int3
    ret
%endif
ENDPROC vmmR0LoggerWrapper

