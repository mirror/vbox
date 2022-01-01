/* $Id$ */
/** @file
 * vbsfmount - Commonly used code to mount shared folders on Linux-based
 *             systems.  Currently used by mount.vboxsf and VBoxService.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <assert.h>
#include <ctype.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mount.h>

#include "vbsfmount.h"


/** @todo Use defines for return values! */
int vbsfmount_complete(const char *pszSharedFolder, const char *pszMountPoint,
                       unsigned long fFlags, const char *pszOpts)
{
    /*
     * Combine pszOpts and fFlags.
     */
    int          rc;
    size_t const cchFlags = (fFlags & MS_NOSUID ? strlen(MNTOPT_NOSUID) + 1 : 0)
                          + (fFlags & MS_RDONLY ? strlen(MNTOPT_RO) : strlen(MNTOPT_RW));
    size_t const cchOpts  = pszOpts ? 1 + strlen(pszOpts) : 0;
    char        *pszBuf   = (char *)malloc(cchFlags + cchOpts + 8);
    if (pszBuf)
    {
        char         *psz = pszBuf;
        FILE         *pMTab;

        strcpy(psz, fFlags & MS_RDONLY ? MNTOPT_RO : MNTOPT_RW);
        psz += strlen(psz);

        if (fFlags & MS_NOSUID)
        {
            *psz++ = ',';
            strcpy(psz, MNTOPT_NOSUID);
            psz += strlen(psz);
        }

        if (cchOpts)
        {
            *psz++ = ',';
            strcpy(psz, pszOpts);
        }

        assert(strlen(pszBuf) <= cchFlags + cchOpts);

        /*
         * Open the mtab and update it:
         */
        pMTab = setmntent(MOUNTED, "a+");
        if (pMTab)
        {
            struct mntent Entry;
            Entry.mnt_fsname = (char*)pszSharedFolder;
            Entry.mnt_dir = (char *)pszMountPoint;
            Entry.mnt_type = "vboxsf";
            Entry.mnt_opts = pszBuf;
            Entry.mnt_freq = 0;
            Entry.mnt_passno = 0;

            if (!addmntent(pMTab, &Entry))
                rc = 0; /* success. */
            else
                rc = 3;  /* Could not add an entry to the mount table. */

            endmntent(pMTab);
        }
        else
            rc = 2; /* Could not open mount table for update. */

        free(pszBuf);
    }
    else
        rc = 1; /* allocation error */
    return rc;
}

