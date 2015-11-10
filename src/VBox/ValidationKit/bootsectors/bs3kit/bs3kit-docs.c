/* $Id$ */
/** @file
 * BS3Kit - Documentation.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */



/** @page pg_bs3kit BS3Kit - Boot Sector 3 Kit
 *
 * The BS3Kit is a framework for bare metal floppy/usb image tests.
 *
 * The 3rd iteration of the framework includes support for 16-bit and 32-bit
 * C/C++ code, with provisions for 64-bit C code to possibly be added later.
 * The C code have to do without a runtime library, otherwhat what we can share
 * possibly with IPRT.
 *
 * This iteration also adds a real linker into the picture, which is an
 * improvment over early when all had to done in a single assembler run with
 * lots of includes and macros controlling what we needed.  The functions are no
 * in separate files and compiled/assembled into libraries, so the linker will
 * only include exactly what is needed.  The current linker is the OpenWatcom
 * one, wlink, that we're already using when building the BIOSes.  If it wasn't
 * for the segment/selector fixups in 16-bit code (mostly), maybe we could
 * convince the ELF linker from GNU binutils to do the job too (with help from
 * the ).
 *
 *
 *
 * @section sec_calling_convention      Calling convention
 *
 * Because we're not mixing with C code, we will use __cdecl for 16-bit and
 * 32-bit code, where as 64-bit code will use the microsoft calling AMD64
 * convention.  To avoid unnecessary %ifdef'ing in assembly code, we will use a
 * macro to load the RCX, RDX, R8 and R9 registers off the stack in 64-bit
 * assembly code.
 *
 * Register treatment in 16-bit __cdecl, 32-bit __cdecl and 64-bit msabi:
 *
 * | Register     | 16-bit      | 32-bit     | 64-bit          | ASM template |
 * | ------------ | ----------- | ---------- | --------------- | ------------ |
 * | EAX, RAX     | volatile    | volatile   | volatile        | volatile     |
 * | EBX, RBX     | volatile    | preserved  | preserved       | both         |
 * | ECX, RCX     | volatile    | volatile   | volatile, arg 0 | volatile     |
 * | EDX, RDX     | volatile    | volatile   | volatile, arg 1 | volatile     |
 * | ESP, RSP     | preserved   | preserved  | preserved       | preserved    |
 * | EBP, RBP     | preserved   | preserved  | preserved       | preserved    |
 * | EDI, RDI     | preserved   | preserved  | preserved       | preserved    |
 * | ESI, RSI     | preserved   | preserved  | preserved       | preserved    |
 * | R8           | volatile    | volatile   | volatile, arg 2 | volatile     |
 * | R9           | volatile    | volatile   | volatile, arg 3 | volatile     |
 * | R10          | volatile    | volatile   | volatile        | volatile     |
 * | R11          | volatile    | volatile   | volatile        | volatile     |
 * | R12          | volatile    | volatile   | preserved       | preserved(*) |
 * | R13          | volatile    | volatile   | preserved       | preserved(*) |
 * | R14          | volatile    | volatile   | preserved       | preserved(*) |
 * | R15          | volatile    | volatile   | preserved       | preserved(*) |
 * | RFLAGS.DF    | =0          | =0         | =0              | =0           |
 * | CS           | preserved   | preserved  | preserved       | preserved    |
 * | DS           | volatile?   | preserved? | preserved       | both         |
 * | ES           | volatile    | volatile   | preserved       | volatile     |
 * | FS           | preserved   | preserved  | preserved       | preserved    |
 * | GS           | preserved   | volatile   | preserved       | both         |
 * | SS           | preserved   | preserved  | preserved       | preserved    |
 *
 * The 'both' here means that we preserve it wrt to our caller, while at the
 * same time assuming anything we call will clobber it.
 *
 * The 'preserved(*)' marking of R12-R15 indicates that they'll be preserved in
 * 64-bit mode, but may be changed in certain cases when running 32-bit or
 * 16-bit code.  This is especially true if switching CPU mode, e.g. from 32-bit
 * protected mode to 32-bit long mode.
 *
 * For an in depth coverage of x86 and AMD64 calling convensions, see
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/function-calling-conventions.html
 *
 *
 */

