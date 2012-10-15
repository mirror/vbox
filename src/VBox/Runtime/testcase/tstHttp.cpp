/* $Id$ */
/** @file
 * IPRT Testcase - Simple cURL testcase.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/thread.h>

int main()
{
    unsigned cErrors = 0;

    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);

    RTHTTP hHttp;
    int rc = RTHttpCreate(&hHttp);
    char *pszBuf = NULL;
    if (RT_SUCCESS(rc))
        rc = RTHttpGet(hHttp,
                       "https://update.virtualbox.org/query.php?platform=LINUX_32BITS_UBUNTU_12_04&version=4.1.18",
                       &pszBuf);
    RTHttpDestroy(hHttp);

    RTPrintf("Error code: %Rrc\nGot: %s\n", rc, pszBuf);
    RTMemFree(pszBuf);

    return !!cErrors;
}
