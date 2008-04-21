/* $Id$ */
/** @file
 * IPRT - Time using SUPLib, the C Implementation.
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

#ifndef IN_GUEST

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/time.h>
#include <iprt/asm.h>
#include <VBox/sup.h>
#include "internal/time.h"


/*
 * Use the CPUID instruction for some kind of serialization.
 */
#undef  ASYNC_GIP
#undef  USE_LFENCE
#define NEED_TRANSACTION_ID
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacySync
#include "timesupref.h"

#define ASYNC_GIP
#ifdef IN_GC
# undef NEED_TRANSACTION_ID
#endif
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacyAsync
#include "timesupref.h"


/*
 * Use LFENCE for load serialization.
 */
#undef  ASYNC_GIP
#define USE_LFENCE
#undef  NEED_TRANSACTION_ID
#define NEED_TRANSACTION_ID
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceSync
#include "timesupref.h"

#define ASYNC_GIP
#ifdef IN_GC
# undef NEED_TRANSACTION_ID
#endif
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceAsync
#include "timesupref.h"


#endif /* !IN_GUEST */
