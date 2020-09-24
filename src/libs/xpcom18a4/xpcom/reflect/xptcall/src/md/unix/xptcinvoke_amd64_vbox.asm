; $Id$
;; @file
; XPCOM - Implementation XPTC_InvokeByIndex in assembly.
;
; This solves the problem of Clang and gcc (sometimes) not playing along with
; the alloca() based trick to pass stack parameters.  We first had trouble
; when enabling asan with gcc 8.2, then Clang 11 on mac had similar issues
; (at least for profile builds).
;

;
; Copyright (C) 2020 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*********************************************************************************************************************************
;*  Internal Functions                                                                                                           *
;*********************************************************************************************************************************
%include "iprt/asmdefs.mac"


;*********************************************************************************************************************************
;*  Structures and Typedefs                                                                                                      *
;*********************************************************************************************************************************
struc nsXPTCVariant
    .val    resq 1
    .ptr    resq 1
    .type   resb 1
    .flags  resb 1
    alignb 8
endstruc


;*********************************************************************************************************************************
;*  Defined Constants And Macros                                                                                                 *
;*********************************************************************************************************************************
;; @name Selected nsXPTCVariant::flags values.
;; @{
%define PTR_IS_DATA 1
;; @}

;; @name Selected nsXPTType (nsXPTCVariant::type) values.
;; @{
%define T_FLOAT     8
%define T_DOUBLE    9
;; @}

;; Error code we use if there are too many parameters.
%define DISP_E_BADPARAMCOUNT    0x8002000e


;;
;
; @cproto nsresult XPTC_InvokeByIndex(nsISupports *pThat, PRUint32 idxMethod, PRUint32 cParams, nsXPTCVariant *paParams)
;
; @param    pThat       Pointer to the object we're invoking a method on.   register:rdi
; @param    idxMethod   The VTable method index.                            register:esi
; @param    cParams     Number of parameters in addition to pThat.          register:edx
; @param    paParams    Array of parameter values and info.                 register:rcx
;
BEGINPROC_EXPORTED VBoxNsxpXPTC_InvokeByIndex
        push    rbp
        mov     rbp, rsp
        push    rbx
        push    r12
        push    r13

        ;
        ; Check that there aren't unreasonably many parameters
        ; (we could do ~255, but 64 is more reasonable number).
        ;
        cmp     edx, 64
        je      .too_many_parameters

        ;
        ; Look up the method address in the vtable and store it in r11.
        ;
        mov     r11, [rdi]              ; r11 = vtable
        mov     esi, esi                ; zero extend vtable index.
        mov     r11, [r11 + rsi * 8]    ; r11 = method to call.

        ;
        ; For simplicity reserve stack space for all parameters and point r10 at it.
        ;
        mov     r12d, edx               ; r12d = parameter count / left
        lea     edx, [edx * 8]
        sub     rsp, rdx
        and     rsp, 0ffffffffffffffe0h ; 32 byte align stack.
        mov     r10, rsp                ; r10 = next stack parameter.

        ;
        ; Set up parameter pointer and register distribution counts.
        ;
        mov     rbx, rcx                ; rbx = current parameter
        mov     eax, 1                  ; al = greg count, ah = fpreg count.

        ;
        ; Anything to do here?
        ;
        test    r12d,r12d
        jz      .make_call
        jmp     .param_loop

        ;
        ; The loop.
        ;
        ALIGNCODE(64)
.param_loop_advance:
        add     rbx, nsXPTCVariant_size
.param_loop:
        ; First test for pointers using 'flags' then work 'type' for the rest.
        test    byte [rbx + nsXPTCVariant.flags], PTR_IS_DATA
        jnz     .is_ptr
        cmp     dword [rbx + nsXPTCVariant.type], T_FLOAT
        jge     .maybe_in_fpreg

        ;
        ; nsXPTCVariant.val belongs in a GREG or on the stack.
        ; Note! Hope we can get away with not zero extending the value here.
        ;
.in_greg:
        inc     al
        cmp     al, 1+1
        je      .in_greg_rsi
        cmp     al, 2+1
        je      .in_greg_rdx
        cmp     al, 3+1
        je      .in_greg_rcx
        cmp     al, 4+1
        je      .in_greg_r8
        cmp     al, 5+1
        ja      .on_stack
.in_greg_r9:
        mov     r9, [rbx + nsXPTCVariant.val]
        jmp     .next
.in_greg_r8:
        mov     r8, [rbx + nsXPTCVariant.val]
        jmp     .next
.in_greg_rcx:
        mov     rcx, [rbx + nsXPTCVariant.val]
        jmp     .next
.in_greg_rdx:
        mov     rdx, [rbx + nsXPTCVariant.val]
        jmp     .next
.in_greg_rsi:
        mov     rsi, [rbx + nsXPTCVariant.val]
        jmp     .next

        ;
        ; Pointers are loaded from the 'ptr' rather than the 'val' member.
        ;
        ALIGNCODE(16)
.is_ptr:
        inc     al
        cmp     al, 1+1
        je      .ptr_in_greg_rsi
        cmp     al, 2+1
        je      .ptr_in_greg_rdx
        cmp     al, 3+1
        je      .ptr_in_greg_rcx
        cmp     al, 4+1
        je      .ptr_in_greg_r8
        cmp     al, 5+1
        je      .ptr_in_greg_r9
        mov     r13, [rbx + nsXPTCVariant.ptr]
        jmp     .r13_on_stack

.ptr_in_greg_r9:
        mov     r9, [rbx + nsXPTCVariant.ptr]
        jmp     .next
.ptr_in_greg_r8:
        mov     r8, [rbx + nsXPTCVariant.ptr]
        jmp     .next
.ptr_in_greg_rcx:
        mov     rcx, [rbx + nsXPTCVariant.ptr]
        jmp     .next
.ptr_in_greg_rdx:
        mov     rdx, [rbx + nsXPTCVariant.ptr]
        jmp     .next
.ptr_in_greg_rsi:
        mov     rsi, [rbx + nsXPTCVariant.ptr]
        jmp     .next

        ;
        ; Maybe we've got a float or double type here...
        ;
.maybe_in_fpreg:
        je      .float_in_fpreg
        cmp     dword [rbx + nsXPTCVariant.type], T_DOUBLE
        jne     .in_greg

.double_in_fpreg:
        cmp     ah, 8                   ; Ensure max AL value of 8 when making call.
        jge     .on_stack
        inc     ah
        cmp     ah, 0+1
        je      .double_in_xmm0
        cmp     ah, 1+1
        je      .double_in_xmm1
        cmp     ah, 2+1
        je      .double_in_xmm2
        cmp     ah, 3+1
        je      .double_in_xmm3
        cmp     ah, 4+1
        je      .double_in_xmm4
        cmp     ah, 5+1
        je      .double_in_xmm5
        cmp     ah, 6+1
        je      .double_in_xmm6
.double_in_xmm7:
        movsd   xmm7, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm6:
        movsd   xmm6, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm5:
        movsd   xmm5, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm4:
        movsd   xmm4, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm3:
        movsd   xmm3, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm2:
        movsd   xmm2, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm1:
        movsd   xmm1, [rbx + nsXPTCVariant.val]
        jmp     .next
.double_in_xmm0:
        movsd   xmm0, [rbx + nsXPTCVariant.val]
        jmp     .next

.float_in_fpreg:
        cmp     ah, 8                   ; Ensure max AL value of 8 when making call.
        jge     .on_stack
        inc     ah
        cmp     ah, 0+1
        je      .float_in_xmm0
        cmp     ah, 1+1
        je      .float_in_xmm1
        cmp     ah, 2+1
        je      .float_in_xmm2
        cmp     ah, 3+1
        je      .float_in_xmm3
        cmp     ah, 4+1
        je      .float_in_xmm4
        cmp     ah, 5+1
        je      .float_in_xmm5
        cmp     ah, 6+1
        je      .float_in_xmm6
.float_in_xmm7:
        movss   xmm7, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm6:
        movss   xmm6, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm5:
        movss   xmm5, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm4:
        movss   xmm4, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm3:
        movss   xmm3, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm2:
        movss   xmm2, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm1:
        movss   xmm1, [rbx + nsXPTCVariant.val]
        jmp     .next
.float_in_xmm0:
        movss   xmm0, [rbx + nsXPTCVariant.val]
        jmp     .next

        ;
        ; Put the value onto the stack via r13.
        ;
        ALIGNCODE(64)
.on_stack:
        mov     r13, [rbx + nsXPTCVariant.val]
.r13_on_stack:
        mov     [r10], r13
        lea     r10, [r10 + 8]

        ;
        ; Update parameter pointer and count and maybe loop again.
        ;
.next:
        dec     r12d
        jnz     .param_loop_advance

        ;
        ; Call the method and return.
        ;
.make_call:
        movzx   eax, ah                 ; AL = number of parameters in XMM registers (variadict only, but easy to do).
        call    r11

.return:
        lea     rsp, [rbp - 8*3]
        pop     r13
        pop     r12
        pop     rbx
        leave
        ret

.too_many_parameters:
        mov     eax, DISP_E_BADPARAMCOUNT
        jmp     .return
ENDPROC            VBoxNsxpXPTC_InvokeByIndex
