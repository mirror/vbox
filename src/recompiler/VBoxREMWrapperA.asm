; $Id$
;; @file
;
; VBoxREM Wrapper, Assembly routines and wrapper Templates.
;
; innotek GmbH confidential
;
; Copyright (C) 2006-2007 innotek GmbH
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

;%define ENTRY_LOGGING   1
;%define EXIT_LOGGING    1


%ifdef __AMD64__
 ;;
 ; 64-bit pushad
 %macro MY_PUSHAQ 0
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
 %endmacro

 ;;
 ; 64-bit popad
 %macro MY_POPAQ 0
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
 %endmacro

 ;;
 ; Entry logging
 %ifdef ENTRY_LOGGING
  %macro LOG_ENTRY 0
    MY_PUSHAQ
    push    rbp
    mov     rbp, rsp
    and     rsp, ~0fh
    sub     rsp, 20h                    ; shadow space

   %ifdef __WIN__
    mov     rcx, 0xdead00010001dead
   %else
    mov     rdi, 0xdead00010001dead
   %endif
    mov     rax, 0xdead00020002dead
    call    rax

    leave
    MY_POPAQ
  %endmacro
 %else
  %define LOG_ENTRY
 %endif

 ;;
 ; Exit logging
 %ifdef EXIT_LOGGING
  %macro LOG_EXIT 0
    MY_PUSHAQ
    push    rbp
    mov     rbp, rsp
    and     rsp, ~0fh
    sub     rsp, 20h                    ; shadow space

   %ifdef __WIN__
    mov     rdx, rax
    mov     rcx, 0xdead00010001dead
   %else
    mov     rsi, eax
    mov     rdi, 0xdead00010001dead
   %endif
    mov     rax, 0xdead00030003dead
    call    rax

    leave
    MY_POPAQ
  %endmacro
 %else
  %define LOG_EXIT
 %endif

%else
 %define LOG_ENTRY
 %define LOG_EXIT
%endif


BEGINCODE

%ifdef __WIN__
 %ifdef __AMD64__


BEGINPROC WrapGCC2MSC0Int
    LOG_ENTRY
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h

%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC0Int


BEGINPROC WrapGCC2MSC1Int
    LOG_ENTRY
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h

    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC1Int


BEGINPROC WrapGCC2MSC2Int
    LOG_ENTRY
    push    rbp
    mov     rbp, rsp
    sub     rsp, 20h

    mov     rdx, rsi
    mov     rcx, rdi
%ifdef USE_DIRECT_CALLS
    call    $+5+0deadbeefh
%else
    mov     rax, 0xdeadf00df00ddead
    call    rax
%endif

    leave
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC2Int


BEGINPROC WrapGCC2MSC3Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC3Int


BEGINPROC WrapGCC2MSC4Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC4Int


BEGINPROC WrapGCC2MSC5Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC5Int


BEGINPROC WrapGCC2MSC6Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC6Int


BEGINPROC WrapGCC2MSC7Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC7Int


BEGINPROC WrapGCC2MSC8Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC8Int


BEGINPROC WrapGCC2MSC9Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC9Int


BEGINPROC WrapGCC2MSC10Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC10Int


BEGINPROC WrapGCC2MSC11Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC11Int


BEGINPROC WrapGCC2MSC12Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapGCC2MSC12Int



BEGINPROC WrapGCC2MSCVariadictInt
    LOG_ENTRY
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
    LOG_EXIT
    jmp     rsi
    ; (*) unconditionally spill the registers, just in case '...' implies weird stuff on MSC. Check this out!
ENDPROC WrapGCC2MSCVariadictInt



;
; The other way around:
;


BEGINPROC WrapMSC2GCC0Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC0Int


BEGINPROC WrapMSC2GCC1Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC1Int


BEGINPROC WrapMSC2GCC2Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC2Int


BEGINPROC WrapMSC2GCC3Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC3Int


BEGINPROC WrapMSC2GCC4Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC4Int


BEGINPROC WrapMSC2GCC5Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC5Int


BEGINPROC WrapMSC2GCC6Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC6Int


BEGINPROC WrapMSC2GCC7Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC7Int


BEGINPROC WrapMSC2GCC8Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC8Int


BEGINPROC WrapMSC2GCC9Int
    LOG_ENTRY
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
    LOG_EXIT
    ret
ENDPROC WrapMSC2GCC9Int

 %endif ; __AMD64__
%endif ; __WIN__

