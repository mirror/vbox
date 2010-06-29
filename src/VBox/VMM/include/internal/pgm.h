/* $Id$ */
/** @file
 * PGM - Internal VMM header file.
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
 */

#ifndef ___PGM_include_internal_h
#define ___PGM_include_internal_h

#include <VBox/pgm.h>

VMMDECL(int)        PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys);
VMMDECL(int)        PGMPhysGCPtr2HCPhys(PVMCPU pVCpu, RTGCPTR GCPtr, PRTHCPHYS pHCPhys);
VMMDECL(int)        PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPtr2CCPtr(PVMCPU pVCpu, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPtr2CCPtrReadOnly(PVMCPU pVCpu, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMMDECL(int)        PGMPhysGCPhys2R3Ptr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTR3PTR pR3Ptr);
#ifdef VBOX_STRICT
VMMDECL(RTR3PTR)    PGMPhysGCPhys2R3PtrAssert(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange);
#endif
VMMDECL(int)        PGMPhysGCPtr2R3Ptr(PVMCPU pVCpu, RTGCPTR GCPtr, PRTR3PTR pR3Ptr);

#endif
