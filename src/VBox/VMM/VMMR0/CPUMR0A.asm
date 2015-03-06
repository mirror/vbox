; $Id$
;; @file
; CPUM - Guest Context Assembly Routines.
;

;
; Copyright (C) 2006-2015 Oracle Corporation
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
%define IP_OFF_IN_X86FXSTATE    08h
%define CS_OFF_IN_X86FXSTATE    0ch
%define DS_OFF_IN_X86FXSTATE    14h


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

;; Macro for FXSAVE/FXRSTOR leaky behaviour on AMD CPUs, see cpumR3CheckLeakyFpu().
; Cleans the FPU state, if necessary, before restoring the FPU.
;
; This macro ASSUMES CR0.TS is not set!
; @remarks Trashes xAX!!
; Changes here should also be reflected in CPUMRCA.asm's copy!
%macro CLEANFPU 0
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USE_FFXSR_LEAKY
    jz      .nothing_to_clean

    xor     eax, eax
    fnstsw  ax               ; Get FSW
    test    eax, RT_BIT(7)   ; If FSW.ES (bit 7) is set, clear it to not cause FPU exceptions
                             ; while clearing & loading the FPU bits in 'clean_fpu'
    jz      .clean_fpu
    fnclex

.clean_fpu:
    ffree   st7              ; Clear FPU stack register(7)'s tag entry to prevent overflow if a wraparound occurs
                             ; for the upcoming push (load)
    fild    dword [xDX + CPUMCPU.Guest.fpu] ; Explicit FPU load to overwrite FIP, FOP, FDP registers in the FPU.

.nothing_to_clean:
%endmacro


;; Macro for FXSAVE for the guest FPU but tries to figure out whether to
;  save the 32-bit FPU state or 64-bit FPU state.
;
; @remarks Requires CPUMCPU pointer in RDX
%macro SAVE_32_OR_64_FPU 0
    o64 fxsave  [rdx + CPUMCPU.Guest.fpu]

    ; Shouldn't be necessary to check if the entire 64-bit FIP is 0 (i.e. guest hasn't used its FPU yet) because it should
    ; be taken care of by the calling code, i.e. hmR0[Vmx|Svm]LoadSharedCR0() and hmR0[Vmx|Svm]ExitXcptNm() which ensure
    ; we swap the guest FPU state when it starts using it (#NM). In any case it's only a performance optimization.
    ; cmp         qword [rdx + CPUMCPU.Guest.fpu + IP_OFF_IN_X86FXSTATE], 0
    ; je          short %%save_done

    cmp         dword [rdx + CPUMCPU.Guest.fpu + CS_OFF_IN_X86FXSTATE], 0
    jne         short %%save_done
    sub         rsp, 20h                         ; Only need 1ch bytes but keep stack aligned otherwise we #GP(0)
    fnstenv     [rsp]
    movzx       eax, word [rsp + 10h]
    mov         [rdx + CPUMCPU.Guest.fpu + CS_OFF_IN_X86FXSTATE], eax
    movzx       eax, word [rsp + 18h]
    mov         [rdx + CPUMCPU.Guest.fpu + DS_OFF_IN_X86FXSTATE], eax
    add         rsp, 20h
    mov         dword [rdx + CPUMCPU.Guest.fpu + X86_OFF_FXSTATE_RSVD], X86_FXSTATE_RSVD_32BIT_MAGIC
%%save_done:
%endmacro

;; Macro for FXRSTOR for the guest FPU but loads the one based on what
;  was saved before using SAVE_32_OR_64_FPU().
;
; @remarks Requires CPUMCPU pointer in RDX
%macro RESTORE_32_OR_64_FPU 0
    cmp         dword [rdx + CPUMCPU.Guest.fpu + X86_OFF_FXSTATE_RSVD], X86_FXSTATE_RSVD_32BIT_MAGIC
    jne         short %%restore_64bit_fpu
    fxrstor     [rdx + CPUMCPU.Guest.fpu]
    jmp         short %%restore_fpu_done
%%restore_64bit_fpu:
    o64 fxrstor [rdx + CPUMCPU.Guest.fpu]
%%restore_fpu_done:
%endmacro


;; Macro to save and modify CR0 (if necessary) before touching the FPU state
;  so as to not cause any FPU exceptions.
;
; @remarks Uses xCX for backing-up CR0 (if CR0 needs to be modified) otherwise clears xCX.
; @remarks Trashes xAX.
%macro SAVE_CR0_CLEAR_FPU_TRAPS 0
    xor     ecx, ecx
    mov     xAX, cr0
    test    eax, X86_CR0_TS | X86_CR0_EM    ; Make sure its safe to access the FPU state.
    jz      %%skip_cr0_write
    mov     xCX, xAX                        ; Save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX
%%skip_cr0_write:
%endmacro

;; Macro to restore CR0 from xCX if necessary.
;
; @remarks xCX should contain the CR0 value to restore or 0 if no restoration is needed.
%macro RESTORE_CR0 0
    cmp     ecx, 0
    je      %%skip_cr0_restore
    mov     cr0, xCX
%%skip_cr0_restore:
%endmacro


;;
; Saves the host FPU/XMM state and restores the guest state.
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
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

    ; Clear CR0 FPU bits to not cause exceptions, uses xCX
    SAVE_CR0_CLEAR_FPU_TRAPS
    ; Do NOT use xCX from this point!

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

%ifdef RT_ARCH_AMD64
    ; Use explicit REX prefix. See @bugref{6398}.
    o64 fxsave  [rdx + CPUMCPU.Host.fpu]    ; ASSUMES that all VT-x/AMD-V boxes sports fxsave/fxrstor (safe assumption)

    ; Restore the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
    test    dword [rdx + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
    jnz     short .fpu_load_32_or_64
    fxrstor [rdx + CPUMCPU.Guest.fpu]
    jmp     short .fpu_load_done
.fpu_load_32_or_64:
    RESTORE_32_OR_64_FPU
.fpu_load_done:
%else
    fxsave  [edx + CPUMCPU.Host.fpu]        ; ASSUMES that all VT-x/AMD-V boxes sports fxsave/fxrstor (safe assumption)
    fxrstor [edx + CPUMCPU.Guest.fpu]
%endif

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
    ; Restore CR0 from xCX if it was previously saved.
    RESTORE_CR0
    popf
    xor     eax, eax
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    o64 fxsave  [rdx + CPUMCPU.Host.fpu]

    ; Restore the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
    test    dword [rdx + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
    jnz     short .fpu_load_32_or_64_darwin
    fxrstor [rdx + CPUMCPU.Guest.fpu]
    jmp     short .fpu_load_done_darwin
.fpu_load_32_or_64_darwin:
    RESTORE_32_OR_64_FPU
.fpu_load_done_darwin:

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
; @param    pCPUMCPU  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostFPUState
    mov     xDX, dword [esp + 4]
    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    ; Switch the state.
    or      dword [xDX + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)

    ; Clear CR0 FPU bits to not cause exceptions, uses xCX
    SAVE_CR0_CLEAR_FPU_TRAPS
    ; Do NOT use xCX from this point!

    fxsave  [xDX + CPUMCPU.Host.fpu]    ; ASSUMES that all VT-x/AMD-V boxes support fxsave/fxrstor (safe assumption)

    ; Restore CR0 from xCX if it was saved previously.
    RESTORE_CR0

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
; @param    pCPUMCPU  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
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
    jz      .fpu_not_used

    pushf                               ; The darwin kernel can get upset or upset things if an
    cli                                 ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

    ; Clear CR0 FPU bits to not cause exceptions, uses xCX
    SAVE_CR0_CLEAR_FPU_TRAPS
    ; Do NOT use xCX from this point!

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

%ifdef RT_ARCH_AMD64
    ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
    test    dword [rdx + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
    jnz     short .fpu_save_32_or_64
    fxsave  [rdx + CPUMCPU.Guest.fpu]
    jmp     short .fpu_save_done
.fpu_save_32_or_64:
    SAVE_32_OR_64_FPU
.fpu_save_done:

    ; Use explicit REX prefix. See @bugref{6398}.
    o64 fxrstor [rdx + CPUMCPU.Host.fpu]
%else
    fxsave  [edx + CPUMCPU.Guest.fpu]       ; ASSUMES that all VT-x/AMD-V boxes support fxsave/fxrstor (safe assumption)
    fxrstor [edx + CPUMCPU.Host.fpu]
%endif

.done:
    ; Restore CR0 from xCX if it was previously saved.
    RESTORE_CR0
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

    ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
    test    dword [rdx + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
    jnz     short .fpu_save_32_or_64_darwin
    fxsave  [rdx + CPUMCPU.Guest.fpu]
    jmp     short .fpu_save_done_darwin
.fpu_save_32_or_64_darwin:
    SAVE_32_OR_64_FPU
.fpu_save_done_darwin:

    o64 fxrstor [rdx + CPUMCPU.Host.fpu]
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
; @param    pCPUMCPU  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
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

    ; Clear CR0 FPU bits to not cause exceptions, uses xCX
    SAVE_CR0_CLEAR_FPU_TRAPS
    ; Do NOT use xCX from this point!

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

%ifdef RT_ARCH_AMD64
    o64 fxrstor [xDX + CPUMCPU.Host.fpu]
%else
    fxrstor [xDX + CPUMCPU.Host.fpu]
%endif

.done:
    ; Restore CR0 from xCX if it was previously saved.
    RESTORE_CR0
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
    o64 fxrstor [rdx + CPUMCPU.Host.fpu]
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

