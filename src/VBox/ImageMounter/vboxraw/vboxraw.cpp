/* $Id$ */
/** @file
 * vboxraw - Disk Image Flattening FUSE Program.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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

#define LOG_GROUP LOG_GROUP_DEFAULT /** @todo log group */
#define VBox_WITH_XPCOM

#define FUSE_USE_VERSION 27
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_FEEBSD)
#   define UNIXY
#endif
#define MAX_READERS (INT32_MAX / 32)

#include <VBox/vd.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/critsect.h>
#include <iprt/assert.h>
#include <iprt/message.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/types.h>
#include <iprt/path.h>

#include <fuse.h>
#ifdef UNIXY
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#if defined(RT_OS_DARWIN) || defined(RT_OS_FREEBSD) || defined(RT_OS_LINUX)
# include <sys/param.h>
# undef PVM     /* Blasted old BSD mess still hanging around darwin. */
#endif

/*
 * Include the XPCOM headers
 */
#include <VirtualBox_XPCOM.h>
#include <nsIComponentRegistrar.h>
#include <nsIServiceManager.h>
#include <nsEventQueueUtils.h>
#include <nsIExceptionService.h>
#include <nsMemory.h>
#include <nsArray.h>
#include <nsString.h>
#include <nsReadableUtils.h>

enum {
     USAGE_FLAG,
};

/* For getting the basename of the image path */
union
{
    RTPATHSPLIT split;
    uint8_t     abPad[RTPATH_MAX + 1024];
} g_u;


/** The VDRead/VDWrite block granularity. */
#define VBoxRAW_MIN_SIZE               512
/** Offset mask corresponding to VBoxRAW_MIN_SIZE. */
#define VBoxRAW_MIN_SIZE_MASK_OFF      (0x1ff)
/** Block mask corresponding to VBoxRAW_MIN_SIZE. */
#define VBoxRAW_MIN_SIZE_MASK_BLK      (~UINT64_C(0x1ff))

#define PADMAX                      50
#define MAX_ID_LEN                  256

static struct fuse_operations   g_vboxrawOps;
PVDISK      g_pVDisk;
int32_t     g_cReaders;
int32_t     g_cWriters;
RTFOFF      g_cbPrimary;
char        *g_pszBaseImageName;
char        *g_pszBaseImagePath;

char *nsIDToString(nsID *guid);
void printErrorInfo();

/** XPCOM stuff */
static struct {
    nsCOMPtr<nsIServiceManager>     serviceManager;
    nsCOMPtr<nsIEventQueue>         eventQ;
    nsCOMPtr<nsIComponentManager>   manager;
    nsCOMPtr<IVirtualBox>           virtualBox;
} g_XPCOM;

static struct vboxrawOpts {
     char *pszVm;
     char *pszImage;
     char *pszImageUuid;
     uint32_t cHddImageDiffMax;
     uint32_t fList;
     uint32_t fWriteable;
     uint32_t fVerbose;
} g_vboxrawOpts;

#define OPTION(fmt, pos, val) { fmt, offsetof(struct vboxrawOpts, pos), val }

static struct fuse_opt vboxrawOptDefs[] = {
    OPTION("--list",          fList,            1),
    OPTION("--vm=%s",         pszVm,            0),
    OPTION("--maxdiff=%d",    cHddImageDiffMax, 1),
    OPTION("--diff=%d",       cHddImageDiffMax, 1),
    OPTION("--image=%s",      pszImage,         0),
    OPTION("--writable",      fWriteable,       1),
    OPTION("--verbose",       fVerbose,         1),
    FUSE_OPT_KEY("-h",        USAGE_FLAG),
    FUSE_OPT_KEY("--help",    USAGE_FLAG),
    FUSE_OPT_END
};

static int vboxrawOptHandler(void *data, const char *arg, int optKey, struct fuse_args *outargs)
{
    (void) data;
    (void) arg;

    char *progname = basename(outargs->argv[0]);
    switch(optKey)
    {
        case USAGE_FLAG:
            RTPrintf("usage: %s [options] <mountpoint>\n\n"
                "%s options:\n"
                "    [--list]                              List media\n"
                "    [--vm <name | UUID >]                 vm UUID (limit media list to specific VM)\n\n"
                "    [--diff=<diff #> ]                    Apply diffs to base image up "
                                                          "to and including specified diff #\n"
                "                                          (0 = no diffs, default: all diffs)\n\n"
                "    [--image=<UUID, name or path>]        Image UUID or path\n"
                "    [--verbose]                           Log extra information\n"
                "    -o opt[,opt...]                       mount options\n"
                "    -h --help                             print usage\n\n",
                    progname, progname);
            RTPrintf("\n");
            RTPrintf("When successful, the --image option creates a one-directory-deep filesystem \n");
            RTPrintf("rooted at the specified mountpoint.  The contents of the directory will be \n");
            RTPrintf("a symbolic link with the base name of the image file pointing to the path of\n");
            RTPrintf("the virtual disk image, and a regular file named 'vhdd', which represents\n");
            RTPrintf("the byte stream of the disk image as interpreted by VirtualBox.\n");
            RTPrintf("It is the vhdd file that the user or a utility will subsequently mount on\n");
            RTPrintf("the host OS to gain access to the virtual disk contents.\n\n");
            fuse_opt_add_arg(outargs, "-ho");
            fuse_main(outargs->argc, outargs->argv, &g_vboxrawOps, NULL);
            exit(1);
    }
    return 1;
}

/** @copydoc fuse_operations::open */
static int vboxrawOp_open(const char *pszPath, struct fuse_file_info *pInfo)
{
    (void) pInfo;
    LogFlowFunc(("pszPath=%s\n", pszPath));
    uint32_t notsup = 0;
    int rc = 0;

#ifdef UNIXY
#   ifdef RT_OS_DARWIN
        notsup = O_APPEND | O_NONBLOCK | O_SYMLINK | O_NOCTTY | O_SHLOCK | O_EXLOCK |
                 O_ASYNC  | O_CREAT    | O_TRUNC   | O_EXCL | O_EVTONLY;
#   elif defined(RT_OS_LINUX)
        notsup = O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK;
                 /* | O_LARGEFILE | O_SYNC | ? */
#   elif defined(RT_OS_FREEBSD)
        notsup = O_APPEND | O_ASYNC | O_DIRECT | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK';
                 /* | O_LARGEFILE | O_SYNC | ? */
#   endif
#else
#   error "Port me"
#endif

if (pInfo->flags & notsup)
    rc -EINVAL;

#ifdef UNIXY
    if ((pInfo->flags & O_ACCMODE) == O_ACCMODE)
        rc = -EINVAL;
#   ifdef O_DIRECTORY
        if (pInfo->flags & O_DIRECTORY)
            rc = -ENOTDIR;
#   endif
#endif

    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("rc=%d \"%s\"\n", rc, pszPath));
        return rc;
    }

    int fWriteable =    (pInfo->flags & O_ACCMODE) == O_WRONLY
                     || (pInfo->flags & O_ACCMODE) == O_RDWR;
    if (g_cWriters)
        rc = -ETXTBSY;
    else
    {
            if (fWriteable)
                g_cWriters++;
            else
            {
                if (g_cReaders + 1 > MAX_READERS)
                    rc = -EMLINK;
                else
                    g_cReaders++;
            }
    }
    LogFlowFunc(("rc=%d \"%s\"\n", rc, pszPath));
    return rc;

}

/** @copydoc fuse_operations::release */
static int vboxrawOp_release(const char *pszPath, struct fuse_file_info *pInfo)
{
    (void) pszPath;

    LogFlowFunc(("pszPath=%s\n", pszPath));

    if (    (pInfo->flags & O_ACCMODE) == O_WRONLY
        ||  (pInfo->flags & O_ACCMODE) == O_RDWR)
    {
        g_cWriters--;
        Assert(g_cWriters >= 0);
    }
    else if ((pInfo->flags & O_ACCMODE) == O_RDONLY)
    {
        g_cReaders--;
        Assert(g_cReaders >= 0);
    }
    else
        AssertFailed();

    LogFlowFunc(("\"%s\"\n", pszPath));
    return 0;
}

/** @copydoc fuse_operations::read */
static int vboxrawOp_read(const char *pszPath, char *pbBuf, size_t cbBuf,
                           off_t offset, struct fuse_file_info *pInfo)
{

    (void) pszPath;
    (void) pInfo;

    LogFlowFunc(("my offset=%#llx size=%#zx path=\"%s\"\n", (uint64_t)offset, cbBuf, pszPath));

    /* paranoia */
    AssertReturn((int)cbBuf >= 0, -EINVAL);
    AssertReturn((unsigned)cbBuf == cbBuf, -EINVAL);
    AssertReturn(offset >= 0, -EINVAL);
    AssertReturn((off_t)(offset + cbBuf) >= offset, -EINVAL);

    int rc;
    if ((off_t)(offset + cbBuf) < offset)
        rc = -EINVAL;
    else if (offset >= g_cbPrimary)
        rc = 0;
    else if (!cbBuf)
        rc = 0;
    else
    {
        /* Adjust for EOF. */
        if ((off_t)(offset + cbBuf) >= g_cbPrimary)
            cbBuf = g_cbPrimary - offset;

        /*
         * Aligned read?
         */
        int rc2;
        if (    !(offset & VBoxRAW_MIN_SIZE_MASK_OFF)
            &&  !(cbBuf   & VBoxRAW_MIN_SIZE_MASK_OFF))
            rc2 = VDRead(g_pVDisk, offset, pbBuf, cbBuf);
        else
        {
            /*
             * Unaligned read - lots of extra work.
             */
            uint8_t abBlock[VBoxRAW_MIN_SIZE];
            if (((offset + cbBuf) & VBoxRAW_MIN_SIZE_MASK_BLK) == (offset & VBoxRAW_MIN_SIZE_MASK_BLK))
            {
                /* a single partial block. */
                rc2 = VDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                if (RT_SUCCESS(rc2))
                    memcpy(pbBuf, &abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], cbBuf);
            }
            else
            {
                /* read unaligned head. */
                rc2 = VINF_SUCCESS;
                if (offset & VBoxRAW_MIN_SIZE_MASK_OFF)
                {
                    rc2 = VDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2))
                    {
                        size_t cbCopy = VBoxRAW_MIN_SIZE - (offset & VBoxRAW_MIN_SIZE_MASK_OFF);
                        memcpy(pbBuf, &abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], cbCopy);
                        pbBuf   += cbCopy;
                        offset += cbCopy;
                        cbBuf   -= cbCopy;
                    }
                }

                /* read the middle. */
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf >= VBoxRAW_MIN_SIZE && RT_SUCCESS(rc2))
                {
                    size_t cbRead = cbBuf & VBoxRAW_MIN_SIZE_MASK_BLK;
                    rc2 = VDRead(g_pVDisk, offset, pbBuf, cbRead);
                    if (RT_SUCCESS(rc2))
                    {
                        pbBuf   += cbRead;
                        offset += cbRead;
                        cbBuf   -= cbRead;
                    }
                }

                /* unaligned tail read. */
                Assert(cbBuf < VBoxRAW_MIN_SIZE);
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf && RT_SUCCESS(rc2))
                {
                    rc2 = VDRead(g_pVDisk, offset, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2))
                        memcpy(pbBuf, &abBlock[0], cbBuf);
                }
            }
        }

        /* convert the return code */
        if (RT_SUCCESS(rc2))
            rc = cbBuf;
        else
            rc = -RTErrConvertToErrno(rc2);
        return rc;
    }
    return rc;
}

/** @copydoc fuse_operations::write */
static int vboxrawOp_write(const char *pszPath, const char *pbBuf, size_t cbBuf,
                           off_t offset, struct fuse_file_info *pInfo)
{

    (void) pszPath;
    (void) pInfo;

    AssertReturn((int)cbBuf >= 0, -EINVAL);
    AssertReturn((unsigned)cbBuf == cbBuf, -EINVAL);
    AssertReturn(offset >= 0, -EINVAL);
    AssertReturn((off_t)(offset + cbBuf) >= offset, -EINVAL);

    LogFlowFunc(("offset=%#llx size=%#zx path=\"%s\"\n", (uint64_t)offset, cbBuf, pszPath));

    int rc;
    if ((off_t)(offset + cbBuf) < offset)
        rc = -EINVAL;
    else if (offset >= g_cbPrimary)
        rc = 0;
    else if (!cbBuf)
        rc = 0;
    else
    {
        /* Adjust for EOF. */
        if ((off_t)(offset + cbBuf) >= g_cbPrimary)
            cbBuf = g_cbPrimary - offset;

        /*
         * Aligned write?
         */
        int rc2;
        if (    !(offset & VBoxRAW_MIN_SIZE_MASK_OFF)
            &&  !(cbBuf   & VBoxRAW_MIN_SIZE_MASK_OFF))
            rc2 = VDWrite(g_pVDisk, offset, pbBuf, cbBuf);
        else
        {
            /*
             * Unaligned write - lots of extra work.
             */
            uint8_t abBlock[VBoxRAW_MIN_SIZE];
            if (((offset + cbBuf) & VBoxRAW_MIN_SIZE_MASK_BLK) == (offset & VBoxRAW_MIN_SIZE_MASK_BLK))
            {
                /* a single partial block. */
                rc2 = VDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                if (RT_SUCCESS(rc2))
                {
                    memcpy(&abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], pbBuf, cbBuf);
                    /* Update the block */
                    rc2 = VDWrite(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                }
            }
            else
            {
                /* read unaligned head. */
                rc2 = VINF_SUCCESS;
                if (offset & VBoxRAW_MIN_SIZE_MASK_OFF)
                {
                    rc2 = VDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2))
                    {
                        size_t cbCopy = VBoxRAW_MIN_SIZE - (offset & VBoxRAW_MIN_SIZE_MASK_OFF);
                        memcpy(&abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], pbBuf, cbCopy);
                        pbBuf   += cbCopy;
                        offset += cbCopy;
                        cbBuf   -= cbCopy;
                        rc2 = VDWrite(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                    }
                }

                /* write the middle. */
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf >= VBoxRAW_MIN_SIZE && RT_SUCCESS(rc2))
                {
                    size_t cbWrite = cbBuf & VBoxRAW_MIN_SIZE_MASK_BLK;
                    rc2 = VDWrite(g_pVDisk, offset, pbBuf, cbWrite);
                    if (RT_SUCCESS(rc2))
                    {
                        pbBuf   += cbWrite;
                        offset += cbWrite;
                        cbBuf   -= cbWrite;
                    }
                }

                /* unaligned tail write. */
                Assert(cbBuf < VBoxRAW_MIN_SIZE);
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf && RT_SUCCESS(rc2))
                {
                    rc2 = VDRead(g_pVDisk, offset, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2))
                    {
                        memcpy(&abBlock[0], pbBuf, cbBuf);
                        rc2 = VDWrite(g_pVDisk, offset, abBlock, VBoxRAW_MIN_SIZE);
                    }
                }
            }
        }

        /* convert the return code */
        if (RT_SUCCESS(rc2))
            rc = cbBuf;
        else
            rc = -RTErrConvertToErrno(rc2);
            return rc;
    }
    return rc;
}

/** @copydoc fuse_operations::getattr */
static int vboxrawOp_getattr(const char *pszPath,
            struct stat *stbuf)
{
    int rc = 0;

    LogFlowFunc(("pszPath=%s, stat(\"%s\")\n", pszPath, g_pszBaseImagePath));

    memset(stbuf, 0, sizeof(struct stat));

    if (RTStrCmp(pszPath, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if (RTStrCmp(pszPath + 1, "vhdd") == 0)
    {
        rc = stat(g_pszBaseImagePath, stbuf);
        if (rc < 0)
            return rc;
        stbuf->st_nlink = 1;
    }
    else if (RTStrCmp(pszPath + 1, g_pszBaseImageName) == 0)
    {
        rc = stat(g_pszBaseImagePath, stbuf);
        if (rc < 0)
            return rc;
        stbuf->st_size = 0;
        stbuf->st_mode = S_IFLNK | 0444;
        stbuf->st_nlink = 1;
    } else
        rc = -ENOENT;

    return rc;
}

/** @copydoc fuse_operations::readdir */
static int vboxrawOp_readdir(const char *pszPath, void *pvBuf, fuse_fill_dir_t pfnFiller,
                              off_t offset, struct fuse_file_info *pInfo)

{

    (void) offset;
    (void) pInfo;

    if (RTStrCmp(pszPath, "/") != 0)
        return -ENOENT;

    /*
     * The usual suspects (mandatory)
     */
    pfnFiller(pvBuf, ".", NULL, 0);
    pfnFiller(pvBuf, "..", NULL, 0);

    /*
     * Create one entry w/basename of original image that getattr() will
     * depict as a symbolic link pointing back to the original file,
     * as a convenience, so anyone who lists the FUSE file system can
     * easily find the associated vdisk.
     */
    pfnFiller(pvBuf, g_pszBaseImageName, NULL, 0);

    /*
     * Create entry named "vhdd", which getattr() will describe as a
     * regular file, and thus will go through the open/release/read/write vectors
     * to access the VirtualBox image as processed by the IRPT VD API.
     */
    pfnFiller(pvBuf, "vhdd", NULL, 0);
    return 0;
}

/** @copydoc fuse_operations::readlink */
static int vboxrawOp_readlink(const char *pszPath, char *buf, size_t size)
{
    (void) pszPath;
    RTStrCopy(buf, size, g_pszBaseImagePath);
    return 0;
}


static int
listMedia(IMachine *pMachine)
{
    IMediumAttachment **pMediumAttachments = NULL;
    PRUint32 pMediumAttachmentsSize = 0;
    nsresult rc = pMachine->GetMediumAttachments(&pMediumAttachmentsSize,  &pMediumAttachments);
    if (NS_SUCCEEDED(rc))
    {
        for (PRUint32 i = 0; i < pMediumAttachmentsSize; i++)
        {
            IMedium *pMedium;
            DeviceType_T deviceType;
            nsXPIDLString pMediumUuid;
            nsXPIDLString pMediumName;
            nsXPIDLString pMediumPath;

            pMediumAttachments[i]->GetType(&deviceType);
            if (deviceType == DeviceType_HardDisk)
            {
                rc = pMediumAttachments[i]->GetMedium(&pMedium);
                if (NS_SUCCEEDED(rc) && pMedium)
                {
                    IMedium *pParent = pMedium;
                    IMedium *pEarliestAncestor;
                    while (pParent != nsnull)
                    {
                        pEarliestAncestor = pParent;
                        pParent->GetParent(&pParent);
                    }
                    PRUint32 cChildren = 1;
                    IMedium **aMediumChildren = nsnull;
                    IMedium *pChild = pEarliestAncestor;
                    uint32_t ancestorNumber = 0;
                    RTPrintf("\n");
                    do
                    {
                        rc = pChild->GetName(getter_Copies(pMediumName));
                        if (NS_FAILED(rc))
                             return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                                "Couldn't access pMedium name rc=%#x\n", rc);

                        rc = pChild->GetId(getter_Copies(pMediumUuid));
                        if (NS_FAILED(rc))
                             return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                                "Couldn't access pMedium ID rc=%#x\n", rc);

                        rc = pChild->GetLocation(getter_Copies(pMediumPath));
                        if (NS_FAILED(rc))
                             return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                                "Couldn't access pMedium location rc=%#x\n", rc);

                        const char *pszMediumName = ToNewCString(pMediumName);
                        const char *pszMediumUuid = ToNewCString(pMediumUuid);
                        const char *pszMediumPath = ToNewCString(pMediumPath);

                        if (ancestorNumber == 0)
                        {
                            RTPrintf("   -----------------------\n");
                            RTPrintf("   HDD base:   \"%s\"\n",   pszMediumName);
                            RTPrintf("   UUID:       %s\n",       pszMediumUuid);
                            RTPrintf("   Location:   %s\n\n",     pszMediumPath);
                        }
                        else
                        {
                            RTPrintf("     Diff %d:\n", ancestorNumber);
                            RTPrintf("          UUID:       %s\n",   pszMediumUuid);
                            RTPrintf("          Location:   %s\n\n", pszMediumPath);
                        }

                        free((void*)pszMediumName);
                        free((void*)pszMediumUuid);
                        free((void*)pszMediumPath);

                        rc = pChild->GetChildren(&cChildren, &aMediumChildren);
                        if (NS_FAILED(rc))
                             return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                                "could not get children of media! rc=%#x\n", rc);

                        pChild = aMediumChildren[0];
                        ++ancestorNumber;
                    } while(NS_SUCCEEDED(rc) && cChildren);
                }
                pMedium->Release();
            }
        }
    }
    return rc;

}
/**
 * Display all registered VMs on the screen with some information about each
 *
 * @param virtualBox VirtualBox instance object.
 */
static int
listVMs(IVirtualBox *virtualBox)
{
    IMachine **ppMachines = NULL;
    PRUint32 cMachines = 0;
    nsresult rc;

    rc = virtualBox->GetMachines(&cMachines, &ppMachines);
    if (NS_SUCCEEDED(rc))
    {
        for (PRUint32 i = 0; i < cMachines; ++ i)
        {
            IMachine *pMachine = ppMachines[i];
            if (pMachine)
            {
                PRBool isAccessible = PR_FALSE;
                pMachine->GetAccessible(&isAccessible);

                if (isAccessible)
                {
                    nsXPIDLString pMachineUuid;
                    nsXPIDLString pMachineName;
                    nsXPIDLString pMachinePath;

                    rc = pMachine->GetName(getter_Copies(pMachineName));
                    if (NS_FAILED(rc))
                         return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                            "Couldn't access pMachine name rc=%#x\n", rc);

                    rc = pMachine->GetId(getter_Copies(pMachineUuid));
                    if (NS_FAILED(rc))
                         return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                            "Couldn't access pMachine Id rc=%#x\n", rc);

                    rc = pMachine->GetStateFilePath(getter_Copies(pMachinePath));
                    if (NS_FAILED(rc))
                         return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                            "Couldn't access pMachine Location rc=%#x\n", rc);

                    const char *pszMachineName = ToNewCString(pMachineName);
                    const char *pszMachineUuid = ToNewCString(pMachineUuid);
                    const char *pszMachinePath = ToNewCString(pMachinePath);

                    if (   g_vboxrawOpts.pszVm == NULL
                        || RTStrNCmp(pszMachineUuid, g_vboxrawOpts.pszVm, MAX_ID_LEN) == 0
                        || RTStrNCmp(pszMachineName, g_vboxrawOpts.pszVm, MAX_ID_LEN) == 0)
                    {
                        RTPrintf("------------------------------------------------------\n");
                        RTPrintf("VM Name:   \"%s\"\n", pszMachineName);
                        RTPrintf("UUID:      %s\n",     pszMachineUuid);
                        RTPrintf("Location:  %s\n",     pszMachinePath);

                        rc = listMedia(pMachine);
                        if (NS_FAILED(rc))
                            return rc;

                        RTPrintf("\n");
                    }
                    free((void *)pszMachineUuid);
                    free((void *)pszMachineName);
                    free((void *)pszMachinePath);
                }
                pMachine->Release();
            }
        }
    }
    return rc;
}

static int
searchForBaseImage(IVirtualBox *virtualBox, char *pszImageString, IMedium **baseImage)
{
    PRUint32 cDisks;
    IMedium **ppDisks;

    nsresult rc = virtualBox->GetHardDisks(&cDisks, &ppDisks);
    if (NS_SUCCEEDED(rc))
    {
        for (PRUint32 i = 0; i < cDisks && ppDisks[i]; i++)
        {
            if (ppDisks[i])
            {
                nsXPIDLString pMediumUuid;
                nsXPIDLString pMediumName;

                rc = ppDisks[i]->GetName(getter_Copies(pMediumName));
                if (NS_FAILED(rc))
                     return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                        "Couldn't access pMedium name rc=%#x\n", rc);

                rc = ppDisks[i]->GetId(getter_Copies(pMediumUuid));
                if (NS_FAILED(rc))
                     return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                        "Couldn't access pMedium Id rc=%#x\n", rc);

                const char *pszMediumName = ToNewCString(pMediumName);
                const char *pszMediumUuid = ToNewCString(pMediumUuid);

                if (   RTStrCmp(pszImageString, pszMediumUuid) == 0
                    || RTStrCmp(pszImageString, pszMediumName) == 0)
                {
                    *baseImage = ppDisks[i];
                    free((void*)pszMediumUuid);
                    free((void*)pszMediumName);
                    return rc;
                }
                ppDisks[i]->Release();
                free((void*)pszMediumUuid);
                free((void*)pszMediumName);
            }
        }
    }
    return rc;
}

int
main(int argc, char **argv)
{

    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("vboxraw: ERROR: RTR3InitExe failed, rc=%Rrc\n", rc);

    rc = VDInit();
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("vboxraw: ERROR: VDInit failed, rc=%Rrc\n", rc);

    memset(&g_vboxrawOps, 0, sizeof(g_vboxrawOps));
    g_vboxrawOps.open        = vboxrawOp_open;
    g_vboxrawOps.read        = vboxrawOp_read;
    g_vboxrawOps.write       = vboxrawOp_write;
    g_vboxrawOps.getattr     = vboxrawOp_getattr;
    g_vboxrawOps.release     = vboxrawOp_release;
    g_vboxrawOps.readdir     = vboxrawOp_readdir;
    g_vboxrawOps.readlink    = vboxrawOp_readlink;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    memset(&g_vboxrawOpts, 0, sizeof(g_vboxrawOpts));

    rc = fuse_opt_parse(&args, &g_vboxrawOpts, vboxrawOptDefs, vboxrawOptHandler);
    if (rc == -1)
        return RTMsgErrorExitFailure("vboxraw: ERROR: Couldn't parse fuse options, rc=%Rrc\n", rc);

    /*
     * Initialize XPCOM to so we can use the API
     */
    rc = NS_InitXPCOM2(getter_AddRefs(g_XPCOM.serviceManager), nsnull, nsnull);
    if (NS_FAILED(rc))
         return RTMsgErrorExitFailure("vboxraw: ERROR: "
            "XPCOM could not be initialized! rc=%#x\n", rc);

    rc = NS_GetMainEventQ(getter_AddRefs(g_XPCOM.eventQ));
    if (NS_FAILED(rc))
         return RTMsgErrorExitFailure("vboxraw: ERROR: "
            "could not get main event queue! rc=%#x\n", rc);

    rc = NS_GetComponentManager(getter_AddRefs(g_XPCOM.manager));
    if (NS_FAILED(rc))
         return RTMsgErrorExitFailure("vboxraw: ERROR: "
            "couldn't get the component manager! rc=%#x\n", rc);

    rc = g_XPCOM.manager->CreateInstanceByContractID(NS_VIRTUALBOX_CONTRACTID,
                                             nsnull,
                                             NS_GET_IID(IVirtualBox),
                                             getter_AddRefs(g_XPCOM.virtualBox));
    if (NS_FAILED(rc))
         return RTMsgErrorExitFailure("vboxraw: ERROR: "
            "could not instantiate VirtualBox object! rc=%#x\n", rc);

    if (g_vboxrawOpts.fVerbose)
        RTPrintf("vboxraw: VirtualBox XPCOM object created\n");

    if (g_vboxrawOpts.fList)
        return listVMs(g_XPCOM.virtualBox);

    if (g_vboxrawOpts.pszImage == NULL)
            RTMsgErrorExitFailure("vboxraw: ERROR: "
                "Must provide at at least --list or --image option\n");

    IMedium *pBaseImageMedium = NULL;
    char    *pszFormat;
    VDTYPE  enmType;

    rc = searchForBaseImage(g_XPCOM.virtualBox, g_vboxrawOpts.pszImage, &pBaseImageMedium);
    if (NS_FAILED(rc))
        RTMsgErrorExitFailure("vboxraw: ERROR: "
            "Couldn't locate base image \"%s\"\n", g_vboxrawOpts.pszImage);

    if (pBaseImageMedium == NULL)
    {
        /*
         * Try to locate base image pMedium via the VirtualBox API, given the user-provided path
         * resolving symlinks back to hard path.
         */
        int cbNameMax = pathconf(g_vboxrawOpts.pszImage, _PC_PATH_MAX);
        if (cbNameMax < 0)
            return cbNameMax;

        char *pszBounceA = (char *)RTMemAlloc(cbNameMax + 1);
        if (!pszBounceA)
            return VERR_NO_MEMORY;
        memset(pszBounceA, 0, cbNameMax + 1);

        char *pszBounceB = (char *)RTMemAlloc(cbNameMax + 1);
        if (!pszBounceB)
            return VERR_NO_MEMORY;
        memset(pszBounceB, 0, cbNameMax + 1);

        RTStrCopy(pszBounceA, cbNameMax, g_vboxrawOpts.pszImage);
        while (readlink(pszBounceA, pszBounceB, cbNameMax) >= 0)
            RTStrCopy(pszBounceA, cbNameMax, pszBounceB);

        free(g_vboxrawOpts.pszImage);

        g_pszBaseImagePath = RTStrDup(pszBounceA);
        if (g_pszBaseImagePath == NULL)
            return RTMsgErrorExitFailure("vboxraw: out of memory\n");

        RTMemFree(pszBounceA);
        RTMemFree(pszBounceB);

        if (access(g_pszBaseImagePath, F_OK) < 0)
            return RTMsgErrorExitFailure("vboxraw: ERROR: "
                    "Virtual disk image not found: \"%s\"\n", g_pszBaseImagePath);

        if (access(g_pszBaseImagePath, R_OK) < 0)
             return RTMsgErrorExitFailure("vboxraw: ERROR: "
                    "Virtual disk image not readable: \"%s\"\n", g_pszBaseImagePath);

        if ( g_vboxrawOpts.fWriteable && access(g_vboxrawOpts.pszImage, W_OK) < 0)
             return RTMsgErrorExitFailure("vboxraw: ERROR: "
                    "Virtual disk image not writeable: \"%s\"\n", g_pszBaseImagePath);
        rc = RTPathSplit(g_pszBaseImagePath, &g_u.split, sizeof(g_u), 0);

        if (RT_FAILURE(rc))
             return RTMsgErrorExitFailure("vboxraw: "
                    "RTPathSplit failed on '%s': %Rrc",  g_pszBaseImagePath);

        if (!(g_u.split.fProps & RTPATH_PROP_FILENAME))
             return RTMsgErrorExitFailure("vboxraw: "
                    "RTPATH_PROP_FILENAME not set for: '%s'",  g_pszBaseImagePath);

        g_pszBaseImageName = g_u.split.apszComps[g_u.split.cComps - 1];
        rc = searchForBaseImage(g_XPCOM.virtualBox, g_pszBaseImageName, &pBaseImageMedium);

        if (pBaseImageMedium == NULL)
        {
            /*
             *  Can't find the user specified image Medium via the VirtualBox API
             *  Try to 'mount' the image via the user-provided path (without differencing images)
             *  Create VirtualBox disk container and open main image
             */
            rc = VDGetFormat(NULL /* pVDIIfsDisk */, NULL /* pVDIIfsImage*/,
                g_pszBaseImagePath, &pszFormat, &enmType);
            if (RT_FAILURE(rc))
                return RTMsgErrorExitFailure("vboxraw: ERROR: VDGetFormat(%s,) "
                    "failed, rc=%Rrc\n", g_pszBaseImagePath, rc);

            g_pVDisk = NULL;
            rc = VDCreate(NULL /* pVDIIfsDisk */, enmType, &g_pVDisk);
            if (RT_SUCCESS(rc))
            {
                rc = VDOpen(g_pVDisk, pszFormat, g_pszBaseImagePath, 0, NULL /* pVDIfsImage */);
                if (RT_FAILURE(rc))
                {
                    VDClose(g_pVDisk, false /* fDeletes */);
                    return RTMsgErrorExitFailure("vboxraw: ERROR: VDCreate(,%s,%s,,,) failed,"
                        " rc=%Rrc\n", pszFormat, g_pszBaseImagePath, rc);
                }
            }
            else
                return RTMsgErrorExitFailure("vboxraw: ERROR: VDCreate failed, rc=%Rrc\n", rc);
        }
    }

    if (g_pVDisk == NULL)
    {
        IMedium **aMediumChildren = nsnull;
        IMedium *pChild = pBaseImageMedium;
        PRUint32 cChildren;
        uint32_t diffNumber = 0; /* diff # 0 = base image */

        do
        {
            nsXPIDLString pMediumUuid;
            nsXPIDLString pMediumName;
            nsXPIDLString pMediumPath;

            rc = pChild->GetName(getter_Copies(pMediumName));
            if (NS_FAILED(rc))
                 return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                    "Couldn't access pMedium name rc=%#x\n", rc);

            rc = pChild->GetId(getter_Copies(pMediumUuid));
            if (NS_FAILED(rc))
                 return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                    "Couldn't access pMedium ID rc=%#x\n", rc);

            rc = pChild->GetLocation(getter_Copies(pMediumPath));
            if (NS_FAILED(rc))
                 return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                    "Couldn't access pMedium location rc=%#x\n", rc);

            const char *pszMediumName = ToNewCString(pMediumName);
            const char *pszMediumUuid = ToNewCString(pMediumUuid);
            const char *pszMediumPath = ToNewCString(pMediumPath);

            if (pChild == pBaseImageMedium)
            {
                free((void *)g_pszBaseImageName);
                g_pszBaseImageName = RTStrDup(pszMediumName);

                free((void *)g_pszBaseImagePath);
                g_pszBaseImagePath = RTStrDup(pszMediumPath);

                free((void *)pszMediumUuid);
                free((void *)pszMediumName);

                if (g_pszBaseImageName == NULL)
                    return RTMsgErrorExitFailure("vboxraw: out of memory\n");

                if (g_pszBaseImagePath == NULL)
                    return RTMsgErrorExitFailure("vboxraw: out of memory\n");
                /*
                 * Create HDD container to open base image and differencing images into
                 */
                rc = VDGetFormat(NULL /* pVDIIfsDisk */, NULL /* pVDIIfsImage*/,
                        g_pszBaseImagePath, &pszFormat, &enmType);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExitFailure("vboxraw: VDGetFormat(,,%s,,) "
                        "failed (during HDD container creation), rc=%Rrc\n", g_pszBaseImagePath, rc);

                if (g_vboxrawOpts.fVerbose)
                    RTPrintf("vboxraw: Creating container for base image of format %s\n", pszFormat);

                g_pVDisk = NULL;
                rc = VDCreate(NULL /* pVDIIfsDisk */, enmType, &g_pVDisk);
                if (NS_FAILED(rc))
                    return RTMsgErrorExitFailure("vboxraw: ERROR: Couldn't create virtual disk container\n");
            }
            else
            {
                free((void *)pszMediumUuid);
                free((void *)pszMediumName);
            }
            if ( g_vboxrawOpts.cHddImageDiffMax != 0 && diffNumber > g_vboxrawOpts.cHddImageDiffMax)
                break;

            if (g_vboxrawOpts.fVerbose)
            {
                if (diffNumber == 0)
                    RTPrintf("\nvboxraw: Opening base image into container:\n       %s\n",
                        pszMediumPath);
                else
                    RTPrintf("\nvboxraw: Opening difference image #%d into container:\n       %s\n",
                        diffNumber, pszMediumPath);
            }

            rc = VDOpen(g_pVDisk, pszFormat, pszMediumPath, 0, NULL /* pVDIfsImage */);
            if (RT_FAILURE(rc))
            {
                VDClose(g_pVDisk, false /* fDeletes */);
                free((void *)pszMediumPath);
                return RTMsgErrorExitFailure("vboxraw: VDOpen(,,%s,,) failed, rc=%Rrc\n", pszMediumPath, rc);
            }

            rc = pChild->GetChildren(&cChildren, &aMediumChildren);
            if (NS_FAILED(rc))
            {
                free((void *)pszMediumPath);
                return RTMsgErrorExitFailure("vboxraw: VBox API/XPCOM ERROR: "
                    "Couldn't get children of medium, rc=%Rrc\n", pszMediumPath, rc);
            }

            pChild = aMediumChildren[0];
            ++diffNumber;
            free((void *)pszMediumPath);


        } while(NS_SUCCEEDED(rc) && cChildren);
    }

    g_cReaders    = VDIsReadOnly(g_pVDisk) ? INT32_MAX / 2 : 0;
    g_cWriters   = 0;
    g_cbPrimary  = VDGetSize(g_pVDisk, 0 /* base */);

    /*
     * Hand control over to libfuse.
     */
    if (g_vboxrawOpts.fVerbose)
        RTPrintf("\nvboxraw: Going into background...\n");

    rc = fuse_main(args.argc, args.argv, &g_vboxrawOps, NULL);

    int rc2 = VDClose(g_pVDisk, false /* fDelete */);
    AssertRC(rc2);
    RTPrintf("vboxraw: fuse_main -> %d\n", rc);
    return rc;
}

