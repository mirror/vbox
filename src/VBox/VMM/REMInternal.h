/* $Id$ */
/** @file
 * REM - Internal header file.
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

#ifndef ___REMInternal_h
#define ___REMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/pgm.h>
#ifdef REM_INCLUDE_CPU_H
# include "target-i386/cpu.h"
#endif


#if !defined(IN_REM_R3) && !defined(IN_REM_R0) && !defined(IN_REM_GC)
# error "Not in REM! This is an internal header!"
#endif

/** @defgroup grp_rem_int   Internals
 * @ingroup grp_rem
 * @internal
 * @{
 */

/** The saved state version number. */
#define REM_SAVED_STATE_VERSION     6


/** @def REM_MONITOR_CODE_PAGES
 * Enable to monitor code pages that have been translated by the recompiler. */
#define REM_MONITOR_CODE_PAGES

typedef enum REMHANDLERNOTIFICATIONKIND
{
    /** The usual invalid 0 entry. */
    REMHANDLERNOTIFICATIONKIND_INVALID = 0,
    /** REMR3NotifyHandlerPhysicalRegister. */
    REMHANDLERNOTIFICATIONKIND_PHYSICAL_REGISTER,
    /** REMR3NotifyHandlerPhysicalDeregister. */
    REMHANDLERNOTIFICATIONKIND_PHYSICAL_DEREGISTER,
    /** REMR3NotifyHandlerPhysicalModify. */
    REMHANDLERNOTIFICATIONKIND_PHYSICAL_MODIFY,
    /** The usual 32-bit hack. */
    REMHANDLERNOTIFICATIONKIND_32BIT_HACK = 0x7fffffff
} REMHANDLERNOTIFICATIONKIND;


/**
 * A recorded handler notificiation.
 */
typedef struct REMHANDLERNOTIFICATION
{
    /** The notification kind. */
    REMHANDLERNOTIFICATIONKIND  enmKind;
    uint32_t                    padding;
    /** Type specific data. */
    union
    {
        struct
        {
            RTGCPHYS            GCPhys;
            RTGCPHYS            cb;
            PGMPHYSHANDLERTYPE  enmType;
            bool                fHasHCHandler;
        } PhysicalRegister;

        struct
        {
            RTGCPHYS            GCPhys;
            RTGCPHYS            cb;
            PGMPHYSHANDLERTYPE  enmType;
            bool                fHasHCHandler;
            bool                fRestoreAsRAM;
        } PhysicalDeregister;

        struct
        {
            RTGCPHYS            GCPhysOld;
            RTGCPHYS            GCPhysNew;
            RTGCPHYS            cb;
            PGMPHYSHANDLERTYPE  enmType;
            bool                fHasHCHandler;
            bool                fRestoreAsRAM;
        } PhysicalModify;
        uint64_t                padding[3 + (HC_ARCH_BITS == 64)];
    } u;
} REMHANDLERNOTIFICATION, *PREMHANDLERNOTIFICATION;

/**
 * Dynamically allocated guest RAM chunk information
 * HC virt to GC Phys
 *
 * A RAM chunk can spawn two chunk regions as we don't align them on chunk boundaries.
 */
typedef struct REMCHUNKINFO
{
    RTHCUINTPTR pChunk1;
    RTHCUINTPTR pChunk2;
    RTGCPHYS    GCPhys1;
    RTGCPHYS    GCPhys2;
} REMCHUNKINFO, *PREMCHUNKINFO;

/** Maximum number of external guest RAM/ROM registrations. */
#define REM_MAX_PHYS_REGISTRATIONS              16

/**
 * Registration record for external guest RAM & ROM
 */
typedef struct REMPHYSREGISTRATION
{
    RTHCUINTPTR     HCVirt;
    RTGCPHYS        GCPhys;
    RTUINT          cb;
} REMPHYSREGISTRATION, *PREMPHYSREGISTRATION;

/**
 * Converts a REM pointer into a VM pointer.
 * @returns Pointer to the VM structure the REM is part of.
 * @param   pREM    Pointer to REM instance data.
 */
#define REM2VM(pREM)  ( (PVM)((char*)pREM - pREM->offVM) )


/**
 * REM Data (part of VM)
 */
typedef struct REM
{
    /** Offset to the VM structure. */
    RTINT                   offVM;
    /** Alignment padding. */
    RTUINT                  uPadding0;

    /** Cached guest cpu context pointer. */
    R3PTRTYPE(PCPUMCTX)     pCtx;

    /** In REM mode.
     * I.e. the correct CPU state and some other bits are with REM. */
    bool                    fInREM;
    /** In REMR3State. */
    bool                    fInStateSync;

    /** Ignore all that can be ignored. */
    bool                    fIgnoreAll;
    /** Ignore CR3 load notifications from the REM. */
    bool                    fIgnoreCR3Load;
    /** Ignore invlpg notifications from the REM. */
    bool                    fIgnoreInvlPg;
    /** Ignore CR0, CR4 and EFER load. */
    bool                    fIgnoreCpuMode;
    /** Ignore set page. */
    bool                    fIgnoreSetPage;

    /** Number of times REMR3CanExecuteRaw has been called.
     * It is used to prevent rescheduling on the first call. */
    RTUINT                  cCanExecuteRaw;

    /** Pending interrupt (~0 -> nothing). */
    RTUINT                  u32PendingInterrupt;

#if HC_ARCH_BITS == 64
    /** Alignment padding. */
    uint32_t                u32Padding;
#endif
    /** Number of recorded invlpg instructions. */
    RTUINT                  cInvalidatedPages;
    /** Array of recorded invlpg instruction.
     * These instructions are replayed when entering REM. */
    RTGCPTR                 aGCPtrInvalidatedPages[48];
    /** The number of recorded handler notifications. */
    RTUINT volatile         cHandlerNotifications;
    RTUINT                  padding0; /**< Padding. */
    /** Array of recorded handler noticications.
     * These are replayed when entering REM. */
    REMHANDLERNOTIFICATION  aHandlerNotifications[32];

    /** Pointer to an array of hc virt to gc phys records. */
    R3PTRTYPE(PREMCHUNKINFO) paHCVirtToGCPhys;
    /** Pointer to a GC Phys to HC Virt lookup table. */
    R3PTRTYPE(PRTHCUINTPTR) paGCPhysToHCVirt;

    /** Array of external RAM and ROM registrations (excluding guest RAM). */
    REMPHYSREGISTRATION     aPhysReg[REM_MAX_PHYS_REGISTRATIONS];
    /** Number of external RAM and ROM registrations (excluding guest RAM). */
    RTUINT                  cPhysRegistrations;

    /** MMIO memory type.
     * This is used to register MMIO physical access handlers. */
    RTINT                   iMMIOMemType;
    /** Handler memory type.
     * This is used to register non-MMIO physical access handlers which are executed in HC. */
    RTINT                   iHandlerMemType;

    /** Pending exception */
    uint32_t                uPendingException;
    /** Pending exception's EIP */
    uint32_t                uPendingExcptEIP;
    /** Pending exception's CR2 */
    uint32_t                uPendingExcptCR2;
    /** Nr of pending exceptions */
    uint32_t                cPendingExceptions;

    /** Pending rc. */
    RTINT                   rc;

    /** Time spent in QEMU. */
    STAMPROFILEADV          StatsInQEMU;
    /** Time spent in rawmode.c. */
    STAMPROFILEADV          StatsInRAWEx;
    /** Time spent switching state. */
    STAMPROFILE             StatsState;
    /** Time spent switching state back. */
    STAMPROFILE             StatsStateBack;

#if HC_ARCH_BITS != 32
    /** Padding the CPUX86State structure to 32 byte. */
    uint32_t                abPadding[HC_ARCH_BITS == 32 ? 0 : 4];
#endif

#define REM_ENV_SIZE        (HC_ARCH_BITS == 32 ? 0x6440 : 0xb4a0)
    /** Recompiler CPU state. */
#ifdef REM_INCLUDE_CPU_H
    CPUX86State             Env;
#else
    struct FakeEnv
    {
        char                achPadding[REM_ENV_SIZE];
    }                       Env;
#endif
} REM;

/** Pointer to the REM Data. */
typedef REM *PREM;


#ifdef REM_INCLUDE_CPU_H
bool    remR3CanExecuteRaw(CPUState *env, RTGCPTR eip, unsigned fFlags, int *piException);
void    remR3CSAMCheckEIP(CPUState *env, RTGCPTR GCPtrCode);
bool    remR3GetOpcode(CPUState *env, RTGCPTR GCPtrInstr, uint8_t *pu8Byte);
bool    remR3DisasInstr(CPUState *env, int f32BitCode, char *pszPrefix);
bool    remR3DisasBlock(CPUState *env, int f32BitCode, int nrInstructions, char *pszPrefix);
void    remR3FlushPage(CPUState *env, RTGCPTR GCPtr);
void    remR3SetPage(CPUState *env, CPUTLBEntry *pRead,  CPUTLBEntry *pWrite, int prot, int is_user);
void    remR3FlushTLB(CPUState *env, bool fGlobal);
void    remR3ProtectCode(CPUState *env, RTGCPTR GCPtr);
void    remR3ChangeCpuMode(CPUState *env);
void    remR3DmaRun(CPUState *env);
void    remR3TimersRun(CPUState *env);
int     remR3NotifyTrap(CPUState *env, uint32_t uTrap, uint32_t uErrorCode, uint32_t pvNextEIP);
void    remR3TrapStat(CPUState *env, uint32_t uTrap);
void    remR3CpuId(CPUState *env, unsigned uOperator, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX);
void    remR3RecordCall(CPUState *env);
#endif
void    remR3TrapClear(PVM pVM);
void    remR3RaiseRC(PVM pVM, int rc);
void    remR3DumpLnxSyscall(PVM pVM);
void    remR3DumpOBsdSyscall(PVM pVM);


/** @todo r=bird: clean up the RAWEx stats. */
/* temporary hacks */
#define RAWEx_ProfileStart(a, b)    remR3ProfileStart(b)
#define RAWEx_ProfileStop(a, b)     remR3ProfileStop(b)


#ifdef VBOX_WITH_STATISTICS

#define STATS_EMULATE_SINGLE_INSTR          1
#define STATS_QEMU_COMPILATION              2
#define STATS_QEMU_RUN_EMULATED_CODE        3
#define STATS_QEMU_TOTAL                    4
#define STATS_QEMU_RUN_TIMERS               5
#define STATS_TLB_LOOKUP                    6
#define STATS_IRQ_HANDLING                  7
#define STATS_RAW_CHECK                     8


void remR3ProfileStart(int statcode);
void remR3ProfileStop(int statcode);
#else
#define remR3ProfileStart(c)
#define remR3ProfileStop(c)
#endif

/** @} */

#endif

