/* $Id$ */
/** @file
 * FTM - Fault Tolerance Manager
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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
#include <VBox/ftm.h>

#include <iprt/assert.h>
#include <VBox/log.h>



VMMR3DECL(int) FTMR3PowerOn(PVM pVM, bool fSource, unsigned uInterval, const char *pszAddress, unsigned uPort)
{
    return VERR_NOT_IMPLEMENTED;
}

