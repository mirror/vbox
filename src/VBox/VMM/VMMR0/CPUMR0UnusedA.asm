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



;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] gcc:rdi msc:rcx     CPUMCTX pointer
;
; @remarks Used by the disabled CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE code.
;
align 16
BEGINPROC   cpumR0LoadFPU
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
.done:
    ret
ENDPROC     cpumR0LoadFPU


;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] gcc:rdi msc:rcx     CPUMCTX pointer
;
; @remarks Used by the disabled CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE code.
;
align 16
BEGINPROC   cpumR0SaveFPU
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
.done:
    ret
ENDPROC cpumR0SaveFPU


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] gcc:rdi msc:rcx     CPUMCTX pointer
;
; @remarks  Used by the disabled CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE code.
;
align 16
BEGINPROC   cpumR0LoadXMM
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
    jz .done

    movdqa  xmm8, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8]
    movdqa  xmm9, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9]
    movdqa  xmm10, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10]
    movdqa  xmm11, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11]
    movdqa  xmm12, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12]
    movdqa  xmm13, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13]
    movdqa  xmm14, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14]
    movdqa  xmm15, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15]
%endif

.done:
    ret
ENDPROC     cpumR0LoadXMM


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] gcc:rdi msc:rcx     CPUMCTX pointer
;
; @remarks  Used by the disabled CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE code.
;
align 16
BEGINPROC   cpumR0SaveXMM
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
    jz .done

    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8], xmm8
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9], xmm9
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10], xmm10
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11], xmm11
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12], xmm12
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13], xmm13
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14], xmm14
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15], xmm15
%endif

.done:
    ret
ENDPROC     cpumR0SaveXMM


;;
; Set the FPU control word; clearing exceptions first
;
; @param  u16FCW    x86:[esp+4] gcc:rdi msc:rcx     New FPU control word
align 16
BEGINPROC cpumR0SetFCW
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
ENDPROC   cpumR0SetFCW


;;
; Get the FPU control word
;
align 16
BEGINPROC cpumR0GetFCW
    fnstcw  [xSP - 8]
    mov     ax, word [xSP - 8]
    ret
ENDPROC   cpumR0GetFCW


;;
; Set the MXCSR;
;
; @param  u32MXCSR    x86:[esp+4] gcc:rdi msc:rcx     New MXCSR
align 16
BEGINPROC cpumR0SetMXCSR
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
ENDPROC   cpumR0SetMXCSR


;;
; Get the MXCSR
;
align 16
BEGINPROC cpumR0GetMXCSR
    stmxcsr [xSP - 8]
    mov     eax, dword [xSP - 8]
    ret
ENDPROC   cpumR0GetMXCSR

