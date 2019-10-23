/* $Id$ */
/** @file
 * IOM - Internal header file.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_IOMInternal_h
#define VMM_INCLUDED_SRC_include_IOMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define IOM_WITH_CRIT_SECT_RW

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/pdmcritsect.h>
#ifdef IOM_WITH_CRIT_SECT_RW
# include <VBox/vmm/pdmcritsectrw.h>
#endif
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/avl.h>



/** @defgroup grp_iom_int   Internals
 * @ingroup grp_iom
 * @internal
 * @{
 */

/**
 * MMIO range descriptor.
 */
typedef struct IOMMMIORANGE
{
    /** Avl node core with GCPhys as Key and GCPhys + cbSize - 1 as KeyLast. */
    AVLROGCPHYSNODECORE         Core;
    /** Start physical address. */
    RTGCPHYS                    GCPhys;
    /** Size of the range. */
    RTGCPHYS                    cb;
    /** The reference counter. */
    uint32_t volatile           cRefs;
    /** Flags, see IOMMMIO_FLAGS_XXX. */
    uint32_t                    fFlags;

    /** Pointer to user argument - R0. */
    RTR0PTR                     pvUserR0;
    /** Pointer to device instance - R0. */
    PPDMDEVINSR0                pDevInsR0;
    /** Pointer to write callback function - R0. */
    R0PTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallbackR0;
    /** Pointer to read callback function - R0. */
    R0PTRTYPE(PFNIOMMMIOREAD)   pfnReadCallbackR0;
    /** Pointer to fill (memset) callback function - R0. */
    R0PTRTYPE(PFNIOMMMIOFILL)   pfnFillCallbackR0;

    /** Pointer to user argument - R3. */
    RTR3PTR                     pvUserR3;
    /** Pointer to device instance - R3. */
    PPDMDEVINSR3                pDevInsR3;
    /** Pointer to write callback function - R3. */
    R3PTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallbackR3;
    /** Pointer to read callback function - R3. */
    R3PTRTYPE(PFNIOMMMIOREAD)   pfnReadCallbackR3;
    /** Pointer to fill (memset) callback function - R3. */
    R3PTRTYPE(PFNIOMMMIOFILL)   pfnFillCallbackR3;

    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;

#if 0
    /** Pointer to user argument - RC. */
    RTRCPTR                     pvUserRC;
    /** Pointer to device instance - RC. */
    PPDMDEVINSRC                pDevInsRC;
    /** Pointer to write callback function - RC. */
    RCPTRTYPE(PFNIOMMMIOWRITE)  pfnWriteCallbackRC;
    /** Pointer to read callback function - RC. */
    RCPTRTYPE(PFNIOMMMIOREAD)   pfnReadCallbackRC;
    /** Pointer to fill (memset) callback function - RC. */
    RCPTRTYPE(PFNIOMMMIOFILL)   pfnFillCallbackRC;
#if HC_ARCH_BITS == 64
    /** Padding structure length to multiple of 8 bytes. */
    RTRCPTR                     RCPtrPadding;
#endif
#endif
} IOMMMIORANGE;
/** Pointer to a MMIO range descriptor, R3 version. */
typedef struct IOMMMIORANGE *PIOMMMIORANGE;


/**
 * MMIO address statistics. (one address)
 *
 * This is a simple way of making on demand statistics, however it's a
 * bit free with the hypervisor heap memory.
 */
typedef struct IOMMMIOSTATS
{
    /** Avl node core with the address as Key. */
    AVLOGCPHYSNODECORE          Core;

    /** Number of accesses (subtract ReadRZToR3 and WriteRZToR3 to get the right
     *  number). */
    STAMCOUNTER                 Accesses;

    /** Profiling read handler overhead in R3. */
    STAMPROFILE                 ProfReadR3;
    /** Profiling write handler overhead in R3. */
    STAMPROFILE                 ProfWriteR3;
    /** Counting and profiling reads in R0/RC. */
    STAMPROFILE                 ProfReadRZ;
    /** Counting and profiling writes in R0/RC. */
    STAMPROFILE                 ProfWriteRZ;

    /** Number of reads to this address from R0/RC which was serviced in R3. */
    STAMCOUNTER                 ReadRZToR3;
    /** Number of writes to this address from R0/RC which was serviced in R3. */
    STAMCOUNTER                 WriteRZToR3;
} IOMMMIOSTATS;
AssertCompileMemberAlignment(IOMMMIOSTATS, Accesses, 8);
/** Pointer to I/O port statistics. */
typedef IOMMMIOSTATS *PIOMMMIOSTATS;

/**
 * I/O port lookup table entry.
 */
typedef struct IOMIOPORTLOOKUPENTRY
{
    /** The first port in the range. */
    RTIOPORT                    uFirstPort;
    /** The last port in the range (inclusive). */
    RTIOPORT                    uLastPort;
    /** The registration handle/index. */
    uint16_t                    idx;
} IOMIOPORTLOOKUPENTRY;
/** Pointer to an I/O port lookup table entry. */
typedef IOMIOPORTLOOKUPENTRY *PIOMIOPORTLOOKUPENTRY;
/** Pointer to a const I/O port lookup table entry. */
typedef IOMIOPORTLOOKUPENTRY const *PCIOMIOPORTLOOKUPENTRY;

/**
 * Ring-0 I/O port handle table entry.
 */
typedef struct IOMIOPORTENTRYR0
{
    /** Pointer to user argument. */
    RTR0PTR                             pvUser;
    /** Pointer to the associated device instance, NULL if entry not used. */
    R0PTRTYPE(PPDMDEVINS)               pDevIns;
    /** Pointer to OUT callback function. */
    R0PTRTYPE(PFNIOMIOPORTNEWOUT)       pfnOutCallback;
    /** Pointer to IN callback function. */
    R0PTRTYPE(PFNIOMIOPORTNEWIN)        pfnInCallback;
    /** Pointer to string OUT callback function. */
    R0PTRTYPE(PFNIOMIOPORTNEWOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    R0PTRTYPE(PFNIOMIOPORTNEWINSTRING)  pfnInStrCallback;
    /** The entry of the first statistics entry, UINT16_MAX if no stats. */
    uint16_t                            idxStats;
    /** The number of ports covered by this entry, 0 if entry not used. */
    RTIOPORT                            cPorts;
    /** Same as the handle index. */
    uint16_t                            idxSelf;
    /** IOM_IOPORT_F_XXX (copied from ring-3). */
    uint16_t                            fFlags;
} IOMIOPORTENTRYR0;
/** Pointer to a ring-0 I/O port handle table entry. */
typedef IOMIOPORTENTRYR0 *PIOMIOPORTENTRYR0;
/** Pointer to a const ring-0 I/O port handle table entry. */
typedef IOMIOPORTENTRYR0 const *PCIOMIOPORTENTRYR0;

/**
 * Ring-3 I/O port handle table entry.
 */
typedef struct IOMIOPORTENTRYR3
{
    /** Pointer to user argument. */
    RTR3PTR                             pvUser;
    /** Pointer to the associated device instance. */
    R3PTRTYPE(PPDMDEVINS)               pDevIns;
    /** Pointer to OUT callback function. */
    R3PTRTYPE(PFNIOMIOPORTNEWOUT)       pfnOutCallback;
    /** Pointer to IN callback function. */
    R3PTRTYPE(PFNIOMIOPORTNEWIN)        pfnInCallback;
    /** Pointer to string OUT callback function. */
    R3PTRTYPE(PFNIOMIOPORTNEWOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    R3PTRTYPE(PFNIOMIOPORTNEWINSTRING)  pfnInStrCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)             pszDesc;
    /** Extended port description table, optional. */
    R3PTRTYPE(PCIOMIOPORTDESC)          paExtDescs;
    /** PCI device the registration is associated with. */
    R3PTRTYPE(PPDMPCIDEV)               pPciDev;
    /** The PCI device region (high 16-bit word) and subregion (low word),
     *  UINT32_MAX if not applicable. */
    uint32_t                            iPciRegion;
    /** The number of ports covered by this entry. */
    RTIOPORT                            cPorts;
    /** The current port mapping (duplicates lookup table). */
    RTIOPORT                            uPort;
    /** The entry of the first statistics entry, UINT16_MAX if no stats. */
    uint16_t                            idxStats;
    /** Set if mapped, clear if not.
     * Only updated when critsect is held exclusively.   */
    bool                                fMapped;
    /** Set if there is an ring-0 entry too. */
    bool                                fRing0;
    /** Set if there is an raw-mode entry too. */
    bool                                fRawMode;
    /** IOM_IOPORT_F_XXX */
    uint8_t                             fFlags;
    /** Same as the handle index. */
    uint16_t                            idxSelf;
} IOMIOPORTENTRYR3;
AssertCompileSize(IOMIOPORTENTRYR3, 9 * sizeof(RTR3PTR) + 16);
/** Pointer to a ring-3 I/O port handle table entry. */
typedef IOMIOPORTENTRYR3 *PIOMIOPORTENTRYR3;
/** Pointer to a const ring-3 I/O port handle table entry. */
typedef IOMIOPORTENTRYR3 const *PCIOMIOPORTENTRYR3;

/**
 * I/O port statistics entry (one I/O port).
 */
typedef struct IOMIOPORTSTATSENTRY
{
    /** Number of INs to this port from R3. */
    STAMCOUNTER                 InR3;
    /** Profiling IN handler overhead in R3. */
    STAMPROFILE                 ProfInR3;
    /** Number of OUTs to this port from R3. */
    STAMCOUNTER                 OutR3;
    /** Profiling OUT handler overhead in R3. */
    STAMPROFILE                 ProfOutR3;

    /** Number of INs to this port from R0/RC. */
    STAMCOUNTER                 InRZ;
    /** Profiling IN handler overhead in R0/RC. */
    STAMPROFILE                 ProfInRZ;
    /** Number of INs to this port from R0/RC which was serviced in R3. */
    STAMCOUNTER                 InRZToR3;

    /** Number of OUTs to this port from R0/RC. */
    STAMCOUNTER                 OutRZ;
    /** Profiling OUT handler overhead in R0/RC. */
    STAMPROFILE                 ProfOutRZ;
    /** Number of OUTs to this port from R0/RC which was serviced in R3. */
    STAMCOUNTER                 OutRZToR3;
} IOMIOPORTSTATSENTRY;
/** Pointer to I/O port statistics entry. */
typedef IOMIOPORTSTATSENTRY *PIOMIOPORTSTATSENTRY;



/**
 * MMIO lookup table entry.
 */
typedef struct IOMMMIOLOOKUPENTRY
{
    /** The first port in the range. */
    RTGCPHYS                    GCPhysFirst;
    /** The last port in the range (inclusive). */
    RTGCPHYS                    GCPhysLast;
    /** The registration handle/index.
     * @todo bake this into the lower/upper bits of GCPhysFirst & GCPhysLast. */
    uint16_t                    idx;
    uint16_t                    abPadding[3];
} IOMMMIOLOOKUPENTRY;
/** Pointer to an MMIO lookup table entry. */
typedef IOMMMIOLOOKUPENTRY *PIOMMMIOLOOKUPENTRY;
/** Pointer to a const MMIO lookup table entry. */
typedef IOMMMIOLOOKUPENTRY const *PCIOMMMIOLOOKUPENTRY;

/**
 * Ring-0 MMIO handle table entry.
 */
typedef struct IOMMMIOENTRYR0
{
    /** The number of bytes covered by this entry, 0 if entry not used. */
    RTGCPHYS                            cbRegion;
    /** Pointer to user argument. */
    RTR0PTR                             pvUser;
    /** Pointer to the associated device instance, NULL if entry not used. */
    R0PTRTYPE(PPDMDEVINS)               pDevIns;
    /** Pointer to the write callback function. */
    R0PTRTYPE(PFNIOMMMIONEWWRITE)       pfnWriteCallback;
    /** Pointer to the read callback function. */
    R0PTRTYPE(PFNIOMMMIONEWREAD)        pfnReadCallback;
    /** Pointer to the fill callback function. */
    R0PTRTYPE(PFNIOMMMIONEWFILL)        pfnFillCallback;
    /** The entry of the first statistics entry, UINT16_MAX if no stats.
     * @note For simplicity, this is always copied from ring-3 for all entries at
     *       the end of VM creation. */
    uint16_t                            idxStats;
    /** Same as the handle index. */
    uint16_t                            idxSelf;
    /** IOM_MMIO_F_XXX (copied from ring-3). */
    uint32_t                            fFlags;
} IOMMMIOENTRYR0;
/** Pointer to a ring-0 MMIO handle table entry. */
typedef IOMMMIOENTRYR0 *PIOMMMIOENTRYR0;
/** Pointer to a const ring-0 MMIO handle table entry. */
typedef IOMMMIOENTRYR0 const *PCIOMMMIOENTRYR0;

/**
 * Ring-3 MMIO handle table entry.
 */
typedef struct IOMMMIOENTRYR3
{
    /** The number of bytes covered by this entry. */
    RTGCPHYS                            cbRegion;
    /** The current mapping address (duplicates lookup table).
     * This is set to NIL_RTGCPHYS if not mapped (exclusive lock + atomic). */
    RTGCPHYS volatile                   GCPhysMapping;
    /** Pointer to user argument. */
    RTR3PTR                             pvUser;
    /** Pointer to the associated device instance. */
    R3PTRTYPE(PPDMDEVINS)               pDevIns;
    /** Pointer to the write callback function. */
    R3PTRTYPE(PFNIOMMMIONEWWRITE)       pfnWriteCallback;
    /** Pointer to the read callback function. */
    R3PTRTYPE(PFNIOMMMIONEWREAD)        pfnReadCallback;
    /** Pointer to the fill callback function. */
    R3PTRTYPE(PFNIOMMMIONEWFILL)        pfnFillCallback;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)             pszDesc;
    /** PCI device the registration is associated with. */
    R3PTRTYPE(PPDMPCIDEV)               pPciDev;
    /** The PCI device region (high 16-bit word) and subregion (low word),
     *  UINT32_MAX if not applicable. */
    uint32_t                            iPciRegion;
    /** IOM_MMIO_F_XXX */
    uint32_t                            fFlags;
    /** The entry of the first statistics entry, UINT16_MAX if no stats. */
    uint16_t                            idxStats;
    /** Set if mapped, clear if not.
     * Only updated when critsect is held exclusively.
     * @todo remove as GCPhysMapping != NIL_RTGCPHYS serves the same purpose. */
    bool volatile                       fMapped;
    /** Set if there is an ring-0 entry too. */
    bool                                fRing0;
    /** Set if there is an raw-mode entry too. */
    bool                                fRawMode;
    uint8_t                             bPadding;
    /** Same as the handle index. */
    uint16_t                            idxSelf;
} IOMMMIOENTRYR3;
AssertCompileSize(IOMMMIOENTRYR3, sizeof(RTGCPHYS) * 2 + 7 * sizeof(RTR3PTR) + 16);
/** Pointer to a ring-3 MMIO handle table entry. */
typedef IOMMMIOENTRYR3 *PIOMMMIOENTRYR3;
/** Pointer to a const ring-3 MMIO handle table entry. */
typedef IOMMMIOENTRYR3 const *PCIOMMMIOENTRYR3;

/**
 * MMIO statistics entry (one MMIO).
 */
typedef struct IOMMMIOSTATSENTRY
{
    /** Counting and profiling reads in R0/RC. */
    STAMPROFILE                 ProfReadRZ;
    /** Number of successful read accesses. */
    STAMCOUNTER                 Reads;
    /** Number of reads to this address from R0/RC which was serviced in R3. */
    STAMCOUNTER                 ReadRZToR3;
    /** Number of complicated reads. */
    STAMCOUNTER                 ComplicatedReads;
    /** Number of reads of 0xff or 0x00. */
    STAMCOUNTER                 FFor00Reads;
    /** Profiling read handler overhead in R3. */
    STAMPROFILE                 ProfReadR3;

    /** Counting and profiling writes in R0/RC. */
    STAMPROFILE                 ProfWriteRZ;
    /** Number of successful read accesses. */
    STAMCOUNTER                 Writes;
    /** Number of writes to this address from R0/RC which was serviced in R3. */
    STAMCOUNTER                 WriteRZToR3;
    /** Number of writes to this address from R0/RC which was committed in R3. */
    STAMCOUNTER                 CommitRZToR3;
    /** Number of complicated writes. */
    STAMCOUNTER                 ComplicatedWrites;
    /** Profiling write handler overhead in R3. */
    STAMPROFILE                 ProfWriteR3;
} IOMMMIOSTATSENTRY;
/** Pointer to MMIO statistics entry. */
typedef IOMMMIOSTATSENTRY *PIOMMMIOSTATSENTRY;



/**
 * I/O port range descriptor, R3 version.
 */
typedef struct IOMIOPORTRANGER3
{
    /** Avl node core with Port as Key and Port + cPorts - 1 as KeyLast. */
    AVLROIOPORTNODECORE         Core;
#if HC_ARCH_BITS == 64 && !defined(RT_OS_WINDOWS)
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
#if HC_ARCH_BITS == 64 && !defined(RT_OS_WINDOWS)
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
 * I/O port range descriptor, RC version.
 */
typedef struct IOMIOPORTRANGERC
{
    /** Avl node core with Port as Key and Port + cPorts - 1 as KeyLast. */
    AVLROIOPORTNODECORE         Core;
    /** Start I/O port address. */
    RTIOPORT                    Port;
    /** Size of the range. */
    uint16_t                    cPorts;
    /** Pointer to user argument. */
    RTRCPTR                     pvUser;
    /** Pointer to the associated device instance. */
    RCPTRTYPE(PPDMDEVINS)       pDevIns;
    /** Pointer to OUT callback function. */
    RCPTRTYPE(PFNIOMIOPORTOUT)  pfnOutCallback;
    /** Pointer to IN callback function. */
    RCPTRTYPE(PFNIOMIOPORTIN)   pfnInCallback;
    /** Pointer to string OUT callback function. */
    RCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback;
    /** Pointer to string IN callback function. */
    RCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback;
#if HC_ARCH_BITS == 64
    RTRCPTR                     RCPtrAlignment; /**< pszDesc is 8 byte aligned. */
#endif
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)     pszDesc;
} IOMIOPORTRANGERC;
/** Pointer to I/O port range descriptor, RC version. */
typedef IOMIOPORTRANGERC *PIOMIOPORTRANGERC;


/**
 * I/O port statistics. (one I/O port)
 *
 * This is a simple way of making on demand statistics, however it's a
 * bit free with the hypervisor heap memory.
 */
typedef struct IOMIOPORTSTATS
{
    /** Avl node core with the port as Key. */
    AVLOIOPORTNODECORE          Core;
#if HC_ARCH_BITS != 64 || !defined(RT_OS_WINDOWS)
    uint32_t                    u32Alignment; /**< The sizeof(Core) differs. */
#endif
    /** Number of INs to this port from R3. */
    STAMCOUNTER                 InR3;
    /** Profiling IN handler overhead in R3. */
    STAMPROFILE                 ProfInR3;
    /** Number of OUTs to this port from R3. */
    STAMCOUNTER                 OutR3;
    /** Profiling OUT handler overhead in R3. */
    STAMPROFILE                 ProfOutR3;

    /** Number of INs to this port from R0/RC. */
    STAMCOUNTER                 InRZ;
    /** Profiling IN handler overhead in R0/RC. */
    STAMPROFILE                 ProfInRZ;
    /** Number of INs to this port from R0/RC which was serviced in R3. */
    STAMCOUNTER                 InRZToR3;

    /** Number of OUTs to this port from R0/RC. */
    STAMCOUNTER                 OutRZ;
    /** Profiling OUT handler overhead in R0/RC. */
    STAMPROFILE                 ProfOutRZ;
    /** Number of OUTs to this port from R0/RC which was serviced in R3. */
    STAMCOUNTER                 OutRZToR3;
} IOMIOPORTSTATS;
AssertCompileMemberAlignment(IOMIOPORTSTATS, InR3, 8);
/** Pointer to I/O port statistics. */
typedef IOMIOPORTSTATS *PIOMIOPORTSTATS;


/**
 * The IOM trees.
 *
 * These are offset based the nodes and root must be in the same
 * memory block in HC. The locations of IOM structure and the hypervisor heap
 * are quite different in R3, R0 and RC.
 */
typedef struct IOMTREES
{
    /** Tree containing I/O port range descriptors registered for HC (IOMIOPORTRANGEHC). */
    AVLROIOPORTTREE         IOPortTreeR3;
    /** Tree containing I/O port range descriptors registered for R0 (IOMIOPORTRANGER0). */
    AVLROIOPORTTREE         IOPortTreeR0;
#if 0
    /** Tree containing I/O port range descriptors registered for RC (IOMIOPORTRANGERC). */
    AVLROIOPORTTREE         IOPortTreeRC;
#endif

    /** Tree containing the MMIO range descriptors (IOMMMIORANGE). */
    AVLROGCPHYSTREE         MMIOTree;

    /** Tree containing I/O port statistics (IOMIOPORTSTATS). */
    AVLOIOPORTTREE          IOPortStatTree;
    /** Tree containing MMIO statistics (IOMMMIOSTATS). */
    AVLOGCPHYSTREE          MmioStatTree;
} IOMTREES;
/** Pointer to the IOM trees. */
typedef IOMTREES *PIOMTREES;


/**
 * IOM per virtual CPU instance data.
 */
typedef struct IOMCPU
{
    /** For saving stack space, the disassembler state is allocated here instead of
     * on the stack. */
    DISCPUSTATE                     DisState;

    /**
     * Pending I/O port write commit (VINF_IOM_R3_IOPORT_COMMIT_WRITE).
     *
     * This is a converted VINF_IOM_R3_IOPORT_WRITE handler return that lets the
     * execution engine commit the instruction and then return to ring-3 to complete
     * the I/O port write there.  This avoids having to decode the instruction again
     * in ring-3.
     */
    struct
    {
        /** The value size (0 if not pending). */
        uint16_t                        cbValue;
        /** The I/O port. */
        RTIOPORT                        IOPort;
        /** The value. */
        uint32_t                        u32Value;
    } PendingIOPortWrite;

    /**
     * Pending MMIO write commit (VINF_IOM_R3_MMIO_COMMIT_WRITE).
     *
     * This is a converted VINF_IOM_R3_MMIO_WRITE handler return that lets the
     * execution engine commit the instruction, stop any more REPs, and return to
     * ring-3 to complete the MMIO write there.  The avoid the tedious decoding of
     * the instruction again once we're in ring-3, more importantly it allows us to
     * correctly deal with read-modify-write instructions like XCHG, OR, and XOR.
     */
    struct
    {
        /** Guest physical MMIO address. */
        RTGCPHYS                        GCPhys;
        /** The number of bytes to write (0 if nothing pending). */
        uint32_t                        cbValue;
        /** Hint. */
        uint32_t                        idxMmioRegionHint;
        /** The value to write. */
        uint8_t                         abValue[128];
    } PendingMmioWrite;

    /** @name Caching of I/O Port and MMIO ranges and statistics.
     * (Saves quite some time in rep outs/ins instruction emulation.)
     * @{ */
    /** I/O port registration index for the last read operation. */
    uint16_t                            idxIoPortLastRead;
    /** I/O port registration index for the last write operation. */
    uint16_t                            idxIoPortLastWrite;
    /** I/O port registration index for the last read string operation. */
    uint16_t                            idxIoPortLastReadStr;
    /** I/O port registration index for the last write string operation. */
    uint16_t                            idxIoPortLastWriteStr;

    /** MMIO port registration index for the last IOMR3MmioPhysHandler call.
     * @note pretty static as only used by APIC on AMD-V.  */
    uint16_t                            idxMmioLastPhysHandler;
    uint16_t                            u16Padding;

    R3PTRTYPE(PIOMIOPORTRANGER3)    pRangeLastReadR3;
    R3PTRTYPE(PIOMIOPORTRANGER3)    pRangeLastWriteR3;
    R3PTRTYPE(PIOMIOPORTSTATS)      pStatsLastReadR3;
    R3PTRTYPE(PIOMIOPORTSTATS)      pStatsLastWriteR3;
    R3PTRTYPE(PIOMMMIORANGE)        pMMIORangeLastR3;
    R3PTRTYPE(PIOMMMIOSTATS)        pMMIOStatsLastR3;

    R0PTRTYPE(PIOMIOPORTRANGER0)    pRangeLastReadR0;
    R0PTRTYPE(PIOMIOPORTRANGER0)    pRangeLastWriteR0;
    R0PTRTYPE(PIOMIOPORTSTATS)      pStatsLastReadR0;
    R0PTRTYPE(PIOMIOPORTSTATS)      pStatsLastWriteR0;
    R0PTRTYPE(PIOMMMIORANGE)        pMMIORangeLastR0;
    R0PTRTYPE(PIOMMMIOSTATS)        pMMIOStatsLastR0;
    /** @} */
} IOMCPU;
/** Pointer to IOM per virtual CPU instance data. */
typedef IOMCPU *PIOMCPU;


/**
 * IOM Data (part of VM)
 */
typedef struct IOM
{
    /** Pointer to the trees - R3 ptr. */
    R3PTRTYPE(PIOMTREES)            pTreesR3;
    /** Pointer to the trees - R0 ptr. */
    R0PTRTYPE(PIOMTREES)            pTreesR0;

    /** MMIO physical access handler type.   */
    PGMPHYSHANDLERTYPE              hMmioHandlerType;
    /** MMIO physical access handler type, new style.   */
    PGMPHYSHANDLERTYPE              hNewMmioHandlerType;

    /** @name I/O ports
     * @note The updating of these variables is done exclusively from EMT(0).
     * @{ */
    /** Number of I/O port registrations. */
    uint32_t                        cIoPortRegs;
    /** The size of the paIoPortRegs allocation (in entries). */
    uint32_t                        cIoPortAlloc;
    /** I/O port registration table for ring-3.
     * There is a parallel table in ring-0, IOMR0PERVM::paIoPortRegs. */
    R3PTRTYPE(PIOMIOPORTENTRYR3)    paIoPortRegs;
    /** Number of entries in the lookup table. */
    uint32_t                        cIoPortLookupEntries;
    uint32_t                        u32Padding1;
    /** I/O port lookup table. */
    R3PTRTYPE(PIOMIOPORTLOOKUPENTRY) paIoPortLookup;

    /** The number of valid entries in paioPortStats. */
    uint32_t                        cIoPortStats;
    /** The size of the paIoPortStats allocation (in entries). */
    uint32_t                        cIoPortStatsAllocation;
    /** I/O port lookup table.   */
    R3PTRTYPE(PIOMIOPORTSTATSENTRY) paIoPortStats;
    /** Dummy stats entry so we don't need to check for NULL pointers so much. */
    IOMIOPORTSTATSENTRY             IoPortDummyStats;
    /** @} */

    /** @name MMIO ports
     * @note The updating of these variables is done exclusively from EMT(0).
     * @{ */
    /** Number of MMIO registrations. */
    uint32_t                        cMmioRegs;
    /** The size of the paMmioRegs allocation (in entries). */
    uint32_t                        cMmioAlloc;
    /** MMIO registration table for ring-3.
     * There is a parallel table in ring-0, IOMR0PERVM::paMmioRegs. */
    R3PTRTYPE(PIOMMMIOENTRYR3)      paMmioRegs;
    /** Number of entries in the lookup table. */
    uint32_t                        cMmioLookupEntries;
    uint32_t                        u32Padding2;
    /** MMIO lookup table. */
    R3PTRTYPE(PIOMMMIOLOOKUPENTRY)  paMmioLookup;

    /** The number of valid entries in paioPortStats. */
    uint32_t                        cMmioStats;
    /** The size of the paMmioStats allocation (in entries). */
    uint32_t                        cMmioStatsAllocation;
    /** MMIO lookup table.   */
    R3PTRTYPE(PIOMMMIOSTATSENTRY)   paMmioStats;
    /** Dummy stats entry so we don't need to check for NULL pointers so much. */
    IOMMMIOSTATSENTRY               MmioDummyStats;
    /** @} */


    /** Lock serializing EMT access to IOM. */
#ifdef IOM_WITH_CRIT_SECT_RW
    PDMCRITSECTRW                   CritSect;
#else
    PDMCRITSECT                     CritSect;
#endif

#if 0 /* unused */
    /** @name I/O Port statistics.
     * @{ */
    STAMCOUNTER                     StatInstIn;
    STAMCOUNTER                     StatInstOut;
    STAMCOUNTER                     StatInstIns;
    STAMCOUNTER                     StatInstOuts;
    /** @} */
#endif

    /** @name MMIO statistics.
     * @{ */
    STAMCOUNTER                     StatIoPortCommits;

    STAMCOUNTER                     StatMMIOStaleMappings;
    STAMPROFILE                     StatRZMMIOHandler;
    STAMCOUNTER                     StatRZMMIOReadsToR3;
    STAMCOUNTER                     StatRZMMIOWritesToR3;
    STAMCOUNTER                     StatRZMMIOCommitsToR3;
    STAMCOUNTER                     StatRZMMIODevLockContention;

    STAMCOUNTER                     StatMmioPfHandlerNew;
    STAMCOUNTER                     StatMmioPhysHandlerNew;
    STAMCOUNTER                     StatMmioHandlerNewR3;
    STAMCOUNTER                     StatMmioHandlerNewR0;
    STAMCOUNTER                     StatMmioCommitsDirect;
    STAMCOUNTER                     StatMmioCommitsPgm;

    STAMCOUNTER                     StatR3MMIOHandler;

    STAMCOUNTER                     StatMmioHandlerR3;
    STAMCOUNTER                     StatMmioHandlerR0;

    RTUINT                          cMovsMaxBytes;
    RTUINT                          cStosMaxBytes;
    /** @} */
} IOM;
/** Pointer to IOM instance data. */
typedef IOM *PIOM;


/**
 * IOM data kept in the ring-0 GVM.
 */
typedef struct IOMR0PERVM
{
    /** @name I/O ports
     * @{ */
    /** The higest ring-0 I/O port registration plus one. */
    uint32_t                        cIoPortMax;
    /** The size of the paIoPortRegs allocation (in entries). */
    uint32_t                        cIoPortAlloc;
    /** I/O port registration table for ring-0.
     * There is a parallel table for ring-3, paIoPortRing3Regs. */
    R0PTRTYPE(PIOMIOPORTENTRYR0)    paIoPortRegs;
    /** I/O port lookup table. */
    R0PTRTYPE(PIOMIOPORTLOOKUPENTRY) paIoPortLookup;
    /** I/O port registration table for ring-3.
     * Also mapped to ring-3 as IOM::paIoPortRegs. */
    R0PTRTYPE(PIOMIOPORTENTRYR3)    paIoPortRing3Regs;
    /** Handle to the allocation backing both the ring-0 and ring-3 registration
     * tables as well as the lookup table. */
    RTR0MEMOBJ                      hIoPortMemObj;
    /** Handle to the ring-3 mapping of the lookup and ring-3 registration table. */
    RTR0MEMOBJ                      hIoPortMapObj;
#ifdef VBOX_WITH_STATISTICS
    /** The size of the paIoPortStats allocation (in entries). */
    uint32_t                        cIoPortStatsAllocation;
    /** Prevents paIoPortStats from growing, set by IOMR0IoPortSyncStatisticsIndices(). */
    bool                            fIoPortStatsFrozen;
    /** I/O port lookup table.   */
    R0PTRTYPE(PIOMIOPORTSTATSENTRY) paIoPortStats;
    /** Handle to the allocation backing the I/O port statistics. */
    RTR0MEMOBJ                      hIoPortStatsMemObj;
    /** Handle to the ring-3 mapping of the I/O port statistics. */
    RTR0MEMOBJ                      hIoPortStatsMapObj;
#endif
    /** @} */

    /** @name MMIO
     * @{ */
    /** The higest ring-0 MMIO registration plus one. */
    uint32_t                        cMmioMax;
    /** The size of the paMmioRegs allocation (in entries). */
    uint32_t                        cMmioAlloc;
    /** MMIO registration table for ring-0.
     * There is a parallel table for ring-3, paMmioRing3Regs. */
    R0PTRTYPE(PIOMMMIOENTRYR0)      paMmioRegs;
    /** MMIO lookup table. */
    R0PTRTYPE(PIOMMMIOLOOKUPENTRY)  paMmioLookup;
    /** MMIO registration table for ring-3.
     * Also mapped to ring-3 as IOM::paMmioRegs. */
    R0PTRTYPE(PIOMMMIOENTRYR3)      paMmioRing3Regs;
    /** Handle to the allocation backing both the ring-0 and ring-3 registration
     * tables as well as the lookup table. */
    RTR0MEMOBJ                      hMmioMemObj;
    /** Handle to the ring-3 mapping of the lookup and ring-3 registration table. */
    RTR0MEMOBJ                      hMmioMapObj;
#ifdef VBOX_WITH_STATISTICS
    /** The size of the paMmioStats allocation (in entries). */
    uint32_t                        cMmioStatsAllocation;
    /* Prevents paMmioStats from growing, set by IOMR0MmioSyncStatisticsIndices(). */
    bool                            fMmioStatsFrozen;
    /** MMIO lookup table.   */
    R0PTRTYPE(PIOMMMIOSTATSENTRY)   paMmioStats;
    /** Handle to the allocation backing the MMIO statistics. */
    RTR0MEMOBJ                      hMmioStatsMemObj;
    /** Handle to the ring-3 mapping of the MMIO statistics. */
    RTR0MEMOBJ                      hMmioStatsMapObj;
#endif
    /** @} */

} IOMR0PERVM;


RT_C_DECLS_BEGIN

void                iomMmioFreeRange(PVMCC pVM, PIOMMMIORANGE pRange);
#ifdef IN_RING3
PIOMMMIOSTATS       iomR3MMIOStatsCreate(PVM pVM, RTGCPHYS GCPhys, const char *pszDesc);
DECLCALLBACK(void)  iomR3IoPortInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
void                iomR3IoPortRegStats(PVM pVM, PIOMIOPORTENTRYR3 pRegEntry);
DECLCALLBACK(void)  iomR3MmioInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
void                iomR3MmioRegStats(PVM pVM, PIOMMMIOENTRYR3 pRegEntry);
VBOXSTRICTRC        iomR3MmioCommitWorker(PVM pVM, PVMCPU pVCpu, PIOMMMIOENTRYR3 pRegEntry, RTGCPHYS offRegion); /* IOMAllMmioNew.cpp */
#endif /* IN_RING3 */
#ifdef IN_RING0
void                iomR0IoPortCleanupVM(PGVM pGVM);
void                iomR0IoPortInitPerVMData(PGVM pGVM);
void                iomR0MmioCleanupVM(PGVM pGVM);
void                iomR0MmioInitPerVMData(PGVM pGVM);
#endif
VBOXSTRICTRC        iomMmioCommonPfHandlerOld(PVMCC pVM, PVMCPUCC pVCpu, uint32_t uErrorCode, PCPUMCTXCORE pCtxCore,
                                              RTGCPHYS GCPhysFault, void *pvUser);

#ifndef IN_RING3
DECLEXPORT(FNPGMRZPHYSPFHANDLER)    iomMmioPfHandler;
DECLEXPORT(FNPGMRZPHYSPFHANDLER)    iomMmioPfHandlerNew;
#endif
PGM_ALL_CB2_PROTO(FNPGMPHYSHANDLER) iomMmioHandler;
PGM_ALL_CB2_PROTO(FNPGMPHYSHANDLER) iomMmioHandlerNew;

/* IOM locking helpers. */
#ifdef IOM_WITH_CRIT_SECT_RW
# define IOM_LOCK_EXCL(a_pVM)                   PDMCritSectRwEnterExcl(&(a_pVM)->iom.s.CritSect, VERR_SEM_BUSY)
# define IOM_UNLOCK_EXCL(a_pVM)                 do { PDMCritSectRwLeaveExcl(&(a_pVM)->iom.s.CritSect); } while (0)
# if 0 /* (in case needed for debugging) */
# define IOM_LOCK_SHARED_EX(a_pVM, a_rcBusy)    PDMCritSectRwEnterExcl(&(a_pVM)->iom.s.CritSect, (a_rcBusy))
# define IOM_UNLOCK_SHARED(a_pVM)               do { PDMCritSectRwLeaveExcl(&(a_pVM)->iom.s.CritSect); } while (0)
# define IOM_IS_SHARED_LOCK_OWNER(a_pVM)        PDMCritSectRwIsWriteOwner(&(a_pVM)->iom.s.CritSect)
# else
# define IOM_LOCK_SHARED_EX(a_pVM, a_rcBusy)    PDMCritSectRwEnterShared(&(a_pVM)->iom.s.CritSect, (a_rcBusy))
# define IOM_UNLOCK_SHARED(a_pVM)               do { PDMCritSectRwLeaveShared(&(a_pVM)->iom.s.CritSect); } while (0)
# define IOM_IS_SHARED_LOCK_OWNER(a_pVM)        PDMCritSectRwIsReadOwner(&(a_pVM)->iom.s.CritSect, true)
# endif
# define IOM_IS_EXCL_LOCK_OWNER(a_pVM)          PDMCritSectRwIsWriteOwner(&(a_pVM)->iom.s.CritSect)
#else
# define IOM_LOCK_EXCL(a_pVM)                   PDMCritSectEnter(&(a_pVM)->iom.s.CritSect, VERR_SEM_BUSY)
# define IOM_UNLOCK_EXCL(a_pVM)                 do { PDMCritSectLeave(&(a_pVM)->iom.s.CritSect); } while (0)
# define IOM_LOCK_SHARED_EX(a_pVM, a_rcBusy)    PDMCritSectEnter(&(a_pVM)->iom.s.CritSect, (a_rcBusy))
# define IOM_UNLOCK_SHARED(a_pVM)               do { PDMCritSectLeave(&(a_pVM)->iom.s.CritSect); } while (0)
# define IOM_IS_SHARED_LOCK_OWNER(a_pVM)        PDMCritSectIsOwner(&(a_pVM)->iom.s.CritSect)
# define IOM_IS_EXCL_LOCK_OWNER(a_pVM)          PDMCritSectIsOwner(&(a_pVM)->iom.s.CritSect)
#endif
#define IOM_LOCK_SHARED(a_pVM)                  IOM_LOCK_SHARED_EX(a_pVM, VERR_SEM_BUSY)


RT_C_DECLS_END


#ifdef IN_RING3

#endif

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IOMInternal_h */

