; $Id$
;; @file
; VMXM - R0 vmx helpers
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
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/vmm/hm_vmx.mac"
%include "VBox/vmm/cpum.mac"
%include "iprt/x86.mac"
%include "HMInternal.mac"

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

;*******************************************************************************
;*  Defined Constants And Macros                                               *
;*******************************************************************************
%ifdef RT_ARCH_AMD64
 %define MAYBE_64_BIT
%endif
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
 %define MAYBE_64_BIT
%else
 %ifdef RT_OS_DARWIN
  %ifdef RT_ARCH_AMD64
   ;;
   ; Load the NULL selector into DS, ES, FS and GS on 64-bit darwin so we don't
   ; risk loading a stale LDT value or something invalid.
   %define HM_64_BIT_USE_NULL_SEL
  %endif
 %endif
%endif

;; The offset of the XMM registers in X86FXSTATE.
; Use define because I'm too lazy to convert the struct.
%define XMM_OFF_IN_X86FXSTATE   160


;; This is too risky wrt. stability, performance and correctness.
;%define VBOX_WITH_DR6_EXPERIMENT 1

;; @def MYPUSHAD
; Macro generating an equivalent to pushad

;; @def MYPOPAD
; Macro generating an equivalent to popad

;; @def MYPUSHSEGS
; Macro saving all segment registers on the stack.
; @param 1  full width register name
; @param 2  16-bit register name for \a 1.

;; @def MYPOPSEGS
; Macro restoring all segment registers on the stack
; @param 1  full width register name
; @param 2  16-bit register name for \a 1.

%ifdef MAYBE_64_BIT
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
%endif

%ifdef ASM_CALL64_GCC
 %macro MYPUSHAD64 0
   push    r15
   push    r14
   push    r13
   push    r12
   push    rbx
 %endmacro
 %macro MYPOPAD64 0
   pop     rbx
   pop     r12
   pop     r13
   pop     r14
   pop     r15
 %endmacro

%else ; ASM_CALL64_MSC
 %macro MYPUSHAD64 0
   push    r15
   push    r14
   push    r13
   push    r12
   push    rbx
   push    rsi
   push    rdi
 %endmacro
 %macro MYPOPAD64 0
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
%macro MYPUSHSEGS64 2
 %ifndef HM_64_BIT_USE_NULL_SEL
   mov     %2, es
   push    %1
   mov     %2, ds
   push    %1
 %endif

   ; Special case for FS; Windows and Linux either don't use it or restore it when leaving kernel mode, Solaris OTOH doesn't and we must save it.
   mov     ecx, MSR_K8_FS_BASE
   rdmsr
   push    rdx
   push    rax
 %ifndef HM_64_BIT_USE_NULL_SEL
   push    fs
 %endif

   ; Special case for GS; OSes typically use swapgs to reset the hidden base register for GS on entry into the kernel. The same happens on exit
   mov     ecx, MSR_K8_GS_BASE
   rdmsr
   push    rdx
   push    rax
 %ifndef HM_64_BIT_USE_NULL_SEL
   push    gs
 %endif
%endmacro

; trashes, rax, rdx & rcx
%macro MYPOPSEGS64 2
   ; Note: do not step through this code with a debugger!
 %ifndef HM_64_BIT_USE_NULL_SEL
   xor     eax, eax
   mov     ds, ax
   mov     es, ax
   mov     fs, ax
   mov     gs, ax
 %endif

 %ifndef HM_64_BIT_USE_NULL_SEL
   pop     gs
 %endif
   pop     rax
   pop     rdx
   mov     ecx, MSR_K8_GS_BASE
   wrmsr

 %ifndef HM_64_BIT_USE_NULL_SEL
   pop     fs
 %endif
   pop     rax
   pop     rdx
   mov     ecx, MSR_K8_FS_BASE
   wrmsr
   ; Now it's safe to step again

 %ifndef HM_64_BIT_USE_NULL_SEL
   pop     %1
   mov     ds, %2
   pop     %1
   mov     es, %2
 %endif
%endmacro

%macro MYPUSHAD32 0
  pushad
%endmacro
%macro MYPOPAD32 0
  popad
%endmacro

%macro MYPUSHSEGS32 2
  push    ds
  push    es
  push    fs
  push    gs
%endmacro
%macro MYPOPSEGS32 2
  pop     gs
  pop     fs
  pop     es
  pop     ds
%endmacro


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
%ifdef VBOX_WITH_KERNEL_USING_XMM
extern NAME(CPUMIsGuestFPUStateActive)
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
GLOBALNAME g_fVMXIs64bitHost
    dd  NAME(SUPR0AbsIs64bit)
%endif


BEGINCODE


;/**
; * Executes VMWRITE, 64-bit value.
; *
; * @returns VBox status code
; * @param   idxField   x86: [ebp + 08h]  msc: rcx  gcc: rdi   VMCS index
; * @param   u64Data    x86: [ebp + 0ch]  msc: rdx  gcc: rsi   VM field value
; */
ALIGNCODE(16)
BEGINPROC VMXWriteVMCS64
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh
    xor         rax, rax
    vmwrite     rdi, rsi
 %else
    and         ecx, 0ffffffffh
    xor         rax, rax
    vmwrite     rcx, rdx
 %endif
%else  ; RT_ARCH_X86
    mov         ecx, [esp + 4]          ; idxField
    lea         edx, [esp + 8]          ; &u64Data
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp         byte [NAME(g_fVMXIs64bitHost)], 0
    jz          .legacy_mode
    db          0xea                    ; jmp far .sixtyfourbit_mode
    dd          .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    vmwrite     ecx, [edx]              ; low dword
    jz          .done
    jc          .done
    inc         ecx
    xor         eax, eax
    vmwrite     ecx, [edx + 4]          ; high dword
.done:
%endif ; RT_ARCH_X86
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    and     ecx, 0ffffffffh
    xor     eax, eax
    vmwrite rcx, [rdx]
    mov     r8d, VERR_VMX_INVALID_VMCS_FIELD
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXWriteVMCS64


;/**
; * Executes VMREAD, 64-bit value
; *
; * @returns VBox status code
; * @param   idxField        VMCS index
; * @param   pData           Ptr to store VM field value
; */
;DECLASM(int) VMXReadVMCS64(uint32_t idxField, uint64_t *pData);
ALIGNCODE(16)
BEGINPROC VMXReadVMCS64
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh
    xor         rax, rax
    vmread      [rsi], rdi
 %else
    and         ecx, 0ffffffffh
    xor         rax, rax
    vmread      [rdx], rcx
 %endif
%else  ; RT_ARCH_X86
    mov         ecx, [esp + 4]          ; idxField
    mov         edx, [esp + 8]          ; pData
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp         byte [NAME(g_fVMXIs64bitHost)], 0
    jz          .legacy_mode
    db          0xea                    ; jmp far .sixtyfourbit_mode
    dd          .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    vmread      [edx], ecx              ; low dword
    jz          .done
    jc          .done
    inc         ecx
    xor         eax, eax
    vmread      [edx + 4], ecx          ; high dword
.done:
%endif ; RT_ARCH_X86
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    and     ecx, 0ffffffffh
    xor     eax, eax
    vmread  [rdx], rcx
    mov     r8d, VERR_VMX_INVALID_VMCS_FIELD
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXReadVMCS64


;/**
; * Executes VMREAD, 32-bit value.
; *
; * @returns VBox status code
; * @param   idxField        VMCS index
; * @param   pu32Data        Ptr to store VM field value
; */
;DECLASM(int) VMXReadVMCS32(uint32_t idxField, uint32_t *pu32Data);
ALIGNCODE(16)
BEGINPROC VMXReadVMCS32
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and     edi, 0ffffffffh
    xor     rax, rax
    vmread  r10, rdi
    mov     [rsi], r10d
 %else
    and     ecx, 0ffffffffh
    xor     rax, rax
    vmread  r10, rcx
    mov     [rdx], r10d
 %endif
%else  ; RT_ARCH_X86
    mov     ecx, [esp + 4]              ; idxField
    mov     edx, [esp + 8]              ; pu32Data
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    xor     eax, eax
    vmread  [edx], ecx
%endif ; RT_ARCH_X86
    jnc     .valid_vmcs
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    and     ecx, 0ffffffffh
    xor     eax, eax
    vmread  r10, rcx
    mov     [rdx], r10d
    mov     r8d, VERR_VMX_INVALID_VMCS_FIELD
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXReadVMCS32


;/**
; * Executes VMWRITE, 32-bit value.
; *
; * @returns VBox status code
; * @param   idxField        VMCS index
; * @param   u32Data         Ptr to store VM field value
; */
;DECLASM(int) VMXWriteVMCS32(uint32_t idxField, uint32_t u32Data);
ALIGNCODE(16)
BEGINPROC VMXWriteVMCS32
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and     edi, 0ffffffffh
    and     esi, 0ffffffffh
    xor     rax, rax
    vmwrite rdi, rsi
 %else
    and     ecx, 0ffffffffh
    and     edx, 0ffffffffh
    xor     rax, rax
    vmwrite rcx, rdx
 %endif
%else  ; RT_ARCH_X86
    mov     ecx, [esp + 4]              ; idxField
    mov     edx, [esp + 8]              ; u32Data
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    xor     eax, eax
    vmwrite ecx, edx
%endif ; RT_ARCH_X86
    jnc     .valid_vmcs
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     edx, 0ffffffffh
    and     ecx, 0ffffffffh
    xor     eax, eax
    vmwrite rcx, rdx
    mov     r8d, VERR_VMX_INVALID_VMCS_FIELD
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXWriteVMCS32


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
%else  ; RT_ARCH_X86
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    xor     eax, eax
    vmxon   [esp + 4]
%endif ; RT_ARCH_X86
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

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    lea     rdx, [rsp + 4]              ; &HCPhysVMXOn.
    and     edx, 0ffffffffh
    xor     eax, eax
    vmxon   [rdx]
    mov     r8d, VERR_INVALID_PARAMETER
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXEnable


;/**
; * Executes VMXOFF
; */
;DECLASM(void) VMXDisable(void);
BEGINPROC VMXDisable
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    vmxoff
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    vmxoff
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXDisable


;/**
; * Executes VMCLEAR
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VM control structure
; */
;DECLASM(int) VMXClearVMCS(RTHCPHYS HCPhysVMCS);
ALIGNCODE(16)
BEGINPROC VMXClearVMCS
%ifdef RT_ARCH_AMD64
    xor     rax, rax
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmclear [rsp]
%else  ; RT_ARCH_X86
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    xor     eax, eax
    vmclear [esp + 4]
%endif ; RT_ARCH_X86
    jnc     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
.the_end:
%ifdef RT_ARCH_AMD64
    add     rsp, 8
%endif
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    lea     rdx, [rsp + 4]              ; &HCPhysVMCS
    and     edx, 0ffffffffh
    xor     eax, eax
    vmclear [rdx]
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif
ENDPROC VMXClearVMCS


;/**
; * Executes VMPTRLD
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VMCS structure
; */
;DECLASM(int) VMXActivateVMCS(RTHCPHYS HCPhysVMCS);
ALIGNCODE(16)
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
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
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

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    lea     rdx, [rsp + 4]              ; &HCPhysVMCS
    and     edx, 0ffffffffh
    xor     eax, eax
    vmptrld [rdx]
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXActivateVMCS


;/**
; * Executes VMPTRST
; *
; * @returns VBox status code
; * @param    [esp + 04h]  gcc:rdi  msc:rcx   Param 1 - First parameter - Address that will receive the current pointer
; */
;DECLASM(int) VMXGetActivateVMCS(RTHCPHYS *pVMCS);
BEGINPROC VMXGetActivateVMCS
%ifdef RT_OS_OS2
    mov     eax, VERR_NOT_SUPPORTED
    ret
%else
 %ifdef RT_ARCH_AMD64
  %ifdef ASM_CALL64_GCC
    vmptrst qword [rdi]
  %else
    vmptrst qword [rcx]
  %endif
 %else
  %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
  %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    vmptrst qword [esp+04h]
 %endif
    xor     eax, eax
.the_end:
    ret

 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    lea     rdx, [rsp + 4]              ; &HCPhysVMCS
    and     edx, 0ffffffffh
    vmptrst qword [rdx]
    xor     eax, eax
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
%endif
ENDPROC VMXGetActivateVMCS

;/**
; * Invalidate a page using invept
; @param   enmFlush     msc:ecx  gcc:edi  x86:[esp+04]  Type of flush
; @param   pDescriptor  msc:edx  gcc:esi  x86:[esp+08]  Descriptor pointer
; */
;DECLASM(int) VMXR0InvEPT(VMX_FLUSH enmFlush, uint64_t *pDescriptor);
BEGINPROC VMXR0InvEPT
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh
    xor         rax, rax
;    invept      rdi, qword [rsi]
    DB          0x66, 0x0F, 0x38, 0x80, 0x3E
 %else
    and         ecx, 0ffffffffh
    xor         rax, rax
;    invept      rcx, qword [rdx]
    DB          0x66, 0x0F, 0x38, 0x80, 0xA
 %endif
%else
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp         byte [NAME(g_fVMXIs64bitHost)], 0
    jz          .legacy_mode
    db          0xea                        ; jmp far .sixtyfourbit_mode
    dd          .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    mov         ecx, [esp + 4]
    mov         edx, [esp + 8]
    xor         eax, eax
;    invept      ecx, qword [edx]
    DB          0x66, 0x0F, 0x38, 0x80, 0xA
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_INVALID_PARAMETER
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     esp, 0ffffffffh
    mov     ecx, [rsp + 4]              ; enmFlush
    mov     edx, [rsp + 8]              ; pDescriptor
    xor     eax, eax
;    invept  rcx, qword [rdx]
    DB      0x66, 0x0F, 0x38, 0x80, 0xA
    mov     r8d, VERR_INVALID_PARAMETER
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXR0InvEPT


;/**
; * Invalidate a page using invvpid
; @param   enmFlush     msc:ecx  gcc:edi  x86:[esp+04]  Type of flush
; @param   pDescriptor  msc:edx  gcc:esi  x86:[esp+08]  Descriptor pointer
; */
;DECLASM(int) VMXR0InvVPID(VMX_FLUSH enmFlush, uint64_t *pDescriptor);
BEGINPROC VMXR0InvVPID
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh
    xor         rax, rax
;    invvpid     rdi, qword [rsi]
    DB          0x66, 0x0F, 0x38, 0x81, 0x3E
 %else
    and         ecx, 0ffffffffh
    xor         rax, rax
;    invvpid     rcx, qword [rdx]
    DB          0x66, 0x0F, 0x38, 0x81, 0xA
 %endif
%else
 %ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    jz      .legacy_mode
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.legacy_mode:
 %endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
    mov         ecx, [esp + 4]
    mov         edx, [esp + 8]
    xor         eax, eax
;    invvpid     ecx, qword [edx]
    DB          0x66, 0x0F, 0x38, 0x81, 0xA
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_INVALID_PARAMETER
.the_end:
    ret

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     esp, 0ffffffffh
    mov     ecx, [rsp + 4]              ; enmFlush
    mov     edx, [rsp + 8]              ; pDescriptor
    xor     eax, eax
;    invvpid rcx, qword [rdx]
    DB      0x66, 0x0F, 0x38, 0x81, 0xA
    mov     r8d, VERR_INVALID_PARAMETER
    cmovz   eax, r8d
    mov     r9d, VERR_VMX_INVALID_VMCS_PTR
    cmovc   eax, r9d
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
ENDPROC VMXR0InvVPID


%if GC_ARCH_BITS == 64
;;
; Executes INVLPGA
;
; @param   pPageGC  msc:rcx  gcc:rdi  x86:[esp+04]  Virtual page to invalidate
; @param   uASID    msc:rdx  gcc:rsi  x86:[esp+0C]  Tagged TLB id
;
;DECLASM(void) SVMR0InvlpgA(RTGCPTR pPageGC, uint32_t uASID);
BEGINPROC SVMR0InvlpgA
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    mov     rax, rdi
    mov     rcx, rsi
 %else
    mov     rax, rcx
    mov     rcx, rdx
 %endif
%else
    mov     eax, [esp + 4]
    mov     ecx, [esp + 0Ch]
%endif
    invlpga [xAX], ecx
    ret
ENDPROC SVMR0InvlpgA

%else ; GC_ARCH_BITS != 64
;;
; Executes INVLPGA
;
; @param   pPageGC  msc:ecx  gcc:edi  x86:[esp+04]  Virtual page to invalidate
; @param   uASID    msc:edx  gcc:esi  x86:[esp+08]  Tagged TLB id
;
;DECLASM(void) SVMR0InvlpgA(RTGCPTR pPageGC, uint32_t uASID);
BEGINPROC SVMR0InvlpgA
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
    movzx   rax, edi
    mov     ecx, esi
 %else
    ; from http://www.cs.cmu.edu/~fp/courses/15213-s06/misc/asm64-handout.pdf:
    ; ``Perhaps unexpectedly, instructions that move or generate 32-bit register
    ;   values also set the upper 32 bits of the register to zero. Consequently
    ;   there is no need for an instruction movzlq.''
    mov     eax, ecx
    mov     ecx, edx
 %endif
%else
    mov     eax, [esp + 4]
    mov     ecx, [esp + 8]
%endif
    invlpga [xAX], ecx
    ret
ENDPROC SVMR0InvlpgA

%endif ; GC_ARCH_BITS != 64

%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL

;/**
; * Gets 64-bit GDTR and IDTR on darwin.
; * @param  pGdtr        Where to store the 64-bit GDTR.
; * @param  pIdtr        Where to store the 64-bit IDTR.
; */
;DECLASM(void) hmR0Get64bitGdtrAndIdtr(PX86XDTR64 pGdtr, PX86XDTR64 pIdtr);
ALIGNCODE(16)
BEGINPROC hmR0Get64bitGdtrAndIdtr
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.the_end:
    ret

ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    and     esp, 0ffffffffh
    mov     ecx, [rsp + 4]              ; pGdtr
    mov     edx, [rsp + 8]              ; pIdtr
    sgdt    [rcx]
    sidt    [rdx]
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
ENDPROC   hmR0Get64bitGdtrAndIdtr


;/**
; * Gets 64-bit CR3 on darwin.
; * @returns CR3
; */
;DECLASM(uint64_t) hmR0Get64bitCR3(void);
ALIGNCODE(16)
BEGINPROC hmR0Get64bitCR3
    db      0xea                        ; jmp far .sixtyfourbit_mode
    dd      .sixtyfourbit_mode, NAME(SUPR0Abs64bitKernelCS)
.the_end:
    ret

ALIGNCODE(16)
BITS 64
.sixtyfourbit_mode:
    mov     rax, cr3
    mov     rdx, rax
    shr     rdx, 32
    jmp far [.fpret wrt rip]
.fpret:                                 ; 16:32 Pointer to .the_end.
    dd      .the_end, NAME(SUPR0AbsKernelCS)
BITS 32
ENDPROC   hmR0Get64bitCR3

%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL

%ifdef VBOX_WITH_KERNEL_USING_XMM

;;
; Wrapper around vmx.pfnStartVM that preserves host XMM registers and
; load the guest ones when necessary.
;
; @cproto       DECLASM(int) hmR0VMXStartVMWrapXMM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu, PFNHMVMXSTARTVM pfnStartVM);
;
; @returns      eax
;
; @param        fResumeVM       msc:rcx
; @param        pCtx            msc:rdx
; @param        pVMCSCache      msc:r8
; @param        pVM             msc:r9
; @param        pVCpu           msc:[rbp+30h]
; @param        pfnStartVM      msc:[rbp+38h]
;
; @remarks      This is essentially the same code as hmR0SVMRunWrapXMM, only the parameters differ a little bit.
;
; ASSUMING 64-bit and windows for now.
ALIGNCODE(16)
BEGINPROC hmR0VMXStartVMWrapXMM
        push    xBP
        mov     xBP, xSP
        sub     xSP, 0a0h + 040h        ; Don't bother optimizing the frame size.

        ; spill input parameters.
        mov     [xBP + 010h], rcx       ; fResumeVM
        mov     [xBP + 018h], rdx       ; pCtx
        mov     [xBP + 020h], r8        ; pVMCSCache
        mov     [xBP + 028h], r9        ; pVM

        ; Ask CPUM whether we've started using the FPU yet.
        mov     rcx, [xBP + 30h]        ; pVCpu
        call    NAME(CPUMIsGuestFPUStateActive)
        test    al, al
        jnz     .guest_fpu_state_active

        ; No need to mess with XMM registers just call the start routine and return.
        mov     r11, [xBP + 38h]        ; pfnStartVM
        mov     r10, [xBP + 30h]        ; pVCpu
        mov     [xSP + 020h], r10
        mov     rcx, [xBP + 010h]       ; fResumeVM
        mov     rdx, [xBP + 018h]       ; pCtx
        mov     r8,  [xBP + 020h]       ; pVMCSCache
        mov     r9,  [xBP + 028h]       ; pVM
        call    r11

        leave
        ret

ALIGNCODE(8)
.guest_fpu_state_active:
        ; Save the host XMM registers.
        movdqa  [rsp + 040h + 000h], xmm6
        movdqa  [rsp + 040h + 010h], xmm7
        movdqa  [rsp + 040h + 020h], xmm8
        movdqa  [rsp + 040h + 030h], xmm9
        movdqa  [rsp + 040h + 040h], xmm10
        movdqa  [rsp + 040h + 050h], xmm11
        movdqa  [rsp + 040h + 060h], xmm12
        movdqa  [rsp + 040h + 070h], xmm13
        movdqa  [rsp + 040h + 080h], xmm14
        movdqa  [rsp + 040h + 090h], xmm15

        ; Load the full guest XMM register state.
        mov     r10, [xBP + 018h]       ; pCtx
        lea     r10, [r10 + XMM_OFF_IN_X86FXSTATE]
        movdqa  xmm0,  [r10 + 000h]
        movdqa  xmm1,  [r10 + 010h]
        movdqa  xmm2,  [r10 + 020h]
        movdqa  xmm3,  [r10 + 030h]
        movdqa  xmm4,  [r10 + 040h]
        movdqa  xmm5,  [r10 + 050h]
        movdqa  xmm6,  [r10 + 060h]
        movdqa  xmm7,  [r10 + 070h]
        movdqa  xmm8,  [r10 + 080h]
        movdqa  xmm9,  [r10 + 090h]
        movdqa  xmm10, [r10 + 0a0h]
        movdqa  xmm11, [r10 + 0b0h]
        movdqa  xmm12, [r10 + 0c0h]
        movdqa  xmm13, [r10 + 0d0h]
        movdqa  xmm14, [r10 + 0e0h]
        movdqa  xmm15, [r10 + 0f0h]

        ; Make the call (same as in the other case ).
        mov     r11, [xBP + 38h]        ; pfnStartVM
        mov     r10, [xBP + 30h]        ; pVCpu
        mov     [xSP + 020h], r10
        mov     rcx, [xBP + 010h]       ; fResumeVM
        mov     rdx, [xBP + 018h]       ; pCtx
        mov     r8,  [xBP + 020h]       ; pVMCSCache
        mov     r9,  [xBP + 028h]       ; pVM
        call    r11

        ; Save the guest XMM registers.
        mov     r10, [xBP + 018h]       ; pCtx
        lea     r10, [r10 + XMM_OFF_IN_X86FXSTATE]
        movdqa  [r10 + 000h], xmm0
        movdqa  [r10 + 010h], xmm1
        movdqa  [r10 + 020h], xmm2
        movdqa  [r10 + 030h], xmm3
        movdqa  [r10 + 040h], xmm4
        movdqa  [r10 + 050h], xmm5
        movdqa  [r10 + 060h], xmm6
        movdqa  [r10 + 070h], xmm7
        movdqa  [r10 + 080h], xmm8
        movdqa  [r10 + 090h], xmm9
        movdqa  [r10 + 0a0h], xmm10
        movdqa  [r10 + 0b0h], xmm11
        movdqa  [r10 + 0c0h], xmm12
        movdqa  [r10 + 0d0h], xmm13
        movdqa  [r10 + 0e0h], xmm14
        movdqa  [r10 + 0f0h], xmm15

        ; Load the host XMM registers.
        movdqa  xmm6,  [rsp + 040h + 000h]
        movdqa  xmm7,  [rsp + 040h + 010h]
        movdqa  xmm8,  [rsp + 040h + 020h]
        movdqa  xmm9,  [rsp + 040h + 030h]
        movdqa  xmm10, [rsp + 040h + 040h]
        movdqa  xmm11, [rsp + 040h + 050h]
        movdqa  xmm12, [rsp + 040h + 060h]
        movdqa  xmm13, [rsp + 040h + 070h]
        movdqa  xmm14, [rsp + 040h + 080h]
        movdqa  xmm15, [rsp + 040h + 090h]
        leave
        ret
ENDPROC   hmR0VMXStartVMWrapXMM

;;
; Wrapper around svm.pfnVMRun that preserves host XMM registers and
; load the guest ones when necessary.
;
; @cproto       DECLASM(int) hmR0SVMRunWrapXMM(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu, PFNHMSVMVMRUN pfnVMRun);
;
; @returns      eax
;
; @param        pVMCBHostPhys   msc:rcx
; @param        pVMCBPhys       msc:rdx
; @param        pCtx            msc:r8
; @param        pVM             msc:r9
; @param        pVCpu           msc:[rbp+30h]
; @param        pfnVMRun        msc:[rbp+38h]
;
; @remarks      This is essentially the same code as hmR0VMXStartVMWrapXMM, only the parameters differ a little bit.
;
; ASSUMING 64-bit and windows for now.
ALIGNCODE(16)
BEGINPROC hmR0SVMRunWrapXMM
        push    xBP
        mov     xBP, xSP
        sub     xSP, 0a0h + 040h        ; Don't bother optimizing the frame size.

        ; spill input parameters.
        mov     [xBP + 010h], rcx       ; pVMCBHostPhys
        mov     [xBP + 018h], rdx       ; pVMCBPhys
        mov     [xBP + 020h], r8        ; pCtx
        mov     [xBP + 028h], r9        ; pVM

        ; Ask CPUM whether we've started using the FPU yet.
        mov     rcx, [xBP + 30h]        ; pVCpu
        call    NAME(CPUMIsGuestFPUStateActive)
        test    al, al
        jnz     .guest_fpu_state_active

        ; No need to mess with XMM registers just call the start routine and return.
        mov     r11, [xBP + 38h]        ; pfnVMRun
        mov     r10, [xBP + 30h]        ; pVCpu
        mov     [xSP + 020h], r10
        mov     rcx, [xBP + 010h]       ; pVMCBHostPhys
        mov     rdx, [xBP + 018h]       ; pVMCBPhys
        mov     r8,  [xBP + 020h]       ; pCtx
        mov     r9,  [xBP + 028h]       ; pVM
        call    r11

        leave
        ret

ALIGNCODE(8)
.guest_fpu_state_active:
        ; Save the host XMM registers.
        movdqa  [rsp + 040h + 000h], xmm6
        movdqa  [rsp + 040h + 010h], xmm7
        movdqa  [rsp + 040h + 020h], xmm8
        movdqa  [rsp + 040h + 030h], xmm9
        movdqa  [rsp + 040h + 040h], xmm10
        movdqa  [rsp + 040h + 050h], xmm11
        movdqa  [rsp + 040h + 060h], xmm12
        movdqa  [rsp + 040h + 070h], xmm13
        movdqa  [rsp + 040h + 080h], xmm14
        movdqa  [rsp + 040h + 090h], xmm15

        ; Load the full guest XMM register state.
        mov     r10, [xBP + 020h]       ; pCtx
        lea     r10, [r10 + XMM_OFF_IN_X86FXSTATE]
        movdqa  xmm0,  [r10 + 000h]
        movdqa  xmm1,  [r10 + 010h]
        movdqa  xmm2,  [r10 + 020h]
        movdqa  xmm3,  [r10 + 030h]
        movdqa  xmm4,  [r10 + 040h]
        movdqa  xmm5,  [r10 + 050h]
        movdqa  xmm6,  [r10 + 060h]
        movdqa  xmm7,  [r10 + 070h]
        movdqa  xmm8,  [r10 + 080h]
        movdqa  xmm9,  [r10 + 090h]
        movdqa  xmm10, [r10 + 0a0h]
        movdqa  xmm11, [r10 + 0b0h]
        movdqa  xmm12, [r10 + 0c0h]
        movdqa  xmm13, [r10 + 0d0h]
        movdqa  xmm14, [r10 + 0e0h]
        movdqa  xmm15, [r10 + 0f0h]

        ; Make the call (same as in the other case ).
        mov     r11, [xBP + 38h]        ; pfnVMRun
        mov     r10, [xBP + 30h]        ; pVCpu
        mov     [xSP + 020h], r10
        mov     rcx, [xBP + 010h]       ; pVMCBHostPhys
        mov     rdx, [xBP + 018h]       ; pVMCBPhys
        mov     r8,  [xBP + 020h]       ; pCtx
        mov     r9,  [xBP + 028h]       ; pVM
        call    r11

        ; Save the guest XMM registers.
        mov     r10, [xBP + 020h]       ; pCtx
        lea     r10, [r10 + XMM_OFF_IN_X86FXSTATE]
        movdqa  [r10 + 000h], xmm0
        movdqa  [r10 + 010h], xmm1
        movdqa  [r10 + 020h], xmm2
        movdqa  [r10 + 030h], xmm3
        movdqa  [r10 + 040h], xmm4
        movdqa  [r10 + 050h], xmm5
        movdqa  [r10 + 060h], xmm6
        movdqa  [r10 + 070h], xmm7
        movdqa  [r10 + 080h], xmm8
        movdqa  [r10 + 090h], xmm9
        movdqa  [r10 + 0a0h], xmm10
        movdqa  [r10 + 0b0h], xmm11
        movdqa  [r10 + 0c0h], xmm12
        movdqa  [r10 + 0d0h], xmm13
        movdqa  [r10 + 0e0h], xmm14
        movdqa  [r10 + 0f0h], xmm15

        ; Load the host XMM registers.
        movdqa  xmm6,  [rsp + 040h + 000h]
        movdqa  xmm7,  [rsp + 040h + 010h]
        movdqa  xmm8,  [rsp + 040h + 020h]
        movdqa  xmm9,  [rsp + 040h + 030h]
        movdqa  xmm10, [rsp + 040h + 040h]
        movdqa  xmm11, [rsp + 040h + 050h]
        movdqa  xmm12, [rsp + 040h + 060h]
        movdqa  xmm13, [rsp + 040h + 070h]
        movdqa  xmm14, [rsp + 040h + 080h]
        movdqa  xmm15, [rsp + 040h + 090h]
        leave
        ret
ENDPROC   hmR0SVMRunWrapXMM

%endif ; VBOX_WITH_KERNEL_USING_XMM

;
; The default setup of the StartVM routines.
;
%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
 %define MY_NAME(name)   name %+ _32
%else
 %define MY_NAME(name)   name
%endif
%ifdef RT_ARCH_AMD64
 %define MYPUSHAD       MYPUSHAD64
 %define MYPOPAD        MYPOPAD64
 %define MYPUSHSEGS     MYPUSHSEGS64
 %define MYPOPSEGS      MYPOPSEGS64
%else
 %define MYPUSHAD       MYPUSHAD32
 %define MYPOPAD        MYPOPAD32
 %define MYPUSHSEGS     MYPUSHSEGS32
 %define MYPOPSEGS      MYPOPSEGS32
%endif

%include "HMR0Mixed.mac"


%ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
 ;
 ; Write the wrapper procedures.
 ;
 ; These routines are probably being too paranoid about selector
 ; restoring, but better safe than sorry...
 ;

; DECLASM(int) VMXR0StartVM32(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache /*, PVM pVM, PVMCPU pVCpu*/);
ALIGNCODE(16)
BEGINPROC VMXR0StartVM32
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    je near NAME(VMXR0StartVM32_32)

    ; stack frame
    push    esi
    push    edi
    push    fs
    push    gs

    ; jmp far .thunk64
    db      0xea
    dd      .thunk64, NAME(SUPR0Abs64bitKernelCS)

ALIGNCODE(16)
BITS 64
.thunk64:
    sub     esp, 20h
    mov     edi, [rsp + 20h + 14h]      ; fResume
    mov     esi, [rsp + 20h + 18h]      ; pCtx
    mov     edx, [rsp + 20h + 1Ch]      ; pCache
    call    NAME(VMXR0StartVM32_64)
    add     esp, 20h
    jmp far [.fpthunk32 wrt rip]
.fpthunk32:                             ; 16:32 Pointer to .thunk32.
    dd      .thunk32, NAME(SUPR0AbsKernelCS)

BITS 32
ALIGNCODE(16)
.thunk32:
    pop     gs
    pop     fs
    pop     edi
    pop     esi
    ret
ENDPROC   VMXR0StartVM32


; DECLASM(int) VMXR0StartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache /*, PVM pVM, PVMCPU pVCpu*/);
ALIGNCODE(16)
BEGINPROC VMXR0StartVM64
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    je      .not_in_long_mode

    ; stack frame
    push    esi
    push    edi
    push    fs
    push    gs

    ; jmp far .thunk64
    db      0xea
    dd      .thunk64, NAME(SUPR0Abs64bitKernelCS)

ALIGNCODE(16)
BITS 64
.thunk64:
    sub     esp, 20h
    mov     edi, [rsp + 20h + 14h]      ; fResume
    mov     esi, [rsp + 20h + 18h]      ; pCtx
    mov     edx, [rsp + 20h + 1Ch]      ; pCache
    call    NAME(VMXR0StartVM64_64)
    add     esp, 20h
    jmp far [.fpthunk32 wrt rip]
.fpthunk32:                             ; 16:32 Pointer to .thunk32.
    dd      .thunk32, NAME(SUPR0AbsKernelCS)

BITS 32
ALIGNCODE(16)
.thunk32:
    pop     gs
    pop     fs
    pop     edi
    pop     esi
    ret

.not_in_long_mode:
    mov     eax, VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE
    ret
ENDPROC   VMXR0StartVM64

;DECLASM(int) SVMR0VMRun(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx /*, PVM pVM, PVMCPU pVCpu*/);
ALIGNCODE(16)
BEGINPROC SVMR0VMRun
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    je near NAME(SVMR0VMRun_32)

    ; stack frame
    push    esi
    push    edi
    push    fs
    push    gs

    ; jmp far .thunk64
    db      0xea
    dd      .thunk64, NAME(SUPR0Abs64bitKernelCS)

ALIGNCODE(16)
BITS 64
.thunk64:
    sub     esp, 20h
    mov     rdi, [rsp + 20h + 14h]      ; pVMCBHostPhys
    mov     rsi, [rsp + 20h + 1Ch]      ; pVMCBPhys
    mov     edx, [rsp + 20h + 24h]      ; pCtx
    call    NAME(SVMR0VMRun_64)
    add     esp, 20h
    jmp far [.fpthunk32 wrt rip]
.fpthunk32:                             ; 16:32 Pointer to .thunk32.
    dd      .thunk32, NAME(SUPR0AbsKernelCS)

BITS 32
ALIGNCODE(16)
.thunk32:
    pop     gs
    pop     fs
    pop     edi
    pop     esi
    ret
ENDPROC   SVMR0VMRun


; DECLASM(int) SVMR0VMRun64(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx /*, PVM pVM, PVMCPU pVCpu*/);
ALIGNCODE(16)
BEGINPROC SVMR0VMRun64
    cmp     byte [NAME(g_fVMXIs64bitHost)], 0
    je      .not_in_long_mode

    ; stack frame
    push    esi
    push    edi
    push    fs
    push    gs

    ; jmp far .thunk64
    db      0xea
    dd      .thunk64, NAME(SUPR0Abs64bitKernelCS)

ALIGNCODE(16)
BITS 64
.thunk64:
    sub     esp, 20h
    mov     rdi, [rbp + 20h + 14h]      ; pVMCBHostPhys
    mov     rsi, [rbp + 20h + 1Ch]      ; pVMCBPhys
    mov     edx, [rbp + 20h + 24h]      ; pCtx
    call    NAME(SVMR0VMRun64_64)
    add     esp, 20h
    jmp far [.fpthunk32 wrt rip]
.fpthunk32:                             ; 16:32 Pointer to .thunk32.
    dd      .thunk32, NAME(SUPR0AbsKernelCS)

BITS 32
ALIGNCODE(16)
.thunk32:
    pop     gs
    pop     fs
    pop     edi
    pop     esi
    ret

.not_in_long_mode:
    mov     eax, VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE
    ret
ENDPROC   SVMR0VMRun64

 ;
 ; Do it a second time pretending we're a 64-bit host.
 ;
 ; This *HAS* to be done at the very end of the file to avoid restoring
 ; macros. So, add new code *BEFORE* this mess.
 ;
 BITS 64
 %undef  RT_ARCH_X86
 %define RT_ARCH_AMD64
 %undef  ASM_CALL64_MSC
 %define ASM_CALL64_GCC
 %define xS             8
 %define xSP            rsp
 %define xBP            rbp
 %define xAX            rax
 %define xBX            rbx
 %define xCX            rcx
 %define xDX            rdx
 %define xDI            rdi
 %define xSI            rsi
 %define MY_NAME(name)   name %+ _64
 %define MYPUSHAD       MYPUSHAD64
 %define MYPOPAD        MYPOPAD64
 %define MYPUSHSEGS     MYPUSHSEGS64
 %define MYPOPSEGS      MYPOPSEGS64

 %include "HMR0Mixed.mac"
%endif ; VBOX_WITH_HYBRID_32BIT_KERNEL
