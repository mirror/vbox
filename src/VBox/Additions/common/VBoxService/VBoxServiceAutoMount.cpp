/* $Id$ */
/** @file
 * VBoxService - Auto-mounting for Shared Folders.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
# include <sys/mntent.h>
# include <sys/mnttab.h>
# include <sys/vfs.h>
#else
# include <mntent.h>
# include <paths.h>
#endif
#include <unistd.h>

RT_C_DECLS_BEGIN
#include "../../linux/sharedfolders/vbsfmount.h"
RT_C_DECLS_END

#ifdef RT_OS_SOLARIS
# define VBOXSERVICE_AUTOMOUNT_DEFAULT_DIR       "/mnt"
#else
# define VBOXSERVICE_AUTOMOUNT_DEFAULT_DIR       "/media"
#endif

#ifndef _PATH_MOUNTED
 #ifdef RT_OS_SOLARIS
  #define _PATH_MOUNTED                          "/etc/mnttab"
 #else
  #define _PATH_MOUNTED                          "/etc/mtab"
 #endif
#endif

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_AutoMountEvent = NIL_RTSEMEVENTMULTI;
/** The Shared Folders service client ID. */
static uint32_t         g_SharedFoldersSvcClientID = 0;

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

    return -1;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceAutoMountInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceAutoMountInit\n");

    int rc = RTSemEventMultiCreate(&g_AutoMountEvent);
    AssertRCReturn(rc, rc);

    rc = VbglR3SharedFolderConnect(&g_SharedFoldersSvcClientID);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "VBoxServiceAutoMountInit: Service Client ID: %#x\n", g_SharedFoldersSvcClientID);
    }
    else
    {
        /* If the service was not found, we disable this service without
           causing VBoxService to fail. */
        if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
        {
            VBoxServiceVerbose(0, "VBoxServiceAutoMountInit: Shared Folders service is not available\n");
            rc = VERR_SERVICE_DISABLED;
        }
        else
            VBoxServiceError("Control: Failed to connect to the Shared Folders service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_AutoMountEvent);
        g_AutoMountEvent = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}


/** @todo Integrate into RTFsQueryMountpoint().  */
static bool VBoxServiceAutoMountShareIsMounted(const char *pszShare,
                                               char *pszMountPoint, size_t cbMountPoint)
{
    AssertPtrReturn(pszShare, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszMountPoint, VERR_INVALID_PARAMETER);
    AssertReturn(cbMountPoint, VERR_INVALID_PARAMETER);

    bool fMounted = false;
    /* @todo What to do if we have a relative path in mtab instead
     *       of an absolute one ("temp" vs. "/media/temp")?
     * procfs contains the full path but not the actual share name ...
     * FILE *pFh = setmntent("/proc/mounts", "r+t"); */
#ifdef RT_OS_SOLARIS
    FILE *pFh = fopen(_PATH_MOUNTED, "r");
    if (!pFh)
        VBoxServiceError("VBoxServiceAutoMountShareIsMounted: Could not open mount tab \"%s\"!\n",
                         _PATH_MOUNTED);
    else
    {
        mnttab mntTab;
        while ((getmntent(pFh, &mntTab)))
        {
            if (!RTStrICmp(mntTab.mnt_special, pszShare))
            {
                fMounted = RTStrPrintf(pszMountPoint, cbMountPoint, "%s", mntTab.mnt_mountp)
                         ? true : false;
                break;
            }
        }
        fclose(pFh);
    }
#else
    FILE *pFh = setmntent(_PATH_MOUNTED, "r+t");
    if (pFh == NULL)
        VBoxServiceError("VBoxServiceAutoMountShareIsMounted: Could not open mount tab \"%s\"!\n",
                         _PATH_MOUNTED);
    else
    {
        mntent *pMntEnt;
        while ((pMntEnt = getmntent(pFh)))
        {
            if (!RTStrICmp(pMntEnt->mnt_fsname, pszShare))
            {
                fMounted = RTStrPrintf(pszMountPoint, cbMountPoint, "%s", pMntEnt->mnt_dir)
                         ? true : false;
                break;
            }
        }
        endmntent(pFh);
    }
#endif

    VBoxServiceVerbose(4, "VBoxServiceAutoMountShareIsMounted: Share \"%s\" at mount point \"%s\" = %s\n",
                       pszShare, fMounted ? pszMountPoint : "<None>", fMounted ? "Yes" : "No");
    return fMounted;
}


static int VBoxServiceAutoMountUnmount(const char *pszMountPoint)
{
    AssertPtrReturn(pszMountPoint, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    uint8_t uTries = 0;
    int r;
    while (uTries++ < 3)
    {
        r = umount(pszMountPoint);
        if (r == 0)
            break;
        RTThreadSleep(5000); /* Wait a while ... */
    }
    if (r == -1)
        rc = RTErrConvertFromErrno(errno);
    return rc;
}


static int VBoxServiceAutoMountPrepareMountPoint(const char *pszMountPoint, const char *pszShareName,
                                                 vbsf_mount_opts *pOpts)
{
    AssertPtrReturn(pOpts, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszMountPoint, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszShareName, VERR_INVALID_PARAMETER);

    RTFMODE fMode = RTFS_UNIX_IRWXU | RTFS_UNIX_IRWXG; /* Owner (=root) and the group (=vboxsf) have full access. */
    int rc = RTDirCreateFullPath(pszMountPoint, fMode);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathSetOwnerEx(pszMountPoint, NIL_RTUID /* Owner, unchanged */, pOpts->gid, RTPATH_F_ON_LINK);
        if (RT_SUCCESS(rc))
        {
            rc = RTPathSetMode(pszMountPoint, fMode);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_WRITE_PROTECT)
                {
                    VBoxServiceVerbose(3, "VBoxServiceAutoMountPrepareMountPoint: Mount directory \"%s\" already is used/mounted\n", pszMountPoint);
                    rc = VINF_SUCCESS;
                }
                else
                    VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not set mode %RTfmode for mount directory \"%s\", rc = %Rrc\n",
                                     fMode, pszMountPoint, rc);
            }
        }
        else
            VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not set permissions for mount directory \"%s\", rc = %Rrc\n",
                             pszMountPoint, rc);
    }
    else
        VBoxServiceError("VBoxServiceAutoMountPrepareMountPoint: Could not create mount directory \"%s\" with mode %RTfmode, rc = %Rrc\n",
                         pszMountPoint, fMode, rc);
    return rc;
}


static int VBoxServiceAutoMountSharedFolder(const char *pszShareName, const char *pszMountPoint,
                                            vbsf_mount_opts *pOpts)
{
    AssertPtr(pOpts);

    int rc = VINF_SUCCESS;
    char szAlreadyMountedTo[RTPATH_MAX];
    /* If a Shared Folder already is mounted but not to our desired mount point,
     * do an unmount first! */
    if (   VBoxServiceAutoMountShareIsMounted(pszShareName, szAlreadyMountedTo, sizeof(szAlreadyMountedTo))
        && RTStrICmp(pszMountPoint, szAlreadyMountedTo))
    {
        VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Shared folder \"%s\" already mounted to \"%s\", unmounting ...\n",
                           pszShareName, szAlreadyMountedTo);
        rc = VBoxServiceAutoMountUnmount(szAlreadyMountedTo);
        if (RT_FAILURE(rc))
            VBoxServiceError("VBoxServiceAutoMountWorker: Failed to unmount \"%s\", %s (%d)!\n",
                             szAlreadyMountedTo, strerror(errno), errno);
    }

    if (RT_SUCCESS(rc))
        rc = VBoxServiceAutoMountPrepareMountPoint(pszMountPoint, pszShareName, pOpts);
    if (RT_SUCCESS(rc))
    {
#ifdef RT_OS_SOLARIS
        char achOptBuf[MAX_MNTOPT_STR] = { '\0', };
        int flags = 0;
        if (pOpts->ronly)
            flags |= MS_RDONLY;
        RTStrPrintf(achOptBuf, sizeof(achOptBuf), "uid=%d,gid=%d", pOpts->uid, pOpts->gid);
        int r = mount(pszShareName,
                      pszMountPoint,
                      flags | MS_OPTIONSTR,
                      "vboxfs",
                      NULL,                     /* char *dataptr */
                      0,                        /* int datalen */
                      achOptBuf,
                      sizeof(achOptBuf));
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
                    /* VBoxServiceError("VBoxServiceAutoMountWorker: Could not add an entry to the mount table: %s\n", strerror(errno)); */
                    errno = 0;
                    break;

                default:
                    VBoxServiceError("VBoxServiceAutoMountWorker: Unknown error while completing mount operation: %d\n", r);
                    break;
            }
        }
        else /* r == -1, we got some error in errno.  */
        {
            if (errno == EPROTO)
            {
                VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Messed up share name, re-trying ...\n");

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
                VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Re-trying with old mounting structure ...\n");

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
            if (r == -1) /* Was there some error from one of the tries above? */
            {
                switch (errno)
                {
                    /* If we get EINVAL here, the system already has mounted the Shared Folder to another
                     * mount point. */
                    case EINVAL:
                        VBoxServiceVerbose(0, "VBoxServiceAutoMountWorker: Shared folder \"%s\" already is mounted!\n", pszShareName);
                        /* Ignore this error! */
                        break;
                    case EBUSY:
                        /* Ignore these errors! */
                        break;

                    default:
                        VBoxServiceError("VBoxServiceAutoMountWorker: Could not mount shared folder \"%s\" to \"%s\": %s (%d)\n",
                                         pszShareName, pszMountPoint, strerror(errno), errno);
                        rc = RTErrConvertFromErrno(errno);
                        break;
                }
            }
        }
#endif /* !RT_OS_SOLARIS */
    }
    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Mounting returned with rc=%Rrc\n", rc);
    return rc;
}

static int VBoxServiceAutoMountProcessMappings(PVBGLR3SHAREDFOLDERMAPPING paMappings, uint32_t cMappings,
                                               const char *pszMountDir, const char *pszSharePrefix, uint32_t uClientID)
{
    if (cMappings == 0)
        return VINF_SUCCESS;
    AssertPtrReturn(paMappings, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszMountDir, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszSharePrefix, VERR_INVALID_PARAMETER);
    AssertReturn(uClientID > 0, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < cMappings && RT_SUCCESS(rc); i++)
    {
        char *pszShareName = NULL;
        rc = VbglR3SharedFolderGetName(uClientID, paMappings[i].u32Root, &pszShareName);
        if (   RT_SUCCESS(rc)
            && *pszShareName)
        {
            VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Connecting share %u (%s) ...\n", i+1, pszShareName);

            char *pszShareNameFull = NULL;
            if (RTStrAPrintf(&pszShareNameFull, "%s%s", pszSharePrefix, pszShareName) > 0)
            {
                char szMountPoint[RTPATH_MAX];
                rc = RTPathJoin(szMountPoint, sizeof(szMountPoint), pszMountDir, pszShareNameFull);
                if (RT_SUCCESS(rc))
                {
                    VBoxServiceVerbose(4, "VBoxServiceAutoMountWorker: Processing mount point \"%s\"\n", szMountPoint);

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

                        rc = VBoxServiceAutoMountSharedFolder(pszShareName, szMountPoint, &mount_opts);
                    }
                    else
                        VBoxServiceError("VBoxServiceAutoMountWorker: Group \"vboxsf\" does not exist\n");
                }
                else
                    VBoxServiceError("VBoxServiceAutoMountWorker: Unable to join mount point/prefix/shrae, rc = %Rrc\n", rc);
                RTStrFree(pszShareNameFull);
            }
            else
                VBoxServiceError("VBoxServiceAutoMountWorker: Unable to allocate full share name\n");
            RTStrFree(pszShareName);
        }
        else
            VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder name for root node = %u, rc = %Rrc\n",
                             paMappings[i].u32Root, rc);
    } /* for cMappings. */
    return rc;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceAutoMountWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    uint32_t cMappings;
    PVBGLR3SHAREDFOLDERMAPPING paMappings;
    int rc = VbglR3SharedFolderGetMappings(g_SharedFoldersSvcClientID, true /* Only process auto-mounted folders */,
                                           &paMappings, &cMappings);
    if (   RT_SUCCESS(rc)
        && cMappings)
    {
        char *pszMountDir;
        rc = VbglR3SharedFolderGetMountDir(&pszMountDir);
        if (rc == VERR_NOT_FOUND)
            rc = RTStrDupEx(&pszMountDir, VBOXSERVICE_AUTOMOUNT_DEFAULT_DIR);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Shared folder mount dir set to \"%s\"\n", pszMountDir);

            char *pszSharePrefix;
            rc = VbglR3SharedFolderGetMountPrefix(&pszSharePrefix);
            if (RT_SUCCESS(rc))
            {
                VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Shared folder mount prefix set to \"%s\"\n", pszSharePrefix);
#ifdef USE_VIRTUAL_SHARES
                /* Check for a fixed/virtual auto-mount share. */
                if (VbglR3SharedFolderExists(g_SharedFoldersSvcClientID, "vbsfAutoMount"))
                {
                    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Host supports auto-mount root\n");
                }
                else
                {
#endif
                    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Got %u shared folder mappings\n", cMappings);
                    rc = VBoxServiceAutoMountProcessMappings(paMappings, cMappings, pszMountDir, pszSharePrefix, g_SharedFoldersSvcClientID);
#ifdef USE_VIRTUAL_SHARES
                }
#endif
                RTStrFree(pszSharePrefix);
            } /* Mount share prefix. */
            else
                VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder mount prefix, rc = %Rrc\n", rc);
            RTStrFree(pszMountDir);
        }
        else
            VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder directory, rc = %Rrc\n", rc);
        VbglR3SharedFolderFreeMappings(paMappings);
    }
    else
    {
        if (RT_FAILURE(rc))
            VBoxServiceError("VBoxServiceAutoMountWorker: Error while getting the shared folder mappings, rc = %Rrc\n", rc);
        else if (!cMappings)
            VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: No shared folder mappings fouund\n");
    }

    /*
     * Because this thread is a one-timer at the moment we don't want to break/change
     * the semantics of the main thread's start/stop sub-threads handling.
     *
     * This thread exits so fast while doing its own startup in VBoxServiceStartServices()
     * that this->fShutdown flag is set to true in VBoxServiceThread() before we have the
     * chance to check for a service failure in VBoxServiceStartServices() to indicate
     * a VBoxService startup error.
     *
     * Therefore *no* service threads are allowed to quit themselves and need to wait
     * for the pfShutdown flag to be set by the main thread.
     */
    for (;;)
    {
        /* Do we need to shutdown? */
        if (*pfShutdown)
            break;

        /* Let's sleep for a bit and let others run ... */
        RTThreadSleep(500);
    }

    RTSemEventMultiDestroy(g_AutoMountEvent);
    g_AutoMountEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServiceAutoMountWorker: Finished with rc=%Rrc\n", rc);
    return VINF_SUCCESS;
}

/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceAutoMountTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServiceAutoMountTerm\n");

    VbglR3SharedFolderDisconnect(g_SharedFoldersSvcClientID);
    g_SharedFoldersSvcClientID = 0;

    if (g_AutoMountEvent != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(g_AutoMountEvent);
        g_AutoMountEvent = NIL_RTSEMEVENTMULTI;
    }
    return;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceAutoMountStop(void)
{
    /*
     * We need this check because at the moment our auto-mount
     * thread really is a one-timer which destroys the event itself
     * after running.
     */
    if (g_AutoMountEvent != NIL_RTSEMEVENTMULTI)
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
