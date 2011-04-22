/* $Id$ */
/** @file
 * IPRT Testcase - IPRT Disk Volume Management (DVM)
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <iprt/test.h>
#include <iprt/dvm.h>
#include <iprt/file.h>

RTFILE   g_hDisk = NIL_RTFILE;  /**< The disk image. */
uint64_t g_cbDisk = 0;          /**< Size of the image. */

static int dvmDiskRead(void *pvUser, uint64_t off, void *pvBuf, size_t cbRead)
{
    return RTFileReadAt(g_hDisk, off, pvBuf, cbRead, NULL);
}

static int dvmDiskWrite(void *pvUser, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    return RTFileWriteAt(g_hDisk, off, pvBuf, cbWrite, NULL);
}

int main(int argc, char **argv)
{
    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTDvm", &hTest);
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
    rc = RTFileOpen(&g_hDisk, argv[1], RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE);
    if (RT_FAILURE(rc))
    {
        RTTestIFailed("RTFileOpen -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    rc = RTFileGetSize(g_hDisk, &g_cbDisk);
    if (   RT_FAILURE(rc)
        || g_cbDisk % 512 != 0) /* Assume 512 byte sector size. */
    {
        RTTestIFailed("RTFileGetSize -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    RTTestSubF(hTest, "Create DVM");
    RTDVM hVolMgr;
    rc = RTDvmCreate(&hVolMgr, dvmDiskRead, dvmDiskWrite, g_cbDisk, 512, NULL);
    if (RT_FAILURE(rc))
    {
        RTTestIFailed("RTDvmCreate -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    RTTestSubF(hTest, "Open volume map");
    rc = RTDvmMapOpen(hVolMgr);
    if (RT_FAILURE(rc))
    {
        RTTestIFailed("RTDvmOpen -> %Rrc", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    RTTestIPrintf(RTTESTLVL_ALWAYS, " Successfully opened map with format: %s.\n", RTDvmMapGetFormat(hVolMgr));

    /* Dump all volumes. */
    RTTestSubF(hTest, "Dump volumes");
    uint32_t cVolume = 0;
    RTDVMVOLUME hVol;

    rc = RTDvmMapQueryFirstVolume(hVolMgr, &hVol);

    while (RT_SUCCESS(rc))
    {
        RTDVMVOLTYPE enmVolType = RTDvmVolumeGetType(hVol);
        uint64_t fVolFlags = RTDvmVolumeGetFlags(hVol);

        RTTestIPrintf(RTTESTLVL_ALWAYS, " Volume %u:\n", cVolume);
        RTTestIPrintf(RTTESTLVL_ALWAYS, " Volume type  %s.\n", RTDvmVolumeTypeGetDescr(enmVolType));
        RTTestIPrintf(RTTESTLVL_ALWAYS, " Volume size  %llu.\n", RTDvmVolumeGetSize(hVol));
        RTTestIPrintf(RTTESTLVL_ALWAYS, " Volume flags %s %s.\n\n", 
                      fVolFlags & DVMVOLUME_FLAGS_BOOTABLE ? "Bootable" : "",
                      fVolFlags & DVMVOLUME_FLAGS_ACTIVE ? "Active" : "");

        RTDVMVOLUME hVolNext;
        rc = RTDvmMapQueryNextVolume(hVolMgr, hVol, &hVolNext);
        RTDvmVolumeRelease(hVol);
        hVol = hVolNext;
        cVolume++;
    }

    RTTestIPrintf(RTTESTLVL_ALWAYS, " Dumped %u volumes\n", cVolume);

    if (   rc == VERR_DVM_MAP_EMPTY
        || rc == VERR_DVM_MAP_NO_VOLUME)
        rc = VINF_SUCCESS;

    RTTESTI_CHECK(rc == VINF_SUCCESS);

    RTDvmRelease(hVolMgr);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

