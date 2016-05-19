 ; $Id$
;; @file
; CPUM - Ring-0 Assembly Routines (supporting HM and IEM).
;

;
; Copyright (C) 2006-2016 Oracle Corporation
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
%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"
%include "VBox/asmdefs.mac"
%include "VBox/vmm/vm.mac"
%include "VBox/err.mac"
%include "VBox/vmm/stam.mac"
%include "CPUMInternal.mac"
%include "iprt/x86.mac"
%include "VBox/vmm/cpum.mac"


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
; @param    pXState     Define for the register containing the extended state pointer.
;
%macro CPUMR0_SAVE_HOST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Host.fXStateMask]

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
; @param    pXState     Define for the register containing the extended state pointer.
;
%macro CPUMR0_LOAD_HOST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Host.fXStateMask]

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
; @param    pXState     Define for the register containing the extended state pointer.
;
%macro CPUMR0_SAVE_GUEST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Guest.fXStateMask]

        ;
        ; XSAVE or FXSAVE?
        ;
        or      eax, eax
        jz      %%guest_fxsave

        ; XSAVE
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
%ifdef VBOX_WITH_KERNEL_USING_XMM
        and     eax, ~CPUM_VOLATILE_XSAVE_GUEST_COMPONENTS ; Already saved in HMR0A.asm.
%endif
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
; @param    pXState     Define for the register containing the extended state pointer.
;
%macro CPUMR0_LOAD_GUEST 0
        ;
        ; Load a couple of registers we'll use later in all branches.
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        mov     eax, [pCpumCpu + CPUMCPU.Guest.fXStateMask]

        ;
        ; XRSTOR or FXRSTOR?
        ;
        or      eax, eax
        jz      %%guest_fxrstor

        ; XRSTOR
        mov     edx, [pCpumCpu + CPUMCPU.Guest.fXStateMask + 4]
%ifdef VBOX_WITH_KERNEL_USING_XMM
        and     eax, ~CPUM_VOLATILE_XSAVE_GUEST_COMPONENTS ; Will be loaded by HMR0A.asm.
%endif
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
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostRestoreGuestFPUState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

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
        mov     ebx, dword [ebp + 8]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.

%ifdef VBOX_WITH_KERNEL_USING_XMM
        movaps  xmm0, xmm0              ; Make 100% sure it's used before we save it or mess with CR0/XCR0.
%endif
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        ;
        ; Save the host state.
        ;
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USED_FPU_HOST
        jnz     .already_saved_host
        CPUMR0_SAVE_HOST
%ifdef VBOX_WITH_KERNEL_USING_XMM
        jmp     .load_guest
%endif
.already_saved_host:
%ifdef VBOX_WITH_KERNEL_USING_XMM
        ; If we didn't save the host state, we must save the non-volatile XMM registers.
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 060h], xmm6
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 070h], xmm7
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 080h], xmm8
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 090h], xmm9
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0a0h], xmm10
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0b0h], xmm11
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0c0h], xmm12
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0d0h], xmm13
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0e0h], xmm14
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0f0h], xmm15

        ;
        ; Load the guest state.
        ;
.load_guest:
%endif
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

        ;; @todo Save CR0 + XCR0 bits related to FPU, SSE and AVX*, leaving these register sets accessible to IEM.
        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_SINCE_REM | CPUM_USED_FPU_HOST)
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        leave
        ret
ENDPROC   cpumR0SaveHostRestoreGuestFPUState


;;
; Saves the host FPU/SSE/AVX state.
;
; @returns  VINF_SUCCESS (0) in EAX
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveHostFPUState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

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
        mov     ebx, dword [ebp + 8]
 %define pCpumCpu ebx
 %define pXState  esi
%endif

        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
%ifdef VBOX_WITH_KERNEL_USING_XMM
        movaps  xmm0, xmm0              ; Make 100% sure it's used before we save it or mess with CR0/XCR0.
%endif
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        CPUMR0_SAVE_HOST
        ;; @todo Save CR0 + XCR0 bits related to FPU, SSE and AVX*, leaving these register sets accessible to IEM.

        RESTORE_CR0 xCX
        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU_HOST | CPUM_USED_FPU_SINCE_REM) ; Latter is not necessarily true, but normally yes.
        popf

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        leave
        ret
%undef pCpumCpu
%undef pXState
ENDPROC   cpumR0SaveHostFPUState


;;
; Saves the guest FPU/SSE/AVX state and restores the host FPU/SSE/AVX state.
;
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0SaveGuestRestoreHostFPUState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

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
        mov     ebx, dword [ebp + 8]
 %define pCpumCpu   ebx
 %define pXState    esi
%endif
        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!


 %ifdef VBOX_WITH_KERNEL_USING_XMM
        ;
        ; Copy non-volatile XMM registers to the host state so we can use
        ; them while saving the guest state (we've gotta do this anyway).
        ;
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 060h], xmm6
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 070h], xmm7
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 080h], xmm8
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 090h], xmm9
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0a0h], xmm10
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0b0h], xmm11
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0c0h], xmm12
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0d0h], xmm13
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0e0h], xmm14
        movdqa  [pXState + XMM_OFF_IN_X86FXSTATE + 0f0h], xmm15
 %endif

        ;
        ; Save the guest state if necessary.
        ;
        test    dword [pCpumCpu + CPUMCPU.fUseFlags], CPUM_USED_FPU_GUEST
        jz      .load_only_host

 %ifdef VBOX_WITH_KERNEL_USING_XMM
        ; Load the guest XMM register values we already saved in HMR0VMXStartVMWrapXMM.
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        movdqa  xmm0,  [pXState + XMM_OFF_IN_X86FXSTATE + 000h]
        movdqa  xmm1,  [pXState + XMM_OFF_IN_X86FXSTATE + 010h]
        movdqa  xmm2,  [pXState + XMM_OFF_IN_X86FXSTATE + 020h]
        movdqa  xmm3,  [pXState + XMM_OFF_IN_X86FXSTATE + 030h]
        movdqa  xmm4,  [pXState + XMM_OFF_IN_X86FXSTATE + 040h]
        movdqa  xmm5,  [pXState + XMM_OFF_IN_X86FXSTATE + 050h]
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
        CPUMR0_SAVE_GUEST

        ;
        ; Load the host state.
        ;
.load_only_host:
        CPUMR0_LOAD_HOST

        ;; @todo Restore CR0 + XCR0 bits related to FPU, SSE and AVX* (for IEM).
        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~(CPUM_USED_FPU_GUEST | CPUM_USED_FPU_HOST)

        popf
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        leave
        ret
%undef pCpumCpu
%undef pXState
ENDPROC   cpumR0SaveGuestRestoreHostFPUState


%if ARCH_BITS == 32
 %ifdef VBOX_WITH_64_BITS_GUESTS
;;
; Restores the host's FPU/SSE/AVX state from pCpumCpu->Host.
;
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumR0RestoreHostFPUState
        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        mov     ebx, dword [ebp + 8]
  %define pCpumCpu ebx
  %define pXState  esi

        ;
        ; Restore host CPU state.
        ;
        pushf                           ; The darwin kernel can get upset or upset things if an
        cli                             ; interrupt occurs while we're doing fxsave/fxrstor/cr0.
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!

        CPUMR0_LOAD_HOST

        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU_HOST
        popf

        pop     esi
        pop     ebx
        leave
        ret
  %undef pCpumCPu
  %undef pXState
ENDPROC   cpumR0RestoreHostFPUState
 %endif ; VBOX_WITH_64_BITS_GUESTS
%endif  ; ARCH_BITS == 32

