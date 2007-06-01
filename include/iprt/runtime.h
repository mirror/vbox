/** @file
 * innotek Portable Runtime - Include Everything.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __iprt_runtime_h__
#define __iprt_runtime_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/param.h>
#include <iprt/initterm.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/crc32.h>
#include <iprt/crc64.h>
#include <iprt/critsect.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/ldr.h>
#include <iprt/log.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/table.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/uni.h>
#include <iprt/uuid.h>
#include <iprt/zip.h>

#ifdef IN_RING3
# include <iprt/stream.h>
# include <iprt/tcp.h>
# include <iprt/ctype.h>
# include <iprt/alloca.h>  /** @todo iprt/alloca.h should be made available in R0 and GC too! */
# include <iprt/process.h> /** @todo iprt/process.h should be made available in R0 too (partly). */
#endif

#ifdef IN_RING0
# include <iprt/memobj.h>
#endif


#endif

