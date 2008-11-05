/* $Id$ */
/** @file
 * VirtualBox File System Driver for Solaris Guests, Utility functions.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <time.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/sunddi.h>
#include "vboxvfs.h"

#include <VBox/log.h>
#include <iprt/time.h>
#ifdef DEBUG_ramshankar
# undef LogFlow
# undef Log
# define LogFlow    LogRel
# define Log        LogRel
#endif

/**
 * Convert from RTTIMESPEC to timestruct_t.
 *
 * @param   pTime       Pointer to destination timestruct_t object.
 * @param   pRTTime     Pointer to source time RTTIMESPEC object.
 */
static void vboxvfs_FileTimeFromTimeSpec(timestruc_t *pTime, PRTTIMESPEC pRTTime)
{
    int64_t t = RTTimeSpecGetNano(pRTTime);
    int64_t nsec = t / 1000000000;

    pTime->tv_sec = t;
    pTime->tv_nsec = nsec;
}


/**
 * Stat for a file on the host.
 *
 * @returns errno error code.
 * @param   pszCaller               Entity calling this function (just used for logging sake)
 * @param   pVBoxVFSGlobalInfo      Pointer to the global filesystem info. struct.
 * @param   pPath                   Pointer to file path on the guest to stat.
 * @param   pResult                 Where to store the result of stat.
 * @param   fAllowFailure           Whether failure is acceptable to the caller (currently just logging).
 */
int vboxvfs_Stat(const char *pszCaller, vboxvfs_globinfo_t *pVBoxVFSGlobalInfo, SHFLSTRING *pPath,
            RTFSOBJINFO *pResult, boolean_t fAllowFailure)
{
    int rc;
    SHFLCREATEPARMS Params;

    LogFlow((DEVICE_NAME ":vboxvfs_Stat caller=%s fAllowFailure=%d\n", pszCaller, fAllowFailure));

    Params.CreateFlags = SHFL_CF_LOOKUP | SHFL_CF_ACT_FAIL_IF_NEW;
    rc = vboxCallCreate(&g_VBoxVFSClient, &pVBoxVFSGlobalInfo->Map, pPath, &Params);
    if (RT_FAILURE(rc))
    {
        Log((DEVICE_NAME ":vboxCallCreate failed! caller=%s rc=%Rrc\n", pszCaller, rc));
        return EPROTO;
    }

    if (Params.Result != SHFL_FILE_EXISTS)
    {
        if (fAllowFailure)
        {
            LogRel((DEVICE_NAME ":vboxCallCreate(%s) file does not exist. caller=%s result=%d\n",
                    pPath->String.utf8, Params.Result, pszCaller));
        }
        return ENOENT;
    }
    *pResult = Params.Info;
    return 0;
}

/**
 * Initializes VNode structure.
 *
 * @param   pVBoxVFSGlobalInfo      Pointer to the global filesystem info. struct.
 * @param   pVBoxVNode              Pointer to the pre-allocated vboxvfs_vnode_t object to initialize.
 * @param   pInfo                   Pointer to the RTFSOBJINFO used for initialization.
 */
void vboxvfs_InitVNode(vboxvfs_globinfo_t *pVBoxVFSGlobalInfo, vboxvfs_vnode_t *pVBoxVNode,
            PRTFSOBJINFO pFSInfo)
{
    RTFSOBJATTR *pFSAttr;
    int fDir;
    vfs_t *pVFS;

    LogFlow((DEVICE_NAME ":vboxvfs_InitVNode pVBoxVFGSGlobalInfo=%p pVBoxVNode=%p pFSInfo=%p\n", pVBoxVFSGlobalInfo,
            pVBoxVNode, pFSInfo));

    mutex_enter(&pVBoxVNode->MtxContents);

    pVFS = VBOXVFS_TO_VFS(pVBoxVFSGlobalInfo);
    pFSAttr = &pFSInfo->Attr;
    fDir = RTFS_IS_DIRECTORY(pFSAttr->fMode);
    int Mode = 0;

#define VBOXMODESET(r) pFSAttr->fMode & (RTFS_UNIX_##r) ? (S_##r) : 0;
    Mode |= VBOXMODESET(ISUID);
    Mode |= VBOXMODESET(ISGID);

    Mode |= VBOXMODESET(IRUSR);
    Mode |= VBOXMODESET(IWUSR);
    Mode |= VBOXMODESET(IXUSR);

    Mode |= VBOXMODESET(IRGRP);
    Mode |= VBOXMODESET(IWGRP);
    Mode |= VBOXMODESET(IXGRP);

    Mode |= VBOXMODESET(IROTH);
    Mode |= VBOXMODESET(IWOTH);
    Mode |= VBOXMODESET(IXOTH);
#undef VBOXMODESET

    bzero(&pVBoxVNode->Attr, sizeof(vattr_t));
    if (fDir)
    {
        pVBoxVNode->Attr.va_mode = (mode_t)(S_IFDIR | Mode);
        pVBoxVNode->Attr.va_type = VDIR;
    }
    else
    {
        pVBoxVNode->Attr.va_mode = (mode_t)(S_IFREG | Mode);
        pVBoxVNode->Attr.va_type = VREG;
    }

    pVBoxVNode->Attr.va_rdev = 0;   /* @todo Verify if setting it to zero is okay, not sure of this. */
    pVBoxVNode->Attr.va_mask = 0;
    pVBoxVNode->Attr.va_seq = 0;
    pVBoxVNode->Attr.va_nlink = 1;
    pVBoxVNode->Attr.va_uid = pVBoxVFSGlobalInfo->Uid;
    pVBoxVNode->Attr.va_gid = pVBoxVFSGlobalInfo->Gid;
    pVBoxVNode->Attr.va_fsid = pVFS->vfs_dev;
    pVBoxVNode->Attr.va_size = pFSInfo->cbObject;
    pVBoxVNode->Attr.va_nblocks = (pFSInfo->cbObject + 4095) / 4096;
    pVBoxVNode->Attr.va_blksize = 4096;

    vboxvfs_FileTimeFromTimeSpec(&pVBoxVNode->Attr.va_atime, &pFSInfo->AccessTime);
    vboxvfs_FileTimeFromTimeSpec(&pVBoxVNode->Attr.va_ctime, &pFSInfo->ChangeTime);
    vboxvfs_FileTimeFromTimeSpec(&pVBoxVNode->Attr.va_mtime, &pFSInfo->ModificationTime);

    /* Allocate and initialize the vnode */
    pVBoxVNode->pVNode = vn_alloc(KM_SLEEP);

    vn_setops(pVBoxVNode->pVNode, g_pVBoxVFS_vnodeops);
    pVBoxVNode->pVNode->v_data = pVBoxVNode;
    pVBoxVNode->pVNode->v_vfsp = pVFS;
    pVBoxVNode->pVNode->v_type = pVBoxVNode->Attr.va_type;
    pVBoxVNode->pVNode->v_rdev = pVBoxVNode->Attr.va_rdev;

    mutex_exit(&pVBoxVNode->MtxContents);
    vn_exists(pVBoxVNode->pVNode);
}

