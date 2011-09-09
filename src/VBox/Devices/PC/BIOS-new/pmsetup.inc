;; @file
;; Initial system setup which needs to run in protected mode.
;;

;;
;; Copyright (C) 2004-2011 Oracle Corporation
;;
;; This file is part of VirtualBox Open Source Edition (OSE), as
;; available from http://www.virtualbox.org. This file is free software;
;; you can redistribute it and/or modify it under the terms of the GNU
;; General Public License (GPL) as published by the Free Software
;; Foundation, in version 2 as it comes in the "COPYING" file of the
;; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;;

LVT0		equ	0FEE00350h
LVT1		equ	0FEE00360h

public		pmode_setup

;; Program LVT0/LVT1 entries in the local APIC. Some Linux kernels (e.g., RHEL4
;; SMP 32-bit) expect the entries to be unmasked in virtual wire mode.

pmode_setup	proc	near

		.386
		push	eax
		push	esi
		pushf
		cli			; Interrupts would kill us!
		call	pmode_enter
		mov	eax, cr0	; Clear CR0.CD and CR0.NW
		and	eax, 09FFFFFFFh
		mov	cr0, eax
		mov	esi, LVT0	; Program LVT0 to ExtINT and unmask
		mov	eax, [esi]
		and	eax, 0FFFE00FFh
		or	ah,  7
		mov	[esi], eax
		mov	esi, LVT1	; Program LVT1 to NMI and unmask
		mov	eax, [esi]
		and	eax, 0FFFE00FFh
		or	ah,  4
		mov	[esi], eax
		call	pmode_exit
		popf
		pop	esi
		pop	eax
		.286
		ret

pmode_setup	endp
