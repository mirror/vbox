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
#include "r0drv/alloc-r0drv.c"
#undef LOG_GROUP
#include "r0drv/initterm-r0drv.c"
#undef LOG_GROUP
#include "r0drv/memobj-r0drv.c"
#undef LOG_GROUP
#include "r0drv/mpnotification-r0drv.c"
#undef LOG_GROUP
#include "r0drv/powernotification-r0drv.c"
#undef LOG_GROUP
#include "r0drv/generic/semspinmutex-r0drv-generic.c"
#undef LOG_GROUP
#include "common/alloc/alloc.c"
#undef LOG_GROUP
#include "common/checksum/crc32.c"
#undef LOG_GROUP
#include "common/checksum/ipv4.c"
#undef LOG_GROUP
#include "common/checksum/ipv6.c"
#undef LOG_GROUP
#include "common/err/errinfo.c"
#undef LOG_GROUP
#include "common/log/log.c"
#undef LOG_GROUP
#include "common/log/logellipsis.c"
#undef LOG_GROUP
#include "common/log/logrel.c"
#undef LOG_GROUP
#include "common/log/logrelellipsis.c"
#undef LOG_GROUP
#include "common/log/logcom.c"
#undef LOG_GROUP
#include "common/log/logformat.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg1Weak.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2Add.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2AddWeak.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2AddWeakV.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2Weak.c"
#undef LOG_GROUP
#include "common/misc/RTAssertMsg2WeakV.c"
#undef LOG_GROUP
#include "common/misc/assert.c"
#undef LOG_GROUP
#include "common/misc/handletable.c"
#undef LOG_GROUP
#include "common/misc/handletablectx.c"
#undef LOG_GROUP
#include "common/misc/thread.c"

