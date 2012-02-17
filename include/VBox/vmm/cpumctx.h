/** @file
 * CPUM - CPU Monitor(/ Manager), Context Structures.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef ___VBox_vmm_cpumctx_h
#define ___VBox_vmm_cpumctx_h

#include <iprt/types.h>
#include <iprt/x86.h>


RT_C_DECLS_BEGIN

/** @addgroup grp_cpum_ctx  The CPUM Context Structures
 * @ingroup grp_cpum
 * @{
 */

/**
 * Selector hidden registers.
 */
typedef struct CPUMSELREGHID
{
    /** Base register.
     *
     *  Long mode remarks:
     *  - Unused in long mode for CS, DS, ES, SS
     *  - 32 bits for FS & GS; FS(GS)_BASE msr used for the base address
     *  - 64 bits for TR & LDTR
     */
    uint64_t    u64Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREGHID;


/**
 * The sysenter register set.
 */
typedef struct CPUMSYSENTER
{
    /** Ring 0 cs.
     * This value +  8 is the Ring 0 ss.
     * This value + 16 is the Ring 3 cs.
     * This value + 24 is the Ring 3 ss.
     */
    uint64_t    cs;
    /** Ring 0 eip. */
    uint64_t    eip;
    /** Ring 0 esp. */
    uint64_t    esp;
} CPUMSYSENTER;


/**
 * CPU context core.
 */
#ifndef VBOX_WITHOUT_UNNAMED_UNIONS
#pragma pack(1)
typedef struct CPUMCTXCORE
{
    union
    {
        uint16_t        di;
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint16_t        si;
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint16_t        bp;
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint16_t        ax;
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint16_t        bx;
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint16_t        dx;
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint16_t        cx;
        uint32_t        ecx;
        uint64_t        rcx;
    };
    union
    {
        uint16_t        sp;
        uint32_t        esp;
        uint64_t        rsp;
    };
    /* Note: lss esp, [] in the switcher needs some space, so we reserve it here instead of relying on the exact esp & ss layout as before. */
    uint32_t            lss_esp;
    RTSEL               ss;
    RTSEL               ssPadding;

    RTSEL               gs;
    RTSEL               gsPadding;
    RTSEL               fs;
    RTSEL               fsPadding;
    RTSEL               es;
    RTSEL               esPadding;
    RTSEL               ds;
    RTSEL               dsPadding;
    RTSEL               cs;
    RTSEL               csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint16_t        ip;
        uint32_t        eip;
        uint64_t        rip;
    };

    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   esHid;
    CPUMSELREGHID   csHid;
    CPUMSELREGHID   ssHid;
    CPUMSELREGHID   dsHid;
    CPUMSELREGHID   fsHid;
    CPUMSELREGHID   gsHid;
    /** @} */

} CPUMCTXCORE;
#pragma pack()
#else  /* VBOX_WITHOUT_UNNAMED_UNIONS */
typedef struct CPUMCTXCORE CPUMCTXCORE;
#endif /* VBOX_WITHOUT_UNNAMED_UNIONS */


/**
 * CPU context.
 */
#ifndef VBOX_WITHOUT_UNNAMED_UNIONS
# pragma pack(1)
typedef struct CPUMCTX
{
    /** FPU state. (16-byte alignment)
     * @todo This doesn't have to be in X86FXSTATE on CPUs without fxsr - we need a type for the
     *       actual format or convert it (waste of time).  */
    X86FXSTATE      fpu;

    /** CPUMCTXCORE Part.
     * @{ */
    union
    {
        uint8_t         dil;
        uint16_t        di;
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint8_t         sil;
        uint16_t        si;
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint16_t        bp;
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint8_t         al;
        uint16_t        ax;
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint8_t         bl;
        uint16_t        bx;
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint8_t         dl;
        uint16_t        dx;
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint8_t         cl;
        uint16_t        cx;
        uint32_t        ecx;
        uint64_t        rcx;
    };
    union
    {
        uint16_t        sp;
        uint32_t        esp;
        uint64_t        rsp;
    };
    /** @note lss esp, [] in the switcher needs some space, so we reserve it here
     *        instead of relying on the exact esp & ss layout as before (prevented
     *        us from using a union with rsp). */
    uint32_t            lss_esp;
    RTSEL               ss;
    RTSEL               ssPadding;

    RTSEL               gs;
    RTSEL               gsPadding;
    RTSEL               fs;
    RTSEL               fsPadding;
    RTSEL               es;
    RTSEL               esPadding;
    RTSEL               ds;
    RTSEL               dsPadding;
    RTSEL               cs;
    RTSEL               csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint16_t        ip;
        uint32_t        eip;
        uint64_t        rip;
    };

    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   esHid;
    CPUMSELREGHID   csHid;
    CPUMSELREGHID   ssHid;
    CPUMSELREGHID   dsHid;
    CPUMSELREGHID   fsHid;
    CPUMSELREGHID   gsHid;
    /** @} */

    /** @} */

    /** Control registers.
     * @{ */
    uint64_t        cr0;
    uint64_t        cr2;
    uint64_t        cr3;
    uint64_t        cr4;
    /** @} */

    /** Debug registers.
     * @remarks DR4 and DR5 should not be used since they are aliases for
     *          DR6 and DR7 respectively on both AMD and Intel CPUs.
     * @remarks DR8-15 are currently not supported by AMD or Intel, so
     *          neither do we.
     * @{ */
    uint64_t        dr[8];
    /** @} */

    /** Global Descriptor Table register. */
    VBOXGDTR        gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    VBOXIDTR        idtr;
    uint16_t        idtrPadding;
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           tr;
    RTSEL           trPadding;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** System MSRs.
     * @{ */
    uint64_t        msrEFER;
    uint64_t        msrSTAR;            /**< Legacy syscall eip, cs & ss. */
    uint64_t        msrPAT;
    uint64_t        msrLSTAR;           /**< 64 bits mode syscall rip. */
    uint64_t        msrCSTAR;           /**< Compatibility mode syscall rip. */
    uint64_t        msrSFMASK;          /**< syscall flag mask. */
    uint64_t        msrKERNELGSBASE;    /**< swapgs exchange value. */
    /** @} */

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   ldtrHid;
    CPUMSELREGHID   trHid;
    /** @} */

# if 0
    /** Padding to align the size on a 64 byte boundary. */
    uint32_t        padding[6];
# endif
} CPUMCTX;
# pragma pack()
#else  /* VBOX_WITHOUT_UNNAMED_UNIONS */
typedef struct CPUMCTX CPUMCTX;
#endif /* VBOX_WITHOUT_UNNAMED_UNIONS */

/**
 * Gets the CPUMCTXCORE part of a CPUMCTX.
 */
#define CPUMCTX2CORE(pCtx) ((PCPUMCTXCORE)(void *)&(pCtx)->edi)

/**
 * Selector hidden registers, for version 1.6 saved state.
 */
typedef struct CPUMSELREGHID_VER1_6
{
    /** Base register. */
    uint32_t    u32Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREGHID_VER1_6;

/**
 * CPU context, for version 1.6 saved state.
 * @remarks PATM uses this, which is why it has to be here.
 */
#ifndef VBOX_WITHOUT_UNNAMED_UNIONS
# pragma pack(1)
typedef struct CPUMCTX_VER1_6
{
    /** FPU state. (16-byte alignment)
     * @todo This doesn't have to be in X86FXSTATE on CPUs without fxsr - we need a type for the
     *       actual format or convert it (waste of time).  */
    X86FXSTATE      fpu;

    /** CPUMCTXCORE Part.
     * @{ */
    union
    {
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint32_t        ecx;
        uint64_t        rcx;
    };
    /** @note We rely on the exact layout, because we use lss esp, [] in the
     *        switcher. */
    uint32_t        esp;
    RTSEL           ss;
    RTSEL           ssPadding;
    /* Note: no overlap with esp here. */
    uint64_t        rsp_notused;

    RTSEL           gs;
    RTSEL           gsPadding;
    RTSEL           fs;
    RTSEL           fsPadding;
    RTSEL           es;
    RTSEL           esPadding;
    RTSEL           ds;
    RTSEL           dsPadding;
    RTSEL           cs;
    RTSEL           csPadding[3];   /**< 3 words to force 8 byte alignment for the remainder. */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint32_t        eip;
        uint64_t        rip;
    };

    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID_VER1_6   esHid;
    CPUMSELREGHID_VER1_6   csHid;
    CPUMSELREGHID_VER1_6   ssHid;
    CPUMSELREGHID_VER1_6   dsHid;
    CPUMSELREGHID_VER1_6   fsHid;
    CPUMSELREGHID_VER1_6   gsHid;
    /** @} */

    /** @} */

    /** Control registers.
     * @{ */
    uint64_t        cr0;
    uint64_t        cr2;
    uint64_t        cr3;
    uint64_t        cr4;
    uint64_t        cr8;
    /** @} */

    /** Debug registers.
     * @{ */
    uint64_t        dr0;
    uint64_t        dr1;
    uint64_t        dr2;
    uint64_t        dr3;
    uint64_t        dr4; /**< @todo remove dr4 and dr5. */
    uint64_t        dr5;
    uint64_t        dr6;
    uint64_t        dr7;
    /* DR8-15 are currently not supported */
    /** @} */

    /** Global Descriptor Table register. */
    VBOXGDTR_VER1_6 gdtr;
    uint16_t        gdtrPadding;
    uint32_t        gdtrPadding64;/** @todo fix this hack */
    /** Interrupt Descriptor Table register. */
    VBOXIDTR_VER1_6 idtr;
    uint16_t        idtrPadding;
    uint32_t        idtrPadding64;/** @todo fix this hack */
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           tr;
    RTSEL           trPadding;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** System MSRs.
     * @{ */
    uint64_t        msrEFER;
    uint64_t        msrSTAR;
    uint64_t        msrPAT;
    uint64_t        msrLSTAR;
    uint64_t        msrCSTAR;
    uint64_t        msrSFMASK;
    uint64_t        msrFSBASE;
    uint64_t        msrGSBASE;
    uint64_t        msrKERNELGSBASE;
    /** @} */

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID_VER1_6   ldtrHid;
    CPUMSELREGHID_VER1_6   trHid;
    /** @} */

    /** padding to get 32byte aligned size. */
    uint32_t        padding[2];
} CPUMCTX_VER1_6;
#pragma pack()
#else  /* VBOX_WITHOUT_UNNAMED_UNIONS */
typedef struct CPUMCTX_VER1_6 CPUMCTX_VER1_6;
#endif /* VBOX_WITHOUT_UNNAMED_UNIONS */

/**
 * Additional guest MSRs (i.e. not part of the CPU context structure).
 *
 * @remarks Never change the order here because of the saved stated!  The size
 *          can in theory be changed, but keep older VBox versions in mind.
 */
typedef union CPUMCTXMSRS
{
    struct
    {
        uint64_t    TscAux;             /**< MSR_K8_TSC_AUX */
        uint64_t    MiscEnable;         /**< MSR_IA32_MISC_ENABLE */
        uint64_t    MtrrDefType;        /**< IA32_MTRR_DEF_TYPE */
        uint64_t    MtrrFix64K_00000;   /**< IA32_MTRR_FIX16K_80000 */
        uint64_t    MtrrFix16K_80000;   /**< IA32_MTRR_FIX16K_80000 */
        uint64_t    MtrrFix16K_A0000;   /**< IA32_MTRR_FIX16K_A0000 */
        uint64_t    MtrrFix4K_C0000;    /**< IA32_MTRR_FIX4K_C0000 */
        uint64_t    MtrrFix4K_C8000;    /**< IA32_MTRR_FIX4K_C8000 */
        uint64_t    MtrrFix4K_D0000;    /**< IA32_MTRR_FIX4K_D0000 */
        uint64_t    MtrrFix4K_D8000;    /**< IA32_MTRR_FIX4K_D8000 */
        uint64_t    MtrrFix4K_E0000;    /**< IA32_MTRR_FIX4K_E0000 */
        uint64_t    MtrrFix4K_E8000;    /**< IA32_MTRR_FIX4K_E8000 */
        uint64_t    MtrrFix4K_F0000;    /**< IA32_MTRR_FIX4K_F0000 */
        uint64_t    MtrrFix4K_F8000;    /**< IA32_MTRR_FIX4K_F8000 */
    } msr;
    uint64_t    au64[64];
} CPUMCTXMSRS;
/** Pointer to the guest MSR state. */
typedef CPUMCTXMSRS *PCPUMCTXMSRS;
/** Pointer to the const guest MSR state. */
typedef const CPUMCTXMSRS *PCCPUMCTXMSRS;

/**
 * The register set returned by a CPUID operation.
 */
typedef struct CPUMCPUID
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} CPUMCPUID;
/** Pointer to a CPUID leaf. */
typedef CPUMCPUID *PCPUMCPUID;
/** Pointer to a const CPUID leaf. */
typedef const CPUMCPUID *PCCPUMCPUID;

/** @}  */

RT_C_DECLS_END

#endif

