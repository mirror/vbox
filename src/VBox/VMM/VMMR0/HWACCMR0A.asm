; $Id$
;; @file
; VMXM - R0 vmx helpers
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/hwacc_vmx.mac"
%include "VBox/cpum.mac"
%include "VBox/x86.mac"

%ifdef RT_OS_OS2 ;; @todo build cvs nasm like on OS X.
 %macro vmwrite 2,
    int3
 %endmacro
 %define vmlaunch int3
 %define vmresume int3
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

%ifdef RT_ARCH_AMD64
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

 %macro MYPUSHSEGS 2
    mov     %2, es
    push    %1
    mov     %2, ds
    push    %1
    push    fs
    ; Special case for GS; OSes typically use swapgs to reset the hidden base register for GS on entry into the kernel. The same happens on exit
    push    rcx
    mov     ecx, MSR_K8_GS_BASE
    rdmsr
    pop     rcx
    push    rdx
    push    rax
    push    gs
 %endmacro

 %macro MYPOPSEGS 2
    ; Note: do not step through this code with a debugger!
    pop     gs
    pop     rax
    pop     rdx
    push    rcx
    mov     ecx, MSR_K8_GS_BASE
    wrmsr
    pop     rcx
    ; Now it's safe to step again

    pop     fs
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
; * Prepares for and executes VMLAUNCH
; *
; * @note identical to VMXResumeVM, except for the vmlaunch/vmresume opcode
; *
; * @returns VBox status code
; * @param   pCtx        Guest context
; */
BEGINPROC VMXStartVM
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
    ;/* @todo assumes success... */
    add     xSP, xS

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; * - EFLAGS (reset to BIT(1); not relevant)
    ; *
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save segment registers */
    MYPUSHSEGS xAX, ax

    ;/* Save the Guest CPU context pointer. */
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     rsi, rdi ; pCtx
 %else
    mov     rsi, rcx ; pCtx
 %endif
%else
    mov     esi, [ebp + 8] ; pCtx
%endif
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

    ; Restore CR2
    mov     ebx, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [xSI + CPUMCTX.eax]
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
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

ENDPROC VMXStartVM


;/**
; * Prepares for and executes VMRESUME
; *
; * @note identical to VMXStartVM, except for the vmlaunch/vmresume opcode
; *
; * @returns VBox status code
; * @param   pCtx        Guest context
; */
BEGINPROC VMXResumeVM
    push    xBP
    mov     xBP, xSP

    pushf
    cli

    ;/* First we have to save some final CPU context registers. */
%ifdef RT_ARCH_AMD64
    mov     rax, qword .vmresume_done
    push    rax
%else
    push    .vmresume_done
%endif
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite xAX, [xSP]
    ;/* @todo assumes success... */
    add     xSP, xS

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; * - EFLAGS (reset to BIT(1); not relevant)
    ; *
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save segment registers */
    MYPUSHSEGS xAX, ax

    ;/* Save the Guest CPU context pointer. */
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     rsi, rdi        ; pCtx
 %else
    mov     rsi, rcx        ; pCtx
 %endif
%else
    mov     esi, [ebp + 8]  ; pCtx
%endif
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

    ; Restore CR2
    mov     xBX, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [xSI + CPUMCTX.eax]
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
    mov     esi, [xSI + CPUMCTX.esi]

    vmresume
    jmp     .vmresume_done;      ;/* here if vmresume detected a failure. */

ALIGNCODE(16)
.vmresume_done:
    jc      near .vmxresume_invalid_vmxon_ptr
    jz      near .vmxresume_start_failed

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

    pop     xAX          ; saved LDTR
    lldt    ax

    add     xSP, xS      ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

.vmresume_end:
    popf
    pop     xBP
    ret

.vmxresume_invalid_vmxon_ptr:
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
    jmp     .vmresume_end

.vmxresume_start_failed:
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
    mov     eax, VERR_VMX_UNABLE_TO_RESUME_VM
    jmp     .vmresume_end

ENDPROC VMXResumeVM


%ifdef RT_ARCH_AMD64
;/**
; * Executes VMWRITE
; *
; * @returns VBox status code
; * @param   idxField   x86: [ebp + 08h]  msc: rcx  gcc: edi   VMCS index
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
    DB      0x0F, 0x01, 0xDB        ; VMSAVE

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
    DB      0x0f, 0x01, 0xDD        ; CLGI
    sti

    ; load guest fs, gs, sysenter msr etc
    DB      0x0f, 0x01, 0xDA        ; VMLOAD
    ; run the VM
    DB      0x0F, 0x01, 0xD8        ; VMRUN

    ;/* EAX is in the VMCB already; we can use it here. */

    ; save guest fs, gs, sysenter msr etc
    DB      0x0F, 0x01, 0xDB        ; VMSAVE

    ; load host fs, gs, sysenter msr etc
    pop     xAX                     ; pushed above
    DB      0x0F, 0x01, 0xDA        ; VMLOAD

    ; Set the global interrupt flag again, but execute cli to make sure IF=0.
    cli
    DB      0x0f, 0x01, 0xDC        ; STGI

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

    pop     xBP
%ifdef RT_ARCH_AMD64
    add     xSP, 4*xS
%endif
    ret
ENDPROC SVMVMRun

%ifdef RT_ARCH_AMD64
%ifdef RT_OS_WINDOWS

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
    invlpga rax, ecx
%else
    mov     eax, [esp + 4]
    mov     ecx, [esp + 8]
    invlpga eax, ecx
%endif
    ret
ENDPROC SVMInvlpgA
%endif
%endif
