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
#include <string>
#include <list>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define VBOXADPCTL_IFCONFIG_PATH "/sbin/ifconfig"

static void showUsage(void)
{
    fprintf(stderr, "Usage: VBoxNetAdpCtl <adapter> <address> ([netmask <address>] | remove)\n");
}

static int executeIfconfig(const char *pcszAdapterName, const char *pcszArg1,
                           const char *pcszArg2 = NULL,
                           const char *pcszArg3 = NULL,
                           const char *pcszArg4 = NULL)
{
    const char * const argv[] =
    {
        VBOXADPCTL_IFCONFIG_PATH,
        pcszAdapterName,
        pcszArg1, /* [address family] */
        pcszArg2, /* address */
        pcszArg3, /* ['netmask'] */
        pcszArg4, /* [network mask] */
        NULL  /* terminator */
    };
    int rc = EXIT_SUCCESS;
    pid_t childPid = fork();
    switch (childPid)
    {
        case -1: /* Something went wrong. */
            perror("fork() failed");
            rc = EXIT_FAILURE;
            break;
        case 0: /* Child process. */
            if (execv(VBOXADPCTL_IFCONFIG_PATH, (char * const*)argv) == -1)
                rc = EXIT_FAILURE;
            break;
        default: /* Parent process. */
            waitpid(childPid, &rc, 0);
            break;
    }

    return rc;
}

static bool removeAddresses(char *pszAdapterName)
{
    static char szCmd[1024], szBuf[1024];

    snprintf(szCmd, sizeof(szCmd), VBOXADPCTL_IFCONFIG_PATH " %s", pszAdapterName);
    FILE *fp = popen(szCmd, "r");

    if (!fp)
        return false;

    std::list<std::string> Addresses;

    while (fgets(szBuf, sizeof(szBuf), fp))
    {
        int cbSkipWS = strspn(szBuf, " \t");
        assert(cbSkipWS < 20);
        char *pszWord = strtok(szBuf + cbSkipWS, " ");
        /* We are concerned with IPv6 address lines only. */
        if (!pszWord || strcmp(pszWord, "inet6"))
            continue;
        pszWord = strtok(NULL, " ");
        /* Skip link-local addresses. */
        if (!pszWord || !strncmp(pszWord, "fe80", 4))
            continue;
        Addresses.push_back(std::string(pszWord));
    }
    pclose(fp);

    std::list<std::string>::const_iterator it;
    for (it = Addresses.begin(); it != Addresses.end(); it++)
    {
        if (executeIfconfig(pszAdapterName, "inet6", it->c_str(), "remove") != EXIT_SUCCESS)
            return false;
    }

    return true;
}


int main(int argc, char *argv[])

{
    char *pszAdapterName;
    char *pszAddress;
    char *pszNetworkMask = NULL;
    char *pszOption = NULL;
    int rc = EXIT_SUCCESS;

    switch (argc)
    {
        case 5:
            if (strcmp("netmask", argv[3]))
            {
                fprintf(stderr, "Invalid argument: %s\n\n", argv[3]);
                showUsage();
                return 1;
            }
            pszOption = "netmask";
            pszNetworkMask = argv[4];
            pszAdapterName = argv[1];
            pszAddress = argv[2];
            break;
        case 4:
            if (strcmp("remove", argv[3]))
            {
                fprintf(stderr, "Invalid argument: %s\n\n", argv[3]);
                showUsage();
                return 1;
            }
            pszOption = "remove";
            pszAdapterName = argv[1];
            pszAddress = argv[2];
            break;
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

    if (strchr(pszAddress, ':'))
    {
        /*
         * Before we set IPv6 address we'd like to remove
         * all previously assigned addresses except the
         * self-assigned one.
         */
        if (pszOption && !strcmp(pszOption, "remove"))
            rc = executeIfconfig(pszAdapterName, "inet6", pszAddress, pszOption);
        else if (!removeAddresses(pszAdapterName))
            rc = EXIT_FAILURE;
        else
            rc = executeIfconfig(pszAdapterName, "inet6", pszAddress, pszOption, pszNetworkMask);
    }
    else
        rc = executeIfconfig(pszAdapterName, pszAddress, pszOption, pszNetworkMask);
    return rc;
}
