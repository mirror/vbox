/* $Id$ */
/** @file
 * IPRT Testcase - Simple cURL testcase.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/initterm.h>

#define CAFILE_NAME "tstHttp-tempcafile.crt"

int main()
{
    unsigned cErrors = 0;

    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);

    RTHTTP hHttp;
    int rc = RTHttpCreate(&hHttp);
    char *pszBuf = NULL;
    PRTSTREAM CAFile = NULL;

    // create certificate file
    rc = RTStrmOpen(CAFILE_NAME, "w+b", &CAFile);

    // fetch root CA certificate (new one, often avoided in cert chains by
    // using an intermediate cert which is signed by old root)
    if (RT_SUCCESS(rc))
        rc = RTHttpGet(hHttp,
                       "http://www.verisign.com/repository/roots/root-certificates/PCA-3G5.pem",
                       &pszBuf);
    if (RT_SUCCESS(rc) && pszBuf)
    {
	/// @todo check certificate fingerprint against a strong hash,
	// otherwise there's a simple way for a man-in-the-middle attack
        rc = RTStrmWrite(CAFile, pszBuf, strlen(pszBuf));
	if (RT_SUCCESS(rc))
            rc = RTStrmWrite(CAFile, RTFILE_LINEFEED, strlen(RTFILE_LINEFEED));
    }
    if (pszBuf)
    {
        RTMemFree(pszBuf);
        pszBuf = NULL;
    }

    // fetch root CA certificate (old one, but still very widely used)
    if (RT_SUCCESS(rc))
        rc = RTHttpGet(hHttp,
                       "http://www.verisign.com/repository/roots/root-certificates/PCA-3.pem",
                       &pszBuf);
    if (RT_SUCCESS(rc) && pszBuf)
    {
	/// @todo check certificate fingerprint against a strong hash,
	// otherwise there's a simple way for a man-in-the-middle attack
        rc = RTStrmWrite(CAFile, pszBuf, strlen(pszBuf));
	if (RT_SUCCESS(rc))
            rc = RTStrmWrite(CAFile, RTFILE_LINEFEED, strlen(RTFILE_LINEFEED));
    }
    if (pszBuf)
    {
        RTMemFree(pszBuf);
        pszBuf = NULL;
    }

    // close certificate file
    if (CAFile)
    {
        RTStrmClose(CAFile);
        CAFile = NULL;
    }

    if (RT_SUCCESS(rc))
        rc = RTHttpSetCAFile(hHttp, CAFILE_NAME);
    if (RT_SUCCESS(rc))
        rc = RTHttpGet(hHttp,
                       "https://update.virtualbox.org/query.php?platform=LINUX_32BITS_UBUNTU_12_04&version=4.1.18",
                       &pszBuf);
    RTHttpDestroy(hHttp);

    if (RT_FAILURE(rc))
        cErrors++;

    RTPrintf("Error code: %Rrc\nGot: %s\n", rc, pszBuf);
    RTMemFree(pszBuf);

//    RTFileDelete(CAFILE_NAME);

    return !!cErrors;
}
