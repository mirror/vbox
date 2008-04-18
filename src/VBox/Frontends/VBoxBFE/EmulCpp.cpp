/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Replacements of new() and delete() to avoid libstc++
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/log.h>
#include <iprt/assert.h>

#include <cstdlib>

// We don't link against the libstdc++ library so far. Since we
// compile with -fno-exceptions and -fno-rtti, we only have to
// provide new() and delete().

/** libstdc++ new emulator. */
void*
operator new(size_t size)
{
    return malloc(size);
}

/** libstdc++ delete emulator. */
void
operator delete(void *addr)
{
    free(addr);
}
