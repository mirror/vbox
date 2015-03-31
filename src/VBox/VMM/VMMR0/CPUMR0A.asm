; $Id$
;; @file
; CPUM - Ring-0 Assembly Routines (supporting HM and IEM).
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
%if 0 ; Currently not used.
g_r32_Zero:    dd 0.0
%endif

;;
; Store the SUPR0AbsIs64bit absolute value here so we can cmp/test without
; needing to clobber a register. (This trick doesn't quite work for PE btw.
; but that's not relevant atm.)
GLOBALNAME g_fCPUMIs64bitHost
    dd  NAME(SUPR0AbsIs64bit)
%endif


BEGINCODE

%if 0 ; Currently not used anywhere.
;;
; Macro for FXSAVE/FXRSTOR leaky behaviour on AMD CPUs, see cpumR3CheckLeakyFpu().
;
; Cleans the FPU state, if necessary, before restoring the FPU.
;
; This macro ASSUMES CR0.TS is not set!
;
; @param    xDX     Pointer to CPUMCPU.
; @uses     xAX, EFLAGS
;
; Changes here should also be reflected in CPUMRCA.asm's copy!
;
%macro CLEANFPU 0
        test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USE_FFXSR_LEAKY
        jz      .nothing_to_clean

        xor     eax, eax
        fnstsw  ax                      ; FSW -> AX.
        test    eax, RT_BIT(7)          ; If FSW.ES (bit 7) is set, clear it to not cause FPU exceptions
                                        ; while clearing & loading the FPU bits in 'clean_fpu' below.
        jz      .clean_fpu
        fnclex

.clean_fpu:
        ffree   st7                     ; Clear FPU stack register(7)'s tag entry to prevent overflow if a wraparound occurs.
                                        ; for the upcoming push (load)
        fild    dword [g_r32_Zero xWrtRIP] ; Explicit FPU load to overwrite FIP, FOP, FDP registers in the FPU.
.nothing_to_clean:
%endmacro
%endif ; Unused.


;; Macro for FXSAVE for the guest FPU but tries to figure out whether to
;  save the 32-bit FPU state or 64-bit FPU state.
;
; @param    %1      Pointer to CPUMCPU.
; @param    %2      Pointer to XState.
; @uses     xAX, xDX, EFLAGS, 20h of stack.
;
%macro SAVE_32_OR_64_FPU 2
        o64 fxsave [%2]

        xor     edx, edx
        cmp     dword [%2 + CS_OFF_IN_X86FXSTATE], 0
        jne     short %%save_done

        sub     rsp, 20h                ; Only need 1ch bytes but keep stack aligned otherwise we #GP(0).
        fnstenv [rsp]
        movzx   eax, word [rsp + 10h]
        mov     [%2 + CS_OFF_IN_X86FXSTATE], eax
        movzx   eax, word [rsp + 18h]
        add     rsp, 20h
        mov     [%2 + DS_OFF_IN_X86FXSTATE], eax
        mov     edx, X86_FXSTATE_RSVD_32BIT_MAGIC

%%save_done:
        mov     dword [%2 + X86_OFF_FXSTATE_RSVD], edx
%endmacro

;;
; Wrapper for selecting 32-bit or 64-bit FXRSTOR according to what SAVE_32_OR_64_FPU did.
;
; @param    %1      Pointer to CPUMCPU.
; @param    %2      Pointer to XState.
; @uses     xAX, xDX, EFLAGS
;
%macro RESTORE_32_OR_64_FPU 2
        cmp     dword [%2 + X86_OFF_FXSTATE_RSVD], X86_FXSTATE_RSVD_32BIT_MAGIC
        jne     short %%restore_64bit_fpu
        fxrstor [%2]
        jmp     short %%restore_fpu_done
%%restore_64bit_fpu:
        o64 fxrstor [%2]
%%restore_fpu_done:
%endmacro


;;
; Clears CR0.TS and CR0.EM if necessary, saving the previous result.
;
; This is used to avoid FPU exceptions when touching the FPU state.
;
; @param    %1      Register to save the old CR0 in (pass to RESTORE_CR0).
; @param    %2      Temporary scratch register.
; @uses     EFLAGS, CR0
;
%macro SAVE_CR0_CLEAR_FPU_TRAPS 2
        xor     %1, %1
        mov     %2, cr0
        test    %2, X86_CR0_TS | X86_CR0_EM ; Make sure its safe to access the FPU state.
        jz      %%skip_cr0_write
        mov     %1, %2                  ; Save old CR0
        and     %2, ~(X86_CR0_TS | X86_CR0_EM)
        mov     cr0, %2
%%skip_cr0_write:
%endmacro

;;
; Restore CR0.TS and CR0.EM state if SAVE_CR0_CLEAR_FPU_TRAPS change it.
;
; @param    %1      The register that SAVE_CR0_CLEAR_FPU_TRAPS saved the old CR0 in.
;
%macro RESTORE_CR0 1
        cmp     %1, 0
        je      %%skip_cr0_restore
        mov     cr0, %1
%%skip_cr0_restore:
%endmacro


;;
; Saves the host FPU/SSE/AVX state and restores the guest FPU/SSE/AVX state.
;
; @returns  0
; @param    pCpumCpu  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostRestoreGuestFPUState
        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
        mov     r11, rcx
 %else
        mov     r11, rdi
 %endif
 %define pCpumCpu   r11
 %define pXState    r10
%else
        push    ebx
        push    esi
        mov     ebx, dword [esp + 4]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        ;
        ; Switch state.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      .legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif

%ifdef RT_ARCH_AMD64
        o64 fxsave [pXState]            ; Use explicit REX prefix. See @bugref{6398}.

        ; Restore the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jnz     short .fpu_load_32_or_64
        fxrstor [pXState]
        jmp     short .fpu_load_done
.fpu_load_32_or_64:
        RESTORE_32_OR_64_FPU pCpumCpu, pXState
.fpu_load_done:
%else
        fxsave  [pXState]
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        fxrstor [pXState]
%endif

%ifdef VBOX_WITH_KERNEL_USING_XMM
        ; Restore the non-volatile xmm registers. ASSUMING 64-bit host.
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        movdqa  xmm6,  [pXState + XMM_OFF_IN_X86FXSTATE + 060h]
        movdqa  xmm7,  [pXState + XMM_OFF_IN_X86FXSTATE + 070h]
        movdqa  xmm8,  [pXState + XMM_OFF_IN_X86FXSTATE + 080h]
        movdqa  xmm9,  [pXState + XMM_OFF_IN_X86FXSTATE + 090h]
        movdqa  xmm10, [pXState + XMM_OFF_IN_X86FXSTATE + 0a0h]
        movdqa  xmm11, [pXState + XMM_OFF_IN_X86FXSTATE + 0b0h]
        movdqa  xmm12, [pXState + XMM_OFF_IN_X86FXSTATE + 0c0h]
        movdqa  xmm13, [pXState + XMM_OFF_IN_X86FXSTATE + 0d0h]
        movdqa  xmm14, [pXState + XMM_OFF_IN_X86FXSTATE + 0e0h]
        movdqa  xmm15, [pXState + XMM_OFF_IN_X86FXSTATE + 0f0h]
%endif

.done:
        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        xor     eax, eax
        ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
        o64 fxsave  [pXState]

        ; Restore the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jnz     short .fpu_load_32_or_64_darwin
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        fxrstor [pXState]
        jmp     short .fpu_load_done_darwin
.fpu_load_32_or_64_darwin:
        RESTORE_32_OR_64_FPU pCpumCpu, pXState
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
; Saves the host FPU/SSE/AVX state.
;
; @returns  VINF_SUCCESS (0) in EAX
; @param    pCpumCpu  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostFPUState
        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
        mov     r11, rcx
 %else
        mov     r11, rdi
 %endif
 %define pCpumCpu   r11
 %define pXState    r10
%else
        push    ebx
        push    esi
        mov     ebx, dword [esp + 4]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        ;
        ; Save the host state.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      .legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif

%ifdef RT_ARCH_AMD64
        o64 fxsave [pXstate]
%else
        fxsave  [pXState]
%endif

.done:
        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        xor     eax, eax
        ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
        ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        o64 fxsave [pXstate]
        jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
        dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
%undef pCpumCpu
%undef pXState
ENDPROC   cpumR0SaveHostFPUState
%endif
%endif
%endif


;;
; Saves the guest FPU/SSE/AVX state and restores the host FPU/SSE/AVX state.
;
; @returns  VINF_SUCCESS (0) in eax.
; @param    pCpumCpu  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveGuestRestoreHostFPUState
        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
        mov     r11, rcx
 %else
        mov     r11, rdi
 %endif
 %define pCpumCpu   r11
 %define pXState    r10
%else
        push    ebx
        push    esi
        mov     ebx, dword [esp + 4]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        ;
        ; Only restore FPU if guest has used it.
        ;
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USED_FPU
        jz      .fpu_not_used

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      .legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif

%ifdef RT_ARCH_AMD64
        ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jnz     short .fpu_save_32_or_64
        fxsave  [pXState]
        jmp     short .fpu_save_done
.fpu_save_32_or_64:
        SAVE_32_OR_64_FPU pCpumCpu, pXState
.fpu_save_done:

        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        o64 fxrstor [pXState]           ; Use explicit REX prefix. See @bugref{6398}.
%else
        fxsave  [pXState]               ; ASSUMES that all VT-x/AMD-V boxes support fxsave/fxrstor (safe assumption)
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        fxrstor [pXState]
%endif

.done:
        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
        popf

.fpu_not_used:
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        xor     eax, eax
        ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
        ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jnz     short .fpu_save_32_or_64_darwin
        fxsave  [pXState]
        jmp     short .fpu_save_done_darwin
.fpu_save_32_or_64_darwin:
        SAVE_32_OR_64_FPU pCpumCpu, pXState
.fpu_save_done_darwin:

        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        o64 fxrstor [pXstate]
        jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
        dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
%undef pCpumCpu
%undef pXState
ENDPROC   cpumR0SaveGuestRestoreHostFPUState


;;
; Restores the host's FPU/SSE/AVX state from pCpumCpu->Host.
;
; @returns  0
; @param    pCpumCpu  x86:[esp+4] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0RestoreHostFPUState
        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
        mov     r11, rcx
 %else
        mov     r11, rdi
 %endif
 %define pCpumCpu   r11
 %define pXState    r10
%else
        push    ebx
        push    esi
        mov     ebx, dword [esp + 4]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        ;
        ; Restore FPU if guest has used it.
        ;
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USED_FPU
        jz short .fpu_not_used

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      .legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif

%ifdef RT_ARCH_AMD64
        o64 fxrstor [pXState]
%else
        fxrstor [pXState]
%endif

.done:
        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
        popf

.fpu_not_used:
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        xor     eax, eax
        ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
        o64 fxrstor [pXState]
        jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
        dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
%undef pCpumCPu
%undef pXState
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

