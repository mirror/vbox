; $Id$
;; @file
; Disassembly testcase - Invalid lock sequences for non-locking instructions.
;
; The intention is to check in a binary using the --all-invalid mode
; of tstDisasm-2.
;

;
; Copyright (C) 2008 Sun Microsystems, Inc.
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

    BITS 32

    lock mov ebp, esp
    lock mov byte [0], 0
    lock mov word [0], 0
    lock mov dword [0], 0
    lock mov word [0], 01234h
    lock mov dword [0], 012348765h
    lock mov byte [ebx], 0
    lock mov [ebx], eax
    lock mov [ebx], ax
    lock mov [ebx], al
    lock mov [ebx], edx
    lock mov [ebx], dx
    lock mov [ebx], dl
    lock ret
    lock pop  ebp
    lock push esp

