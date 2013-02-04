/* $Id$ */
/** @file
 * STAM Internal Header.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___STAMInternal_h
#define ___STAMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/vmm/gmm.h>
#include <iprt/semaphore.h>



RT_C_DECLS_BEGIN

/** @defgroup grp_stam_int   Internals
 * @ingroup grp_stam
 * @internal
 * @{
 */

/**
 * Sample descriptor.
 */
typedef struct STAMDESC
{
    /** Pointer to the next sample. */
    struct STAMDESC    *pNext;
    /** Sample name. */
    const char         *pszName;
    /** Sample type. */
    STAMTYPE            enmType;
    /** Visibility type. */
    STAMVISIBILITY      enmVisibility;
    /** Pointer to the sample data. */
    union STAMDESCSAMPLEDATA
    {
        /** Counter. */
        PSTAMCOUNTER    pCounter;
        /** Profile. */
        PSTAMPROFILE    pProfile;
        /** Advanced profile. */
        PSTAMPROFILEADV pProfileAdv;
        /** Ratio, unsigned 32-bit. */
        PSTAMRATIOU32   pRatioU32;
        /** unsigned 8-bit. */
        uint8_t        *pu8;
        /** unsigned 16-bit. */
        uint16_t       *pu16;
        /** unsigned 32-bit. */
        uint32_t       *pu32;
        /** unsigned 64-bit. */
        uint64_t       *pu64;
        /** Simple void pointer. */
        void           *pv;
        /** Boolean. */
        bool           *pf;
        /** */
        struct STAMDESCSAMPLEDATACALLBACKS
        {
            /** The same pointer. */
            void                   *pvSample;
            /** Pointer to the reset callback. */
            PFNSTAMR3CALLBACKRESET  pfnReset;
            /** Pointer to the print callback. */
            PFNSTAMR3CALLBACKPRINT  pfnPrint;
        }               Callback;
    }                   u;
    /** Unit. */
    STAMUNIT            enmUnit;
    /** Description. */
    const char         *pszDesc;
} STAMDESC;
/** Pointer to sample descriptor. */
typedef STAMDESC        *PSTAMDESC;
/** Pointer to const sample descriptor. */
typedef const STAMDESC  *PCSTAMDESC;


/**
 * STAM data kept in the UVM.
 */
typedef struct STAMUSERPERVM
{
    /** Pointer to the first sample. */
    R3PTRTYPE(PSTAMDESC)    pHead;
    /** RW Lock for the list. */
    RTSEMRW                 RWSem;

    /** The copy of the GVMM statistics. */
    GVMMSTATS               GVMMStats;
    /** The number of registered host CPU leaves. */
    uint32_t                cRegisteredHostCpus;

    /** Explicit alignment padding. */
    uint32_t                uAlignment;
    /** The copy of the GMM statistics. */
    GMMSTATS                GMMStats;
} STAMUSERPERVM;
AssertCompileMemberAlignment(STAMUSERPERVM, GMMStats, 8);

/** Pointer to the STAM data kept in the UVM. */
typedef STAMUSERPERVM *PSTAMUSERPERVM;


/** Locks the sample descriptors for reading. */
#define STAM_LOCK_RD(pUVM)      do { int rcSem = RTSemRWRequestRead(pUVM->stam.s.RWSem, RT_INDEFINITE_WAIT);  AssertRC(rcSem); } while (0)
/** Locks the sample descriptors for writing. */
#define STAM_LOCK_WR(pUVM)      do { int rcSem = RTSemRWRequestWrite(pUVM->stam.s.RWSem, RT_INDEFINITE_WAIT); AssertRC(rcSem); } while (0)
/** UnLocks the sample descriptors after reading. */
#define STAM_UNLOCK_RD(pUVM)    do { int rcSem = RTSemRWReleaseRead(pUVM->stam.s.RWSem);  AssertRC(rcSem); } while (0)
/** UnLocks the sample descriptors after writing. */
#define STAM_UNLOCK_WR(pUVM)    do { int rcSem = RTSemRWReleaseWrite(pUVM->stam.s.RWSem); AssertRC(rcSem); } while (0)

/** @} */

RT_C_DECLS_END

#endif
