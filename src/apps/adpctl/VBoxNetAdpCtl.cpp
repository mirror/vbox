/* $Id$ */
/** @file
 * Apps - VBoxAdpCtl, Configuration tool for vboxnetX adapters.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef RT_OS_LINUX
# include <net/if.h>
# include <linux/types.h>
/* Older versions of ethtool.h rely on these: */
typedef unsigned long long u64;
typedef __uint32_t u32;
typedef __uint16_t u16;
typedef __uint8_t u8;
# include <linux/ethtool.h>
# include <linux/sockios.h>
#endif
#ifdef RT_OS_SOLARIS
# include <sys/ioccom.h>
#endif

/** @todo Error codes must be moved to some header file */
#define ADPCTLERR_BAD_NAME         2
#define ADPCTLERR_NO_CTL_DEV       3
#define ADPCTLERR_IOCTL_FAILED     4
#define ADPCTLERR_SOCKET_FAILED    5

/** @todo These are duplicates from src/VBox/HostDrivers/VBoxNetAdp/VBoxNetAdpInternal.h */
#define VBOXNETADP_CTL_DEV_NAME    "/dev/vboxnetctl"
#define VBOXNETADP_MAX_INSTANCES   128
#define VBOXNETADP_NAME            "vboxnet"
#define VBOXNETADP_MAX_NAME_LEN    32
#define VBOXNETADP_CTL_ADD   _IOWR('v', 1, VBOXNETADPREQ)
#define VBOXNETADP_CTL_REMOVE _IOW('v', 2, VBOXNETADPREQ)
typedef struct VBoxNetAdpReq
{
    char szName[VBOXNETADP_MAX_NAME_LEN];
} VBOXNETADPREQ;
typedef VBOXNETADPREQ *PVBOXNETADPREQ;


#define VBOXADPCTL_IFCONFIG_PATH "/sbin/ifconfig"

#if defined(RT_OS_LINUX)
# define VBOXADPCTL_DEL_CMD "del"
# define VBOXADPCTL_ADD_CMD "add"
#elif defined(RT_OS_SOLARIS)
# define VBOXADPCTL_DEL_CMD "removeif"
# define VBOXADPCTL_ADD_CMD "addif"
#else
# define VBOXADPCTL_DEL_CMD "delete"
# define VBOXADPCTL_ADD_CMD "add"
#endif

static void showUsage(void)
{
    fprintf(stderr, "Usage: VBoxNetAdpCtl <adapter> <address> ([netmask <address>] | remove)\n");
    fprintf(stderr, "     | VBoxNetAdpCtl [<adapter>] add\n");
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

static int doIOCtl(unsigned long uCmd, VBOXNETADPREQ *pReq)
{
    int fd = open(VBOXNETADP_CTL_DEV_NAME, O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "VBoxNetAdpCtl: Error while %s %s: ",
                uCmd == VBOXNETADP_CTL_REMOVE ? "removing" : "adding",
                pReq->szName[0] ? pReq->szName : "new interface");
        perror("failed to open " VBOXNETADP_CTL_DEV_NAME);
        return ADPCTLERR_NO_CTL_DEV;
    }

    int rc = ioctl(fd, uCmd, pReq);
    if (rc == -1)
    {
        fprintf(stderr, "VBoxNetAdpCtl: Error while %s %s: ",
                uCmd == VBOXNETADP_CTL_REMOVE ? "removing" : "adding",
                pReq->szName[0] ? pReq->szName : "new interface");
        perror("VBoxNetAdpCtl: ioctl failed for " VBOXNETADP_CTL_DEV_NAME);
        rc = ADPCTLERR_IOCTL_FAILED;
    }

    close(fd);

    return rc;
}

static int checkAdapterName(const char *pcszNameIn, char *pszNameOut)
{
    int iAdapterIndex = -1;

    if (   strlen(pcszNameIn) >= VBOXNETADP_MAX_NAME_LEN
        || sscanf(pcszNameIn, "vboxnet%d", &iAdapterIndex) != 1
        || iAdapterIndex < 0 || iAdapterIndex >= VBOXNETADP_MAX_INSTANCES )
    {
        fprintf(stderr, "VBoxNetAdpCtl: Setting configuration for '%s' is not supported.\n", pcszNameIn);
        return ADPCTLERR_BAD_NAME;
    }
    sprintf(pszNameOut, "vboxnet%d", iAdapterIndex);
    if (strcmp(pszNameOut, pcszNameIn))
    {
        fprintf(stderr, "VBoxNetAdpCtl: Invalid adapter name '%s'.\n", pcszNameIn);
        return ADPCTLERR_BAD_NAME;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    char szAdapterName[VBOXNETADP_MAX_NAME_LEN];
    char *pszAdapterName = NULL;
    const char *pszAddress = NULL;
    const char *pszNetworkMask = NULL;
    const char *pszOption = NULL;
    int rc = EXIT_SUCCESS;
    bool fRemove = false;
    VBOXNETADPREQ Req;

    switch (argc)
    {
        case 5:
        {
            /* Add a netmask to existing interface */
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
        }

        case 4:
        {
            /* Remove a single address from existing interface */
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
        }

        case 3:
        {
            pszAdapterName = argv[1];
            memset(&Req, '\0', sizeof(Req));
#if 0
//def RT_OS_LINUX
            if (strcmp("speed", argv[2]) == 0)
            {
                /*
                 * This ugly hack is needed for retrieving the link speed on
                 * pre-2.6.33 kernels (see @bugref{6345}).
                 */
                if (strlen(pszAdapterName) >= IFNAMSIZ)
                {
                    showUsage();
                    return -1;
                }
                struct ifreq IfReq;
                struct ethtool_cmd EthToolReq;
                int fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (fd < 0)
                {
                    fprintf(stderr, "VBoxNetAdpCtl: Error while retrieving link "
                            "speed for %s: ", pszAdapterName);
                    perror("VBoxNetAdpCtl: failed to open control socket");
                    return ADPCTLERR_SOCKET_FAILED;
                }
                memset(&IfReq, 0, sizeof(IfReq));
                snprintf(IfReq.ifr_name, sizeof(IfReq.ifr_name), "%s", pszAdapterName);
                EthToolReq.cmd = ETHTOOL_GSET;
                IfReq.ifr_data = (caddr_t)&EthToolReq;
                rc = ioctl(fd, SIOCETHTOOL, &IfReq);
                if (rc == 0)
                {
                    printf("%u", EthToolReq.speed);
                }
                else
                {
                    fprintf(stderr, "VBoxNetAdpCtl: Error while retrieving link "
                            "speed for %s: ", pszAdapterName);
                    perror("VBoxNetAdpCtl: ioctl failed");
                    rc = ADPCTLERR_IOCTL_FAILED;
                }
                close(fd);
                return rc;
            }
#endif
            rc = checkAdapterName(pszAdapterName, szAdapterName);
            if (rc)
                return rc;
            snprintf(Req.szName, sizeof(Req.szName), "%s", szAdapterName);
            pszAddress = argv[2];
            if (strcmp("remove", pszAddress) == 0)
            {
                /* Remove an existing interface */
#ifdef RT_OS_SOLARIS
                return 1;
#else
                return doIOCtl(VBOXNETADP_CTL_REMOVE, &Req);
#endif
            }
            else if (strcmp("add", pszAddress) == 0)
            {
                /* Create an interface with given name */
#ifdef RT_OS_SOLARIS
                return 1;
#else
                rc = doIOCtl(VBOXNETADP_CTL_ADD, &Req);
                if (rc == 0)
                    puts(Req.szName);
#endif
                return rc;
            }
            break;
        }

        case 2:
        {
            /* Create a new interface */
            if (strcmp("add", argv[1]) == 0)
            {
#ifdef RT_OS_SOLARIS
                return 1;
#else
                memset(&Req, '\0', sizeof(Req));
                rc = doIOCtl(VBOXNETADP_CTL_ADD, &Req);
                if (rc == 0)
                    puts(Req.szName);
#endif
                return rc;
            }
            /* Fall through */
        }

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
#if defined(RT_OS_LINUX)
            rc = executeIfconfig(pszAdapterName, "0.0.0.0");
#else
            rc = executeIfconfig(pszAdapterName, VBOXADPCTL_DEL_CMD, pszAddress);
#endif

#ifdef RT_OS_SOLARIS
            /* On Solaris we can unplumb the ipv4 interface */
            executeIfconfig(pszAdapterName, "inet", "unplumb");
#endif
        }
    }
    else
    {
        /* We are setting/replacing address. */
        if (strchr(pszAddress, ':'))
        {
#ifdef RT_OS_SOLARIS
            /* On Solaris we need to plumb the interface first if it's not already plumbed. */
            if (executeIfconfig(pszAdapterName, "inet6") != 0)
                executeIfconfig(pszAdapterName, "inet6", "plumb", "up");
#endif
            /*
             * Before we set IPv6 address we'd like to remove
             * all previously assigned addresses except the
             * self-assigned one.
             */
            if (!removeAddresses(pszAdapterName))
                rc = EXIT_FAILURE;
            else
                rc = executeIfconfig(pszAdapterName, "inet6", VBOXADPCTL_ADD_CMD, pszAddress, pszOption, pszNetworkMask);
        }
        else
        {
#ifdef RT_OS_SOLARIS
            /* On Solaris we need to plumb the interface first if it's not already plumbed. */
            if (executeIfconfig(pszAdapterName, "inet") != 0)
                executeIfconfig(pszAdapterName, "plumb", "up");
#endif
            rc = executeIfconfig(pszAdapterName, pszAddress, pszOption, pszNetworkMask);
        }
    }
    return rc;
}

