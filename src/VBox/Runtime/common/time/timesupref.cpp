/* $Id$ */
/** @file
 * IPRT - Time using SUPLib, the C Implementation.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
 */

#if !defined(IN_GUEST) && !defined(RT_NO_GIP)

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/time.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/asm-amd64-x86.h>
#include <VBox/sup.h>
#include "internal/time.h"


#define GIP_MODE_SYNC_NO_DELTA          1
#define GIP_MODE_SYNC_WITH_DELTA        2
#define GIP_MODE_ASYNC                  3
#define GIP_MODE_INVARIANT_NO_DELTA     4
#define GIP_MODE_INVARIANT_WITH_DELTA   5
#define IS_GIP_MODE_WITH_DELTA(a_enmMode) \
    ((a_enmMode) == GIP_MODE_SYNC_WITH_DELTA || (a_enmMode) == GIP_MODE_INVARIANT_WITH_DELTA)


/*
 * Use the CPUID instruction for some kind of serialization.
 */
#define GIP_MODE GIP_MODE_SYNC_NO_DELTA
#undef  USE_LFENCE
#define NEED_TRANSACTION_ID
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacySyncNoDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLegacySyncNoDelta);

#undef  GIP_MODE
#define GIP_MODE  GIP_MODE_SYNC_NO_DELTA
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacySyncWithDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLegacySyncWithDelta);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_ASYNC
#ifdef IN_RC
# undef NEED_TRANSACTION_ID
#endif
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacyAsync
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLegacyAsync);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_INVARIANT_NO_DELTA
#undef  NEED_TRANSACTION_ID
#define NEED_TRANSACTION_ID
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacyInvariantNoDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLegacyInvariantNoDelta);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_INVARIANT_WITH_DELTA
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLegacyInvariantWithDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLegacyInvariantWithDelta);


/*
 * Use LFENCE for load serialization.
 */
#undef  GIP_MODE
#define GIP_MODE GIP_MODE_SYNC_NO_DELTA
#define USE_LFENCE
#undef  NEED_TRANSACTION_ID
#define NEED_TRANSACTION_ID
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceSyncNoDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLFenceSyncNoDelta);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_SYNC_WITH_DELTA
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceSyncWithDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLFenceSyncWithDelta);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_ASYNC
#ifdef IN_RC
# undef NEED_TRANSACTION_ID
#endif
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceAsync
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLFenceAsync);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_INVARIANT_NO_DELTA
#undef  NEED_TRANSACTION_ID
#define NEED_TRANSACTION_ID
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceInvariantNoDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLFenceInvariantNoDelta);

#undef  GIP_MODE
#define GIP_MODE GIP_MODE_INVARIANT_WITH_DELTA
#undef  rtTimeNanoTSInternalRef
#define rtTimeNanoTSInternalRef RTTimeNanoTSLFenceInvariantWithDelta
#include "timesupref.h"
RT_EXPORT_SYMBOL(RTTimeNanoTSLFenceInvariantWithDelta);


#endif /* !IN_GUEST && !RT_NO_GIP */

