/* $Id$ */
/** @file
 * VirtualBox Guest Additions Mouse Driver for Solaris: user space loader tool.
 */

/*
 * Copyright (C) 2012-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#include <VBox/version.h>
#include <iprt/buildconfig.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>

static void handleArgs(int argc, char *argv[], int *pfNoLogo);

int main(int argc, char *argv[])
{
    int fNoLogo, hVBoxMS, hConsMS, idConnection;

    handleArgs(argc, argv, &fNoLogo);
    /* Open our pointer integration driver (vboxms). */
    hVBoxMS = open("/dev/vboxms", O_RDWR);
    if (hVBoxMS < 0)
    {
        printf("Failed to open /dev/vboxms - please make sure that the node exists and that\n"
               "you have permission to open it.  The error reported was:\n"
               "%s\n", strerror(errno));
        exit(1);
    }
    /* Open the Solaris virtual mouse driver (consms). */
    hConsMS = open("/dev/mouse", O_RDWR);
    if (hConsMS < 0)
    {
        printf("Failed to open /dev/mouse - please make sure that the node exists and that\n"
               "you have permission to open it.  The error reported was:\n"
               "%s\n", strerror(errno));
        exit(1);
    }
    /* Link vboxms to consms from below.  What this means is that vboxms is
     * added to the list of input sources multiplexed by consms, and vboxms
     * will receive any control messages (such as information about guest
     * resolution changes) sent to consms.  The link can only be broken
     * explicitly using the connection ID returned from the IOCtl. */
    idConnection = ioctl(hConsMS, I_PLINK, hVBoxMS);
    if (idConnection < 0)
    {
        printf("Failed to add /dev/vboxms (the pointer integration driver) to /dev/mouse\n"
               "(the Solaris virtual master mouse).  The error reported was:\n"
               "%s\n", strerror(errno));
        exit(1);
    }
    close(hVBoxMS);
    if (!fNoLogo)
        printf("Successfully enabled pointer integration.  Connection ID number to the\n"
               "Solaris virtual master mouse:\n");
    printf("%d\n", idConnection);
    exit(0);
}

void handleArgs(int argc, char *argv[], int *pfNoLogo)
{
    int fNoLogo = 0, fShowUsage = 0, fShowVersion = 0;

    if (argc != 1 && argc != 2)
        fShowUsage = 1;
    if (argc == 2)
    {
        if (!strcmp(argv[1], "--nologo"))
            fNoLogo = 1;
        else if (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))
            fShowVersion = 1;
        else
            fShowUsage = 1;
    }
    if (fShowVersion)
    {
        printf("%sr%u\n", VBOX_VERSION_STRING, RTBldCfgRevision());
        exit(0);
    }
    if (!fNoLogo)
        printf(VBOX_PRODUCT
               " Guest Additions enabling utility for Solaris pointer\nintegration Version "
               VBOX_VERSION_STRING "\n"
               "Copyright (C) " VBOX_C_YEAR " " VBOX_VENDOR "\n\n");
    if (fShowUsage)
    {
        printf("Usage:\n  -V|--version  print the tool version.\n"
               "  --nologo      do not display the logo text and only output the connection\n"
               "                ID number needed to disable pointer integration\n"
               "                again.\n"
               "  -h|--help     display this help text.\n");
        exit(0);
    }
    *pfNoLogo = fNoLogo;
}

