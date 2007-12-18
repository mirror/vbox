/** $Id$ */
/** @file
 * VBoxService - Guest Additions Service Skeleton.
 */

/*
 * Copyright (C) 2007 innotek GmbH
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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

/**
 * Solaris daemon() call. Probably might work for other Unix.
 *
 * @returns 0 on success
 */
int daemon(int nochdir, int noclose)
{
    if (getppid() == 1) /* We already belong to init process */
        return -1;

    int pid = fork();
    if (pid < 0)         /* The fork() failed. Bad. */
        return -1;

    if (pid > 0)         /* Quit parent process */
        exit(0);

    /*
     * The orphaned child becomes a daemon after attaching to init. We need to get
     * rid of signals, file descriptors & other stuff we inherited from the parent.
     */
    setsid();

    int size = getdtablesize();
    if (size < 0)
        size = 3;

    for (int i = size; i >= 0; i--)
        close(i);

    /* Open stdin(0), stdout(1) and stderr(2) to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    dup(fd);
    dup(fd);

    /* Switch our current directory to root */
    if (!nochdir)
        chdir("/");     /* @todo Check if switching to '/' is the convention for Solaris daemons. */

    /* Set file permission to something secure. */
    umask(027);
    if (noclose)        /* Presuming this means ignore SIGTERM, SIGHUP... */
    {
        signal(SIGTERM, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
    }
    return 0;
}

