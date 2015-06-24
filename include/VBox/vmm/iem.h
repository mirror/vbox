/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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

#ifndef ___VBox_vmm_iem_h
#define ___VBox_vmm_iem_h

#include <VBox/types.h>
#include <VBox/vmm/trpm.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_iem       The Interpreted Execution Manager API.
 * @{
 */


/**
 * Operand or addressing mode.
 */
typedef enum IEMMODE
{
    IEMMODE_16BIT = 0,
    IEMMODE_32BIT,
    IEMMODE_64BIT
} IEMMODE;
AssertCompileSize(IEMMODE, 4);


/** @name IEM status codes.
 *
 * Not quite sure how this will play out in the end, just aliasing safe status
 * codes for now.
 *
 * @{ */
#define VINF_IEM_RAISED_XCPT    VINF_EM_RESCHEDULE
/** @} */


VMMDECL(VBOXSTRICTRC)       IEMExecOne(PVMCPU pVCpu);
VMMDECL(VBOXSTRICTRC)       IEMExecOneEx(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t *pcbWritten);
VMMDECL(VBOXSTRICTRC)       IEMExecOneWithPrefetchedByPC(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint64_t OpcodeBytesPC,
                                                         const void *pvOpcodeBytes, size_t cbOpcodeBytes);
VMMDECL(VBOXSTRICTRC)       IEMExecOneBypassEx(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t *pcbWritten);
VMMDECL(VBOXSTRICTRC)       IEMExecOneBypassWithPrefetchedByPC(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint64_t OpcodeBytesPC,
                                                               const void *pvOpcodeBytes, size_t cbOpcodeBytes);
VMMDECL(VBOXSTRICTRC)       IEMExecLots(PVMCPU pVCpu);
VMMDECL(VBOXSTRICTRC)       IEMInjectTrpmEvent(PVMCPU pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  IEMInjectTrap(PVMCPU pVCpu, uint8_t u8TrapNo, TRPMEVENT enmType, uint16_t uErrCode, RTGCPTR uCr2,
                                          uint8_t cbInstr);

VMM_INT_DECL(int)           IEMBreakpointSet(PVM pVM, RTGCPTR GCPtrBp);
VMM_INT_DECL(int)           IEMBreakpointClear(PVM pVM, RTGCPTR GCPtrBp);

/** @name Given Instruction Interpreters
 * @{ */
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoWrite(PVMCPU pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                 bool fRepPrefix, uint8_t cbInstr, uint8_t iEffSeg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoRead(PVMCPU pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                bool fRepPrefix, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxWrite(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iCrReg, uint8_t iGReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxRead(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iCrReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedClts(PVMCPU pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedLmsw(PVMCPU pVCpu, uint8_t cbInstr, uint16_t uValue);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedXsetbv(PVMCPU pVCpu, uint8_t cbInstr);
/** @}  */

#if defined(IEM_VERIFICATION_MODE) && defined(IN_RING3)
VMM_INT_DECL(void)   IEMNotifyMMIORead(PVM pVM, RTGCPHYS GCPhys, size_t cbValue);
VMM_INT_DECL(void)   IEMNotifyMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue);
VMM_INT_DECL(void)   IEMNotifyIOPortRead(PVM pVM, RTIOPORT Port, size_t cbValue);
VMM_INT_DECL(void)   IEMNotifyIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue);
VMM_INT_DECL(void)   IEMNotifyIOPortReadString(PVM pVM, RTIOPORT Port, void *pvDst, RTGCUINTREG cTransfers, size_t cbValue);
VMM_INT_DECL(void)   IEMNotifyIOPortWriteString(PVM pVM, RTIOPORT Port, void const *pvSrc, RTGCUINTREG cTransfers, size_t cbValue);
#endif


/** @defgroup grp_iem_r3     The IEM Host Context Ring-3 API.
 * @{
 */
VMMR3DECL(int)      IEMR3Init(PVM pVM);
VMMR3DECL(int)      IEMR3Term(PVM pVM);
VMMR3DECL(void)     IEMR3Relocate(PVM pVM);
VMMR3_INT_DECL(VBOXSTRICTRC) IEMR3DoPendingAction(PVMCPU pVCpu, VBOXSTRICTRC rcStrict);
/** @} */

/** @} */

RT_C_DECLS_END

#endif

