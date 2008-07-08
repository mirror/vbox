/* $Id$ */
/** @file
 * VirtualBox File System Driver for Solaris Guests. VFS operations.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/param.h>
#include <sys/modctl.h>
#include <sys/mount.h>
#include <sys/policy.h>
#include <sys/atomic.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "vboxvfs.h"

#if defined(DEBUG_ramshankar) && !defined(LOG_ENABLED)
# define LOG_ENABLED
# define LOG_TO_BACKDOOR
#endif
#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#if defined(DEBUG_ramshankar)
# undef LogFlow
# define LogFlow        LogRel
# undef Log
# define Log            LogRel
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Mount Options */
#define MNTOPT_VBOXVFS_UID       "uid"
#define MNTOPT_VBOXVFS_GID       "gid"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxVFS_Init(int fType, char *pszName);
static int VBoxVFS_Mount(vfs_t *pVFS, vnode_t *pVNode, struct mounta *pMount, cred_t *pCred);
static int VBoxVFS_Unmount(vfs_t *pVFS, int fFlags, cred_t *pCred);
static int VBoxVFS_Root(vfs_t *pVFS, vnode_t **ppVNode);
static int VBoxVFS_Statfs(register vfs_t *pVFS, struct statvfs64 *pStat);
static int VBoxVFS_VGet(vfs_t *pVFS, vnode_t **ppVNode, struct fid *pFid);
static void VBoxVFS_FreeVFS(vfs_t *pVFS);

static int vboxvfs_CheckMountPerm(vfs_t *pVFS, struct mounta *pMount, vnode_t *pVNodeSpec, cred_t *pCred);
static int vboxvfs_GetIntOpt(vfs_t *pVFS, char *pszOpt, int *pValue);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * mntopts_t: mount options table array
 */
static mntopt_t g_VBoxVFSMountOptions[] =
{
    /* Option Name           Cancel Opt.     Default Arg       Flags           Data */
    { MNTOPT_VBOXVFS_UID,    NULL,           NULL,             MO_HASVALUE,    NULL },
    { MNTOPT_VBOXVFS_GID,    NULL,           NULL,             MO_HASVALUE,    NULL }
};

/**
 * mntopts_t: mount options table prototype
 */
static mntopts_t g_VBoxVFSMountTableProt =
{
    sizeof(g_VBoxVFSMountOptions) / sizeof(mntopt_t),
    g_VBoxVFSMountOptions
};

/**
 * vfsdef_t: driver specific mount options
 */
static vfsdef_t g_VBoxVFSDef =
{
    VFSDEF_VERSION,
    DEVICE_NAME,
    VBoxVFS_Init,
    VSW_HASPROTO,
    &g_VBoxVFSMountTableProt
};

/**
 * modlfs: loadable file system
 */
static struct modlfs g_VBoxVFSLoadMod =
{
    &mod_fsops,     /* extern from kernel */
    DEVICE_DESC,
    &g_VBoxVFSDef
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxVFSModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_VBoxVFSLoadMod,
    NULL                    /* terminate array of linkage structures */
};


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xdecea5ed;
/** Global connection to the client. */
VBSFCLIENT g_VBoxVFSClient;
/** Global VFS Operations pointer. */
vfsops_t *g_pVBoxVFS_vfsops;
/** The file system type identifier. */
static int g_VBoxVFSType;
/** Major number of this module */
static major_t g_VBoxVFSMajor;
/** Minor instance number */
static minor_t g_VBoxVFSMinor;
/** Minor lock mutex protection */
static kmutex_t g_VBoxVFSMinorMtx;


/**
 * Kernel entry points
 */
int _init(void)
{
    LogFlow((DEVICE_NAME ":_init\n"));

    return mod_install(&g_VBoxVFSModLinkage);;
}


int _fini(void)
{
    LogFlow((DEVICE_NAME ":_fini\n"));

    int rc = mod_remove(&g_VBoxVFSModLinkage);
    if (rc)
        return rc;

    /* Blow away the operation vectors*/
    vfs_freevfsops_by_type(g_VBoxVFSType);
    vn_freevnodeops(g_pVBoxVFS_vnodeops);
}


int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));

    return mod_info(&g_VBoxVFSModLinkage, pModInfo);
}


static int VBoxVFS_Init(int fType, char *pszName)
{
    int rc;

    LogFlow((DEVICE_NAME ":VBoxVFS_Init\n"));

    g_VBoxVFSType = fType;

    /* Initialize the R0 guest library. */
    rc = vboxInit();
    if (VBOX_SUCCESS(rc))
    {
        /* Connect to the host service. */
        rc = vboxConnect(&g_VBoxVFSClient);
        if (VBOX_SUCCESS(rc))
        {
            /* Use UTF-8 encoding. */
            rc = vboxCallSetUtf8 (&g_VBoxVFSClient);
            if (VBOX_SUCCESS(rc))
            {
                /* Fill up VFS user entry points. */
                static const fs_operation_def_t s_VBoxVFS_vfsops_template[] =
                {
                    VFSNAME_MOUNT,    { .vfs_mount =     VBoxVFS_Mount   },
                    VFSNAME_UNMOUNT,  { .vfs_unmount =   VBoxVFS_Unmount },
                    VFSNAME_ROOT,     { .vfs_root =      VBoxVFS_Root    },
                    VFSNAME_STATVFS,  { .vfs_statvfs =   VBoxVFS_Statfs  },
                    VFSNAME_VGET,     { .vfs_vget =      VBoxVFS_VGet    },
                    VFSNAME_FREEVFS,  { .vfs_freevfs =   VBoxVFS_FreeVFS },
                    NULL,             NULL
                };

                rc = vfs_setfsops(fType, s_VBoxVFS_vfsops_template, &g_pVBoxVFS_vfsops);
                if (!rc)
                {
                    /* Set VNode operations. */
                    rc = vn_make_ops(pszName, g_VBoxVFS_vnodeops_template, &g_pVBoxVFS_vnodeops);
                    if (!rc)
                    {
                        /* Get a free major number. */
                        g_VBoxVFSMajor = getudev();
                        if (g_VBoxVFSMajor != (major_t)-1)
                        {
                            /* Initialize minor mutex here. */
                            mutex_init(&g_VBoxVFSMinorMtx, "VBoxVFSMinorMtx", MUTEX_DEFAULT, NULL);
                            LogFlow((DEVICE_NAME ":Successfully loaded vboxvfs.\n"));
                            return 0;
                        }
                        else
                        {
                            LogRel((DEVICE_NAME ":getudev failed.\n"));
                            rc = EMFILE;
                        }
                    }
                    else
                        LogRel((DEVICE_NAME ":vn_make_ops failed. rc=%d\n", rc));
                }
                else
                    LogRel((DEVICE_NAME ":vfs_setfsops failed. rc=%d\n", rc));
            }
            else
            {
                LogRel((DEVICE_NAME ":vboxCallSetUtf8 failed. rc=%d\n", rc));
                rc = EPROTO;
            }
            vboxDisconnect(&g_VBoxVFSClient);
        }
        else
        {
            LogRel((DEVICE_NAME ":Failed to connect to host! rc=%d\n", rc));
            rc = ENXIO;
        }
        vboxUninit();
    }
    else
    {
        LogRel((DEVICE_NAME ":Failed to initialize R0 lib. rc=%d\n", rc));
        rc = ENXIO;
    }
    return rc;
}

static int VBoxVFS_Mount(vfs_t *pVFS, vnode_t *pVNode, struct mounta *pMount, cred_t *pCred)
{
    int rc = 0;
    int Uid = 0;
    int Gid = 0;
    char *pszShare = NULL;
    size_t cbShare = NULL;
    pathname_t PathName;
    vboxvfs_vnode_t *pVNodeRoot = NULL;
    vnode_t *pVNodeSpec = NULL;
    vnode_t *pVNodeDev = NULL;
    dev_t Dev = 0;
    SHFLSTRING *pShflShareName = NULL;
    RTFSOBJINFO FSInfo;
    size_t cbShflShareName = 0;
    vboxvfs_globinfo_t *pVBoxVFSGlobalInfo = NULL;
    int AddrSpace = (pMount->flags & MS_SYSSPACE) ? UIO_SYSSPACE : UIO_USERSPACE;

    LogFlow((DEVICE_NAME ":VBoxVFS_Mount\n"));

    /* Check user credentials for mounting in the specified target. */
    rc = secpolicy_fs_mount(pCred, pVNode, pVFS);
    if (rc)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: secpolicy_fs_mount failed! invalid credentials.rc=%d\n", rc));
        return EPERM;
    }

    /* We can mount to only directories. */
    if (pVNode->v_type != VDIR)
        return ENOTDIR;

    /* We don't support remounting. */
    if (pMount->flags & MS_REMOUNT)
        return ENOTSUP;

    mutex_enter(&pVNode->v_lock);
    if (   !(pMount->flags & MS_OVERLAY)
        &&  (pVNode->v_count > 1 || (pVNode->v_flag & VROOT)))
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: device busy.\n"));
        mutex_exit(&pVNode->v_lock);
        return EBUSY;
    }
    mutex_exit(&pVNode->v_lock);

    /* From what I understood the options are already parsed at a higher level */
    if (   (pMount->flags & MS_DATA)
        && pMount->datalen > 0)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: unparsed options not supported.\n"));
        return EINVAL;
    }

    /* Get UID argument (optional). */
    rc = vboxvfs_GetIntOpt(pVFS, MNTOPT_VBOXVFS_UID, &Uid);
    if (rc < 0)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: invalid uid value.\n"));
        return EINVAL;
    }

    /* Get GID argument (optional). */
    rc = vboxvfs_GetIntOpt(pVFS, MNTOPT_VBOXVFS_GID, &Gid);
    if (rc < 0)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: invalid gid value.\n"));
        return EINVAL;
    }

    /* Get special (sharename). */
    rc = pn_get(pMount->spec, AddrSpace, &PathName);
    if (!rc)
    {
        /* Get an available minor instance number for this mount. */
        mutex_enter(&g_VBoxVFSMinorMtx);
        do
        {
            atomic_add_32_nv(&g_VBoxVFSMinor, 1) & L_MAXMIN32;
            Dev = makedevice(g_VBoxVFSMajor, g_VBoxVFSMinor);
        } while (vfs_devismounted(Dev));
        mutex_exit(&g_VBoxVFSMinorMtx);

        cbShare = strlen(PathName.pn_path);
        pszShare = RTMemAlloc(cbShare);
        if (pszShare)
            memcpy(pszShare, PathName.pn_path, cbShare);
        else
        {
            LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to alloc %d bytes for sharename.\n", cbShare));
            rc = ENOMEM;
        }
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to get sharename.rc=%d\n", rc));
        rc = EINVAL;
    }
    pn_free(&PathName);

    if (rc)
    {
        if (pszShare)
            RTMemFree(pszShare);
        return rc;
    }

    /* Allocate the global info. structure. */
    pVBoxVFSGlobalInfo = RTMemAlloc(sizeof(*pVBoxVFSGlobalInfo));
    if (pVBoxVFSGlobalInfo)
    {
        cbShflShareName = offsetof(SHFLSTRING, String.utf8) + cbShare + 1;
        pShflShareName  = RTMemAllocZ(cbShflShareName);
        if (pShflShareName)
        {
            pShflShareName->u16Length = cbShflShareName;
            pShflShareName->u16Size   = cbShflShareName + 1;
            memcpy (pShflShareName->String.utf8, pszShare, cbShare + 1);

            rc = vboxCallMapFolder(&g_VBoxVFSClient, pShflShareName, &pVBoxVFSGlobalInfo->Map);
            RTMemFree(pShflShareName);
            if (VBOX_SUCCESS(rc))
                rc = 0;
            else
            {
                LogRel((DEVICE_NAME ":VBoxVFS_Mount: vboxCallMapFolder failed rc=%d\n", rc));
                rc = EPROTO;
            }
        }
        else
        {
            LogRel((DEVICE_NAME ":VBoxVFS_Mount: RTMemAllocZ failed to alloc %d bytes for ShFlShareName.\n", cbShflShareName));
            rc = ENOMEM;
        }
        RTMemFree(pVBoxVFSGlobalInfo);
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: RTMemAlloc failed to alloc %d bytes for global struct.\n", sizeof(*pVBoxVFSGlobalInfo)));
        rc = ENOMEM;
    }

    /* Undo work on failure. */
    if (rc)
        goto mntError1;

    /* Initialize the per-filesystem mutex */
    mutex_init(&pVBoxVFSGlobalInfo->MtxFS, "VBoxVFS_FSMtx", MUTEX_DEFAULT, NULL);

    pVBoxVFSGlobalInfo->Uid = Uid;
    pVBoxVFSGlobalInfo->Gid = Gid;
    pVBoxVFSGlobalInfo->pVFS = pVFS;
    pVFS->vfs_data = pVBoxVFSGlobalInfo;
    pVFS->vfs_fstype = g_VBoxVFSType;
    pVFS->vfs_dev = Dev;
    vfs_make_fsid(&pVFS->vfs_fsid, Dev, g_VBoxVFSType);

    /* Allocate root vboxvfs_vnode_t object */
    pVNodeRoot = RTMemAlloc(sizeof(*pVNodeRoot));
    if (pVNodeRoot)
    {
        /* Initialize vnode protection mutex */
        mutex_init(&pVNodeRoot->MtxContents, "VBoxVFS_VNodeMtx", MUTEX_DEFAULT, NULL);

        pVBoxVFSGlobalInfo->pVNodeRoot = pVNodeRoot;

        /* Allocate root path */
        pVNodeRoot->pPath = RTMemAllocZ(sizeof(SHFLSTRING) + 1);
        if (pVNodeRoot->pPath)
        {
            /* Initialize root path */
            pVNodeRoot->pPath->u16Length = 1;
            pVNodeRoot->pPath->u16Size = 2;
            pVNodeRoot->pPath->String.utf8[0] = '/';
            pVNodeRoot->pPath->String.utf8[1] = '\0';

            /* Stat root node info from host */
            rc = vboxvfs_Stat(__func__, pVBoxVFSGlobalInfo, pVNodeRoot->pPath, &FSInfo, B_FALSE);
            if (!rc)
            {
                /* Initialize the root vboxvfs_node_t object */
                vboxvfs_InitVNode(pVBoxVFSGlobalInfo, pVNodeRoot, &FSInfo);

                /* Success! */
                return 0;
            }
            else
                LogRel((DEVICE_NAME ":VBoxVFS_Mount: vboxvfs_Stat failed rc(errno)=%d\n", rc));
            RTMemFree(pVNodeRoot->pPath);
        }
        else
        {
            LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to alloc memory for root node path.\n"));
            rc = ENOMEM;
        }
        RTMemFree(pVNodeRoot);
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to alloc memory for root node.\n"));
        rc = ENOMEM;
    }

    /* Undo work in reverse. */
mntError1:
    RTMemFree(pszShare);
    return rc;
}

static int VBoxVFS_Unmount(vfs_t *pVFS, int fUnmount, cred_t *pCred)
{
    int rc;
    vboxvfs_globinfo_t *pVBoxVFSGlobalInfo;

    LogFlow((DEVICE_NAME ":VBoxVFS_Unmount.\n"));

    /* Check if user can unmount. */
    rc = secpolicy_fs_unmount(pCred, pVFS);
    if (rc)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Unmount: insufficient privileges to unmount.rc=%d\n", rc));
        return EPERM;
    }

    /* @todo -XXX - Not sure of supporting force unmounts. What this means is that a failed force mount could bring down
     * the entire system as hanging about vnode releases would no longer be valid after unloading ourselves...
     */
    if (fUnmount & MS_FORCE)
        pVFS->vfs_flag |= VFS_UNMOUNTED;

    /* @todo implement ref-counting of active vnodes & check for busy state here. */
    /* @todo mutex protection needed here */
    pVBoxVFSGlobalInfo = VFS_TO_VBOXVFS(pVFS);

    rc = vboxCallUnmapFolder(&g_VBoxVFSClient, &pVBoxVFSGlobalInfo->Map);
    if (VBOX_FAILURE(rc))
        LogRel((DEVICE_NAME ":VBoxVFS_Unmount: failed to unmap shared folder. rc=%d\n", rc));

    VN_RELE(VBOXVN_TO_VN(pVBoxVFSGlobalInfo->pVNodeRoot));

    RTMemFree(pVBoxVFSGlobalInfo->pVNodeRoot->pPath);
    RTMemFree(pVBoxVFSGlobalInfo->pVNodeRoot);
    RTMemFree(pVBoxVFSGlobalInfo);
    pVBoxVFSGlobalInfo = NULL;
    pVFS->vfs_data = NULL;

    return 0;
}

static int VBoxVFS_Root(vfs_t *pVFS, vnode_t **ppVNode)
{
    vboxvfs_globinfo_t *pVBoxVFSGlobalInfo = VFS_TO_VBOXVFS(pVFS);
    *ppVNode = VBOXVN_TO_VN(pVBoxVFSGlobalInfo->pVNodeRoot);
    VN_HOLD(*ppVNode);

    return 0;
}

static int VBoxVFS_Statfs(register vfs_t *pVFS, struct statvfs64 *pStat)
{
    SHFLVOLINFO         VolumeInfo;
    uint32_t            cbBuffer;
    vboxvfs_globinfo_t *pVBoxVFSGlobalInfo;
    dev32_t             Dev32;
    int                 rc;

    pVBoxVFSGlobalInfo = VFS_TO_VBOXVFS(pVFS);
    cbBuffer = sizeof(VolumeInfo);
    rc = vboxCallFSInfo(&g_VBoxVFSClient, &pVBoxVFSGlobalInfo->Map, 0, SHFL_INFO_GET | SHFL_INFO_VOLUME, &cbBuffer,
                    (PSHFLDIRINFO)&VolumeInfo);
    if (VBOX_FAILURE(rc))
        return RTErrConvertToErrno(rc);

    bzero(pStat, sizeof(*pStat));
    cmpldev(&Dev32, pVFS->vfs_dev);
    pStat->f_fsid        = Dev32;
    pStat->f_flag        = vf_to_stf(pVFS->vfs_flag);
    pStat->f_bsize       = VolumeInfo.ulBytesPerAllocationUnit;
    pStat->f_frsize      = VolumeInfo.ulBytesPerAllocationUnit;
    pStat->f_bfree       = VolumeInfo.ullAvailableAllocationBytes / VolumeInfo.ulBytesPerAllocationUnit;
    pStat->f_bavail      = VolumeInfo.ullAvailableAllocationBytes / VolumeInfo.ulBytesPerAllocationUnit;
    pStat->f_blocks      = VolumeInfo.ullTotalAllocationBytes / VolumeInfo.ulBytesPerAllocationUnit;
    pStat->f_files       = 1000;
    pStat->f_ffree       = 1000; /* don't return 0 here since the guest may think that it is not possible to create any more files */
    pStat->f_namemax     = 255;  /* @todo is this correct?? */

    strlcpy(pStat->f_basetype, vfssw[pVFS->vfs_fstype].vsw_name, sizeof(pStat->f_basetype));
    strlcpy(pStat->f_fstr, DEVICE_NAME, sizeof(pStat->f_fstr));

    return 0;
}

static int VBoxVFS_VGet(vfs_t *pVFS, vnode_t **ppVNode, struct fid *pFid)
{
    /* -- TODO -- */
    return 0;
}

static void VBoxVFS_FreeVFS(vfs_t *pVFS)
{
    vboxDisconnect(&g_VBoxVFSClient);
    vboxUninit();
}

static int vboxvfs_CheckMountPerm(vfs_t *pVFS, struct mounta *pMount, vnode_t *pVNodeSpec, cred_t *pCred)
{
    /* Check if user has the rights to mount the special file. */
    int fOpen = FREAD | FWRITE;
    int fAccess = VREAD | VWRITE;
    int rc;

    if (pVNodeSpec->v_type != VBLK)
        return ENOTBLK;

    if (   (pVFS->vfs_flag & VFS_RDONLY)
        || (pMount->flags  & MS_RDONLY))
    {
        fOpen = FREAD;
        fAccess = VREAD;
    }

    rc = VOP_ACCESS(pVNodeSpec, fAccess, 0, pCred, NULL /* caller_context */);
    if (!rc)
        rc = secpolicy_spec_open(pCred, pVNodeSpec, fOpen);

    return rc;
}

static int vboxvfs_GetIntOpt(vfs_t *pVFS, char *pszOpt, int *pValue)
{
    int rc;
    long Val;
    char *pchOpt = NULL;
    char *pchEnd = NULL;

    rc = vfs_optionisset(pVFS, pszOpt, &pchOpt);
    if (rc)
    {
        rc = ddi_strtol(pchOpt, &pchEnd, 10 /* base */, &Val);
        if (   !rc
            && Val > INT_MIN
            && Val < INT_MAX
            && pchEnd == pchOpt + strlen(pchOpt))
        {
            *pValue = (int)Val;
            return 0;
        }
        return -1;
    }
    return 1;
}

