/* $Id$ */
/** @file
 * Testing how the compiler deals with various things.
 *
 * This is testcase requires manual inspection and might not be very useful
 * in non-optimized compiler modes.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <VBox/x86.h>

#if 1

/**
 * PAE page table entry.
 */
#ifdef __GNUC__
__extension__ /* Makes it shut up about the 40 bit uint64_t field. */
#endif
typedef struct X86PTEPAEBITS64
{
    /** Flags whether(=1) or not the page is present. */
    uint64_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint64_t    u1Write : 1;
    /** User(=1) / Supervisor(=0) flag. */
    uint64_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint64_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint64_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint64_t    u1Accessed : 1;
    /** Dirty flag.
     * Indicates that the page have been written to. */
    uint64_t    u1Dirty : 1;
    /** Reserved / If PAT enabled, bit 2 of the index.  */
    uint64_t    u1PAT : 1;
    /** Global flag. (Ignored in all but final level.) */
    uint64_t    u1Global : 1;
    /** Available for use to system software. */
    uint64_t    u3Available : 3;
    /** Physical Page number of the next level. */
    uint64_t    u40PageNo : 40;
    /** MBZ bits */
    uint64_t    u11Reserved : 11;
    /** No Execute flag. */
    uint64_t    u1NoExecute : 1;
} X86PTEPAEBITS64;
/** Pointer to a page table entry. */
typedef X86PTEPAEBITS64 *PX86PTEPAEBITS64;

/**
 * PAE Page table entry.
 */
typedef union X86PTEPAE64
{
    /** Bit field view. */
    X86PTEPAEBITS64 n;
    /** Unsigned integer view */
    X86PGPAEUINT    u;
    /** 32-bit view. */
    uint32_t        au32[2];
    /** 16-bit view. */
    uint16_t        au16[4];
    /** 8-bit view. */
    uint8_t         au8[8];
} X86PTEPAE64;
/** Pointer to a PAE page table entry. */
typedef X86PTEPAE64 *PX86PTEPAE64;
/** @} */

#else /* use current (uint32_t based) PAE structures */

#define X86PTEPAE64     X86PTEPAE
#define PX86PTEPAE64    PX86PTEPAE

#endif


void SetPresent(PX86PTE pPte)
{
    pPte->n.u1Present = 1;
}


void SetPresent64(PX86PTEPAE64 pPte)
{
    pPte->n.u1Present = 1;
}


void SetWriteDirtyAccessed(PX86PTE pPte)
{
    pPte->n.u1Write = 1;
    pPte->n.u1Dirty = 1;
    pPte->n.u1Accessed = 1;
}


void SetWriteDirtyAccessed64(PX86PTEPAE64 pPte)
{
    pPte->n.u1Write = 1;
    pPte->n.u1Dirty = 1;
    pPte->n.u1Accessed = 1;
}


void SetWriteDirtyAccessedClearAVL(PX86PTE pPte)
{
    pPte->n.u1Write = 1;
    pPte->n.u1Dirty = 1;
    pPte->n.u1Accessed = 1;
    pPte->u &= ~RT_BIT(10);
}


void SetWriteDirtyAccessedClearAVL64(PX86PTEPAE64 pPte)
{
    pPte->n.u1Write = 1;
    pPte->n.u1Dirty = 1;
    pPte->n.u1Accessed = 1;
    pPte->u &= ~RT_BIT(10);                /* bad, but serves as demonstration. */
}


bool Test3232(X86PTEPAE Pte)
{
    return !!(Pte.u & RT_BIT(10));
}


bool Test3264(X86PTEPAE Pte)
{
    return !!(Pte.u & RT_BIT_64(10));
}


bool Test6432(X86PTEPAE64 Pte)
{
    return !!(Pte.u & RT_BIT(10));
}


bool Test6464(X86PTEPAE64 Pte)
{
    return !!(Pte.u & RT_BIT_64(10));
}


void Mix6432Consts(PX86PTEPAE64 pPteDst, PX86PTEPAE64 pPteSrc)
{
    pPteDst->u = pPteSrc->u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT);
}


void Mix32Var64Const64Data(PX86PTEPAE64 pPteDst, uint32_t fMask, uint32_t fFlags)
{
    pPteDst->u = (pPteDst->u & (fMask | X86_PTE_PAE_PG_MASK)) | (fFlags & ~X86_PTE_PAE_PG_MASK);
}


X86PTE Return32BitStruct(PX86PTE paPT)
{
    return paPT[10];
}


X86PTEPAE64 Return64BitStruct(PX86PTEPAE64 paPT)
{
    return paPT[10];
}


static void DisasFunction(const char *pszName, PFNRT pv)
{
    RTPrintf("tstBitFields: Disassembly of %s:\n", pszName);
    RTUINTPTR uCur = (RTUINTPTR)pv;
    RTUINTPTR uCurMax = uCur + 256;
    DISCPUSTATE Cpu = {0};
    Cpu.mode = CPUMODE_32BIT;
    do
    {
        char        sz[256];
        uint32_t    cbInstr = 0;
        if (RT_SUCCESS(DISInstr(&Cpu, uCur, 0, &cbInstr, sz)))
        {
            RTPrintf("tstBitFields: %s", sz);
            uCur += cbInstr;
        }
        else
        {
            RTPrintf("tstBitFields: %p: %02x - DISInstr failed!\n", uCur, *(uint8_t *)uCur);
            uCur += 1;
        }
    } while (Cpu.pCurInstr->opcode != OP_RETN || uCur > uCurMax);
}


int main()
{
    RTPrintf("tstBitFields: This testcase requires manual inspection of the output!\n"
             "\n"
             "tstBitFields: The compiler must be able to combine operations when\n"
             "tstBitFields: optimizing, if not we're screwed.\n"
             "\n");
    DisasFunction("SetPresent", (PFNRT)&SetPresent);
    RTPrintf("\n");
    DisasFunction("SetPresent64", (PFNRT)&SetPresent64);
    RTPrintf("\n");
    DisasFunction("SetWriteDirtyAccessed", (PFNRT)&SetWriteDirtyAccessed);
    RTPrintf("\n");
    DisasFunction("SetWriteDirtyAccessed64", (PFNRT)&SetWriteDirtyAccessed64);
    RTPrintf("\n");
    DisasFunction("SetWriteDirtyAccessedClearAVL", (PFNRT)&SetWriteDirtyAccessedClearAVL);
    RTPrintf("\n");
    DisasFunction("SetWriteDirtyAccessedClearAVL64", (PFNRT)&SetWriteDirtyAccessedClearAVL64);
    RTPrintf("\n");
    DisasFunction("Test3232", (PFNRT)&Test3232);
    DisasFunction("Test3264", (PFNRT)&Test3264);
    DisasFunction("Test6432", (PFNRT)&Test6432);
    DisasFunction("Test6464", (PFNRT)&Test6464);
    RTPrintf("\n");
    DisasFunction("Mix6432Consts", (PFNRT)&Mix6432Consts);
    RTPrintf("\n");
    DisasFunction("Mix32Var64Const64Data", (PFNRT)&Mix32Var64Const64Data);
    RTPrintf("\n");
    DisasFunction("Return32BitStruct", (PFNRT)&Return32BitStruct);
    RTPrintf("\n");
    DisasFunction("Return64BitStruct", (PFNRT)&Return64BitStruct);
    return 0;
}

