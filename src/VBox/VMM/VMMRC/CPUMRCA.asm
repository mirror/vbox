; $Id$
;; @file
; CPUM - Guest Context Assembly Routines.
;

; Copyright (C) 2006-2007 Oracle Corporation
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
%include "VMMRC.mac"
%include "VBox/vmm/vm.mac"
%include "VBox/err.mac"
%include "VBox/vmm/stam.mac"
%include "CPUMInternal.mac"
%include "iprt/x86.mac"
%include "VBox/vmm/cpum.mac"


;*******************************************************************************
;* External Symbols                                                            *
;*******************************************************************************
extern IMPNAME(g_CPUM)                 ; VMM GC Builtin import
extern IMPNAME(g_VM)                   ; VMM GC Builtin import
extern NAME(cpumGCHandleNPAndGP)       ; CPUMGC.cpp

;
; Enables write protection of Hypervisor memory pages.
; !note! Must be commented out for Trap8 debug handler.
;
%define ENABLE_WRITE_PROTECTION 1

BEGINCODE


;;
; Calls a guest trap/interrupt handler directly
; Assumes a trap stack frame has already been setup on the guest's stack!
;
; @param   pRegFrame   [esp + 4]  Original trap/interrupt context
; @param   selCS       [esp + 8]  Code selector of handler
; @param   pHandler    [esp + 12] GC virtual address of handler
; @param   eflags      [esp + 16] Callee's EFLAGS
; @param   selSS       [esp + 20] Stack selector for handler
; @param   pEsp        [esp + 24] Stack address for handler
;
; @remark This call never returns!
;
; VMMRCDECL(void) CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTGCPTR pHandler, uint32_t eflags, uint32_t selSS, RTGCPTR pEsp);
align 16
BEGINPROC_EXPORTED CPUMGCCallGuestTrapHandler
    mov     ebp, esp

    ; construct iret stack frame
    push    dword [ebp + 20]                ; SS
    push    dword [ebp + 24]                ; ESP
    push    dword [ebp + 16]                ; EFLAGS
    push    dword [ebp + 8]                 ; CS
    push    dword [ebp + 12]                ; EIP

    ;
    ; enable WP
    ;
%ifdef ENABLE_WRITE_PROTECTION
    mov     eax, cr0
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
%endif

    ; restore CPU context (all except cs, eip, ss, esp & eflags; which are restored or overwritten by iret)
    mov     ebp, [ebp + 4]                  ; pRegFrame
    mov     ebx, [ebp + CPUMCTXCORE.ebx]
    mov     ecx, [ebp + CPUMCTXCORE.ecx]
    mov     edx, [ebp + CPUMCTXCORE.edx]
    mov     esi, [ebp + CPUMCTXCORE.esi]
    mov     edi, [ebp + CPUMCTXCORE.edi]

    ;; @todo  load segment registers *before* enabling WP.
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_GS | CPUM_HANDLER_CTXCORE_IN_EBP
    mov     gs, [ebp + CPUMCTXCORE.gs.Sel]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_FS | CPUM_HANDLER_CTXCORE_IN_EBP
    mov     fs, [ebp + CPUMCTXCORE.fs.Sel]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_ES | CPUM_HANDLER_CTXCORE_IN_EBP
    mov     es, [ebp + CPUMCTXCORE.es.Sel]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_DS | CPUM_HANDLER_CTXCORE_IN_EBP
    mov     ds, [ebp + CPUMCTXCORE.ds.Sel]

    mov     eax, [ebp + CPUMCTXCORE.eax]
    mov     ebp, [ebp + CPUMCTXCORE.ebp]

    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_IRET
    iret
ENDPROC CPUMGCCallGuestTrapHandler


;;
; Performs an iret to V86 code
; Assumes a trap stack frame has already been setup on the guest's stack!
;
; @param   pRegFrame   Original trap/interrupt context
;
; This function does not return!
;
;VMMRCDECL(void) CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);
align 16
BEGINPROC CPUMGCCallV86Code
    mov     ebp, [esp + 4]                  ; pRegFrame

    ; construct iret stack frame
    push    dword [ebp + CPUMCTXCORE.gs.Sel]
    push    dword [ebp + CPUMCTXCORE.fs.Sel]
    push    dword [ebp + CPUMCTXCORE.ds.Sel]
    push    dword [ebp + CPUMCTXCORE.es.Sel]
    push    dword [ebp + CPUMCTXCORE.ss.Sel]
    push    dword [ebp + CPUMCTXCORE.esp]
    push    dword [ebp + CPUMCTXCORE.eflags]
    push    dword [ebp + CPUMCTXCORE.cs.Sel]
    push    dword [ebp + CPUMCTXCORE.eip]

    ;
    ; enable WP
    ;
%ifdef ENABLE_WRITE_PROTECTION
    mov     eax, cr0
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
%endif

    ; restore CPU context (all except cs, eip, ss, esp, eflags, ds, es, fs & gs; which are restored or overwritten by iret)
    mov     eax, [ebp + CPUMCTXCORE.eax]
    mov     ebx, [ebp + CPUMCTXCORE.ebx]
    mov     ecx, [ebp + CPUMCTXCORE.ecx]
    mov     edx, [ebp + CPUMCTXCORE.edx]
    mov     esi, [ebp + CPUMCTXCORE.esi]
    mov     edi, [ebp + CPUMCTXCORE.edi]
    mov     ebp, [ebp + CPUMCTXCORE.ebp]

    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_IRET
    iret
ENDPROC CPUMGCCallV86Code


;;
; This is a main entry point for resuming (or starting) guest
; code execution.
;
; We get here directly from VMMSwitcher.asm (jmp at the end
; of VMMSwitcher_HostToGuest).
;
; This call never returns!
;
; @param    edx     Pointer to CPUM structure.
;
align 16
BEGINPROC_EXPORTED CPUMGCResumeGuest
    ; Convert to CPUMCPU pointer
    add     edx, [edx + CPUM.offCPUMCPU0]
    ;
    ; Setup iretd
    ;
    push    dword [edx + CPUMCPU.Guest.ss.Sel]
    push    dword [edx + CPUMCPU.Guest.esp]
    push    dword [edx + CPUMCPU.Guest.eflags]
    push    dword [edx + CPUMCPU.Guest.cs.Sel]
    push    dword [edx + CPUMCPU.Guest.eip]

    ;
    ; Restore registers.
    ;
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_ES
    mov     es,  [edx + CPUMCPU.Guest.es.Sel]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_FS
    mov     fs,  [edx + CPUMCPU.Guest.fs.Sel]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_GS
    mov     gs,  [edx + CPUMCPU.Guest.gs.Sel]

%ifdef VBOX_WITH_STATISTICS
    ;
    ; Statistics.
    ;
    push    edx
    mov     edx, IMP(g_VM)
    lea     edx, [edx + VM.StatTotalQemuToGC]
    STAM_PROFILE_ADV_STOP edx

    mov     edx, IMP(g_VM)
    lea     edx, [edx + VM.StatTotalInGC]
    STAM_PROFILE_ADV_START edx
    pop     edx
%endif

    ;
    ; enable WP
    ;
%ifdef ENABLE_WRITE_PROTECTION
    mov     eax, cr0
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
%endif

    ;
    ; Continue restore.
    ;
    mov     esi, [edx + CPUMCPU.Guest.esi]
    mov     edi, [edx + CPUMCPU.Guest.edi]
    mov     ebp, [edx + CPUMCPU.Guest.ebp]
    mov     ebx, [edx + CPUMCPU.Guest.ebx]
    mov     ecx, [edx + CPUMCPU.Guest.ecx]
    mov     eax, [edx + CPUMCPU.Guest.eax]
    push    dword [edx + CPUMCPU.Guest.ds.Sel]
    mov     edx, [edx + CPUMCPU.Guest.edx]
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_DS
    pop     ds

    ; restart execution.
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_IRET
    iretd
ENDPROC     CPUMGCResumeGuest


;;
; This is a main entry point for resuming (or starting) guest
; code execution for raw V86 mode
;
; We get here directly from VMMSwitcher.asm (jmp at the end
; of VMMSwitcher_HostToGuest).
;
; This call never returns!
;
; @param    edx     Pointer to CPUM structure.
;
align 16
BEGINPROC_EXPORTED CPUMGCResumeGuestV86
    ; Convert to CPUMCPU pointer
    add     edx, [edx + CPUM.offCPUMCPU0]
    ;
    ; Setup iretd
    ;
    push    dword [edx + CPUMCPU.Guest.gs.Sel]
    push    dword [edx + CPUMCPU.Guest.fs.Sel]
    push    dword [edx + CPUMCPU.Guest.ds.Sel]
    push    dword [edx + CPUMCPU.Guest.es.Sel]

    push    dword [edx + CPUMCPU.Guest.ss.Sel]
    push    dword [edx + CPUMCPU.Guest.esp]

    push    dword [edx + CPUMCPU.Guest.eflags]
    push    dword [edx + CPUMCPU.Guest.cs.Sel]
    push    dword [edx + CPUMCPU.Guest.eip]

    ;
    ; Restore registers.
    ;

%ifdef VBOX_WITH_STATISTICS
    ;
    ; Statistics.
    ;
    push    edx
    mov     edx, IMP(g_VM)
    lea     edx, [edx + VM.StatTotalQemuToGC]
    STAM_PROFILE_ADV_STOP edx

    mov     edx, IMP(g_VM)
    lea     edx, [edx + VM.StatTotalInGC]
    STAM_PROFILE_ADV_START edx
    pop     edx
%endif

    ;
    ; enable WP
    ;
%ifdef ENABLE_WRITE_PROTECTION
    mov     eax, cr0
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
%endif

    ;
    ; Continue restore.
    ;
    mov     esi, [edx + CPUMCPU.Guest.esi]
    mov     edi, [edx + CPUMCPU.Guest.edi]
    mov     ebp, [edx + CPUMCPU.Guest.ebp]
    mov     ecx, [edx + CPUMCPU.Guest.ecx]
    mov     ebx, [edx + CPUMCPU.Guest.ebx]
    mov     eax, [edx + CPUMCPU.Guest.eax]
    mov     edx, [edx + CPUMCPU.Guest.edx]

    ; restart execution.
    TRPM_NP_GP_HANDLER NAME(cpumGCHandleNPAndGP), CPUM_HANDLER_IRET
    iretd
ENDPROC     CPUMGCResumeGuestV86

