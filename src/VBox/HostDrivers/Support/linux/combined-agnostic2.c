/* $Id$ */
/** @file
 * SUPDrv - Combine a bunch of OS agnostic sources into one compile unit.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#define LOG_GROUP LOG_GROUP_DEFAULT
#include "internal/iprt.h"
#include <VBox/log.h>

#undef LOG_GROUP
#include "common/string/RTStrCat.c"
#undef LOG_GROUP
#include "common/string/RTStrCopy.c"
#undef LOG_GROUP
#include "common/string/RTStrCopyEx.c"
#undef LOG_GROUP
#include "common/string/RTStrCopyP.c"
#undef LOG_GROUP
#include "common/string/RTStrEnd.c"
#undef LOG_GROUP
#include "common/string/RTStrNCmp.c"
#undef LOG_GROUP
#include "common/string/RTStrNLen.c"
#undef LOG_GROUP
#include "common/string/stringalloc.c"
#undef LOG_GROUP
#include "common/string/strformat.c"
#undef LOG_GROUP
#include "common/string/strformatnum.c"
#undef LOG_GROUP
#include "common/string/strformattype.c"
#undef LOG_GROUP
#include "common/string/strprintf.c"
#undef LOG_GROUP
#include "common/string/strtonum.c"
#undef LOG_GROUP
#include "common/table/avlpv.c"
#undef LOG_GROUP
#include "common/time/time.c"
#undef LOG_GROUP
#include "generic/RTAssertShouldPanic-generic.c"
#undef LOG_GROUP
#include "generic/RTLogWriteStdErr-stub-generic.c"
#undef LOG_GROUP
#include "generic/RTLogWriteStdOut-stub-generic.c"
#undef LOG_GROUP
#include "generic/RTLogWriteUser-generic.c"
#undef LOG_GROUP
#include "generic/RTMpGetArraySize-generic.c"
#undef LOG_GROUP
#include "generic/RTMpGetCoreCount-generic.c"
#undef LOG_GROUP
#include "generic/RTSemEventWait-2-ex-generic.c"
#undef LOG_GROUP
#include "generic/RTSemEventWaitNoResume-2-ex-generic.c"
#undef LOG_GROUP
#include "generic/RTSemEventMultiWait-2-ex-generic.c"
#undef LOG_GROUP
#include "generic/RTSemEventMultiWaitNoResume-2-ex-generic.c"
#undef LOG_GROUP
#include "generic/RTTimerCreate-generic.c"
#undef LOG_GROUP
#include "generic/errvars-generic.c"
#undef LOG_GROUP
#include "generic/mppresent-generic.c"
#undef LOG_GROUP
#include "generic/uuid-generic.c"
#undef LOG_GROUP
#include "VBox/log-vbox.c"

#ifdef RT_ARCH_AMD64
# undef LOG_GROUP
# include "common/alloc/heapsimple.c"
#endif

