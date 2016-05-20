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



BEGINCODE


;;
; Saves the host FPU/SSE/AVX state.
;
; @returns  VINF_SUCCESS (0) in EAX
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
%ifdef VBOX_WITH_KERNEL_USING_XMM
 %ifdef IN_RING0
        movaps  xmm0, xmm0              ; Make 100% sure it's used before we save it or mess with CR0/XCR0.
 %endif
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
ENDPROC   cpumRZSaveHostFPUState


;;
; Saves the guest FPU/SSE/AVX state.
;
; @param    pCpumCpu  x86:[ebp+8] gcc:rdi msc:rcx     CPUMCPU pointer
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
        SAVE_CR0_CLEAR_FPU_TRAPS xCX, xAX ; xCX is now old CR0 value, don't use!


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

        RESTORE_CR0 xCX
        and     dword [pCpumCpu + CPUMCPU.fUseFlags], ~CPUM_USED_FPU_GUEST

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
%endif ; !VBOX_WITH_KERNEL_USING_XMM

        leave
        ret
ENDPROC   cpumRZSaveGuestSseRegisters

