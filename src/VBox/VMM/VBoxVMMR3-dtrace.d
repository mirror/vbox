/* $Id$ */
/** @file
 * VBoxVMM - Static ring-3 dtrace probes.
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

provider vboxvmmr3
{
    probe em__state__changed(void *a_pVCpu, int a_enmOldState, int a_enmNewState, int a_rc);
    probe em__state__unchanged(void *a_pVCpu, int a_enmState, int a_rc);
    probe em__raw__run__pre(void *a_pVCpu, void *a_pCtx);
    probe em__raw__run__ret(void *a_pVCpu, void *a_pCtx, int a_rc);
    probe em__raw__ff__high(void *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal, int a_rc);
    probe em__raw__ff__all(void *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal, int a_rc);
    probe em__raw__ff__all_ret(void *a_pVCpu, int a_rc);
    probe em__raw__ff__raw(void *a_pVCpu, unsigned int a_fGlobal, unsigned int a_fLocal);
    probe em__raw__ff__raw_ret(void *a_pVCpu, int a_rc);
};

#pragma D attributes Evolving/Evolving/Common provider vboxdd provider
#pragma D attributes Private/Private/Unknown  provider vboxdd module
#pragma D attributes Private/Private/Unknown  provider vboxdd function
#pragma D attributes Evolving/Evolving/Common provider vboxdd name
#pragma D attributes Evolving/Evolving/Common provider vboxdd args

