; $Id$
;; @file
; EM Assembly Routines.
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
%include "VBox/err.mac"
%include "VBox/x86.mac"

;; @def MY_PTR_REG
; The register we use for value pointers (And,Or,Dec,Inc).
%ifdef RT_ARCH_AMD64
%define MY_PTR_REG  rcx
%else
%define MY_PTR_REG  ecx
%endif

;; @def MY_RET_REG
; The register we return the result in.
%ifdef RT_ARCH_AMD64
%define MY_RET_REG  rax
%else
%define MY_RET_REG  eax
%endif

BEGINCODE


;;
; Emulate CMP instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateCmp(uint32_t u32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]  rdi  rcx       Param 1 - First parameter (Dst).
; @param    [esp + 08h]  rsi  edx       Param 2 - Second parameter (Src).
; @param    [esp + 0ch]  rdx  r8        Param 3 - Size of parameters, only 1/2/4 is valid.
;
align 16
BEGINPROC   EMEmulateCmp
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    cmp     rcx, rdx                    ; do 8 bytes CMP
    jmp short .done
%endif

.do_dword:
    cmp     ecx, edx                    ; do 4 bytes CMP
    jmp short .done

.do_word:
    cmp     cx, dx                      ; do 2 bytes CMP
    jmp short .done

.do_byte:
    cmp     cl, dl                      ; do 1 byte CMP

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateCmp


;;
; Emulate AND instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateAnd(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateAnd
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    and     [MY_PTR_REG], rdx           ; do 8 bytes AND
    jmp short .done
%endif

.do_dword:
    and     [MY_PTR_REG], edx           ; do 4 bytes AND
    jmp short .done

.do_word:
    and     [MY_PTR_REG], dx            ; do 2 bytes AND
    jmp short .done

.do_byte:
    and     [MY_PTR_REG], dl            ; do 1 byte AND

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateAnd


;;
; Emulate OR instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateOr(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateOr
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    or      [MY_PTR_REG], rdx           ; do 8 bytes OR
    jmp short .done
%endif

.do_dword:
    or      [MY_PTR_REG], edx           ; do 4 bytes OR
    jmp short .done

.do_word:
    or      [MY_PTR_REG], dx            ; do 2 bytes OR
    jmp short .done

.do_byte:
    or      [MY_PTR_REG], dl            ; do 1 byte OR

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateOr

;;
; Emulate LOCK OR instruction.
; EMDECL(int) EMEmulateLockOr(RTGCPTR GCPtrParam1, RTGCUINTREG Param2, size_t cbSize, RTGCUINTREG *pf);
;
; @returns VINF_SUCCESS on success, VERR_ACCESS_DENIED on \#PF (GC only).
; @param    [esp + 04h]  gcc:rdi  msc:rcx   Param 1 - First parameter - pointer to data item (the real stuff).
; @param    [esp + 08h]  gcc:rsi  msc:rdx   Param 2 - Second parameter- the immediate / register value.
; @param    [esp + 0ch]  gcc:rdx  msc:r8    Param 3 - Size of the operation - 1, 2, 4 or 8 bytes.
; @param    [esp + 10h]  gcc:rcx  msc:r9    Param 4 - Where to store the eflags on success.
;                                                     only arithmetic flags are valid.
align 16
BEGINPROC   EMEmulateLockOr
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter (MY_PTR_REG)
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    lock or [MY_PTR_REG], rdx           ; do 8 bytes OR
    jmp short .done
%endif

.do_dword:
    lock or [MY_PTR_REG], edx           ; do 4 bytes OR
    jmp short .done

.do_word:
    lock or [MY_PTR_REG], dx            ; do 2 bytes OR
    jmp short .done

.do_byte:
    lock or [MY_PTR_REG], dl            ; do 1 byte OR

    ; collect flags and return.
.done:
    pushf
%ifdef RT_ARCH_AMD64
    pop    rax
 %ifdef RT_OS_WINDOWS
    mov    [r9], eax
 %else  ; !RT_OS_WINDOWS
    mov    [rcx], eax
 %endif ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 10h + 4]
    pop     dword [eax]
%endif
    mov     eax, VINF_SUCCESS     
    retn

%ifdef IN_GC
; #PF resume point. 
GLOBALNAME EMEmulateLockOr_Error
    mov     eax, VERR_ACCESS_DENIED
    ret
%endif

ENDPROC     EMEmulateLockOr

;;
; Emulate XOR instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateXor(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateXor
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    xor     [MY_PTR_REG], rdx           ; do 8 bytes XOR
    jmp short .done
%endif

.do_dword:
    xor     [MY_PTR_REG], edx           ; do 4 bytes XOR
    jmp short .done

.do_word:
    xor     [MY_PTR_REG], dx            ; do 2 bytes XOR
    jmp short .done

.do_byte:
    xor     [MY_PTR_REG], dl            ; do 1 byte XOR

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateXor

;;
; Emulate INC instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateInc(uint32_t *pu32Param1, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags are valid.
; @param    [esp + 04h]  rdi  rcx   Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]  rsi  rdx   Param 2 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateInc
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, rdx                    ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rsi                    ; eax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 08h]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    inc     qword [MY_PTR_REG]          ; do 8 bytes INC
    jmp short .done
%endif

.do_dword:
    inc     dword [MY_PTR_REG]          ; do 4 bytes INC
    jmp short .done

.do_word:
    inc     word [MY_PTR_REG]           ; do 2 bytes INC
    jmp short .done

.do_byte:
    inc     byte [MY_PTR_REG]           ; do 1 byte INC
    jmp short .done

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateInc


;;
; Emulate DEC instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateDec(uint32_t *pu32Param1, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags are valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateDec
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, rdx                    ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rsi                    ; eax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 08h]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    dec     qword [MY_PTR_REG]          ; do 8 bytes DEC
    jmp short .done
%endif

.do_dword:
    dec     dword [MY_PTR_REG]          ; do 4 bytes DEC
    jmp short .done

.do_word:
    dec     word [MY_PTR_REG]           ; do 2 bytes DEC
    jmp short .done

.do_byte:
    dec     byte [MY_PTR_REG]           ; do 1 byte DEC
    jmp short .done

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateDec

;;
; Emulate ADD instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateAdd(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateAdd
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    add     [MY_PTR_REG], rdx           ; do 8 bytes ADD
    jmp short .done
%endif

.do_dword:
    add     [MY_PTR_REG], edx           ; do 4 bytes ADD
    jmp short .done

.do_word:
    add     [MY_PTR_REG], dx            ; do 2 bytes ADD
    jmp short .done

.do_byte:
    add     [MY_PTR_REG], dl            ; do 1 byte ADD

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateAdd

;;
; Emulate ADC instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateAdcWithCarrySet(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateAdcWithCarrySet
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    stc     ; set carry flag
    adc     [MY_PTR_REG], rdx           ; do 8 bytes ADC
    jmp short .done
%endif

.do_dword:
    stc     ; set carry flag
    adc     [MY_PTR_REG], edx           ; do 4 bytes ADC
    jmp short .done

.do_word:
    stc     ; set carry flag
    adc     [MY_PTR_REG], dx            ; do 2 bytes ADC
    jmp short .done

.do_byte:
    stc     ; set carry flag
    adc     [MY_PTR_REG], dl            ; do 1 byte ADC

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateAdcWithCarrySet

;;
; Emulate SUB instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateSub(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateSub
%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; eax = size of parameters
%else   ; !RT_OS_WINDOWS
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef RT_ARCH_AMD64
    cmp     al, 8
    je short .do_qword                  ; 8 bytes variant
%endif
    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

    ; workers
%ifdef RT_ARCH_AMD64
.do_qword:
    sub     [MY_PTR_REG], rdx           ; do 8 bytes SUB
    jmp short .done
%endif

.do_dword:
    sub     [MY_PTR_REG], edx           ; do 4 bytes SUB
    jmp short .done

.do_word:
    sub     [MY_PTR_REG], dx            ; do 2 bytes SUB
    jmp short .done

.do_byte:
    sub     [MY_PTR_REG], dl            ; do 1 byte SUB

    ; collect flags and return.
.done:
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateSub


;;
; Emulate BTR instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateBtr(uint32_t *pu32Param1, uint32_t u32Param2);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateBtr
%ifdef RT_ARCH_AMD64
%ifndef RT_OS_WINDOWS
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    and     edx, 7
    btr    [MY_PTR_REG], edx

    ; collect flags and return.
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateBtr

;;
; Emulate LOCK BTR instruction.
; EMDECL(int) EMEmulateLockBtr(RTGCPTR GCPtrParam1, RTGCUINTREG Param2, uint32_t *pf);
;
; @returns VINF_SUCCESS on success, VERR_ACCESS_DENIED on \#PF (GC only).
; @param    [esp + 04h]  gcc:rdi  msc:rcx   Param 1 - First parameter - pointer to data item (the real stuff).
; @param    [esp + 08h]  gcc:rsi  msc:rdx   Param 2 - Second parameter- the immediate / register value. (really an 8 byte value)
; @param    [esp + 0ch]  gcc:rdx  msc:r8    Param 3 - Where to store the eflags on success.
;
align 16
BEGINPROC   EMEmulateLockBtr
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     rax, r8                     ; rax = third parameter
 %else  ; !RT_OS_WINDOWS
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rax, rdx                    ; rax = third parameter
    mov     rdx, rsi                    ; rdx = second parameter
 %endif ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
    mov     eax, [esp + 0ch]            ; eax = third parameter
%endif

    lock btr [MY_PTR_REG], edx

    ; collect flags and return.
    pushf
    pop     xDX
    mov     [xAX], edx
    mov     eax, VINF_SUCCESS
    retn

%ifdef IN_GC
; #PF resume point. 
GLOBALNAME EMEmulateLockBtr_Error
    mov     eax, VERR_ACCESS_DENIED
    ret
%endif

ENDPROC     EMEmulateLockBtr

;;
; Emulate BTC instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateBtc(uint32_t *pu32Param1, uint32_t u32Param2);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateBtc
%ifdef RT_ARCH_AMD64
%ifndef RT_OS_WINDOWS
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    and     edx, 7
    btc    [MY_PTR_REG], edx

    ; collect flags and return.
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateBtc

;;
; Emulate BTS instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateBts(uint32_t *pu32Param1, uint32_t u32Param2);
;
; @returns EFLAGS after the operation, only arithmetic flags are valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateBts
%ifdef RT_ARCH_AMD64
%ifndef RT_OS_WINDOWS
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !RT_OS_WINDOWS
%else   ; !RT_ARCH_AMD64
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    and     edx, 7
    bts    [MY_PTR_REG], edx

    ; collect flags and return.
    pushf
    pop     MY_RET_REG
    retn
ENDPROC     EMEmulateBts


%if 0
;;
; Emulate LOCK CMPXCHG instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateLockCmpXchg32(RTHCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]  gcc:rdi  msc:rcx   Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]  gcc:rsi  msc:rdx   Param 2 - pointer to second parameter (eax)
; @param    [esp + 0ch]  gcc:rdx  msc:r8    Param 3 - third parameter
; @param    [esp + 10h]  gcc:rcx  msc:r9    Param 4 - Size of parameters, only 1/2/4 is valid
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateLockCmpXchg32
    push    ebx
    mov     ecx, [esp + 04h + 4]        ; ecx = first parameter
    mov     ebx, [esp + 08h + 4]        ; ebx = 2nd parameter (eax)
    mov     edx, [esp + 0ch + 4]        ; edx = third parameter
    mov     eax, [esp + 10h + 4]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg dword [ecx], edx            ; do 4 bytes CMPXCHG
    mov     dword [ebx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg word [ecx], dx              ; do 2 bytes CMPXCHG
    mov     word [ebx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg byte [ecx], dl              ; do 1 bytes CMPXCHG
    mov     byte [ebx], al

.done:
    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 14h + 4]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMEmulateLockCmpXchg32_Error
    pop     ebx
    mov     eax, VERR_ACCESS_DENIED
    ret

ENDPROC     EMEmulateLockCmpXchg32

;;
; Emulate CMPXCHG instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateCmpXchg32(RTHCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]  gcc:rdi  msc:rcx   Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]  gcc:rsi  msc:rdx   Param 2 - pointer to second parameter (eax)
; @param    [esp + 0ch]  gcc:rdx  msc:r8    Param 3 - third parameter
; @param    [esp + 10h]  gcc:rcx  msc:r9    Param 4 - Size of parameters, only 1/2/4 is valid.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateCmpXchg32
    push    ebx
    mov     ecx, [esp + 04h + 4]        ; ecx = first parameter
    mov     ebx, [esp + 08h + 4]        ; ebx = 2nd parameter (eax)
    mov     edx, [esp + 0ch + 4]        ; edx = third parameter
    mov     eax, [esp + 10h + 4]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg dword [ecx], edx            ; do 4 bytes CMPXCHG
    mov     dword [ebx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg word [ecx], dx              ; do 2 bytes CMPXCHG
    mov     word [ebx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg byte [ecx], dl              ; do 1 bytes CMPXCHG
    mov     byte [ebx], al

.done:
    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 14h + 4]        ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMEmulateCmpXchg32_Error
    pop     ebx
    mov     eax, VERR_ACCESS_DENIED
    ret
ENDPROC     EMEmulateCmpXchg32

;;
; Emulate LOCK CMPXCHG8B instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateLockCmpXchg8b(RTHCPTR pu32Param1, uint32_t *pEAX, uint32_t *pEDX, uint32_t uEBX, uint32_t uECX);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Address of the eax register
; @param    [esp + 0ch]    Param 3 - Address of the edx register
; @param    [esp + 10h]    Param 4 - EBX
; @param    [esp + 14h]    Param 5 - ECX
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateLockCmpXchg8b32
    push    ebp
    push    ebx
    mov     ebp, [esp + 04h + 8]        ; ebp = first parameter
    mov     eax, [esp + 08h + 8]        ; &EAX
    mov     eax, dword [eax]
    mov     edx, [esp + 0ch + 8]        ; &EDX
    mov     edx, dword [edx]
    mov     ebx, [esp + 10h + 8]        ; EBX
    mov     ecx, [esp + 14h + 8]        ; ECX

    lock cmpxchg8b qword [ebp]          ; do CMPXCHG8B
    mov     dword [esp + 08h + 8], eax
    mov     dword [esp + 0ch + 8], edx

    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 18h + 8]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    pop     ebp
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMEmulateLockCmpXchg8b32_Error
    pop     ebx
    pop     ebp
    mov     eax, VERR_ACCESS_DENIED
    ret

ENDPROC     EMEmulateLockCmpXchg8b32

;;
; Emulate CMPXCHG8B instruction, CDECL calling conv.
; EMDECL(uint32_t) EMEmulateCmpXchg8b32(RTHCPTR pu32Param1, uint32_t *pEAX, uint32_t *pEDX, uint32_t uEBX, uint32_t uECX);
;
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Address of the eax register
; @param    [esp + 0ch]    Param 3 - Address of the edx register
; @param    [esp + 10h]    Param 4 - EBX
; @param    [esp + 14h]    Param 5 - ECX
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateCmpXchg8b32
    push    ebp
    push    ebx
    mov     ebp, [esp + 04h + 8]        ; ebp = first parameter
    mov     eax, [esp + 08h + 8]        ; &EAX
    mov     eax, dword [eax]
    mov     edx, [esp + 0ch + 8]        ; &EDX
    mov     edx, dword [edx]
    mov     ebx, [esp + 10h + 8]        ; EBX
    mov     ecx, [esp + 14h + 8]        ; ECX

    cmpxchg8b qword [ebp]               ; do CMPXCHG8B
    mov     dword [esp + 08h + 8], eax
    mov     dword [esp + 0ch + 8], edx

    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 18h + 8]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    pop     ebp
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMEmulateCmpXchg8b32_Error
    pop     ebx
    pop     ebp
    mov     eax, VERR_ACCESS_DENIED
    ret
ENDPROC     EMEmulateCmpXchg8b32

%endif
