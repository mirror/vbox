; $Id$
;; @file
; VMXM - GC vmx helpers
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
%undef RT_ARCH_X86
%define RT_ARCH_AMD64
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/hwacc_vmx.mac"
%include "VBox/cpum.mac"
%include "VBox/x86.mac"

%ifdef RT_OS_OS2 ;; @todo fix OMF support in yasm and kick nasm out completely.
 %macro vmwrite 2,
    int3
 %endmacro
 %define vmlaunch int3
 %define vmresume int3
 %define vmsave int3
 %define vmload int3
 %define vmrun int3
 %define clgi int3
 %define stgi int3
 %macro invlpga 2,
    int3
 %endmacro
%endif

;; @def MYPUSHAD
; Macro generating an equivalent to pushad

;; @def MYPOPAD
; Macro generating an equivalent to popad

;; @def MYPUSHSEGS
; Macro saving all segment registers on the stack.
; @param 1  full width register name
; @param 2  16-bit regsiter name for \a 1.

;; @def MYPOPSEGS
; Macro restoring all segment registers on the stack
; @param 1  full width register name
; @param 2  16-bit regsiter name for \a 1.

  ; Load the corresponding guest MSR (trashes rdx & rcx)
  %macro LOADGUESTMSR 2
    mov     edx, dword [rsi + %2 + 4]
    mov     eax, dword [rsi + %2]
    wrmsr
  %endmacro

  ; Save a guest and load the corresponding host MSR (trashes rdx & rcx)
  ; Only really useful for gs kernel base as that one can be changed behind our back (swapgs)
  %macro LOADHOSTMSREX 2
    mov     rcx, %1
    rdmsr
    mov     dword [rsi + %2], eax
    mov     dword [rsi + %2 + 4], edx
  %endmacro

 %ifdef ASM_CALL64_GCC
  %macro MYPUSHAD 0
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx
  %endmacro
  %macro MYPOPAD 0
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
  %endmacro

 %else ; ASM_CALL64_MSC
  %macro MYPUSHAD 0
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx
    push    rsi
    push    rdi
  %endmacro
  %macro MYPOPAD 0
    pop     rdi
    pop     rsi
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
  %endmacro
 %endif

; trashes, rax, rdx & rcx
 %macro MYPUSHSEGS 2
    mov     %2, es
    push    %1
    mov     %2, ds
    push    %1

    ; Special case for FS; Windows and Linux either don't use it or restore it when leaving kernel mode, Solaris OTOH doesn't and we must save it.
    mov     ecx, MSR_K8_FS_BASE
    rdmsr
    push    rdx
    push    rax
    push    fs

    ; Special case for GS; OSes typically use swapgs to reset the hidden base register for GS on entry into the kernel. The same happens on exit
    mov     ecx, MSR_K8_GS_BASE
    rdmsr
    push    rdx
    push    rax
    push    gs
 %endmacro

; trashes, rax, rdx & rcx
 %macro MYPOPSEGS 2
    ; Note: do not step through this code with a debugger!
    pop     gs
    pop     rax
    pop     rdx
    mov     ecx, MSR_K8_GS_BASE
    wrmsr

    pop     fs
    pop     rax
    pop     rdx
    mov     ecx, MSR_K8_FS_BASE
    wrmsr
    ; Now it's safe to step again

    pop     %1
    mov     ds, %2
    pop     %1
    mov     es, %2
 %endmacro



BEGINCODE
BITS 64


;/**
; * Prepares for and executes VMLAUNCH/VMRESUME (64 bits guest mode)
; *
; * @returns VBox status code
; * @param   pCtx       Guest context (rsi)
; */
BEGINPROC VMXGCStartVM64
    push    rbp
    mov     rbp, rsp

    pushf
    cli

    ; Have to sync half the guest state as we can't access most of the 64 bits state. Sigh
;    VMCSWRITE VMX_VMCS64_GUEST_CS_BASE,         [rsi + CPUMCTX.csHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_DS_BASE,         [rsi + CPUMCTX.dsHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_ES_BASE,         [rsi + CPUMCTX.esHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_FS_BASE,         [rsi + CPUMCTX.fsHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_GS_BASE,         [rsi + CPUMCTX.gsHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_SS_BASE,         [rsi + CPUMCTX.ssHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_LDTR_BASE,       [rsi + CPUMCTX.ldtrHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_GDTR_BASE,       [rsi + CPUMCTX.gdtrHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_IDTR_BASE,       [rsi + CPUMCTX.idtrHid.u64Base]
;    VMCSWRITE VMX_VMCS64_GUEST_TR_BASE,         [rsi + CPUMCTX.trHid.u64Base]
;    
;    VMCSWRITE VMX_VMCS64_GUEST_SYSENTER_EIP,    [rsi + CPUMCTX.SysEnter.eip]
;    VMCSWRITE VMX_VMCS64_GUEST_SYSENTER_ESP,    [rsi + CPUMCTX.SysEnter.esp]
;    
;    VMCSWRITE VMX_VMCS64_GUEST_RIP,             [rsi + CPUMCTX.eip]
;    VMCSWRITE VMX_VMCS64_GUEST_RSP,             [rsi + CPUMCTX.esp]
    

    ;/* First we have to save some final CPU context registers. */
    lea     rax, [.vmlaunch64_done wrt rip]    
    push    rax
    mov     rax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite rax, [rsp]
    ;/* Note: assumes success... */
    add     rsp, 8

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; * - EFLAGS (reset to RT_BIT(1); not relevant)
    ; *
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save the Guest CPU context pointer. */
    ; pCtx    already in rsi

    ;/* Save segment registers */
    ; Note: MYPUSHSEGS trashes rdx & rcx, so we moved it here (msvc amd64 case)
    MYPUSHSEGS rax, ax

    ; Save the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs and restore the guest MSRs
    ;; @todo use the automatic load feature for MSRs
    LOADGUESTMSR MSR_K8_LSTAR,          CPUMCTX.msrLSTAR
    LOADGUESTMSR MSR_K6_STAR,           CPUMCTX.msrSTAR
    LOADGUESTMSR MSR_K8_SF_MASK,        CPUMCTX.msrSFMASK
    LOADGUESTMSR MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

    ; Save the pCtx pointer
    push    rsi

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    rax

    ; VMX only saves the base of the GDTR & IDTR and resets the limit to 0xffff; we must restore the limit correctly!
    sub     rsp, 8*2
    sgdt    [rsp]

    sub     rsp, 8*2
    sidt    [rsp]

    ; Restore CR2
    mov     rbx, qword [rsi + CPUMCTX.cr2]
    mov     cr2, rbx

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite rax, rsp
    ;/* Note: assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     rax, qword [rsi + CPUMCTX.eax]
    mov     rbx, qword [rsi + CPUMCTX.ebx]
    mov     rcx, qword [rsi + CPUMCTX.ecx]
    mov     rdx, qword [rsi + CPUMCTX.edx]
    mov     rbp, qword [rsi + CPUMCTX.ebp]
    mov     r8,  qword [rsi + CPUMCTX.r8]
    mov     r9,  qword [rsi + CPUMCTX.r9]
    mov     r10, qword [rsi + CPUMCTX.r10]
    mov     r11, qword [rsi + CPUMCTX.r11]
    mov     r12, qword [rsi + CPUMCTX.r12]
    mov     r13, qword [rsi + CPUMCTX.r13]
    mov     r14, qword [rsi + CPUMCTX.r14]
    mov     r15, qword [rsi + CPUMCTX.r15]

    ;/* Restore rdi & rsi. */
    mov     rdi, qword [rsi + CPUMCTX.edi]
    mov     rsi, qword [rsi + CPUMCTX.esi]

    vmlaunch
    jmp     .vmlaunch64_done;      ;/* here if vmlaunch detected a failure. */

ALIGNCODE(16)
.vmlaunch64_done:
    jc      near .vmstart64_invalid_vmxon_ptr
    jz      near .vmstart64_start_failed

    ; Restore base and limit of the IDTR & GDTR
    lidt    [rsp]
    add     rsp, 8*2
    lgdt    [rsp]
    add     rsp, 8*2

    push    rdi
    mov     rdi, [rsp + 8 * 2]         ; pCtx

    mov     qword [rdi + CPUMCTX.eax], rax
    mov     qword [rdi + CPUMCTX.ebx], rbx
    mov     qword [rdi + CPUMCTX.ecx], rcx
    mov     qword [rdi + CPUMCTX.edx], rdx
    mov     qword [rdi + CPUMCTX.esi], rsi
    mov     qword [rdi + CPUMCTX.ebp], rbp
    mov     qword [rdi + CPUMCTX.r8],  r8
    mov     qword [rdi + CPUMCTX.r9],  r9
    mov     qword [rdi + CPUMCTX.r10], r10
    mov     qword [rdi + CPUMCTX.r11], r11
    mov     qword [rdi + CPUMCTX.r12], r12
    mov     qword [rdi + CPUMCTX.r13], r13
    mov     qword [rdi + CPUMCTX.r14], r14
    mov     qword [rdi + CPUMCTX.r15], r15

    pop     rax                                 ; the guest edi we pushed above
    mov     qword [rdi + CPUMCTX.edi], rax

    pop     rax         ; saved LDTR
    lldt    ax

    pop     rsi         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ;; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

    ; Restore segment registers
    MYPOPSEGS rax, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

.vmstart64_end:
    popf
    pop     rbp
    ret


.vmstart64_invalid_vmxon_ptr:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [rsp]
    add     rsp, 8*2
    lgdt    [rsp]
    add     rsp, 8*2

    pop     rax         ; saved LDTR
    lldt    ax

    pop     rsi         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ;; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

    ; Restore segment registers
    MYPOPSEGS rax, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart64_end

.vmstart64_start_failed:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [rsp]
    add     rsp, 8*2
    lgdt    [rsp]
    add     rsp, 8*2

    pop     rax         ; saved LDTR
    lldt    ax

    pop     rsi         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ;; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

    ; Restore segment registers
    MYPOPSEGS rax, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_UNABLE_TO_START_VM
    jmp     .vmstart64_end
ENDPROC VMXGCStartVM64


;/**
; * Prepares for and executes VMRUN (64 bits guests)
; *
; * @returns VBox status code
; * @param   HCPhysVMCB     Physical address of host VMCB       (rsp+8)
; * @param   HCPhysVMCB     Physical address of guest VMCB      (rsp+16)
; * @param   pCtx           Guest context                       (rsi)
; */
BEGINPROC SVMGCVMRun64
    push    rbp
    mov     rbp, rsp
    pushf

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP, RAX
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save the Guest CPU context pointer. */
    push    rsi                     ; push for saving the state at the end

    ; Restore CR2
    mov     rbx, [rsi + CPUMCTX.cr2]
    mov     cr2, rbx

    ; save host fs, gs, sysenter msr etc
    mov     rax, [rbp + 8]                  ; pVMCBHostPhys (64 bits physical address)
    push    rax                             ; save for the vmload after vmrun
    vmsave

    ; setup eax for VMLOAD
    mov     rax, [rbp + 8 + RTHCPHYS_CB]    ; pVMCBPhys (64 bits physical address)

    ;/* Restore Guest's general purpose registers. */
    ;/* RAX is loaded from the VMCB by VMRUN */
    mov     rbx, qword [rsi + CPUMCTX.ebx]
    mov     rcx, qword [rsi + CPUMCTX.ecx]
    mov     rdx, qword [rsi + CPUMCTX.edx]
    mov     rdi, qword [rsi + CPUMCTX.edi]
    mov     rbp, qword [rsi + CPUMCTX.ebp]
    mov     r8,  qword [rsi + CPUMCTX.r8]
    mov     r9,  qword [rsi + CPUMCTX.r9]
    mov     r10, qword [rsi + CPUMCTX.r10]
    mov     r11, qword [rsi + CPUMCTX.r11]
    mov     r12, qword [rsi + CPUMCTX.r12]
    mov     r13, qword [rsi + CPUMCTX.r13]
    mov     r14, qword [rsi + CPUMCTX.r14]
    mov     r15, qword [rsi + CPUMCTX.r15]
    mov     rsi, qword [rsi + CPUMCTX.esi]

    ; Clear the global interrupt flag & execute sti to make sure external interrupts cause a world switch
    clgi
    sti

    ; load guest fs, gs, sysenter msr etc
    vmload
    ; run the VM
    vmrun

    ;/* RAX is in the VMCB already; we can use it here. */

    ; save guest fs, gs, sysenter msr etc
    vmsave

    ; load host fs, gs, sysenter msr etc
    pop     rax                     ; pushed above
    vmload

    ; Set the global interrupt flag again, but execute cli to make sure IF=0.
    cli
    stgi

    pop     rax                     ; pCtx

    mov     qword [rax + CPUMCTX.ebx], rbx
    mov     qword [rax + CPUMCTX.ecx], rcx
    mov     qword [rax + CPUMCTX.edx], rdx
    mov     qword [rax + CPUMCTX.esi], rsi
    mov     qword [rax + CPUMCTX.edi], rdi
    mov     qword [rax + CPUMCTX.ebp], rbp
    mov     qword [rax + CPUMCTX.r8],  r8
    mov     qword [rax + CPUMCTX.r9],  r9
    mov     qword [rax + CPUMCTX.r10], r10
    mov     qword [rax + CPUMCTX.r11], r11
    mov     qword [rax + CPUMCTX.r12], r12
    mov     qword [rax + CPUMCTX.r13], r13
    mov     qword [rax + CPUMCTX.r14], r14
    mov     qword [rax + CPUMCTX.r15], r15

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

    popf
    pop     rbp
    ret
ENDPROC SVMGCVMRun64

;/**
; * Saves the guest FPU context
; *
; * @returns VBox status code
; * @param   pCtx       Guest context [rsi]
; */
BEGINPROC HWACCMSaveGuestFPU64
    mov     rax, cr0
    mov     rcx, rax                    ; save old CR0
    and     rax, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, rax

    fxsave  [rsi + CPUMCTX.fpu]

    mov     cr0, rcx                    ; and restore old CR0 again
    
    mov     eax, VINF_SUCCESS
    ret
ENDPROC HWACCMSaveGuestFPU64

;/**
; * Saves the guest debug context (DR0-3, DR6)
; *
; * @returns VBox status code
; * @param   pCtx       Guest context [rsi]
; */
BEGINPROC HWACCMSaveGuestDebug64
    mov rax, dr0
    mov qword [rsi + CPUMCTX.dr + 0*8], rax
    mov rax, dr1
    mov qword [rsi + CPUMCTX.dr + 1*8], rax
    mov rax, dr2
    mov qword [rsi + CPUMCTX.dr + 2*8], rax
    mov rax, dr3
    mov qword [rsi + CPUMCTX.dr + 3*8], rax
    mov rax, dr6
    mov qword [rsi + CPUMCTX.dr + 6*8], rax
    mov eax, VINF_SUCCESS
    ret
ENDPROC HWACCMSaveGuestDebug64

;/**
; * Dummy callback handler
; *
; * @returns VBox status code
; * @param   pCtx       Guest context [rsi]
; */
BEGINPROC HWACCMTestSwitcher64
    mov eax, VINF_SUCCESS
    ret
ENDPROC HWACCMTestSwitcher64
