; $Id$
;; @file
; Stuff for drawing the BIOS logo.
;

;
; Copyright (C) 2004-2016 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;; Switches back to default CPU mode.
SET_DEFAULT_CPU macro
 if VBOX_BIOS_CPU eq 8086
		.8086
 elseif VBOX_BIOS_CPU eq 80186
		.186
 elseif VBOX_BIOS_CPU eq 80286
		.286
 elseif VBOX_BIOS_CPU eq 80386
		.386
 else
  .errnz 1, Unsupported target CPU value: VBOX_BIOS_CPU
		.386
 endif
endm


;; Switches back to default CPU mode, max 286.
SET_DEFAULT_CPU_286 macro
 if VBOX_BIOS_CPU eq 8086
		.8086
 elseif VBOX_BIOS_CPU eq 80186
		.186
 else
		.286
 endif
endm


;; For handling the pusha instruction depending on target CPU.
DO_PUSHA        macro
 if VBOX_BIOS_CPU eq 8086
        push    ax
        push    cx
        push    dx
        push    bx
        push    sp
        push    bp
        push    si
        push    di
 else
        pusha
 endif
endm


;; For handling the popad/popa instruction depending on target CPU.
DO_POPA         macro
 if VBOX_BIOS_CPU eq 8086
        pop     di
        pop     si
        pop     bp
        pop     bx ;pop sp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
 else
        popa
 endif
endm


;; For handling the pushad/pusha instruction depending on target CPU.
DO_PUSHAD       macro
 if VBOX_BIOS_CPU eq 8086
        push    ax
        push    cx
        push    dx
        push    bx
        push    sp
        push    bp
        push    si
        push    di
 elseif VBOX_BIOS_CPU lt 80386
        pusha
 else
        pushad
 endif
endm


;; For handling the popad/popa instruction depending on target CPU.
DO_POPAD        macro
 if VBOX_BIOS_CPU eq 8086
        pop     di
        pop     si
        pop     bp
        pop     bx ;pop sp
        pop     bx
        pop     dx
        pop     cx
        pop     ax
 elseif VBOX_BIOS_CPU lt 80386
        popa
 else
        popad
 endif
endm


;; Adds a special label that will have its address checked after linking.
BIOSORG_CHECK macro addr
public biosorg_check_at_&addr
biosorg_check_at_&addr:
		endm

;; Adds a special label that will have its address checked after linking.
BIOSORG_CHECK_BEFORE macro addr
public	biosorg_check_before_or_at_&addr
biosorg_check_before_or_at_&addr:
		endm


