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

#define HOST_I386 1
#ifdef __WIN32__
# define CONFIG_WIN32 1
#elif defined(__OS2__)
# define CONFIG_OS2
#elif defined(__DARWIN__)
# define CONFIG_DARWIN
#else
# define HAVE_BYTESWAP_H 1
#endif
#define CONFIG_SOFTMMU 1

#define CONFIG_SDL 1
#define CONFIG_SLIRP 1

#ifdef __LINUX__
#define CONFIG_GDBSTUB 1
#endif
/* #define HAVE_GPROF 1 */
/* #define CONFIG_STATIC 1 */
#define QEMU_VERSION "0.6.1"
#define CONFIG_QEMU_SHAREDIR "."
