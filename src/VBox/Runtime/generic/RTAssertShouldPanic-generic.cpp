/* $Id$ */
/** @file
 * IPRT - Assertions, generic RTAssertShouldPanic.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>


RTDECL(bool) RTAssertShouldPanic(void)
{
    return true;
}


#if defined(IN_RING0) && defined(RT_OS_LINUX) && defined(IN_MODULE)
/*
 * When we build this in the Linux kernel module, we wish to make the
 * symbols available to other modules as well.
 */
#  include "the-linux-kernel.h"
EXPORT_SYMBOL(RTAssertShouldPanic);
#endif /* IN_RING0 && RT_OS_LINUX && IN_MODULE */
