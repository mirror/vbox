;;
;; Copyright (C) 2006-2011 Oracle Corporation
;;
;; This file is part of VirtualBox Open Source Edition (OSE), as
;; available from http://www.virtualbox.org. This file is free software;
;; you can redistribute it and/or modify it under the terms of the GNU
;; General Public License (GPL) as published by the Free Software
;; Foundation, in version 2 as it comes in the "COPYING" file of the
;; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
;; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;; --------------------------------------------------------------------
;;
;; This code is based on:
;;
;;  ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
;;
;;  Copyright (C) 2002  MandrakeSoft S.A.
;;
;;    MandrakeSoft S.A.
;;    43, rue d'Aboukir
;;    75002 Paris - France
;;    http://www.linux-mandrake.com/
;;    http://www.mandrakesoft.com/
;;
;;  This library is free software; you can redistribute it and/or
;;  modify it under the terms of the GNU Lesser General Public
;;  License as published by the Free Software Foundation; either
;;  version 2 of the License, or (at your option) any later version.
;;
;;  This library is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;  Lesser General Public License for more details.
;;
;;  You should have received a copy of the GNU Lesser General Public
;;  License along with this library; if not, write to the Free Software
;;  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
;;
;;

include	pcicfg.inc

if BX_PCIBIOS

.386

BIOS32		segment	public 'CODE' use32

public		pcibios_protected

align   16
bios32_structure:
		db	'_32_'		; signature
		dw	bios32_entry_point, 0Fh ;; 32 bit physical address
		db	0		; revision level
		db	1		; length in paragraphs
		db	0		; checksum (updated externally)
		db 	0,0,0,0,0	; reserved

align 16
bios32_entry_point:
		pushfd
		cmp	eax, 'ICP$'		; 0x49435024 "$PCI"
		jne	unknown_service

ifdef PCI_FIXED_HOST_BRIDGE_1
		mov	eax, 80000000h
		mov	dx, PCI_CFG1
		out	dx, eax
		mov	dx, PCI_CFG2
		in	eax, dx
		cmp	eax, PCI_FIXED_HOST_BRIDGE_1
		je	device_ok
endif

ifdef PCI_FIXED_HOST_BRIDGE_2
		; 18h << 11
		mov	eax, 8000c000h
		mov	dx, PCI_CFG1
		out	dx, eax
		mov	dx, PCI_CFG2
		in	eax, dx
		cmp	eax, PCI_FIXED_HOST_BRIDGE_2
		je	device_ok
		; 19h << 11
		mov	eax, 8000c800h
		mov	dx, PCI_CFG1
		out	dx, eax
		mov	dx, PCI_CFG2
		in	eax, dx
		cmp	eax, PCI_FIXED_HOST_BRIDGE_2
		je	device_ok
endif
		jmp	unknown_service
device_ok:
		mov	ebx, 000f0000h
		mov	ecx, 0
		mov	edx, pcibios_protected
		xor	al, al
		jmp	bios32_end

unknown_service:
		mov	al, 80h
bios32_end:
		popfd
		retf

align 16
pcibios_protected:
if 1
;; The old implementation of pcibios_protected will eventually go,
;; replaced by C code.
else

extrn	_pci32_function:near

		pushfd
		pushad
		call	_pci32_function
		popad
		popfd
		retf
endif
		pushfd
		cli
		push	esi
		push	edi
		cmp	al, 1		; installation check
		jne	pci_pro_f02

		mov	bx, 0210h
		mov	cx, 0
		mov	edx, ' ICP'	; 0x20494350 "PCI "
		mov	al, 1
		jmp	pci_pro_ok

pci_pro_f02: ;; find pci device
		cmp	al, 2
		jne	pci_pro_f03

		shl	ecx, 16
		mov	cx, dx
		xor	ebx, ebx
		mov	di, 0
pci_pro_devloop:
		call	pci_pro_select_reg
		mov	dx, PCI_CFG2
		in	eax, dx
		cmp	eax, ecx
		jne	pci_pro_nextdev

		cmp	si, 0
		je	pci_pro_ok

		dec	si
pci_pro_nextdev:
		inc	ebx
		cmp	ebx, MAX_BUSDEVFN 
		jne	pci_pro_devloop

		mov	ah, 86h
		jmp	pci_pro_fail

pci_pro_f03: ;; find class code
		cmp	al, 3
		jne	pci_pro_f08

		xor	ebx, ebx
		mov	di, 8
pci_pro_devloop2:
		call	pci_pro_select_reg
		mov	dx, PCI_CFG2
		in	eax, dx
		shr	eax, 8
		cmp	eax, ecx
		jne	pci_pro_nextdev2

		cmp	si, 0
		je	pci_pro_ok

		dec	si
pci_pro_nextdev2:
		inc	ebx
		cmp	ebx, MAX_BUSDEVFN 
		jne	pci_pro_devloop2

		mov	ah, 86h
		jmp	pci_pro_fail

pci_pro_f08: ;; read configuration byte
		cmp	al, 8
		jne	pci_pro_f09

		call	pci_pro_select_reg
		push	edx
		mov	dx, di
		and	dx, 3
		add	dx, PCI_CFG2
		in	al, dx
		pop	edx
		mov	cl, al
		jmp	 pci_pro_ok

pci_pro_f09: ;; read configuration word
		cmp	al, 9
		jne	pci_pro_f0a

		call	pci_pro_select_reg
		push	edx
		mov	dx, di
		and	dx, 2
		add	dx, PCI_CFG2
		in	ax, dx
		pop	edx
		mov	cx, ax
		jmp	pci_pro_ok

pci_pro_f0a: ;; read configuration dword
		cmp	al, 0Ah
		jne	pci_pro_f0b

		call	pci_pro_select_reg
		push	edx
		mov	dx, PCI_CFG2
		in	eax, dx
		pop	edx
		mov	ecx, eax
		jmp	pci_pro_ok

pci_pro_f0b: ;; write configuration byte
		cmp	al, 0Bh
		jne	pci_pro_f0c

		call	pci_pro_select_reg
		push	edx
		mov	dx, di
		and	dx, 3
		add	dx, PCI_CFG2
		mov	al, cl
		out	dx, al
		pop	edx
		jmp	pci_pro_ok

pci_pro_f0c: ;; write configuration word
		cmp	al, 0Ch
		jne	pci_pro_f0d

		call	pci_pro_select_reg
		push	edx
		mov	dx, di
		and	dx, 2
		add	dx, PCI_CFG2
		mov	ax, cx
		out	dx, ax
		pop	edx
		jmp	pci_pro_ok

pci_pro_f0d: ;; write configuration dword
		cmp	al, 0Dh
		jne	pci_pro_unknown
		call	pci_pro_select_reg
		push	edx
		mov	dx, PCI_CFG2
		mov	eax, ecx
		out	dx, eax
		pop	edx
		jmp	pci_pro_ok

pci_pro_unknown:
		mov	ah, 81h
pci_pro_fail:
		pop	edi
		pop	esi
		popfd
		stc
		retf

pci_pro_ok:
		xor	ah, ah
		pop	edi
		pop	esi
		popfd
		clc
		retf

pci_pro_select_reg:
		push	edx
		mov	eax, 800000h
		mov	ax,  bx
		shl	eax, 8
		and	di,  0FFh
		or	ax,  di
		and	al,  0FCh
		mov	dx,  PCI_CFG1
		out	dx,  eax
		pop	edx
		ret

BIOS32		ends

endif		 ; BX_PCIBIOS

		end
