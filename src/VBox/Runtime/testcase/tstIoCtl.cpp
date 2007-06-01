/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - file IoCtl.
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
//#include <sys/types.h>
#include "soundcard.h"
//#include <VBox/pdm.h>
#include <VBox/err.h>

#include <iprt/log.h>
#include <VBox/log.h>
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
//#include <iprt/assert.h>
//#include <iprt/uuid.h>
//#include <iprt/string.h>
//#include <iprt/alloc.h>
#include <iprt/file.h>

//#include "audio.h"
//#include "audio_int.h"

#include <stdio.h>
#include <iprt/uuid.h>

#ifdef __L4ENV__
extern char **__environ;
char *myenv[] = { "+all.e", NULL };
#endif

int main()
{
#ifdef __L4ENV__
    __environ = myenv;
#endif
    int         rcRet = 0;
    int ret, err;
    printf("tstIoCtl: TESTING\n");

    RTFILE    File;

    if (RT_FAILURE(err = RTFileOpen(&File, "/dev/dsp", (RTFILE_O_READWRITE) | RTFILE_O_NON_BLOCK))) {
        printf("Fatal error: failed to open /dev/dsp:\n"
               "VBox error code: %d\n", err);
        return 1;
    }

    int rate = 100;

    if (RT_FAILURE(err = RTFileIoCtl(File, SNDCTL_DSP_SPEED, &rate, sizeof(rate), &ret)) || ret) {
        printf("Failed to set playback speed on /dev/dsp\n"
               "VBox error code: %d, IOCTL return value: %d\n",
               err, ret);
        rcRet++;
    } else printf("Playback speed successfully set to 100, reported speed is %d\n",
        rate);

    rate = 48000;

    if (RT_FAILURE(err = RTFileIoCtl(File, SNDCTL_DSP_SPEED, &rate, sizeof(rate), &ret)) || ret) {
        printf("Failed to set playback speed on /dev/dsp\n"
               "VBox error code: %d, IOCTL return value: %d\n",
               err, ret);
        rcRet++;
    } else printf("Playback speed successfully set to 48000, reported speed is %d\n",
        rate);

    /*
     * Cleanup.
     */
    ret = RTFileClose(File);
    if (RT_FAILURE(ret))
    {
        printf("Failed to close /dev/dsp. ret=%d\n", ret);
        rcRet++;
    }

    /* Under Linux and L4, this involves ioctls internally */
    RTUUID TestUuid;
    if (RT_FAILURE(RTUuidCreate(&TestUuid)))
    {
        printf("Failed to create a UUID. ret=%d\n", ret);
        rcRet++;
    }

    /*
     * Summary
     */
    if (rcRet == 0)
        printf("tstIoCtl: SUCCESS\n");
    else
        printf("tstIoCtl: FAILURE - %d errors\n", rcRet);
    return rcRet;
}
