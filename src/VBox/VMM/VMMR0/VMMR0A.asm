; $Id$
;; @file
; VMM - R0 assembly routines.
;

;
; Copyright (C) 2006 InnoTek Systemberatung GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;
; If you received this file as part of a commercial VirtualBox
; distribution, then only the terms of your commercial VirtualBox
; license agreement apply instead of the previous paragraph.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VMMInternal.mac"
%include "iprt/err.mac"


%ifdef __X86__      ; The other architecture(s) use(s) C99 variadict macros.
extern IMPNAME(RTLogLogger)
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
; @param    pJmpBuf     Our jmp_buf.
; @param    pfn         The function to be called when not resuming.
; @param    pVM         The argument of that function.
;
BEGINPROC vmmR0CallHostSetJmp
%ifdef __X86__
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

    mov     ecx, [esp + 0ch]            ; pvArg
    mov     eax, [esp + 08h]            ; pfn
    push    ecx
    call    eax
    add     esp, 4
    mov     edx, [esp + 4h]             ; pJmpBuf
    and     dword [edx + VMMR0JMPBUF.eip], byte 0 ; used for valid check.
    ret

    ;
    ; Resume VMMR0CallHost the call.
    ;
.resume:
    ; Sanity checks.
    cmp     ecx, [edx + VMMR0JMPBUF.SpCheck]
    je      .espCheck_ok
.bad:
    and     dword [edx + VMMR0JMPBUF.eip], byte 0 ; used for valid check.
    mov     edi, [edx + VMMR0JMPBUF.edi]
    mov     esi, [edx + VMMR0JMPBUF.esi]
    mov     ebx, [edx + VMMR0JMPBUF.ebx]
    mov     eax, VERR_INTERNAL_ERROR    ; todo better return code!
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
    mov     byte [edx + VMMR0JMPBUF.fInRing3Call], 0
    mov     ecx, [edx + VMMR0JMPBUF.cbSavedStack]
    shr     ecx, 2
    mov     esi, [edx + VMMR0JMPBUF.pvSavedStack]
    mov     edi, [edx + VMMR0JMPBUF.SpResume]
    mov     esp, edi
    rep movsd

    ;
    ; Continue where we left off.
    ;
    popf
    pop     ebx
    pop     esi
    pop     edi
    pop     ebp
    xor     eax, eax                    ; VINF_SUCCESS
    ret
%endif ; __X86__

%ifdef __AMD64__
    int3    ; implement me!
%endif
ENDPROC vmmR0CallHostSetJmp


;;
; Worker for VMMR0CallHost.
; This will save the stack and registers.
;
; @param    pJmpBuf         Pointer to the jump buffer.
; @param    rc              The return code.
;
BEGINPROC vmmR0CallHostLongJmp
%ifdef __X86__
    ;
    ; Save the registers on the stack.
    ;
    push    ebp
    mov     ebp, esp
    push    edi
    push    esi
    push    ebx
    pushf

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
    ; Save the stack.
    ;
    mov     edi, [edx + VMMR0JMPBUF.pvSavedStack]
    mov     [edx + VMMR0JMPBUF.SpResume], esp
    mov     esi, esp
    mov     ecx, [edx + VMMR0JMPBUF.esp]
    sub     ecx, esi

    ; two sanity checks on the size.
    cmp     ecx, 8192                   ; check max size.
    jbe     .ok
.nok:
    mov     eax, VERR_INTERNAL_ERROR
    popf
    pop     ebx
    pop     esi
    pop     edi
    leave
    ret
.ok:
    test    ecx, 3                      ; check alignment
    jnz     .nok
    mov     [edx + VMMR0JMPBUF.cbSavedStack], ecx
    shr     ecx, 2
    rep movsd

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
%endif ; __X86__

%ifdef __AMD64__
    int3    ; implement me!
%endif
ENDPROC vmmR0CallHostLongJmp


;;
; Internal R0 logger worker: Logger wrapper.
;
; @cproto VMMR0DECL(void) vmmR0LoggerWrapper(const char *pszFormat, ...)
;
EXPORTEDNAME vmmR0LoggerWrapper
%ifdef __X86__      ; The other architecture(s) use(s) C99 variadict macros.
    push    0                           ; assumes we're the wrapper for a default instance.
    call    IMP(RTLogLogger)
    add     esp, byte 4
    ret
%else
    int3
    int3
    int3
    ret
%endif
ENDPROC vmmR0LoggerWrapper

