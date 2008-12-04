; $Id$
;; @file
; CPUM - Guest Context Assembly Routines.
;

;
; Copyright (C) 2006-2007 Sun Microsystems, Inc.
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/vm.mac"
%include "VBox/err.mac"
%include "VBox/stam.mac"
%include "CPUMInternal.mac"
%include "VBox/x86.mac"
%include "VBox/cpum.mac"

%ifdef IN_RING3
 %error "The jump table doesn't link on leopard."
%endif

BEGINCODE


;;
; Restores the host's FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC CPUMR0SaveGuestRestoreHostFPUState
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
    jz short gth_fpu_no

    mov     xAX, cr0
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX

    fxsave  [xDX + CPUMCPU.Guest.fpu]
    fxrstor [xDX + CPUMCPU.Host.fpu]

    mov     cr0, xCX                    ; and restore old CR0 again
    and     dword [xDX + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
gth_fpu_no:
    xor     eax, eax
    ret
ENDPROC   CPUMR0SaveGuestRestoreHostFPUState

;;
; Sets the host's FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC CPUMR0RestoreHostFPUState
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
    jz short gth_fpu_no_2

    mov     xAX, cr0
    mov     xCX, xAX                    ; save old CR0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX

    fxrstor [xDX + CPUMCPU.Host.fpu]

    mov     cr0, xCX                    ; and restore old CR0 again
    and     dword [xDX + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
gth_fpu_no_2:
    xor     eax, eax
    ret
ENDPROC   CPUMR0RestoreHostFPUState

;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMLoadFPU
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    fxrstor [xDX + CPUMCTX.fpu]
    ret
ENDPROC     CPUMLoadFPU


;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMSaveFPU
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    fxsave  [xDX + CPUMCTX.fpu]
    ret
ENDPROC CPUMSaveFPU


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMLoadXMM
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    movdqa  xmm0, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*0]
    movdqa  xmm1, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*1]
    movdqa  xmm2, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*2]
    movdqa  xmm3, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*3]
    movdqa  xmm4, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*4]
    movdqa  xmm5, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*5]
    movdqa  xmm6, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*6]
    movdqa  xmm7, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*7]

%ifdef RT_ARCH_AMD64
    test qword [xDX + CPUMCTX.msrEFER], MSR_K6_EFER_LMA
    jz CPUMLoadXMM_done

    movdqa  xmm8, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8]
    movdqa  xmm9, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9]
    movdqa  xmm10, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10]
    movdqa  xmm11, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11]
    movdqa  xmm12, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12]
    movdqa  xmm13, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13]
    movdqa  xmm14, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14]
    movdqa  xmm15, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15]
CPUMLoadXMM_done:
%endif

    ret
ENDPROC     CPUMLoadXMM


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMSaveXMM
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*0], xmm0
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*1], xmm1
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*2], xmm2
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*3], xmm3
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*4], xmm4
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*5], xmm5
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*6], xmm6
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*7], xmm7

%ifdef RT_ARCH_AMD64
    test qword [xDX + CPUMCTX.msrEFER], MSR_K6_EFER_LMA
    jz CPUMSaveXMM_done

    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8], xmm8
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9], xmm9
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10], xmm10
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11], xmm11
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12], xmm12
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13], xmm13
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14], xmm14
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15], xmm15

CPUMSaveXMM_done:
%endif
    ret
ENDPROC     CPUMSaveXMM


;;
; Set the FPU control word; clearing exceptions first
;
; @param  u16FCW    x86:[esp+4] GCC:rdi MSC:rcx     New FPU control word
align 16
BEGINPROC CPUMR0SetFCW
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xAX, rcx
 %else
    mov     xAX, rdi
 %endif
%else
    mov     xAX, dword [esp + 4]
%endif
    fnclex
    push    xAX
    fldcw   [xSP]
    pop     xAX
    ret
ENDPROC   CPUMR0SetFCW


;;
; Get the FPU control word
;
align 16
BEGINPROC CPUMR0GetFCW
    fnstcw  [xSP - 8]
    mov     ax, word [xSP - 8]
    ret
ENDPROC   CPUMR0GetFCW


;;
; Set the MXCSR;
;
; @param  u32MXCSR    x86:[esp+4] GCC:rdi MSC:rcx     New MXCSR
align 16
BEGINPROC CPUMR0SetMXCSR
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xAX, rcx
 %else
    mov     xAX, rdi
 %endif
%else
    mov     xAX, dword [esp + 4]
%endif
    push    xAX
    ldmxcsr [xSP]
    pop     xAX
    ret
ENDPROC   CPUMR0SetMXCSR


;;
; Get the MXCSR
;
align 16
BEGINPROC CPUMR0GetMXCSR
    stmxcsr [xSP - 8]
    mov     eax, dword [xSP - 8]
    ret
ENDPROC   CPUMR0GetMXCSR
