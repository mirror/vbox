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
; Restores the host's FPU/XMM state
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

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USED_FPU
    jz short .fpu_not_used

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

    fxsave  [xDX + CPUMCPU.Guest.fpu]
    fxrstor [xDX + CPUMCPU.Host.fpu]

.done:
    mov     cr0, xCX                    ; and restore old CR0 again
    and     dword [xDX + CPUMCPU.fUseFlags], ~CPUM_USED_FPU
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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

    fxrstor [xDX + CPUMCTX.fpu]
.done:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    fxrstor [rdx + CPUMCTX.fpu]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    fxsave  [xDX + CPUMCTX.fpu]
.done:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    fxsave  [rdx + CPUMCTX.fpu]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

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

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh

    movdqa  xmm0, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*0]
    movdqa  xmm1, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*1]
    movdqa  xmm2, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*2]
    movdqa  xmm3, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*3]
    movdqa  xmm4, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*4]
    movdqa  xmm5, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*5]
    movdqa  xmm6, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*6]
    movdqa  xmm7, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*7]

    test qword [rdx + CPUMCTX.msrEFER], MSR_K6_EFER_LMA
    jz .sixtyfourbit_done

    movdqa  xmm8,  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8]
    movdqa  xmm9,  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9]
    movdqa  xmm10, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10]
    movdqa  xmm11, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11]
    movdqa  xmm12, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12]
    movdqa  xmm13, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13]
    movdqa  xmm14, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14]
    movdqa  xmm15, [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15]
.sixtyfourbit_done:
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

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

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh

    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*0], xmm0
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*1], xmm1
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*2], xmm2
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*3], xmm3
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*4], xmm4
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*5], xmm5
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*6], xmm6
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*7], xmm7

    test qword [rdx + CPUMCTX.msrEFER], MSR_K6_EFER_LMA
    jz .sixtyfourbit_done

    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8], xmm8
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9], xmm9
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10], xmm10
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11], xmm11
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12], xmm12
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13], xmm13
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14], xmm14
    movdqa  [rdx + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15], xmm15

.sixtyfourbit_done:
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .done, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC     CPUMSaveXMM


;;
; Set the FPU control word; clearing exceptions first
;
; @param  u16FCW    x86:[esp+4] GCC:rdi MSC:rcx     New FPU control word
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
; @param  u32MXCSR    x86:[esp+4] GCC:rdi MSC:rcx     New MXCSR
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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
%endif

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
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fCPUMIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
%endif

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
