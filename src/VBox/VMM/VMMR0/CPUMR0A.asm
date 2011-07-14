; $Id$
;; @file
; CPUM - Guest Context Assembly Routines.
;

;
; Copyright (C) 2006-2007 Oracle Corporation
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
%include "VBox/vmm/vm.mac"
%include "VBox/err.mac"
%include "VBox/vmm/stam.mac"
%include "CPUMInternal.mac"
%include "iprt/x86.mac"
%include "VBox/vmm/cpum.mac"

%ifdef IN_RING3
 %error "The jump table doesn't link on leopard."
%endif

;*******************************************************************************
;*      Defined Constants And Macros                                           *
;*******************************************************************************
;; The offset of the XMM registers in X86FXSTATE.
; Use define because I'm too lazy to convert the struct.
%define XMM_OFF_IN_X86FXSTATE   160


;*******************************************************************************
;* External Symbols                                                            *
;*******************************************************************************
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
extern NAME(SUPR0AbsIs64bit)
extern NAME(SUPR0Abs64bitKernelCS)
extern NAME(SUPR0Abs64bitKernelSS)
extern NAME(SUPR0Abs64bitKernelDS)
extern NAME(SUPR0AbsKernelCS)
%endif


;*******************************************************************************
;*  Global Variables                                                           *
;*******************************************************************************
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
BEGINDATA
;;
; Store the SUPR0AbsIs64bit absolute value here so we can cmp/test without
; needing to clobber a register. (This trick doesn't quite work for PE btw.
; but that's not relevant atm.)
GLOBALNAME g_fCPUMIs64bitHost
    dd  NAME(SUPR0AbsIs64bit)
%endif


BEGINCODE


;;
; Saves the host FPU/XMM state and restores the guest state.
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostRestoreGuestFPUState
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    ; Switch the state.
    or      dword [xDX + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)

    mov     xAX, cr0                    ; Make sure its safe to access the FPU state.
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX                    ;; @todo optimize this.

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    fxsave  [xDX + CPUMCPU.Host.fpu]    ; ASSUMES that all VT-x/AMD-V boxes sports fxsave/fxrstor (safe assumption)
    fxrstor [xDX + CPUMCPU.Guest.fpu]

%ifdef VBOX_WITH_KERNEL_USING_XMM
    ; Restore the non-volatile xmm registers. ASSUMING 64-bit windows
    lea     r11, [xDX + CPUMCPU.Host.fpu + XMM_OFF_IN_X86FXSTATE]
    movdqa  xmm6,  [r11 + 060h]
    movdqa  xmm7,  [r11 + 070h]
    movdqa  xmm8,  [r11 + 080h]
    movdqa  xmm9,  [r11 + 090h]
    movdqa  xmm10, [r11 + 0a0h]
    movdqa  xmm11, [r11 + 0b0h]
    movdqa  xmm12, [r11 + 0c0h]
    movdqa  xmm13, [r11 + 0d0h]
    movdqa  xmm14, [r11 + 0e0h]
    movdqa  xmm15, [r11 + 0f0h]
%endif

.done:
    mov     cr0, xCX                    ; and restore old CR0 again ;; @todo optimize this.
    popf
    xor     eax, eax
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    fxsave  [rdx + CPUMCPU.Host.fpu]
    fxrstor [rdx + CPUMCPU.Guest.fpu]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC   cpumR0SaveHostRestoreGuestFPUState


%ifndef RT_ARCH_AMD64
%ifdef  VBOX_WITH_64_BITS_GUESTS
%ifndef VBOX_WITH_HYBRID_32BIT_KERNEL
;;
; Saves the host FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostFPUState
    mov     xDX, dword [esp + 4]
    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    ; Switch the state.
    or      dword [xDX + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)

    mov     xAX, cr0                    ; Make sure its safe to access the FPU state.
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX                    ;; @todo optimize this.

    fxsave  [xDX + CPUMCPU.Host.fpu]    ; ASSUMES that all VT-x/AMD-V boxes support fxsave/fxrstor (safe assumption)

    mov     cr0, xCX                    ; and restore old CR0 again ;; @todo optimize this.
    popf
    xor     eax, eax
    ret
ENDPROC   cpumR0SaveHostFPUState
%endif
%endif
%endif


;;
; Saves the guest FPU/XMM state and restores the host state.
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveGuestRestoreHostFPUState
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif

    ; Only restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USED_FPU
    jz short .fpu_not_used

    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    mov     xAX, cr0                    ; Make sure it's safe to access the FPU state.
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX                    ;; @todo optimize this.

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    fxsave  [xDX + CPUMCPU.Guest.fpu]   ; ASSUMES that all VT-x/AMD-V boxes support fxsave/fxrstor (safe assumption)
    fxrstor [xDX + CPUMCPU.Host.fpu]

.done:
    mov     cr0, xCX                    ; and restore old CR0 again ;; @todo optimize this.
    and     dword [xDX + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
    popf
.fpu_not_used:
    xor     eax, eax
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    fxsave  [rdx + CPUMCPU.Guest.fpu]
    fxrstor [rdx + CPUMCPU.Host.fpu]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC   cpumR0SaveGuestRestoreHostFPUState


;;
; Sets the host's FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0RestoreHostFPUState
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USED_FPU
    jz short .fpu_not_used

    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    mov     xAX, cr0
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    fxrstor [xDX + CPUMCPU.Host.fpu]

.done:
    mov     cr0, xCX                    ; and restore old CR0 again
    and     dword [xDX + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
    popf
.fpu_not_used:
    xor     eax, eax
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    fxrstor [rdx + CPUMCPU.Host.fpu]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC   cpumR0RestoreHostFPUState


%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
;;
; DECLASM(void)     cpumR0SaveDRx(uint64_t *pa4Regs);
;
ALIGNCODE(16)
BEGINPROC cpumR0SaveDRx
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     xCX, rdi
 %endif
%else
    mov     xCX, dword [esp + 4]
%endif
    pushf                               ; Just to be on the safe side.
    cli
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    ;
    ; Do the job.
    ;
    mov     xAX, dr0
    mov     xDX, dr1
    mov     [xCX],         xAX
    mov     [xCX + 8 * 1], xDX
    mov     xAX, dr2
    mov     xDX, dr3
    mov     [xCX + 8 * 2], xAX
    mov     [xCX + 8 * 3], xDX

.done:
    popf
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     ecx, 0ffffffffh

    mov     rax, dr0
    mov     rdx, dr1
    mov     r8,  dr2
    mov     r9,  dr3
    mov     [rcx],         rax
    mov     [rcx + 8 * 1], rdx
    mov     [rcx + 8 * 2], r8
    mov     [rcx + 8 * 3], r9
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC   cpumR0SaveDRx


;;
; DECLASM(void)     cpumR0LoadDRx(uint64_t const *pa4Regs);
;
ALIGNCODE(16)
BEGINPROC cpumR0LoadDRx
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     xCX, rdi
 %endif
%else
    mov     xCX, dword [esp + 4]
%endif
    pushf                               ; Just to be on the safe side.
    cli
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    ;
    ; Do the job.
    ;
    mov     xAX, [xCX]
    mov     xDX, [xCX + 8 * 1]
    mov     dr0, xAX
    mov     dr1, xDX
    mov     xAX, [xCX + 8 * 2]
    mov     xDX, [xCX + 8 * 3]
    mov     dr2, xAX
    mov     dr3, xDX

.done:
    popf
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     ecx, 0ffffffffh

    mov     rax, [rcx]
    mov     rdx, [rcx + 8 * 1]
    mov     r8,  [rcx + 8 * 2]
    mov     r9,  [rcx + 8 * 3]
    mov     dr0, rax
    mov     dr1, rdx
    mov     dr2, r8
    mov     dr3, r9
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC   cpumR0LoadDRx

%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0

