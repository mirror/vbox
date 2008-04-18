/** @file
 *
 * VirtualBox timesync daemon for Linux
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <VBox/VBoxGuest.h>
#include <VBox/err.h>

static void usage(void)
{
    printf("vboxadd-timesync [--interval <seconds>]\n"
           "                 [--daemonize]\n");
}

static void safe_sleep(time_t seconds)
{
    struct timespec requestedSleepTime;

    requestedSleepTime.tv_sec = seconds;
    requestedSleepTime.tv_nsec = 0;

    for (;;)
    {
        int err;
        struct timespec remainingSleepTime;

        err = nanosleep(&requestedSleepTime, &remainingSleepTime);
        if (err)
        {
            /* if the sleep was interrupted, remember remaining sleep time */
            if (errno == EINTR)
            {
                requestedSleepTime = remainingSleepTime;
            }
            else
            {
                /* not good... */
            }
        }
        else
        {
            /* nanosleep completed and took at least the requested time */
            break;
        }
    }
}


int main(int argc, char *argv[])
{
    const struct option options[] =
    {
        { "interval",  required_argument, NULL, 'i' },
        { "daemonize", no_argument,       NULL, 'd' },
        { "help",      no_argument,       NULL, 'h' },
        { NULL,        0,                 NULL, 0   }
    };
    int secInterval = 10;
    int fDaemonize = 0;
    int c, fd;
    VMMDevReqHostTime req;

    /* command line parsing */
    for (;;)
    {
        c = getopt_long(argc, argv, "", options, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'i':
            {
                secInterval = atoi(optarg);
                break;
            }

            case 'd':
            {
                fDaemonize = 1;
                break;
            }

            case 'h':
            {
                usage();
                return 0;
            }

            case ':':
            case '?':
            {
                /* unrecognized option (?) or parameter missing (:) */
                return 1;
            }

            default:
                break;
        }
    }

    /* open the driver */
    fd = open(VBOXGUEST_DEVICE_NAME, O_RDWR, 0);
    if (fd < 0)
    {
        printf("Error opening kernel module! rc = %d\n", errno);
        return 1;
    }

    /* prepare the request structure */
    vmmdevInitRequest((VMMDevRequestHeader*)&req, VMMDevReq_GetHostTime);

    if (!fDaemonize)
        printf("VirtualBox timesync daemon.\n"
               "(C) 2008 Sun Microsystems, Inc.\n"
               "\nSync interval: %d seconds.\n", secInterval);

    if (fDaemonize)
        daemon(1, 0);

    do
    {
        /* perform VMM request */
        if (ioctl(fd, IOCTL_VBOXGUEST_VMMREQUEST, (void*)&req) >= 0)
        {
            if (VBOX_SUCCESS(req.header.rc))
            {
                /** Set the system time.
                 *  @@todo r=frank: This is too choppy. Adapt time smoother and try
                 *                  to prevent negative time differences. */
                struct timeval tv;
                tv.tv_sec  =  req.time / (uint64_t)1000;
                tv.tv_usec = (req.time % (uint64_t)1000) * 1000;
                settimeofday(&tv, NULL);
            }
            else
            {
                printf("Error querying host time! header.rc = %d\n", req.header.rc);
            }
        }
        else
        {
            printf("Error performing VMM request! errno = %d\n", errno);
        }
	    /* wait for the next run */
        safe_sleep(secInterval);

    } while (1);

    close(fd);

    return 0;
}
