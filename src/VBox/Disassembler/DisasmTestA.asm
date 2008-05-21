;
; VBox disassembler:
; Assembler test routines

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
%include "VBox/nasm.mac"
%include "VBox/vm.mac"
%include "VBox/err.mac"
%include "VBox/stam.mac"
%include "VBox/x86.mac"

BITS 32

BEGINCODE

align 16
BEGINPROC   TestProc
      mov   word [edi], 0123ah
      movzx eax,byte  [edx]
      movzx eax,word  [edx]
;      mov dword es:[ebx + 1234h], 0789h
;      mov word  fs:[ebx + ecx], 0654h
;      mov byte  [esi + eax*4], 0654h
;      mov bl, byte  ds:[ebp + 1234h]
;      mov al, cs:[1234h + ecx*8]
;      mov al, cs:[1234h]
;      mov ax, cs:[1234h]
;      mov eax, cs:[1234h]
      lock cmpxchg [ecx], eax
      lock cmpxchg [ecx], ax
      lock cmpxchg [ecx], dl
      movzx ESI,word  [EAX]
      in al, dx
      in ax, dx
      in eax, dx
      mov ebx, [ecx + eax*4 + 17]
      mov ebx, [ebp + eax*4 + 4]
      mov ebx, [ebp + eax*4]
      int 80h
      in  al, 60h
      in  ax, dx
      out 64h, eax

      movss xmm0, xmm1
      movsd xmm6, xmm1

      pause

ENDPROC   TestProc

%ifndef RT_OS_OS2
BITS 64
align 16
BEGINPROC TestProc64
      ;incorrectly assembled by yasm; REX.W should not be added!
      ;test rax, dword 0cc90cc90h
      mov rax, dword 0cc90cc90h
      mov rax, qword 0ffffcc90cc90h

      movzx rax,byte  [edx]
      movzx rax,word  [edx]
      movzx rax,byte  [rdx]
      lock cmpxchg [rcx], rax
      lock cmpxchg [rcx], ax
      lock cmpxchg [r15], dl
      movzx RSI, word [R8]
      in al, dx
      in ax, dx
      in eax, dx
      mov rbx, [rcx + rax*4 + 17]
      mov rbx, [rbp + rax*4 + 4]
      mov rbx, [rbp + rax*4]
      mov rbx, [ebp + eax*4]
      int 80h
      in  al, 60h
      in  ax, dx
      out 64h, eax

      movss xmm0, xmm14
      movsd xmm6, xmm1

      ret
ENDPROC   TestProc64
%endif