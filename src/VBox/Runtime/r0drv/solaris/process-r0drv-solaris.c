/* $Id$ */
/** @file
 * innotek Portable Runtime - Process Management, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"

#include <iprt/process.h>


RTDECL(RTPROCESS) RTProcSelf(void)
{
    struct pid *pPidInfo = curproc->p_pidp;
    return pPidInfo->pid_id;
}


RTR0DECL(RTR0PROCESS) RTR0ProcHandleSelf(void)
{
    return (RTR0PROCESS)curproc;
}

