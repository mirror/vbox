/* $Id$ */
/** @file
 * Apps - VBoxAdpCtl, Configuration tool for vboxnetX adapters.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define VBOXADPCTL_IFCONFIG_PATH "/sbin/ifconfig"

static void showUsage(void)
{
    fprintf(stderr, "Usage: VBoxNetAdpCtl <adapter> <address> [netmask <address>]\n");
}

static int doExec(char *pszAdapterName, char *pszAddress, char *pszNetworkMask)
{
    char *argv[] =
    {
        VBOXADPCTL_IFCONFIG_PATH,
        pszAdapterName,
        NULL, /* [address family] */
        NULL, /* address */
        NULL, /* ['netmask'] */
        NULL, /* [network mask] */
        NULL  /* terminator */
    };
    int i = 2; /* index in argv. we start from address family */

    if (strchr(pszAddress, ':'))
        argv[i++] = "inet6";
    argv[i++] = pszAddress;
    if (pszNetworkMask)
    {
        argv[i++] = "netmask";
        argv[i++] = pszNetworkMask;
    }

    return (execv(VBOXADPCTL_IFCONFIG_PATH, argv) == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int executeIfconfig(char *pszAdapterName, char *pszAddress, char *pszNetworkMask)
{
    int rc = EXIT_SUCCESS;
    pid_t childPid = fork();
    switch (childPid)
    {
        case -1: /* Something went wrong. */
            perror("fork() failed");
            rc = EXIT_FAILURE;
            break;
        case 0: /* Child process. */
            rc = doExec(pszAdapterName, pszAddress, pszNetworkMask);
            break;
        default: /* Parent process. */
            waitpid(childPid, &rc, 0);
            break;
    }

    return rc;
}

int main(int argc, char *argv[])

{
    char *pszAdapterName;
    char *pszAddress;
    char *pszNetworkMask = NULL;

    switch (argc)
    {
        case 5:
            if (strcmp("netmask", argv[3]))
            {
                fprintf(stderr, "Invalid argument: %s\n\n", argv[3]);
                showUsage();
                return 1;
            }
            pszNetworkMask = argv[4];
            /* Fall through */
        case 3:
            pszAdapterName = argv[1];
            pszAddress = argv[2];
            break;
        default:
            fprintf(stderr, "Invalid number of arguments.\n\n");
            /* Fall through */
        case 1:
            showUsage();
            return 1;
    }

    if (strcmp("vboxnet0", pszAdapterName))
    {
        fprintf(stderr, "Setting configuration for %s is not supported.\n", pszAdapterName);
        return 2;
    }

    return executeIfconfig(pszAdapterName, pszAddress, pszNetworkMask);
}
