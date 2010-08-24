/** $Id$ */
/** @file
 * VBoxUSBHelper, setuid binary wrapper for dynamic driver alias updating.
 */

/*
 * Copyright (C) 2008-2009 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

/*
 * I think it is currently a security risk using VBoxUSBHelper. By that I mean
 * here we have an application that can add/remove driver aliases in a generic
 * fashion without requiring root privileges (well setuid).
 *
 * Later maybe we can change this to hardcode aliases to prefix "usb" and then
 * hardcode the driver as "ugen".
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libdevice.h>
#include <errno.h>
#include <syslog.h>

typedef enum AliasOp
{
    ADD_ALIAS = 12,
    DEL_ALIAS
} AliasOp;

/**
 * Print usage to stdout.
 */
static void Usage(char *pszName)
{
    /* @todo Eventually remove the usage syntax display & only display the copyright and warning. */
    fprintf(stderr, "Oracle VM VirtualBox USB Helper\nCopyright (C) Oracle Corporation\n\n");
#if 0
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "Add:    %s add <alias> <drivername>\n", pszName);
    fprintf(stderr, "Delete: %s del <alias> <drivername>\n", pszName);
    fprintf(stderr, "Config: %s cfg <device>\n\n", pszName);
    fprintf(stderr, "Warning!! This program is internally used by VirtualBox and not meant to be used directly.\n");
#endif
    exit(-1);
}

/**
 * Return code legend:
 *      0 - success.
 *     -1 - no root permission
 *     -2 - insufficient arguments
 *     -3 - abormal termination of udpate_drv (not likely)
 *     -4 - update_drv failed (mostly trying to add already existing alias)
 */
static int UpdateDrv(AliasOp Op, char *pszAlias, char *pszDrv)
{
    char szAlias[256];
    sprintf(szAlias, "\"%s\"", pszAlias);

    /* Store current user ID and raise privilege. */
    uid_t userID = getuid();
    setuid(0);

    /* For adding alias add the driver once to make sure we can update alias, no error checking needed. */
    if (Op == ADD_ALIAS)
    {
        char szAddDrvCmd[512];
        sprintf(szAddDrvCmd, "/usr/sbin/add_drv %s\n", pszDrv);
        system(szAddDrvCmd);
    }

    /* Update the alias. */
    char *pszArgs[6];
    int rc = 0;
    pszArgs[0] = "/usr/sbin/update_drv";
    pszArgs[1] = (char *)(Op == ADD_ALIAS ? "-a" : "-d");
    pszArgs[2] = "-i";
    pszArgs[3] = pszAlias;
    pszArgs[4] = pszDrv;
    pszArgs[5] = NULL;
    pid_t processID = fork();
    if (processID < 0)
    {
        /* Bad. fork() failed! */
        rc = -2;
    }
    if (processID == 0)
    {
        /* Child process. */
        syslog(LOG_ERR, "VBoxUSBHelper: %s %s %s %s %s\n", pszArgs[0], pszArgs[1], pszArgs[2], pszArgs[3], pszArgs[4]);
        execv(pszArgs[0], pszArgs);
        _exit(1);
    }

    /* Parent process. */
    int result;
    while (waitpid(processID, &result, 0) < 0)
        ;
    if (!WIFEXITED(result))         /* abnormally terminated. */
        rc = -3;
    if (WEXITSTATUS(result) != 0)   /* non-zero exit status. */
        rc = -4;

    /* Restore user ID. */
    setuid(userID);

    fprintf(stdout, "rc=%d result=%d\n", rc, result);
    return rc;
}

/**
 * Return code legend:
 *      0 - success.
 *     -1 - no root permission
 *     -2 - invalid USB device path
 *     -3 - failed to acquire devctl_handle
 *     -4 - failed to reset
 */
static int ResetDev(char *pszDevicePath, bool fHardReset)
{
    /* Convert the di_node device path into a devctl compatible Ap-Id. */
    char szApId[PATH_MAX + 1];
    char *pszPort = NULL;
    char *pszDir = NULL;
    int rc = 0;

    syslog(LOG_ERR, "VBosUSBHelper: given path=%s\n", pszDevicePath);

    strcpy(szApId, "/devices");
    strcat(szApId, pszDevicePath);
    pszPort = strrchr(pszDevicePath, '@');
    if (pszPort)
    {
        pszPort++;
        pszDir = strrchr(szApId, '/');
        if (pszDir)
        {
            pszDir[0] = '\0';
            strcat(szApId, ":");
            strcat(szApId, pszPort);

            /* Acquire devctl handle. */
            devctl_hdl_t pDevHandle;
            pDevHandle = devctl_ap_acquire(szApId, 0);
            if (pDevHandle)
            {
                /* Prepare to configure. */
                nvlist_t *pValueList = NULL;
                nvlist_alloc(&pValueList, NV_UNIQUE_NAME_TYPE, NULL);
                nvlist_add_int32(pValueList, "port", atoi(pszPort));

                /* Reset the device */
                rc = devctl_ap_unconfigure(pDevHandle, pValueList);
                if (!rc)
                {
                    if (fHardReset)
                        rc = devctl_ap_disconnect(pDevHandle, pValueList);

                    if (!rc)
                    {
                        if (fHardReset)
                            rc = devctl_ap_connect(pDevHandle, pValueList);

                        if (!rc)
                        {
                            rc = devctl_ap_configure(pDevHandle, pValueList);
                            if (rc)
                                syslog(LOG_ERR, "VBoxUSBHelper: Configuring %s failed. rc=%d errno=%d\n", szApId, rc, errno);
                        }
                        else
                            syslog(LOG_ERR, "VBoxUSBHelper: Connecting %s failed. rc=%d errno=%d\n", szApId, rc, errno);
                    }
                    else
                        syslog(LOG_ERR, "VBoxUSBHelper: Disconnecting for %s failed. rc=%d errno=%d\n", szApId, rc, errno);
                }
                else
                    syslog(LOG_ERR, "VBoxUSBHelper: Unconfigure for %s failed. rc=%d errno=%d\n", szApId, rc, errno);

                if (rc)
                    rc = -4;

                /* Free-up the name-value list and the obtained handle. */
                nvlist_free(pValueList);
                devctl_release(pDevHandle);
            }
            else
            {
                syslog(LOG_ERR, "VBoxUSBHelper: devctl_ap_acquire for %s failed. errno=%d\n", szApId, errno);
                rc = -3;
            }
        }
        else
        {
            syslog(LOG_ERR, "VBoxUSBHelper: invalid device %s\n", pszDevicePath);
            rc = -1;
        }
    }
    else
    {
        syslog(LOG_ERR, "VBoxUSBHelper: invalid device %s\n", pszDevicePath);
        rc = -1;
    }
    fprintf(stdout, "rc=%d\n", rc);
    return rc;
}

/**
 * returns error code from UpdateDrv. See UpdateDrv error codes.
 */
int main(int argc, char *argv[])
{
    /* Check root permissions. */
    if (geteuid() != 0)
    {
        fprintf(stderr, "This program needs administrator privileges.\n");
        return -1;
    }

    if (argc < 3)
    {
        Usage(argv[0]);
        return -2;
    }

    AliasOp Operation = ADD_ALIAS;
    char *pszOp = argv[1];
    if (!strcasecmp(pszOp, "hardreset"))
        return ResetDev(argv[2], true);
    else if (!strcasecmp(pszOp, "softreset"))
        return ResetDev(argv[2], false);
    else if (!strcasecmp(pszOp, "del"))
        Operation = DEL_ALIAS;
    else if (!strcasecmp(pszOp, "add"))
        Operation = ADD_ALIAS;
    else
    {
        Usage(argv[0]);
        return -2;
    }

    char *pszAlias = argv[2];
    char *pszDrv = argv[3];
    /* Return status from update_drv */
    return UpdateDrv(Operation, pszAlias, pszDrv);
}

