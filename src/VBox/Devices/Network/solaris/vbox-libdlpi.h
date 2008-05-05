/** $Id$ */
/** @file
 * Dynamically load libdpli & symbols on Solaris hosts, Internal header.
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

#ifndef ____H_VBOX_LIBDLPI
#define ____H_VBOX_LIBDLPI

#define LIB_DLPI "libdlpi.so"
#include <libdlpi.h>

extern int (*gLibDlpiOpen)(const char *, dlpi_handle_t *, uint_t);
extern void (*gLibDlpiClose)(dlpi_handle_t);
extern int (*gLibDlpiInfo)(dlpi_handle_t, dlpi_info_t *, uint_t);
extern int (*gLibDlpiBind)(dlpi_handle_t, uint_t, uint_t *);
extern int (*gLibDlpiSetPhysAddr)(dlpi_handle_t, uint_t, const void *, size_t);
extern int (*gLibDlpiPromiscon)(dlpi_handle_t, uint_t);
extern int (*gLibDlpiRecv)(dlpi_handle_t, void *, size_t *, void *, size_t *, int, dlpi_recvinfo_t *);
extern int (*gLibDlpiFd)(dlpi_handle_t);

extern bool gLibDlpiFound(void);

#endif /* ____H_VBOX_LIBDLPI not defined */

