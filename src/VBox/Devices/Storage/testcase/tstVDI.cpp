/** @file
 *
 * Simple VBox HDD container test utility.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/err.h>
#include <VBox/VBoxHDD.h>
#include <iprt/runtime.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/mem.h>


int dotest(const char *pszBaseFilename, const char *pszDiffFilename)
{
    PVDIDISK pVdi = VDIDiskCreate();

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Vrc\n", str, rc); \
        if (VBOX_FAILURE(rc)) \
        { \
            VDIDiskCloseAllImages(pVdi); \
            return rc; \
        } \
    } while (0)


    int rc = VDIDiskOpenImage(pVdi, pszBaseFilename, VDI_OPEN_FLAGS_NORMAL);
    RTPrintf("openImage() rc=%Vrc\n", rc);
    if (VBOX_FAILURE(rc))
    {
        rc = VDICreateBaseImage(pszBaseFilename, VDI_IMAGE_TYPE_NORMAL,
#ifdef _MSC_VER
                                (1000 * 1024 * 1024UI64),
#else
                                (1000 * 1024 * 1024ULL),
#endif
                                "Test image", NULL, NULL);
        CHECK("createImage()");

        rc = VDIDiskOpenImage(pVdi, pszBaseFilename, VDI_OPEN_FLAGS_NORMAL);
        CHECK("openImage()");
    }

    void *pvBuf = RTMemAlloc(1*1124*1024);

    memset(pvBuf, 0x33, 1*1124*1024);
    rc = VDIDiskWrite(pVdi, 20*1024*1024 + 594040, pvBuf, 1024*1024);
    CHECK("write()");

    memset(pvBuf, 0x46, 1*1124*1024);
    rc = VDIDiskWrite(pVdi, 20*1024*1024 + 594040, pvBuf, 1024);
    CHECK("write()");

    memset(pvBuf, 0x51, 1*1124*1024);
    rc = VDIDiskWrite(pVdi, 40*1024*1024 + 594040, pvBuf, 1024);
    CHECK("write()");

    rc = VDIDiskCreateOpenDifferenceImage(pVdi, pszDiffFilename, "Test diff image", NULL, NULL);
    CHECK("create undo");
//    rc = VHDDOpenSecondImage(pVdi, "undoimg.vdi");
//    RTPrintf("open undo rc=%Vrc\n", rc);

    memset(pvBuf, '_', 1*1124*1024);
    rc = VDIDiskWrite(pVdi, 20*1024*1024 + 594040, pvBuf, 512);
    CHECK("write()");

    rc = VDIDiskWrite(pVdi, 22*1024*1024 + 594040, pvBuf, 78263);
    CHECK("write()");
    rc = VDIDiskWrite(pVdi, 13*1024*1024 + 594040, pvBuf, 782630);
    CHECK("write()");
    rc = VDIDiskWrite(pVdi, 44*1024*1024 + 594040, pvBuf, 67899);
    CHECK("write()");

    RTPrintf("committing..\n");
    VDIDiskDumpImages(pVdi);
    rc = VDIDiskCommitLastDiff(pVdi, NULL, NULL);
    CHECK("commit last diff");
    VDIDiskCloseAllImages(pVdi);
#undef CHECK
    return 0;
}


int main()
{
    RTR3Init();

    RTFileDelete("tmpVdiBase.vdi");
    RTFileDelete("tmpVdiDiff.vdi");

    int rc = dotest("tmpVdiBase.vdi", "tmpVdiDiff.vdi");
    if (!rc)
        RTPrintf("tstVDI: SUCCESS\n");
    else
        RTPrintf("tstVDI: FAILURE\n");

    RTFileDelete("tmpVdiBase.vdi");
    RTFileDelete("tmpVdiDiff.vdi");
    return !!rc;
}

