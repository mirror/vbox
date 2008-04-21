/* $Id$ */
/** @file
 * IPRT Testcase - file IoCtl.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#ifdef RT_OS_L4
extern char **__environ;
char *myenv[] = { "+all.e", NULL };
#endif

int main()
{
#ifdef RT_OS_L4
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
