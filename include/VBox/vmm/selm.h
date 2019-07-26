/** @file
 * SELM - The Selector Manager.
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

#ifndef VBOX_INCLUDED_vmm_selm_h
#define VBOX_INCLUDED_vmm_selm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <iprt/x86.h>
#include <VBox/dis.h>
#include <VBox/vmm/dbgfsel.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_selm      The Selector Monitor(/Manager) API
 * @ingroup grp_vmm
 * @{
 */

VMMDECL(int)            SELMGetTSSInfo(PVM pVM, PVMCPU pVCpu, PRTGCUINTPTR pGCPtrTss, PRTGCUINTPTR pcbTss, bool *pfCanHaveIOBitmap);
VMMDECL(RTGCPTR)        SELMToFlat(PVM pVM, DISSELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr);
VMMDECL(RTGCPTR)        SELMToFlatBySel(PVM pVM, RTSEL Sel, RTGCPTR Addr);

/** Flags for SELMToFlatEx().
 * @{ */
/** Don't check the RPL,DPL or CPL. */
#define SELMTOFLAT_FLAGS_NO_PL      RT_BIT(8)
/** Flags contains CPL information. */
#define SELMTOFLAT_FLAGS_HAVE_CPL   RT_BIT(9)
/** CPL is 3. */
#define SELMTOFLAT_FLAGS_CPL3       3
/** CPL is 2. */
#define SELMTOFLAT_FLAGS_CPL2       2
/** CPL is 1. */
#define SELMTOFLAT_FLAGS_CPL1       1
/** CPL is 0. */
#define SELMTOFLAT_FLAGS_CPL0       0
/** Get the CPL from the flags. */
#define SELMTOFLAT_FLAGS_CPL(fFlags)    ((fFlags) & X86_SEL_RPL)
#define SELMTOFLAT_FLAGS_HYPER      RT_BIT(10)
/** @} */

VMMDECL(int)            SELMToFlatEx(PVMCPU pVCpu, DISSELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr, uint32_t fFlags,
                                     PRTGCPTR ppvGC);
VMMDECL(int)            SELMValidateAndConvertCSAddr(PVMCPU pVCpu, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS,
                                                     PCPUMSELREG pSRegCS, RTGCPTR Addr, PRTGCPTR ppvFlat);
#ifdef VBOX_WITH_RAW_MODE
VMM_INT_DECL(void)      SELMLoadHiddenSelectorReg(PVMCPU pVCpu, PCCPUMCTX pCtx, PCPUMSELREG pSReg);
#endif


#ifdef IN_RING3
/** @defgroup grp_selm_r3   The SELM ring-3 Context API
 * @{
 */
VMMR3DECL(int)          SELMR3Init(PVM pVM);
VMMR3DECL(void)         SELMR3Relocate(PVM pVM);
VMMR3DECL(int)          SELMR3Term(PVM pVM);
VMMR3DECL(void)         SELMR3Reset(PVM pVM);
VMMR3DECL(int)          SELMR3GetSelectorInfo(PVMCPU pVCpu, RTSEL Sel, PDBGFSELINFO pSelInfo);
VMMR3DECL(void)         SELMR3DumpDescriptor(X86DESC  Desc, RTSEL Sel, const char *pszMsg);
VMMR3DECL(void)         SELMR3DumpGuestGDT(PVM pVM);
VMMR3DECL(void)         SELMR3DumpGuestLDT(PVM pVM);

/** @def SELMR3_DEBUG_CHECK
 * Invokes SELMR3DebugCheck in stricts builds. */
# ifdef VBOX_STRICT
#  define SELMR3_DEBUG_CHECK(pVM)    SELMR3DebugCheck(pVM)
# else
#  define SELMR3_DEBUG_CHECK(pVM)    do { } while (0)
# endif
/** @} */
#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_selm_h */

