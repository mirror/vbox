/* $Id$ */
/** @file
 * innotek Portable Runtime - Threads (Part 2), Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>
#include "internal/thread.h"


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative(((RTNATIVETHREAD)current);
}

