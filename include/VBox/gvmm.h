/* $Id$ */
/** @file
 * GVMM - The Global VM Manager.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

#ifndef ___VBox_gvmm_h
#define ___VBox_gvmm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/sup.h>

/** @defgroup grp_GVMM  GVMM - The Global VM Manager.
 * @{
 */

/** @def IN_GVMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global VM Manager or not.
 */
/** @def GVMMR0DECL
 * Ring 0 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GVMM_R0
# define GVMMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GVMMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def NIL_GVM_HANDLE
 * The nil GVM VM handle value (VM::hSelf).
 */
#define NIL_GVM_HANDLE 0

GVMMR0DECL(int)     GVMMR0Init(void);
GVMMR0DECL(void)    GVMMR0Term(void);

GVMMR0DECL(int)     GVMMR0CreateVM(PSUPDRVSESSION pSession, PVM *ppVM);
GVMMR0DECL(int)     GVMMR0CreateVMReq(PSUPVMMR0REQHDR pReqHdr);
GVMMR0DECL(int)     GVMMR0DisassociateEMTFromVM(PVM pVM);
GVMMR0DECL(int)     GVMMR0InitVM(PVM pVM);
GVMMR0DECL(int)     GVMMR0AssociateEMTWithVM(PVM pVM);
GVMMR0DECL(int)     GVMMR0DestroyVM(PVM pVM);
GVMMR0DECL(PGVM)    GVMMR0ByHandle(uint32_t hGVM);
GVMMR0DECL(PGVM)    GVMMR0ByVM(PVM pVM);
GVMMR0DECL(int)     GVMMR0ByVMAndEMT(PVM pVM, PGVM *ppGVM);
GVMMR0DECL(PVM)     GVMMR0GetVMByHandle(uint32_t hGVM);
GVMMR0DECL(PVM)     GVMMR0GetVMByEMT(RTNATIVETHREAD hEMT);
GVMMR0DECL(int)     GVMMR0SchedHalt(PVM pVM, uint64_t u64ExpireGipTime);
GVMMR0DECL(int)     GVMMR0SchedWakeUp(PVM pVM);
GVMMR0DECL(int)     GVMMR0SchedPoll(PVM pVM, bool fYield);



/**
 * Request packet for calling GVMMR0CreateVM.
 */
typedef struct GVMMCREATEVMREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** The support driver session. (IN) */
    PSUPDRVSESSION  pSession;
    /** Pointer to the ring-3 mapping of the shared VM structure on return. (OUT) */
    PVMR3           pVMR3;
    /** Pointer to the ring-0 mapping of the shared VM structure on return. (OUT) */
    PVMR0           pVMR0;
} GVMMCREATEVMREQ;
/** Pointer to a GVMMR0CreateVM request packet. */
typedef GVMMCREATEVMREQ *PGVMMCREATEVMREQ;


/** @} */

#endif

