 ; $Id$
;; @file
; CPUM - Raw-mode and Ring-0 Context Assembly Routines.
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
%include "VBox/asmdefs.mac"
%include "CPUMInternal.mac"
%include "iprt/x86.mac"
%include "VBox/vmm/cpum.mac"
%include "VBox/err.mac"



BEGINCODE


;;
; Saves the host FPU/SSE/AVX state.
;
; Will return with CR0.EM and CR0.TS cleared!  This is the normal state in
; ring-0, whereas in raw-mode the caller will probably set VMCPU_FF_CPUM to
; re-evaluate the situation before executing more guest code.
;
; @returns  VINF_SUCCESS (0) or VINF_CPUM_HOST_CR0_MODIFIED. (EAX)
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumRZSaveHostFPUState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
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

%ifndef CPUM_CAN_USE_FPU_IN_R0
        ;
        ; In raw-mode context and on systems where the kernel doesn't necessarily
        ; allow us to use the FPU in ring-0 context, we have to disable FPU traps
        ; before doing fxsave/xsave here.  (xCX is 0 if no CR0 was necessary.)  We
        ; leave it like that so IEM can use the FPU/SSE/AVX host CPU features directly.
        ;
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX               ; xCX must be preserved!
        ;; @todo What about XCR0?
 %ifdef IN_RING0
        mov     [pCpumCpu + CPUMCPU.Host.cr0Fpu], xCX
 %endif
%endif
        ;
        ; Save the host state (xsave/fxsave will cause thread FPU state to be
        ; loaded on systems where we are allowed to use it in ring-0.
        ;
        CPUMR0_SAVE_HOST

        or      dword [pCpumCpu + CPUMCPU.fUseFlags], (CPUM_USED_FPU_HOST | CPUM_USED_FPU_SINCE_REM) ; Latter is not necessarily true, but normally yes.
        popf

%ifndef CPUM_CAN_USE_FPU_IN_R0
        ; Figure the return code.
        test    ecx, ecx
        jnz     .modified_cr0
%endif
        xor     eax, eax
.return:

%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        leave
        ret

%ifndef CPUM_CAN_USE_FPU_IN_R0
.modified_cr0:
        mov     eax, VINF_CPUM_HOST_CR0_MODIFIED
        jmp     .return
%endif
%undef pCpumCpu
%undef pXState
ENDPROC   cpumRZSaveHostFPUState


;;
; Saves the guest FPU/SSE/AVX state.
;
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
; @param    fLeaveFpuAccessible  x86:[ebp+c] gcc:sil msc:dl      Whether to restore CR0 and XCR0 on
;                                                                the way out. Only really applicable to RC.
;
align 16
BEGINPROC cpumRZSaveGuestFpuState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

        ;
        ; Prologue - xAX+xDX must be free for XSAVE/XRSTOR input.
        ;
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
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

 %ifdef IN_RC
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX must be preserved until CR0 is restored!
 %endif

 %ifndef VBOX_WITH_KERNEL_USING_XMM
        CPUMR0_SAVE_GUEST
 %else
        ;
        ; The XMM0..XMM15 registers have been saved already.  We exploit the
        ; host state here to temporarly save the non-volatile XMM registers,
        ; so we can load the guest ones while saving.  This is safe.
        ;

        ; Save caller's XMM registers.
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        movdqa  [pXState + X86FXSTATE.xmm6 ], xmm6
        movdqa  [pXState + X86FXSTATE.xmm7 ], xmm7
        movdqa  [pXState + X86FXSTATE.xmm8 ], xmm8
        movdqa  [pXState + X86FXSTATE.xmm9 ], xmm9
        movdqa  [pXState + X86FXSTATE.xmm10], xmm10
        movdqa  [pXState + X86FXSTATE.xmm11], xmm11
        movdqa  [pXState + X86FXSTATE.xmm12], xmm12
        movdqa  [pXState + X86FXSTATE.xmm13], xmm13
        movdqa  [pXState + X86FXSTATE.xmm14], xmm14
        movdqa  [pXState + X86FXSTATE.xmm15], xmm15

        ; Load the guest XMM register values we already saved in HMR0VMXStartVMWrapXMM.
        mov     pXState, [pCpumCpu + CPUMCPU.Guest.pXStateR0]
        movdqa  xmm0,  [pXState + X86FXSTATE.xmm0]
        movdqa  xmm1,  [pXState + X86FXSTATE.xmm1]
        movdqa  xmm2,  [pXState + X86FXSTATE.xmm2]
        movdqa  xmm3,  [pXState + X86FXSTATE.xmm3]
        movdqa  xmm4,  [pXState + X86FXSTATE.xmm4]
        movdqa  xmm5,  [pXState + X86FXSTATE.xmm5]
        movdqa  xmm6,  [pXState + X86FXSTATE.xmm6]
        movdqa  xmm7,  [pXState + X86FXSTATE.xmm7]
        movdqa  xmm8,  [pXState + X86FXSTATE.xmm8]
        movdqa  xmm9,  [pXState + X86FXSTATE.xmm9]
        movdqa  xmm10, [pXState + X86FXSTATE.xmm10]
        movdqa  xmm11, [pXState + X86FXSTATE.xmm11]
        movdqa  xmm12, [pXState + X86FXSTATE.xmm12]
        movdqa  xmm13, [pXState + X86FXSTATE.xmm13]
        movdqa  xmm14, [pXState + X86FXSTATE.xmm14]
        movdqa  xmm15, [pXState + X86FXSTATE.xmm15]

        CPUMR0_SAVE_GUEST

        ; Restore caller's XMM registers.
        mov     pXState, [pCpumCpu + CPUMCPU.Host.pXStateR0]
        movdqa  xmm6,  [pXState + X86FXSTATE.xmm6 ]
        movdqa  xmm7,  [pXState + X86FXSTATE.xmm7 ]
        movdqa  xmm8,  [pXState + X86FXSTATE.xmm8 ]
        movdqa  xmm9,  [pXState + X86FXSTATE.xmm9 ]
        movdqa  xmm10, [pXState + X86FXSTATE.xmm10]
        movdqa  xmm11, [pXState + X86FXSTATE.xmm11]
        movdqa  xmm12, [pXState + X86FXSTATE.xmm12]
        movdqa  xmm13, [pXState + X86FXSTATE.xmm13]
        movdqa  xmm14, [pXState + X86FXSTATE.xmm14]
        movdqa  xmm15, [pXState + X86FXSTATE.xmm15]

 %endif

        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU_GUEST
 %ifdef IN_RC
        test    byte [ebp + 0ch], 1     ; fLeaveFpuAccessible
        jz      .no_cr0_restore
        RESTORE_CR0 xCX
.no_cr0_restore:
 %endif
        popf
%ifdef RT_ARCH_X86
        pop     esi
        pop     ebx
%endif
        leave
        ret
%undef pCpumCpu
%undef pXState
ENDPROC   cpumRZSaveGuestFpuState


;;
; Saves the guest XMM0..15 registers.
;
; The purpose is to actualize the register state for read-only use, so CR0 is
; restored in raw-mode context (so, the FPU/SSE/AVX CPU features can be
; inaccessible upon return).
;
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
;
align 16
BEGINPROC cpumRZSaveGuestSseRegisters
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

%ifndef VBOX_WITH_KERNEL_USING_XMM
        ;
        ; Load xCX with the guest pXStateR0.
        ;
 %ifdef ASM_CALL64_GCC
        mov     xCX, rdi
 %elifdef RT_ARCH_X86
        mov     xCX, dword [ebp + 8]
 %endif
 %ifdef IN_RING0
        mov     xCX, [xCX + CPUMCPU.Guest.pXStateR0]
 %elifdef IN_RC
        mov     xCX, [xCX + CPUMCPU.Guest.pXStateRC]
 %else
  %error "Invalid context!"
 %endif

 %ifdef IN_RC
        ; Temporarily grant access to the SSE state. xDX must be preserved until CR0 is restored!
        SAVE_CR0_CLEAR_FPU_TRAPS xDX, xAX
 %endif

        ;
        ; Do the job.
        ;
        movdqa  [xCX + X86FXSTATE.xmm0 ], xmm0
        movdqa  [xCX + X86FXSTATE.xmm1 ], xmm1
        movdqa  [xCX + X86FXSTATE.xmm2 ], xmm2
        movdqa  [xCX + X86FXSTATE.xmm3 ], xmm3
        movdqa  [xCX + X86FXSTATE.xmm4 ], xmm4
        movdqa  [xCX + X86FXSTATE.xmm5 ], xmm5
        movdqa  [xCX + X86FXSTATE.xmm6 ], xmm6
        movdqa  [xCX + X86FXSTATE.xmm7 ], xmm7
 %if ARCH_BITS == 64
        movdqa  [xCX + X86FXSTATE.xmm8 ], xmm8
        movdqa  [xCX + X86FXSTATE.xmm9 ], xmm9
        movdqa  [xCX + X86FXSTATE.xmm10], xmm10
        movdqa  [xCX + X86FXSTATE.xmm11], xmm11
        movdqa  [xCX + X86FXSTATE.xmm12], xmm12
        movdqa  [xCX + X86FXSTATE.xmm13], xmm13
        movdqa  [xCX + X86FXSTATE.xmm14], xmm14
        movdqa  [xCX + X86FXSTATE.xmm15], xmm15
 %endif

 %ifdef IN_RC
        RESTORE_CR0 xDX                 ; Restore CR0 if we changed it above.
 %endif

%endif ; !VBOX_WITH_KERNEL_USING_XMM

        leave
        ret
ENDPROC   cpumRZSaveGuestSseRegisters

