/* $Id$ */
/** @file
 * IPRT Testcase - IPRT Filesystem API (Fileystem)
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
#include <iprt/filesystem.h>

#include <iprt/err.h>
#include <iprt/test.h>
#include <iprt/file.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

static int filesystemDiskRead(void *pvUser, uint64_t off, void *pvBuf, size_t cbRead)
{
    RTFILE hFile = (RTFILE)pvUser;

    return RTFileReadAt(hFile, off, pvBuf, cbRead, NULL);
}

static int filesystemDiskWrite(void *pvUser, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    RTFILE hFile = (RTFILE)pvUser;

    return RTFileWriteAt(hFile, off, pvBuf, cbWrite, NULL);
}

static int tstRTFilesystem(RTTEST hTest, RTFILE hFile, uint64_t cb)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(hTest, "Create filesystem object");
    RTFILESYSTEM hFs;
    rc = RTFilesystemOpen(&hFs, filesystemDiskRead, filesystemDiskWrite, cb, 512, hFile, 0 /* fFlags */);
    if (RT_FAILURE(rc))
    {
        RTTestIFailed("RTFilesystemOpen -> %Rrc", rc);
        return rc;
    }

    RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully opened filesystem with format: %s.\n",
                  RTFilesystemGetFormat(hFs));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Block size is: %llu.\n",
                  RTFilesystemGetBlockSize(hFs));

    /* Check all blocks. */
    uint64_t off = 0;
    uint32_t cBlocksUsed = 0;
    uint32_t cBlocksUnused = 0;

    while (off < cb)
    {
        bool fUsed = false;

        rc = RTFilesystemQueryRangeUse(hFs, off, 1024, &fUsed);
        if (RT_FAILURE(rc))
        {
            RTTestIFailed("RTFileSysQueryRangeUse -> %Rrc", rc);
            break;
        }

        if (fUsed)
            cBlocksUsed++;
        else
            cBlocksUnused++;

        off += 1024;
    }

    if (RT_SUCCESS(rc))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%u blocks used and %u blocks unused\n",
                      cBlocksUsed, cBlocksUnused);

    RTFilesystemRelease(hFs);

    return rc;
}

int main(int argc, char **argv)
{
    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTFilesystem", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * If no args, display usage.
     */
    if (argc < 2)
    {
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Syntax: %s <image>\n", argv[0]);
        return RTTestSkipAndDestroy(hTest, "Missing required arguments\n");
    }

    /* Open image. */
    RTFILE hFile;
    uint64_t cb = 0;
    rc = RTFileOpen(&hFile, argv[1], RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ);
    if (RT_FAILURE(rc))
    {
        RTTestIFailed("RTFileOpen -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    rc = RTFileGetSize(hFile, &cb);
    if (   RT_FAILURE(rc)
        || cb % 512 != 0) /* Assume 512 byte sector size. */
    {
        RTTestIFailed("RTFileGetSize -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    rc = tstRTFilesystem(hTest, hFile, cb);

    RTTESTI_CHECK(rc == VINF_SUCCESS);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

