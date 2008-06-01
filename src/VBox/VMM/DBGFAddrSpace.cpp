/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Address Space Management.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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


/** @page pg_dbgf_addr_space     Address Space Management
 *
 * What's an address space? It's mainly a convenient way of stuffing
 * module segments and ad-hoc symbols together. It will also help out
 * when the debugger gets extended to deal with user processes later.
 *
 * There are two standard address spaces that will always be present:
 *   - The physical address space.
 *   - The global virtual address space.
 *
 * Additional address spaces will be added and removed at runtime for
 * guest processes. The global virtual address space will be used to
 * track the kernel parts of the OS, or at least the bits of the kernel
 * that is part of all address spaces (mac os x and 4G/4G patched linux).
 *
 */


