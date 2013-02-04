/* $Id$ */
/** @file
 * PGM - Internal VMM header file.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___PGM_include_internal_h
#define ___PGM_include_internal_h

#include <VBox/vmm/pgm.h>

/** @defgroup grp_pgm_int   Internals
 * @ingroup grp_pgm
 * @internal
 * @{
 */

/**
 * Page type.
 *
 * @remarks This enum has to fit in a 3-bit field (see PGMPAGE::u3Type).
 * @remarks This is used in the saved state, so changes to it requires bumping
 *          the saved state version.
 * @todo    So, convert to \#defines!
 */
typedef enum PGMPAGETYPE
{
    /** The usual invalid zero entry. */
    PGMPAGETYPE_INVALID = 0,
    /** RAM page. (RWX) */
    PGMPAGETYPE_RAM,
    /** MMIO2 page. (RWX) */
    PGMPAGETYPE_MMIO2,
    /** MMIO2 page aliased over an MMIO page. (RWX)
     * See PGMHandlerPhysicalPageAlias(). */
    PGMPAGETYPE_MMIO2_ALIAS_MMIO,
    /** Shadowed ROM. (RWX) */
    PGMPAGETYPE_ROM_SHADOW,
    /** ROM page. (R-X) */
    PGMPAGETYPE_ROM,
    /** MMIO page. (---) */
    PGMPAGETYPE_MMIO,
    /** End of valid entries. */
    PGMPAGETYPE_END
} PGMPAGETYPE;
AssertCompile(PGMPAGETYPE_END <= 7);

VMMDECL(PGMPAGETYPE) PGMPhysGetPageType(PVM pVM, RTGCPHYS GCPhys);

VMMDECL(int)        PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys);
VMMDECL(int)        PGMPhysGCPtr2HCPhys(PVMCPU pVCpu, RTGCPTR GCPtr, PRTHCPHYS pHCPhys);
VMMDECL(int)        PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPtr2CCPtr(PVMCPU pVCpu, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPtr2CCPtrReadOnly(PVMCPU pVCpu, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMMR3DECL(void)     PGMR3ResetNoMorePhysWritesFlag(PVM pVM);

/** @} */
#endif

