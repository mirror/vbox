/* $Id: GMM.cpp 63491 2010-07-08 09:12:48Z sandervl $ */
/** @file
 * GMM - Global Memory Manager, ring-3 request wrappers.
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
#include <VBox/gmm.h>
#include <VBox/vmm.h>
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>


VMMR3DECL(int) FTMR3PowerOn(PVM pVM, bool fSource, unsigned uInterval, char *pszAddress, unsigned uPort)
{
    return VERR_NOT_IMPLEMENTED;
}

