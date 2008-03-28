/* $Id$ */
/** @file
 * IOM - Internal header file.
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

#ifndef ___IOMInternal_h
#define ___IOMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/iom.h>
#include <VBox/stam.h>
#include <iprt/avl.h>

#if !defined(IN_IOM_R3) && !defined(IN_IOM_R0) && !defined(IN_IOM_GC)
# error "Not in IOM! This is an internal header!"
#endif


/** @defgroup grp_iom_int   Internals
 * @ingroup grp_iom
 * @internal
 * @{
 */

/**
 * MMIO range descriptor, R3 version.
 */
typedef struct IOMMMIORANGER3
{
    /** Avl node core with GCPhys as Key and GCPhys + cbSize - 1 as KeyLast. */
    AVLROGCPHYSNODECORE         Core;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32 && !defined(RT_OS_WINDOWS)
    uint32_t                    u32Alignment; /**< The sizeof(Core) differs. */
#endif
    /** Start physical address. */
    RTGCPHYS                    GCPhys;
    /** Size of the range. */
    RTUINT                      cbSize;
    /** Pointer to user argument. */
    RTR3PTR                     pvUser;
    /** Pointer to device instance. */
    R3PTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to write callback function. */
    R3PTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallback;
    /** Pointer to read callback function. */
    R3PTRTYPE(PFNIOMMMIOREAD)   pfnReadCallback;
    /** Pointer to fill (memset) callback function. */
    R3PTRTYPE(PFNIOMMMIOFILL)   pfnFillCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMMMIORANGER3;
/** Pointer to a MMIO range descriptor, R3 version. */
typedef struct IOMMMIORANGER3 *PIOMMMIORANGER3;

/**
 * MMIO range descriptor, R0 version.
 */
typedef struct IOMMMIORANGER0
{
    /** Avl node core with GCPhys as Key and GCPhys + cbSize - 1 as KeyLast. */
    AVLROGCPHYSNODECORE         Core;
    /** Start physical address. */
    RTGCPHYS                    GCPhys;
#if HC_ARCH_BITS == 64
    uint32_t                    u32Alignment;
#endif
    /** Size of the range. */
    RTUINT                      cbSize;
    /** Pointer to user argument. */
    RTR0PTR                     pvUser;
    /** Pointer to device instance. */
    R0PTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to write callback function. */
    R0PTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallback;
    /** Pointer to read callback function. */
    R0PTRTYPE(PFNIOMMMIOREAD)   pfnReadCallback;
    /** Pointer to fill (memset) callback function. */
    R0PTRTYPE(PFNIOMMMIOFILL)   pfnFillCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMMMIORANGER0;
/** Pointer to a MMIO range descriptor, R0 version. */
typedef struct IOMMMIORANGER0 *PIOMMMIORANGER0;

/**
 * MMIO range descriptor, GC version.
 */
typedef struct IOMMMIORANGEGC
{
    /** Avl node core with GCPhys as Key and GCPhys + cbSize - 1 as KeyLast. */
    AVLROGCPHYSNODECORE         Core;
    /** Start physical address. */
    RTGCPHYS                    GCPhys;
    /** Size of the range. */
    RTUINT                      cbSize;
#if HC_ARCH_BITS == 64
    uint32_t                    u32Alignment;
#endif
    /** Pointer to user argument. */
    RTGCPTR                     pvUser;
    /** Pointer to device instance. */
    GCPTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to write callback function. */
    GCPTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallback;
    /** Pointer to read callback function. */
    GCPTRTYPE(PFNIOMMMIOREAD)   pfnReadCallback;
    /** Pointer to fill (memset) callback function. */
    GCPTRTYPE(PFNIOMMMIOFILL)   pfnFillCallback;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    RTGCPTR                     GCPtrAlignment; /**< pszDesc is 8 byte aligned. */
#endif 
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMMMIORANGEGC;
/** Pointer to a MMIO range descriptor, GC version. */
typedef struct IOMMMIORANGEGC *PIOMMMIORANGEGC;


/**
 * MMIO address statistics. (one address)
 *
 * This is a simple way of making on demand statistics, however it's a
 * bit free with the hypervisor heap memory..
 */
typedef struct IOMMMIOSTATS
{
    /** Avl node core with the address as Key. */
    AVLOGCPHYSNODECORE          Core;
    /** Number of reads to this address from R3. */
    STAMCOUNTER                 ReadR3;
    /** Number of writes to this address from R3. */
    STAMCOUNTER                 WriteR3;
    /** Number of reads to this address from R0. */
    STAMCOUNTER                 ReadR0;
    /** Number of writes to this address from R0. */
    STAMCOUNTER                 WriteR0;
    /** Number of reads to this address from GC. */
    STAMCOUNTER                 ReadGC;
    /** Number of writes to this address from GC. */
    STAMCOUNTER                 WriteGC;
    /** Profiling read handler overhead in R3. */
    STAMPROFILEADV              ProfReadR3;
    /** Profiling write handler overhead in R3. */
    STAMPROFILEADV              ProfWriteR3;
    /** Profiling read handler overhead in R0. */
    STAMPROFILEADV              ProfReadR0;
    /** Profiling write handler overhead in R0. */
    STAMPROFILEADV              ProfWriteR0;
    /** Profiling read handler overhead in GC. */
    STAMPROFILEADV              ProfReadGC;
    /** Profiling write handler overhead in GC. */
    STAMPROFILEADV              ProfWriteGC;
    /** Number of reads to this address from R0 which was serviced in R3. */
    STAMCOUNTER                 ReadR0ToR3;
    /** Number of writes to this address from R0 which was serviced in R3. */
    STAMCOUNTER                 WriteR0ToR3;
    /** Number of reads to this address from GC which was serviced in R3. */
    STAMCOUNTER                 ReadGCToR3;
    /** Number of writes to this address from GC which was serviced in R3. */
    STAMCOUNTER                 WriteGCToR3;
} IOMMMIOSTATS;
/** Pointer to I/O port statistics. */
typedef IOMMMIOSTATS *PIOMMMIOSTATS;


/**
 * I/O port range descriptor, R3 version.
 */
typedef struct IOMIOPORTRANGER3
{
    /** Avl node core with Port as Key and Port + cPorts - 1 as KeyLast. */
    AVLROIOPORTNODECORE         Core;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32 && !defined(RT_OS_WINDOWS)
    uint32_t                    u32Alignment; /**< The sizeof(Core) differs. */
#endif
    /** Start I/O port address. */
    RTIOPORT                    Port;
    /** Size of the range. */
    uint16_t                    cPorts;
    /** Pointer to user argument. */
    RTR3PTR                     pvUser;
    /** Pointer to the associated device instance. */
    R3PTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to OUT callback function. */
    R3PTRTYPE(PFNIOMIOPORTOUT)  pfnOutCallback;
    /** Pointer to IN callback function. */
    R3PTRTYPE(PFNIOMIOPORTIN)   pfnInCallback;
    /** Pointer to string OUT callback function. */
    R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMIOPORTRANGER3;
/** Pointer to I/O port range descriptor, R3 version. */
typedef IOMIOPORTRANGER3 *PIOMIOPORTRANGER3;

/**
 * I/O port range descriptor, R0 version.
 */
typedef struct IOMIOPORTRANGER0
{
    /** Avl node core with Port as Key and Port + cPorts - 1 as KeyLast. */
    AVLROIOPORTNODECORE         Core;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32 && !defined(RT_OS_WINDOWS)
    uint32_t                    u32Alignment; /**< The sizeof(Core) differs. */
#endif
    /** Start I/O port address. */
    RTIOPORT                    Port;
    /** Size of the range. */
    uint16_t                    cPorts;
    /** Pointer to user argument. */
    RTR0PTR                     pvUser;
    /** Pointer to the associated device instance. */
    R0PTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to OUT callback function. */
    R0PTRTYPE(PFNIOMIOPORTOUT)  pfnOutCallback;
    /** Pointer to IN callback function. */
    R0PTRTYPE(PFNIOMIOPORTIN)   pfnInCallback;
    /** Pointer to string OUT callback function. */
    R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMIOPORTRANGER0;
/** Pointer to I/O port range descriptor, R0 version. */
typedef IOMIOPORTRANGER0 *PIOMIOPORTRANGER0;

/**
 * I/O port range descriptor.
 */
typedef struct IOMIOPORTRANGEGC
{
    /** Avl node core with Port as Key and Port + cPorts - 1 as KeyLast. */
    AVLROIOPORTNODECORE         Core;
    /** Start I/O port address. */
    RTIOPORT                    Port;
    /** Size of the range. */
    uint16_t                    cPorts;
    /** Pointer to user argument. */
    RTGCPTR                     pvUser;
    /** Pointer to the associated device instance. */
    GCPTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to OUT callback function. */
    GCPTRTYPE(PFNIOMIOPORTOUT)  pfnOutCallback;
    /** Pointer to IN callback function. */
    GCPTRTYPE(PFNIOMIOPORTIN)   pfnInCallback;
    /** Pointer to string OUT callback function. */
    GCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    GCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    RTGCPTR                     GCPtrAlignment; /**< pszDesc is 8 byte aligned. */
#endif 
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMIOPORTRANGEGC;
/** Pointer to I/O port range descriptor, GC version. */
typedef IOMIOPORTRANGEGC *PIOMIOPORTRANGEGC;


/**
 * I/O port statistics. (one I/O port)
 *
 * This is a simple way of making on demand statistics, however it's a
 * bit free with the hypervisor heap memory..
 */
typedef struct IOMIOPORTSTATS
{
    /** Avl node core with the port as Key. */
    AVLOIOPORTNODECORE          Core;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32 && !defined(RT_OS_WINDOWS)
    uint32_t                    u32Alignment; /**< The sizeof(Core) differs. */
#endif
    /** Number of INs to this port from R3. */
    STAMCOUNTER                 InR3;
    /** Number of OUTs to this port from R3. */
    STAMCOUNTER                 OutR3;
    /** Number of INs to this port from R0. */
    STAMCOUNTER                 InR0;
    /** Number of OUTs to this port from R0. */
    STAMCOUNTER                 OutR0;
    /** Number of INs to this port from GC. */
    STAMCOUNTER                 InGC;
    /** Number of OUTs to this port from GC. */
    STAMCOUNTER                 OutGC;
    /** Profiling IN handler overhead in R3. */
    STAMPROFILEADV              ProfInR3;
    /** Profiling OUT handler overhead in R3. */
    STAMPROFILEADV              ProfOutR3;
    /** Profiling IN handler overhead in R0. */
    STAMPROFILEADV              ProfInR0;
    /** Profiling OUT handler overhead in R0. */
    STAMPROFILEADV              ProfOutR0;
    /** Profiling IN handler overhead in GC. */
    STAMPROFILEADV              ProfInGC;
    /** Profiling OUT handler overhead in GC. */
    STAMPROFILEADV              ProfOutGC;
    /** Number of INs to this port from R0 which was serviced in R3. */
    STAMCOUNTER                 InR0ToR3;
    /** Number of OUTs to this port from R0 which was serviced in R3. */
    STAMCOUNTER                 OutR0ToR3;
    /** Number of INs to this port from GC which was serviced in R3. */
    STAMCOUNTER                 InGCToR3;
    /** Number of OUTs to this port from GC which was serviced in R3. */
    STAMCOUNTER                 OutGCToR3;
} IOMIOPORTSTATS;
/** Pointer to I/O port statistics. */
typedef IOMIOPORTSTATS *PIOMIOPORTSTATS;


/**
 * The IOM trees.
 * These are offset based the nodes and root must be in the same
 * memory block in HC. The locations of IOM structure and the hypervisor heap
 * are quite different in HC and GC.
 */
typedef struct IOMTREES
{
    /** Tree containing I/O port range descriptors registered for HC (IOMIOPORTRANGEHC). */
    AVLROIOPORTTREE         IOPortTreeR3;
    /** Tree containing I/O port range descriptors registered for R0 (IOMIOPORTRANGER0). */
    AVLROIOPORTTREE         IOPortTreeR0;
    /** Tree containing I/O port range descriptors registered for GC (IOMIOPORTRANGEGC). */
    AVLROIOPORTTREE         IOPortTreeGC;

    /** Tree containing MMIO range descriptors registered for HC (IOMMMIORANGEHC). */
    AVLROGCPHYSTREE         MMIOTreeR3;
    /** Tree containing MMIO range descriptors registered for R0 (IOMMMIORANGER0). */
    AVLROGCPHYSTREE         MMIOTreeR0;
    /** Tree containing MMIO range descriptors registered for GC (IOMMMIORANGEGC). */
    AVLROGCPHYSTREE         MMIOTreeGC;

    /** Tree containing I/O port statistics (IOMIOPORTSTATS). */
    AVLOIOPORTTREE          IOPortStatTree;
    /** Tree containing MMIO statistics (IOMMMIOSTATS). */
    AVLOGCPHYSTREE          MMIOStatTree;
} IOMTREES;
/** Pointer to the IOM trees. */
typedef IOMTREES *PIOMTREES;


/**
 * Converts an IOM pointer into a VM pointer.
 * @returns Pointer to the VM structure the PGM is part of.
 * @param   pIOM   Pointer to IOM instance data.
 */
#define IOM2VM(pIOM)  ( (PVM)((char*)pIOM - pIOM->offVM) )

/**
 * IOM Data (part of VM)
 */
typedef struct IOM
{
    /** Offset to the VM structure. */
    RTINT                           offVM;

    /** Pointer to the trees - GC ptr. */
    GCPTRTYPE(PIOMTREES)            pTreesGC;
    /** Pointer to the trees - HC ptr. */
    R3R0PTRTYPE(PIOMTREES)          pTreesHC;


    /** @name Caching of I/O Port ranges and statistics.
     * (Saves quite some time in rep outs/ins instruction emulation.)
     * @{ */
    R3PTRTYPE(PIOMIOPORTRANGER3)    pRangeLastReadR3;
    R3PTRTYPE(PIOMIOPORTRANGER3)    pRangeLastWriteR3;
    R3PTRTYPE(PIOMIOPORTSTATS)      pStatsLastReadR3;
    R3PTRTYPE(PIOMIOPORTSTATS)      pStatsLastWriteR3;

    R3R0PTRTYPE(PIOMIOPORTRANGER0)  pRangeLastReadR0;
    R3R0PTRTYPE(PIOMIOPORTRANGER0)  pRangeLastWriteR0;
    R3R0PTRTYPE(PIOMIOPORTSTATS)    pStatsLastReadR0;
    R3R0PTRTYPE(PIOMIOPORTSTATS)    pStatsLastWriteR0;

    GCPTRTYPE(PIOMIOPORTRANGEGC)    pRangeLastReadGC;
    GCPTRTYPE(PIOMIOPORTRANGEGC)    pRangeLastWriteGC;
    GCPTRTYPE(PIOMIOPORTSTATS)      pStatsLastReadGC;
    GCPTRTYPE(PIOMIOPORTSTATS)      pStatsLastWriteGC;
    /** @} */

    /** @name I/O Port statistics.
     * @{ */
    STAMPROFILE             StatGCIOPortHandler;

    STAMCOUNTER             StatGCInstIn;
    STAMCOUNTER             StatGCInstOut;
    STAMCOUNTER             StatGCInstIns;
    STAMCOUNTER             StatGCInstOuts;
    /** @} */

    /** @name MMIO statistics.
     * @{ */
    STAMPROFILE             StatGCMMIOHandler;
    STAMCOUNTER             StatGCMMIOFailures;

    STAMPROFILE             StatGCInstMov;
    STAMPROFILE             StatGCInstCmp;
    STAMPROFILE             StatGCInstAnd;
    STAMPROFILE             StatGCInstTest;
    STAMPROFILE             StatGCInstXchg;
    STAMPROFILE             StatGCInstStos;
    STAMPROFILE             StatGCInstLods;
    STAMPROFILE             StatGCInstMovs;
    STAMPROFILE             StatGCInstMovsToMMIO;
    STAMPROFILE             StatGCInstMovsFromMMIO;
    STAMPROFILE             StatGCInstMovsMMIO;
    STAMCOUNTER             StatGCInstOther;

    STAMCOUNTER             StatGCMMIO1Byte;
    STAMCOUNTER             StatGCMMIO2Bytes;
    STAMCOUNTER             StatGCMMIO4Bytes;

    RTUINT                  cMovsMaxBytes;
    RTUINT                  cStosMaxBytes;
    /** @} */

} IOM;
/** Pointer to IOM instance data. */
typedef IOM *PIOM;


__BEGIN_DECLS

#ifdef IN_IOM_R3
PIOMIOPORTSTATS iomr3IOPortStatsCreate(PVM pVM, RTIOPORT Port, const char *pszDesc);
PIOMMMIOSTATS iomR3MMIOStatsCreate(PVM pVM, RTGCPHYS GCPhys, const char *pszDesc);
#endif /* IN_IOM_R3 */

/**
 * \#PF Handler callback for MMIO ranges.
 *
 * @returns VBox status code (appropriate for GC return).
 *
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      Pointer to the MMIO range entry.
 */
IOMDECL(int) IOMMMIOHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser);

/**
 * Gets the I/O port range for the specified I/O port in the current context.
 *
 * @returns Pointer to I/O port range.
 * @returns NULL if no port registered.
 *
 * @param   pIOM    IOM instance data.
 * @param   Port    Port to lookup.
 */
inline CTXALLSUFF(PIOMIOPORTRANGE) iomIOPortGetRange(PIOM pIOM, RTIOPORT Port)
{
    CTXALLSUFF(PIOMIOPORTRANGE) pRange = (CTXALLSUFF(PIOMIOPORTRANGE))RTAvlroIOPortRangeGet(&pIOM->CTXSUFF(pTrees)->CTXALLSUFF(IOPortTree), Port);
    return pRange;
}

/**
 * Gets the I/O port range for the specified I/O port in the HC.
 *
 * @returns Pointer to I/O port range.
 * @returns NULL if no port registered.
 *
 * @param   pIOM    IOM instance data.
 * @param   Port    Port to lookup.
 */
inline PIOMIOPORTRANGER3 iomIOPortGetRangeHC(PIOM pIOM, RTIOPORT Port)
{
    PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pIOM->CTXSUFF(pTrees)->IOPortTreeR3, Port);
    return pRange;
}


/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pIOM    IOM instance data.
 * @param   GCPhys  Physical address to lookup.
 */
inline CTXALLSUFF(PIOMMMIORANGE) iomMMIOGetRange(PIOM pIOM, RTGCPHYS GCPhys)
{
    CTXALLSUFF(PIOMMMIORANGE) pRange = (CTXALLSUFF(PIOMMMIORANGE))RTAvlroGCPhysRangeGet(&pIOM->CTXSUFF(pTrees)->CTXALLSUFF(MMIOTree), GCPhys);
    return pRange;
}


/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pIOM    IOM instance data.
 * @param   GCPhys  Physical address to lookup.
 */
inline PIOMMMIORANGER3 iomMMIOGetRangeHC(PIOM pIOM, RTGCPHYS GCPhys)
{
    PIOMMMIORANGER3 pRange = (PIOMMMIORANGER3)RTAvlroGCPhysRangeGet(&pIOM->CTXSUFF(pTrees)->MMIOTreeR3, GCPhys);
    return pRange;
}

#ifdef VBOX_WITH_STATISTICS
/**
 * Gets the MMIO statistics record.
 * @returns Pointer to MMIO stats.
 * @returns NULL if not found.
 *
 * @param   pIOM    IOM instance data.
 * @param   GCPhys  Physical address to lookup.
 */
inline PIOMMMIOSTATS iomMMIOGetStats(PIOM pIOM, RTGCPHYS GCPhys)
{
    PIOMMMIOSTATS pStats = (PIOMMMIOSTATS)RTAvloGCPhysGet(&pIOM->CTXSUFF(pTrees)->MMIOStatTree, GCPhys);
    return pStats;
}
#endif

__END_DECLS

#ifdef IN_RING3

#endif

/** @} */

#endif /* ___IOMInternal_h */
