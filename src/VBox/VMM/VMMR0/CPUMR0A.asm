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

;; For numeric expressions
%ifdef RT_ARCH_AMD64
 %define CPUMR0_IS_AMD64        1
%else
 %define CPUMR0_IS_AMD64        0
%endif


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
; Saves the host state.
;
; @uses     rax, rdx
; @param    pCpumCpu    Define for the register containing the CPUMCPU pointer.
; @param    pXState     Define for the regsiter containing the extended state pointer.
;
%macro CPUMR0_SAVE_HOST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Host.fXStateMask]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        ; The joy of 32-bit darwin kernels that runs the CPU in 64-bit mode.
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      %%host_legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      %%host_sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
BITS 64
%%host_sixtyfourbit_mode:
        or      eax, eax
        jz      %%host_sixtyfourbit_fxsave

        ; XSAVE
        mov     edx, [pCpumCpu + CPUMCPU.Host.fXStateMask + 4]
        o64 xsave [pXState]
        jmp     %%host_sixtyfourbit_done

        ; FXSAVE
%%host_sixtyfourbit_fxsave:
        o64 fxsave [pXState]

%%host_sixtyfourbit_done:
        jmp far [%%host_fpret wrt rip]
%%host_fpret:                           ; 16:32 Pointer to %%host_done.
        dd      %%host_done, NAME(SUPR0AbsKernelCS)
BITS 32

%%host_legacy_mode:
%endif

        ;
        ; XSAVE or FXSAVE?
        ;
        or      eax, eax
        jz      %%host_fxsave

        ; XSAVE
        mov     edx, [pCpumCpu + CPUMCPU.Host.fXStateMask + 4]
%ifdef RT_ARCH_AMD64
        o64 xsave [pXState]
%else
        xsave   [pXState]
%endif
        jmp     %%host_done

        ; FXSAVE
%%host_fxsave:
%ifdef RT_ARCH_AMD64
        o64 fxsave [pXState]            ; Use explicit REX prefix. See @bugref{6398}.
%else
        fxsave  [pXState]
%endif

%%host_done:
%endmacro ; CPUMR0_SAVE_HOST


;;
; Loads the host state.
;
; @uses     rax, rdx
; @param    pCpumCpu    Define for the register containing the CPUMCPU pointer.
; @param    pXState     Define for the regsiter containing the extended state pointer.
;
%macro CPUMR0_LOAD_HOST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Host.fXStateMask]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        ; The joy of 32-bit darwin kernels that runs the CPU in 64-bit mode.
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      %%host_legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      %%host_sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
BITS 64
%%host_sixtyfourbit_mode:
        or      eax, eax
        jz      %%host_sixtyfourbit_fxrstor

        ; XRSTOR
        mov     edx, [pCpumCpu + CPUMCPU.Host.fXStateMask + 4]
        o64 xrstor [pXState]
        jmp     %%host_sixtyfourbit_done

        ; FXRSTOR
%%host_sixtyfourbit_fxrstor:
        o64 fxrstor [pXState]

%%host_sixtyfourbit_done:
        jmp far [%%host_fpret wrt rip]
%%host_fpret:                           ; 16:32 Pointer to %%host_done.
        dd      %%host_done, NAME(SUPR0AbsKernelCS)
BITS 32

%%host_legacy_mode:
%endif

        ;
        ; XRSTOR or FXRSTOR?
        ;
        or      eax, eax
        jz      %%host_fxrstor

        ; XRSTOR
        mov     edx, [pCpumCpu + CPUMCPU.Host.fXStateMask + 4]
%ifdef RT_ARCH_AMD64
        o64 xrstor [pXState]
%else
        xrstor  [pXState]
%endif
        jmp     %%host_done

        ; FXRSTOR
%%host_fxrstor:
%ifdef RT_ARCH_AMD64
        o64 fxrstor [pXState]           ; Use explicit REX prefix. See @bugref{6398}.
%else
        fxrstor [pXState]
%endif

%%host_done:
%endmacro ; CPUMR0_LOAD_HOST



;; Macro for FXSAVE for the guest FPU but tries to figure out whether to
;  save the 32-bit FPU state or 64-bit FPU state.
;
; @param    %1      Pointer to CPUMCPU.
; @param    %2      Pointer to XState.
; @param    %3      Force AMD64
; @uses     xAX, xDX, EFLAGS, 20h of stack.
;
%macro SAVE_32_OR_64_FPU 3
%if CPUMR0_IS_AMD64 || %3
        ; Save the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jnz     short %%save_long_mode_guest
%endif
        fxsave  [pXState]
%if CPUMR0_IS_AMD64 || %3
        jmp     %%save_done_32bit_cs_ds

%%save_long_mode_guest:
        o64 fxsave [pXState]

        xor     edx, edx
        cmp     dword [pXState + CS_OFF_IN_X86FXSTATE], 0
        jne     short %%save_done

        sub     rsp, 20h                ; Only need 1ch bytes but keep stack aligned otherwise we #GP(0).
        fnstenv [rsp]
        movzx   eax, word [rsp + 10h]
        mov     [pXState + CS_OFF_IN_X86FXSTATE], eax
        movzx   eax, word [rsp + 18h]
        add     rsp, 20h
        mov     [pXState + DS_OFF_IN_X86FXSTATE], eax
%endif
%%save_done_32bit_cs_ds:
        mov     edx, X86_FXSTATE_RSVD_32BIT_MAGIC
%%save_done:
        mov     dword [pXState + X86_OFF_FXSTATE_RSVD], edx
%endmacro ; SAVE_32_OR_64_FPU


;;
; Save the guest state.
;
; @uses     rax, rdx
; @param    pCpumCpu    Define for the register containing the CPUMCPU pointer.
; @param    pXState     Define for the regsiter containing the extended state pointer.
;
%macro CPUMR0_SAVE_GUEST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Guest.fXStateMask]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        ; The joy of 32-bit darwin kernels that runs the CPU in 64-bit mode.
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      %%guest_legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      %%guest_sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
BITS 64
%%guest_sixtyfourbit_mode:
        or      eax, eax
        jz      %%guest_sixtyfourbit_fxsave

        ; XSAVE
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
        o64 xsave [pXState]
        jmp     %%guest_sixtyfourbit_done

        ; FXSAVE
%%guest_sixtyfourbit_fxsave:
        SAVE_32_OR_64_FPU pCpumCpu, pXState, 1

%%guest_sixtyfourbit_done:
        jmp far [%%guest_fpret wrt rip]
%%guest_fpret:                          ; 16:32 Pointer to %%guest_done.
        dd      %%guest_done, NAME(SUPR0AbsKernelCS)
BITS 32

%%guest_legacy_mode:
%endif

        ;
        ; XSAVE or FXSAVE?
        ;
        or      eax, eax
        jz      %%guest_fxsave

        ; XSAVE
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
%ifdef RT_ARCH_AMD64
        o64 xsave [pXState]
%else
        xsave   [pXState]
%endif
        jmp     %%guest_done

        ; FXSAVE
%%guest_fxsave:
        SAVE_32_OR_64_FPU pCpumCpu, pXState, 0

%%guest_done:
%endmacro ; CPUMR0_SAVE_GUEST


;;
; Wrapper for selecting 32-bit or 64-bit FXRSTOR according to what SAVE_32_OR_64_FPU did.
;
; @param    %1      Pointer to CPUMCPU.
; @param    %2      Pointer to XState.
; @param    %3      Force AMD64.
; @uses     xAX, xDX, EFLAGS
;
%macro RESTORE_32_OR_64_FPU 3
%if CPUMR0_IS_AMD64 || %3
        ; Restore the guest FPU (32-bit or 64-bit), preserves existing broken state. See @bugref{7138}.
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USE_SUPPORTS_LONGMODE
        jz      %%restore_32bit_fpu
        cmp     dword [pXState + X86_OFF_FXSTATE_RSVD], X86_FXSTATE_RSVD_32BIT_MAGIC
        jne     short %%restore_64bit_fpu
%%restore_32bit_fpu:
%endif
        fxrstor [pXState]
%if CPUMR0_IS_AMD64 || %3
        ; TODO: Restore XMM8-XMM15!
        jmp     short %%restore_fpu_done
%%restore_64bit_fpu:
        o64 fxrstor [pXState]
%%restore_fpu_done:
%endif
%endmacro ; RESTORE_32_OR_64_FPU


;;
; Loads the guest state.
;
; @uses     rax, rdx
; @param    pCpumCpu    Define for the register containing the CPUMCPU pointer.
; @param    pXState     Define for the regsiter containing the extended state pointer.
;
%macro CPUMR0_LOAD_GUEST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Guest.fXStateMask]

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        ; The joy of 32-bit darwin kernels that runs the CPU in 64-bit mode.
        cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
        jz      %%guest_legacy_mode
        db      0xea                    ; jmp far .sixtyfourbit_mode
        dd      %%guest_sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
BITS 64
%%guest_sixtyfourbit_mode:
        or      eax, eax
        jz      %%guest_sixtyfourbit_fxrstor

        ; XRSTOR
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
        o64 xrstor [pXState]
        jmp     %%guest_sixtyfourbit_done

        ; FXRSTOR
%%guest_sixtyfourbit_fxrstor:
        RESTORE_32_OR_64_FPU pCpumCpu, pXState, 1

%%guest_sixtyfourbit_done:
        jmp far [%%guest_fpret wrt rip]
%%guest_fpret:                          ; 16:32 Pointer to %%guest_done.
        dd      %%guest_done, NAME(SUPR0AbsKernelCS)
BITS 32

%%guest_legacy_mode:
%endif

        ;
        ; XRSTOR or FXRSTOR?
        ;
        or      eax, eax
        jz      %%guest_fxrstor

        ; XRSTOR
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
%ifdef RT_ARCH_AMD64
        o64 xrstor [pXState]
%else
        xrstor  [pXState]
%endif
        jmp     %%guest_done

        ; FXRSTOR
%%guest_fxrstor:
        RESTORE_32_OR_64_FPU pCpumCpu, pXState, 0

%%guest_done:
%endmacro ; CPUMR0_LOAD_GUEST


;;
; Saves the host FPU/SSE/AVX state and restores the guest FPU/SSE/AVX state.
;
; @returns  0
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
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
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        mov     ebx, dword [ebp + 8]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        CPUMR0_SAVE_HOST
        CPUMR0_LOAD_GUEST

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

        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
        leave
%endif
        xor     eax, eax
        ret
ENDPROC   cpumR0SaveHostRestoreGuestFPUState


%ifndef RT_ARCH_AMD64
%ifdef  VBOX_WITH_64_BITS_GUESTS
%ifndef VBOX_WITH_HYBRID_32BIT_KERNEL
;;
; Saves the host FPU/SSE/AVX state.
;
; @returns  VINF_SUCCESS (0) in EAX
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
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
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        mov     ebx, dword [ebp + 8]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        CPUMR0_SAVE_HOST

        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
        leave
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
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
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
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        mov     ebx, dword [ebp + 8]
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

        CPUMR0_SAVE_GUEST
        CPUMR0_LOAD_HOST

        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
        popf

.fpu_not_used:
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
        leave
%endif
        xor     eax, eax
        ret
%undef pCpumCpu
%undef pXState
ENDPROC   cpumR0SaveGuestRestoreHostFPUState


;;
; Restores the host's FPU/SSE/AVX state from pCpumCpu->Host.
;
; @returns  0
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
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
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        mov     ebx, dword [ebp + 8]
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

        CPUMR0_LOAD_HOST

        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
        popf

.fpu_not_used:
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
        leave
%endif
        xor     eax, eax
        ret
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

