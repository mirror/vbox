/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - RTFs Queries..
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/path.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/err.h>


int main(int argc, char **argv)
{
    RTR3Init();

    /*
     * Process all arguments (including the executable).
     */
    int cErrors = 0;
    for (int i = 0; i < argc; i++)
    {
        RTPrintf("tstRTFsQueries: '%s'...\n", argv[i]);

        uint32_t u32Serial;
        int rc = RTFsQuerySerial(argv[i], &u32Serial);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: u32Serial=%#010RX32\n", u32Serial);
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySerial failed, rc=%Vrc\n", rc);
            cErrors++;
        }

        RTFOFF cbTotal = 42;
        RTFOFF cbFree  = 42;
        uint32_t cbBlock = 42;
        uint32_t cbSector = 42;
        rc = RTFsQuerySizes(argv[i], &cbTotal, &cbFree, &cbBlock, &cbSector);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: cbTotal=%RTfoff cbFree=%RTfoff cbBlock=%d cbSector=%d\n",
                     cbTotal, cbFree, cbBlock, cbSector);
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySerial failed, rc=%Vrc\n", rc);
            cErrors++;
        }

        rc = RTFsQuerySizes(argv[i], NULL, NULL, NULL, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySizes(nop) failed, rc=%Vrc\n", rc);
            cErrors++;
        }

        RTFSPROPERTIES Props;
        rc = RTFsQueryProperties(argv[i], &Props);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: cbMaxComponent=%u %s %s %s %s %s %s\n",
                     Props.cbMaxComponent,
                     Props.fCaseSensitive ? "case" : "not-case",
                     Props.fCompressed ? "compressed" : "not-compressed",
                     Props.fFileCompression ? "file-compression" : "no-file-compression",
                     Props.fReadOnly ? "readonly" : "readwrite",
                     Props.fRemote ? "remote" : "not-remote",
                     Props.fSupportsUnicode ? "supports-unicode" : "doesn't-support-unicode");
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQueryProperties failed, rc=%Vrc\n", rc);
            cErrors++;
        }
    }

    if (!cErrors)
        RTPrintf("tstRTFsQueries: SUCCESS\n");
    else
        RTPrintf("tstRTFsQueries: FAIlURE - %u errors\n", cErrors);
    return !!cErrors;
}
