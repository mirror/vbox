/* $Id$ */
/** @file
 * PATM - Internal header file.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___PATMInternal_h
#define ___PATMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/patm.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <iprt/avl.h>
#include <iprt/param.h>
#include <VBox/log.h>

#if !defined(IN_PATM_R3) && !defined(IN_PATM_R0) && !defined(IN_PATM_GC)
# error "Not in PATM! This is an internal header!"
#endif


#define PATM_SSM_VERSION                    53

/* Enable for call patching. */
#define PATM_ENABLE_CALL
#define PATCH_MEMORY_SIZE                  (2*1024*1024)
#define MAX_PATCH_SIZE                     (1024*4)

/*
 * Internal patch type flags (starts at RT_BIT(11))
 */

#define PATMFL_CHECK_SIZE                   RT_BIT_64(11)
#define PATMFL_FOUND_PATCHEND               RT_BIT_64(12)
#define PATMFL_SINGLE_INSTRUCTION           RT_BIT_64(13)
#define PATMFL_SYSENTER_XP                  RT_BIT_64(14)
#define PATMFL_JUMP_CONFLICT                RT_BIT_64(15)
#define PATMFL_READ_ORIGINAL_BYTES          RT_BIT_64(16) /** opcode might have already been patched */
#define PATMFL_INT3_REPLACEMENT             RT_BIT_64(17)
#define PATMFL_SUPPORT_CALLS                RT_BIT_64(18)
#define PATMFL_SUPPORT_INDIRECT_CALLS       RT_BIT_64(19)
#define PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT RT_BIT_64(20) /** internal flag to avoid duplicate entrypoints */
#define PATMFL_INHIBIT_IRQS                 RT_BIT_64(21) /** temporary internal flag */
#define PATMFL_GENERATE_JUMPTOGUEST         RT_BIT_64(22) /** temporary internal flag */
#define PATMFL_RECOMPILE_NEXT               RT_BIT_64(23) /** for recompilation of the next instruction */
#define PATMFL_CODE_MONITORED               RT_BIT_64(24) /** code pages of guest monitored for self-modifying code. */
#define PATMFL_CALLABLE_AS_FUNCTION         RT_BIT_64(25) /** cli and pushf blocks can be used as callable functions. */
#define PATMFL_GLOBAL_FUNCTIONS             RT_BIT_64(26) /** fake patch for global patm functions. */
#define PATMFL_TRAMPOLINE                   RT_BIT_64(27) /** trampoline patch that clears PATM_INTERRUPTFLAG and jumps to patch destination */
#define PATMFL_GENERATE_SETPIF              RT_BIT_64(28) /** generate set PIF for the next instruction */
#define PATMFL_INSTR_HINT                   RT_BIT_64(29) /** Generate patch, but don't activate it. */
#define PATMFL_PATCHED_GUEST_CODE           RT_BIT_64(30) /** Patched guest code. */
#define PATMFL_MUST_INSTALL_PATCHJMP        RT_BIT_64(31) /** Need to patch guest code in order to activate patch. */
#define PATMFL_INT3_REPLACEMENT_BLOCK       RT_BIT_64(32) /** int 3 replacement block */
#define PATMFL_EXTERNAL_JUMP_INSIDE         RT_BIT_64(33) /** A trampoline patch was created that jumps to an instruction in the patch block */

#define SIZEOF_NEARJUMP8                   2 //opcode byte + 1 byte relative offset
#define SIZEOF_NEARJUMP16                  3 //opcode byte + 2 byte relative offset
#define SIZEOF_NEARJUMP32                  5 //opcode byte + 4 byte relative offset
#define SIZEOF_NEAR_COND_JUMP32            6 //0xF + opcode byte + 4 byte relative offset

#define MAX_INSTR_SIZE                     16

//Patch states
#define PATCH_REFUSED                     1
#define PATCH_DISABLED                    2
#define PATCH_ENABLED                     4
#define PATCH_UNUSABLE                    8
#define PATCH_DIRTY                       16
#define PATCH_DISABLE_PENDING             32


#define MAX_PATCH_TRAPS                    4
#define PATM_MAX_CALL_DEPTH                32
/* Maximum nr of writes before a patch is marked dirty. (disabled) */
#define PATM_MAX_CODE_WRITES               32
/* Maximum nr of invalid writes before a patch is disabled. */
#define PATM_MAX_INVALID_WRITES            16384

#define FIXUP_ABSOLUTE                     0
#define FIXUP_REL_JMPTOPATCH               1
#define FIXUP_REL_JMPTOGUEST               2

#define PATM_ILLEGAL_DESTINATION           0xDEADBEEF

/** Size of the instruction that's used for requests from patch code (currently only call) */
#define PATM_ILLEGAL_INSTR_SIZE            2


/** No statistics counter index allocated just yet */
#define PATM_STAT_INDEX_NONE                (uint32_t)-1
/** Dummy counter to handle overflows */
#define PATM_STAT_INDEX_DUMMY               0
#define PATM_STAT_INDEX_IS_VALID(a)         (a != PATM_STAT_INDEX_DUMMY && a != PATM_STAT_INDEX_NONE)

#ifdef VBOX_WITH_STATISTICS
#define PATM_STAT_RUN_INC(pPatch)                                             \
        if (PATM_STAT_INDEX_IS_VALID((pPatch)->uPatchIdx))                \
            CTXSUFF(pVM->patm.s.pStats)[(pPatch)->uPatchIdx].u32A++;
#define PATM_STAT_FAULT_INC(pPatch)                                           \
        if (PATM_STAT_INDEX_IS_VALID((pPatch)->uPatchIdx))                \
            CTXSUFF(pVM->patm.s.pStats)[(pPatch)->uPatchIdx].u32B++;
#else
#define PATM_STAT_RUN_INC(pPatch)           do { } while (0)
#define PATM_STAT_FAULT_INC(pPatch)         do { } while (0)
#endif

/** Maximum number of stat counters. */
#define PATM_STAT_MAX_COUNTERS              1024
/** Size of memory allocated for patch statistics. */
#define PATM_STAT_MEMSIZE                   (PATM_STAT_MAX_COUNTERS*sizeof(STAMRATIOU32))


typedef struct
{
    /** The key is a HC virtual address. */
    AVLPVNODECORE   Core;

    uint32_t        uType;
    R3PTRTYPE(uint8_t *) pRelocPos;
    RTGCPTR32       pSource;
    RTGCPTR32       pDest;
} RELOCREC, *PRELOCREC;

typedef struct
{
    R3PTRTYPE(uint8_t *) pPatchLocStartHC;
    R3PTRTYPE(uint8_t *) pPatchLocEndHC;
    RCPTRTYPE(uint8_t *) pGuestLoc;
    uint32_t             opsize;
} P2GLOOKUPREC, *PP2GLOOKUPREC;

typedef struct
{
    /** The key is a pointer to a JUMPREC structure. */
    AVLPVNODECORE   Core;

    R3PTRTYPE(uint8_t *) pJumpHC;
    RCPTRTYPE(uint8_t *) pTargetGC;
    uint32_t            offDispl;
    uint32_t            opcode;
} JUMPREC, *PJUMPREC;

/**
 * Patch to guest lookup type (single or both direction)
 */
typedef enum
{
    PATM_LOOKUP_PATCH2GUEST,    /* patch to guest */
    PATM_LOOKUP_BOTHDIR         /* guest to patch + patch to guest */
} PATM_LOOKUP_TYPE;

/**
 * Patch to guest address lookup record
 */
typedef struct RECPATCHTOGUEST
{
    /** The key is an offset inside the patch memory block. */
    AVLU32NODECORE   Core;

    RTGCPTR32        pOrgInstrGC;
    PATM_LOOKUP_TYPE enmType;
    bool             fDirty;
    bool             fJumpTarget;
    uint8_t          u8DirtyOpcode;  /* original opcode before writing 0xCC there to mark it dirty */
} RECPATCHTOGUEST, *PRECPATCHTOGUEST;

/**
 * Guest to patch address lookup record
 */
typedef struct RECGUESTTOPATCH
{
    /** The key is a GC virtual address. */
    AVLGCPTRNODECORE    Core;

    /** Patch offset (relative to PATM::pPatchMemGC / PATM::pPatchMemHC). */
    uint32_t            PatchOffset;
} RECGUESTTOPATCH, *PRECGUESTTOPATCH;

/**
 * Temporary information used in ring 3 only; no need to waste memory in the patch record itself.
 */
typedef struct
{
    /* Temporary tree for storing the addresses of illegal instructions. */
    R3PTRTYPE(PAVLPVNODECORE)   IllegalInstrTree;
    uint32_t                    nrIllegalInstr;

    int32_t                     nrJumps;
    uint32_t                    nrRetInstr;

    /* Temporary tree of encountered jumps. (debug only) */
    R3PTRTYPE(PAVLPVNODECORE)   DisasmJumpTree;

    int32_t                     nrCalls;

    /** Last original guest instruction pointer; used for disassmebly log. */
    RTGCPTR32                   pLastDisasmInstrGC;

    /** Keeping track of multiple ret instructions. */
    RTGCPTR32                 pPatchRetInstrGC;
    uint32_t                    uPatchRetParam1;
} PATCHINFOTEMP, *PPATCHINFOTEMP;

typedef struct _PATCHINFO
{
    uint32_t        uState;
    uint32_t        uOldState;
    DISCPUMODE      uOpMode;

    RCPTRTYPE(uint8_t *)  pPrivInstrGC;    //GC pointer of privileged instruction
    R3PTRTYPE(uint8_t *)  pPrivInstrHC;    //HC pointer of privileged instruction
    uint8_t         aPrivInstr[MAX_INSTR_SIZE];
    uint32_t        cbPrivInstr;
    uint32_t        opcode;      //opcode for priv instr (OP_*)
    uint32_t        cbPatchJump; //patch jump size

    /* Only valid for PATMFL_JUMP_CONFLICT patches */
    RTGCPTR32       pPatchJumpDestGC;

    RTGCUINTPTR     pPatchBlockOffset;
    uint32_t        cbPatchBlockSize;
    uint32_t        uCurPatchOffset;
#if HC_ARCH_BITS == 64
    uint32_t        Alignment0;         /**< Align flags correctly. */
#endif

    uint64_t        flags;

    /**
     * Lowest and highest patched GC instruction address. To optimize searches.
     */
    RTGCPTR32                 pInstrGCLowest;
    RTGCPTR32                 pInstrGCHighest;

    /* Tree of fixup records for the patch. */
    R3PTRTYPE(PAVLPVNODECORE) FixupTree;
    int32_t         nrFixups;

    /* Tree of jumps inside the generated patch code. */
    int32_t         nrJumpRecs;
    R3PTRTYPE(PAVLPVNODECORE) JumpTree;

    /**
     * Lookup trees for determining the corresponding guest address of an
     * instruction in the patch block.
     */
    R3PTRTYPE(PAVLU32NODECORE) Patch2GuestAddrTree;
    R3PTRTYPE(PAVLGCPTRNODECORE) Guest2PatchAddrTree;
    uint32_t                  nrPatch2GuestRecs;
#if HC_ARCH_BITS == 64
    uint32_t        Alignment1;
#endif

    // Cache record for PATMGCVirtToHCVirt
    P2GLOOKUPREC    cacheRec;

    /* Temporary information during patch creation. Don't waste hypervisor memory for this. */
    R3PTRTYPE(PPATCHINFOTEMP) pTempInfo;

    /* Count the number of writes to the corresponding guest code. */
    uint32_t        cCodeWrites;

    /* Count the number of invalid writes to pages monitored for the patch. */
    //some statistics to determine if we should keep this patch activated
    uint32_t        cTraps;

    uint32_t        cInvalidWrites;

    // Index into the uPatchRun and uPatchTrap arrays (0..MAX_PATCHES-1)
    uint32_t        uPatchIdx;

    /* First opcode byte, that's overwritten when a patch is marked dirty. */
    uint8_t         bDirtyOpcode;
    uint8_t         Alignment2[7];      /**< Align the structure size on a 8-byte boundrary. */
} PATCHINFO, *PPATCHINFO;

#define PATCHCODE_PTR_GC(pPatch)    (RTGCPTR)  (pVM->patm.s.pPatchMemGC + (pPatch)->pPatchBlockOffset)
#define PATCHCODE_PTR_HC(pPatch)    (uint8_t *)(pVM->patm.s.pPatchMemHC + (pPatch)->pPatchBlockOffset)

/**
 * Lookup record for patches
 */
typedef struct PATMPATCHREC
{
    /** The key is a GC virtual address. */
    AVLOGCPTRNODECORE  Core;
    /** The key is a patch offset. */
    AVLOGCPTRNODECORE  CoreOffset;

    PATCHINFO  patch;
} PATMPATCHREC, *PPATMPATCHREC;

/** Increment for allocating room for pointer array */
#define PATMPATCHPAGE_PREALLOC_INCREMENT        16

/**
 * Lookup record for patch pages
 */
typedef struct PATMPATCHPAGE
{
    /** The key is a GC virtual address. */
    AVLOGCPTRNODECORE  Core;
    /** Region to monitor. */
    RTGCPTR32          pLowestAddrGC;
    RTGCPTR32          pHighestAddrGC;
    /** Number of patches for this page. */
    uint32_t           cCount;
    /** Maximum nr of pointers in the array. */
    uint32_t           cMaxPatches;
    /** Array of patch pointers for this page. */
    R3PTRTYPE(PPATCHINFO *) aPatch;
} PATMPATCHPAGE, *PPATMPATCHPAGE;

#define PATM_PATCHREC_FROM_COREOFFSET(a)  (PPATMPATCHREC)((uintptr_t)a - RT_OFFSETOF(PATMPATCHREC, CoreOffset))
#define PATM_PATCHREC_FROM_PATCHINFO(a)   (PPATMPATCHREC)((uintptr_t)a - RT_OFFSETOF(PATMPATCHREC, patch))

typedef struct PATMTREES
{
    /**
     * AVL tree with all patches (active or disabled) sorted by guest instruction address
     */
    AVLOGCPTRTREE           PatchTree;

    /**
     * AVL tree with all patches sorted by patch address (offset actually)
     */
    AVLOGCPTRTREE           PatchTreeByPatchAddr;

    /**
     * AVL tree with all pages which were (partly) patched
     */
    AVLOGCPTRTREE           PatchTreeByPage;

    uint32_t                align[1];
} PATMTREES, *PPATMTREES;

/**
 * PATM VM Instance data.
 * Changes to this must checked against the padding of the patm union in VM!
 */
typedef struct PATM
{
    /** Offset to the VM structure.
     * See PATM2VM(). */
    RTINT                   offVM;

    RCPTRTYPE(uint8_t *)    pPatchMemGC;
    R3PTRTYPE(uint8_t *)    pPatchMemHC;
    uint32_t                cbPatchMem;
    uint32_t                offPatchMem;
    bool                    fOutOfMemory;

    int32_t                 deltaReloc;

    /* GC PATM state pointers */
    R3PTRTYPE(PPATMGCSTATE) pGCStateHC;
    RCPTRTYPE(PPATMGCSTATE) pGCStateGC;

    /** PATM stack page for call instruction execution. (2 parts: one for our private stack and one to store the original return address */
    RCPTRTYPE(RTGCPTR32 *)    pGCStackGC;
    R3PTRTYPE(RTGCPTR32 *)    pGCStackHC;

    /** GC pointer to CPUMCTX structure. */
    RCPTRTYPE(PCPUMCTX)     pCPUMCtxGC;

    /* GC statistics pointers */
    RCPTRTYPE(PSTAMRATIOU32) pStatsGC;
    R3PTRTYPE(PSTAMRATIOU32) pStatsHC;

    /* Current free index value (uPatchRun/uPatchTrap arrays). */
    uint32_t                uCurrentPatchIdx;

    /* Temporary counter for patch installation call depth. (in order not to go on forever) */
    uint32_t                ulCallDepth;

    /** Number of page lookup records. */
    uint32_t                cPageRecords;

    /**
     * Lowest and highest patched GC instruction addresses. To optimize searches.
     */
    RTGCPTR32                 pPatchedInstrGCLowest;
    RTGCPTR32                 pPatchedInstrGCHighest;

    /** Pointer to the patch tree for instructions replaced by 'int 3'. */
    RCPTRTYPE(PPATMTREES)   PatchLookupTreeGC;
    R3PTRTYPE(PPATMTREES)   PatchLookupTreeHC;

    /** Global PATM lookup and call function (used by call patches). */
    RTGCPTR32               pfnHelperCallGC;
    /** Global PATM return function (used by ret patches). */
    RTGCPTR32               pfnHelperRetGC;
    /** Global PATM jump function (used by indirect jmp patches). */
    RTGCPTR32               pfnHelperJumpGC;
    /** Global PATM return function (used by iret patches). */
    RTGCPTR32               pfnHelperIretGC;

    /** Fake patch record for global functions. */
    R3PTRTYPE(PPATMPATCHREC) pGlobalPatchRec;

    /** Pointer to original sysenter handler */
    RTGCPTR32               pfnSysEnterGC;
    /** Pointer to sysenter handler trampoline */
    RTGCPTR32               pfnSysEnterPatchGC;
    /** Sysenter patch index (for stats only) */
    uint32_t                uSysEnterPatchIdx;

    // GC address of fault in monitored page (set by PATMGCMonitorPage, used by PATMR3HandleMonitoredPage)
    RTGCPTR32               pvFaultMonitor;

    /* Temporary information for pending MMIO patch. Set in GC or R0 context. */
    struct
    {
        RTGCPHYS            GCPhys;
        RTGCPTR32           pCachedData;
#if GC_ARCH_BITS == 32
        RTGCPTR32           Alignment0; /**< Align the structure size on a 8-byte boundrary. */
#endif
    } mmio;

    /* Temporary storage during load/save state */
    struct
    {
        R3PTRTYPE(PSSMHANDLE) pSSM;
        uint32_t            cPatches;
#if HC_ARCH_BITS == 64
        uint32_t            Alignment0; /**< Align the structure size on a 8-byte boundrary. */
#endif
    } savedstate;

    STAMCOUNTER             StatNrOpcodeRead;
    STAMCOUNTER             StatDisabled;
    STAMCOUNTER             StatUnusable;
    STAMCOUNTER             StatEnabled;
    STAMCOUNTER             StatInstalled;
    STAMCOUNTER             StatInstalledFunctionPatches;
    STAMCOUNTER             StatInstalledTrampoline;
    STAMCOUNTER             StatInstalledJump;
    STAMCOUNTER             StatInt3Callable;
    STAMCOUNTER             StatInt3BlockRun;
    STAMCOUNTER             StatOverwritten;
    STAMCOUNTER             StatFixedConflicts;
    STAMCOUNTER             StatFlushed;
    STAMCOUNTER             StatPageBoundaryCrossed;
    STAMCOUNTER             StatMonitored;
    STAMPROFILEADV          StatHandleTrap;
    STAMCOUNTER             StatSwitchBack;
    STAMCOUNTER             StatSwitchBackFail;
    STAMCOUNTER             StatPATMMemoryUsed;
    STAMCOUNTER             StatDuplicateREQSuccess;
    STAMCOUNTER             StatDuplicateREQFailed;
    STAMCOUNTER             StatDuplicateUseExisting;
    STAMCOUNTER             StatFunctionFound;
    STAMCOUNTER             StatFunctionNotFound;
    STAMPROFILEADV          StatPatchWrite;
    STAMPROFILEADV          StatPatchWriteDetect;
    STAMCOUNTER             StatDirty;
    STAMCOUNTER             StatPushTrap;
    STAMCOUNTER             StatPatchWriteInterpreted;
    STAMCOUNTER             StatPatchWriteInterpretedFailed;

    STAMCOUNTER             StatSysEnter;
    STAMCOUNTER             StatSysExit;
    STAMCOUNTER             StatEmulIret;
    STAMCOUNTER             StatEmulIretFailed;

    STAMCOUNTER             StatInstrDirty;
    STAMCOUNTER             StatInstrDirtyGood;
    STAMCOUNTER             StatInstrDirtyBad;

    STAMCOUNTER             StatPatchPageInserted;
    STAMCOUNTER             StatPatchPageRemoved;

    STAMCOUNTER             StatPatchRefreshSuccess;
    STAMCOUNTER             StatPatchRefreshFailed;

    STAMCOUNTER             StatGenRet;
    STAMCOUNTER             StatGenRetReused;
    STAMCOUNTER             StatGenJump;
    STAMCOUNTER             StatGenCall;
    STAMCOUNTER             StatGenPopf;

    STAMCOUNTER             StatCheckPendingIRQ;

    STAMCOUNTER             StatFunctionLookupReplace;
    STAMCOUNTER             StatFunctionLookupInsert;
    uint32_t                StatU32FunctionMaxSlotsUsed;
    uint32_t                Alignment0; /**< Align the structure size on a 8-byte boundrary. */
} PATM, *PPATM;


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) patmr3Save(PVM pVM, PSSMHANDLE pSSM);


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
DECLCALLBACK(int) patmr3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);

#ifdef IN_RING3
RTGCPTR patmPatchGCPtr2GuestGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pPatchGC);
RTGCPTR patmGuestGCPtrToPatchGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t*) pInstrGC);
RTGCPTR patmGuestGCPtrToClosestPatchGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t*) pInstrGC);
#endif

/* Add a patch to guest lookup record
 *
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure ptr
 * @param   pPatchInstrHC   Guest context pointer to patch block
 * @param   pInstrGC        Guest context pointer to privileged instruction
 * @param   enmType         Lookup type
 * @param   fDirty          Dirty flag
 *
 */
void patmr3AddP2GLookupRecord(PVM pVM, PPATCHINFO pPatch, uint8_t *pPatchInstrHC, RTGCPTR pInstrGC, PATM_LOOKUP_TYPE enmType, bool fDirty=false);

/**
 * Insert page records for all guest pages that contain instructions that were recompiled for this patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
int patmInsertPatchPages(PVM pVM, PPATCHINFO pPatch);

/**
 * Remove page records for all guest pages that contain instructions that were recompiled for this patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
int patmRemovePatchPages(PVM pVM, PPATCHINFO pPatch);

/**
 * Returns the GC address of the corresponding patch statistics counter
 *
 * @returns Stat address
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 */
RTGCPTR patmPatchQueryStatAddress(PVM pVM, PPATCHINFO pPatch);

/**
 * Remove patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   pPatchRec       Patch record
 * @param   fForceRemove    Remove *all* patches
 */
int PATMRemovePatch(PVM pVM, PPATMPATCHREC pPatchRec, bool fForceRemove);

/**
 * Call for analysing the instructions following the privileged instr. for compliance with our heuristics
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrHC    Guest context pointer to privileged instruction
 * @param   pCurInstrHC Guest context pointer to current instruction
 * @param   pUserData   User pointer
 *
 */
typedef int (VBOXCALL *PFN_PATMR3ANALYSE)(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData);

/**
 * Install guest OS specific patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on
 * @param   pCpu        Disassembly state of instruction.
 * @param   pInstrGC    GC Instruction pointer for instruction
 * @param   pInstrHC    GC Instruction pointer for instruction
 * @param   pPatchRec   Patch structure
 *
 */
int PATMInstallGuestSpecificPatch(PVM pVM, PDISCPUSTATE pCpu, RTGCPTR pInstrGC, uint8_t *pInstrHC, PPATMPATCHREC pPatchRec);

/**
 * Convert guest context address to host context pointer
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch block structure pointer
 * @param   pGCPtr      Guest context pointer
 *
 * @returns             Host context pointer or NULL in case of an error
 *
 */
R3PTRTYPE(uint8_t *) PATMGCVirtToHCVirt(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pGCPtr);


/**
 * Check if the instruction is patched as a duplicated function
 *
 * @returns patch record
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to the instruction
 *
 */
PATMDECL(PPATMPATCHREC) PATMQueryFunctionPatch(PVM pVM, RTGCPTR pInstrGC);


/**
 * Empty the specified tree (PV tree, MMR3 heap)
 *
 * @param   pVM             The VM to operate on.
 * @param   ppTree          Tree to empty
 */
void patmEmptyTree(PVM pVM, PPAVLPVNODECORE ppTree);


/**
 * Empty the specified tree (U32 tree, MMR3 heap)
 *
 * @param   pVM             The VM to operate on.
 * @param   ppTree          Tree to empty
 */
void patmEmptyTreeU32(PVM pVM, PPAVLU32NODECORE ppTree);


/**
 * Return the name of the patched instruction
 *
 * @returns instruction name
 *
 * @param   opcode      DIS instruction opcode
 * @param   fPatchFlags Patch flags
 */
PATMDECL(const char *) patmGetInstructionString(uint32_t opcode, uint32_t fPatchFlags);


/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   size        Number of bytes to read
 * @param   pvUserdata  Callback specific user data (pCpu)
 *
 */
int patmReadBytes(RTUINTPTR pSrc, uint8_t *pDest, unsigned size, void *pvUserdata);


#ifndef IN_GC

#define PATMREAD_RAWCODE        1  /* read code as-is */
#define PATMREAD_ORGCODE        2  /* read original guest opcode bytes; not the patched bytes */
#define PATMREAD_NOCHECK        4  /* don't check for patch conflicts */

/*
 * Private structure used during disassembly
 */
typedef struct
{
    PVM           pVM;
    PPATCHINFO    pPatchInfo;
    R3PTRTYPE(uint8_t *) pInstrHC;
    RTGCPTR       pInstrGC;
    uint32_t      fReadFlags;
} PATMDISASM, *PPATMDISASM;

inline bool PATMR3DISInstr(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR InstrGC,
                           uint8_t *InstrHC, uint32_t *pOpsize, char *pszOutput,
                           uint32_t fReadFlags = PATMREAD_ORGCODE)
{
    PATMDISASM disinfo;
    disinfo.pVM    = pVM;
    disinfo.pPatchInfo = pPatch;
    disinfo.pInstrHC = InstrHC;
    disinfo.pInstrGC = InstrGC;
    disinfo.fReadFlags = fReadFlags;
    (pCpu)->pfnReadBytes  = patmReadBytes;
    (pCpu)->apvUserData[0] = &disinfo;
    return VBOX_SUCCESS(DISInstr(pCpu, InstrGC, 0, pOpsize, pszOutput));
}
#endif /* !IN_GC */

__BEGIN_DECLS
/**
 * #PF Virtual Handler callback for Guest access a page monitored by PATM
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
PATMGCDECL(int) PATMGCMonitorPage(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);

/**
 * Find patch for privileged instruction at specified location
 *
 * @returns Patch structure pointer if found; else NULL
 * @param   pVM           The VM to operate on.
 * @param   pInstr        Guest context point to instruction that might lie within 5 bytes of an existing patch jump
 * @param   fIncludeHints Include hinted patches or not
 *
 */
PPATCHINFO PATMFindActivePatchByEntrypoint(PVM pVM, RTGCPTR pInstrGC, bool fIncludeHints=false);

/**
 * Patch cli/sti pushf/popf instruction block at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   uOpcode     Instruction opcodee
 * @param   uOpSize     Size of starting instruction
 * @param   pPatchRec   Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
PATMR3DECL(int) PATMR3PatchBlock(PVM pVM, RTGCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC,
                                 uint32_t uOpcode, uint32_t uOpSize, PPATMPATCHREC pPatchRec);


/**
 * Replace an instruction with a breakpoint (0xCC), that is handled dynamically in the guest context.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   pCpu        Disassembly CPU structure ptr
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
PATMR3DECL(int) PATMR3PatchInstrInt3(PVM pVM, RTGCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC, DISCPUSTATE *pCpu, PPATCHINFO pPatch);

/**
 * Mark patch as dirty
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
PATMR3DECL(int) PATMR3MarkDirtyPatch(PVM pVM, PPATCHINFO pPatch);

/**
 * Calculate the branch destination
 *
 * @returns branch destination or 0 if failed
 * @param   pCpu            Disassembly state of instruction.
 * @param   pBranchInstrGC  GC pointer of branch instruction
 */
inline RTGCPTR PATMResolveBranch(PDISCPUSTATE pCpu, RTGCPTR pBranchInstrGC)
{
    uint32_t disp;
    if (pCpu->param1.flags & USE_IMMEDIATE8_REL)
    {
        disp = (int32_t)(char)pCpu->param1.parval;
    }
    else
    if (pCpu->param1.flags & USE_IMMEDIATE16_REL)
    {
        disp = (int32_t)(uint16_t)pCpu->param1.parval;
    }
    else
    if (pCpu->param1.flags & USE_IMMEDIATE32_REL)
    {
        disp = (int32_t)pCpu->param1.parval;
    }
    else
    {
        Log(("We don't support far jumps here!! (%08X)\n", pCpu->param1.flags));
        return 0;
    }
#ifdef IN_GC
    return (RTGCPTR)((uint8_t *)pBranchInstrGC + pCpu->opsize + disp);
#else
    return pBranchInstrGC + pCpu->opsize + disp;
#endif
}

__END_DECLS

#ifdef DEBUG
int patmr3DisasmCallback(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData);
int patmr3DisasmCodeStream(PVM pVM, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, PFN_PATMR3ANALYSE pfnPATMR3Analyse, void *pUserData);
#endif

#endif
