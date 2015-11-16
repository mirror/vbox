/** @file
 * IPRT - Assembly Functions, x86 32-bit Watcom C/C++ pragma aux.
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

#ifndef ___iprt_asm_h
# error "Don't include this header directly."
#endif
#ifndef ___iprt_asm_watcom_x86_32_h
#define ___iprt_asm_watcom_x86_32_h

#ifndef __FLAT__
# error "Only works with flat pointers! (-mf)"
#endif

/*
 * Turns out we cannot use 'ds' for segment stuff here because the compiler
 * seems to insists on loading the DGROUP segment into 'ds' before calling
 * stuff when using -ecc.  Using 'es' instead as this seems to work fine.
 */

#if 0 /* overkill version. */
# pragma aux ASMCompilerBarrier = \
    "nop" \
    parm [] \
    modify exact [eax ebx ecx edx es ds fs gs];
#else
# pragma aux ASMCompilerBarrier = \
    "" \
    parm [] \
    modify exact [];
#endif

#pragma aux ASMNopPause = \
    ".686p" \
    ".xmm2" \
    "pause" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMAtomicXchgU8 = \
    "xchg [ecx], al" \
    parm [ecx] [al] \
    value [al] \
    modify exact [al];

#pragma aux ASMAtomicXchgU16 = \
    "xchg [ecx], ax" \
    parm [ecx] [ax] \
    value [ax] \
    modify exact [ax];

#pragma aux ASMAtomicXchgU32 = \
    "xchg [ecx], eax" \
    parm [ecx] [eax] \
    value [eax] \
    modify exact [eax];

#pragma aux ASMAtomicXchgU64 = \
    ".586" \
    "try_again:" \
    "lock cmpxchg8b [esi]" \
    "jnz try_again" \
    parm [esi] [ebx ecx] \
    value [eax edx] \
    modify exact [edx ecx ebx eax];

#pragma aux ASMAtomicCmpXchgU8 = \
    ".486" \
    "lock cmpxchg [edx], cl" \
    "setz al" \
    parm [edx] [cl] [al] \
    value [al] \
    modify exact [al];

#pragma aux ASMAtomicCmpXchgU16 = \
    ".486" \
    "lock cmpxchg [edx], cx" \
    "setz al" \
    parm [edx] [cx] [ax] \
    value [al] \
    modify exact [ax];

#pragma aux ASMAtomicCmpXchgU32 = \
    ".486" \
    "lock cmpxchg [edx], ecx" \
    "setz al" \
    parm [edx] [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMAtomicCmpXchgU64 = \
    ".586" \
    "lock cmpxchg8b [edi]" \
    "setz al" \
    parm [edi] [ebx ecx] [eax edx] \
    value [al] \
    modify exact [eax edx];

#pragma aux ASMAtomicCmpXchgExU32 = \
    ".586" \
    "lock cmpxchg [edx], ecx" \
    "mov [edi], eax" \
    "setz al" \
    parm [edx] [ecx] [eax] [edi] \
    value [al] \
    modify exact [eax];

#pragma aux ASMAtomicCmpXchgExU64 = \
    ".586" \
    "lock cmpxchg8b [edi]" \
    "mov [esi], eax" \
    "mov [esi + 4], edx" \
    "setz al" \
    parm [edi] [ebx ecx] [eax edx] [esi] \
    value [al] \
    modify exact [eax edx];

#pragma aux ASMSerializeInstruction = \
    ".586" \
    "xor eax, eax" \
    "cpuid" \
    parm [] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMAtomicReadU64 = \
    ".586" \
    "xor eax, eax" \
    "mov edx, eax" \
    "mov ebx, eax" \
    "mov ecx, eax" \
    "lock cmpxchg8b [edi]" \
    parm [edi] \
    value [eax edx] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMAtomicUoReadU64 = \
    ".586" \
    "xor eax, eax" \
    "mov edx, eax" \
    "mov ebx, eax" \
    "mov ecx, eax" \
    "lock cmpxchg8b [edi]" \
    parm [edi] \
    value [eax edx] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMAtomicAddU16 = \
    ".486" \
    "lock xadd [ecx], ax" \
    parm [ecx] [ax] \
    value [ax] \
    modify exact [ax];

#pragma aux ASMAtomicAddU32 = \
    ".486" \
    "lock xadd [ecx], eax" \
    parm [ecx] [eax] \
    value [eax] \
    modify exact [eax];

#pragma aux ASMAtomicIncU16 = \
    ".486" \
    "mov ax, 1" \
    "lock xadd [ecx], ax" \
    "inc ax" \
    parm [ecx] \
    value [ax] \
    modify exact [ax];

#pragma aux ASMAtomicIncU32 = \
    ".486" \
    "mov eax, 1" \
    "lock xadd [ecx], eax" \
    "inc eax" \
    parm [ecx] \
    value [eax] \
    modify exact [eax];

/* ASMAtomicIncU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicDecU16 = \
    ".486" \
    "mov ax, 0ffffh" \
    "lock xadd [ecx], ax" \
    "dec ax" \
    parm [ecx] \
    value [ax] \
    modify exact [ax];

#pragma aux ASMAtomicDecU32 = \
    ".486" \
    "mov eax, 0ffffffffh" \
    "lock xadd [ecx], eax" \
    "dec eax" \
    parm [ecx] \
    value [eax] \
    modify exact [eax];

/* ASMAtomicDecU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicOrU32 = \
    "lock or [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

/* ASMAtomicOrU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicAndU32 = \
    "lock and [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

/* ASMAtomicAndU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicUoOrU32 = \
    "or [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

/* ASMAtomicUoOrU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicUoAndU32 = \
    "and [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

/* ASMAtomicUoAndU64: Should be done by C inline or in external file. */

#pragma aux ASMAtomicUoIncU32 = \
    ".486" \
    "xadd [ecx], eax" \
    "inc eax" \
    parm [ecx] \
    value [eax] \
    modify exact [eax];

#pragma aux ASMAtomicUoDecU32 = \
    ".486" \
    "mov eax, 0ffffffffh" \
    "xadd [ecx], eax" \
    "dec eax" \
    parm [ecx] \
    value [eax] \
    modify exact [eax];

#pragma aux ASMMemZeroPage = \
    "mov ecx, 1024" \
    "xor eax, eax" \
    "rep stosd"  \
    parm [edi] \
    modify exact [eax ecx edi];

#pragma aux ASMMemZero32 = \
    "shr ecx, 2" \
    "xor eax, eax" \
    "rep stosd"  \
    parm [edi] [ecx] \
    modify exact [eax ecx edi];

#pragma aux ASMMemZero32 = \
    "shr ecx, 2" \
    "rep stosd"  \
    parm [edi] [ecx] [eax]\
    modify exact [ecx edi];

#pragma aux ASMProbeReadByte = \
    "mov al, [ecx]" \
    parm [ecx] \
    value [al] \
    modify exact [al];

#pragma aux ASMBitSet = \
    "bts [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

#pragma aux ASMAtomicBitSet = \
    "lock bts [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

#pragma aux ASMBitClear = \
    "btr [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

#pragma aux ASMAtomicBitClear = \
    "lock btr [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

#pragma aux ASMBitToggle = \
    "btc [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];

#pragma aux ASMAtomicBitToggle = \
    "lock btc [ecx], eax" \
    parm [ecx] [eax] \
    modify exact [];


#pragma aux ASMBitTestAndSet = \
    "bts [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMAtomicBitTestAndSet = \
    "lock bts [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMBitTestAndClear = \
    "btr [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMAtomicBitTestAndClear = \
    "lock btr [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMBitTestAndToggle = \
    "btc [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

#pragma aux ASMAtomicBitTestAndToggle = \
    "lock btc [ecx], eax" \
    "setc al" \
    parm [ecx] [eax] \
    value [al] \
    modify exact [eax];

/** @todo this is way to much inline assembly, better off in an external function. */
#pragma aux ASMBitFirstClear = \
    "mov edx, edi" /* save start of bitmap for later */ \
    "add ecx, 31" \
    "shr ecx, 5" /* cDWord = RT_ALIGN_32(cBits, 32) / 32; */  \
    "mov eax, 0ffffffffh" \
    "repe scasd" \
    "je done" \
    "lea edi, [edi - 4]" /* rewind edi */ \
    "xor eax, [edi]" /* load inverted bits */ \
    "sub edi, edx" /* calc byte offset */ \
    "shl edi, 3" /* convert byte to bit offset */ \
    "mov edx, eax" \
    "bsf eax, edx" \
    "add eax, edi" \
    "done:" \
    parm [edi] [ecx] \
    value [eax] \
    modify exact [eax ecx edx edi];

/* ASMBitNextClear: Too much work, do when needed. */

/** @todo this is way to much inline assembly, better off in an external function. */
#pragma aux ASMBitFirstSet = \
    "mov edx, edi" /* save start of bitmap for later */ \
    "add ecx, 31" \
    "shr ecx, 5" /* cDWord = RT_ALIGN_32(cBits, 32) / 32; */  \
    "mov eax, 0ffffffffh" \
    "repe scasd" \
    "je done" \
    "lea edi, [edi - 4]" /* rewind edi */ \
    "mov eax, [edi]" /* reload previous dword */ \
    "sub edi, edx" /* calc byte offset */ \
    "shl edi, 3" /* convert byte to bit offset */ \
    "mov edx, eax" \
    "bsf eax, edx" \
    "add eax, edi" \
    "done:" \
    parm [edi] [ecx] \
    value [eax] \
    modify exact [eax ecx edx edi];

/* ASMBitNextSet: Too much work, do when needed. */

#pragma aux ASMBitFirstSetU32 = \
    "bsf eax, eax" \
    "jz  not_found" \
    "inc eax" \
    "jmp done" \
    "not_found:" \
    "xor eax, eax" \
    "done:" \
    parm [eax] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMBitLastSetU32 = \
    "bsr eax, eax" \
    "jz  not_found" \
    "inc eax" \
    "jmp done" \
    "not_found:" \
    "xor eax, eax" \
    "done:" \
    parm [eax] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMByteSwapU16 = \
    "ror ax, 8" \
    parm [ax] nomemory \
    value [ax] \
    modify exact [ax] nomemory;

#pragma aux ASMByteSwapU32 = \
    "bswap eax" \
    parm [eax] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMRotateLeftU32 = \
    "rol    eax, cl" \
    parm [eax] [ecx] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMRotateRightU32 = \
    "ror    eax, cl" \
    parm [eax] [ecx] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#endif

