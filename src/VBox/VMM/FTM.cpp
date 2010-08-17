/* $Id */
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
#include <VBox/vmm.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include "FTMInternal.h"

#include <iprt/assert.h>
#include <VBox/log.h>


/**
 * Powers on the fault tolerant virtual machine.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to power on.
 * @param   fMaster     FT master or standby
 * @param   uInterval   FT sync interval
 * @param   pszAddress  Master VM address
 * @param   uPort       Master VM port
 *
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   PoweringOn+Running (master), PoweringOn+Running_FT (standby)
 */
VMMR3DECL(int) FTMR3PowerOn(PVM pVM, bool fMaster, unsigned uInterval, const char *pszAddress, unsigned uPort)
{
    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_POWERING_ON,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);

    if (fMaster)
    {
        return VMR3PowerOn(pVM);
    }
    else
    {
        /* standby */
    }
    return VERR_NOT_IMPLEMENTED;
}

