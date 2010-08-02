/* $Id$ */
/** @file
 * VBoxService - Auto-mounting for Shared Folders.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

#include <errno.h>
#include <grp.h>
#include <sys/mount.h>
#ifdef RT_OS_SOLARIS
#include <sys/vfs.h>
#endif
#include <unistd.h>

RT_C_DECLS_BEGIN
#include "../../linux/sharedfolders/vbsfmount.h"
RT_C_DECLS_END

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_AutoMountEvent = NIL_RTSEMEVENTMULTI;


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceAutoMountPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceAutoMountOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    NOREF(ppszShort);
    NOREF(argc);
    NOREF(argv);
    NOREF(pi);
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceAutoMountInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceAutoMountInit\n");

    int rc = RTSemEventMultiCreate(&g_AutoMountEvent);
    AssertRCReturn(rc, rc);

    return rc;
}


static int VBoxServiceAutoMountPrepareMountPoint(const char *pszMountPoint, vbsf_mount_opts *pOpts)
{
    AssertPtr(pOpts);

    RTFMODE fMode = 0770; /* Owner (=root) and the group (=vboxsf) have full access. */
    int rc = RTDirCreateFullPath(pszMountPoint, fMode);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathSetOwnerEx(pszMountPoint, -1 /* Owner, unchanged */, pOpts->gid, RTPATH_F_ON_LINK);
        if (RT_SUCCESS(rc))
        {
            rc = RTPathSetMode(pszMountPoint, fMode);
            VBoxServiceVerbose(3, "RTPathSetMode = rc = %Rrc\n");
            if (RT_FAILURE(rc))
                VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not set mode for mount directory \"%s\", rc = %Rrc\n",
                                 pszMountPoint, rc);
        }
        else
            VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not set permissions for mount directory \"%s\", rc = %Rrc\n",
                             pszMountPoint, rc);
    }
    else
        VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not create mount directory \"%s\", rc = %Rrc\n",
                         pszMountPoint, rc);
    return rc;
}


static int VBoxServiceAutoMountSharedFolder(const char *pszShareName, const char *pszMountPoint,
                                            vbsf_mount_opts *pOpts)
{
    AssertPtr(pOpts);

    int rc = VBoxServiceAutoMountPrepareMountPoint(pszMountPoint, pOpts);
    if (RT_SUCCESS(rc))
    {
#ifdef RT_OS_SOLARIS
        int flags = 0; /* No flags used yet. */
        int r = mount(pszShareName,
                      pszMountPoint,
                      flags,
                      "vboxsf",
                      NULL,                     /* char *dataptr */
                      0,                        /* int datalen */
                      NULL,                     /* char *optptr */
                      0);                       /* int optlen */
        if (r == 0)
        {
            VBoxServiceVerbose(0, "VBoxServiceAutoMountWorker: Shared folder \"%s\" was mounted to \"%s\"\n", pszShareName, pszMountPoint);
        }
        else
        {
            if (errno != EBUSY) /* Share is already mounted? Then skip error msg. */
                VBoxServiceError("VBoxServiceAutoMountWorker: Could not mount shared folder \"%s\" to \"%s\", error = %s\n",
                                 pszShareName, pszMountPoint, strerror(errno));
        }
#else /* !RT_OS_SOLARIS */
        unsigned long flags = MS_NODEV;

        const char *szOptions = { "rw" };
        struct vbsf_mount_info_new mntinf;

        mntinf.nullchar     = '\0';
        mntinf.signature[0] = VBSF_MOUNT_SIGNATURE_BYTE_0;
        mntinf.signature[1] = VBSF_MOUNT_SIGNATURE_BYTE_1;
        mntinf.signature[2] = VBSF_MOUNT_SIGNATURE_BYTE_2;
        mntinf.length       = sizeof(mntinf);

        mntinf.uid   = pOpts->uid;
        mntinf.gid   = pOpts->gid;
        mntinf.ttl   = pOpts->ttl;
        mntinf.dmode = pOpts->dmode;
        mntinf.fmode = pOpts->fmode;
        mntinf.dmask = pOpts->dmask;
        mntinf.fmask = pOpts->fmask;

        strcpy(mntinf.name, pszShareName);
        strcpy(mntinf.nls_name, "\0");

        int r = mount(NULL,
                      pszMountPoint,
                      "vboxsf",
                      flags,
                      &mntinf);
        if (r == 0)
        {
            VBoxServiceVerbose(0, "VBoxServiceAutoMountWorker: Shared folder \"%s\" was mounted to \"%s\"\n", pszShareName, pszMountPoint);

            r = vbsfmount_complete(pszShareName, pszMountPoint, flags, pOpts);
            switch (r)
            {
                case 0: /* Success. */
                    errno = 0; /* Clear all errors/warnings. */
                    break;

                case 1:
                    VBoxServiceError("VBoxServiceAutoMountWorker: Could not update mount table (failed to create memstream): %s\n", strerror(errno));
                    break;

                case 2:
                    VBoxServiceError("VBoxServiceAutoMountWorker: Could not open mount table for update: %s\n", strerror(errno));
                    break;

                case 3:
                    VBoxServiceError("VBoxServiceAutoMountWorker: Could not add an entry to the mount table: %s\n", strerror(errno));
                    break;

                default:
                    VBoxServiceError("VBoxServiceAutoMountWorker: Unknown error while completing mount operation: %d\n", r);
                    break;
            }
        }
        else /* r != 0 */
        {
            if (errno == EPROTO)
            {
                /* Sometimes the mount utility messes up the share name.  Try to
                 * un-mangle it again. */
                char szCWD[4096];
                size_t cchCWD;
                if (!getcwd(szCWD, sizeof(szCWD)))
                    VBoxServiceError("VBoxServiceAutoMountWorker: Failed to get the current working directory\n");
                cchCWD = strlen(szCWD);
                if (!strncmp(pszMountPoint, szCWD, cchCWD))
                {
                    while (pszMountPoint[cchCWD] == '/')
                        ++cchCWD;
                    /* We checked before that we have enough space */
                    strcpy(mntinf.name, pszMountPoint + cchCWD);
                }
                r = mount(NULL, pszMountPoint, "vboxsf", flags, &mntinf);
            }
            if (errno == EPROTO)
            {
                /* New mount tool with old vboxsf module? Try again using the old
                 * vbsf_mount_info_old structure. */
                struct vbsf_mount_info_old mntinf_old;
                memcpy(&mntinf_old.name, &mntinf.name, MAX_HOST_NAME);
                memcpy(&mntinf_old.nls_name, mntinf.nls_name, MAX_NLS_NAME);
                mntinf_old.uid = mntinf.uid;
                mntinf_old.gid = mntinf.gid;
                mntinf_old.ttl = mntinf.ttl;
                r = mount(NULL, pszMountPoint, "vboxsf", flags, &mntinf_old);
            }
            if (errno != EBUSY) /* Share is already mounted? Then skip error msg. */
                VBoxServiceError("VBoxServiceAutoMountWorker: Could not mount shared folder \"%s\" to \"%s\", error = %s\n",
                                 pszShareName, pszMountPoint, strerror(errno));
        }
#endif /* !RT_OS_SOLARIS */
    }

    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Mounting returned with rc=%Rrc, errno=%d, error=%s\n",
                       rc, errno, strerror(errno));
    return RTErrConvertFromErrno(errno);
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceAutoMountWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    uint32_t u32ClientId;
    int rc = VbglR3SharedFolderConnect(&u32ClientId);
    if (!RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Failed to connect to the shared folder service, error %Rrc\n", rc);
    else
    {
        uint32_t cMappings;
        VBGLR3SHAREDFOLDERMAPPING *paMappings;

        rc = VbglR3SharedFolderGetMappings(u32ClientId, true /* Only process auto-mounted folders */,
                                           &paMappings, &cMappings);
        if (RT_SUCCESS(rc))
        {
#if 0
            /* Check for a fixed/virtual auto-mount share. */
            if (VbglR3SharedFolderExists(u32ClientId, "vbsfAutoMount"))
            {
                VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Host supports auto-mount root\n");
            }
            else
            {
#endif
                VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Got %u shared folder mappings\n", cMappings);
                for (uint32_t i = 0; i < cMappings && RT_SUCCESS(rc); i++)
                {
                    char *pszShareName = NULL;
                    rc = VbglR3SharedFolderGetName(u32ClientId, paMappings[i].u32Root, &pszShareName);
                    if (   RT_SUCCESS(rc)
                        && *pszShareName)
                    {
                        VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Connecting share %u (%s) ...\n", i+1, pszShareName);

                        char *pszMountPoint = NULL;
#ifdef RT_OS_SOLARIS
                        if (   RTStrAPrintf(&pszMountPoint, "/mnt/sf_%s", pszShareName) > 0
#else
                        if (   RTStrAPrintf(&pszMountPoint, "/media/sf_%s", pszShareName) > 0
#endif
                            && pszMountPoint)
                        {
                            struct group *grp_vboxsf = getgrnam("vboxsf");
                            if (grp_vboxsf)
                            {
                                struct vbsf_mount_opts mount_opts =
                                {
                                    0,                     /* uid */
                                    grp_vboxsf->gr_gid,    /* gid */
                                    0,                     /* ttl */
                                    0770,                  /* dmode, owner and group "vboxsf" have full access */
                                    0770,                  /* fmode, owner and group "vboxsf" have full access */
                                    0,                     /* dmask */
                                    0,                     /* fmask */
                                    0,                     /* ronly */
                                    0,                     /* noexec */
                                    0,                     /* nodev */
                                    0,                     /* nosuid */
                                    0,                     /* remount */
                                    "\0",                  /* nls_name */
                                    NULL,                  /* convertcp */
                                };

                                /* We always use "/media" as our root mounting directory. */
                                /** @todo Detect the correct "media/mnt" directory, based on the current guest (?). */
                                rc = VBoxServiceAutoMountSharedFolder(pszShareName, pszMountPoint, &mount_opts);
                            }
                            else
                                VBoxServiceError("VBoxServiceAutoMountWorker: Group \"vboxsf\" does not exist\n");
                            RTStrFree(pszMountPoint);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                        RTStrFree(pszShareName);
                    }
                    else
                        VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder name for root node = %u, rc = %Rrc\n",
                                         paMappings[i].u32Root, rc);
                }
#if 0
            }
#endif
            RTMemFree(paMappings);
        }
        else
            VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder mappings, rc = %Rrc\n", rc);
        VbglR3SharedFolderDisconnect(u32ClientId);
    }

    RTSemEventMultiDestroy(g_AutoMountEvent);
    g_AutoMountEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Finished\n");
    return 0;
}

/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceAutoMountTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServiceAutoMountTerm\n");
    return;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceAutoMountStop(void)
{
    RTSemEventMultiSignal(g_AutoMountEvent);
}


/**
 * The 'automount' service description.
 */
VBOXSERVICE g_AutoMount =
{
    /* pszName. */
    "automount",
    /* pszDescription. */
    "Auto-mount for Shared Folders",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceAutoMountPreInit,
    VBoxServiceAutoMountOption,
    VBoxServiceAutoMountInit,
    VBoxServiceAutoMountWorker,
    VBoxServiceAutoMountStop,
    VBoxServiceAutoMountTerm
};
