/** @file
 *
 * Simple VBox HDD container test utility.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

#include <VBox/err.h>
#include <VBox/VBoxHDD.h>
#include <iprt/runtime.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h>

int main ()
{
    int rc;
    RTR3Init();

    PVDIDISK vhdd = VDIDiskCreate();

    rc = VDIDiskOpenImage(vhdd, "testimg.vdi", VDI_OPEN_FLAGS_NORMAL);
    printf("openImage() rc=%d\r\n", rc);

    if (VBOX_FAILURE(rc))
    {
        rc = VDICreateBaseImage("testimg.vdi", VDI_IMAGE_TYPE_NORMAL,
#ifdef _MSC_VER
                                (1000 * 1024 * 1024UI64),
#else
                                (1000 * 1024 * 1024ULL),
#endif
                                "Test image", NULL, NULL);
        printf ("createImage() rc=%d\r\n", rc);

        rc = VDIDiskOpenImage(vhdd, "testimg.vdi", VDI_OPEN_FLAGS_NORMAL);
        printf ("openImage() rc=%d\r\n", rc);
    }

    void *buf = malloc(1*1124*1024);

    memset(buf, 0x33, 1*1124*1024);
    rc = VDIDiskWrite(vhdd, 20*1024*1024 + 594040, buf, 1024*1024);
    printf ("write() rc=%d\r\n", rc);

    memset(buf, 0x46, 1*1124*1024);
    rc = VDIDiskWrite(vhdd, 20*1024*1024 + 594040, buf, 1024);
    printf ("write() rc=%d\r\n", rc);

    memset(buf, 0x51, 1*1124*1024);
    rc = VDIDiskWrite(vhdd, 40*1024*1024 + 594040, buf, 1024);
    printf ("write() rc=%d\r\n", rc);

    rc = VDIDiskCreateOpenDifferenceImage(vhdd, "diffimg.vdi", "Test diff image", NULL, NULL);
    printf("create undo rc=%d\r\n", rc);
//    rc = VHDDOpenSecondImage(vhdd, "undoimg.vdi");
//    printf ("open undo rc=%d\r\n", rc);

    memset(buf, '_', 1*1124*1024);
    rc = VDIDiskWrite(vhdd, 20*1024*1024 + 594040, buf, 512);
    printf("write() rc=%d\r\n", rc);

    rc = VDIDiskWrite(vhdd, 22*1024*1024 + 594040, buf, 78263);
    printf("write() rc=%d\r\n", rc);
    rc = VDIDiskWrite(vhdd, 13*1024*1024 + 594040, buf, 782630);
    printf("write() rc=%d\r\n", rc);
    rc = VDIDiskWrite(vhdd, 44*1024*1024 + 594040, buf, 67899);
    printf("write() rc=%d\r\n", rc);

    printf("committing..\r\n");
    VDIDiskDumpImages(vhdd);
    rc = VDIDiskCommitLastDiff(vhdd, NULL, NULL);
    printf("commit last diff rc=%d\r\n", rc);
    VDIDiskCloseAllImages(vhdd);

    return (0);
}

