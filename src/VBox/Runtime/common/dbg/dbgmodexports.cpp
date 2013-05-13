/* $Id$ */
/** @file
 * IPRT - Debug Module Using Image Exports.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"






/**
 * Creates the debug info side of affairs based on exports and segments found in
 * the image part.
 *
 * The image part must be successfully initialized prior to the call, while the
 * debug bits must not be present of course.
 *
 * @returns IPRT status code
 * @param   pDbgMod             The debug module structure.
 */
DECLHIDDEN(int) rtDbgModCreateForExports(PRTDBGMODINT pDbgMod)
{
    AssertReturn(!pDbgMod->pDbgVt, VERR_DBG_MOD_IPE);
    AssertReturn(pDbgMod->pImgVt, VERR_DBG_MOD_IPE);
    RTUINTPTR cbImage = pDbgMod->pImgVt->pfnImageSize(pDbgMod);
    AssertReturn(cbImage > 0, VERR_DBG_MOD_IPE);

    /*
     * We simply use a container type for this work.
     */
    /// @todo later int rc = rtDbgModContainerCreate(pDbgMod, 0);
    int rc = rtDbgModContainerCreate(pDbgMod, cbImage);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Copy the segments.
     */

    /*
     * Copy the symbols.
     */


    return VINF_SUCCESS;
}

