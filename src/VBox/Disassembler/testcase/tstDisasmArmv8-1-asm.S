/* $Id$ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

.private_extern _TestProcA64
_TestProcA64:
        ; Miscellaneous instructions without a parameter
        nop
        yield
        wfe
        wfi
        sev
        sevl
        dgh
        xpaclri

        ; Control flow instructions
        svc #0xfefe
        hvc #0xdead
        smc #0xcafe
        brk #0xd0d0
        hlt #0xc0de
;        tcancel #0xd00f Requires FEAT_TME
        dcps1   #0xdeca
        dcps2   #0xdec0
        dcps3   #0xfeed
        b #0x100
        b #-0x100
        bl #0x100
        bl #-0x100
        b.ne #+0x1000
        b.eq #-0x1000
;        bc.ne #+0x1000 Requires FEAT_HBC
;        bc.eq #-0x1000 Requires FEAT_HBC
        cbz x0, #+0x100
        cbz x0, #-0x100
        cbz w0, #+0x100
        cbnz x0, #+0x100
        cbnz x0, #-0x100
        cbnz w0, #+0x100
        tbz w0, #13, #+0x100
        tbz x0, #63, #-0x100
        tbz w0, #8,  #+0x100
        ret x30
        ret x1
        ret x2
        ret x15

        ; System register access instructions
        msr ttbr0_el1, x0
        mrs x0, ttbr0_el1

        ; Arithmetic instructions
        add x0, x0, #0x0
        add w0, w1, #0x10000
        add w0, w1, #65536

        adds x0, x0, #0x0
        adds w0, w1, #0x10000
        adds w0, w1, #65536

        sub x0, x0, #0x0
        sub w0, w1, #0x10000
        sub w0, w1, #65536

        subs x0, x0, #0x0
        subs w0, w1, #0x10000
        subs w0, w1, #65536

;       mov x0, x1 @todo Aliases are not supported right now.
;       mov w0, w1

        ; Logical instructions
        and  x0,  x0,  #0xffff
        and  w0,  wzr, #0xffff

        ands x0,  x0,  #0x00ffff00
        ands w10, w23, #0x55555555

        orr  x0,  x0,  #0xffff
        orr  w0,  wzr, #0xffff

        eor  x0,  x0,  #0x00ffff00
        eor w10, w23,  #0x55555555

        sbfm x0, x0, #0x1, #0x2
        sbfm w0, w0, #0xf, #0x9
        bfm  x0, x0, #0x1, #0x2
        bfm  w0, w0, #0xf, #0x9
        ubfm x0, x0, #0x1, #0x2
        ubfm w0, w0, #0xf, #0x9

        movn x0, #0xffff
        movn x0, #0xffff, LSL #16
        movn w0, #0xffff
        movn w0, #0xffff, LSL #16

        movz x0, #0xffff
        movz x0, #0xffff, LSL #48
        movz w0, #0xffff
        movz w0, #0xffff, LSL #16

        movk x0, #0xffff
        movk x0, #0xffff, LSL #32
        movk w0, #0xffff
        movk w0, #0xffff, LSL #16

        ; Logical instructions with a shifted register
        and  w0, w0, w27
        and  w0, w1, w28, LSL #1
        and  w0, w1, w28, LSL #31
        and  w0, w1, w28, LSR #1
        and  w0, w1, w28, LSR #31
        and  w0, w1, w28, ASR #1
        and  w0, w1, w28, ASR #31
        and  w0, w1, w28, ROR #1
        and  w0, w1, w28, ROR #31

        and  x0, x0, x27
        and  x0, x1, x28, LSL #1
        and  x0, x1, x28, LSL #63
        and  x0, x1, x28, LSR #1
        and  x0, x1, x28, LSR #63
        and  x0, x1, x28, ASR #1
        and  x0, x1, x28, ASR #63
        and  x0, x1, x28, ROR #1
        and  x0, x1, x28, ROR #63

        orr  w0, w0, w27
        orr  w0, w1, w28, LSL #1
        orr  w0, w1, w28, LSL #31
        orr  w0, w1, w28, LSR #1
        orr  w0, w1, w28, LSR #31
        orr  w0, w1, w28, ASR #1
        orr  w0, w1, w28, ASR #31
        orr  w0, w1, w28, ROR #1
        orr  w0, w1, w28, ROR #31

        orr  x0, x0, x27
        orr  x0, x1, x28, LSL #1
        orr  x0, x1, x28, LSL #63
        orr  x0, x1, x28, LSR #1
        orr  x0, x1, x28, LSR #63
        orr  x0, x1, x28, ASR #1
        orr  x0, x1, x28, ASR #63
        orr  x0, x1, x28, ROR #1
        orr  x0, x1, x28, ROR #63

        eor  w0, w0, w27
        eor  w0, w1, w28, LSL #1
        eor  w0, w1, w28, LSL #31
        eor  w0, w1, w28, LSR #1
        eor  w0, w1, w28, LSR #31
        eor  w0, w1, w28, ASR #1
        eor  w0, w1, w28, ASR #31
        eor  w0, w1, w28, ROR #1
        eor  w0, w1, w28, ROR #31

        eor  x0, x0, x27
        eor  x0, x1, x28, LSL #1
        eor  x0, x1, x28, LSL #63
        eor  x0, x1, x28, LSR #1
        eor  x0, x1, x28, LSR #63
        eor  x0, x1, x28, ASR #1
        eor  x0, x1, x28, ASR #63
        eor  x0, x1, x28, ROR #1
        eor  x0, x1, x28, ROR #63

        ands x0, x0, x27
        ands x0, x1, x28, LSL #1
        ands x0, x1, x28, LSL #63
        ands x0, x1, x28, LSR #1
        ands x0, x1, x28, LSR #63
        ands x0, x1, x28, ASR #1
        ands x0, x1, x28, ASR #63
        ands x0, x1, x28, ROR #1
        ands x0, x1, x28, ROR #63

        bic  w0, w0, w27
        bic  w0, w1, w28, LSL #1
        bic  w0, w1, w28, LSL #31
        bic  w0, w1, w28, LSR #1
        bic  w0, w1, w28, LSR #31
        bic  w0, w1, w28, ASR #1
        bic  w0, w1, w28, ASR #31
        bic  w0, w1, w28, ROR #1
        bic  w0, w1, w28, ROR #31

        bic  x0, x0, x27
        bic  x0, x1, x28, LSL #1
        bic  x0, x1, x28, LSL #63
        bic  x0, x1, x28, LSR #1
        bic  x0, x1, x28, LSR #63
        bic  x0, x1, x28, ASR #1
        bic  x0, x1, x28, ASR #63
        bic  x0, x1, x28, ROR #1
        bic  x0, x1, x28, ROR #63

        orn  w0, w0, w27
        orn  w0, w1, w28, LSL #1
        orn  w0, w1, w28, LSL #31
        orn  w0, w1, w28, LSR #1
        orn  w0, w1, w28, LSR #31
        orn  w0, w1, w28, ASR #1
        orn  w0, w1, w28, ASR #31
        orn  w0, w1, w28, ROR #1
        orn  w0, w1, w28, ROR #31

        orn  x0, x0, x27
        orn  x0, x1, x28, LSL #1
        orn  x0, x1, x28, LSL #63
        orn  x0, x1, x28, LSR #1
        orn  x0, x1, x28, LSR #63
        orn  x0, x1, x28, ASR #1
        orn  x0, x1, x28, ASR #63
        orn  x0, x1, x28, ROR #1
        orn  x0, x1, x28, ROR #63

        eon  w0, w0, w27
        eon  w0, w1, w28, LSL #1
        eon  w0, w1, w28, LSL #31
        eon  w0, w1, w28, LSR #1
        eon  w0, w1, w28, LSR #31
        eon  w0, w1, w28, ASR #1
        eon  w0, w1, w28, ASR #31
        eon  w0, w1, w28, ROR #1
        eon  w0, w1, w28, ROR #31

        eon  x0, x0, x27
        eon  x0, x1, x28, LSL #1
        eon  x0, x1, x28, LSL #63
        eon  x0, x1, x28, LSR #1
        eon  x0, x1, x28, LSR #63
        eon  x0, x1, x28, ASR #1
        eon  x0, x1, x28, ASR #63
        eon  x0, x1, x28, ROR #1
        eon  x0, x1, x28, ROR #63

        bics w0, w0, w27
        bics w0, w1, w28, LSL #1
        bics w0, w1, w28, LSL #31
        bics w0, w1, w28, LSR #1
        bics w0, w1, w28, LSR #31
        bics w0, w1, w28, ASR #1
        bics w0, w1, w28, ASR #31
        bics w0, w1, w28, ROR #1
        bics w0, w1, w28, ROR #31

        bics x0, x0, x27
        bics x0, x1, x28, LSL #1
        bics x0, x1, x28, LSL #63
        bics x0, x1, x28, LSR #1
        bics x0, x1, x28, LSR #63
        bics x0, x1, x28, ASR #1
        bics x0, x1, x28, ASR #63
        bics x0, x1, x28, ROR #1
        bics x0, x1, x28, ROR #63

        ; Memory loads
        ldr x0, [x28]
        ldr x0, [x28, #8]
        ldr x0, [x28, #32760]

        ldr w0, [x28]
        ldr w0, [x28, #4]
        ldr w0, [x28, #16380]

        ; Memory stores
        str x0, [x28]
        str x0, [x28, #8]
        str x0, [x28, #32760]

        str w0, [x28]
        str w0, [x28, #4]
        str w0, [x28, #16380]

        ;
        ; Keep last so the testcase can catch errors in
        ; the disassembly of the last instruction.
        ;
        nop

.private_extern _TestProcA64_EndProc
_TestProcA64_EndProc:
