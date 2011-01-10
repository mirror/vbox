/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager, Debugger & Debugging APIs.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/thread.h>


#if 0
/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_Generic(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU      pVCpu   = (PVMCPU)pvUser;
    void const *pv      = (uint8_t const *)&pVCpu->cpum.s.Guest + pDesc->offRegister;

    VMCPU_ASSERT_EMT(pVCpu);

    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U8:        pValue->u8   = *(uint8_t  const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U16:       pValue->u16  = *(uint16_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U32:       pValue->u32  = *(uint32_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U64:       pValue->u64  = *(uint64_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U128:      pValue->u128 = *(PCRTUINT128U    )pv; return VINF_SUCCESS;
        default:
            AssertMsgFailedReturn(("%d %s\n", pDesc->enmType, pDesc->pszName), VERR_INTERNAL_ERROR_3);
    }
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_Generic(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    PVMCPU      pVCpu = (PVMCPU)pvUser;
    void       *pv    = (uint8_t *)&pVCpu->cpum.s.Guest + pDesc->offRegister;

    VMCPU_ASSERT_EMT(pVCpu);

    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U8:
            *(uint8_t *)pv &= ~pfMask->u8;
            *(uint8_t *)pv |= pValue->u8 & pfMask->u8;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U16:
            *(uint16_t *)pv &= ~pfMask->u16;
            *(uint16_t *)pv |= pValue->u16 & pfMask->u16;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U32:
            *(uint32_t *)pv &= ~pfMask->u32;
            *(uint32_t *)pv |= pValue->u32 & pfMask->u32;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U64:
            *(uint64_t *)pv &= ~pfMask->u64;
            *(uint64_t *)pv |= pValue->u64 & pfMask->u64;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U128:
            ((PRTUINT128U)pv)->s.Hi &= ~pfMask->u128.s.Hi;
            ((PRTUINT128U)pv)->s.Lo &= ~pfMask->u128.s.Lo;
            ((PRTUINT128U)pv)->s.Hi |= pValue->u128.s.Hi & pfMask->u128.s.Hi;
            ((PRTUINT128U)pv)->s.Lo |= pValue->u128.s.Lo & pfMask->u128.s.Lo;
            return VINF_SUCCESS;

        default:
            AssertMsgFailedReturn(("%d %s\n", pDesc->enmType, pDesc->pszName), VERR_INTERNAL_ERROR_3);
    }
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_seg(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /** @todo perform a selector load, updating hidden selectors and stuff. */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) cpumR3RegGet_crX(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_crX(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegGet_drX(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_drX(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegGet_msr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_msr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegGet_gdtr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_gdtr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegGet_idtr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_idtr(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegGet_ftw(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) cpumR3RegSet_ftw(PVMCPU pVCpu, PCDBGFREGDESC pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_stN(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_stN(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_NOT_IMPLEMENTED;
}


/*
 * Set up aliases.
 */
#define CPUMREGALIAS_STD(Name, psz32, psz16, psz8)  \
    static DBGFREGALIAS const g_aCpumRegAliases_##Name[] = \
    { \
        { psz32, DBGFREGVALTYPE_U32     }, \
        { psz16, DBGFREGVALTYPE_U16     }, \
        { psz8,  DBGFREGVALTYPE_U8      }, \
        { NULL,  DBGFREGVALTYPE_INVALID } \
    }
CPUMREGALIAS_STD(rax,  "eax",   "ax",   "al");
CPUMREGALIAS_STD(rcx,  "ecx",   "cx",   "cl");
CPUMREGALIAS_STD(rdx,  "edx",   "dx",   "dl");
CPUMREGALIAS_STD(rbx,  "ebx",   "bx",   "bl");
CPUMREGALIAS_STD(rsp,  "esp",   "sp",   NULL);
CPUMREGALIAS_STD(rbp,  "ebp",   "bp",   NULL);
CPUMREGALIAS_STD(rsi,  "esi",   "si",  "sil");
CPUMREGALIAS_STD(rdi,  "edi",   "di",  "dil");
CPUMREGALIAS_STD(r8,   "r8d",  "r8w",  "r8b");
CPUMREGALIAS_STD(r9,   "r9d",  "r9w",  "r9b");
CPUMREGALIAS_STD(r10, "r10d", "r10w", "r10b");
CPUMREGALIAS_STD(r11, "r11d", "r11w", "r11b");
CPUMREGALIAS_STD(r12, "r12d", "r12w", "r12b");
CPUMREGALIAS_STD(r13, "r13d", "r13w", "r13b");
CPUMREGALIAS_STD(r14, "r14d", "r14w", "r14b");
CPUMREGALIAS_STD(r15, "r15d", "r15w", "r15b");
CPUMREGALIAS_STD(rip, "eip",   "ip",    NULL);
CPUMREGALIAS_STD(rflags, "eflags", "flags", NULL);
#undef CPUMREGALIAS_STD

static DBGFREGALIAS const g_aCpumRegAliases_fpuip[] =
{
    { "fpuip", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aCpumRegAliases_fpudp[] =
{
    { "fpudp", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aCpumRegAliases_cr0[] =
{
    { "msw", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

/*
 * Sub fields.
 */
/** Sub-fields for the (hidden) segment attribute register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_seg[] =
{
    DBGFREGSUBFIELD_RW("type",   0,   4,  0),
    DBGFREGSUBFIELD_RW("s",      4,   1,  0),
    DBGFREGSUBFIELD_RW("dpl",    5,   2,  0),
    DBGFREGSUBFIELD_RW("p",      7,   1,  0),
    DBGFREGSUBFIELD_RW("avl",   12,   1,  0),
    DBGFREGSUBFIELD_RW("l",     13,   1,  0),
    DBGFREGSUBFIELD_RW("d",     14,   1,  0),
    DBGFREGSUBFIELD_RW("g",     15,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the flags register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_rflags[] =
{
    DBGFREGSUBFIELD_RW("cf",     0,   1,  0),
    DBGFREGSUBFIELD_RW("pf",     2,   1,  0),
    DBGFREGSUBFIELD_RW("af",     4,   1,  0),
    DBGFREGSUBFIELD_RW("zf",     6,   1,  0),
    DBGFREGSUBFIELD_RW("sf",     7,   1,  0),
    DBGFREGSUBFIELD_RW("tf",     8,   1,  0),
    DBGFREGSUBFIELD_RW("if",     9,   1,  0),
    DBGFREGSUBFIELD_RW("df",    10,   1,  0),
    DBGFREGSUBFIELD_RW("of",    11,   1,  0),
    DBGFREGSUBFIELD_RW("iopl",  12,   2,  0),
    DBGFREGSUBFIELD_RW("nt",    14,   1,  0),
    DBGFREGSUBFIELD_RW("rf",    16,   1,  0),
    DBGFREGSUBFIELD_RW("vm",    17,   1,  0),
    DBGFREGSUBFIELD_RW("ac",    18,   1,  0),
    DBGFREGSUBFIELD_RW("vif",   19,   1,  0),
    DBGFREGSUBFIELD_RW("vip",   20,   1,  0),
    DBGFREGSUBFIELD_RW("id",    21,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU control word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_fcw[] =
{
    DBGFREGSUBFIELD_RW("im",     1,   1,  0),
    DBGFREGSUBFIELD_RW("dm",     2,   1,  0),
    DBGFREGSUBFIELD_RW("zm",     3,   1,  0),
    DBGFREGSUBFIELD_RW("om",     4,   1,  0),
    DBGFREGSUBFIELD_RW("um",     5,   1,  0),
    DBGFREGSUBFIELD_RW("pm",     6,   1,  0),
    DBGFREGSUBFIELD_RW("pc",     8,   2,  0),
    DBGFREGSUBFIELD_RW("rc",    10,   2,  0),
    DBGFREGSUBFIELD_RW("x",     12,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU status word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_fsw[] =
{
    DBGFREGSUBFIELD_RW("ie",     0,   1,  0),
    DBGFREGSUBFIELD_RW("de",     1,   1,  0),
    DBGFREGSUBFIELD_RW("ze",     2,   1,  0),
    DBGFREGSUBFIELD_RW("oe",     3,   1,  0),
    DBGFREGSUBFIELD_RW("ue",     4,   1,  0),
    DBGFREGSUBFIELD_RW("pe",     5,   1,  0),
    DBGFREGSUBFIELD_RW("se",     6,   1,  0),
    DBGFREGSUBFIELD_RW("es",     7,   1,  0),
    DBGFREGSUBFIELD_RW("c0",     8,   1,  0),
    DBGFREGSUBFIELD_RW("c1",     9,   1,  0),
    DBGFREGSUBFIELD_RW("c2",    10,   1,  0),
    DBGFREGSUBFIELD_RW("top",   11,   3,  0),
    DBGFREGSUBFIELD_RW("c3",    14,   1,  0),
    DBGFREGSUBFIELD_RW("b",     15,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_ftw[] =
{
    DBGFREGSUBFIELD_RW("tag0",   0,   2,  0),
    DBGFREGSUBFIELD_RW("tag1",   2,   2,  0),
    DBGFREGSUBFIELD_RW("tag2",   4,   2,  0),
    DBGFREGSUBFIELD_RW("tag3",   6,   2,  0),
    DBGFREGSUBFIELD_RW("tag4",   8,   2,  0),
    DBGFREGSUBFIELD_RW("tag5",  10,   2,  0),
    DBGFREGSUBFIELD_RW("tag6",  12,   2,  0),
    DBGFREGSUBFIELD_RW("tag7",  14,   2,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the Multimedia Extensions Control and Status Register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_mxcsr[] =
{
    DBGFREGSUBFIELD_RW("ie",     0,   1,  0),
    DBGFREGSUBFIELD_RW("de",     1,   1,  0),
    DBGFREGSUBFIELD_RW("ze",     2,   1,  0),
    DBGFREGSUBFIELD_RW("oe",     3,   1,  0),
    DBGFREGSUBFIELD_RW("ue",     4,   1,  0),
    DBGFREGSUBFIELD_RW("pe",     5,   1,  0),
    DBGFREGSUBFIELD_RW("daz",    6,   1,  0),
    DBGFREGSUBFIELD_RW("im",     7,   1,  0),
    DBGFREGSUBFIELD_RW("dm",     8,   1,  0),
    DBGFREGSUBFIELD_RW("zm",     9,   1,  0),
    DBGFREGSUBFIELD_RW("om",    10,   1,  0),
    DBGFREGSUBFIELD_RW("um",    11,   1,  0),
    DBGFREGSUBFIELD_RW("pm",    12,   1,  0),
    DBGFREGSUBFIELD_RW("rc",    13,   2,  0),
    DBGFREGSUBFIELD_RW("fz",    14,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_stN[] =
{
    DBGFREGSUBFIELD_RW("man",    0,  64,  0),
    DBGFREGSUBFIELD_RW("exp",   64,  15,  0),
    DBGFREGSUBFIELD_RW("sig",   79,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the MMX registers. */
static DBGFREGSUBFIELD const g_aCpumRegFields_mmN[] =
{
    DBGFREGSUBFIELD_RW("dw0",    0,  32,  0),
    DBGFREGSUBFIELD_RW("dw1",   32,  32,  0),
    DBGFREGSUBFIELD_RW("w0",     0,  16,  0),
    DBGFREGSUBFIELD_RW("w1",    16,  16,  0),
    DBGFREGSUBFIELD_RW("w2",    32,  16,  0),
    DBGFREGSUBFIELD_RW("w3",    48,  16,  0),
    DBGFREGSUBFIELD_RW("b0",     0,   8,  0),
    DBGFREGSUBFIELD_RW("b1",     8,   8,  0),
    DBGFREGSUBFIELD_RW("b2",    16,   8,  0),
    DBGFREGSUBFIELD_RW("b3",    24,   8,  0),
    DBGFREGSUBFIELD_RW("b4",    32,   8,  0),
    DBGFREGSUBFIELD_RW("b5",    40,   8,  0),
    DBGFREGSUBFIELD_RW("b6",    48,   8,  0),
    DBGFREGSUBFIELD_RW("b7",    56,   8,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the XMM registers. */
static DBGFREGSUBFIELD const g_aCpumRegFields_xmmN[] =
{
    DBGFREGSUBFIELD_RW("r0",      0,     32,  0),
    DBGFREGSUBFIELD_RW("r0.man",  0+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r0.exp",  0+23,   8,  0),
    DBGFREGSUBFIELD_RW("r0.sig",  0+31,   1,  0),
    DBGFREGSUBFIELD_RW("r1",     32,     32,  0),
    DBGFREGSUBFIELD_RW("r1.man", 32+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r1.exp", 32+23,   8,  0),
    DBGFREGSUBFIELD_RW("r1.sig", 32+31,   1,  0),
    DBGFREGSUBFIELD_RW("r2",     64,     32,  0),
    DBGFREGSUBFIELD_RW("r2.man", 64+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r2.exp", 64+23,   8,  0),
    DBGFREGSUBFIELD_RW("r2.sig", 64+31,   1,  0),
    DBGFREGSUBFIELD_RW("r3",     96,     32,  0),
    DBGFREGSUBFIELD_RW("r3.man", 96+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r3.exp", 96+23,   8,  0),
    DBGFREGSUBFIELD_RW("r3.sig", 96+31,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR0 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr0[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR3 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr3[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR4 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr4[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the DR6 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_dr6[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the DR7 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_dr7[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_apic_base[] =
{
    DBGFREGSUBFIELD_RW("bsp",     8,      1,  0),
    DBGFREGSUBFIELD_RW("ge",      9,      1,  0),
    DBGFREGSUBFIELD_RW("base",    12,    20, 12),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr_pat[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the PERF_STATUS MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_perf_status[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the EFER MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_efer[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the STAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_star[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CSTAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cstar[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the LSTAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_lstar[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the SF_MASK MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_sf_mask[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};



/**
 * The register descriptors.
 */
static DBGFREGDESC const g_aCpumRegDescs[] =
{
#define CPUMREGDESC_REG(UName, LName) \
    { #LName, DBGFREG_##UName, DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, LName), cpumR3RegGet_Generic, cpumR3RegSet_Generic, g_aCpumRegAliases_##LName, NULL }
    CPUMREGDESC_REG(RAX, rax),
    CPUMREGDESC_REG(RCX, rcx),
    CPUMREGDESC_REG(RDX, rdx),
    CPUMREGDESC_REG(RSP, rsp),
    CPUMREGDESC_REG(RBP, rbp),
    CPUMREGDESC_REG(RSI, rsi),
    CPUMREGDESC_REG(RDI, rdi),
    CPUMREGDESC_REG(R8,   r8),
    CPUMREGDESC_REG(R9,   r9),
    CPUMREGDESC_REG(R10, r10),
    CPUMREGDESC_REG(R11, r11),
    CPUMREGDESC_REG(R12, r12),
    CPUMREGDESC_REG(R13, r13),
    CPUMREGDESC_REG(R14, r14),
    CPUMREGDESC_REG(R15, r15),
#define CPUMREGDESC_SEG(UName, LName) \
    { #LName,         DBGFREG_##UName,        DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, LName),               cpumR3RegGet_Generic, cpumR3RegSet_seg,     NULL, NULL },  \
    { #LName "_attr", DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, LName##Hid.Attr.u),   cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_seg }, \
    { #LName "_base", DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, LName##Hid.u64Base),  cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL }, \
    { #LName "_lim",  DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, LName##Hid.u32Limit), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL }
    CPUMREGDESC_SEG(CS, cs),
    CPUMREGDESC_SEG(DS, ds),
    CPUMREGDESC_SEG(ES, es),
    CPUMREGDESC_SEG(FS, fs),
    CPUMREGDESC_SEG(GS, gs),
    CPUMREGDESC_SEG(SS, ss),
    CPUMREGDESC_REG(RIP, rip),
    { "rflags",     DBGFREG_RFLAGS,     DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, rflags),         cpumR3RegGet_Generic, cpumR3RegSet_Generic, g_aCpumRegAliases_rflags, g_aCpumRegFields_rflags },
    { "fcw",        DBGFREG_FCW,        DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.FCW),        cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_fcw },
    { "fsw",        DBGFREG_FSW,        DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.FSW),        cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_fsw },
    { "ftw",        DBGFREG_FTW,        DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.FTW),        cpumR3RegGet_ftw,     cpumR3RegSet_ftw,     NULL, g_aCpumRegFields_ftw },
    { "fop",        DBGFREG_FOP,        DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.FOP),        cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "fpuip",      DBGFREG_FPUIP,      DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, fpu.FPUIP),      cpumR3RegGet_Generic, cpumR3RegSet_Generic, g_aCpumRegAliases_fpuip, NULL },
    { "fpucs",      DBGFREG_FPUCS,      DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.CS),         cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "fpudp",      DBGFREG_FPUDP,      DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, fpu.FPUDP),      cpumR3RegGet_Generic, cpumR3RegSet_Generic, g_aCpumRegAliases_fpudp, NULL },
    { "fpuds",      DBGFREG_FPUDS,      DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, fpu.DS),         cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "mxcsr",      DBGFREG_MXCSR,      DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, fpu.MXCSR),      cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_mxcsr },
    { "mxcsr_mask", DBGFREG_MXCSR_MASK, DBGFREGVALTYPE_U32, 0, RT_OFFSETOF(CPUMCTX, fpu.MXCSR_MASK), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_mxcsr },
#define CPUMREGDESC_ST(n) \
    { "st" #n,      DBGFREG_ST##n,      DBGFREGVALTYPE_LRD, 0, ~(size_t)0, cpumR3RegGet_stN, cpumR3RegSet_stN, NULL, g_aCpumRegFields_stN }
    CPUMREGDESC_ST(0),
    CPUMREGDESC_ST(1),
    CPUMREGDESC_ST(2),
    CPUMREGDESC_ST(3),
    CPUMREGDESC_ST(4),
    CPUMREGDESC_ST(5),
    CPUMREGDESC_ST(6),
    CPUMREGDESC_ST(7),
#define CPUMREGDESC_MM(n) \
    { "mm" #n,      DBGFREG_MM##n,      DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, fpu.aRegs[n].mmx), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_mmN }
    CPUMREGDESC_MM(0),
    CPUMREGDESC_MM(1),
    CPUMREGDESC_MM(2),
    CPUMREGDESC_MM(3),
    CPUMREGDESC_MM(4),
    CPUMREGDESC_MM(5),
    CPUMREGDESC_MM(6),
    CPUMREGDESC_MM(7),
#define CPUMREGDESC_XMM(n) \
    { "xmm" #n,     DBGFREG_XMM##n,     DBGFREGVALTYPE_U128, 0, RT_OFFSETOF(CPUMCTX, fpu.aXMM[n].xmm), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, g_aCpumRegFields_xmmN }
    CPUMREGDESC_XMM(0),
    CPUMREGDESC_XMM(1),
    CPUMREGDESC_XMM(2),
    CPUMREGDESC_XMM(3),
    CPUMREGDESC_XMM(4),
    CPUMREGDESC_XMM(5),
    CPUMREGDESC_XMM(6),
    CPUMREGDESC_XMM(7),
    CPUMREGDESC_XMM(8),
    CPUMREGDESC_XMM(9),
    CPUMREGDESC_XMM(10),
    CPUMREGDESC_XMM(11),
    CPUMREGDESC_XMM(12),
    CPUMREGDESC_XMM(13),
    CPUMREGDESC_XMM(14),
    CPUMREGDESC_XMM(15),
    { "gdtr_base",  DBGFREG_GDTR_BASE,      DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, gdtr.pGdt),  cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "gdtr_limit", DBGFREG_GDTR_LIMIT,     DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, gdtr.cbGdt), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "idtr_base",  DBGFREG_IDTR_BASE,      DBGFREGVALTYPE_U64, 0, RT_OFFSETOF(CPUMCTX, idtr.pIdt),  cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    { "idtr_limit", DBGFREG_IDTR_LIMIT,     DBGFREGVALTYPE_U16, 0, RT_OFFSETOF(CPUMCTX, idtr.cbIdt), cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL, NULL },
    CPUMREGDESC_SEG(LDTR, ldtr),
    CPUMREGDESC_SEG(TR, tr),
    { "cr0",        DBGFREG_CR0,       DBGFREGVALTYPE_U32, 0, 0, cpumR3RegGet_crX, cpumR3RegSet_crX, g_aCpumRegAliases_cr0, g_aCpumRegFields_cr0 },
    { "cr2",        DBGFREG_CR2,       DBGFREGVALTYPE_U64, 0, 2, cpumR3RegGet_crX, cpumR3RegSet_crX, NULL, NULL },
    { "cr3",        DBGFREG_CR3,       DBGFREGVALTYPE_U64, 0, 3, cpumR3RegGet_crX, cpumR3RegSet_crX, NULL, g_aCpumRegFields_cr3 },
    { "cr4",        DBGFREG_CR4,       DBGFREGVALTYPE_U32, 0, 4, cpumR3RegGet_crX, cpumR3RegSet_crX, NULL, g_aCpumRegFields_cr4 },
    { "cr8",        DBGFREG_CR8,       DBGFREGVALTYPE_U32, 0, 8, cpumR3RegGet_crX, cpumR3RegSet_crX, NULL, NULL },
    { "dr0",        DBGFREG_DR0,       DBGFREGVALTYPE_U64, 0, 0, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, NULL },
    { "dr1",        DBGFREG_DR1,       DBGFREGVALTYPE_U64, 0, 1, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, NULL },
    { "dr2",        DBGFREG_DR2,       DBGFREGVALTYPE_U64, 0, 2, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, NULL },
    { "dr3",        DBGFREG_DR3,       DBGFREGVALTYPE_U64, 0, 3, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, NULL },
    { "dr6",        DBGFREG_DR6,       DBGFREGVALTYPE_U32, 0, 6, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, g_aCpumRegFields_dr6 },
    { "dr7",        DBGFREG_DR7,       DBGFREGVALTYPE_U32, 0, 7, cpumR3RegGet_drX, cpumR3RegSet_drX, NULL, g_aCpumRegFields_dr7 },
    { "apic_base",    DBGFREG_MSR_IA32_APICBASE,      DBGFREGVALTYPE_U32, 0, MSR_IA32_APICBASE,      cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_apic_base },
    { "pat",          DBGFREG_MSR_IA32_CR_PAT,        DBGFREGVALTYPE_U64, 0, MSR_IA32_CR_PAT,        cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_cr_pat },
    { "perf_status",  DBGFREG_MSR_IA32_PERF_STATUS,   DBGFREGVALTYPE_U64, 0, MSR_IA32_PERF_STATUS,   cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_perf_status },
    { "sysenter_cs",  DBGFREG_MSR_IA32_SYSENTER_CS,   DBGFREGVALTYPE_U16, 0, MSR_IA32_SYSENTER_CS,   cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "sysenter_eip", DBGFREG_MSR_IA32_SYSENTER_EIP,  DBGFREGVALTYPE_U32, 0, MSR_IA32_SYSENTER_EIP,  cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "sysenter_esp", DBGFREG_MSR_IA32_SYSENTER_ESP,  DBGFREGVALTYPE_U32, 0, MSR_IA32_SYSENTER_ESP,  cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "tsc",          DBGFREG_MSR_IA32_TSC,           DBGFREGVALTYPE_U32, 0, MSR_IA32_TSC,           cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "efer",         DBGFREG_MSR_K6_EFER,            DBGFREGVALTYPE_U32, 0, MSR_K6_EFER,            cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_efer },
    { "star",         DBGFREG_MSR_K6_STAR,            DBGFREGVALTYPE_U64, 0, MSR_K6_STAR,            cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_star },
    { "cstar",        DBGFREG_MSR_K8_CSTAR,           DBGFREGVALTYPE_U64, 0, MSR_K8_CSTAR,           cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_cstar },
    { "msr_fs_base",  DBGFREG_MSR_K8_FS_BASE,         DBGFREGVALTYPE_U64, 0, MSR_K8_FS_BASE,         cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "msr_gs_base",  DBGFREG_MSR_K8_GS_BASE,         DBGFREGVALTYPE_U64, 0, MSR_K8_GS_BASE,         cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "krnl_gs_base", DBGFREG_MSR_K8_KERNEL_GS_BASE,  DBGFREGVALTYPE_U64, 0, MSR_K8_KERNEL_GS_BASE,  cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "lstar",        DBGFREG_MSR_K8_LSTAR,           DBGFREGVALTYPE_U64, 0, MSR_K8_LSTAR,           cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, g_aCpumRegFields_lstar },
    { "tsc_aux",      DBGFREG_MSR_K8_TSC_AUX,         DBGFREGVALTYPE_U64, 0, MSR_K8_TSC_AUX,         cpumR3RegGet_msr, cpumR3RegSet_msr, NULL, NULL },
    { "ah",           DBGFREG_AH,       DBGFREGVALTYPE_U8,  0, RT_OFFSETOF(CPUMCTX, rax) + 1,  NULL, NULL, NULL, NULL },
    { "ch",           DBGFREG_CH,       DBGFREGVALTYPE_U8,  0, RT_OFFSETOF(CPUMCTX, rcx) + 1,  NULL, NULL, NULL, NULL },
    { "dh",           DBGFREG_DH,       DBGFREGVALTYPE_U8,  0, RT_OFFSETOF(CPUMCTX, rdx) + 1,  NULL, NULL, NULL, NULL },
    { "bh",           DBGFREG_BH,       DBGFREGVALTYPE_U8,  0, RT_OFFSETOF(CPUMCTX, rbx) + 1,  NULL, NULL, NULL, NULL },
    { "gdtr",         DBGFREG_GDTR,     DBGFREGVALTYPE_DTR, 0, ~(size_t)0, cpumR3RegGet_gdtr, cpumR3RegSet_gdtr, NULL, NULL },
    { "idtr",         DBGFREG_IDTR,     DBGFREGVALTYPE_DTR, 0, ~(size_t)0, cpumR3RegGet_idtr, cpumR3RegSet_idtr, NULL, NULL },
#undef CPUMREGDESC_REG
#undef CPUMREGDESC_SEG
#undef CPUMREGDESC_ST
#undef CPUMREGDESC_MM
#undef CPUMREGDESC_XMM
};

#endif
