; $Id$
;; @file
;
; VBoxREM Wrapper, Assembly routines and wrapper Templates.
;
; InnoTek Systemberatung GmbH confidential
;
; Copyright (c) 2006 InnoTek Systemberatung GmbH
;
; Author: knut st. osmundsen <bird@innotek.de>
;
; All Rights Reserved
;
;




;*******************************************************************************
;*  Header Files                                                               *
;*******************************************************************************
%include "iprt/asmdefs.mac"



BEGINCODE

%ifdef __WIN64__


BEGINPROC WrapGCC2MSC0Int
%ifdef USE_DIRECT_CALLS
    jmp     $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    jmp     rax
%endif
ENDPROC WrapGCC2MSC0Int


BEGINPROC WrapGCC2MSC1Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 10h

    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC1Int


BEGINPROC WrapGCC2MSC2Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 10h

    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC2Int


BEGINPROC WrapGCC2MSC3Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h

    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC3Int


BEGINPROC WrapGCC2MSC4Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h

    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC4Int


BEGINPROC WrapGCC2MSC5Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h

    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC5Int


BEGINPROC WrapGCC2MSC6Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h

    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC6Int


BEGINPROC WrapGCC2MSC7Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h

    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC7Int


BEGINPROC WrapGCC2MSC8Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h

    mov     r10, [ebp + 18h]
    mov     [rsp + 38h], r10
    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC8Int


BEGINPROC WrapGCC2MSC9Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 50h

    mov     rax, [ebp + 20h]
    mov     [rsp + 40h], rax
    mov     r10, [ebp + 18h]
    mov     [rsp + 38h], r10
    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC9Int


BEGINPROC WrapGCC2MSC10Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 50h

    mov     r11, [ebp + 28h]
    mov     [rsp + 48h], r11
    mov     rax, [ebp + 20h]
    mov     [rsp + 40h], rax
    mov     r10, [ebp + 18h]
    mov     [rsp + 38h], r10
    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC10Int


BEGINPROC WrapGCC2MSC11Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 60h

    mov     r10, [ebp + 30h]
    mov     [rsp + 50h], r10
    mov     r11, [ebp + 28h]
    mov     [rsp + 48h], r11
    mov     rax, [ebp + 20h]
    mov     [rsp + 40h], rax
    mov     r10, [ebp + 18h]
    mov     [rsp + 38h], r10
    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC11Int


BEGINPROC WrapGCC2MSC12Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 60h

    mov     rax, [ebp + 28h]
    mov     [rsp + 48h], rax
    mov     r10, [ebp + 30h]
    mov     [rsp + 50h], r10
    mov     r11, [ebp + 28h]
    mov     [rsp + 48h], r11
    mov     rax, [ebp + 20h]
    mov     [rsp + 40h], rax
    mov     r10, [ebp + 18h]
    mov     [rsp + 38h], r10
    mov     r11, [ebp + 10h]
    mov     [rsp + 30h], r11
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     r8, rdx
    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    ret
ENDPROC WrapGCC2MSC12Int



BEGINPROC WrapGCC2MSCVariadictInt
%ifdef DEBUG
    ; check that there are NO floting point arguments in XMM registers!
    or      rax, rax
    jz      .ok
    int3
.ok:
%endif
    sub     rsp, 28h
    mov     r11, [rsp + 28h]            ; r11 = return address.
    mov     [rsp + 28h], r9
    mov     [rsp + 20h], r8
    mov     r9, rcx
    mov     [rsp + 18h], r9             ; (*)
    mov     r8, rdx
    mov     [rsp + 14h], r8             ; (*)
    mov     rdx, rsi
    mov     [rsp + 8h], rdx             ; (*)
    mov     rcx, rdi
    mov     [rsp], rcx                  ; (*)
    mov     rsi, r11                    ; rsi is preserved by the callee.
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    add     rsp, 30h
    jmp     rsi
    ; (*) unconditionally spill the registers, just in case '...' implies weird stuff on MSC. Check this out!
ENDPROC WrapGCC2MSCVariadictInt



;
; The other way around:
;


BEGINPROC WrapMSC2GCC0Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 10h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC0Int


BEGINPROC WrapMSC2GCC1Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC1Int


BEGINPROC WrapMSC2GCC2Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC2Int


BEGINPROC WrapMSC2GCC3Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC3Int


BEGINPROC WrapMSC2GCC4Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC4Int


BEGINPROC WrapMSC2GCC5Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    mov     r8, [ebp + 30h]
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC5Int


BEGINPROC WrapMSC2GCC6Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    mov     r8, [ebp + 30h]
    mov     r9, [ebp + 38h]
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC6Int


BEGINPROC WrapMSC2GCC7Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    mov     r8, [ebp + 30h]
    mov     r9, [ebp + 38h]
    mov     r10, [ebp + 40h]
    mov     [esp], r10
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC7Int


BEGINPROC WrapMSC2GCC8Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    mov     r8, [ebp + 30h]
    mov     r9, [ebp + 38h]
    mov     r10, [ebp + 40h]
    mov     [esp], r10
    mov     r11, [ebp + 48h]
    mov     [esp + 8], r11
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC8Int


BEGINPROC WrapMSC2GCC9Int
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    mov     [ebp - 10h], rsi
    mov     [ebp - 18h], rdi

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rdx, r8
    mov     rcx, r9
    mov     r8, [ebp + 30h]
    mov     r9, [ebp + 38h]
    mov     r10, [ebp + 40h]
    mov     [esp], r10
    mov     r11, [ebp + 48h]
    mov     [esp + 8], r11
    mov     rax, [ebp + 50h]
    mov     [esp + 10h], rax
    call    $+5+0deadbeefh

    mov     rdi, [ebp - 18h]
    mov     rsi, [ebp - 10h]
    leave
    ret
ENDPROC WrapMSC2GCC9Int

%endif ; __WIN64__
