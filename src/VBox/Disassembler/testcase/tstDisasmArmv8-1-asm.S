; $Id$
;; @file
; VBox disassembler: Assembler test routines - ARMv8 A64 variant
;

;
; Copyright (C) 2024 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; SPDX-License-Identifier: GPL-3.0-only
;

.private_extern _TestProcA64
_TestProcA64:
        nop
        yield
        wfe
        wfi
        sev
        sevl
        dgh
        xpaclri
        ret x30
        ret x1
        ret x2
        ret x15

        msr ttbr0_el1, x0
        mrs x0, ttbr0_el1

        b #0x100
        b #-0x100
        b.ne #+0x1000
        b.eq #-0x1000
;        bc.ne #+0x1000 Requires FEAT_HBC
;        bc.eq #-0x1000 Requires FEAT_HBC

.private_extern _TestProcA64_EndProc
_TestProcA64_EndProc:
