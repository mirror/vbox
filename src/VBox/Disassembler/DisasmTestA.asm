;
; VBox disassembler:
; Assembler test routines

;
; Copyright (C) 2006 InnoTek Systemberatung GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;
; If you received this file as part of a commercial VirtualBox
; distribution, then only the terms of your commercial VirtualBox
; license agreement apply instead of the previous paragraph.
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

