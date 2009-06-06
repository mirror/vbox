/** $Id$ */
/** @file
 * Counters definitions and declarations
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
#ifndef PROFILE_COUNTER
# error PROFILE_COUNTER not defied
#endif
#ifndef COUNTING_COUTER
#error COUNTING_COUTER
#endif

PROFILE_COUNTER(Fill, "Profiling slirp fills");
PROFILE_COUNTER(Poll, "Profiling slirp polls");
PROFILE_COUNTER(FastTimer, "Profiling slirp fast timer");
PROFILE_COUNTER(SlowTimer, "Profiling slirp slow timer");
PROFILE_COUNTER(IOwrite, "Profiling IO sowrite");
PROFILE_COUNTER(IOread, "Profiling IO soread");

COUNTING_COUTER(TCP, "TCP sockets");
COUNTING_COUTER(TCPHot, "TCP sockets active");
COUNTING_COUTER(UDP, "UDP sockets");
COUNTING_COUTER(UDPHot, "UDP sockets active");
COUNTING_COUTER(SBAlloc, "SB Alloc");
COUNTING_COUTER(SBReAlloc, "SB ReAlloc");

COUNTING_COUTER(IORead_in_1, "SB IORead_in_1");
COUNTING_COUTER(IORead_in_2, "SB IORead_in_2");
COUNTING_COUTER(IOWrite_in_1, "SB IOWrite_in_1");
COUNTING_COUTER(IOWrite_in_2, "SB IOWrite_in_2");
