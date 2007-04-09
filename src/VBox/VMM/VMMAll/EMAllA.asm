; $Id$
;; @file
; EM Assembly Routines.
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
%include "VBox/err.mac"
%include "VBox/x86.mac"

;; @def MY_PTR_REG
; The register we use for value pointers (And,Or,Dec,Inc).
%ifdef __AMD64__
%define MY_PTR_REG  rcx
%else
%define MY_PTR_REG  ecx
%endif

;; @def MY_RET_REG
; The register we return the result in.
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, rdx                    ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rsi                    ; eax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 08h]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, rdx                    ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rsi                    ; eax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 08h]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifdef __WIN64__
    mov     rax, r8                     ; eax = size of parameters
%else   ; !__WIN64__
    mov     rax, rdx                    ; rax = size of parameters
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
    mov     eax, [esp + 0ch]            ; eax = size of parameters
    mov     ecx, [esp + 04h]            ; ecx = first parameter
    mov     edx, [esp + 08h]            ; edx = second parameter
%endif

    ; switch on size
%ifdef __AMD64__
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
%ifdef __AMD64__
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
%ifdef __AMD64__
%ifndef __WIN64__
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
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
%ifdef __AMD64__
%ifndef __WIN64__
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
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
; @returns EFLAGS after the operation, only arithmetic flags is valid.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to data item.
; @param    [esp + 08h]    Param 2 - Second parameter.
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMEmulateBts
%ifdef __AMD64__
%ifndef __WIN64__
    mov     rcx, rdi                    ; rcx = first parameter
    mov     rdx, rsi                    ; rdx = second parameter
%endif  ; !__WIN64__
%else   ; !__AMD64__
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
