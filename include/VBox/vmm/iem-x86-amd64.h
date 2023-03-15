/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_vmm_iem_x86_amd64_h
#define VBOX_INCLUDED_vmm_iem_x86_amd64_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
# include <VBox/vmm/hm_vmx.h>
#endif


RT_C_DECLS_BEGIN

/** @addtogroup grp_iem
 * @{ */


/** @name IEMTARGETCPU_XXX - IEM target CPU specification.
 *
 * This is a gross simpliciation of CPUMMICROARCH for dealing with really old
 * CPUs which didn't have much in the way of hinting at supported instructions
 * and features.  This slowly changes with the introduction of CPUID with the
 * Intel Pentium.
 *
 * @{
 */
/** The dynamic target CPU mode is for getting thru the BIOS and then use
 * the debugger or modifying instruction behaviour (e.g. HLT) to switch to a
 * different target CPU. */
#define IEMTARGETCPU_DYNAMIC    UINT32_C(0)
/** Intel 8086/8088.  */
#define IEMTARGETCPU_8086       UINT32_C(1)
/** NEC V20/V30.
 * @remarks must be between 8086 and 80186. */
#define IEMTARGETCPU_V20        UINT32_C(2)
/** Intel 80186/80188.  */
#define IEMTARGETCPU_186        UINT32_C(3)
/** Intel 80286.  */
#define IEMTARGETCPU_286        UINT32_C(4)
/** Intel 80386.  */
#define IEMTARGETCPU_386        UINT32_C(5)
/** Intel 80486.  */
#define IEMTARGETCPU_486        UINT32_C(6)
/** Intel Pentium .  */
#define IEMTARGETCPU_PENTIUM    UINT32_C(7)
/** Intel PentiumPro.  */
#define IEMTARGETCPU_PPRO       UINT32_C(8)
/** A reasonably current CPU, probably newer than the pentium pro when it comes
 * to the feature set and behaviour.  Generally the CPUID info and CPU vendor
 * dicates the behaviour here. */
#define IEMTARGETCPU_CURRENT    UINT32_C(9)
/** @} */


/** The CPUMCTX_EXTRN_XXX mask required to be cleared when interpreting anything.
 * IEM will ASSUME the caller of IEM APIs has ensured these are already present. */
#define IEM_CPUMCTX_EXTRN_MUST_MASK                (  CPUMCTX_EXTRN_GPRS_MASK \
                                                    | CPUMCTX_EXTRN_RIP \
                                                    | CPUMCTX_EXTRN_RFLAGS \
                                                    | CPUMCTX_EXTRN_SS \
                                                    | CPUMCTX_EXTRN_CS \
                                                    | CPUMCTX_EXTRN_CR0 \
                                                    | CPUMCTX_EXTRN_CR3 \
                                                    | CPUMCTX_EXTRN_CR4 \
                                                    | CPUMCTX_EXTRN_APIC_TPR \
                                                    | CPUMCTX_EXTRN_EFER \
                                                    | CPUMCTX_EXTRN_DR7 )
/** The CPUMCTX_EXTRN_XXX mask needed when injecting an exception/interrupt.
 * IEM will import missing bits, callers are encouraged to make these registers
 * available prior to injection calls if fetching state anyway.  */
#define IEM_CPUMCTX_EXTRN_XCPT_MASK                (  IEM_CPUMCTX_EXTRN_MUST_MASK \
                                                    | CPUMCTX_EXTRN_CR2 \
                                                    | CPUMCTX_EXTRN_SREG_MASK \
                                                    | CPUMCTX_EXTRN_TABLE_MASK )
/** The CPUMCTX_EXTRN_XXX mask required to be cleared when calling any
 * IEMExecDecoded API not using memory.  IEM will ASSUME the caller of IEM
 * APIs has ensured these are already present.
 * @note ASSUMES execution engine has checked for instruction breakpoints
 *       during decoding. */
#define IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK (  CPUMCTX_EXTRN_RIP \
                                                    | CPUMCTX_EXTRN_RFLAGS \
                                                    | CPUMCTX_EXTRN_SS   /* for CPL */ \
                                                    | CPUMCTX_EXTRN_CS   /* for mode */ \
                                                    | CPUMCTX_EXTRN_CR0  /* for mode */ \
                                                    | CPUMCTX_EXTRN_EFER /* for mode */ )
/** The CPUMCTX_EXTRN_XXX mask required to be cleared when calling any
 * IEMExecDecoded API using memory.  IEM will ASSUME the caller of IEM
 * APIs has ensured these are already present.
 * @note ASSUMES execution engine has checked for instruction breakpoints
 *       during decoding. */
#define IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK    (  IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK \
                                                    | CPUMCTX_EXTRN_CR3 /* for page tables */ \
                                                    | CPUMCTX_EXTRN_CR4 /* for mode paging mode */ \
                                                    | CPUMCTX_EXTRN_DR7 /* for memory breakpoints */ )

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/** The CPUMCTX_EXTRN_XXX mask needed when calling IEMExecDecodedVmlaunchVmresume().
 * IEM will ASSUME the caller has ensured these are already present. */
# define IEM_CPUMCTX_EXTRN_VMX_VMENTRY_MASK        (  IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK \
                                                    | CPUMCTX_EXTRN_CR2 \
                                                    | CPUMCTX_EXTRN_HWVIRT )

/** The CPUMCTX_EXTRN_XXX mask that the IEM VM-exit code will import on-demand when
 *  needed, primarily because there are several IEM VM-exit interface functions and
 *  some of which may not cause a VM-exit at all.
 *
 *  This is currently unused, but keeping it here in case we can get away a bit more
 *  fine-grained state handling.
 *
 *  @note Update HM_CHANGED_VMX_VMEXIT_MASK if something here changes. */
# define IEM_CPUMCTX_EXTRN_VMX_VMEXIT_MASK         (  CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4 \
                                                    | CPUMCTX_EXTRN_DR7 | CPUMCTX_EXTRN_DR6 \
                                                    | CPUMCTX_EXTRN_EFER \
                                                    | CPUMCTX_EXTRN_SYSENTER_MSRS \
                                                    | CPUMCTX_EXTRN_OTHER_MSRS    /* for PAT MSR */ \
                                                    | CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RSP | CPUMCTX_EXTRN_RFLAGS \
                                                    | CPUMCTX_EXTRN_SREG_MASK \
                                                    | CPUMCTX_EXTRN_TR \
                                                    | CPUMCTX_EXTRN_LDTR | CPUMCTX_EXTRN_GDTR | CPUMCTX_EXTRN_IDTR \
                                                    | CPUMCTX_EXTRN_HWVIRT )
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/** The CPUMCTX_EXTRN_XXX mask needed when calling IEMExecSvmVmexit().
 * IEM will ASSUME the caller has ensured these are already present. */
# define IEM_CPUMCTX_EXTRN_SVM_VMEXIT_MASK         (  CPUMCTX_EXTRN_RSP \
                                                    | CPUMCTX_EXTRN_RAX \
                                                    | CPUMCTX_EXTRN_RIP \
                                                    | CPUMCTX_EXTRN_RFLAGS \
                                                    | CPUMCTX_EXTRN_CS \
                                                    | CPUMCTX_EXTRN_SS \
                                                    | CPUMCTX_EXTRN_DS \
                                                    | CPUMCTX_EXTRN_ES \
                                                    | CPUMCTX_EXTRN_GDTR \
                                                    | CPUMCTX_EXTRN_IDTR \
                                                    | CPUMCTX_EXTRN_CR_MASK \
                                                    | CPUMCTX_EXTRN_EFER \
                                                    | CPUMCTX_EXTRN_DR6 \
                                                    | CPUMCTX_EXTRN_DR7 \
                                                    | CPUMCTX_EXTRN_OTHER_MSRS \
                                                    | CPUMCTX_EXTRN_HWVIRT \
                                                    | CPUMCTX_EXTRN_APIC_TPR \
                                                    | CPUMCTX_EXTRN_HM_SVM_HWVIRT_VIRQ)

/** The CPUMCTX_EXTRN_XXX mask needed when calling IEMExecDecodedVmrun().
 *  IEM will ASSUME the caller has ensured these are already present. */
# define IEM_CPUMCTX_EXTRN_SVM_VMRUN_MASK          IEM_CPUMCTX_EXTRN_SVM_VMEXIT_MASK
#endif

/** @name Given Instruction Interpreters
 * @{ */
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoWrite(PVMCPUCC pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                 bool fRepPrefix, uint8_t cbInstr, uint8_t iEffSeg, bool fIoChecked);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoRead(PVMCPUCC pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                bool fRepPrefix, uint8_t cbInstr, bool fIoChecked);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedOut(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t u16Port, bool fImm, uint8_t cbReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedIn(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t u16Port, bool fImm, uint8_t cbReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxWrite(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iCrReg, uint8_t iGReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxRead(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iCrReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovDRxWrite(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iDrReg, uint8_t iGReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovDRxRead(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iDrReg);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedClts(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedLmsw(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uValue, RTGCPTR GCPtrEffDst);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedXsetbv(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedWbinvd(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvd(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvlpg(PVMCPUCC pVCpu,  uint8_t cbInstr, RTGCPTR GCPtrPage);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvpcid(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPTR GCPtrDesc,
                                                  uint64_t uType);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedCpuid(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdpmc(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdtsc(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdtscp(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdmsr(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedWrmsr(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMonitor(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMwait(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedHlt(PVMCPUCC pVCpu, uint8_t cbInstr);

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedClgi(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedStgi(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmload(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmsave(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvlpga(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmrun(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecSvmVmexit(PVMCPUCC pVCpu, uint64_t uExitCode, uint64_t uExitInfo1, uint64_t uExitInfo2);
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
VMM_INT_DECL(void)          IEMReadVmxVmcsField(PCVMXVVMCS pVmcs, uint64_t u64VmcsField, uint64_t *pu64Dst);
VMM_INT_DECL(void)          IEMWriteVmxVmcsField(PVMXVVMCS pVmcs, uint64_t u64VmcsField, uint64_t u64Val);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVirtApicAccessMsr(PVMCPUCC pVCpu, uint32_t idMsr, uint64_t *pu64Val, bool fWrite);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitApicWrite(PVMCPUCC pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitPreemptTimer(PVMCPUCC pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitExtInt(PVMCPUCC pVCpu, uint8_t uVector, bool fIntPending);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitXcpt(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo, PCVMXVEXITEVENTINFO pExitEventInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitXcptNmi(PVMCPUCC pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitTripleFault(PVMCPUCC pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitStartupIpi(PVMCPUCC pVCpu, uint8_t uVector);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitInstrWithInfo(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitInstr(PVMCPUCC pVCpu, uint32_t uExitReason, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitTrapLike(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitTaskSwitch(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo, PCVMXVEXITEVENTINFO pExitEventInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitApicAccess(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo, PCVMXVEXITEVENTINFO pExitEventInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexit(PVMCPUCC pVCpu, uint32_t uExitReason, uint64_t uExitQual);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmread(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmwrite(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmptrld(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmptrst(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmclear(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmlaunchVmresume(PVMCPUCC pVCpu, uint8_t cbInstr, VMXINSTRID uInstrId);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmxon(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedVmxoff(PVMCPUCC pVCpu, uint8_t cbInstr);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvvpid(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
# ifdef VBOX_WITH_NESTED_HWVIRT_VMX_EPT
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvept(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitEptViolation(PVMCPUCC pVCpu, PCVMXVEXITINFO pExitInfo, PCVMXVEXITEVENTINFO pExitEventInfo);
VMM_INT_DECL(VBOXSTRICTRC)  IEMExecVmxVmexitEptMisconfig(PVMCPUCC pVCpu, RTGCPHYS GCPhysAddr, PCVMXVEXITEVENTINFO pExitEventInfo);
# endif
#endif
/** @}  */

/** @defgroup grp_iem_r0     The IEM Host Context Ring-0 API.
 * @{
 */
VMMR0_INT_DECL(int) IEMR0InitVM(PGVM pGVM);
/** @} */

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_iem_x86_amd64_h */

