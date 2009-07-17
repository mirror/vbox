/* $Id$ */
/** @file
 * IPRT - rtPathVolumeSpecLen
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/string.h>
#include "internal/path.h"



/**
 * Returns the length of the volume name specifier of the given path.
 * If no such specifier zero is returned.
 */
size_t rtPathVolumeSpecLen(const char *pszPath)
{
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
    if (pszPath && *pszPath)
    {
        /* UTC path. */
        /** @todo r=bird: it's UNC and we have to check that the next char isn't a
         *        slash, then skip both the server and the share name. */
        if (    (pszPath[0] == '\\' || pszPath[0] == '/')
            &&  (pszPath[1] == '\\' || pszPath[1] == '/'))
            return strcspn(pszPath + 2, "\\/") + 2;

        /* Drive letter. */
        if (    pszPath[1] == ':'
            &&  toupper(pszPath[0]) >= 'A' && toupper(pszPath[0]) <= 'Z')
            return 2;
    }
    return 0;

#else
    /* This isn't quite right when looking at the above stuff, but it works assuming that '//' does not mean UNC. */
    /// @todo (dmik) well, it's better to consider there's no volume name
    //  at all on *nix systems
    return 0;
//    return pszPath && pszPath[0] == '/';
#endif
}

