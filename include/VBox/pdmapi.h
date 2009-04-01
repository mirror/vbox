/** @file
 * PDM - Pluggable Device Manager, Core API.
 *
 * The 'Core API' has been put in a different header because everyone
 * is currently including pdm.h. So, pdm.h is for including all of the
 * PDM stuff, while pdmapi.h is for the core stuff.
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

#ifndef ___VBox_pdmapi_h
#define ___VBox_pdmapi_h

#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_pdm       The Pluggable Device Manager API
 * @{
 */

VMMDECL(int)    PDMGetInterrupt(PVM pVM, uint8_t *pu8Interrupt);
VMMDECL(int)    PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);
VMMDECL(int)    PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);
VMMDECL(int)    PDMApicHasPendingIrq(PVM pVM, bool *pfPending);
VMMDECL(int)    PDMApicSetBase(PVM pVM, uint64_t u64Base);
VMMDECL(int)    PDMApicGetBase(PVM pVM, uint64_t *pu64Base);
VMMDECL(int)    PDMApicSetTPR(PVM pVM, uint8_t u8TPR);
VMMDECL(int)    PDMApicGetTPR(PVM pVM, uint8_t *pu8TPR, bool *pfPending);
VMMDECL(int)    PDMApicWriteMSR(PVM pVM, VMCPUID iCpu, uint32_t u32Reg, uint64_t u64Value);
VMMDECL(int)    PDMApicReadMSR(PVM pVM, VMCPUID iCpu, uint32_t u32Reg, uint64_t *pu64Value);
VMMDECL(int)    PDMVMMDevHeapR3ToGCPhys(PVM pVM, RTR3PTR pv, RTGCPHYS *pGCPhys);

#ifdef IN_RING3
/** @defgroup grp_pdm_r3    The PDM Host Context Ring-3 API
 * @ingroup grp_pdm
 * @{
 */

VMMR3DECL(int)  PDMR3InitUVM(PUVM pUVM);
VMMR3DECL(int)  PDMR3LdrLoadVMMR0U(PUVM pUVM);
VMMR3DECL(int)  PDMR3Init(PVM pVM);
VMMR3DECL(void) PDMR3PowerOn(PVM pVM);
VMMR3DECL(void) PDMR3Reset(PVM pVM);
VMMR3DECL(void) PDMR3Suspend(PVM pVM);
VMMR3DECL(void) PDMR3Resume(PVM pVM);
VMMR3DECL(void) PDMR3PowerOff(PVM pVM);
VMMR3DECL(void) PDMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(int)  PDMR3Term(PVM pVM);
VMMR3DECL(void) PDMR3TermUVM(PUVM pUVM);

/**
 * Module enumeration callback function.
 *
 * @returns VBox status.
 *          Failure will stop the search and return the return code.
 *          Warnings will be ignored and not returned.
 * @param   pVM             VM Handle.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name. (short and unique)
 * @param   ImageBase       Address where to executable image is loaded.
 * @param   cbImage         Size of the executable image.
 * @param   fRC             Set if raw-mode context, clear if host context.
 * @param   pvArg           User argument.
 */
typedef DECLCALLBACK(int) FNPDMR3ENUM(PVM pVM, const char *pszFilename, const char *pszName, RTUINTPTR ImageBase, size_t cbImage, bool fRC, void *pvArg);
/** Pointer to a FNPDMR3ENUM() function. */
typedef FNPDMR3ENUM *PFNPDMR3ENUM;
VMMR3DECL(int)  PDMR3LdrEnumModules(PVM pVM, PFNPDMR3ENUM pfnCallback, void *pvArg);
VMMR3DECL(void) PDMR3LdrRelocateU(PUVM pUVM, RTGCINTPTR offDelta);
VMMR3DECL(int)  PDMR3LdrGetSymbolR3(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue);
VMMR3DECL(int)  PDMR3LdrGetSymbolR0(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue);
VMMR3DECL(int)  PDMR3LdrGetSymbolR0Lazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue);
VMMR3DECL(int)  PDMR3LdrLoadRC(PVM pVM, const char *pszFilename, const char *pszName);
VMMR3DECL(int)  PDMR3LdrGetSymbolRC(PVM pVM, const char *pszModule, const char *pszSymbol, PRTRCPTR pRCPtrValue);
VMMR3DECL(int)  PDMR3LdrGetSymbolRCLazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTRCPTR pRCPtrValue);
VMMR3DECL(int)  PDMR3LdrQueryRCModFromPC(PVM pVM, RTRCPTR uPC,
                                         char *pszModName,  size_t cchModName,  PRTRCPTR pMod,
                                         char *pszNearSym1, size_t cchNearSym1, PRTRCPTR pNearSym1,
                                         char *pszNearSym2, size_t cchNearSym2, PRTRCPTR pNearSym2);

VMMR3DECL(int)  PDMR3QueryDevice(PVM pVM, const char *pszDevice, unsigned iInstance, PPPDMIBASE ppBase);
VMMR3DECL(int)  PDMR3QueryDeviceLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);
VMMR3DECL(int)  PDMR3QueryLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);
VMMR3DECL(int)  PDMR3DeviceAttach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);
VMMR3DECL(int)  PDMR3DeviceDetach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun);
VMMR3DECL(void) PDMR3DmaRun(PVM pVM);
VMMR3DECL(int)  PDMR3LockCall(PVM pVM);
VMMR3DECL(int)  PDMR3RegisterVMMDevHeap(PVM pVM, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize);
VMMR3DECL(int)  PDMR3VMMDevHeapAlloc(PVM pVM, unsigned cbSize, RTR3PTR *ppv);
VMMR3DECL(int)  PDMR3VMMDevHeapFree(PVM pVM, RTR3PTR pv);
VMMR3DECL(int)  PDMR3UnregisterVMMDevHeap(PVM pVM, RTGCPHYS GCPhys);

/** @} */
#endif


#ifdef IN_RC
/** @defgroup grp_pdm_gc    The PDM Guest Context API
 * @ingroup grp_pdm
 * @{
 */
/** @} */
#endif

__END_DECLS

/** @} */

#endif

