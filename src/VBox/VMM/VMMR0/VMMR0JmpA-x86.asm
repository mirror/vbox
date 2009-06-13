; $Id$
;; @file
; VMM - R0 SetJmp / LongJmp routines for X86.
;

;
; Copyright (C) 2006-2009 Sun Microsystems, Inc.
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


; For vmmR0LoggerWrapper. (The other architecture(s) use(s) C99 variadict macros.)
extern NAME(RTLogLogger)

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
    cmp     dword [esi], 0h
    jne     .entry_error
    mov     edx, esi
    mov     edi, esi
    mov     ecx, 2048
    mov     eax, 0eeeeeeeeh
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

 %ifdef VBOX_STRICT
    mov     esi, [ebx + VMMR0JMPBUF.pvSavedStack]
    cmp     [esi], 0eeeeeeeeh           ; Check for stack overflow
    jne     .stack_overflow
    cmp     [esi + 04h], 0eeeeeeeeh
    jne     .stack_overflow
    cmp     [esi + 08h], 0eeeeeeeeh
    jne     .stack_overflow
    cmp     [esi + 0ch], 0eeeeeeeeh
    jne     .stack_overflow
    cmp     [esi + 10h], 0eeeeeeeeh
    jne     .stack_overflow
    cmp     [esi + 20h], 0eeeeeeeeh
    jne     .stack_overflow
    cmp     [esi + 30h], 0eeeeeeeeh
    jne     .stack_overflow
    mov     dword [esi], 0h             ; Reset the marker
 %endif

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

.stack_overflow:
    mov     eax, VERR_INTERNAL_ERROR_5
    jmp     .proper_return

    ;
    ; Aborting resume.
    ;
.bad:
    and     dword [edx + VMMR0JMPBUF.eip], byte 0 ; used for valid check.
    mov     edi, [edx + VMMR0JMPBUF.edi]
    mov     esi, [edx + VMMR0JMPBUF.esi]
    mov     ebx, [edx + VMMR0JMPBUF.ebx]
    mov     eax, VERR_INTERNAL_ERROR_3    ; todo better return code!
    ret

    ;
    ; Resume VMMR0CallHost the call.
    ;
.resume:
    ; Sanity checks.
%ifdef VMM_R0_SWITCH_STACK
    mov     eax, [edx + VMMR0JMPBUF.pvSavedStack]
 %ifdef RT_STRICT
    cmp     dword [eax], 0eeeeeeeeh
 %endif
    lea     eax, [eax + 8192 - 32]
    cmp     dword [eax + 1ch], 0deadbeefh       ; Marker 1.
    jne     .bad
 %ifdef RT_STRICT
    cmp     [esi + 18h], edx                    ; The saved pJmpBuf pointer.
    jne     .bad
    cmp     dword [esi + 14h], 00c00ffeeh       ; Marker 2.
    jne     .bad
    cmp     dword [esi + 10h], 0f00dbeefh       ; Marker 3.
    jne     .bad
 %endif
%else  ; !VMM_R0_SWITCH_STACK
    cmp     ecx, [edx + VMMR0JMPBUF.SpCheck]
    jne     .bad
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
%endif

%ifdef VMM_R0_SWITCH_STACK
    ; Switch stack.
    mov     esp, [edx + VMMR0JMPBUF.SpResume]
%else
    ; Restore the stack.
    mov     ecx, [edx + VMMR0JMPBUF.cbSavedStack]
    shr     ecx, 2
    mov     esi, [edx + VMMR0JMPBUF.pvSavedStack]
    mov     edi, [edx + VMMR0JMPBUF.SpResume]
    mov     esp, edi
    rep movsd
%endif ; !VMM_R0_SWITCH_STACK
    mov     byte [edx + VMMR0JMPBUF.fInRing3Call], 0

    ; Continue where we left off.
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
ENDPROC vmmR0CallHostSetJmp


;;
; Worker for VMMR0CallHost.
; This will save the stack and registers.
;
; @param    pJmpBuf msc:rcx gcc:rdi x86:[ebp+8]     Pointer to the jump buffer.
; @param    rc      msc:rdx gcc:rsi x86:[ebp+c]     The return code.
;
BEGINPROC vmmR0CallHostLongJmp
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

    ; Save ESP & EBP to enable stack dumps
    mov     ecx, ebp
    mov     [edx + VMMR0JMPBUF.SavedEbp], ecx
    sub     ecx, 4
    mov     [edx + VMMR0JMPBUF.SavedEsp], ecx

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
ENDPROC vmmR0CallHostLongJmp


;;
; Internal R0 logger worker: Logger wrapper.
;
; @cproto VMMR0DECL(void) vmmR0LoggerWrapper(const char *pszFormat, ...)
;
EXPORTEDNAME vmmR0LoggerWrapper
    push    0                           ; assumes we're the wrapper for a default instance.
    call    NAME(RTLogLogger)
    add     esp, byte 4
    ret
ENDPROC vmmR0LoggerWrapper

