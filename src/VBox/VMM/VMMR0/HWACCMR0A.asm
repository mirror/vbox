; $Id$
;; @file
; VMXM - R0 vmx helpers
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

;; This is too risky wrt. stability, performance and correctness.
;%define VBOX_WITH_DR6_EXPERIMENT 1

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

%ifdef RT_ARCH_AMD64
  ; Save a host and load the corresponding guest MSR (trashes rdx & rcx)
  %macro LOADGUESTMSR 2
    mov     rcx, %1
    rdmsr
    push    rdx
    push    rax
    mov     edx, dword [xSI + %2 + 4]
    mov     eax, dword [xSI + %2]
    wrmsr
  %endmacro

  ; Save a guest and load the corresponding host MSR (trashes rdx & rcx)
  ; Only really useful for gs kernel base as that one can be changed behind our back (swapgs)
  %macro LOADHOSTMSREX 2
    mov     rcx, %1
    rdmsr 
    mov     dword [xSI + %2], eax
    mov     dword [xSI + %2 + 4], edx
    pop     rax
    pop     rdx
    wrmsr
  %endmacro

  ; Load the corresponding host MSR (trashes rdx & rcx)
  %macro LOADHOSTMSR 1
    mov     rcx, %1
    pop     rax
    pop     rdx
    wrmsr
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

%else ; RT_ARCH_X86
  %macro MYPUSHAD 0
    pushad
  %endmacro
  %macro MYPOPAD 0
    popad
  %endmacro

  %macro MYPUSHSEGS 2
    push    ds
    push    es
    push    fs
    push    gs
  %endmacro
  %macro MYPOPSEGS 2
    pop     gs
    pop     fs
    pop     es
    pop     ds
  %endmacro
%endif


BEGINCODE

;/**
; * Prepares for and executes VMLAUNCH/VMRESUME (32 bits guest mode)
; *
; * @returns VBox status code
; * @param   fResume    vmlauch/vmresume
; * @param   pCtx       Guest context
; */
BEGINPROC VMXR0StartVM32
    push    xBP
    mov     xBP, xSP

    pushf
    cli

    ;/* First we have to save some final CPU context registers. */
%ifdef RT_ARCH_AMD64
    mov     rax, qword .vmlaunch_done
    push    rax
%else
    push    .vmlaunch_done
%endif
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite xAX, [xSP]
    ;/* Note: assumes success... */
    add     xSP, xS

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
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    ; fResume already in rdi
    ; pCtx    already in rsi
 %else
    mov     rdi, rcx        ; fResume
    mov     rsi, rdx        ; pCtx
 %endif
%else
    mov     edi, [ebp + 8]  ; fResume
    mov     esi, [ebp + 12] ; pCtx
%endif

    ;/* Save segment registers */
    ; Note: MYPUSHSEGS trashes rdx & rcx, so we moved it here (msvc amd64 case)
    MYPUSHSEGS xAX, ax

    ; Save the pCtx pointer
    push    xSI

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    xAX

    ; VMX only saves the base of the GDTR & IDTR and resets the limit to 0xffff; we must restore the limit correctly!
    sub     xSP, xS*2
    sgdt    [xSP]

    sub     xSP, xS*2
    sidt    [xSP]

%ifdef VBOX_WITH_DR6_EXPERIMENT
    ; Restore DR6 - experiment, not safe!
    mov     xBX, [xSI + CPUMCTX.dr6]
    mov     dr6, xBX
%endif

    ; Restore CR2
    mov     ebx, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* Note: assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [xSI + CPUMCTX.eax]
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     ebp, [xSI + CPUMCTX.ebp]

    ; resume or start?
    cmp     xDI, 0                  ; fResume
    je      .vmlauch_lauch

    ;/* Restore edi & esi. */
    mov     edi, [xSI + CPUMCTX.edi]
    mov     esi, [xSI + CPUMCTX.esi]

    vmresume
    jmp     .vmlaunch_done;      ;/* here if vmresume detected a failure. */
    
.vmlauch_lauch:    
    ;/* Restore edi & esi. */
    mov     edi, [xSI + CPUMCTX.edi]
    mov     esi, [xSI + CPUMCTX.esi]

    vmlaunch
    jmp     .vmlaunch_done;      ;/* here if vmlaunch detected a failure. */

ALIGNCODE(16)
.vmlaunch_done:
    jc      near .vmxstart_invalid_vmxon_ptr
    jz      near .vmxstart_start_failed

    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    push    xDI
    mov     xDI, [xSP + xS * 2]         ; pCtx

    mov     [ss:xDI + CPUMCTX.eax], eax
    mov     [ss:xDI + CPUMCTX.ebx], ebx
    mov     [ss:xDI + CPUMCTX.ecx], ecx
    mov     [ss:xDI + CPUMCTX.edx], edx
    mov     [ss:xDI + CPUMCTX.esi], esi
    mov     [ss:xDI + CPUMCTX.ebp], ebp
%ifdef RT_ARCH_AMD64
    pop     xAX                                 ; the guest edi we pushed above
    mov     dword [ss:xDI + CPUMCTX.edi], eax
%else
    pop     dword [ss:xDI + CPUMCTX.edi]        ; the guest edi we pushed above
%endif

%ifdef VBOX_WITH_DR6_EXPERIMENT
    ; Save DR6 - experiment, not safe!
    mov     xAX, dr6
    mov     [ss:xDI + CPUMCTX.dr6], xAX
%endif

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, xS      ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

.vmstart_end:
    popf
    pop     xBP
    ret


.vmxstart_invalid_vmxon_ptr:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, xS     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart_end

.vmxstart_start_failed:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, xS     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_UNABLE_TO_START_VM
    jmp     .vmstart_end

ENDPROC VMXR0StartVM32

%ifdef RT_ARCH_AMD64
;/**
; * Prepares for and executes VMLAUNCH/VMRESUME (32 bits guest mode)
; *
; * @returns VBox status code
; * @param   fResume    vmlauch/vmresume
; * @param   pCtx       Guest context
; */
BEGINPROC VMXR0StartVM64
    push    xBP
    mov     xBP, xSP

    pushf
    cli

    ;/* First we have to save some final CPU context registers. */
    mov     rax, qword .vmlaunch64_done
    push    rax
    mov     rax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite rax, [xSP]
    ;/* Note: assumes success... */
    add     xSP, xS

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
%ifdef ASM_CALL64_GCC
    ; fResume already in rdi
    ; pCtx    already in rsi
%else
    mov     rdi, rcx        ; fResume
    mov     rsi, rdx        ; pCtx
%endif

    ;/* Save segment registers */
    ; Note: MYPUSHSEGS trashes rdx & rcx, so we moved it here (msvc amd64 case)
    MYPUSHSEGS xAX, ax

    ; Save the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs and restore the guest MSRs
    ; @todo use the automatic load feature for MSRs
    LOADGUESTMSR MSR_K8_LSTAR, CPUMCTX.msrLSTAR
    LOADGUESTMSR MSR_K8_CSTAR, CPUMCTX.msrCSTAR
    LOADGUESTMSR MSR_K8_SF_MASK, CPUMCTX.msrSFMASK
    LOADGUESTMSR MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE

    ; Save the pCtx pointer
    push    xSI

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    xAX

    ; VMX only saves the base of the GDTR & IDTR and resets the limit to 0xffff; we must restore the limit correctly!
    sub     xSP, xS*2
    sgdt    [xSP]

    sub     xSP, xS*2
    sidt    [xSP]

%ifdef VBOX_WITH_DR6_EXPERIMENT
    ; Restore DR6 - experiment, not safe!
    mov     xBX, [xSI + CPUMCTX.dr6]
    mov     dr6, xBX
%endif

    ; Restore CR2
    mov     rbx, qword [xSI + CPUMCTX.cr2]
    mov     cr2, rbx

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* Note: assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     rax, qword [xSI + CPUMCTX.eax]
    mov     rbx, qword [xSI + CPUMCTX.ebx]
    mov     rcx, qword [xSI + CPUMCTX.ecx]
    mov     rdx, qword [xSI + CPUMCTX.edx]
    mov     rbp, qword [xSI + CPUMCTX.ebp]
    mov     r8,  qword [xSI + CPUMCTX.r8]
    mov     r9,  qword [xSI + CPUMCTX.r9]
    mov     r10, qword [xSI + CPUMCTX.r10]
    mov     r11, qword [xSI + CPUMCTX.r11]
    mov     r12, qword [xSI + CPUMCTX.r12]
    mov     r13, qword [xSI + CPUMCTX.r13]
    mov     r14, qword [xSI + CPUMCTX.r14]
    mov     r15, qword [xSI + CPUMCTX.r15]

    ; resume or start?
    cmp     xDI, 0                  ; fResume
    je      .vmlauch64_lauch

    ;/* Restore edi & esi. */
    mov     rdi, qword [xSI + CPUMCTX.edi]
    mov     rsi, qword [xSI + CPUMCTX.esi]

    vmresume
    jmp     .vmlaunch64_done;      ;/* here if vmresume detected a failure. */
    
.vmlauch64_lauch:    
    ;/* Restore rdi & rsi. */
    mov     rdi, qword [xSI + CPUMCTX.edi]
    mov     rsi, qword [xSI + CPUMCTX.esi]

    vmlaunch
    jmp     .vmlaunch64_done;      ;/* here if vmlaunch detected a failure. */

ALIGNCODE(16)
.vmlaunch64_done:
    jc      near .vmxstart64_invalid_vmxon_ptr
    jz      near .vmxstart64_start_failed

    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    push    xDI
    mov     xDI, [xSP + xS * 2]         ; pCtx

    mov     qword [xDI + CPUMCTX.eax], rax
    mov     qword [xDI + CPUMCTX.ebx], rbx
    mov     qword [xDI + CPUMCTX.ecx], rcx
    mov     qword [xDI + CPUMCTX.edx], rdx
    mov     qword [xDI + CPUMCTX.esi], rsi
    mov     qword [xDI + CPUMCTX.ebp], rbp
    mov     qword [xDI + CPUMCTX.r8],  r8
    mov     qword [xDI + CPUMCTX.r9],  r9
    mov     qword [xDI + CPUMCTX.r10], r10
    mov     qword [xDI + CPUMCTX.r11], r11
    mov     qword [xDI + CPUMCTX.r12], r12
    mov     qword [xDI + CPUMCTX.r13], r13
    mov     qword [xDI + CPUMCTX.r14], r14
    mov     qword [xDI + CPUMCTX.r15], r15

    pop     xAX                                 ; the guest edi we pushed above
    mov     qword [xDI + CPUMCTX.edi], rax

%ifdef VBOX_WITH_DR6_EXPERIMENT
    ; Save DR6 - experiment, not safe!
    mov     xAX, dr6
    mov     [xDI + CPUMCTX.dr6], xAX
%endif

    pop     xAX         ; saved LDTR
    lldt    ax

    pop     xSI         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE
    LOADHOSTMSR MSR_K8_SF_MASK
    LOADHOSTMSR MSR_K8_CSTAR
    LOADHOSTMSR MSR_K8_LSTAR

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

.vmstart64_end:
    popf
    pop     xBP
    ret


.vmxstart64_invalid_vmxon_ptr:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    pop     xAX         ; saved LDTR
    lldt    ax

    pop     xSI         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE
    LOADHOSTMSR MSR_K8_SF_MASK
    LOADHOSTMSR MSR_K8_CSTAR
    LOADHOSTMSR MSR_K8_LSTAR

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart64_end

.vmxstart64_start_failed:
    ; Restore base and limit of the IDTR & GDTR
    lidt    [xSP]
    add     xSP, xS*2
    lgdt    [xSP]
    add     xSP, xS*2

    pop     xAX         ; saved LDTR
    lldt    ax

    pop     xSI         ; pCtx (needed in rsi by the macros below)

    ; Restore the host LSTAR, CSTAR, SFMASK & KERNEL_GSBASE MSRs
    ; @todo use the automatic load feature for MSRs
    LOADHOSTMSREX MSR_K8_KERNEL_GS_BASE, CPUMCTX.msrKERNELGSBASE
    LOADHOSTMSR MSR_K8_SF_MASK
    LOADHOSTMSR MSR_K8_CSTAR
    LOADHOSTMSR MSR_K8_LSTAR

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_UNABLE_TO_START_VM
    jmp     .vmstart64_end
ENDPROC VMXR0StartVM64

;/**
; * Executes VMWRITE
; *
; * @returns VBox status code
; * @param   idxField   x86: [ebp + 08h]  msc: rcx  gcc: rdi   VMCS index
; * @param   pData      x86: [ebp + 0ch]  msc: rdx  gcc: rsi   VM field value
; */
BEGINPROC VMXWriteVMCS64
%ifdef ASM_CALL64_GCC
    mov         eax, 0ffffffffh
    and         rdi, rax
    xor         rax, rax
    vmwrite     rdi, rsi
%else
    mov         eax, 0ffffffffh
    and         rcx, rax
    xor         rax, rax
    vmwrite     rcx, rdx
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret
ENDPROC VMXWriteVMCS64

;/**
; * Executes VMREAD
; *
; * @returns VBox status code
; * @param   idxField        VMCS index
; * @param   pData           Ptr to store VM field value
; */
;DECLASM(int) VMXReadVMCS64(uint32_t idxField, uint64_t *pData);
BEGINPROC VMXReadVMCS64
%ifdef ASM_CALL64_GCC
    mov         eax, 0ffffffffh
    and         rdi, rax
    xor         rax, rax
    vmread      [rsi], rdi
%else
    mov         eax, 0ffffffffh
    and         rcx, rax
    xor         rax, rax
    vmread      [rdx], rcx
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret
ENDPROC VMXReadVMCS64


;/**
; * Executes VMXON
; *
; * @returns VBox status code
; * @param   HCPhysVMXOn      Physical address of VMXON structure
; */
;DECLASM(int) VMXEnable(RTHCPHYS HCPhysVMXOn);
BEGINPROC VMXEnable
%ifdef RT_ARCH_AMD64
    xor     rax, rax
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmxon   [rsp]
%else
    xor     eax, eax
    vmxon   [esp + 4]
%endif
    jnc     .good
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .the_end

.good:
    jnz     .the_end
    mov     eax, VERR_VMX_GENERIC

.the_end:
%ifdef RT_ARCH_AMD64
    add     rsp, 8
%endif
    ret
ENDPROC VMXEnable


;/**
; * Executes VMXOFF
; */
;DECLASM(void) VMXDisable(void);
BEGINPROC VMXDisable
    vmxoff
    ret
ENDPROC VMXDisable


;/**
; * Executes VMCLEAR
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VM control structure
; */
;DECLASM(int) VMXClearVMCS(RTHCPHYS HCPhysVMCS);
BEGINPROC VMXClearVMCS
%ifdef RT_ARCH_AMD64
    xor     rax, rax
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmclear [rsp]
%else
    xor     eax, eax
    vmclear [esp + 4]
%endif
    jnc     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
.the_end:
%ifdef RT_ARCH_AMD64
    add     rsp, 8
%endif
    ret
ENDPROC VMXClearVMCS


;/**
; * Executes VMPTRLD
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VMCS structure
; */
;DECLASM(int) VMXActivateVMCS(RTHCPHYS HCPhysVMCS);
BEGINPROC VMXActivateVMCS
%ifdef RT_ARCH_AMD64
    xor     rax, rax
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmptrld [rsp]
%else
    xor     eax, eax
    vmptrld [esp + 4]
%endif
    jnc     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
.the_end:
%ifdef RT_ARCH_AMD64
    add     rsp, 8
%endif
    ret
ENDPROC VMXActivateVMCS

%endif ; RT_ARCH_AMD64


;/**
; * Prepares for and executes VMRUN
; *
; * @returns VBox status code
; * @param   HCPhysVMCB     Physical address of host VMCB
; * @param   HCPhysVMCB     Physical address of guest VMCB
; * @param   pCtx           Guest context
; */
BEGINPROC SVMVMRun
%ifdef RT_ARCH_AMD64 ; fake a cdecl stack frame - I'm lazy, sosume.
 %ifdef ASM_CALL64_GCC
    push    rdx
    push    rsi
    push    rdi
 %else
    push    r8
    push    rdx
    push    rcx
 %endif
    push    0
%endif
    push    xBP
    mov     xBP, xSP
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
    mov     xSI, [xBP + xS*2 + RTHCPHYS_CB*2]   ; pCtx
    push    xSI                     ; push for saving the state at the end

    ; Restore CR2
    mov     ebx, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    ; save host fs, gs, sysenter msr etc
    mov     xAX, [xBP + xS*2]       ; pVMCBHostPhys (64 bits physical address; x86: take low dword only)
    push    xAX                     ; save for the vmload after vmrun
    vmsave

    ; setup eax for VMLOAD
    mov     xAX, [xBP + xS*2 + RTHCPHYS_CB]     ; pVMCBPhys (64 bits physical address; take low dword only)

    ;/* Restore Guest's general purpose registers. */
    ;/* EAX is loaded from the VMCB by VMRUN */
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
    mov     esi, [xSI + CPUMCTX.esi]

    ; Clear the global interrupt flag & execute sti to make sure external interrupts cause a world switch
    clgi
    sti

    ; load guest fs, gs, sysenter msr etc
    vmload
    ; run the VM
    vmrun

    ;/* EAX is in the VMCB already; we can use it here. */

    ; save guest fs, gs, sysenter msr etc
    vmsave

    ; load host fs, gs, sysenter msr etc
    pop     xAX                     ; pushed above
    vmload

    ; Set the global interrupt flag again, but execute cli to make sure IF=0.
    cli
    stgi

    pop     xAX                     ; pCtx

    mov     [ss:xAX + CPUMCTX.ebx], ebx
    mov     [ss:xAX + CPUMCTX.ecx], ecx
    mov     [ss:xAX + CPUMCTX.edx], edx
    mov     [ss:xAX + CPUMCTX.esi], esi
    mov     [ss:xAX + CPUMCTX.edi], edi
    mov     [ss:xAX + CPUMCTX.ebp], ebp

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

    popf
    pop     xBP
%ifdef RT_ARCH_AMD64
    add     xSP, 4*xS
%endif
    ret
ENDPROC SVMVMRun


;;
; Executes INVLPGA
;
; @param   pPageGC  msc:ecx  gcc:edi  x86:[esp+04]  Virtual page to invalidate
; @param   uASID    msc:edx  gcc:esi  x86:[esp+08]  Tagged TLB id
;
;DECLASM(void) SVMInvlpgA(RTGCPTR pPageGC, uint32_t uASID);
BEGINPROC SVMInvlpgA
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     eax, edi                    ;; @todo 64-bit guest.
    mov     ecx, esi
 %else
    mov     eax, ecx                    ;; @todo 64-bit guest.
    mov     ecx, edx
 %endif
%else
    mov     eax, [esp + 4]
    mov     ecx, [esp + 8]
%endif
    invlpga [xAX], ecx
    ret
ENDPROC SVMInvlpgA

