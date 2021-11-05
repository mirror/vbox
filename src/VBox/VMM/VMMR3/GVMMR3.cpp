/* $Id$ */
/** @file
 * GVMM - Global VM Manager, ring-3 request wrappers.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GVMM
#include <VBox/vmm/gvmm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/sup.h>


/**
 * @see GMMR0RegisterWorkerThread
 */
VMMR3_INT_DECL(int)  GVMMR3RegisterWorkerThread(PVM pVM, GVMMWORKERTHREAD enmWorker)
{
    GVMMREGISTERWORKERTHREADREQ Req;
    Req.Hdr.u32Magic    = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq       = sizeof(Req);
    Req.hNativeThreadR3 = RTThreadNativeSelf();
    return SUPR3CallVMMR0Ex(VMCC_GET_VMR0_FOR_CALL(pVM), NIL_VMCPUID,
                            VMMR0_DO_GVMM_REGISTER_WORKER_THREAD, (unsigned)enmWorker, &Req.Hdr);
}


/**
 * @see GMMR0DeregisterWorkerThread
 */
VMMR3_INT_DECL(int)  GVMMR3DeregisterWorkerThread(PVM pVM, GVMMWORKERTHREAD enmWorker)
{
    return SUPR3CallVMMR0Ex(VMCC_GET_VMR0_FOR_CALL(pVM), NIL_VMCPUID,
                            VMMR0_DO_GVMM_DEREGISTER_WORKER_THREAD, (unsigned)enmWorker, NULL);
}

