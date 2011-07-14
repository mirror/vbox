; $Id$
;; @file
; VMXM - GC vmx helpers
;

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
%undef RT_ARCH_X86
%define RT_ARCH_AMD64
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/vmm/hwacc_vmx.mac"
%include "VBox/vmm/cpum.mac"
%include "iprt/x86.mac"
%include "HWACCMInternal.mac"

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

;; @def MYPUSHSEGS
; Macro saving all segment registers on the stack.
; @param 1  full width register name

;; @def MYPOPSEGS
; Macro restoring all segment registers on the stack
; @param 1  full width register name

  ; Load the corresponding guest MSR (trashes rdx & rcx)
  %macro LOADGUESTMSR 2
    mov     rcx, %1
    mov     edx, dword [rsi + %2 + 4]
    mov     eax, dword [rsi + %2]
    wrmsr
  %endmacro

  ; Save a guest MSR (trashes rdx & rcx)
  ; Only really useful for gs kernel base as that one can be changed behind our back (swapgs)
  %macro SAVEGUESTMSR 2
    mov     rcx, %1
    rdmsr
    mov     dword [rsi + %2], eax
    mov     dword [rsi + %2 + 4], edx
  %endmacro

 %macro MYPUSHSEGS 1
    mov     %1, es
    push    %1
    mov     %1, ds
    push    %1
 %endmacro

 %macro MYPOPSEGS 1
    pop     %1
    mov     ds, %1
    pop     %1
    mov     es, %1
 %endmacro

BEGINCODE
BITS 64


;/**
; * Prepares for and executes VMLAUNCH/VMRESUME (64 bits guest mode)
; *
; * @returns VBox status code
; * @param   HCPhysCpuPage  VMXON physical address  [rsp+8]
; * @param   HCPhysVMCS     VMCS physical address   [rsp+16]
; * @param   pCache         VMCS cache              [rsp+24]
; * @param   pCtx           Guest context (rsi)
; */
BEGINPROC VMXGCStartVM64
    push    rbp
    mov     rbp, rsp

    ; Make sure VT-x instructions are allowed
    mov     rax, cr4
    or      rax, X86_CR4_VMXE
    mov     cr4, rax

    ;/* Enter VMX Root Mode */
    vmxon   [rbp + 8 + 8]
    jnc     .vmxon_success
    mov     rax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart64_vmxon_failed

.vmxon_success:
    jnz     .vmxon_success2
    mov     rax, VERR_VMX_GENERIC
    jmp     .vmstart64_vmxon_failed

.vmxon_success2:
    ; Activate the VMCS pointer
    vmptrld [rbp + 16 + 8]
    jnc     .vmptrld_success
    mov     rax, VERR_VMX_INVALID_VMCS_PTR
    jmp     .vmstart64_vmxoff_end

.vmptrld_success:
    jnz     .vmptrld_success2
    mov     rax, VERR_VMX_GENERIC
    jmp     .vmstart64_vmxoff_end

.vmptrld_success2:

    ; Save the VMCS pointer on the stack
    push    qword [rbp + 16 + 8];

    ;/* Save segment registers */
    MYPUSHSEGS rax

%ifdef VMX_USE_CACHED_VMCS_ACCESSES
    ; Flush the VMCS write cache first (before any other vmreads/vmwrites!)
    mov     rbx, [rbp + 24 + 8]                             ; pCache

%ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     qword [rbx + VMCSCACHE.uPos], 2
%endif

%ifdef DEBUG
    mov     rax, [rbp + 8 + 8]                              ; HCPhysCpuPage
    mov     [rbx + VMCSCACHE.TestIn.HCPhysCpuPage], rax
    mov     rax, [rbp + 16 + 8]                             ; HCPhysVMCS
    mov     [rbx + VMCSCACHE.TestIn.HCPhysVMCS], rax
    mov     [rbx + VMCSCACHE.TestIn.pCache], rbx
    mov     [rbx + VMCSCACHE.TestIn.pCtx], rsi
%endif

    mov     ecx, [rbx + VMCSCACHE.Write.cValidEntries]
    cmp     ecx, 0
    je      .no_cached_writes
    mov     rdx, rcx
    mov     rcx, 0
    jmp     .cached_write

ALIGN(16)
.cached_write:
    mov     eax, [rbx + VMCSCACHE.Write.aField + rcx*4]
    vmwrite rax, qword [rbx + VMCSCACHE.Write.aFieldVal + rcx*8]
    inc     rcx
    cmp     rcx, rdx
    jl     .cached_write

    mov     dword [rbx + VMCSCACHE.Write.cValidEntries], 0
.no_cached_writes:

 %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     qword [rbx + VMCSCACHE.uPos], 3
 %endif
    ; Save the pCache pointer
    push    xBX
%endif

    ; Save the host state that's relevant in the temporary 64 bits mode
    mov     rdx, cr0
    mov     eax, VMX_VMCS_HOST_CR0
    vmwrite rax, rdx

    mov     rdx, cr3
    mov     eax, VMX_VMCS_HOST_CR3
    vmwrite rax, rdx

    mov     rdx, cr4
    mov     eax, VMX_VMCS_HOST_CR4
    vmwrite rax, rdx

    mov     rdx, cs
    mov     eax, VMX_VMCS_HOST_FIELD_CS
    vmwrite rax, rdx

    mov     rdx, ss
    mov     eax, VMX_VMCS_HOST_FIELD_SS
    vmwrite rax, rdx

    sub     rsp, 8*2
    sgdt    [rsp]
    mov     eax, VMX_VMCS_HOST_GDTR_BASE
    vmwrite rax, [rsp+2]
    add     rsp, 8*2

%ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     qword [rbx + VMCSCACHE.uPos], 4
%endif

    ; hopefully we can ignore TR (we restore it anyway on the way back to 32 bits mode)

    ;/* First we have to save some final CPU context registers. */
    lea     rdx, [.vmlaunch64_done wrt rip]
    mov     rax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite rax, rdx
    ;/* Note: assumes success... */

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

    ; Load the guest LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ;; @todo use the automatic load feature for MSRs
    LOADGUESTMSR MSR_K8_LSTAR,          CPUMCTX.msrLSTAR
%if 0  ; not supported on Intel CPUs
    LOADGUESTMSR MSR_K8_CSTAR,          CPUMCTX.msrCSTAR
%endif
    LOADGUESTMSR MSR_K6_STAR,           CPUMCTX.msrSTAR
    LOADGUESTMSR MSR_K8_SF_MASK,        CPUMCTX.msrSFMASK
    LOADGUESTMSR MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

%ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     qword [rbx + VMCSCACHE.uPos], 5
%endif

    ; Save the pCtx pointer
    push    rsi

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

    push    rdi
    mov     rdi, [rsp + 8]         ; pCtx

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

    pop     rsi         ; pCtx (needed in rsi by the macros below)

    ;; @todo use the automatic load feature for MSRs
    SAVEGUESTMSR MSR_K8_LSTAR,          CPUMCTX.msrLSTAR
%if 0  ; not supported on Intel CPUs
    SAVEGUESTMSR MSR_K8_CSTAR,          CPUMCTX.msrCSTAR
%endif
    SAVEGUESTMSR MSR_K6_STAR,           CPUMCTX.msrSTAR
    SAVEGUESTMSR MSR_K8_SF_MASK,        CPUMCTX.msrSFMASK
    SAVEGUESTMSR MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

%ifdef VMX_USE_CACHED_VMCS_ACCESSES
    pop     rdi         ; saved pCache

 %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 7
 %endif
 %ifdef DEBUG
    mov     [rdi + VMCSCACHE.TestOut.pCache], rdi
    mov     [rdi + VMCSCACHE.TestOut.pCtx], rsi
    mov     rax, cr8
    mov     [rdi + VMCSCACHE.TestOut.cr8], rax
 %endif

    mov     ecx, [rdi + VMCSCACHE.Read.cValidEntries]
    cmp     ecx, 0  ; can't happen
    je      .no_cached_reads
    jmp     .cached_read

ALIGN(16)
.cached_read:
    dec     rcx
    mov     eax, [rdi + VMCSCACHE.Read.aField + rcx*4]
    vmread  qword [rdi + VMCSCACHE.Read.aFieldVal + rcx*8], rax
    cmp     rcx, 0
    jnz     .cached_read
.no_cached_reads:

    ; Save CR2 for EPT
    mov     rax, cr2
    mov     [rdi + VMCSCACHE.cr2], rax
 %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 8
 %endif
%endif

    ; Restore segment registers
    MYPOPSEGS rax

    mov     eax, VINF_SUCCESS

%ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 9
%endif
.vmstart64_end:

%ifdef VMX_USE_CACHED_VMCS_ACCESSES
 %ifdef DEBUG
    mov     rdx, [rsp]                             ; HCPhysVMCS
    mov     [rdi + VMCSCACHE.TestOut.HCPhysVMCS], rdx
 %endif
%endif

    ; Write back the data and disable the VMCS
    vmclear qword [rsp]  ;Pushed pVMCS
    add     rsp, 8

.vmstart64_vmxoff_end:
    ; Disable VMX root mode
    vmxoff
.vmstart64_vmxon_failed:
%ifdef VMX_USE_CACHED_VMCS_ACCESSES
 %ifdef DEBUG
    cmp     eax, VINF_SUCCESS
    jne     .skip_flags_save

    pushf
    pop     rdx
    mov     [rdi + VMCSCACHE.TestOut.eflags], rdx
  %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 12
  %endif
.skip_flags_save:
 %endif
%endif
    pop     rbp
    ret


.vmstart64_invalid_vmxon_ptr:
    pop     rsi         ; pCtx (needed in rsi by the macros below)

%ifdef VMX_USE_CACHED_VMCS_ACCESSES
    pop     rdi         ; pCache
 %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 10
 %endif

 %ifdef DEBUG
    mov     [rdi + VMCSCACHE.TestOut.pCache], rdi
    mov     [rdi + VMCSCACHE.TestOut.pCtx], rsi
 %endif
%endif

    ; Restore segment registers
    MYPOPSEGS rax

    ; Restore all general purpose host registers.
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart64_end

.vmstart64_start_failed:
    pop     rsi         ; pCtx (needed in rsi by the macros below)

%ifdef VMX_USE_CACHED_VMCS_ACCESSES
    pop     rdi         ; pCache

 %ifdef DEBUG
    mov     [rdi + VMCSCACHE.TestOut.pCache], rdi
    mov     [rdi + VMCSCACHE.TestOut.pCtx], rsi
 %endif
 %ifdef VBOX_WITH_CRASHDUMP_MAGIC
    mov     dword [rdi + VMCSCACHE.uPos], 11
 %endif
%endif

    ; Restore segment registers
    MYPOPSEGS rax

    ; Restore all general purpose host registers.
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

    ;/* Save the Guest CPU context pointer. */
    push    rsi                     ; push for saving the state at the end

    ; save host fs, gs, sysenter msr etc
    mov     rax, [rbp + 8 + 8]              ; pVMCBHostPhys (64 bits physical address)
    push    rax                             ; save for the vmload after vmrun
    vmsave

    ; setup eax for VMLOAD
    mov     rax, [rbp + 8 + 8 + RTHCPHYS_CB]   ; pVMCBPhys (64 bits physical address)

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
; * @param   param1     Parameter 1 [rsp+8]
; * @param   param2     Parameter 2 [rsp+12]
; * @param   param3     Parameter 3 [rsp+16]
; * @param   param4     Parameter 4 [rsp+20]
; * @param   param5     Parameter 5 [rsp+24]
; * @param   pCtx       Guest context [rsi]
; */
BEGINPROC HWACCMTestSwitcher64
    mov eax, [rsp+8]
    ret
ENDPROC HWACCMTestSwitcher64
