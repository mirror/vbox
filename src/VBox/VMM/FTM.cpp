/* $Id$ */
/** @file
 * FTM - Fault Tolerance Manager
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_FTM
#include "FTMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/ssm.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/tcp.h>

/**
 * Initializes the FTM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3Init(PVM pVM)
{
    /** @todo saved state for master nodes! */
    pVM->ftm.s.pszAddress       = NULL;
    pVM->ftm.s.pszPassword      = NULL;
    pVM->fFaultTolerantMaster   = false;
    pVM->ftm.s.fIsStandbyNode   = false;
    pVM->ftm.s.standby.hServer  = NULL;
    return VINF_SUCCESS;
}

/**
 * Terminates the FTM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3Term(PVM pVM)
{
    if (pVM->ftm.s.pszAddress)
        RTMemFree(pVM->ftm.s.pszAddress);
    if (pVM->ftm.s.pszPassword)
        RTMemFree(pVM->ftm.s.pszPassword);
    if (pVM->ftm.s.standby.hServer)
        RTTcpServerDestroy(pVM->ftm.s.standby.hServer);

    return VINF_SUCCESS;
}

/**
 * Thread function which starts syncing process for this master VM
 *
 * @param   Thread      The thread id.
 * @param   pvUser      Not used
 * @return  VINF_SUCCESS (ignored).
 *
 * @note Locks the Console object for writing.
 */
static DECLCALLBACK(int) ftmR3MasterThread(RTTHREAD Thread, void *pvUser)
{
    return VINF_SUCCESS;
}

/**
 * Listen for incoming traffic destined for the standby VM.
 *
 * @copydoc FNRTTCPSERVE
 *
 * @returns VINF_SUCCESS or VERR_TCP_SERVER_STOP.
 */
static DECLCALLBACK(int) ftmR3StandbyServeConnection(RTSOCKET Sock, void *pvUser)
{
    PVM pVM = (PVM)pvUser;

    /*
     * Disable Nagle.
     */
    int rc = RTTcpSetSendCoalescing(Sock, false /*fEnable*/);
    AssertRC(rc);

    return VINF_SUCCESS;
}

/**
 * Powers on the fault tolerant virtual machine.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to power on.
 * @param   fMaster     FT master or standby
 * @param   uInterval   FT sync interval
 * @param   pszAddress  Standby VM address
 * @param   uPort       Standby VM port
 * @param   pszPassword FT password (NULL for none)
 *
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   PoweringOn+Running (master), PoweringOn+Running_FT (standby)
 */
VMMR3DECL(int) FTMR3PowerOn(PVM pVM, bool fMaster, unsigned uInterval, const char *pszAddress, unsigned uPort, const char *pszPassword)
{
    int rc = VINF_SUCCESS;

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_POWERING_ON,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);
    AssertReturn(pszAddress, VERR_INVALID_PARAMETER);

    pVM->ftm.s.uInterval   = uInterval;
    pVM->ftm.s.uPort       = uPort;
    pVM->ftm.s.pszAddress  = RTStrDup(pszAddress);
    if (pszPassword)
        pVM->ftm.s.pszPassword = RTStrDup(pszPassword);
    if (fMaster)
    {
        rc = RTThreadCreate(NULL, ftmR3MasterThread, NULL,
                            0, RTTHREADTYPE_IO /* higher than normal priority */, 0, "ftmR3MasterThread");
        if (RT_FAILURE(rc))
            return rc;

        pVM->fFaultTolerantMaster = true;
        return VMR3PowerOn(pVM);
    }
    else
    {
        /* standby */
        rc = RTTcpServerCreateEx(pszAddress, uPort, &pVM->ftm.s.standby.hServer);
        if (RT_FAILURE(rc))
            return rc;
        pVM->ftm.s.fIsStandbyNode = true;

        rc = RTTcpServerListen(pVM->ftm.s.standby.hServer, ftmR3StandbyServeConnection, pVM);
        /** @todo deal with the exit code to check if we should activate this standby VM. */

        RTTcpServerDestroy(pVM->ftm.s.standby.hServer);
        pVM->ftm.s.standby.hServer = NULL;
    }
    return rc;
}

/**
 * Powers off the fault tolerant virtual machine (standby).
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to power on.
 */
VMMR3DECL(int) FTMR3StandbyCancel(PVM pVM)
{
    AssertReturn(!pVM->fFaultTolerantMaster, VERR_NOT_SUPPORTED);
    Assert(pVM->ftm.s.standby.hServer);

    return RTTcpServerShutdown(pVM->ftm.s.standby.hServer);
}
