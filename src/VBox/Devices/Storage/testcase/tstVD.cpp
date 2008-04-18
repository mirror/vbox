/** @file
 *
 * Simple VBox HDD container test utility.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/err.h>
#include <VBox/VBoxHDD-new.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;


static void tstVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                       const char *pszFormat, va_list va)
{
    g_cErrors++;
    RTPrintf("tstVD: Error %Vrc at %s:%u (%s): ");
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
}


static int tstVDOpenCreateWriteMerge(const char *pszBackend,
                                     const char *pszBaseFilename,
                                     const char *pszDiffFilename)
{
    int rc;
    PVBOXHDD pVD = NULL;
    char *pszFormat;
    PDMMEDIAGEOMETRY PCHS = { 0, 0, 0 };
    PDMMEDIAGEOMETRY LCHS = { 0, 0, 0 };

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Vrc\n", str, rc); \
        if (VBOX_FAILURE(rc)) \
        { \
            VDCloseAll(pVD); \
            return rc; \
        } \
    } while (0)

    rc = VDCreate(tstVDError, NULL, &pVD);
    CHECK("VDCreate()");

    RTFILE File;
    rc = RTFileOpen(&File, pszBaseFilename, RTFILE_O_READ);
    if (VBOX_SUCCESS(rc))
    {
        RTFileClose(File);
        rc = VDGetFormat(pszBaseFilename, &pszFormat);
        RTPrintf("VDGetFormat() pszFormat=%s rc=%Vrc\n", pszFormat, rc);
        if (VBOX_SUCCESS(rc) && strcmp(pszFormat, pszBackend))
        {
            rc = VERR_GENERAL_FAILURE;
            RTPrintf("VDGetFormat() returned incorrect backend name\n");
        }
        RTStrFree(pszFormat);
        CHECK("VDGetFormat()");

        rc = VDOpen(pVD, pszBackend, pszBaseFilename, VD_OPEN_FLAGS_NORMAL);
        CHECK("VDOpen()");
    }
    else
    {
        rc = VDCreateBase(pVD, pszBackend, pszBaseFilename,
                          VD_IMAGE_TYPE_NORMAL, 1000 * _1M,
                          VD_IMAGE_FLAGS_NONE, "Test image",
                          &PCHS, &LCHS, VD_OPEN_FLAGS_NORMAL,
                          NULL, NULL);
        CHECK("VDCreateBase()");
    }

    void *pvBuf = RTMemAlloc(_1M);

    memset(pvBuf, 0x33, _1M);
    rc = VDWrite(pVD, 20 * _1M + 594432, pvBuf, _1M);
    CHECK("VDWrite()");

    memset(pvBuf, 0x46, _1M);
    rc = VDWrite(pVD, 20 * _1M + 594432, pvBuf, _1K);
    CHECK("VDWrite()");

    memset(pvBuf, 0x51, _1M);
    rc = VDWrite(pVD, 40 * _1M + 594432, pvBuf, _1K);
    CHECK("VDWrite()");

    rc = VDCreateDiff(pVD, pszBackend, pszDiffFilename,
                      VD_IMAGE_FLAGS_NONE, "Test diff image",
                      VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    CHECK("VDCreateDiff()");

    memset(pvBuf, '_', _1M);
    rc = VDWrite(pVD, 20 * _1M + 594432, pvBuf, 512);
    CHECK("VDWrite()");

    rc = VDWrite(pVD, 22 * _1M + 594432, pvBuf, 78336);
    CHECK("VDWrite()");
    rc = VDWrite(pVD, 13 * _1M + 594432, pvBuf, 783360);
    CHECK("VDWrite()");
    rc = VDWrite(pVD, 44 * _1M + 594432, pvBuf, 68096);
    CHECK("VDWrite()");

    VDDumpImages(pVD);

    RTPrintf("Merging diff into base..\n");
    rc = VDMerge(pVD, -1, 0, NULL, NULL);
    CHECK("VDMerge()");

    VDDumpImages(pVD);

    VDCloseAll(pVD);
#undef CHECK
    return 0;
}


int main()
{
    int rc;

    RTR3Init();
    RTPrintf("tstVD: TESTING...\n");

    /*
     * Clean up potential leftovers from previous unsuccessful runs.
     */
    RTFileDelete("tmpVDBase.vdi");
    RTFileDelete("tmpVDDiff.vdi");
    RTFileDelete("tmpVDBase.vmdk");
    RTFileDelete("tmpVDDiff.vmdk");

    rc = tstVDOpenCreateWriteMerge("VDI", "tmpVDBase.vdi", "tmpVDDiff.vdi");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VDI test failed (new image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VDI", "tmpVDBase.vdi", "tmpVDDiff.vdi");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VDI test failed (existing image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VMDK", "tmpVDBase.vmdk", "tmpVDDiff.vmdk");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VMDK test failed (new image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VMDK", "tmpVDBase.vmdk", "tmpVDDiff.vmdk");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VMDK test failed (existing image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }

    /*
     * Clean up any leftovers.
     */
    RTFileDelete("tmpVDBase.vdi");
    RTFileDelete("tmpVDDiff.vdi");
    RTFileDelete("tmpVDBase.vmdk");
    RTFileDelete("tmpVDDiff.vmdk");

    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstVD: SUCCESS\n");
    else
        RTPrintf("tstVD: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

