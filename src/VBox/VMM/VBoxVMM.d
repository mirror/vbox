/* $Id$ */
/** @file
 * VBoxVMM - Static dtrace probes.
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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

    probe em__ff__high(struct VMCPU *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal, int a_rc);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc) */

    probe em__ff__all(struct VMCPU *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal, int a_rc);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc) */

    probe em__ff__all_ret(struct VMCPU *a_pVCpu, int a_rc);
    /*^^VMM-ALT-TP: "%d", (a_rc) */

    probe em__ff__raw(struct VMCPU *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal);
    /*^^VMM-ALT-TP: "vm=%#x cpu=%#x", (a_fGlobal), (a_fLocal) */

    probe em__ff__raw_ret(struct VMCPU *a_pVCpu, int a_rc);
    /*^^VMM-ALT-TP: "%d", (a_rc) */

    probe r0__gvmm__vm__created(void *a_pGVM, void *a_pVM, unsigned int a_Pid, void *a_hEMT0, unsigned int a_cCpus);
    probe r0__hmsvm__vmexit(struct VMCPU *a_pVM, struct CPUMCTX *a_pCtx, unsigned long a_ExitCode, 
                            unsigned long a_ExitInfo1, unsigned long a_ExitInfo2, unsigned long a_ExitIntInfo, 
                            unsigned long a_TestArgument);

};

#pragma D attributes Evolving/Evolving/Common provider vboxvmm provider
#pragma D attributes Private/Private/Unknown  provider vboxvmm module
#pragma D attributes Private/Private/Unknown  provider vboxvmm function
#pragma D attributes Evolving/Evolving/Common provider vboxvmm name
#pragma D attributes Evolving/Evolving/Common provider vboxvmm args

