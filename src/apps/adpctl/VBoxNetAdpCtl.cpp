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
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef RT_OS_SOLARIS
# include <sys/ioccom.h>
#endif

/** @todo Error codes must be moved to some header file */
#define ADPCTLERR_BAD_NAME         2
#define ADPCTLERR_NO_CTL_DEV       3
#define ADPCTLERR_IOCTL_FAILED     4

/** @todo These are duplicates from src/VBox/HostDrivers/VBoxNetAdp/VBoxNetAdpInternal.h */
#define VBOXNETADP_CTL_DEV_NAME    "/dev/vboxnetctl"
#define VBOXNETADP_NAME            "vboxnet"
#define VBOXNETADP_MAX_NAME_LEN    32
#define VBOXNETADP_CTL_ADD    _IOR('v', 1, VBOXNETADPREQ)
#define VBOXNETADP_CTL_REMOVE _IOW('v', 2, VBOXNETADPREQ)
typedef struct VBoxNetAdpReq
{
    char szName[VBOXNETADP_MAX_NAME_LEN];
} VBOXNETADPREQ;
typedef VBOXNETADPREQ *PVBOXNETADPREQ;


#define VBOXADPCTL_IFCONFIG_PATH "/sbin/ifconfig"

#ifdef RT_OS_LINUX
#define VBOXADPCTL_DEL_CMD "del"
#else
#define VBOXADPCTL_DEL_CMD "delete"
#endif

static void showUsage(void)
{
    fprintf(stderr, "Usage: VBoxNetAdpCtl <adapter> <address> ([netmask <address>] | remove)\n");
    fprintf(stderr, "     | VBoxNetAdpCtl add\n");
    fprintf(stderr, "     | VBoxNetAdpCtl <adapter> remove\n");
}

static int executeIfconfig(const char *pcszAdapterName, const char *pcszArg1,
                           const char *pcszArg2 = NULL,
                           const char *pcszArg3 = NULL,
                           const char *pcszArg4 = NULL,
                           const char *pcszArg5 = NULL)
{
    const char * const argv[] =
    {
        VBOXADPCTL_IFCONFIG_PATH,
        pcszAdapterName,
        pcszArg1, /* [address family] */
        pcszArg2, /* address */
        pcszArg3, /* ['netmask'] */
        pcszArg4, /* [network mask] */
        pcszArg5, /* [network mask] */
        NULL  /* terminator */
    };
    char * const envp[] = { (char*)"LC_ALL=C", NULL };
    int rc = EXIT_SUCCESS;
    pid_t childPid = fork();
    switch (childPid)
    {
        case -1: /* Something went wrong. */
            perror("fork() failed");
            rc = EXIT_FAILURE;
            break;
        case 0: /* Child process. */
            if (execve(VBOXADPCTL_IFCONFIG_PATH, (char * const*)argv, envp) == -1)
                rc = EXIT_FAILURE;
            break;
        default: /* Parent process. */
            waitpid(childPid, &rc, 0);
            break;
    }

    return rc;
}

#define MAX_ADDRESSES 128
#define MAX_ADDRLEN   64

static bool removeAddresses(char *pszAdapterName)
{
    char szBuf[1024];
    char aszAddresses[MAX_ADDRESSES][MAX_ADDRLEN];
    int rc;
    int fds[2];
    char * const argv[] = { (char*)VBOXADPCTL_IFCONFIG_PATH, pszAdapterName, NULL };
    char * const envp[] = { (char*)"LC_ALL=C", NULL };

    memset(aszAddresses, 0, sizeof(aszAddresses));

    rc = pipe(fds);
    if (rc < 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        /* child */
        close(fds[0]);
        close(STDOUT_FILENO);
        rc = dup2(fds[1], STDOUT_FILENO);
        if (rc >= 0)
            execve(VBOXADPCTL_IFCONFIG_PATH, argv, envp);
        return false;
    }

    /* parent */
    close(fds[1]);
    FILE *fp = fdopen(fds[0], "r");
    if (!fp)
        return false;

    int cAddrs;
    for (cAddrs = 0; cAddrs < MAX_ADDRESSES && fgets(szBuf, sizeof(szBuf), fp);)
    {
        int cbSkipWS = strspn(szBuf, " \t");
        char *pszWord = strtok(szBuf + cbSkipWS, " ");
        /* We are concerned with IPv6 address lines only. */
        if (!pszWord || strcmp(pszWord, "inet6"))
            continue;
#ifdef RT_OS_LINUX
        pszWord = strtok(NULL, " ");
        /* Skip "addr:". */
        if (!pszWord || strcmp(pszWord, "addr:"))
            continue;
#endif
        pszWord = strtok(NULL, " ");
        /* Skip link-local addresses. */
        if (!pszWord || !strncmp(pszWord, "fe80", 4))
            continue;
        strncpy(aszAddresses[cAddrs++], pszWord, MAX_ADDRLEN-1);
    }
    fclose(fp);

    for (int i = 0; i < cAddrs; i++)
    {
        if (executeIfconfig(pszAdapterName, "inet6",
                            VBOXADPCTL_DEL_CMD, aszAddresses[i]) != EXIT_SUCCESS)
            return false;
    }

    return true;
}

int doIOCtl(unsigned long uCmd, void *pData)
{
    int fd = open(VBOXNETADP_CTL_DEV_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("VBoxNetAdpCtl: failed to open " VBOXNETADP_CTL_DEV_NAME);
        return ADPCTLERR_NO_CTL_DEV;
    }

    int rc = ioctl(fd, uCmd, pData);
    if (rc == -1)
    {
        perror("VBoxNetAdpCtl: ioctl failed for " VBOXNETADP_CTL_DEV_NAME);
        rc = ADPCTLERR_IOCTL_FAILED;
    }

    close(fd);

    return rc;
}

int checkAdapterName(const char *pcszNameIn, char *pszNameOut)
{
    int iAdapterIndex = -1;

    if (   strlen(pcszNameIn) >= VBOXNETADP_MAX_NAME_LEN
        || sscanf(pcszNameIn, "vboxnet%d", &iAdapterIndex) != 1
        || iAdapterIndex < 0 || iAdapterIndex > 99 )
    {
        fprintf(stderr, "Setting configuration for %s is not supported.\n", pcszNameIn);
        return ADPCTLERR_BAD_NAME;
    }
    sprintf(pszNameOut, "vboxnet%d", iAdapterIndex);
    if (strcmp(pszNameOut, pcszNameIn))
    {
        fprintf(stderr, "Invalid adapter name %s.\n", pcszNameIn);
        return ADPCTLERR_BAD_NAME;
    }

    return 0;
}

int main(int argc, char *argv[])

{
    char szAdapterName[VBOXNETADP_MAX_NAME_LEN];
    char *pszAdapterName;
    const char *pszAddress;
    const char *pszNetworkMask = NULL;
    const char *pszOption = NULL;
    int rc = EXIT_SUCCESS;
    bool fRemove = false;
    VBOXNETADPREQ Req;

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
            fRemove = true;
            pszAdapterName = argv[1];
            pszAddress = argv[2];
            break;
        case 3:
            pszAdapterName = argv[1];
            pszAddress = argv[2];
            if (strcmp("remove", pszAddress) == 0)
            {
                rc = checkAdapterName(pszAdapterName, szAdapterName);
                if (rc)
                    return rc;
                memset(&Req, '\0', sizeof(Req));
                snprintf(Req.szName, sizeof(Req.szName), "%s", szAdapterName);
                return doIOCtl(VBOXNETADP_CTL_REMOVE, &Req);
            }
            break;
        case 2:
            if (strcmp("add", argv[1]) == 0)
            {
                memset(&Req, '\0', sizeof(Req));
                rc = doIOCtl(VBOXNETADP_CTL_ADD, &Req);
                if (rc == 0)
                    puts(Req.szName);
                return rc;
            }
            /* Fall through */
        default:
            fprintf(stderr, "Invalid number of arguments.\n\n");
            /* Fall through */
        case 1:
            showUsage();
            return 1;
    }

    rc = checkAdapterName(pszAdapterName, szAdapterName);
    if (rc)
        return rc;

    pszAdapterName = szAdapterName;

    if (fRemove)
    {
        if (strchr(pszAddress, ':'))
            rc = executeIfconfig(pszAdapterName, "inet6", VBOXADPCTL_DEL_CMD, pszAddress);
        else
        {
#ifdef RT_OS_LINUX
            rc = executeIfconfig(pszAdapterName, "0.0.0.0");
#else
            rc = executeIfconfig(pszAdapterName, "delete", pszAddress);
#endif
        }
    }
    else
    {
        /* We are setting/replacing address. */
        if (strchr(pszAddress, ':'))
        {
            /*
             * Before we set IPv6 address we'd like to remove
             * all previously assigned addresses except the
             * self-assigned one.
             */
            if (!removeAddresses(pszAdapterName))
                rc = EXIT_FAILURE;
            else
                rc = executeIfconfig(pszAdapterName, "inet6", "add", pszAddress, pszOption, pszNetworkMask);
        }
        else
            rc = executeIfconfig(pszAdapterName, pszAddress, pszOption, pszNetworkMask);
    }
    return rc;
}

