/** @file
 * SELM - The Selector Manager.
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

#ifndef ___VBox_selm_h
#define ___VBox_selm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>
#include <VBox/dis.h>
#include <VBox/dbgfsel.h>


__BEGIN_DECLS

/** @defgroup grp_selm      The Selector Monitor(/Manager) API
 * @{
 */

VMMDECL(RTSEL)      SELMGetTrap8Selector(PVM pVM);
VMMDECL(void)       SELMSetTrap8EIP(PVM pVM, uint32_t u32EIP);
VMMDECL(int)        SELMGetRing1Stack(PVM pVM, uint32_t *pSS, PRTGCPTR32 pEsp);
VMMDECL(RTGCPTR)    SELMGetGuestTSS(PVM pVM);
VMMDECL(RTSEL)      SELMGetHyperCS(PVM pVM);
VMMDECL(RTSEL)      SELMGetHyperCS64(PVM pVM);
VMMDECL(RTSEL)      SELMGetHyperDS(PVM pVM);
VMMDECL(RTSEL)      SELMGetHyperTSS(PVM pVM);
VMMDECL(RTSEL)      SELMGetHyperTSSTrap08(PVM pVM);
VMMDECL(RTRCPTR)    SELMGetHyperGDT(PVM pVM);
VMMDECL(int)        SELMGetTSSInfo(PVM pVM, PVMCPU pVCpu, PRTGCUINTPTR pGCPtrTss, PRTGCUINTPTR pcbTss, bool *pfCanHaveIOBitmap);
VMMDECL(RTGCPTR)    SELMToFlat(PVM pVM, DIS_SELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr);
VMMDECL(RTGCPTR)    SELMToFlatBySel(PVM pVM, RTSEL Sel, RTGCPTR Addr);
VMMDECL(void)       SELMShadowCR3Changed(PVM pVM, PVMCPU pVCpu);

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
/** Allow converting using Hypervisor GDT entries. */
#define SELMTOFLAT_FLAGS_HYPER      RT_BIT(10)
/** @} */

VMMDECL(int)        SELMToFlatEx(PVM pVM, DIS_SELREG SelReg, PCCPUMCTXCORE pCtxCore, RTGCPTR Addr, unsigned fFlags, PRTGCPTR ppvGC);
VMMDECL(int)        SELMToFlatBySelEx(PVM pVM, X86EFLAGS eflags, RTSEL Sel, RTGCPTR Addr, PCPUMSELREGHID pHiddenSel, unsigned fFlags, PRTGCPTR ppvGC, uint32_t *pcb);
VMMDECL(int)        SELMValidateAndConvertCSAddr(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, PCPUMSELREGHID pHiddenCSSel, RTGCPTR Addr, PRTGCPTR ppvFlat);
VMMDECL(int)        SELMValidateAndConvertCSAddrGCTrap(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, RTGCPTR Addr, PRTGCPTR ppvFlat, uint32_t *pcBits);
VMMDECL(DISCPUMODE) SELMGetCpuModeFromSelector(PVM pVM, X86EFLAGS eflags, RTSEL Sel, PCPUMSELREGHID pHiddenSel);
VMMDECL(int)        SELMGetLDTFromSel(PVM pVM, RTSEL SelLdt, PRTGCPTR ppvLdt, unsigned *pcbLimit);


#ifdef IN_RING3
/** @defgroup grp_selm_r3   The Selector Monitor(/Manager) API
 * @ingroup grp_selm
 * @{
 */
VMMR3DECL(int)      SELMR3Init(PVM pVM);
VMMR3DECL(int)      SELMR3InitFinalize(PVM pVM);
VMMR3DECL(void)     SELMR3Relocate(PVM pVM);
VMMR3DECL(int)      SELMR3Term(PVM pVM);
VMMR3DECL(void)     SELMR3Reset(PVM pVM);
VMMR3DECL(int)      SELMR3UpdateFromCPUM(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(int)      SELMR3SyncTSS(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(int)      SELMR3GetSelectorInfo(PVM pVM, PVMCPU pVCpu, RTSEL Sel, PDBGFSELINFO pSelInfo);
VMMR3DECL(int)      SELMR3GetShadowSelectorInfo(PVM pVM, RTSEL Sel, PDBGFSELINFO pSelInfo);
VMMR3DECL(void)     SELMR3DisableMonitoring(PVM pVM);
VMMR3DECL(void)     SELMR3DumpDescriptor(X86DESC  Desc, RTSEL Sel, const char *pszMsg);
VMMR3DECL(void)     SELMR3DumpHyperGDT(PVM pVM);
VMMR3DECL(void)     SELMR3DumpHyperLDT(PVM pVM);
VMMR3DECL(void)     SELMR3DumpGuestGDT(PVM pVM);
VMMR3DECL(void)     SELMR3DumpGuestLDT(PVM pVM);
VMMR3DECL(bool)     SELMR3CheckTSS(PVM pVM);
VMMR3DECL(int)      SELMR3DebugCheck(PVM pVM);
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
__END_DECLS

#endif

