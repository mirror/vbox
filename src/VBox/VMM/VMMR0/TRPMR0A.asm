; $Id$
;; @file
; TRPM - Host Context Ring-0
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
%include "VBox/x86.mac"


BEGINCODE
    align 16

;;
; Calls the interrupt gate as if we received an interrupt while in Ring-0.
;
; Returns with interrupts enabled.
;
; @param   uIP     x86:[ebp+8]   msc:rcx  gcc:rdi  The interrupt gate IP.
; @param   SelCS   x86:[ebp+12]  msc:dx   gcc:si   The interrupt gate CS.
; @param   RSP                   msc:r8   gcc:rdx  The interrupt gate RSP. ~0 if no stack switch should take place. (only AMD64)
;DECLASM(void) trpmR0DispatchHostInterrupt(RTR0UINTPTR uIP, RTSEL SelCS, RTR0UINTPTR RSP);
BEGINPROC trpmR0DispatchHostInterrupt
    push    xBP
    mov     xBP, xSP

%ifdef __AMD64__
    mov     rax, rsp
    and     rsp, 15h                    ; align the stack. (do it unconditionally saves some jump mess)

    ; switch stack?
 %ifdef ASM_CALL64_MSC
    cmp     r8, 0ffffffffffffffffh
    je      .no_stack_switch
    mov     rsp, r8
 %else
    cmp     rdx, 0ffffffffffffffffh
    je      .no_stack_switch
    mov     rsp, rdx
 %endif
.no_stack_switch:

    ; create the iret frame
    push    0                           ; SS
    push    rax                         ; RSP
    pushfd                              ; RFLAGS
    and     dword [rsp], ~X86_EFL_IF
    mov     ax, cs
    push    rax                         ; CS
    mov     rax, .return                ; RIP
    push    rax

    ; create the retf frame
 %ifdef ASM_CALL64_MSC
    movzx   rdx, dx
    push    rdx
    push    rcx
 %else
    movzx   rdi, di
    push    rdi
    push    rsi
 %endif

    ; dispatch it!
    db 048h
    retf

%else ; 32-bit:
    mov     ecx, [ebp + 8]              ; uIP
    movzx   edx, word [ebp + 12]        ; SelCS

    ; create the iret frame
    pushfd                              ; EFLAGS
    and     dword [esp], ~X86_EFL_IF
    push    cs                          ; CS
    push    .return                     ; EIP

    ; create the retf frame
    push    edx
    push    ecx

    ; dispatch it!
    retf
%endif
.return:

    leave
    ret
ENDPROC trpmR0DispatchHostInterrupt


%ifndef VBOX_WITHOUT_IDT_PATCHING

    align 16
;;
; This is the alternative return from VMMR0Entry() used when
; we need to dispatch an interrupt to the Host (we received it in GC).
;
; As seen in TRPMR0SetupInterruptDispatcherFrame() the stack is different
; than for the normal VMMR0Entry() return.
;
; 32-bit:
;           18  iret frame
;           14  retf selector (interrupt handler)
;           10  retf offset   (interrupt handler)
;            c  es
;            8  fs
;            4  ds
;            0  pVM (esp here)
;
; 64-bit:
;           24  iret frame
;           18  retf selector (interrupt handler)
;           10  retf offset   (interrupt handler)
;            8  uOperation
;            0  pVM
;
BEGINPROC trpmR0InterruptDispatcher
%ifdef __AMD64__
    lea     rsp, [rsp + 10h]            ; skip pVM and uOperation
    db 48h
    retf
%else  ; !__AMD64__
    add     esp, byte 4                 ; skip pVM
    pop     ds
    pop     fs
    pop     es
    retf
%endif ; !__AMD64__
ENDPROC   trpmR0InterruptDispatcher

%endif ; !VBOX_WITHOUT_IDT_PATCHING

