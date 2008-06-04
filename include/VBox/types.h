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

/** @deprecated
 * @{ */
typedef RTHCPHYS    VBOXHCPHYS;
typedef VBOXHCPHYS *PVBOXHCPHYS;
#define NILVBOXHCPHYS NIL_RTHCPHYS
typedef RTHCPTR     VBOXHCPTR;
typedef VBOXHCPTR  *PVBOXHCPTR;
/** @} */

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
typedef struct VM              *PVM;
/** Pointer to a VM - Ring-0 Ptr. */
typedef R0PTRTYPE(struct VM *)  PVMR0;
/** Pointer to a VM - Ring-3 Ptr. */
typedef R3PTRTYPE(struct VM *)  PVMR3;
/** Pointer to a VM - RC Ptr. */
typedef RCPTRTYPE(struct VM *)  PVMRC;

/** Pointer to a ring-0 (global) VM structure. */
typedef R0PTRTYPE(struct GVM *) PGVM;

/** Pointer to a ring-3 (user mode) VM structure. */
typedef R3PTRTYPE(struct UVM *) PUVM;


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
/** GC pointer to a PDM Device Instance. */
typedef RCPTRTYPE(PPDMDEVINS) PPDMDEVINSGC;

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

/** GC pointer to a timer. */
typedef RCPTRTYPE(struct TMTIMER *) PTMTIMERGC;
/** Pointer to a GC pointer to a timer. */
typedef PTMTIMERGC *PPTMTIMERGC;

/** Pointer to a timer. */
typedef CTXALLSUFF(PTMTIMER)   PTMTIMER;
/** Pointer to a pointer to a timer. */
typedef CTXALLSUFF(PPTMTIMER)  PPTMTIMER;

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

#pragma pack(1)
/** IDTR */
typedef struct VBOXIDTR
{
    /** Size of the IDT. */
    uint16_t    cbIdt;
    /** Address of the IDT. */
    uint32_t    pIdt;
} VBOXIDTR, *PVBOXIDTR;
#pragma pack()
/** @} */


/** @defgroup grp_types_desc    Descriptor Table Entry.
 * @ingroup grp_types
 * @{ */

#pragma pack(1)
/**
 * Memory descriptor.
 */
typedef struct VBOXDESCGENERIC
{
    /**  0-15 - Limit - Low word. */
    unsigned    u16LimitLow : 16;
    /** 16-31 - Base address - lowe word.
     * Don't try set this to 24 because MSC is doing studing things then. */
    unsigned    u16BaseLow : 16;
    /** 32-39 - Base address - first 8 bits of high word. */
    unsigned    u8BaseHigh1 : 8;
    /** 40-43 - Segment Type. */
    unsigned    u4Type : 4;
    /** 44    - Descriptor Type. System(=0) or code/data selector */
    unsigned    u1DescType : 1;
    /** 45-46 - Descriptor Privelege level. */
    unsigned    u2Dpl : 2;
    /** 47    - Flags selector present(=1) or not. */
    unsigned    u1Present : 1;
    /** 48-51 - Segment limit 16-19. */
    unsigned    u4LimitHigh : 4;
    /** 52    - Available for system software. */
    unsigned    u1Available : 1;
    /** 53    - Reserved - 0. In long mode this is the 'Long' (L) attribute bit. */
    unsigned    u1Reserved : 1;
    /** 54    - This flags meaning depends on the segment type. Try make sense out
     * of the intel manual yourself.  */
    unsigned    u1DefBig : 1;
    /** 55    - Granularity of the limit. If set 4KB granularity is used, if
     * clear byte. */
    unsigned    u1Granularity : 1;
    /** 56-63 - Base address - highest 8 bits. */
    unsigned    u8BaseHigh2 : 8;
} VBOXDESCGENERIC;
#pragma pack()
/** Pointer to a generic descriptor entry. */
typedef VBOXDESCGENERIC *PVBOXDESCGENERIC;

#pragma pack(1)
/**
 * Descriptor table entry.
 */
typedef union VBOXDESC
{
    /** Generic descriptor view. */
    VBOXDESCGENERIC Gen;
    /** IDT view. */
    VBOXIDTE        Idt;

    /** 8 bit unsigned interger view. */
    uint8_t         au8[8];
    /** 16 bit unsigned interger view. */
    uint16_t        au16[4];
    /** 32 bit unsigned interger view. */
    uint32_t        au32[2];
} VBOXDESC;
#pragma pack()
/** Pointer to descriptor table entry. */
typedef VBOXDESC *PVBOXDESC;
/** Pointer to const descriptor table entry. */
typedef VBOXDESC const *PCVBOXDESC;


#pragma pack(1)
/** GDTR */
typedef struct VBOXGDTR
{
    /** Size of the GDT. */
    uint16_t    cbGdt;
    /** Address of the GDT. */
    uint32_t    pGdt;
} VBOXGDTR;
#pragma pack()
/** Pointer to GDTR. */
typedef VBOXGDTR *PVBOXGDTR;

/** @} */


/**
 * Task Segment
 */
#pragma pack(1)
typedef struct VBOXTSS
{
    /** Back link to previous task. (static) */
    RTSEL       selPrev;
    uint16_t    padding1;
    /** Ring-0 stack pointer. (static) */
    uint32_t    esp0;
    /** Ring-0 stack segment. (static) */
    RTSEL       ss0;
    uint16_t    padding_ss0;
    /** Ring-1 stack pointer. (static) */
    uint32_t    esp1;
    /** Ring-1 stack segment. (static) */
    RTSEL       ss1;
    uint16_t    padding_ss1;
    /** Ring-2 stack pointer. (static) */
    uint32_t    esp2;
    /** Ring-2 stack segment. (static) */
    RTSEL       ss2;
    uint16_t    padding_ss2;
    /** Page directory for the task. (static) */
    uint32_t    cr3;
    /** EIP before task switch. */
    uint32_t    eip;
    /** EFLAGS before task switch. */
    uint32_t    eflags;
    /** EAX before task switch. */
    uint32_t    eax;
    /** ECX before task switch. */
    uint32_t    ecx;
    /** EDX before task switch. */
    uint32_t    edx;
    /** EBX before task switch. */
    uint32_t    ebx;
    /** ESP before task switch. */
    uint32_t    esp;
    /** EBP before task switch. */
    uint32_t    ebp;
    /** ESI before task switch. */
    uint32_t    esi;
    /** EDI before task switch. */
    uint32_t    edi;
    /** ES before task switch. */
    RTSEL       es;
    uint16_t    padding_es;
    /** CS before task switch. */
    RTSEL       cs;
    uint16_t    padding_cs;
    /** SS before task switch. */
    RTSEL       ss;
    uint16_t    padding_ss;
    /** DS before task switch. */
    RTSEL       ds;
    uint16_t    padding_ds;
    /** FS before task switch. */
    RTSEL       fs;
    uint16_t    padding_fs;
    /** GS before task switch. */
    RTSEL       gs;
    uint16_t    padding_gs;
    /** LDTR before task switch. */
    RTSEL       selLdt;
    uint16_t    padding_ldt;
    /** Debug trap flag */
    uint16_t    fDebugTrap;
    /** Offset relative to the TSS of the start of the I/O Bitmap
     * and the end of the interrupt redirection bitmap. */
    uint16_t    offIoBitmap;
    /** 32 bytes for the virtual interrupt redirection bitmap. (VME) */
    uint8_t     IntRedirBitmap[32];
} VBOXTSS;
#pragma pack()
/** Pointer to task segment. */
typedef VBOXTSS *PVBOXTSS;
/** Pointer to const task segment. */
typedef const VBOXTSS *PCVBOXTSS;


/** @} */

#endif
