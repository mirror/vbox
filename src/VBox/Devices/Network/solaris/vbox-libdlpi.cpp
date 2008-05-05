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

#include "vbox-libdlpi.h"

#include <iprt/err.h>
#include <iprt/ldr.h>

/**
 * Global pointer to the libdlpi module. This should only be set once all needed libraries
 * and symbols have been successfully loaded.
 */
static RTLDRMOD ghLibDlpi = NULL;

/**
 * Whether we have tried to load libdlpi yet.  This flag should only be set
 * to "true" after we have either loaded both libraries and all symbols which we need,
 * or failed to load something and unloaded.
 */
static bool gCheckedForLibDlpi = false;

/** 
 * All the symbols we need from libdlpi.
 */
int (*gLibDlpiOpen)(const char *, dlpi_handle_t *, uint_t);
void (*gLibDlpiClose)(dlpi_handle_t);
int (*gLibDlpiInfo)(dlpi_handle_t, dlpi_info_t *, uint_t);
int (*gLibDlpiBind)(dlpi_handle_t, uint_t, uint_t *);
int (*gLibDlpiSetPhysAddr)(dlpi_handle_t, uint_t, const void *, size_t);
int (*gLibDlpiPromiscon)(dlpi_handle_t, uint_t);
int (*gLibDlpiRecv)(dlpi_handle_t, void *, size_t *, void *, size_t *, int, dlpi_recvinfo_t *);
int (*gLibDlpiFd)(dlpi_handle_t);

bool gLibDlpiFound(void)
{
    RTLDRMOD hLibDlpi;

    if (ghLibDlpi && gCheckedForLibDlpi == true)
        return true;
    if (gCheckedForLibDlpi == true)
        return false;
    if (!RT_SUCCESS(RTLdrLoad(LIB_DLPI, &hLibDlpi)))
    {
        return false;
    }
    if (   RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_open", (void **)&gLibDlpiOpen))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_close", (void **)&gLibDlpiClose))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_info", (void **)&gLibDlpiInfo))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_bind", (void **)&gLibDlpiBind))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_promiscon", (void **)&gLibDlpiPromiscon))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_set_physaddr", (void **)&gLibDlpiSetPhysAddr))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_recv", (void **)&gLibDlpiRecv))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_fd", (void **)&gLibDlpiFd))
       )
    {
        ghLibDlpi = hLibDlpi;
        gCheckedForLibDlpi = true;
        return true;
    }
    else
    {
        RTLdrClose(hLibDlpi);
        gCheckedForLibDlpi = true;
        return false;
    }    
}

