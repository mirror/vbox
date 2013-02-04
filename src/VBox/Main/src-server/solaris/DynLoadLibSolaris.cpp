/* $Id$ */
/** @file
 * Dynamically load libraries for Solaris hosts.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "DynLoadLibSolaris.h"

#include <iprt/err.h>
#include <iprt/ldr.h>


/** -=-=-=-=-= LIB DLPI -=-=-=-=-=-=- **/

/**
 * Global pointer to the libdlpi module. This should only be set once all needed libraries
 * and symbols have been successfully loaded.
 */
static RTLDRMOD g_hLibDlpi = NULL;

/**
 * Whether we have tried to load libdlpi yet.  This flag should only be set
 * to "true" after we have either loaded both libraries and all symbols which we need,
 * or failed to load something and unloaded.
 */
static bool g_fCheckedForLibDlpi = false;

/** All the symbols we need from libdlpi.
 * @{
 */
int (*g_pfnLibDlpiWalk)(dlpi_walkfunc_t *, void *, uint_t);
/** @} */

bool VBoxSolarisLibDlpiFound(void)
{
    RTLDRMOD hLibDlpi;

    if (g_hLibDlpi && g_fCheckedForLibDlpi)
        return true;
    if (g_fCheckedForLibDlpi)
        return false;
    if (!RT_SUCCESS(RTLdrLoad(LIB_DLPI, &hLibDlpi)))
        return false;
    /*
     * Unfortunately; we cannot make use of dlpi_get_physaddr because it requires us to
     * open the VNIC/link which requires root permissions :/
     */
    if (RT_SUCCESS(RTLdrGetSymbol(hLibDlpi, "dlpi_walk", (void **)&g_pfnLibDlpiWalk)))
    {
        g_hLibDlpi = hLibDlpi;
        g_fCheckedForLibDlpi = true;
        return true;
    }
    else
    {
        RTLdrClose(hLibDlpi);
        g_fCheckedForLibDlpi = true;
        return false;
    }
}

