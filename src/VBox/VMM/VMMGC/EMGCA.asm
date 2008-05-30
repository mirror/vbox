; $Id: EMAllA.asm 20278 2007-04-09 11:56:29Z sandervl $
;; @file
; EM Assembly Routines.
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
%include "VBox/x86.mac"

BEGINCODE

;;
; Emulate LOCK CMPXCHG instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateLockCmpXchg(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);
;
; @returns eax=0 if data written, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Second parameter - pointer to second parameter (eax)
; @param    [esp + 0ch]    Param 3 - Third parameter - third parameter
; @param    [esp + 10h]    Param 4 - Size of parameters, only 1/2/4 is valid.
; @param    [esp + 14h]    Param 4 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateLockCmpXchg
    push    ebx
    mov     ecx, [esp + 04h + 4]        ; ecx = first parameter
    mov     ebx, [esp + 08h + 4]        ; ebx = 2nd parameter (eax)
    mov     edx, [esp + 0ch + 4]        ; edx = third parameter
    mov     eax, [esp + 10h + 4]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg dword [ecx], edx            ; do 4 bytes CMPXCHG
    mov     dword [ebx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg word [ecx], dx              ; do 2 bytes CMPXCHG
    mov     word [ebx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    lock cmpxchg byte [ecx], dl              ; do 1 bytes CMPXCHG
    mov     byte [ebx], al

.done:
    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 14h + 4]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateLockCmpXchg_Error
    pop     ebx
    mov     eax, VERR_ACCESS_DENIED
    ret

ENDPROC     EMGCEmulateLockCmpXchg

;;
; Emulate CMPXCHG instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateCmpXchg(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);
;
; @returns eax=0 if data written, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Second parameter - pointer to second parameter (eax)
; @param    [esp + 0ch]    Param 3 - Third parameter - third parameter
; @param    [esp + 10h]    Param 4 - Size of parameters, only 1/2/4 is valid.
; @param    [esp + 14h]    Param 4 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateCmpXchg
    push    ebx
    mov     ecx, [esp + 04h + 4]        ; ecx = first parameter
    mov     ebx, [esp + 08h + 4]        ; ebx = 2nd parameter (eax)
    mov     edx, [esp + 0ch + 4]        ; edx = third parameter
    mov     eax, [esp + 10h + 4]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg dword [ecx], edx            ; do 4 bytes CMPXCHG
    mov     dword [ebx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg word [ecx], dx              ; do 2 bytes CMPXCHG
    mov     word [ebx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [ebx]

    cmpxchg byte [ecx], dl              ; do 1 bytes CMPXCHG
    mov     byte [ebx], al

.done:
    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 14h + 4]        ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateCmpXchg_Error
    pop     ebx
    mov     eax, VERR_ACCESS_DENIED
    ret
ENDPROC     EMGCEmulateCmpXchg

;;
; Emulate LOCK CMPXCHG8B instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateLockCmpXchg8b(RTGCPTR pu32Param1, uint32_t *pEAX, uint32_t *pEDX, uint32_t uEBX, uint32_t uECX, uint32_t *pEflags);
;
; @returns eax=0 if data written, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Address of the eax register
; @param    [esp + 0ch]    Param 3 - Address of the edx register
; @param    [esp + 10h]    Param 4 - EBX
; @param    [esp + 14h]    Param 5 - ECX
; @param    [esp + 18h]    Param 6 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateLockCmpXchg8b
    push    ebp
    push    ebx
    mov     ebp, [esp + 04h + 8]        ; ebp = first parameter
    mov     eax, [esp + 08h + 8]        ; &EAX
    mov     eax, dword [eax]
    mov     edx, [esp + 0ch + 8]        ; &EDX
    mov     edx, dword [edx]
    mov     ebx, [esp + 10h + 8]        ; EBX
    mov     ecx, [esp + 14h + 8]        ; ECX

%ifdef RT_OS_OS2
    lock cmpxchg8b [ebp]                ; do CMPXCHG8B
%else
    lock cmpxchg8b qword [ebp]          ; do CMPXCHG8B
%endif
    mov     ebx, dword [esp + 08h + 8]
    mov     dword [ebx], eax
    mov     ebx, dword [esp + 0ch + 8]
    mov     dword [ebx], edx

    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 18h + 8]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    pop     ebp
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateLockCmpXchg8b_Error
    pop     ebx
    pop     ebp
    mov     eax, VERR_ACCESS_DENIED
    ret

ENDPROC     EMGCEmulateLockCmpXchg8b

;;
; Emulate CMPXCHG8B instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateCmpXchg8b(RTGCPTR pu32Param1, uint32_t *pEAX, uint32_t *pEDX, uint32_t uEBX, uint32_t uECX, uint32_t *pEflags);
;
; @returns eax=0 if data written, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Address of the eax register
; @param    [esp + 0ch]    Param 3 - Address of the edx register
; @param    [esp + 10h]    Param 4 - EBX
; @param    [esp + 14h]    Param 5 - ECX
; @param    [esp + 18h]    Param 6 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateCmpXchg8b
    push    ebp
    push    ebx
    mov     ebp, [esp + 04h + 8]        ; ebp = first parameter
    mov     eax, [esp + 08h + 8]        ; &EAX
    mov     eax, dword [eax]
    mov     edx, [esp + 0ch + 8]        ; &EDX
    mov     edx, dword [edx]
    mov     ebx, [esp + 10h + 8]        ; EBX
    mov     ecx, [esp + 14h + 8]        ; ECX

%ifdef RT_OS_OS2
    cmpxchg8b [ebp]                     ; do CMPXCHG8B
%else
    cmpxchg8b qword [ebp]               ; do CMPXCHG8B
%endif
    mov     ebx, dword [esp + 08h + 8]
    mov     dword [ebx], eax
    mov     ebx, dword [esp + 0ch + 8]
    mov     dword [ebx], edx

    ; collect flags and return.
    pushf
    pop     eax

    mov     edx, [esp + 18h + 8]            ; eflags pointer
    mov     dword [edx], eax

    pop     ebx
    pop     ebp
    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateCmpXchg8b_Error
    pop     ebx
    pop     ebp
    mov     eax, VERR_ACCESS_DENIED
    ret
ENDPROC     EMGCEmulateCmpXchg8b

;;
; Emulate LOCK XADD instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateLockXAdd(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);
;
; @returns eax=0 if data exchanged, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Second parameter - pointer to second parameter (general register)
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @param    [esp + 10h]    Param 4 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateLockXAdd
    mov     ecx, [esp + 04h + 0]        ; ecx = first parameter
    mov     edx, [esp + 08h + 0]        ; edx = 2nd parameter
    mov     eax, [esp + 0ch + 0]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    lock xadd dword [ecx], eax              ; do 4 bytes XADD
    mov     dword [edx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    lock xadd word [ecx], ax                ; do 2 bytes XADD
    mov     word [edx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    lock xadd byte [ecx], al                ; do 1 bytes XADD
    mov     byte [edx], al

.done:
    ; collect flags and return.
    mov     edx, [esp + 10h + 0]            ; eflags pointer
    pushf
    pop     dword [edx]

    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateLockXAdd_Error
    mov     eax, VERR_ACCESS_DENIED
    ret

ENDPROC     EMGCEmulateLockXAdd

;;
; Emulate XADD instruction, CDECL calling conv.
; EMGCDECL(uint32_t) EMGCEmulateXAdd(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);
;
; @returns eax=0 if data written, other code - invalid access, #PF was generated.
; @param    [esp + 04h]    Param 1 - First parameter - pointer to first parameter
; @param    [esp + 08h]    Param 2 - Second parameter - pointer to second parameter (general register)
; @param    [esp + 0ch]    Param 3 - Size of parameters, only 1/2/4 is valid.
; @param    [esp + 10h]    Param 4 - Pointer to eflags (out)
; @uses     eax, ecx, edx
;
align 16
BEGINPROC   EMGCEmulateXAdd
    mov     ecx, [esp + 04h + 0]        ; ecx = first parameter
    mov     edx, [esp + 08h + 0]        ; edx = 2nd parameter (eax)
    mov     eax, [esp + 0ch + 0]        ; eax = size of parameters

    cmp     al, 4
    je short .do_dword                  ; 4 bytes variant
    cmp     al, 2
    je short .do_word                   ; 2 byte variant
    cmp     al, 1
    je short .do_byte                   ; 1 bytes variant
    int3

.do_dword:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    xadd    dword [ecx], eax            ; do 4 bytes XADD
    mov     dword [edx], eax
    jmp     short .done

.do_word:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    xadd    word [ecx], ax              ; do 2 bytes XADD
    mov     word [edx], ax
    jmp     short .done

.do_byte:
    ; load 2nd parameter's value
    mov     eax, dword [edx]
    xadd    byte [ecx], al              ; do 1 bytes XADD
    mov     byte [edx], al

.done:
    ; collect flags and return.
    mov     edx, [esp + 10h + 0]        ; eflags pointer
    pushf
    pop     dword [edx]

    mov     eax, VINF_SUCCESS
    retn

; Read error - we will be here after our page fault handler.
GLOBALNAME EMGCEmulateXAdd_Error
    mov     eax, VERR_ACCESS_DENIED
    ret
ENDPROC     EMGCEmulateXAdd
