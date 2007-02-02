/* $Id$ */
/** @file
 * Innotek Host Config - Maintained by hand
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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


#if defined(__amd64__) || defined(HOST_X86_64) /* latter, for dyngen on win64. */
# define HOST_X86_64 1
# define HOST_LONG_BITS 64
#else
# define HOST_I386 1
# define HOST_LONG_BITS 32
# ifdef __WIN32__
#  define CONFIG_WIN32 1
# elif defined(__OS2__)
#  define CONFIG_OS2
# elif defined(__DARWIN__)
#  define CONFIG_DARWIN
# elif defined(__FREEBSD__) || defined(__NETBSD__) || defined(__OPENBSD__)
/*#  define CONFIG_BSD*/
# elif defined(__SOLARIS__)
#  error "configure me (decide if you HAVE_BYTESWAP_H or not, and check what HOST_SOLARIS does to the code)"
# elif !defined(IPRT_NO_CRT)
#  define HAVE_BYTESWAP_H 1
# endif
#endif
#define QEMU_VERSION "0.8.1"
#define CONFIG_UNAME_RELEASE ""
#define CONFIG_QEMU_SHAREDIR "."

