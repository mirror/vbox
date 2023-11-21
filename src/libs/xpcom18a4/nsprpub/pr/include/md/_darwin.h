/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nspr_darwin_defs_h___
#define nspr_darwin_defs_h___

#include <sys/syscall.h>

#ifdef XP_MACOSX
#include <AvailabilityMacros.h>
#endif

#define PR_LINKER_ARCH	"darwin"
#define _PR_SI_SYSNAME  "DARWIN"
#ifdef __i386__
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__ppc__)
#define _PR_SI_ARCHITECTURE "ppc"
#elif defined(__amd64__)
#define _PR_SI_ARCHITECTURE "amd64"
#elif defined(__arm__)
#define _PR_SI_ARCHITECTURE "arm"
#elif defined(__arm64__)
#define _PR_SI_ARCHITECTURE "arm64"
#else
#error "unknown architecture."
#endif
#define PR_DLL_SUFFIX		".dylib"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define _PR_HAVE_SOCKADDR_LEN
#define _PR_STAT_HAS_ST_ATIMESPEC
#define _PR_HAVE_LARGE_OFF_T
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY

#define _MD_EARLY_INIT          _MD_EarlyInit
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_GET_INTERVAL        _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC    _PR_UNIX_TicksPerSecond

extern void             _MD_EarlyInit(void);
extern PRIntervalTime   _PR_UNIX_GetInterval(void);
extern PRIntervalTime   _PR_UNIX_TicksPerSecond(void);

#endif /* nspr_darwin_defs_h___ */
