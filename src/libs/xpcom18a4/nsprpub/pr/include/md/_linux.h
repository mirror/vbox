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

#ifndef nspr_linux_defs_h___
#define nspr_linux_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"linux"
#define _PR_SI_SYSNAME  "LINUX"
#ifdef __powerpc__
#define _PR_SI_ARCHITECTURE "ppc"
#elif defined(__alpha)
#define _PR_SI_ARCHITECTURE "alpha"
#elif defined(__ia64__)
#define _PR_SI_ARCHITECTURE "ia64"
#elif defined(__x86_64__)
#define _PR_SI_ARCHITECTURE "x86-64"
#elif defined(__mc68000__)
#define _PR_SI_ARCHITECTURE "m68k"
#elif defined(__sparc__)
#define _PR_SI_ARCHITECTURE "sparc"
#elif defined(__i386__)
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__mips__)
#define _PR_SI_ARCHITECTURE "mips"
#elif defined(__arm__)
#define _PR_SI_ARCHITECTURE "arm"
#elif defined(__hppa__)
#define _PR_SI_ARCHITECTURE "hppa"
#elif defined(__s390__)
#define _PR_SI_ARCHITECTURE "s390"
#elif defined(__s390x__)
#define _PR_SI_ARCHITECTURE "s390x"
#else
#error "Unknown CPU architecture"
#endif
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef	HAVE_STACK_GROWING_UP

extern void _MD_CleanupBeforeExit(void);
#define _MD_CLEANUP_BEFORE_EXIT _MD_CleanupBeforeExit

extern void _MD_EarlyInit(void);
extern PRIntervalTime _PR_UNIX_GetInterval(void);
extern PRIntervalTime _PR_UNIX_TicksPerSecond(void);

#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT					_PR_UnixInit
#define _MD_GET_INTERVAL                _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC            _PR_UNIX_TicksPerSecond

#endif /* nspr_linux_defs_h___ */

