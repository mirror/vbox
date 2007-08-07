/* $Id$ */
/** @file
 * innotek Portable Runtime - Process, Common.
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
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/process.h"
#include "internal/thread.h"

#ifdef RT_OS_WINDOWS
# include <process.h>
#else
# include <unistd.h>
#endif


/**
 * Get the identifier for the current process.
 *
 * @returns Process identifier.
 */
RTDECL(RTPROCESS) RTProcSelf(void)
{
    RTPROCESS Self = g_ProcessSelf;
    if (Self != NIL_RTPROCESS)
        return Self;

    /* lazy init. */
#ifdef _MSC_VER
    Self = _getpid(); /* crappy ansi compiler */
#else
    Self = getpid();
#endif
    g_ProcessSelf = Self;
    return Self;
}


/**
 * Attempts to alter the priority of the current process.
 *
 * @returns iprt status code.
 * @param   enmPriority     The new priority.
 */
RTR3DECL(int) RTProcSetPriority(RTPROCPRIORITY enmPriority)
{
    if (enmPriority > RTPROCPRIORITY_INVALID && enmPriority < RTPROCPRIORITY_LAST)
        return rtThreadDoSetProcPriority(enmPriority);
    AssertMsgFailed(("enmPriority=%d\n", enmPriority));
    return VERR_INVALID_PARAMETER;
}


/**
 * Gets the current priority of this process.
 *
 * @returns The priority (see RTPROCPRIORITY).
 */
RTR3DECL(RTPROCPRIORITY) RTProcGetPriority(void)
{
    return g_enmProcessPriority;
}

