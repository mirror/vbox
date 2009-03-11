/** @file
 * VirtualBox - Types.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_types_h
#define ___VBox_types_h

#include <VBox/cdefs.h>
#include <iprt/types.h>


/** @defgroup grp_types     Basic VBox Types
 * @{
 */


/** @defgroup grp_types_both  Common Guest and Host Context Basic Types
 * @ingroup grp_types
 * @{
 */


/** @defgroup grp_types_hc  Host Context Basic Types
 * @ingroup grp_types_both
 * @{
 */

/** @} */


/** @defgroup grp_types_gc  Guest Context Basic Types
 * @ingroup grp_types_both
 * @{
 */

/** @} */


/** Pointer to per support driver session data.
 * (The data is a R0 entity and private to the the R0 SUP part. All
 * other should consider this a sort of handle.) */
typedef R0PTRTYPE(struct SUPDRVSESSION *) PSUPDRVSESSION;

/** Pointer to a VM. */
typedef struct VM                  *PVM;
/** Pointer to a VM - Ring-0 Ptr. */
typedef R0PTRTYPE(struct VM *)      PVMR0;
/** Pointer to a VM - Ring-3 Ptr. */
typedef R3PTRTYPE(struct VM *)      PVMR3;
/** Pointer to a VM - RC Ptr. */
typedef RCPTRTYPE(struct VM *)      PVMRC;

/** Pointer to a virtual CPU structure. */
typedef struct VMCPU *              PVMCPU;
/** Pointer to a virtual CPU structure - Ring-3 Ptr. */
typedef R3PTRTYPE(struct VMCPU *)   PVMCPUR3;
/** Pointer to a virtual CPU structure - Ring-0 Ptr. */
typedef R0PTRTYPE(struct VMCPU *)   PVMCPUR0;
/** Pointer to a virtual CPU structure - RC Ptr. */
typedef RCPTRTYPE(struct VMCPU *)   PVMCPURC;

/** Pointer to a ring-0 (global) VM structure. */
typedef R0PTRTYPE(struct GVM *)     PGVM;

/** Pointer to a ring-3 (user mode) VM structure. */
typedef R3PTRTYPE(struct UVM *)     PUVM;

/** Virtual CPU ID. */
typedef uint32_t VMCPUID;
/** Pointer to a virtual CPU ID. */
typedef VMCPUID *PVMCPUID;
/** @name Special CPU ID values.
 * @{ */
/** All virtual CPUs. */
#define VMCPUID_ALL         UINT32_C(0xffffffff)
/** Any virtual CPU, preferrably an idle one.
 * Intended for scheduling a VM request or some other task. */
#define VMCPUID_ANY_IDLE    UINT32_C(0xfffffffe)
/** @} */

/** VM State
 */
typedef enum VMSTATE
{
    /** The VM is being created. */
    VMSTATE_CREATING = 0,
    /** The VM is created. */
    VMSTATE_CREATED,
    /** The VM is runnning. */
    VMSTATE_RUNNING,
    /** The VM state is being loaded from file. */
    VMSTATE_LOADING,
    /** The VM is screwed because of a failed state loading. */
    VMSTATE_LOAD_FAILURE,
    /** The VM state is being saved to file. */
    VMSTATE_SAVING,
    /** The VM is suspended. */
    VMSTATE_SUSPENDED,
    /** The VM is being reset. */
    VMSTATE_RESETTING,
    /** The VM is in guru meditation over a fatal failure. */
    VMSTATE_GURU_MEDITATION,
    /** The VM is switched off, awaiting destruction. */
    VMSTATE_OFF,
    /** The VM is being destroyed. */
    VMSTATE_DESTROYING,
    /** Terminated. */
    VMSTATE_TERMINATED,
    /** hack forcing the size of the enum to 32-bits. */
    VMSTATE_MAKE_32BIT_HACK = 0x7fffffff
} VMSTATE;


/** Pointer to a PDM Driver Base Interface. */
typedef struct PDMIBASE *PPDMIBASE;
/** Pointer to a pointer to a PDM Driver Base Interface. */
typedef PPDMIBASE *PPPDMIBASE;

/** Pointer to a PDM Device Instance. */
typedef struct PDMDEVINS *PPDMDEVINS;
/** Pointer to a pointer to a PDM Device Instance. */
typedef PPDMDEVINS *PPPDMDEVINS;
/** R3 pointer to a PDM Device Instance. */
typedef R3PTRTYPE(PPDMDEVINS) PPDMDEVINSR3;
/** R0 pointer to a PDM Device Instance. */
typedef R0PTRTYPE(PPDMDEVINS) PPDMDEVINSR0;
/** RC pointer to a PDM Device Instance. */
typedef RCPTRTYPE(PPDMDEVINS) PPDMDEVINSRC;

/** Pointer to a PDM USB Device Instance. */
typedef struct PDMUSBINS *PPDMUSBINS;
/** Pointer to a pointer to a PDM USB Device Instance. */
typedef PPDMUSBINS *PPPDMUSBINS;

/** Pointer to a PDM Driver Instance. */
typedef struct PDMDRVINS *PPDMDRVINS;
/** Pointer to a pointer to a PDM Driver Instance. */
typedef PPDMDRVINS *PPPDMDRVINS;

/** Pointer to a PDM Service Instance. */
typedef struct PDMSRVINS *PPDMSRVINS;
/** Pointer to a pointer to a PDM Service Instance. */
typedef PPDMSRVINS *PPPDMSRVINS;

/** R3 pointer to a timer. */
typedef R3PTRTYPE(struct TMTIMER *) PTMTIMERR3;
/** Pointer to a R3 pointer to a timer. */
typedef PTMTIMERR3 *PPTMTIMERR3;

/** R0 pointer to a timer. */
typedef R0PTRTYPE(struct TMTIMER *) PTMTIMERR0;
/** Pointer to a R3 pointer to a timer. */
typedef PTMTIMERR0 *PPTMTIMERR0;

/** RC pointer to a timer. */
typedef RCPTRTYPE(struct TMTIMER *) PTMTIMERRC;
/** Pointer to a RC pointer to a timer. */
typedef PTMTIMERRC *PPTMTIMERRC;

/** Pointer to a timer. */
typedef CTX_SUFF(PTMTIMER)     PTMTIMER;
/** Pointer to a pointer to a timer. */
typedef PTMTIMER              *PPTMTIMER;

/** SSM Operation handle. */
typedef struct SSMHANDLE *PSSMHANDLE;

/** Pointer to a CPUMCTX. */
typedef struct CPUMCTX *PCPUMCTX;
/** Pointer to a const CPUMCTX. */
typedef const struct CPUMCTX *PCCPUMCTX;

/** Pointer to a CPU context core. */
typedef struct CPUMCTXCORE *PCPUMCTXCORE;
/** Pointer to a const CPU context core. */
typedef const struct CPUMCTXCORE *PCCPUMCTXCORE;

/** Pointer to selector hidden registers. */
typedef struct CPUMSELREGHID *PCPUMSELREGHID;
/** Pointer to const selector hidden registers. */
typedef const struct CPUMSELREGHID *PCCPUMSELREGHID;

/** @} */


/** @defgroup grp_types_idt     Interrupt Descriptor Table Entry.
 * @ingroup grp_types
 * @todo This all belongs in x86.h!
 * @{ */

/** @todo VBOXIDT -> VBOXDESCIDT, skip the complex variations. We'll never use them. */

/** IDT Entry, Task Gate view. */
#pragma pack(1)                         /* paranoia */
typedef struct VBOXIDTE_TASKGATE
{
    /** Reserved. */
    unsigned    u16Reserved1 : 16;
    /** Task Segment Selector. */
    unsigned    u16TSS : 16;
    /** More reserved. */
    unsigned    u8Reserved2 : 8;
    /** Fixed value bit 0 - Set to 1. */
    unsigned    u1Fixed0 : 1;
    /** Busy bit. */
    unsigned    u1Busy : 1;
    /** Fixed value bit 2 - Set to 1. */
    unsigned    u1Fixed1 : 1;
    /** Fixed value bit 3 - Set to 0. */
    unsigned    u1Fixed2: 1;
    /** Fixed value bit 4 - Set to 0. */
    unsigned    u1Fixed3 : 1;
    /** Descriptor Privilege level. */
    unsigned    u2DPL : 2;
    /** Present flag. */
    unsigned    u1Present : 1;
    /** Reserved. */
    unsigned    u16Reserved3 : 16;
} VBOXIDTE_TASKGATE;
#pragma pack()
/** Pointer to IDT Entry, Task gate view. */
typedef VBOXIDTE_TASKGATE *PVBOXIDTE_TASKGATE;


/** IDT Entry, Intertupt gate view. */
#pragma pack(1)                         /* paranoia */
typedef struct VBOXIDTE_INTERRUPTGATE
{
    /** Low offset word. */
    unsigned    u16OffsetLow : 16;
    /** Segment Selector. */
    unsigned    u16SegSel : 16;
    /** Reserved. */
    unsigned    u5Reserved2 : 5;
    /** Fixed value bit 0 - Set to 0. */
    unsigned    u1Fixed0 : 1;
    /** Fixed value bit 1 - Set to 0. */
    unsigned    u1Fixed1 : 1;
    /** Fixed value bit 2 - Set to 0. */
    unsigned    u1Fixed2 : 1;
    /** Fixed value bit 3 - Set to 0. */
    unsigned    u1Fixed3: 1;
    /** Fixed value bit 4 - Set to 1. */
    unsigned    u1Fixed4 : 1;
    /** Fixed value bit 5 - Set to 1. */
    unsigned    u1Fixed5 : 1;
    /** Gate size, 1 = 32 bits, 0 = 16 bits. */
    unsigned    u132BitGate : 1;
    /** Fixed value bit 5 - Set to 0. */
    unsigned    u1Fixed6 : 1;
    /** Descriptor Privilege level. */
    unsigned    u2DPL : 2;
    /** Present flag. */
    unsigned    u1Present : 1;
    /** High offset word. */
    unsigned    u16OffsetHigh : 16;
} VBOXIDTE_INTERRUPTGATE;
#pragma pack()
/** Pointer to IDT Entry, Interrupt gate view. */
typedef  VBOXIDTE_INTERRUPTGATE *PVBOXIDTE_INTERRUPTGATE;

/** IDT Entry, Trap Gate view. */
#pragma pack(1)                         /* paranoia */
typedef struct VBOXIDTE_TRAPGATE
{
    /** Low offset word. */
    unsigned    u16OffsetLow : 16;
    /** Segment Selector. */
    unsigned    u16SegSel : 16;
    /** Reserved. */
    unsigned    u5Reserved2 : 5;
    /** Fixed value bit 0 - Set to 0. */
    unsigned    u1Fixed0 : 1;
    /** Fixed value bit 1 - Set to 0. */
    unsigned    u1Fixed1 : 1;
    /** Fixed value bit 2 - Set to 0. */
    unsigned    u1Fixed2 : 1;
    /** Fixed value bit 3 - Set to 1. */
    unsigned    u1Fixed3: 1;
    /** Fixed value bit 4 - Set to 1. */
    unsigned    u1Fixed4 : 1;
    /** Fixed value bit 5 - Set to 1. */
    unsigned    u1Fixed5 : 1;
    /** Gate size, 1 = 32 bits, 0 = 16 bits. */
    unsigned    u132BitGate : 1;
    /** Fixed value bit 5 - Set to 0. */
    unsigned    u1Fixed6 : 1;
    /** Descriptor Privilege level. */
    unsigned    u2DPL : 2;
    /** Present flag. */
    unsigned    u1Present : 1;
    /** High offset word. */
    unsigned    u16OffsetHigh : 16;
} VBOXIDTE_TRAPGATE;
#pragma pack()
/** Pointer to IDT Entry, Trap Gate view. */
typedef VBOXIDTE_TRAPGATE *PVBOXIDTE_TRAPGATE;

/** IDT Entry Generic view. */
#pragma pack(1)                         /* paranoia */
typedef struct VBOXIDTE_GENERIC
{
    /** Low offset word. */
    unsigned    u16OffsetLow : 16;
    /** Segment Selector. */
    unsigned    u16SegSel : 16;
    /** Reserved. */
    unsigned    u5Reserved : 5;
    /** IDT Type part one (not used for task gate). */
    unsigned    u3Type1 : 3;
    /** IDT Type part two. */
    unsigned    u5Type2 : 5;
    /** Descriptor Privilege level. */
    unsigned    u2DPL : 2;
    /** Present flag. */
    unsigned    u1Present : 1;
    /** High offset word. */
    unsigned    u16OffsetHigh : 16;
} VBOXIDTE_GENERIC;
#pragma pack()
/** Pointer to IDT Entry Generic view. */
typedef VBOXIDTE_GENERIC *PVBOXIDTE_GENERIC;

/** IDT Type1 value. (Reserved for task gate!) */
#define VBOX_IDTE_TYPE1             0
/** IDT Type2 value - Task gate. */
#define VBOX_IDTE_TYPE2_TASK        0x5
/** IDT Type2 value - 16 bit interrupt gate. */
#define VBOX_IDTE_TYPE2_INT_16      0x6
/** IDT Type2 value - 32 bit interrupt gate. */
#define VBOX_IDTE_TYPE2_INT_32      0xe
/** IDT Type2 value - 16 bit trap gate. */
#define VBOX_IDTE_TYPE2_TRAP_16     0x7
/** IDT Type2 value - 32 bit trap gate. */
#define VBOX_IDTE_TYPE2_TRAP_32     0xf

/** IDT Entry. */
#pragma pack(1)                         /* paranoia */
typedef union VBOXIDTE
{
    /** Task gate view. */
    VBOXIDTE_TASKGATE       Task;
    /** Trap gate view. */
    VBOXIDTE_TRAPGATE       Trap;
    /** Interrupt gate view. */
    VBOXIDTE_INTERRUPTGATE  Int;
    /** Generic IDT view. */
    VBOXIDTE_GENERIC        Gen;

    /** 8 bit unsigned integer view. */
    uint8_t     au8[8];
    /** 16 bit unsigned integer view. */
    uint16_t    au16[4];
    /** 32 bit unsigned integer view. */
    uint32_t    au32[2];
    /** 64 bit unsigned integer view. */
    uint64_t    au64;
} VBOXIDTE;
#pragma pack()
/** Pointer to IDT Entry. */
typedef VBOXIDTE *PVBOXIDTE;
/** Pointer to IDT Entry. */
typedef VBOXIDTE const *PCVBOXIDTE;

#pragma pack(1)
/** IDTR */
typedef struct VBOXIDTR
{
    /** Size of the IDT. */
    uint16_t    cbIdt;
    /** Address of the IDT. */
    uint64_t     pIdt;
} VBOXIDTR, *PVBOXIDTR;
#pragma pack()

#pragma pack(1)
/** IDTR from version 1.6 */
typedef struct VBOXIDTR_VER1_6
{
    /** Size of the IDT. */
    uint16_t    cbIdt;
    /** Address of the IDT. */
    uint32_t     pIdt;
} VBOXIDTR_VER1_6, *PVBOXIDTR_VER1_6;
#pragma pack()

/** @} */


/** @def VBOXIDTE_OFFSET
 * Return the offset of an IDT entry.
 */
#define VBOXIDTE_OFFSET(desc) \
        (  ((uint32_t)((desc).Gen.u16OffsetHigh) << 16) \
         | (           (desc).Gen.u16OffsetLow        ) )

#pragma pack(1)
/** GDTR */
typedef struct VBOXGDTR
{
    /** Size of the GDT. */
    uint16_t    cbGdt;
    /** Address of the GDT. */
    uint64_t    pGdt;
} VBOXGDTR;
#pragma pack()
/** Pointer to GDTR. */
typedef VBOXGDTR *PVBOXGDTR;

#pragma pack(1)
/** GDTR from version 1.6 */
typedef struct VBOXGDTR_VER1_6
{
    /** Size of the GDT. */
    uint16_t    cbGdt;
    /** Address of the GDT. */
    uint32_t    pGdt;
} VBOXGDTR_VER1_6;
#pragma pack()

/** @} */


/**
 * 32-bit Task Segment used in raw mode.
 * @todo Move this to SELM! Use X86TSS32 instead.
 */
#pragma pack(1)
typedef struct VBOXTSS
{
    /** 0x00 - Back link to previous task. (static) */
    RTSEL       selPrev;
    uint16_t    padding1;
    /** 0x04 - Ring-0 stack pointer. (static) */
    uint32_t    esp0;
    /** 0x08 - Ring-0 stack segment. (static) */
    RTSEL       ss0;
    uint16_t    padding_ss0;
    /** 0x0c - Ring-1 stack pointer. (static) */
    uint32_t    esp1;
    /** 0x10 - Ring-1 stack segment. (static) */
    RTSEL       ss1;
    uint16_t    padding_ss1;
    /** 0x14 - Ring-2 stack pointer. (static) */
    uint32_t    esp2;
    /** 0x18 - Ring-2 stack segment. (static) */
    RTSEL       ss2;
    uint16_t    padding_ss2;
    /** 0x1c - Page directory for the task. (static) */
    uint32_t    cr3;
    /** 0x20 - EIP before task switch. */
    uint32_t    eip;
    /** 0x24 - EFLAGS before task switch. */
    uint32_t    eflags;
    /** 0x28 - EAX before task switch. */
    uint32_t    eax;
    /** 0x2c - ECX before task switch. */
    uint32_t    ecx;
    /** 0x30 - EDX before task switch. */
    uint32_t    edx;
    /** 0x34 - EBX before task switch. */
    uint32_t    ebx;
    /** 0x38 - ESP before task switch. */
    uint32_t    esp;
    /** 0x3c - EBP before task switch. */
    uint32_t    ebp;
    /** 0x40 - ESI before task switch. */
    uint32_t    esi;
    /** 0x44 - EDI before task switch. */
    uint32_t    edi;
    /** 0x48 - ES before task switch. */
    RTSEL       es;
    uint16_t    padding_es;
    /** 0x4c - CS before task switch. */
    RTSEL       cs;
    uint16_t    padding_cs;
    /** 0x50 - SS before task switch. */
    RTSEL       ss;
    uint16_t    padding_ss;
    /** 0x54 - DS before task switch. */
    RTSEL       ds;
    uint16_t    padding_ds;
    /** 0x58 - FS before task switch. */
    RTSEL       fs;
    uint16_t    padding_fs;
    /** 0x5c - GS before task switch. */
    RTSEL       gs;
    uint16_t    padding_gs;
    /** 0x60 - LDTR before task switch. */
    RTSEL       selLdt;
    uint16_t    padding_ldt;
    /** 0x64 - Debug trap flag */
    uint16_t    fDebugTrap;
    /** 0x66 -  Offset relative to the TSS of the start of the I/O Bitmap
     * and the end of the interrupt redirection bitmap. */
    uint16_t    offIoBitmap;
    /** 0x68 -  32 bytes for the virtual interrupt redirection bitmap. (VME) */
    uint8_t     IntRedirBitmap[32];
} VBOXTSS;
#pragma pack()
/** Pointer to task segment. */
typedef VBOXTSS *PVBOXTSS;
/** Pointer to const task segment. */
typedef const VBOXTSS *PCVBOXTSS;


/**
 * Data transport buffer (scatter/gather)
 */
typedef struct PDMDATASEG
{
    /** Length of buffer in entry. */
    size_t  cbSeg;
    /** Pointer to the start of the buffer. */
    void   *pvSeg;
} PDMDATASEG;
/** Pointer to a data transport segment. */
typedef PDMDATASEG *PPDMDATASEG;
/** Pointer to a const data transport segment. */
typedef PDMDATASEG const *PCPDMDATASEG;


/**
 * The current ROM page protection.
 *
 * @remarks This is part of the saved state.
 */
typedef enum PGMROMPROT
{
    /** The customary invalid value. */
    PGMROMPROT_INVALID = 0,
    /** Read from the virgin ROM page, ignore writes.
     * Map the virgin page, use write access handler to ignore writes. */
    PGMROMPROT_READ_ROM_WRITE_IGNORE,
    /** Read from the virgin ROM page, write to the shadow RAM.
     * Map the virgin page, use write access handler change the RAM. */
    PGMROMPROT_READ_ROM_WRITE_RAM,
    /** Read from the shadow ROM page, ignore writes.
     * Map the shadow page read-only, use write access handler to ignore writes. */
    PGMROMPROT_READ_RAM_WRITE_IGNORE,
    /** Read from the shadow ROM page, ignore writes.
     * Map the shadow page read-write, disabled write access handler. */
    PGMROMPROT_READ_RAM_WRITE_RAM,
    /** The end of valid values. */
    PGMROMPROT_END,
    /** The usual 32-bit type size hack. */
    PGMROMPROT_32BIT_HACK = 0x7fffffff
} PGMROMPROT;


/** @} */

#endif
