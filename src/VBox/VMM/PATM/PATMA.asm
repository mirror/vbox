; $Id$
;; @file
; PATM Assembly Routines.
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

;;
; @note This method has problems in theory. If we fault for any reason, then we won't be able to restore
;       the guest's context properly!!
;       E.g if one of the push instructions causes a fault or SS isn't wide open and our patch GC state accesses aren't valid.
; @assumptions
;       - Enough stack for a few pushes
;       - The SS selector has base 0 and limit 0xffffffff
;
; @todo stack probing is currently hardcoded and not present everywhere (search for 'probe stack')


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/x86.mac"
%include "VBox/vm.mac"
%include "PATMA.mac"

%ifdef DEBUG
; Noisy, but useful for debugging certain problems
;;;%define PATM_LOG_PATCHINSTR
;;%define PATM_LOG_PATCHIRET
%endif

BEGINCONST

%ifdef RT_ARCH_AMD64
 BITS 32 ; switch to 32-bit mode (x86).
%endif

%ifdef VBOX_WITH_STATISTICS
;
; Patch call statistics
;
BEGINPROC   PATMStats
PATMStats_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf
    inc     dword [ss:PATM_ALLPATCHCALLS]
    inc     dword [ss:PATM_PERPATCHCALLS]
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMStats_End:
ENDPROC     PATMStats


; Patch record for statistics
GLOBALNAME PATMStatsRecord
    RTCCPTR_DEF PATMStats_Start
    DD      0
    DD      0
    DD      0
    DD      PATMStats_End - PATMStats_Start
    DD      4
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_ALLPATCHCALLS
    DD      0
    DD	    PATM_PERPATCHCALLS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh
%endif

;
; Set PATM_INTERRUPTFLAG
;
BEGINPROC   PATMSetPIF
PATMSetPIF_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMSetPIF_End:
ENDPROC     PATMSetPIF


; Patch record for setting PATM_INTERRUPTFLAG
GLOBALNAME PATMSetPIFRecord
    RTCCPTR_DEF PATMSetPIF_Start
    DD      0
    DD      0
    DD      0
    DD      PATMSetPIF_End - PATMSetPIF_Start
    DD      1
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Clear PATM_INTERRUPTFLAG
;
BEGINPROC   PATMClearPIF
PATMClearPIF_Start:
    ; probe stack here as we can't recover from page faults later on
    not     dword [esp-64]
    not     dword [esp-64]
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
PATMClearPIF_End:
ENDPROC     PATMClearPIF


; Patch record for clearing PATM_INTERRUPTFLAG
GLOBALNAME PATMClearPIFRecord
    RTCCPTR_DEF PATMClearPIF_Start
    DD      0
    DD      0
    DD      0
    DD      PATMClearPIF_End - PATMClearPIF_Start
	DD		1
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Clear PATM_INHIBITIRQADDR and fault if IF=0
;
BEGINPROC   PATMClearInhibitIRQFaultIF0
PATMClearInhibitIRQFaultIF0_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    mov     dword [ss:PATM_INHIBITIRQADDR], 0
    pushf

	test	dword [ss:PATM_VMFLAGS], X86_EFL_IF
	jz		PATMClearInhibitIRQFaultIF0_Fault

    ; if interrupts are pending, then we must go back to the host context to handle them!
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMClearInhibitIRQFaultIF0_Continue

    ; Go to our hypervisor trap handler to dispatch the pending irq
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_EDI], edi
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX | PATM_RESTORE_EDI
    mov     eax, PATM_ACTION_DISPATCH_PENDING_IRQ
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, PATM_NEXTINSTRADDR
    popfd                   ; restore flags we pushed above (the or instruction changes the flags as well)
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return

PATMClearInhibitIRQFaultIF0_Fault:
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMClearInhibitIRQFaultIF0_Continue:
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMClearInhibitIRQFaultIF0_End:
ENDPROC     PATMClearInhibitIRQFaultIF0


; Patch record for clearing PATM_INHIBITIRQADDR
GLOBALNAME PATMClearInhibitIRQFaultIF0Record
    RTCCPTR_DEF PATMClearInhibitIRQFaultIF0_Start
    DD      0
    DD      0
    DD      0
    DD      PATMClearInhibitIRQFaultIF0_End - PATMClearInhibitIRQFaultIF0_Start
	DD		12
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INHIBITIRQADDR
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_EDI
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_NEXTINSTRADDR
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Clear PATM_INHIBITIRQADDR and continue if IF=0 (duplicated function only; never jump back to guest code afterwards!!)
;
BEGINPROC   PATMClearInhibitIRQContIF0
PATMClearInhibitIRQContIF0_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    mov     dword [ss:PATM_INHIBITIRQADDR], 0
    pushf

	test	dword [ss:PATM_VMFLAGS], X86_EFL_IF
	jz		PATMClearInhibitIRQContIF0_Continue

    ; if interrupts are pending, then we must go back to the host context to handle them!
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMClearInhibitIRQContIF0_Continue

    ; Go to our hypervisor trap handler to dispatch the pending irq
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_EDI], edi
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX | PATM_RESTORE_EDI
    mov     eax, PATM_ACTION_DISPATCH_PENDING_IRQ
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, PATM_NEXTINSTRADDR
    popfd                   ; restore flags we pushed above (the or instruction changes the flags as well)
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return

PATMClearInhibitIRQContIF0_Continue:
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMClearInhibitIRQContIF0_End:
ENDPROC     PATMClearInhibitIRQContIF0


; Patch record for clearing PATM_INHIBITIRQADDR
GLOBALNAME PATMClearInhibitIRQContIF0Record
    RTCCPTR_DEF PATMClearInhibitIRQContIF0_Start
    DD      0
    DD      0
    DD      0
    DD      PATMClearInhibitIRQContIF0_End - PATMClearInhibitIRQContIF0_Start
	DD		11
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INHIBITIRQADDR
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_EDI
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_NEXTINSTRADDR
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMCliReplacement
PATMCliStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf
%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    mov     eax, PATM_ACTION_LOG_CLI
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif

    and     dword [ss:PATM_VMFLAGS], ~X86_EFL_IF
    popf

    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMCliJump:
    DD      PATM_JUMPDELTA
PATMCliEnd:
ENDPROC     PATMCliReplacement


; Patch record for 'cli'
GLOBALNAME PATMCliRecord
    RTCCPTR_DEF PATMCliStart
    DD      PATMCliJump - PATMCliStart
    DD      0
    DD      0
    DD      PATMCliEnd - PATMCliStart
%ifdef PATM_LOG_PATCHINSTR
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMStiReplacement
PATMStiStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    mov     dword [ss:PATM_INHIBITIRQADDR], PATM_NEXTINSTRADDR
    pushf
%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    mov     eax, PATM_ACTION_LOG_STI
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif
    or      dword [ss:PATM_VMFLAGS], X86_EFL_IF
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMStiEnd:
ENDPROC     PATMStiReplacement

; Patch record for 'sti'
GLOBALNAME PATMStiRecord
    RTCCPTR_DEF PATMStiStart
    DD      0
    DD      0
    DD      0
    DD      PATMStiEnd - PATMStiStart
%ifdef PATM_LOG_PATCHINSTR
    DD      6
%else
    DD      5
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INHIBITIRQADDR
    DD      0
    DD      PATM_NEXTINSTRADDR
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Trampoline code for trap entry (without error code on the stack)
;
; esp + 32 - GS         (V86 only)
; esp + 28 - FS         (V86 only)
; esp + 24 - DS         (V86 only)
; esp + 20 - ES         (V86 only)
; esp + 16 - SS         (if transfer to inner ring)
; esp + 12 - ESP        (if transfer to inner ring)
; esp + 8  - EFLAGS
; esp + 4  - CS
; esp      - EIP
;
BEGINPROC   PATMTrapEntry
PATMTrapEntryStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf

%ifdef PATM_LOG_PATCHIRET
    push    eax
    push    ecx
    push    edx
    lea     edx, dword [ss:esp+12+4]        ;3 dwords + pushed flags -> iret eip
    mov     eax, PATM_ACTION_LOG_GATE_ENTRY
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    test    dword [esp+12], X86_EFL_VM
    jnz     PATMTrapNoRing1

    ; make sure the saved CS selector for ring 1 is made 0
    test    dword [esp+8], 2
    jnz     PATMTrapNoRing1
    test    dword [esp+8], 1
    jz      PATMTrapNoRing1
    and     dword [esp+8], dword ~1     ; yasm / nasm dword
PATMTrapNoRing1:

	; correct EFLAGS on the stack to include the current IOPL
	push	eax
	mov     eax, dword [ss:PATM_VMFLAGS]
	and     eax, X86_EFL_IOPL
	and     dword [esp+16], ~X86_EFL_IOPL		; esp+16 = eflags = esp+8+4(efl)+4(eax)
	or      dword [esp+16], eax
    pop		eax

    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMTrapEntryJump:
    DD      PATM_JUMPDELTA
PATMTrapEntryEnd:
ENDPROC     PATMTrapEntry


; Patch record for trap gate entrypoint
GLOBALNAME PATMTrapEntryRecord
    RTCCPTR_DEF PATMTrapEntryStart
    DD      PATMTrapEntryJump - PATMTrapEntryStart
    DD      0
    DD      0
    DD      PATMTrapEntryEnd - PATMTrapEntryStart
%ifdef PATM_LOG_PATCHIRET
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHIRET
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Trampoline code for trap entry (with error code on the stack)
;
; esp + 36 - GS         (V86 only)
; esp + 32 - FS         (V86 only)
; esp + 28 - DS         (V86 only)
; esp + 24 - ES         (V86 only)
; esp + 20 - SS         (if transfer to inner ring)
; esp + 16 - ESP        (if transfer to inner ring)
; esp + 12 - EFLAGS
; esp + 8  - CS
; esp + 4  - EIP
; esp      - error code
;
BEGINPROC   PATMTrapEntryErrorCode
PATMTrapErrorCodeEntryStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf

%ifdef PATM_LOG_PATCHIRET
    push    eax
    push    ecx
    push    edx
    lea     edx, dword [ss:esp+12+4+4]        ;3 dwords + pushed flags + error code -> iret eip
    mov     eax, PATM_ACTION_LOG_GATE_ENTRY
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    test    dword [esp+16], X86_EFL_VM
    jnz     PATMTrapErrorCodeNoRing1

    ; make sure the saved CS selector for ring 1 is made 0
    test    dword [esp+12], 2
    jnz     PATMTrapErrorCodeNoRing1
    test    dword [esp+12], 1
    jz      PATMTrapErrorCodeNoRing1
    and     dword [esp+12], dword ~1     ; yasm / nasm dword
PATMTrapErrorCodeNoRing1:

	; correct EFLAGS on the stack to include the current IOPL
	push	eax
	mov     eax, dword [ss:PATM_VMFLAGS]
	and     eax, X86_EFL_IOPL
	and     dword [esp+20], ~X86_EFL_IOPL		; esp+20 = eflags = esp+8+4(efl)+4(error code)+4(eax)
	or      dword [esp+20], eax
    pop		eax

    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMTrapErrorCodeEntryJump:
    DD      PATM_JUMPDELTA
PATMTrapErrorCodeEntryEnd:
ENDPROC     PATMTrapEntryErrorCode


; Patch record for trap gate entrypoint
GLOBALNAME PATMTrapEntryRecordErrorCode
    RTCCPTR_DEF PATMTrapErrorCodeEntryStart
    DD      PATMTrapErrorCodeEntryJump - PATMTrapErrorCodeEntryStart
    DD      0
    DD      0
    DD      PATMTrapErrorCodeEntryEnd - PATMTrapErrorCodeEntryStart
%ifdef PATM_LOG_PATCHIRET
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHIRET
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


;
; Trampoline code for interrupt gate entry (without error code on the stack)
;
; esp + 32 - GS         (V86 only)
; esp + 28 - FS         (V86 only)
; esp + 24 - DS         (V86 only)
; esp + 20 - ES         (V86 only)
; esp + 16 - SS         (if transfer to inner ring)
; esp + 12 - ESP        (if transfer to inner ring)
; esp + 8  - EFLAGS
; esp + 4  - CS
; esp      - EIP
;
BEGINPROC   PATMIntEntry
PATMIntEntryStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf

%ifdef PATM_LOG_PATCHIRET
    push    eax
    push    ecx
    push    edx
    lea     edx, dword [ss:esp+12+4]        ;3 dwords + pushed flags -> iret eip
    mov     eax, PATM_ACTION_LOG_GATE_ENTRY
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    test    dword [esp+12], X86_EFL_VM
    jnz     PATMIntNoRing1

    ; make sure the saved CS selector for ring 1 is made 0
    test    dword [esp+8], 2
    jnz     PATMIntNoRing1
    test    dword [esp+8], 1
    jz      PATMIntNoRing1
    and     dword [esp+8], dword ~1     ; yasm / nasm dword
PATMIntNoRing1:

	; correct EFLAGS on the stack to include the current IOPL
	push	eax
	mov     eax, dword [ss:PATM_VMFLAGS]
	and     eax, X86_EFL_IOPL
	and     dword [esp+16], ~X86_EFL_IOPL		; esp+16 = eflags = esp+8+4(efl)+4(eax)
	or      dword [esp+16], eax
    pop		eax

    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMIntEntryEnd:
ENDPROC     PATMIntEntry


; Patch record for interrupt gate entrypoint
GLOBALNAME PATMIntEntryRecord
    RTCCPTR_DEF PATMIntEntryStart
    DD      0
    DD      0
    DD      0
    DD      PATMIntEntryEnd - PATMIntEntryStart
%ifdef PATM_LOG_PATCHIRET
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHIRET
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Trampoline code for interrupt gate entry (*with* error code on the stack)
;
; esp + 36 - GS         (V86 only)
; esp + 32 - FS         (V86 only)
; esp + 28 - DS         (V86 only)
; esp + 24 - ES         (V86 only)
; esp + 20 - SS         (if transfer to inner ring)
; esp + 16 - ESP        (if transfer to inner ring)
; esp + 12 - EFLAGS
; esp + 8  - CS
; esp + 4  - EIP
; esp      - error code
;
BEGINPROC   PATMIntEntryErrorCode
PATMIntEntryErrorCodeStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf

%ifdef PATM_LOG_PATCHIRET
    push    eax
    push    ecx
    push    edx
    lea     edx, dword [ss:esp+12+4+4]        ;3 dwords + pushed flags + error code -> iret eip
    mov     eax, PATM_ACTION_LOG_GATE_ENTRY
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    test    dword [esp+16], X86_EFL_VM
    jnz     PATMIntNoRing1_ErrorCode

    ; make sure the saved CS selector for ring 1 is made 0
    test    dword [esp+12], 2
    jnz     PATMIntNoRing1_ErrorCode
    test    dword [esp+12], 1
    jz      PATMIntNoRing1_ErrorCode
    and     dword [esp+12], dword ~1     ; yasm / nasm dword
PATMIntNoRing1_ErrorCode:

	; correct EFLAGS on the stack to include the current IOPL
	push	eax
	mov     eax, dword [ss:PATM_VMFLAGS]
	and     eax, X86_EFL_IOPL
	and     dword [esp+20], ~X86_EFL_IOPL		; esp+20 = eflags = esp+8+4(efl)+4(eax)+4(error code)
	or      dword [esp+20], eax
    pop		eax

    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMIntEntryErrorCodeEnd:
ENDPROC     PATMIntEntryErrorCode


; Patch record for interrupt gate entrypoint
GLOBALNAME PATMIntEntryRecordErrorCode
    RTCCPTR_DEF PATMIntEntryErrorCodeStart
    DD      0
    DD      0
    DD      0
    DD      PATMIntEntryErrorCodeEnd - PATMIntEntryErrorCodeStart
%ifdef PATM_LOG_PATCHIRET
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHIRET
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; 32 bits Popf replacement that faults when IF remains 0
;
BEGINPROC   PATMPopf32Replacement
PATMPopf32Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    mov     eax, PATM_ACTION_LOG_POPF_IF1
    test    dword [esp+8], X86_EFL_IF
    jnz     PATMPopf32_Log
    mov     eax, PATM_ACTION_LOG_POPF_IF0

PATMPopf32_Log:
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif

    test    dword [esp], X86_EFL_IF
    jnz     PATMPopf32_Ok
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMPopf32_Ok:
    ; Note: we don't allow popf instructions to change the current IOPL; we simply ignore such changes (!!!)
    ; In this particular patch it's rather unlikely the pushf was included, so we have no way to check if the flags on the stack were correctly synched
    ; PATMPopf32Replacement_NoExit is different, because it's only used in IDT and function patches
    or      dword [ss:PATM_VMFLAGS], X86_EFL_IF

    ; if interrupts are pending, then we must go back to the host context to handle them!
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMPopf32_Continue

    ; Go to our hypervisor trap handler to dispatch the pending irq
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_EDI], edi
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX | PATM_RESTORE_EDI
    mov     eax, PATM_ACTION_DISPATCH_PENDING_IRQ
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, PATM_NEXTINSTRADDR

    popfd                   ; restore flags we pushed above (the or instruction changes the flags as well)
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return

PATMPopf32_Continue:
    popfd                   ; restore flags we pushed above
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMPopf32Jump:
    DD      PATM_JUMPDELTA
PATMPopf32End:
ENDPROC     PATMPopf32Replacement


; Patch record for 'popfd'
GLOBALNAME PATMPopf32Record
    RTCCPTR_DEF PATMPopf32Start
    DD      PATMPopf32Jump - PATMPopf32Start
    DD      0
    DD      0
    DD      PATMPopf32End - PATMPopf32Start
%ifdef PATM_LOG_PATCHINSTR
    DD      12
%else
    DD      11
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_EDI
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_NEXTINSTRADDR
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

; no need to check the IF flag when popf isn't an exit point of a patch (e.g. function duplication)
BEGINPROC   PATMPopf32Replacement_NoExit
PATMPopf32_NoExitStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    mov     eax, PATM_ACTION_LOG_POPF_IF1
    test    dword [esp+8], X86_EFL_IF
    jnz     PATMPopf32_NoExitLog
    mov     eax, PATM_ACTION_LOG_POPF_IF0

PATMPopf32_NoExitLog:
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif
    test    dword [esp], X86_EFL_IF
    jz      PATMPopf32_NoExit_Continue

    ; if interrupts are pending, then we must go back to the host context to handle them!
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMPopf32_NoExit_Continue

    ; Go to our hypervisor trap handler to dispatch the pending irq
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_EDI], edi
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX | PATM_RESTORE_EDI
    mov     eax, PATM_ACTION_DISPATCH_PENDING_IRQ
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, PATM_NEXTINSTRADDR

    pop     dword [ss:PATM_VMFLAGS]     ; restore flags now (the or instruction changes the flags as well)
    push    dword [ss:PATM_VMFLAGS]
    popfd

    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return

PATMPopf32_NoExit_Continue:
    pop     dword [ss:PATM_VMFLAGS]
    push    dword [ss:PATM_VMFLAGS]
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMPopf32_NoExitEnd:
ENDPROC     PATMPopf32Replacement_NoExit


; Patch record for 'popfd'
GLOBALNAME PATMPopf32Record_NoExit
    RTCCPTR_DEF PATMPopf32_NoExitStart
    DD      0
    DD      0
    DD      0
    DD      PATMPopf32_NoExitEnd - PATMPopf32_NoExitStart
%ifdef PATM_LOG_PATCHINSTR
    DD      14
%else
    DD      13
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_EDI
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_NEXTINSTRADDR
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


;
; 16 bits Popf replacement that faults when IF remains 0
;
BEGINPROC   PATMPopf16Replacement
PATMPopf16Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    test    word [esp], X86_EFL_IF
    jnz     PATMPopf16_Ok
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMPopf16_Ok:
    ; if interrupts are pending, then we must go back to the host context to handle them!
    ; @note we destroy the flags here, but that should really not matter (PATM_INT3 case)
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMPopf16_Continue
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMPopf16_Continue:

    pop     word [ss:PATM_VMFLAGS]
    push    word [ss:PATM_VMFLAGS]
    and     dword [ss:PATM_VMFLAGS], PATM_VIRTUAL_FLAGS_MASK
    or      dword [ss:PATM_VMFLAGS], PATM_VIRTUAL_FLAGS_MASK

    DB      0x66    ; size override
    popf    ;after the and and or operations!! (flags must be preserved)
    mov     dword [ss:PATM_INTERRUPTFLAG], 1

    DB      0xE9
PATMPopf16Jump:
    DD      PATM_JUMPDELTA
PATMPopf16End:
ENDPROC     PATMPopf16Replacement


; Patch record for 'popf'
GLOBALNAME PATMPopf16Record
    RTCCPTR_DEF PATMPopf16Start
    DD      PATMPopf16Jump - PATMPopf16Start
    DD      0
    DD      0
    DD      PATMPopf16End - PATMPopf16Start
    DD      9
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; 16 bits Popf replacement that faults when IF remains 0
; @todo not necessary to fault in that case (see 32 bits version)
BEGINPROC   PATMPopf16Replacement_NoExit
PATMPopf16Start_NoExit:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    test    word [esp], X86_EFL_IF
    jnz     PATMPopf16_Ok_NoExit
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMPopf16_Ok_NoExit:
    ; if interrupts are pending, then we must go back to the host context to handle them!
    ; @note we destroy the flags here, but that should really not matter (PATM_INT3 case)
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST
    jz      PATMPopf16_Continue_NoExit
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

PATMPopf16_Continue_NoExit:

    pop     word [ss:PATM_VMFLAGS]
    push    word [ss:PATM_VMFLAGS]
    and     dword [ss:PATM_VMFLAGS], PATM_VIRTUAL_FLAGS_MASK
    or      dword [ss:PATM_VMFLAGS], PATM_VIRTUAL_FLAGS_MASK

    DB      0x66    ; size override
    popf    ;after the and and or operations!! (flags must be preserved)
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMPopf16End_NoExit:
ENDPROC     PATMPopf16Replacement_NoExit


; Patch record for 'popf'
GLOBALNAME PATMPopf16Record_NoExit
    RTCCPTR_DEF PATMPopf16Start_NoExit
    DD      0
    DD      0
    DD      0
    DD      PATMPopf16End_NoExit - PATMPopf16Start_NoExit
    DD      9
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMPushf32Replacement
PATMPushf32Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushfd
%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    mov     eax, PATM_ACTION_LOG_PUSHF
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif

    pushfd
    push    eax
    mov     eax, dword [esp+8]
    and     eax, PATM_FLAGS_MASK
    or      eax, dword [ss:PATM_VMFLAGS]
    mov     dword [esp+8], eax
    pop     eax
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMPushf32End:
ENDPROC     PATMPushf32Replacement


; Patch record for 'pushfd'
GLOBALNAME PATMPushf32Record
    RTCCPTR_DEF PATMPushf32Start
    DD      0
    DD      0
    DD      0
    DD      PATMPushf32End - PATMPushf32Start
%ifdef PATM_LOG_PATCHINSTR
    DD      4
%else
    DD      3
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMPushf16Replacement
PATMPushf16Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    DB      0x66    ; size override
    pushf
    DB      0x66    ; size override
    pushf
    push    eax
    xor     eax, eax
    mov     ax, word [esp+6]
    and     eax, PATM_FLAGS_MASK
    or      eax, dword [ss:PATM_VMFLAGS]
    mov     word [esp+6], ax
    pop     eax

    DB      0x66    ; size override
    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMPushf16End:
ENDPROC     PATMPushf16Replacement


; Patch record for 'pushf'
GLOBALNAME PATMPushf16Record
    RTCCPTR_DEF PATMPushf16Start
    DD      0
    DD      0
    DD      0
    DD      PATMPushf16End - PATMPushf16Start
    DD      3
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMPushCSReplacement
PATMPushCSStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    push    cs
    pushfd

    test    dword [esp+4], 2
    jnz     pushcs_notring1

    ; change dpl from 1 to 0
    and     dword [esp+4], dword ~1     ; yasm / nasm dword

pushcs_notring1:
    popfd

    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMPushCSJump:
    DD      PATM_JUMPDELTA
PATMPushCSEnd:
ENDPROC     PATMPushCSReplacement


; Patch record for 'push cs'
GLOBALNAME PATMPushCSRecord
    RTCCPTR_DEF PATMPushCSStart
    DD      PATMPushCSJump - PATMPushCSStart
    DD      0
    DD      0
    DD      PATMPushCSEnd - PATMPushCSStart
    DD      2
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;;****************************************************
;; Abstract:
;;
;; if eflags.NT==0 && iretstack.eflags.VM==0 && iretstack.eflags.IOPL==0
;; then
;;     if return to ring 0 (iretstack.new_cs & 3 == 0)
;;     then
;;          if iretstack.new_eflags.IF == 1 && iretstack.new_eflags.IOPL == 0
;;          then
;;              iretstack.new_cs |= 1
;;          else
;;              int 3
;;     endif
;;     uVMFlags &= ~X86_EFL_IF
;;     iret
;; else
;;     int 3
;;****************************************************
;;
; Stack:
;
; esp + 32 - GS         (V86 only)
; esp + 28 - FS         (V86 only)
; esp + 24 - DS         (V86 only)
; esp + 20 - ES         (V86 only)
; esp + 16 - SS         (if transfer to outer ring)
; esp + 12 - ESP        (if transfer to outer ring)
; esp + 8  - EFLAGS
; esp + 4  - CS
; esp      - EIP
;;
BEGINPROC   PATMIretReplacement
PATMIretStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushfd

%ifdef PATM_LOG_PATCHIRET
    push    eax
    push    ecx
    push    edx
    lea     edx, dword [ss:esp+12+4]        ;3 dwords + pushed flags -> iret eip
    mov     eax, PATM_ACTION_LOG_IRET
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    test    dword [esp], X86_EFL_NT
    jnz near iret_fault1

    ; we can't do an iret to v86 code, as we run with CPL=1. The iret would attempt a protected mode iret and (most likely) fault.
    test    dword [esp+12], X86_EFL_VM
    jnz     near iret_return_to_v86

    ;;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ;;@todo: not correct for iret back to ring 2!!!!!
    ;;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    test    dword [esp+8], 2
    jnz     iret_notring0

    test    dword [esp+12], X86_EFL_IF
    jz near iret_clearIF

    ; force ring 1 CS RPL
    or      dword [esp+8], 1
iret_notring0:

; if interrupts are pending, then we must go back to the host context to handle them!
; Note: This is very important as pending pic interrupts can be overriden by apic interrupts if we don't check early enough (Fedora 5 boot)
; @@todo fix this properly, so we can dispatch pending interrupts in GC
    test    dword [ss:PATM_VM_FORCEDACTIONS], VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC
    jz      iret_continue

; Go to our hypervisor trap handler to dispatch the pending irq
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_EDI], edi
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX | PATM_RESTORE_EDI
    mov     eax, PATM_ACTION_PENDING_IRQ_AFTER_IRET
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, PATM_CURINSTRADDR

    popfd
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return

iret_continue :
	; This section must *always* be executed (!!)
	; Extract the IOPL from the return flags, save them to our virtual flags and
	; put them back to zero
    ; @note we assume iretd doesn't fault!!!
	push	eax
	mov     eax, dword [esp+16]
	and     eax, X86_EFL_IOPL
	and     dword [ss:PATM_VMFLAGS], ~X86_EFL_IOPL
	or      dword [ss:PATM_VMFLAGS], eax
    pop		eax
	and		dword [esp+12], ~X86_EFL_IOPL

    ; Set IF again; below we make sure this won't cause problems.
    or      dword [ss:PATM_VMFLAGS], X86_EFL_IF

    ; make sure iret is executed fully (including the iret below; cli ... iret can otherwise be interrupted)
    mov     dword [ss:PATM_INHIBITIRQADDR], PATM_CURINSTRADDR

    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    iretd
    PATM_INT3

iret_fault:
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

iret_fault1:
    nop
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

iret_clearIF:
    push    dword [esp+4]           ; eip to return to
    pushfd
    push    eax
    push    PATM_FIXUP
    DB      0E8h                    ; call
    DD      PATM_IRET_FUNCTION
    add     esp, 4                  ; pushed address of jump table

    cmp     eax, 0
    je      near iret_fault3

    mov     dword [esp+12+4], eax   ; stored eip in iret frame
    pop     eax
    popfd
    add     esp, 4                  ; pushed eip

    ; always ring 0 return -> change to ring 1 (CS in iret frame)
    or      dword [esp+8], 1

	; This section must *always* be executed (!!)
	; Extract the IOPL from the return flags, save them to our virtual flags and
	; put them back to zero
	push	eax
	mov     eax, dword [esp+16]
	and     eax, X86_EFL_IOPL
	and     dword [ss:PATM_VMFLAGS], ~X86_EFL_IOPL
	or      dword [ss:PATM_VMFLAGS], eax
    pop		eax
	and		dword [esp+12], ~X86_EFL_IOPL

    ; Clear IF
    and     dword [ss:PATM_VMFLAGS], ~X86_EFL_IF
    popfd

                                                ; the patched destination code will set PATM_INTERRUPTFLAG after the return!
    iretd

iret_return_to_v86:
    test    dword [esp+12], X86_EFL_IF
    jz      iret_fault

    ; Go to our hypervisor trap handler to perform the iret to v86 code
    mov     dword [ss:PATM_TEMP_EAX], eax
    mov     dword [ss:PATM_TEMP_ECX], ecx
    mov     dword [ss:PATM_TEMP_RESTORE_FLAGS], PATM_RESTORE_EAX | PATM_RESTORE_ECX
    mov     eax, PATM_ACTION_DO_V86_IRET
    lock    or dword [ss:PATM_PENDINGACTION], eax
    mov     ecx, PATM_ACTION_MAGIC

    popfd

    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    ; does not return


iret_fault3:
    pop     eax
    popfd
    add     esp, 4                  ; pushed eip
    jmp     iret_fault

align   4
PATMIretTable:
    DW      PATM_MAX_JUMPTABLE_ENTRIES          ; nrSlots
    DW      0                                   ; ulInsertPos
    DD      0                                   ; cAddresses
    TIMES PATCHJUMPTABLE_SIZE DB 0              ; lookup slots

PATMIretEnd:
ENDPROC     PATMIretReplacement

; Patch record for 'iretd'
GLOBALNAME PATMIretRecord
    RTCCPTR_DEF PATMIretStart
    DD      0
    DD      0
    DD      0
    DD      PATMIretEnd- PATMIretStart
%ifdef PATM_LOG_PATCHIRET
    DD      26
%else
    DD      25
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
%ifdef PATM_LOG_PATCHIRET
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_VM_FORCEDACTIONS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_EDI
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_CURINSTRADDR
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INHIBITIRQADDR
    DD      0
    DD      PATM_CURINSTRADDR
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_FIXUP
    DD      PATMIretTable - PATMIretStart
    DD      PATM_IRET_FUNCTION
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_ECX
    DD      0
    DD      PATM_TEMP_RESTORE_FLAGS
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      0ffffffffh


;
; global function for implementing 'iret' to code with IF cleared
;
; Caller is responsible for right stack layout
;  + 16 original return address
;  + 12 eflags
;  +  8 eax
;  +  4 Jump table address
;( +  0 return address )
;
; @note assumes PATM_INTERRUPTFLAG is zero
; @note assumes it can trash eax and eflags
;
; @returns eax=0 on failure
;          otherwise return address in eax
;
; @note NEVER change this without bumping the SSM version
align 32
BEGINPROC   PATMIretFunction
PATMIretFunction_Start:
    push    ecx
    push    edx
    push    edi

    ; Event order:
    ; 1) Check if the return patch address can be found in the lookup table
    ; 2) Query return patch address from the hypervisor

    ; 1) Check if the return patch address can be found in the lookup table
    mov     edx, dword [esp+12+16]  ; pushed target address

    xor     eax, eax                ; default result -> nothing found
    mov     edi, dword [esp+12+4]  ; jump table
    mov     ecx, [ss:edi + PATCHJUMPTABLE.cAddresses]
    cmp     ecx, 0
    je      near PATMIretFunction_AskHypervisor

PATMIretFunction_SearchStart:
    cmp     [ss:edi + PATCHJUMPTABLE.Slot_pInstrGC + eax*8], edx        ; edx = GC address to search for
    je      near PATMIretFunction_SearchHit
    inc     eax
    cmp     eax, ecx
    jl      near PATMIretFunction_SearchStart

PATMIretFunction_AskHypervisor:
    ; 2) Query return patch address from the hypervisor
    ; @todo private ugly interface, since we have nothing generic at the moment
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOOKUP_ADDRESS
    mov     eax, PATM_ACTION_LOOKUP_ADDRESS
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, dword [esp+12+4]               ; jump table address
    mov     edx, dword [esp+12+16]              ; original return address
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    jmp     near PATMIretFunction_SearchEnd

PATMIretFunction_SearchHit:
    mov     eax, [ss:edi + PATCHJUMPTABLE.Slot_pRelPatchGC + eax*8]        ; found a match!
    ;@note can be zero, so the next check is required!!

PATMIretFunction_SearchEnd:
    cmp     eax, 0
    jz      PATMIretFunction_Failure

    add     eax, PATM_PATCHBASE

    pop     edi
    pop     edx
    pop     ecx
    ret

PATMIretFunction_Failure:
    ;signal error
    xor     eax, eax
    pop     edi
    pop     edx
    pop     ecx
    ret

PATMIretFunction_End:
ENDPROC     PATMIretFunction

GLOBALNAME PATMIretFunctionRecord
    RTCCPTR_DEF PATMIretFunction_Start
    DD      0
    DD      0
    DD      0
    DD      PATMIretFunction_End - PATMIretFunction_Start
    DD      2
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_PATCHBASE
    DD      0
    DD      0ffffffffh


align 32 ; yasm / nasm diff - remove me!
BEGINPROC   PATMCpuidReplacement
PATMCpuidStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf

    cmp     eax, PATM_CPUID_STD_MAX
    jb      cpuid_std
    cmp     eax, 0x80000000
    jb      cpuid_def
    cmp     eax, PATM_CPUID_EXT_MAX
    jb      cpuid_ext
    cmp     eax, 0xc0000000
    jb      cpuid_def
    cmp     eax, PATM_CPUID_CENTAUR_MAX
    jb      cpuid_centaur

cpuid_def:
    mov     eax, PATM_CPUID_DEF_PTR
    jmp     cpuid_fetch

cpuid_std:
    mov     edx, PATM_CPUID_STD_PTR
    jmp     cpuid_calc

cpuid_ext:
    and     eax, 0ffh                   ; strictly speaking not necessary.
    mov     edx, PATM_CPUID_EXT_PTR
    jmp     cpuid_calc

cpuid_centaur:
    and     eax, 0ffh                   ; strictly speaking not necessary.
    mov     edx, PATM_CPUID_CENTAUR_PTR

cpuid_calc:
    lea     eax, [ss:eax * 4]              ; 4 entries...
    lea     eax, [ss:eax * 4]              ; 4 bytes each
    add     eax, edx

cpuid_fetch:
    mov     edx, [ss:eax + 12]             ; CPUMCPUID layout assumptions!
    mov     ecx, [ss:eax + 8]
    mov     ebx, [ss:eax + 4]
    mov     eax, [ss:eax]

    popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1

PATMCpuidEnd:
ENDPROC PATMCpuidReplacement

; Patch record for 'cpuid'
GLOBALNAME PATMCpuidRecord
    RTCCPTR_DEF PATMCpuidStart
    DD      0
    DD      0
    DD      0
    DD      PATMCpuidEnd- PATMCpuidStart
    DD      9
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_CPUID_STD_MAX
    DD      0
    DD      PATM_CPUID_EXT_MAX
    DD      0
    DD      PATM_CPUID_CENTAUR_MAX
    DD      0
    DD      PATM_CPUID_DEF_PTR
    DD      0
    DD      PATM_CPUID_STD_PTR
    DD      0
    DD      PATM_CPUID_EXT_PTR
    DD      0
    DD      PATM_CPUID_CENTAUR_PTR
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMJEcxReplacement
PATMJEcxStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushfd
PATMJEcxSizeOverride:
    DB      0x90             ; nop
    cmp     ecx, dword 0                ; yasm / nasm dword
    jnz     PATMJEcxContinue

    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMJEcxJump:
    DD      PATM_JUMPDELTA

PATMJEcxContinue:
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMJEcxEnd:
ENDPROC PATMJEcxReplacement

; Patch record for 'JEcx'
GLOBALNAME PATMJEcxRecord
    RTCCPTR_DEF PATMJEcxStart
    DD      0
    DD      PATMJEcxJump - PATMJEcxStart
    DD      PATMJEcxSizeOverride - PATMJEcxStart
    DD      PATMJEcxEnd- PATMJEcxStart
    DD      3
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

align 32; yasm / nasm diffing. remove me!
BEGINPROC   PATMLoopReplacement
PATMLoopStart:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushfd
PATMLoopSizeOverride:
    DB      0x90             ; nop
    dec     ecx
    jz      PATMLoopContinue

    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMLoopJump:
    DD      PATM_JUMPDELTA

PATMLoopContinue:
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMLoopEnd:
ENDPROC PATMLoopReplacement

; Patch record for 'Loop'
GLOBALNAME PATMLoopRecord
    RTCCPTR_DEF PATMLoopStart
    DD      0
    DD      PATMLoopJump - PATMLoopStart
    DD      PATMLoopSizeOverride - PATMLoopStart
    DD      PATMLoopEnd- PATMLoopStart
    DD      3
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

BEGINPROC   PATMLoopZReplacement
PATMLoopZStart:
    ; jump if ZF=1 AND (E)CX != 0

    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    jnz     PATMLoopZEnd
    pushfd
PATMLoopZSizeOverride:
    DB      0x90             ; nop
    dec     ecx
    jz      PATMLoopZContinue

    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMLoopZJump:
    DD      PATM_JUMPDELTA

PATMLoopZContinue:
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMLoopZEnd:
ENDPROC PATMLoopZReplacement

; Patch record for 'Loopz'
GLOBALNAME PATMLoopZRecord
    RTCCPTR_DEF PATMLoopZStart
    DD      0
    DD      PATMLoopZJump - PATMLoopZStart
    DD      PATMLoopZSizeOverride - PATMLoopZStart
    DD      PATMLoopZEnd- PATMLoopZStart
    DD      3
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


BEGINPROC   PATMLoopNZReplacement
PATMLoopNZStart:
    ; jump if ZF=0 AND (E)CX != 0

    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    jz      PATMLoopNZEnd
    pushfd
PATMLoopNZSizeOverride:
    DB      0x90             ; nop
    dec     ecx
    jz      PATMLoopNZContinue

    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMLoopNZJump:
    DD      PATM_JUMPDELTA

PATMLoopNZContinue:
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
PATMLoopNZEnd:
ENDPROC PATMLoopNZReplacement

; Patch record for 'LoopNZ'
GLOBALNAME PATMLoopNZRecord
    RTCCPTR_DEF PATMLoopNZStart
    DD      0
    DD      PATMLoopNZJump - PATMLoopNZStart
    DD      PATMLoopNZSizeOverride - PATMLoopNZStart
    DD      PATMLoopNZEnd- PATMLoopNZStart
    DD      3
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

align 32
; Global patch function for indirect calls
; Caller is responsible for clearing PATM_INTERRUPTFLAG and doing:
;  + 20 push    [pTargetGC]
;  + 16 pushfd
;  + 12 push    [JumpTableAddress]
;  +  8 push    [PATMRelReturnAddress]
;  +  4 push    [GuestReturnAddress]
;( +  0 return address )
;
; @note NEVER change this without bumping the SSM version
BEGINPROC PATMLookupAndCall
PATMLookupAndCallStart:
    push    eax
    push    edx
    push    edi
    push    ecx

    mov     eax, dword [esp+16+4]                   ; guest return address
    mov     dword [ss:PATM_CALL_RETURN_ADDR], eax                               ; temporary storage

    mov     edx, dword [esp+16+20]  ; pushed target address

    xor     eax, eax                ; default result -> nothing found
    mov     edi, dword [esp+16+12]  ; jump table
    mov     ecx, [ss:edi + PATCHJUMPTABLE.cAddresses]
    cmp     ecx, 0
    je      near PATMLookupAndCall_QueryPATM

PATMLookupAndCall_SearchStart:
    cmp     [ss:edi + PATCHJUMPTABLE.Slot_pInstrGC + eax*8], edx        ; edx = GC address to search for
    je      near PATMLookupAndCall_SearchHit
    inc     eax
    cmp     eax, ecx
    jl      near PATMLookupAndCall_SearchStart

PATMLookupAndCall_QueryPATM:
    ; nothing found -> let our trap handler try to find it
    ; @todo private ugly interface, since we have nothing generic at the moment
    lock    or  dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOOKUP_ADDRESS
    mov     eax, PATM_ACTION_LOOKUP_ADDRESS
    mov     ecx, PATM_ACTION_MAGIC
    ; edx = GC address to find
    ; edi = jump table address
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)

    jmp     near PATMLookupAndCall_SearchEnd

PATMLookupAndCall_Failure:
    ; return to caller; it must raise an error, due to patch to guest address translation (remember that there's only one copy of this code block).
    pop     ecx
    pop     edi
    pop     edx
    pop     eax
    ret

PATMLookupAndCall_SearchHit:
    mov     eax, [ss:edi + PATCHJUMPTABLE.Slot_pRelPatchGC + eax*8]        ; found a match!

    ;@note can be zero, so the next check is required!!

PATMLookupAndCall_SearchEnd:
    cmp     eax, 0
    je      near PATMLookupAndCall_Failure

    mov     ecx, eax                            ; ECX = target address (relative!)
    add     ecx, PATM_PATCHBASE                 ; Make it absolute

    mov     edx, dword PATM_STACKPTR
    cmp     dword [ss:edx], PATM_STACK_SIZE
    ja      near PATMLookupAndCall_Failure                    ; should never happen actually!!!
    cmp     dword [ss:edx], 0
    je      near PATMLookupAndCall_Failure                    ; no more room

    ; save the patch return address on our private stack
    sub     dword [ss:edx], 4                   ; sizeof(RTGCPTR)
    mov     eax, dword PATM_STACKBASE
    add     eax, dword [ss:edx]                 ; stack base + stack position
    mov     edi, dword [esp+16+8]               ; PATM return address
    mov     dword [ss:eax], edi                 ; relative address of patch return (instruction following this block)

    ; save the original return address as well (checked by ret to make sure the guest hasn't messed around with the stack)
    mov     edi, dword PATM_STACKBASE_GUEST
    add     edi, dword [ss:edx]                 ; stack base (guest) + stack position
    mov     eax, dword [esp+16+4]               ; guest return address
    mov     dword [ss:edi], eax

    mov     dword [ss:PATM_CALL_PATCH_TARGET_ADDR], ecx       ; temporarily store the target address
    pop     ecx
    pop     edi
    pop     edx
    pop     eax
    add     esp, 24                             ; parameters + return address pushed by caller (changes the flags, but that shouldn't matter)

%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    push    edx
    lea     edx, [esp + 12 - 4]                 ; stack address to store return address
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOG_CALL
    mov     eax, PATM_ACTION_LOG_CALL
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     eax
%endif

    push    dword [ss:PATM_CALL_RETURN_ADDR]   ; push original guest return address

    ; the called function will set PATM_INTERRUPTFLAG (!!)
    jmp     dword [ss:PATM_CALL_PATCH_TARGET_ADDR]

PATMLookupAndCallEnd:
; returning here -> do not add code here or after the jmp!!!!!
ENDPROC PATMLookupAndCall

; Patch record for indirect calls and jumps
GLOBALNAME PATMLookupAndCallRecord
    RTCCPTR_DEF PATMLookupAndCallStart
    DD      0
    DD      0
    DD      0
    DD      PATMLookupAndCallEnd - PATMLookupAndCallStart
%ifdef PATM_LOG_PATCHINSTR
    DD      10
%else
    DD      9
%endif
    DD      PATM_CALL_RETURN_ADDR
    DD      0
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_PATCHBASE
    DD      0
    DD      PATM_STACKPTR
    DD      0
    DD      PATM_STACKBASE
    DD      0
    DD      PATM_STACKBASE_GUEST
    DD      0
    DD      PATM_CALL_PATCH_TARGET_ADDR
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_CALL_RETURN_ADDR
    DD      0
    DD      PATM_CALL_PATCH_TARGET_ADDR
    DD      0
    DD      0ffffffffh


align 32
; Global patch function for indirect jumps
; Caller is responsible for clearing PATM_INTERRUPTFLAG and doing:
;  +  8 push    [pTargetGC]
;  +  4 push    [JumpTableAddress]
;( +  0 return address )
; And saving eflags in PATM_TEMP_EFLAGS
;
; @note NEVER change this without bumping the SSM version
BEGINPROC PATMLookupAndJump
PATMLookupAndJumpStart:
    push    eax
    push    edx
    push    edi
    push    ecx

    mov     edx, dword [esp+16+8]  ; pushed target address

    xor     eax, eax                ; default result -> nothing found
    mov     edi, dword [esp+16+4]  ; jump table
    mov     ecx, [ss:edi + PATCHJUMPTABLE.cAddresses]
    cmp     ecx, 0
    je      near PATMLookupAndJump_QueryPATM

PATMLookupAndJump_SearchStart:
    cmp     [ss:edi + PATCHJUMPTABLE.Slot_pInstrGC + eax*8], edx        ; edx = GC address to search for
    je      near PATMLookupAndJump_SearchHit
    inc     eax
    cmp     eax, ecx
    jl      near PATMLookupAndJump_SearchStart

PATMLookupAndJump_QueryPATM:
    ; nothing found -> let our trap handler try to find it
    ; @todo private ugly interface, since we have nothing generic at the moment
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOOKUP_ADDRESS
    mov     eax, PATM_ACTION_LOOKUP_ADDRESS
    mov     ecx, PATM_ACTION_MAGIC
    ; edx = GC address to find
    ; edi = jump table address
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)

    jmp     near PATMLookupAndJump_SearchEnd

PATMLookupAndJump_Failure:
    ; return to caller; it must raise an error, due to patch to guest address translation (remember that there's only one copy of this code block).
    pop     ecx
    pop     edi
    pop     edx
    pop     eax
    ret

PATMLookupAndJump_SearchHit:
    mov     eax, [ss:edi + PATCHJUMPTABLE.Slot_pRelPatchGC + eax*8]        ; found a match!

    ;@note can be zero, so the next check is required!!

PATMLookupAndJump_SearchEnd:
    cmp     eax, 0
    je      near PATMLookupAndJump_Failure

    mov     ecx, eax                            ; ECX = target address (relative!)
    add     ecx, PATM_PATCHBASE                 ; Make it absolute

    ; save jump patch target
    mov     dword [ss:PATM_TEMP_EAX], ecx
    pop     ecx
    pop     edi
    pop     edx
    pop     eax
    add     esp, 12                             ; parameters + return address pushed by caller
    ; restore flags (just to be sure)
    push    dword [ss:PATM_TEMP_EFLAGS]
    popfd

    ; the jump destination will set PATM_INTERRUPTFLAG (!!)
    jmp     dword [ss:PATM_TEMP_EAX]                      ; call duplicated patch destination address

PATMLookupAndJumpEnd:
ENDPROC PATMLookupAndJump

; Patch record for indirect calls and jumps
GLOBALNAME PATMLookupAndJumpRecord
    RTCCPTR_DEF PATMLookupAndJumpStart
    DD      0
    DD      0
    DD      0
    DD      PATMLookupAndJumpEnd - PATMLookupAndJumpStart
    DD      5
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_PATCHBASE
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      PATM_TEMP_EFLAGS
    DD      0
    DD      PATM_TEMP_EAX
    DD      0
    DD      0ffffffffh




align 32
; Patch function for static calls
; @note static calls have only one lookup slot!
; Caller is responsible for clearing PATM_INTERRUPTFLAG and adding:
;   push    [pTargetGC]
;
BEGINPROC PATMCall
PATMCallStart:
    pushfd
    push    PATM_FIXUP              ; fixup for jump table below
    push    PATM_PATCHNEXTBLOCK
    push    PATM_RETURNADDR
    DB      0E8h                    ; call
    DD      PATM_LOOKUP_AND_CALL_FUNCTION
    ; we only return in case of a failure
    add     esp, 12                 ; pushed address of jump table
    popfd
    add     esp, 4                  ; pushed by caller (changes the flags, but that shouldn't matter (@todo))
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3
%ifdef DEBUG
    ; for disassembly
    jmp     PATMCallEnd
%endif

align   4
PATMCallTable:
    DW      1                                   ; nrSlots
    DW      0                                   ; ulInsertPos
    DD      0                                   ; cAddresses
    TIMES PATCHDIRECTJUMPTABLE_SIZE DB 0        ; only one lookup slot

PATMCallEnd:
; returning here -> do not add code here or after the jmp!!!!!
ENDPROC PATMCall

; Patch record for direct calls
GLOBALNAME PATMCallRecord
    RTCCPTR_DEF PATMCallStart
    DD      0
    DD      0
    DD      0
    DD      PATMCallEnd - PATMCallStart
    DD      5
    DD      PATM_FIXUP
    DD      PATMCallTable - PATMCallStart
    DD      PATM_PATCHNEXTBLOCK
    DD      0
    DD      PATM_RETURNADDR
    DD      0
    DD      PATM_LOOKUP_AND_CALL_FUNCTION
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


align 32
; Patch function for indirect calls
; Caller is responsible for clearing PATM_INTERRUPTFLAG and adding:
;   push    [pTargetGC]
;
BEGINPROC PATMCallIndirect
PATMCallIndirectStart:
    pushfd
    push    PATM_FIXUP              ; fixup for jump table below
    push    PATM_PATCHNEXTBLOCK
    push    PATM_RETURNADDR
    DB      0E8h                    ; call
    DD      PATM_LOOKUP_AND_CALL_FUNCTION
    ; we only return in case of a failure
    add     esp, 12                 ; pushed address of jump table
    popfd
    add     esp, 4                  ; pushed by caller (changes the flags, but that shouldn't matter (@todo))
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3
%ifdef DEBUG
    ; for disassembly
    jmp     PATMCallIndirectEnd
%endif

align   4
PATMCallIndirectTable:
    DW      PATM_MAX_JUMPTABLE_ENTRIES          ; nrSlots
    DW      0                                   ; ulInsertPos
    DD      0                                   ; cAddresses
    TIMES PATCHJUMPTABLE_SIZE DB 0              ; lookup slots

PATMCallIndirectEnd:
; returning here -> do not add code here or after the jmp!!!!!
ENDPROC PATMCallIndirect

; Patch record for indirect calls
GLOBALNAME PATMCallIndirectRecord
    RTCCPTR_DEF PATMCallIndirectStart
    DD      0
    DD      0
    DD      0
    DD      PATMCallIndirectEnd - PATMCallIndirectStart
    DD      5
    DD      PATM_FIXUP
    DD      PATMCallIndirectTable - PATMCallIndirectStart
    DD      PATM_PATCHNEXTBLOCK
    DD      0
    DD      PATM_RETURNADDR
    DD      0
    DD      PATM_LOOKUP_AND_CALL_FUNCTION
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


align 32
; Patch function for indirect jumps
; Caller is responsible for clearing PATM_INTERRUPTFLAG and adding:
;   push    [pTargetGC]
;
BEGINPROC PATMJumpIndirect
PATMJumpIndirectStart:
    ; save flags (just to be sure)
    pushfd
    pop     dword [ss:PATM_TEMP_EFLAGS]

    push    PATM_FIXUP              ; fixup for jump table below
    DB      0E8h                    ; call
    DD      PATM_LOOKUP_AND_JUMP_FUNCTION
    ; we only return in case of a failure
    add     esp, 8                  ; pushed address of jump table + pushed target address

    ; restore flags (just to be sure)
    push    dword [ss:PATM_TEMP_EFLAGS]
    popfd

    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

%ifdef DEBUG
    ; for disassembly
    jmp     PATMJumpIndirectEnd
%endif

align   4
PATMJumpIndirectTable:
    DW      PATM_MAX_JUMPTABLE_ENTRIES          ; nrSlots
    DW      0                                   ; ulInsertPos
    DD      0                                   ; cAddresses
    TIMES PATCHJUMPTABLE_SIZE DB 0              ; lookup slots

PATMJumpIndirectEnd:
; returning here -> do not add code here or after the jmp!!!!!
ENDPROC PATMJumpIndirect

; Patch record for indirect jumps
GLOBALNAME PATMJumpIndirectRecord
    RTCCPTR_DEF PATMJumpIndirectStart
    DD      0
    DD      0
    DD      0
    DD      PATMJumpIndirectEnd - PATMJumpIndirectStart
    DD      5
    DD      PATM_TEMP_EFLAGS
    DD      0
    DD      PATM_FIXUP
    DD      PATMJumpIndirectTable - PATMJumpIndirectStart
    DD      PATM_LOOKUP_AND_JUMP_FUNCTION
    DD      0
    DD      PATM_TEMP_EFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; return from duplicated function
;
align 32
BEGINPROC   PATMRet
PATMRet_Start:
    ; probe stack here as we can't recover from page faults later on
    not     dword [esp-32]
    not     dword [esp-32]
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushfd
    push    eax
    push    PATM_FIXUP
    DB      0E8h                    ; call
    DD      PATM_RETURN_FUNCTION
    add     esp, 4                  ; pushed address of jump table

    cmp     eax, 0
    jne     near PATMRet_Success

    pop     eax
    popfd
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    PATM_INT3

%ifdef DEBUG
    ; for disassembly
    jmp     PATMRet_Success
%endif
align   4
PATMRetTable:
    DW      PATM_MAX_JUMPTABLE_ENTRIES          ; nrSlots
    DW      0                                   ; ulInsertPos
    DD      0                                   ; cAddresses
    TIMES PATCHJUMPTABLE_SIZE DB 0              ; lookup slots

PATMRet_Success:
    mov     dword [esp+8], eax                  ; overwrite the saved return address
    pop     eax
    popf
                                                ; caller will duplicate the ret or ret n instruction
                                                ; the patched call will set PATM_INTERRUPTFLAG after the return!
PATMRet_End:
ENDPROC     PATMRet

GLOBALNAME PATMRetRecord
    RTCCPTR_DEF PATMRet_Start
    DD      0
    DD      0
    DD      0
    DD      PATMRet_End - PATMRet_Start
    DD      4
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_FIXUP
    DD      PATMRetTable - PATMRet_Start
    DD      PATM_RETURN_FUNCTION
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; global function for implementing 'retn'
;
; Caller is responsible for right stack layout
;  + 16 original return address
;  + 12 eflags
;  +  8 eax
;  +  4 Jump table address
;( +  0 return address )
;
; @note assumes PATM_INTERRUPTFLAG is zero
; @note assumes it can trash eax and eflags
;
; @returns eax=0 on failure
;          otherwise return address in eax
;
; @note NEVER change this without bumping the SSM version
align 32
BEGINPROC   PATMRetFunction
PATMRetFunction_Start:
    push    ecx
    push    edx
    push    edi

    ; Event order:
    ; (@todo figure out which path is taken most often (1 or 2))
    ; 1) Check if the return patch address was pushed onto the PATM stack
    ; 2) Check if the return patch address can be found in the lookup table
    ; 3) Query return patch address from the hypervisor


    ; 1) Check if the return patch address was pushed on the PATM stack
    cmp     dword [ss:PATM_STACKPTR], PATM_STACK_SIZE
    jae     near PATMRetFunction_FindReturnAddress

    mov     edx, dword PATM_STACKPTR

    ; check if the return address is what we expect it to be
    mov     eax, dword PATM_STACKBASE_GUEST
    add     eax, dword [ss:edx]                 ; stack base + stack position
    mov     eax, dword [ss:eax]                 ; original return address
    cmp     eax, dword [esp+12+16]              ; pushed return address

    ; the return address was changed -> let our trap handler try to find it
    ; (can happen when the guest messes with the stack (seen it) or when we didn't call this function ourselves)
    jne     near PATMRetFunction_FindReturnAddress

    ; found it, convert relative to absolute patch address and return the result to the caller
    mov     eax, dword PATM_STACKBASE
    add     eax, dword [ss:edx]                 ; stack base + stack position
    mov     eax, dword [ss:eax]                 ; relative patm return address
    add     eax, PATM_PATCHBASE

%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ebx
    push    ecx
    push    edx
    mov     edx, eax                            ; return address
    lea     ebx, [esp+16+12+16]                 ; stack address containing the return address
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOG_RET
    mov     eax, PATM_ACTION_LOG_RET
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax
%endif

    add     dword [ss:edx], 4                   ; pop return address from the PATM stack (sizeof(RTGCPTR); @note hardcoded assumption!)

    pop     edi
    pop     edx
    pop     ecx
    ret

PATMRetFunction_FindReturnAddress:
    ; 2) Check if the return patch address can be found in the lookup table
    mov     edx, dword [esp+12+16]  ; pushed target address

    xor     eax, eax                ; default result -> nothing found
    mov     edi, dword [esp+12+4]  ; jump table
    mov     ecx, [ss:edi + PATCHJUMPTABLE.cAddresses]
    cmp     ecx, 0
    je      near PATMRetFunction_AskHypervisor

PATMRetFunction_SearchStart:
    cmp     [ss:edi + PATCHJUMPTABLE.Slot_pInstrGC + eax*8], edx        ; edx = GC address to search for
    je      near PATMRetFunction_SearchHit
    inc     eax
    cmp     eax, ecx
    jl      near PATMRetFunction_SearchStart

PATMRetFunction_AskHypervisor:
    ; 3) Query return patch address from the hypervisor
    ; @todo private ugly interface, since we have nothing generic at the moment
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOOKUP_ADDRESS
    mov     eax, PATM_ACTION_LOOKUP_ADDRESS
    mov     ecx, PATM_ACTION_MAGIC
    mov     edi, dword [esp+12+4]               ; jump table address
    mov     edx, dword [esp+12+16]              ; original return address
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    jmp     near PATMRetFunction_SearchEnd

PATMRetFunction_SearchHit:
    mov     eax, [ss:edi + PATCHJUMPTABLE.Slot_pRelPatchGC + eax*8]        ; found a match!
    ;@note can be zero, so the next check is required!!

PATMRetFunction_SearchEnd:
    cmp     eax, 0
    jz      PATMRetFunction_Failure

    add     eax, PATM_PATCHBASE

%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ebx
    push    ecx
    push    edx
    mov     edx, eax                            ; return address
    lea     ebx, [esp+16+12+16]                 ; stack address containing the return address
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOG_RET
    mov     eax, PATM_ACTION_LOG_RET
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax
%endif

    pop     edi
    pop     edx
    pop     ecx
    ret

PATMRetFunction_Failure:
    ;signal error
    xor     eax, eax
    pop     edi
    pop     edx
    pop     ecx
    ret

PATMRetFunction_End:
ENDPROC     PATMRetFunction

GLOBALNAME PATMRetFunctionRecord
    RTCCPTR_DEF PATMRetFunction_Start
    DD      0
    DD      0
    DD      0
    DD      PATMRetFunction_End - PATMRetFunction_Start
%ifdef PATM_LOG_PATCHINSTR
    DD      9
%else
    DD      7
%endif
    DD      PATM_STACKPTR
    DD      0
    DD      PATM_STACKPTR
    DD      0
    DD      PATM_STACKBASE_GUEST
    DD      0
    DD      PATM_STACKBASE
    DD      0
    DD      PATM_PATCHBASE
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_PENDINGACTION
    DD      0
    DD      PATM_PATCHBASE
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      0ffffffffh


;
; Jump to original instruction if IF=1
;
BEGINPROC   PATMCheckIF
PATMCheckIF_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf
	test	dword [ss:PATM_VMFLAGS], X86_EFL_IF
	jnz		PATMCheckIF_Safe
	nop

	; IF=0 -> unsafe, so we must call the duplicated function (which we don't do here)
	popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
	jmp     PATMCheckIF_End

PATMCheckIF_Safe:
    ; invalidate the PATM stack as we'll jump back to guest code
    mov     dword [ss:PATM_STACKPTR], PATM_STACK_SIZE

%ifdef PATM_LOG_PATCHINSTR
    push    eax
    push    ecx
    lock    or dword [ss:PATM_PENDINGACTION], PATM_ACTION_LOG_IF1
    mov     eax, PATM_ACTION_LOG_IF1
    mov     ecx, PATM_ACTION_MAGIC
    db      0fh, 0bh        ; illegal instr (hardcoded assumption in PATMHandleIllegalInstrTrap)
    pop     ecx
    pop     eax
%endif
	popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
	; IF=1 -> we can safely jump back to the original instruction
    DB      0xE9
PATMCheckIF_Jump:
    DD      PATM_JUMPDELTA
PATMCheckIF_End:
ENDPROC     PATMCheckIF

; Patch record for call instructions
GLOBALNAME PATMCheckIFRecord
    RTCCPTR_DEF PATMCheckIF_Start
    DD      PATMCheckIF_Jump - PATMCheckIF_Start
    DD      0
    DD      0
    DD      PATMCheckIF_End - PATMCheckIF_Start
%ifdef PATM_LOG_PATCHINSTR
    DD      6
%else
    DD      5
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_STACKPTR
    DD      0
%ifdef PATM_LOG_PATCHINSTR
    DD      PATM_PENDINGACTION
    DD      0
%endif
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh

;
; Jump back to guest if IF=1, else fault
;
BEGINPROC   PATMJumpToGuest_IF1
PATMJumpToGuest_IF1_Start:
    mov     dword [ss:PATM_INTERRUPTFLAG], 0
    pushf
	test	dword [ss:PATM_VMFLAGS], X86_EFL_IF
	jnz		PATMJumpToGuest_IF1_Safe
	nop

	; IF=0 -> unsafe, so fault
	popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
	PATM_INT3

PATMJumpToGuest_IF1_Safe:
	; IF=1 -> we can safely jump back to the original instruction
	popf
    mov     dword [ss:PATM_INTERRUPTFLAG], 1
    DB      0xE9
PATMJumpToGuest_IF1_Jump:
    DD      PATM_JUMPDELTA
PATMJumpToGuest_IF1_End:
ENDPROC     PATMJumpToGuest_IF1

; Patch record for call instructions
GLOBALNAME PATMJumpToGuest_IF1Record
    RTCCPTR_DEF PATMJumpToGuest_IF1_Start
    DD      PATMJumpToGuest_IF1_Jump - PATMJumpToGuest_IF1_Start
    DD      0
    DD      0
    DD      PATMJumpToGuest_IF1_End - PATMJumpToGuest_IF1_Start
    DD      4
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_VMFLAGS
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      PATM_INTERRUPTFLAG
    DD      0
    DD      0ffffffffh


; check and correct RPL of pushed ss
BEGINPROC PATMMovFromSS
PATMMovFromSS_Start:
    push    eax
    pushfd
    mov     ax, ss
    and     ax, 3
    cmp     ax, 1
    jne     near PATMMovFromSS_Continue

    and     dword [esp+8], ~3     ; clear RPL 1
PATMMovFromSS_Continue:
    popfd
    pop     eax
PATMMovFromSS_Start_End:
ENDPROC PATMMovFromSS

GLOBALNAME PATMMovFromSSRecord
    RTCCPTR_DEF PATMMovFromSS_Start
    DD      0
    DD      0
    DD      0
    DD      PATMMovFromSS_Start_End - PATMMovFromSS_Start
    DD      0
    DD      0ffffffffh




; For assertion during init (to make absolutely sure the flags are in sync in vm.mac & vm.h)
GLOBALNAME PATMInterruptFlag
    DD      VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_REQUEST

