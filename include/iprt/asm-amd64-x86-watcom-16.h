/** @file
 * IPRT - AMD64 and x86 Specific Assembly Functions, 16-bit Watcom C pragma aux.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___iprt_asm_amd64_x86_h
# error "Don't include this header directly."
#endif
#ifndef ___iprt_asm_amd64_x86_watcom_16_h
#define ___iprt_asm_amd64_x86_watcom_16_h

#if !RT_FAR_DATA
# error "Only works with far data pointers!"
#endif

/*
 * Turns out we cannot use 'ds' for segment stuff here because the compiler
 * seems to insists on loading the DGROUP segment into 'ds' before calling
 * stuff when using -ecc.  Using 'es' instead as this seems to work fine.
 */

#pragma aux ASMGetIDTR = \
    "sidt fword ptr es:[bx]" \
    parm [es bx] \
    modify exact [];

#pragma aux ASMGetIdtrLimit = \
    "sub  sp, 8" \
    "mov  bx, sp" \
    "sidt fword ptr ss:[bx]" \
    "mov  bx, ss:[bx]" \
    "add  sp, 8" \
    parm [] \
    value [bx] \
    modify exact [bx];

#pragma aux ASMSetIDTR = \
    "lidt fword ptr es:[bx]" \
    parm [es bx] nomemory \
    modify nomemory;

#pragma aux ASMGetGDTR = \
    "sgdt fword ptr es:[bx]" \
    parm [es bx] \
    modify exact [];

#pragma aux ASMSetGDTR = \
    "lgdt fword ptr es:[bx]" \
    parm [es bx] nomemory \
    modify exact [] nomemory;

#pragma aux ASMGetCS = \
    "mov ax, cs" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetDS = \
    "mov ax, ds" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetES = \
    "mov ax, es" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetFS = \
    "mov ax, fs" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetGS = \
    "mov ax, gs" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetSS = \
    "mov ax, ss" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetTR = \
    "str ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetLDTR = \
    "sldt ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMGetGS = \
    "mov ax, gs" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

/** @todo ASMGetSegAttr   */

#pragma aux ASMGetFlags = \
    "pushf" \
    "pop ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMSetFlags = \
    "push ax" \
    "popf" \
    parm [ax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMChangeFlags = \
    "pushf" \
    "pop ax" \
    "and dx, ax" \
    "or  dx, cx" \
    "push dx" \
    "popf" \
    parm [dx] [cx] nomemory \
    value [ax] \
    modify exact [dx] nomemory;

#pragma aux ASMAddFlags = \
    "pushf" \
    "pop ax" \
    "or  dx, ax" \
    "push dx" \
    "popf" \
    parm [dx] nomemory \
    value [ax] \
    modify exact [dx] nomemory;

#pragma aux ASMClearFlags = \
    "pushf" \
    "pop ax" \
    "and dx, ax" \
    "push dx" \
    "popf" \
    parm [dx] nomemory \
    value [ax] \
    modify exact [dx] nomemory;

/* Note! Must use the 64-bit integer return value convension.
         The order of registers in the value [set] does not seem to mean anything. */
#pragma aux ASMReadTSC = \
    ".586" \
    "rdtsc" \
    "mov ebx, edx" \
    "mov ecx, eax" \
    "shr ecx, 16" \
    "xchg eax, edx" \
    "shr eax, 16" \
    parm [] nomemory \
    value [dx cx bx ax] \
    modify exact [ax bx cx dx] nomemory;

/** @todo ASMReadTscWithAux if needed (rdtscp not recognized by compiler)   */


/* ASMCpuId: Implemented externally, too many parameters. */
/* ASMCpuId_Idx_ECX: Implemented externally, too many parameters. */
/* ASMCpuIdExSlow: Always implemented externally. */
/* ASMCpuId_ECX_EDX: Implemented externally, too many parameters. */
/* ASMCpuId_EAX: Implemented externally, lazy bird. */
/* ASMCpuId_EBX: Implemented externally, lazy bird. */
/* ASMCpuId_ECX: Implemented externally, lazy bird. */
/* ASMCpuId_EDX: Implemented externally, lazy bird. */
/* ASMHasCpuId: MSC inline in main source file. */
/* ASMGetApicId: Implemented externally, lazy bird. */

/* Note! Again, when returning two registers, watcom have certain fixed ordering rules (low:high):
            ax:bx, ax:cx, ax:dx, ax:si, ax:di
            bx:cx, bx:dx, bx:si, bx:di
            dx:cx, si:cx, di:cx
            si:dx, di:dx
            si:di
         This ordering seems to apply to parameter values too. */
#pragma aux ASMGetCR0 = \
    "mov eax, cr0" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMSetCR0 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov cr0, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMGetCR2 = \
    "mov eax, cr2" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMSetCR2 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov cr2, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMGetCR3 = \
    "mov eax, cr3" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMSetCR3 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov cr3, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMReloadCR3 = \
    "mov eax, cr3" \
    "mov cr3, eax" \
    parm [] nomemory \
    modify exact [ax] nomemory;

#pragma aux ASMGetCR4 = \
    "mov eax, cr4" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMSetCR4 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov cr4, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

/* ASMGetCR8: Don't bother for 16-bit. */
/* ASMSetCR8: Don't bother for 16-bit. */

#pragma aux ASMIntEnable = \
    "sti" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMIntDisable = \
    "cli" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMIntDisableFlags = \
    "pushf" \
    "cli" \
    "pop ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [] nomemory;

#pragma aux ASMHalt = \
    "hlt" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMRdMsr = \
    ".586" \
    "shl ecx, 16" \
    "mov cx, ax" \
    "rdmsr" \
    "mov ebx, edx" \
    "mov ecx, eax" \
    "shr ecx, 16" \
    "xchg eax, edx" \
    "shr eax, 16" \
    parm [ax cx] nomemory \
    value [dx cx bx ax] \
    modify exact [ax bx cx dx] nomemory;

/* ASMWrMsr: Implemented externally, lazy bird. */
/* ASMRdMsrEx: Implemented externally, lazy bird. */
/* ASMWrMsrEx: Implemented externally, lazy bird. */

#pragma aux ASMRdMsr_Low = \
    ".586" \
    "shl ecx, 16" \
    "mov cx, ax" \
    "rdmsr" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [ax cx] nomemory \
    value [ax dx] \
    modify exact [ax bx cx dx] nomemory;

#pragma aux ASMRdMsr_High = \
    ".586" \
    "shl ecx, 16" \
    "mov cx, ax" \
    "rdmsr" \
    "mov eax, edx" \
    "shr edx, 16" \
    parm [ax cx] nomemory \
    value [ax dx] \
    modify exact [ax bx cx dx] nomemory;


#pragma aux ASMGetDR0 = \
    "mov eax, dr0" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetDR1 = \
    "mov eax, dr1" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetDR2 = \
    "mov eax, dr2" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetDR3 = \
    "mov eax, dr3" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetDR6 = \
    "mov eax, dr6" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetAndClearDR6 = \
    "mov edx, 0ffff0ff0h" \
    "mov eax, dr6" \
    "mov dr6, edx" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMGetDR7 = \
    "mov eax, dr7" \
    "mov edx, eax" \
    "shr edx, 16" \
    parm [] nomemory \
    value [ax dx] \
    modify exact [ax dx] nomemory;

#pragma aux ASMSetDR0 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr0, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMSetDR1 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr1, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMSetDR2 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr2, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMSetDR3 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr3, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMSetDR6 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr6, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

#pragma aux ASMSetDR7 = \
    "shl edx, 16" \
    "mov dx, ax" \
    "mov dr7, edx" \
    parm [ax dx] nomemory \
    modify exact [dx] nomemory;

/* Yeah, could've used outp here, but this keeps the main file simpler. */
#pragma aux ASMOutU8 = \
    "out dx, al" \
    parm [dx] [al] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU8 = \
    "in al, dx" \
    parm [dx] nomemory \
    value [al] \
    modify exact [] nomemory;

#pragma aux ASMOutU16 = \
    "out dx, ax" \
    parm [dx] [ax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU16 = \
    "in ax, dx" \
    parm [dx] nomemory \
    value [ax] \
    modify exact [] nomemory;

#pragma aux ASMOutU32 = \
    "shl ecx, 16" \
    "mov cx, ax" \
    "mov eax, ecx" \
    "out dx, eax" \
    parm [dx] [ax cx] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU32 = \
    "in eax, dx" \
    "mov ecx, eax" \
    "shr ecx, 16" \
    parm [dx] nomemory \
    value [ax cx] \
    modify exact [] nomemory;

#pragma aux ASMOutStrU8 = \
    "mov ds, bx" \
    "rep outsb" \
    parm [dx] [bx si] [cx] nomemory \
    modify exact [si cx ds] nomemory;

#pragma aux ASMInStrU8 = \
    "rep insb" \
    parm [dx] [es di] [cx] \
    modify exact [di cx];

#pragma aux ASMOutStrU16 = \
    "mov ds, bx" \
    "rep outsw" \
    parm [dx] [bx si] [cx] nomemory \
    modify exact [si cx ds] nomemory;

#pragma aux ASMInStrU16 = \
    "rep insw" \
    parm [dx] [es di] [cx] \
    modify exact [di cx];

#pragma aux ASMOutStrU32 = \
    "mov ds, bx" \
    "rep outsd" \
    parm [dx] [bx si] [cx] nomemory \
    modify exact [si cx ds] nomemory;

#pragma aux ASMInStrU32 = \
    "rep insd" \
    parm [dx] [es di] [cx] \
    modify exact [di cx];

/* ASMInvalidatePage: When needed. */

#pragma aux ASMWriteBackAndInvalidateCaches = \
    ".486" \
    "wbinvd" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInvalidateInternalCaches = \
    ".486" \
    "invd" \
    parm [] \
    modify exact [];

#endif

