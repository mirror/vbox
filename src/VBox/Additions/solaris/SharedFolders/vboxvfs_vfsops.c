/* $Id$ */
/** @file
 * VirtualBox File System Driver for Solaris Guests.
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
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "vboxvfs.h"

#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/mem.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME              "vboxvfs"
/** The module description as seen in 'modinfo'. */
#define DEVICE_DESC              "VirtualBox Shared Filesystem"

/** Mount Options */
#define MNTOPT_VBOXVFS_UID       "uid"
#define MNTOPT_VBOXVFS_GID       "gid"

/** Helpers */
#define VFS2VBOXVFS(vfs)         ((vboxvfs_globinfo_t *)((vfs)->vfs_data))
#define VBOXVFS2VFS(vboxvfs)     ((vboxvfs)->pVFS)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxVFS_Init(int fType, char *pszName);
static int VBoxVFS_Mount(vfs_t *pVFS, vnode_t *pVNode, struct mounta *pMount, cred_t *pCred);
static int VBoxVFS_Unmount(vfs_t *pVFS, int fFlags, cred_t *pCred);
static int VBoxVFS_Root(vfs_t *pVFS, vnode_t **ppVNode);
static int VBoxVFS_Statfs(register vfs_t *pVFS, struct statvfs64 *pStat);
static int VBoxVFS_Sync(vfs_t *pVFS, short fFlags, cred_t *pCred);
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

/**
 * state info. for vboxvfs
 */
typedef struct
{
    /** Device Info handle. */
    dev_info_t             *pDip;
    /** Driver Mutex. */
    kmutex_t                Mtx;
} vboxvfs_state_t;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Opaque pointer to list of states. */
static void *g_pVBoxVFSState;
/** GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xdecea5ed;
/** Global connection to the client. */
static VBSFCLIENT g_VBoxVFSClient;
/** Global VFS Operations pointer. */
vfsops_t *g_pVBoxVFS_vfsops;
/** The file system type identifier. */
static int g_VBoxVFSType;


/**
 * Kernel entry points
 */
int _init(void)
{
    LogFlow((DEVICE_NAME ":_init\n"));
    int rc = ddi_soft_state_init(&g_pVBoxVFSState, sizeof(vboxvfs_state_t), 1);
    if (!rc)
    {
        rc = mod_install(&g_VBoxVFSModLinkage);
        if (rc)
            ddi_soft_state_fini(&g_pVBoxVFSState);
    }
    return rc;
}


int _fini(void)
{
    LogFlow((DEVICE_NAME ":_fini\n"));
    int rc = mod_remove(&g_VBoxVFSModLinkage);
    if (!rc)
        ddi_soft_state_fini(&g_pVBoxVFSState);
    return rc;
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
                    VFSNAME_SYNC,     { .vfs_sync =      VBoxVFS_Sync    },
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
                        g_VBoxVFSType = fType;
                        LogFlow((DEVICE_NAME ":Successfully loaded vboxvfs.\n"));
                        return 0;
                    }
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
    int rc;
    int Uid = 0;
    int Gid = 0;
    char *pszShare;
    size_t cbShare;
    pathname_t PathName;
    vnode_t *pVNodeSpec;
    vnode_t *pVNodeDev;
    dev_t Dev;
    SHFLSTRING *pShflShareName = NULL;
    size_t cbShflShareName;
    vboxvfs_globinfo_t *pVBoxVFSGlobalInfo;
    int AddrSpace = (pMount->flags & MS_SYSSPACE) ? UIO_SYSSPACE : UIO_USERSPACE;
#if 0
    caddr_t pData;
    size_t cbData;
    vboxvfs_mountinfo_t Args;
#endif

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
    if (   !(pMount->flags & MS_REMOUNT)
        && !(pMount->flags & MS_OVERLAY)
        && (pVNode->v_count != -1 || (pVNode->v_flag & VROOT)))
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: device busy.\n"));
        mutex_exit(&pVNode->v_lock);
        return EBUSY;
    }
    mutex_exit(&pVNode->v_lock);

    /* I don't think we really need to support kernel mount arguments anymore. */
#if 0
    /* Retreive arguments. */
    bzero(&Args, sizeof(Args));
    cbData = pMount->datalen;
    pData = pMount->data;
    if (   (pMount->flags & MS_DATA)
        && pData != NULL
        && cbData > 0)
    {
        if (cbData > sizeof(Args))
        {
            LogRel((DEVICE_NAME: "VBoxVFS_Mount: argument length too long. expected=%d. received=%d\n", sizeof(Args), cbData));
            return EINVAL;
        }

        /* Copy arguments; they can be in kernel or user space. */
        rc = ddi_copyin(pData, &Args, cbData, (pMount->flags & MS_SYSSPACE) ? FKIOCTL : 0);
        if (rc)
        {
            LogRel((DEVICE_NAME: "VBoxVFS_Mount: ddi_copyin failed to copy arguments.rc=%d\n", rc));
            return EFAULT;
        }
    }
    else
    {
        cbData = 0;
        pData = NULL;
    }
#endif

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
        /* Get the vnode for the special file for storing the device identifier and path. */
        rc = lookupname(PathName.pn_path, AddrSpace, FOLLOW, NULLVPP, &pVNodeSpec);
        if (!rc)
        {
            /* Check if user has permission to use the special file for mounting. */
            rc = vboxvfs_CheckMountPerm(pVFS, pMount, pVNodeSpec, pCred);
            VN_RELE(pVNodeSpec);
            if (!rc)
            {
                Dev = pVNodeSpec->v_rdev;
                pszShare = RTStrDup(PathName.pn_path);
                cbShare = strlen(pszShare);
            }
            else
                LogRel((DEVICE_NAME ":VBoxVFS_Mount: invalid permissions to mount %s.rc=%d\n", pszShare, rc));
            rc = EPERM;
        }
        else
            LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to lookup sharename.rc=%d\n", rc));
        rc = EINVAL;
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to get special file path.rc=%d\n", rc));
        rc = EINVAL;
    }
    pn_free(&PathName);

    if (!rc)
        return rc;

    /* Get VNode of the special file being mounted. */
    pVNodeDev = makespecvp(Dev, VBLK);
    if (!IS_SWAPVP(pVNodeDev))
    {
        /* Open the vnode for mounting. */
        rc = VOP_OPEN(&pVNodeDev, (pVFS->vfs_flag & VFS_RDONLY) ? FREAD : FREAD | FWRITE, pCred, NULL);
        if (rc)
        {
            LogRel((DEVICE_NAME ":VBoxVFS_Mount: failed to mount.\n"));
            rc = EINVAL;
        }
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: cannot mount from swap.\n"));
        rc = EINVAL;
    }

    if (!rc)
    {
        VN_RELE(pVNodeDev);
        return rc;
    }

    /* Allocate the global info. structure. */
    pVBoxVFSGlobalInfo = RTMemAlloc(sizeof(*pVBoxVFSGlobalInfo));
    if (!pVBoxVFSGlobalInfo)
    {
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: RTMemAlloc failed to alloc %d bytes for global struct.\n", sizeof(*pVBoxVFSGlobalInfo)));
        return ENOMEM;
    }

    cbShflShareName = offsetof (SHFLSTRING, String.utf8) + cbShare + 1;
    pShflShareName  = RTMemAllocZ(cbShflShareName);
    if (!pShflShareName)
    {
        RTMemFree(pVBoxVFSGlobalInfo);
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: RTMemAlloc failed to alloc %d bytes for ShFlShareName.\n", cbShflShareName));
        return ENOMEM;
    }

    pShflShareName->u16Length = cbShflShareName;
    pShflShareName->u16Size   = cbShflShareName + 1;
    memcpy (pShflShareName->String.utf8, pszShare, cbShare + 1);

    rc = vboxCallMapFolder(&g_VBoxVFSClient, pShflShareName, &pVBoxVFSGlobalInfo->Map);
    RTMemFree(pShflShareName);
    if (VBOX_FAILURE (rc))
    {
        RTMemFree(pVBoxVFSGlobalInfo);
        LogRel((DEVICE_NAME ":VBoxVFS_Mount: vboxCallMapFolder failed rc=%d\n", rc));
        return EPROTO;
    }

    /* @todo mutex for protecting the structure. */
    pVBoxVFSGlobalInfo->Uid = Uid;
    pVBoxVFSGlobalInfo->Gid = Gid;
    pVBoxVFSGlobalInfo->pVFS = pVFS;
    pVBoxVFSGlobalInfo->Dev = Dev;
    pVBoxVFSGlobalInfo->pVNodeDev = pVNodeDev;
    pVFS->vfs_data = pVBoxVFSGlobalInfo;
    pVFS->vfs_fstype = g_VBoxVFSType;
    vfs_make_fsid(&pVFS->vfs_fsid, Dev, g_VBoxVFSType);

    /* @todo root vnode */

    return 0;
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

    if (fUnmount & MS_FORCE)
        pVFS->vfs_flag |= VFS_UNMOUNTED;

    /* @todo implement ref-counting of active vnodes & check for busy state here. */
    /* @todo mutex protection needed here */
    pVBoxVFSGlobalInfo = VFS2VBOXVFS(pVFS);

    rc = vboxCallUnmapFolder(&g_VBoxVFSClient, &pVBoxVFSGlobalInfo->Map);
    if (VBOX_FAILURE(rc))
        LogRel((DEVICE_NAME ":VBoxVFS_Unmount: failed to unmap shared folder. rc=%d\n", rc));

    RTMemFree(pVBoxVFSGlobalInfo);
    pVFS->vfs_data = NULL;

    return 0;
}

static int VBoxVFS_Root(vfs_t *pVFS, vnode_t **ppVNode)
{
    return 0;
}

static int VBoxVFS_Statfs(register vfs_t *pVFS, struct statvfs64 *pStat)
{
    return 0;
}

static int VBoxVFS_Sync(vfs_t *pVFS, short fFlags, cred_t *pCred)
{
    return 0;
}

static int VBoxVFS_VGet(vfs_t *pVFS, vnode_t **ppVNode, struct fid *pFid)
{
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

