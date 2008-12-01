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

;
; Enables write protection of Hypervisor memory pages.
; !note! Must be commented out for Trap8 debug handler.
;
%define ENABLE_WRITE_PROTECTION 1

BEGINCODE


;;
; Handles lazy FPU saving and restoring.
;
; This handler will implement lazy fpu (sse/mmx/stuff) saving.
; Two actions may be taken in this handler since the Guest OS may
; be doing lazy fpu switching. So, we'll have to generate those
; traps which the Guest CPU CTX shall have according to the
; its CR0 flags. If no traps for the Guest OS, we'll save the host
; context and restore the guest context.
;
; @returns  0 if caller should continue execution.
; @returns  VINF_EM_RAW_GUEST_TRAP if a guest trap should be generated.
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC   CPUMHandleLazyFPUAsm
    ;
    ; Figure out what to do.
    ;
    ; There are two basic actions:
    ;   1. Save host fpu and restore guest fpu.
    ;   2. Generate guest trap.
    ;
    ; When entering the hypervisor we'll always enable MP (for proper wait
    ; trapping) and TS (for intercepting all fpu/mmx/sse stuff). The EM flag
    ; is taken from the guest OS in order to get proper SSE handling.
    ;
    ;
    ; Actions taken depending on the guest CR0 flags:
    ;
    ;   3    2    1
    ;  TS | EM | MP | FPUInstr | WAIT :: VMM Action
    ; ------------------------------------------------------------------------
    ;   0 |  0 |  0 | Exec     | Exec :: Clear TS & MP, Save HC, Load GC.
    ;   0 |  0 |  1 | Exec     | Exec :: Clear TS, Save HC, Load GC.
    ;   0 |  1 |  0 | #NM      | Exec :: Clear TS & MP, Save HC, Load GC;
    ;   0 |  1 |  1 | #NM      | Exec :: Clear TS, Save HC, Load GC.
    ;   1 |  0 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already cleared.)
    ;   1 |  0 |  1 | #NM      | #NM  :: Go to host taking trap there.
    ;   1 |  1 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already set.)
    ;   1 |  1 |  1 | #NM      | #NM  :: Go to host taking trap there.

    ;
    ; Before taking any of these actions we're checking if we have already
    ; loaded the GC FPU. Because if we have, this is an trap for the guest - raw ring-3.
    ;
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USED_FPU
    jz      hlfpua_not_loaded
    jmp     hlfpua_to_host

    ;
    ; Take action.
    ;
align 16
hlfpua_not_loaded:
    mov     eax, [xDX + CPUMCPU.Guest.cr0]
    and     eax, X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
%ifdef RT_ARCH_AMD64
    lea     r8, [hlfpuajmp1 wrt rip]
    jmp     qword [rax*4 + r8]
%else
    jmp     dword [eax*2 + hlfpuajmp1]
%endif
align 16
;; jump table using fpu related cr0 flags as index.
hlfpuajmp1:
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_to_host
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_to_host
;; and mask for cr0.
hlfpu_afFlags:
    RTCCPTR_DEF ~(X86_CR0_TS | X86_CR0_MP)
    RTCCPTR_DEF ~(X86_CR0_TS)
    RTCCPTR_DEF ~(X86_CR0_TS | X86_CR0_MP)
    RTCCPTR_DEF ~(X86_CR0_TS)
    RTCCPTR_DEF ~(X86_CR0_MP)
    RTCCPTR_DEF 0
    RTCCPTR_DEF ~(X86_CR0_MP)
    RTCCPTR_DEF 0

    ;
    ; Action - switch FPU context and change cr0 flags.
    ;
align 16
hlfpua_switch_fpu_ctx:
%ifndef IN_RING3 ; IN_RC or IN_RING0
    mov     xCX, cr0
 %ifdef RT_ARCH_AMD64
    lea     r8, [hlfpu_afFlags wrt rip]
    and     rcx, [rax*4 + r8]                   ; calc the new cr0 flags.
 %else
    and     ecx, [eax*2 + hlfpu_afFlags]        ; calc the new cr0 flags.
 %endif
    mov     xAX, cr0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX                            ; clear flags so we don't trap here.
%endif
%ifndef RT_ARCH_AMD64
    mov     eax, edx
    ; Calculate the PCPUM pointer
    sub     eax, [edx + CPUMCPU.ulOffCPUM]
    test    dword [eax + CPUM.CPUFeatures.edx], X86_CPUID_FEATURE_EDX_FXSR
    jz short hlfpua_no_fxsave
%endif

    fxsave  [xDX + CPUMCPU.Host.fpu]
    or      dword [xDX + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
    fxrstor [xDX + CPUMCPU.Guest.fpu]
hlfpua_finished_switch:
%ifdef IN_RC
    mov     cr0, xCX                            ; load the new cr0 flags.
%endif
    ; return continue execution.
    xor     eax, eax
    ret

%ifndef RT_ARCH_AMD64
; legacy support.
hlfpua_no_fxsave:
    fnsave  [xDX + CPUMCPU.Host.fpu]
    or      dword [xDX + CPUMCPU.fUseFlags], dword (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM) ; yasm / nasm
    mov     eax, [xDX + CPUMCPU.Guest.fpu]    ; control word
    not     eax                                 ; 1 means exception ignored (6 LS bits)
    and     eax, byte 03Fh                      ; 6 LS bits only
    test    eax, [xDX + CPUMCPU.Guest.fpu + 4]; status word
    jz short hlfpua_no_exceptions_pending
    ; technically incorrect, but we certainly don't want any exceptions now!!
    and     dword [xDX + CPUMCPU.Guest.fpu + 4], ~03Fh
hlfpua_no_exceptions_pending:
    frstor  [xDX + CPUMCPU.Guest.fpu]
    jmp near hlfpua_finished_switch
%endif ; !RT_ARCH_AMD64


    ;
    ; Action - Generate Guest trap.
    ;
hlfpua_action_4:
hlfpua_to_host:
    mov     eax, VINF_EM_RAW_GUEST_TRAP
    ret
ENDPROC     CPUMHandleLazyFPUAsm


;;
; Restores the host's FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC CPUMSaveGuestRestoreHostFPUStateAsm
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
ENDPROC   CPUMSaveGuestRestoreHostFPUStateAsm

;;
; Sets the host's FPU/XMM state
;
; @returns  0
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC CPUMRestoreHostFPUStateAsm
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
ENDPROC   CPUMRestoreHostFPUStateAsm

;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMLoadFPUAsm
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
ENDPROC     CPUMLoadFPUAsm


;;
; Restores the guest's FPU/XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMSaveFPUAsm
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
ENDPROC CPUMSaveFPUAsm


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMLoadXMMAsm
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
    jz CPUMLoadXMMAsm_done

    movdqa  xmm8, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8]
    movdqa  xmm9, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9]
    movdqa  xmm10, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10]
    movdqa  xmm11, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11]
    movdqa  xmm12, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12]
    movdqa  xmm13, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13]
    movdqa  xmm14, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14]
    movdqa  xmm15, [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15]
CPUMLoadXMMAsm_done:
%endif

    ret
ENDPROC     CPUMLoadXMMAsm


;;
; Restores the guest's XMM state
;
; @param    pCtx  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCTX pointer
;
align 16
BEGINPROC   CPUMSaveXMMAsm
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
    jz CPUMSaveXMMAsm_done

    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*8], xmm8
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*9], xmm9
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*10], xmm10
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*11], xmm11
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*12], xmm12
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*13], xmm13
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*14], xmm14
    movdqa  [xDX + CPUMCTX.fpu + X86FXSTATE.aXMM + 16*15], xmm15

CPUMSaveXMMAsm_done:
%endif
    ret
ENDPROC     CPUMSaveXMMAsm


;;
; Set the FPU control word; clearing exceptions first
;
; @param  u16FCW    x86:[esp+4] GCC:rdi MSC:rcx     New FPU control word
align 16
BEGINPROC CPUMSetFCW
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
ENDPROC   CPUMSetFCW


;;
; Get the FPU control word
;
align 16
BEGINPROC CPUMGetFCW
    fnstcw  [xSP - 8]
    mov     ax, word [xSP - 8]
    ret
ENDPROC   CPUMGetFCW


;;
; Set the MXCSR;
;
; @param  u32MXCSR    x86:[esp+4] GCC:rdi MSC:rcx     New MXCSR
align 16
BEGINPROC CPUMSetMXCSR
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
ENDPROC   CPUMSetMXCSR


;;
; Get the MXCSR
;
align 16
BEGINPROC CPUMGetMXCSR
    stmxcsr [xSP - 8]
    mov     eax, dword [xSP - 8]
    ret
ENDPROC   CPUMGetMXCSR
