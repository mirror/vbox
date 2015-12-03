/* $Id$ */
/** @file
 * VBoxVMM - Static dtrace probes.
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

provider vboxvmm
{
    probe em__state__changed(struct VMCPU *a_pVCpu, int a_enmOldState, int a_enmNewState, int a_rc);
    /*^^VMM-ALT-TP: "%d -> %d (rc=%d)", a_enmOldState, a_enmNewState, a_rc */

    probe em__state__unchanged(struct VMCPU *a_pVCpu, int a_enmState, int a_rc);
    /*^^VMM-ALT-TP: "%d (rc=%d)", a_enmState, a_rc */

    probe em__raw__run__pre(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /*^^VMM-ALT-TP: "%04x:%08llx", (a_pCtx)->cs, (a_pCtx)->rip */

    probe em__raw__run__ret(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, int a_rc);
    /*^^VMM-ALT-TP: "%04x:%08llx rc=%d", (a_pCtx)->cs, (a_pCtx)->rip, (a_rc) */

    probe em__ff__high(struct VMCPU *a_pVCpu, uint32_t a_fGlobal, uint32_t a_fLocal, int a_rc);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc) */

    probe em__ff__all(struct VMCPU *a_pVCpu, uint32_t a_fGlobal, uint32_t a_fLocal, int a_rc);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc) */

    probe em__ff__all__ret(struct VMCPU *a_pVCpu, int a_rc);
    /*^^VMM-ALT-TP: "%d", (a_rc) */

    probe em__ff__raw(struct VMCPU *a_pVCpu, uint32_t a_fGlobal, uint32_t a_fLocal);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x", (a_fGlobal), (a_fLocal) */

    probe em__ff__raw_ret(struct VMCPU *a_pVCpu, int a_rc);
    /*^^VMM-ALT-TP: "%d", (a_rc) */

    probe pdm__irq__get( struct VMCPU *a_pVCpu, uint32_t a_uTag, uint32_t a_idSource, uint32_t a_iIrq);
    probe pdm__irq__high(struct VMCPU *a_pVCpu, uint32_t a_uTag, uint32_t a_idSource);
    probe pdm__irq__low( struct VMCPU *a_pVCpu, uint32_t a_uTag, uint32_t a_idSource);
    probe pdm__irq__hilo(struct VMCPU *a_pVCpu, uint32_t a_uTag, uint32_t a_idSource);


    probe r0__gvmm__vm__created(void *a_pGVM, void *a_pVM, uint32_t a_Pid, void *a_hEMT0, uint32_t a_cCpus);
    probe r0__hmsvm__vmexit(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint64_t a_ExitCode, struct SVMVMCB *a_pVmcb);
    probe r0__hmvmx__vmexit(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint64_t a_ExitReason, uint64_t a_ExitQualification);
    probe r0__hmvmx__vmexit__noctx(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pIncompleteCtx, uint64_t a_ExitReason);

    probe r0__vmm__return__to__ring3__rc(struct VMCPU *a_pVCpu, struct CPUMCTX *p_Ctx, int a_rc);
    probe r0__vmm__return__to__ring3__hm(struct VMCPU *a_pVCpu, struct CPUMCTX *p_Ctx, int a_rc);


    /** \#DE - integer divide error.  */
    probe xcpt__de(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#DB - debug fault / trap.  */
    probe xcpt__db(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint64_t a_dr6);
    /** \#BP - breakpoint (INT3).  */
    probe xcpt__bp(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#OF - overflow (INTO).  */
    probe xcpt__of(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#BR - bound range exceeded (BOUND).  */
    probe xcpt__br(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#UD - undefined opcode.  */
    probe xcpt__ud(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#NM - FPU not avaible and more.  */
    probe xcpt__nm(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#DF - double fault.  */
    probe xcpt__df(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#TS - TSS related fault.  */
    probe xcpt__ts(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr);
    /** \#NP - segment not present.  */
    probe xcpt__np(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr);
    /** \#SS - stack segment fault.  */
    probe xcpt__ss(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr);
    /** \#GP - general protection fault.  */
    probe xcpt__gp(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr);
    /** \#PF - page fault.  */
    probe xcpt__pg(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr, uint64_t a_cr2);
    /** \#MF - math fault (FPU).  */
    probe xcpt__mf(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#AC - alignment check.  */
    probe xcpt__ac(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#XF - SIMD floating point exception.  */
    probe xcpt__xf(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#VE - virtualization exception.  */
    probe xcpt__ve(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** \#SX - security exception.  */
    probe xcpt__sx(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_uErr);

    /** Software interrupt (INT XXh). */
    probe int__software(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iInterrupt);
    /** Hardware interrupt being dispatched. */
    probe int__hardware(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iInterrupt, uint32_t a_uTag, uint32_t a_idSource);

    /** Exit - Task switch. */
    probe exit__task__switch(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - HALT instruction.
     * @todo not yet implemented. */
    probe exit__halt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - MWAIT instruction. */
    probe exit__mwait(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - MONITOR instruction. */
    probe exit__monitor(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - CPUID instruction (missing stuff in raw-mode). */
    probe exit__cpuid(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t uLeaf, uint32_t uSubLeaf);
    /** Exit - INVD instruction.  */
    probe exit__invd(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - WBINVD instruction. */
    probe exit__wbinvd(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - INVLPG instruction. */
    probe exit__invlpg(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDTSC instruction.  */
    probe exit__rdtsc(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDTSCP instruction. */
    probe exit__rdtscp(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDPMC instruction.  */
    probe exit__rdpmc(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDMSR instruction.  */
    probe exit__rdmsr(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_idMsr);
    /** Exit - WRMSR instruction.  */
    probe exit__wrmsr(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint32_t a_idMsr, uint64_t a_uValue);
    /** Exit - CRx read instruction (missing smsw in raw-mode,
     *  and reads in general in VT-x). */
    probe exit__crx__read(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iReg);
    /** Exit - CRx write instruction. */
    probe exit__crx__write(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iReg);
    /** Exit - DRx read instruction. */
    probe exit__drx__read(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iReg);
    /** Exit - DRx write instruction. */
    probe exit__drx__write(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx, uint8_t a_iReg);
    /** Exit - PAUSE instruction (not in raw-mode). */
    probe exit__pause(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - XSETBV instruction. */
    probe exit__xsetbv(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - SIDT instruction.  */
    probe exit__sidt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - LIDT instruction. */
    probe exit__lidt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - SGDT instruction. */
    probe exit__sgdt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - LGDT instruction. */
    probe exit__lgdt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - SLDT instruction. */
    probe exit__sldt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - LLDT instruction. */
    probe exit__lldt(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - STR instruction. */
    probe exit__str(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - LTR instruction. */
    probe exit__ltr(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - GETSEC instruction. */
    probe exit__getsec(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RSM instruction. */
    probe exit__rsm(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDRAND instruction. */
    probe exit__rdrand(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - RDSEED instruction. */
    probe exit__rdseed(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - XSAVES instruction. */
    probe exit__xsaves(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - XRSTORS instruction.  */
    probe exit__xrstors(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VMCALL (intel) or VMMCALL (AMD) instruction. */
    probe exit__vmm__call(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);

    /** Exit - VT-x VMCLEAR instruction. */
    probe exit__vmx__vmclear(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMLAUNCH instruction. */
    probe exit__vmx__vmlaunch(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMPTRLD instruction. */
    probe exit__vmx__vmptrld(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMPTRST instruction. */
    probe exit__vmx__vmptrst(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMREAD instruction. */
    probe exit__vmx__vmread(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMRESUME instruction. */
    probe exit__vmx__vmresume(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMWRITE instruction. */
    probe exit__vmx__vmwrite(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMXOFF instruction. */
    probe exit__vmx__vmxoff(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMXON instruction. */
    probe exit__vmx__vmxon(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x VMFUNC instruction. */
    probe exit__vmx__vmfunc(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x INVEPT instruction. */
    probe exit__vmx__invept(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x INVVPID instruction. */
    probe exit__vmx__invvpid(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x INVPCID instruction. */
    probe exit__vmx__invpcid(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x EPT violation. */
    probe exit__vmx__ept__violation(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x EPT misconfiguration. */
    probe exit__vmx__ept__misconfig(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x Virtual APIC page access. */
    probe exit__vmx__vapic__access(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - VT-x Virtual APIC page write needing virtualizing. */
    probe exit__vmx__vapic__write(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);

    /** Exit - AMD-V VMRUN instruction. */
    probe exit__svm__vmrun(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - AMD-V VMLOAD instruction. */
    probe exit__svm__vmload(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - AMD-V VMSAVE instruction. */
    probe exit__svm__vmsave(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - AMD-V STGI instruction. */
    probe exit__svm__stgi(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
    /** Exit - AMD-V CLGI instruction. */
    probe exit__svm__clgi(struct VMCPU *a_pVCpu, struct CPUMCTX *a_pCtx);
};

#pragma D attributes Evolving/Evolving/Common provider vboxvmm provider
#pragma D attributes Private/Private/Unknown  provider vboxvmm module
#pragma D attributes Private/Private/Unknown  provider vboxvmm function
#pragma D attributes Evolving/Evolving/Common provider vboxvmm name
#pragma D attributes Evolving/Evolving/Common provider vboxvmm args

