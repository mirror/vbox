/* $Id$ */
/** @file
 * VBoxVMM - Trace point macros for the VMM.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ___VMMTracing_h___
#define ___VMMTracing_h___


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef DOXYGEN_RUNNING
# undef VBOX_WITH_DTRACE
# undef VBOX_WITH_DTRACE_R3
# undef VBOX_WITH_DTRACE_R0
# undef VBOX_WITH_DTRACE_RC
# define DBGFTRACE_ENABLED
#endif
#include <VBox/vmm/dbgftrace.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Gets the trace buffer handle from a VMCPU pointer. */
#define VMCPU_TO_HTB(a_pVCpu)     ((a_pVCpu)->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf))

/** Gets the trace buffer handle from a VMCPU pointer. */
#define VM_TO_HTB(a_pVM)          ((a_pVM)->CTX_SUFF(hTraceBuf))


/** @name Ring-3 trace points.
 * @{
 */
#ifdef IN_RING3
# ifdef VBOX_WITH_DTRACE_R3
#  include "VBoxVMMR3-dtrace.h"

# elif defined(DBGFTRACE_ENABLED)
#  define VBOXVMMR3_EM_STATE_CHANGED(a_pVCpu, a_enmOldState, a_enmNewState, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-state-changed %d -> %d (rc=%d)", a_enmOldState, a_enmNewState, a_rc)
#  define VBOXVMMR3_EM_STATE_UNCHANGED(a_pVCpu, a_enmState, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-state-unchanged %d (rc=%d)", a_enmState, a_rc)
#   define VBOXVMMR3_EM_RAW_RUN_PRE(a_pVCpu, a_pCtx) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-raw-pre %04x:%08llx", (a_pCtx)->cs, (a_pCtx)->rip)
#   define VBOXVMMR3_EM_RAW_RUN_RET(a_pVCpu, a_pCtx, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-raw-ret %04x:%08llx rc=%d", (a_pCtx)->cs, (a_pCtx)->rip, (a_rc))
#   define VBOXVMMR3_EM_FF_HIGH(a_pVCpu, a_fGlobal, a_fLocal, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-ff-high vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc))
#   define VBOXVMMR3_EM_FF_ALL(a_pVCpu, a_fGlobal, a_fLocal, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-ff-all vm=%#x cpu=%#x rc=%d", (a_fGlobal), (a_fLocal), (a_rc))
#   define VBOXVMMR3_EM_FF_ALL_RET(a_pVCpu, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-ff-all-ret %d", (a_rc))
#   define VBOXVMMR3_EM_FF_RAW(a_pVCpu, a_fGlobal, a_fLocal) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-ff-raw vm=%#x cpu=%#x", (a_fGlobal), (a_fLocal))
#   define VBOXVMMR3_EM_FF_RAW_RET(a_pVCpu, a_rc) \
        RTTraceBufAddMsgF(VMCPU_TO_HTB(a_pVCpu), "em-ff-raw-ret %d", (a_rc))

# else
#   define VBOXVMMR3_EM_STATE_CHANGED(a_pVCpu, a_enmOldState, a_enmNewState, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_STATE_UNCHANGED(a_pVCpu, a_enmState, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_RAW_RUN_PRE(a_pVCpu, a_pCtx) do { } while (0)
#   define VBOXVMMR3_EM_RAW_RUN_RET(a_pVCpu, a_pCtx, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_FF_HIGH(a_pVCpu, a_fGlobal, a_fLocal, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_FF_ALL(a_pVCpu, a_fGlobal, a_fLocal, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_FF_ALL_RET(a_pVCpu, a_rc) do { } while (0)
#   define VBOXVMMR3_EM_FF_RAW(a_pVCpu, a_fGlobal, a_fLocal) do { } while (0)
#   define VBOXVMMR3_EM_FF_RAW_RET(a_pVCpu, a_rc) do { } while (0)

# endif
#endif  /* IN_RING3 */


/** @name Ring-0 trace points.
 * @{
 */
#ifdef IN_RING0
# ifdef VBOX_WITH_DTRACE_R0
#  include "VBoxVMMR0-dtrace.h"

# elif defined(DBGFTRACE_ENABLED)

# else

# endif
#endif /* IN_RING0*/
/** @} */


/** @} */

#endif

