/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Module & Segment Management.
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


/** @page pg_dbgf_module    Module & Segment Management
 *
 * A module is our representation of an executable binary. It's main purpose
 * is to provide segments that can be mapped into address spaces and thereby
 * provide debug info for those parts for the guest code or data.
 *
 * This module will not deal directly with debug info, it will only serve
 * as an interface between the debugger / symbol lookup and the debug info
 * readers.
 *
 * An executable binary doesn't need to have a file, or that is, we don't
 * need the file to create a module for it. There will be interfaces for
 * ROMs to register themselves so we can get to their symbols, and there
 * will be interfaces for the guest OS plugins (@see pg_dbgf_os) to
 * register kernel, drivers and other global modules.
 */


