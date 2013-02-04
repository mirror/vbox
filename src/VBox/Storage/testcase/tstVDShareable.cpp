/* $Id$ */
/** @file
 * Simple VBox HDD container test utility for shareable images.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vd.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/dir.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include "stdio.h"
#include "stdlib.h"

#define VHD_TEST
#define VDI_TEST
#define VMDK_TEST

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;


static void tstVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                       const char *pszFormat, va_list va)
{
    g_cErrors++;
    RTPrintf("tstVD: Error %Rrc at %s:%u (%s): ", rc, RT_SRC_POS_ARGS);
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
}

static int tstVDMessage(void *pvUser, const char *pszFormat, va_list va)
{
    RTPrintf("tstVD: ");
    RTPrintfV(pszFormat, va);
    return VINF_SUCCESS;
}

static int tstVDCreateShareDelete(const char *pszBackend, const char *pszFilename,
                                  uint64_t cbSize, unsigned uFlags)
{
    int rc;
    PVBOXHDD pVD = NULL, pVD2 = NULL;
    VDGEOMETRY       PCHS = { 0, 0, 0 };
    VDGEOMETRY       LCHS = { 0, 0, 0 };
    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACEERROR VDIfError;

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Rrc\n", str, rc); \
        if (RT_FAILURE(rc)) \
        { \
            VDDestroy(pVD); \
            return rc; \
        } \
    } while (0)

    /* Create error interface. */
    VDIfError.pfnError = tstVDError;
    VDIfError.pfnMessage = tstVDMessage;

    rc = VDInterfaceAdd(&VDIfError.Core, "tstVD_Error", VDINTERFACETYPE_ERROR,
                        NULL, sizeof(VDINTERFACEERROR), &pVDIfs);
    AssertRC(rc);

    rc = VDCreate(pVDIfs, VDTYPE_HDD, &pVD);
    CHECK("VDCreate()");
    rc = VDCreate(pVDIfs, VDTYPE_HDD, &pVD2);
    CHECK("VDCreate() #2");

    rc = VDCreateBase(pVD, pszBackend, pszFilename, cbSize,
                      uFlags, "Test image", &PCHS, &LCHS, NULL,
                      VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    CHECK("VDCreateBase()");

    VDClose(pVD, false);

    rc = VDOpen(pVD, pszBackend, pszFilename, VD_OPEN_FLAGS_SHAREABLE, NULL);
    CHECK("VDOpen()");
    rc = VDOpen(pVD2, pszBackend, pszFilename, VD_OPEN_FLAGS_SHAREABLE, NULL);
    CHECK("VDOpen() #2");
    if (VDIsReadOnly(pVD2))
        rc = VERR_VD_IMAGE_READ_ONLY;

    VDClose(pVD2, false);
    VDClose(pVD, true);

    VDDestroy(pVD);
    VDDestroy(pVD2);
#undef CHECK
    return 0;
}

int main(int argc, char *argv[])
{
    RTR3InitExe(argc, &argv, 0);
    int rc;

    RTPrintf("tstVD: TESTING...\n");

    /*
     * Clean up potential leftovers from previous unsuccessful runs.
     */
    RTFileDelete("tmpVDCreate.vdi");

    if (!RTDirExists("tmp"))
    {
        rc = RTDirCreate("tmp", RTFS_UNIX_IRWXU, 0);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstVD: Failed to create 'tmp' directory! rc=%Rrc\n", rc);
            g_cErrors++;
        }
    }

#ifdef VDI_TEST
    rc = tstVDCreateShareDelete("VDI", "tmpVDCreate.vdi", 10 * _1M,
                                VD_IMAGE_FLAGS_FIXED);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVD: VDI shareable test failed! rc=%Rrc\n", rc);
        g_cErrors++;
    }
#endif /* VDI_TEST */

    /*
     * Clean up any leftovers.
     */
    RTFileDelete("tmpVDCreate.vdi");

    rc = VDShutdown();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVD: unloading backends failed! rc=%Rrc\n", rc);
        g_cErrors++;
    }
     /*
      * Summary
      */
    if (!g_cErrors)
        RTPrintf("tstVD: SUCCESS\n");
    else
        RTPrintf("tstVD: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

