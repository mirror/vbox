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
#define UNUSED(x) (void)(x)

#define FUSE_USE_VERSION 27
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_FEEBSD)
#   define UNIX_DERIVATIVE
#endif
#define MAX_READERS (INT32_MAX / 32)

#include <fuse.h>
#ifdef UNIX_DERIVATIVE
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
#ifdef RT_OS_LINUX
# include <linux/fs.h>
# include <linux/hdreg.h>
#endif
#include <VirtualBox_XPCOM.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/vd.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/NativeEventQueue.h>
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/errorprint.h>

#include <iprt/initterm.h>
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

using namespace com;

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
#define CSTR(arg) Utf8Str(arg).c_str()

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

static struct vboxrawOpts {
     char *pszVm;
     char *pszImage;
     char *pszImageUuid;
     uint32_t cHddImageDiffMax;
     uint32_t fList;
     uint32_t fAllowRoot;
     uint32_t fRW;
     uint32_t fVerbose;
} g_vboxrawOpts;

#define OPTION(fmt, pos, val) { fmt, offsetof(struct vboxrawOpts, pos), val }

static struct fuse_opt vboxrawOptDefs[] = {
    OPTION("--list",          fList,            1),
    OPTION("--root",          fAllowRoot,       1),
    OPTION("--vm=%s",         pszVm,            0),
    OPTION("--maxdiff=%d",    cHddImageDiffMax, 1),
    OPTION("--diff=%d",       cHddImageDiffMax, 1),
    OPTION("--image=%s",      pszImage,         0),
    OPTION("--rw",            fRW,              1),
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
                "    [--root]                              Same as -o allow_root\n"
                "    [--rw]                                writeable (default = readonly)\n"
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

#ifdef UNIX_DERIVATIVE
#   ifdef RT_OS_DARWIN
        notsup = O_APPEND | O_NONBLOCK | O_SYMLINK | O_NOCTTY | O_SHLOCK | O_EXLOCK |
                 O_ASYNC  | O_CREAT    | O_TRUNC   | O_EXCL | O_EVTONLY;
#   elif defined(RT_OS_LINUX)
        notsup = O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK;
                 /* | O_LARGEFILE | O_SYNC | ? */
#   elif defined(RT_OS_FREEBSD)
        notsup = O_APPEND | O_ASYNC | O_DIRECT | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK;
                 /* | O_LARGEFILE | O_SYNC | ? */
#   endif
#else
#   error "Port me"
#endif

if (pInfo->flags & notsup)
    rc -EINVAL;

#ifdef UNIX_DERIVATIVE
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

static int retryableVDRead(PVDISK pvDisk, uint64_t offset, void *pvBuf, size_t cbRead)
{
    int rc = -1;
    int cRetry = 5;
    do
    {
        if (cRetry < 5)
            Log(("(rc=%d retrying read)\n", rc));
        rc = VDRead(pvDisk, offset, pvBuf, cbRead);
    } while (RT_FAILURE(rc) && --cRetry);
    return rc;
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
        {
                rc2 = retryableVDRead(g_pVDisk, offset, pbBuf, cbBuf);
                if (RT_FAILURE(rc2))
                {
                    rc = -RTErrConvertToErrno(rc2);
                    LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                    return rc;
                }
        }
        else
        {
            /*
             * Unaligned read - lots of extra work.
             */
            uint8_t abBlock[VBoxRAW_MIN_SIZE];
            if (((offset + cbBuf) & VBoxRAW_MIN_SIZE_MASK_BLK) == (offset & VBoxRAW_MIN_SIZE_MASK_BLK))
            {
                /* a single partial block. */
                rc2 = retryableVDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                if (RT_SUCCESS(rc2))
                {
                    memcpy(pbBuf, &abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], cbBuf);
                }
                else
                {
                    rc = -RTErrConvertToErrno(rc2);
                    LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                    return rc;
                }
            }
            else
            {
                /* read unaligned head. */
                rc2 = VINF_SUCCESS;
                if (offset & VBoxRAW_MIN_SIZE_MASK_OFF)
                {
                    rc2 = retryableVDRead(g_pVDisk, offset & VBoxRAW_MIN_SIZE_MASK_BLK, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2))
                    {
                        size_t cbCopy = VBoxRAW_MIN_SIZE - (offset & VBoxRAW_MIN_SIZE_MASK_OFF);
                        memcpy(pbBuf, &abBlock[offset & VBoxRAW_MIN_SIZE_MASK_OFF], cbCopy);
                        pbBuf   += cbCopy;
                        offset += cbCopy;
                        cbBuf   -= cbCopy;
                    }
                    else
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
                    }
                }

                /* read the middle. */
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf >= VBoxRAW_MIN_SIZE && RT_SUCCESS(rc2))
                {
                    size_t cbRead = cbBuf & VBoxRAW_MIN_SIZE_MASK_BLK;
                    rc2 = retryableVDRead(g_pVDisk, offset, pbBuf, cbRead);
                    if (RT_SUCCESS(rc2))
                    {
                        pbBuf   += cbRead;
                        offset += cbRead;
                        cbBuf   -= cbRead;
                    }
                    else
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
                    }
                }

                /* unaligned tail read. */
                Assert(cbBuf < VBoxRAW_MIN_SIZE);
                Assert(!(offset & VBoxRAW_MIN_SIZE_MASK_OFF));
                if (cbBuf && RT_SUCCESS(rc2))
                {
                    rc2 = retryableVDRead(g_pVDisk, offset, abBlock, VBoxRAW_MIN_SIZE);
                    if (RT_SUCCESS(rc2)) {
                        memcpy(pbBuf, &abBlock[0], cbBuf);
                    }
                    else
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
                    }
                }
            }
        }

        /* convert the return code */
        if (RT_SUCCESS(rc2))
            rc = cbBuf;
        else
        {
            rc = -RTErrConvertToErrno(rc2);
            LogFlowFunc(("Error rc2=%d, rc=%d=%s\n", rc2, rc, strerror(rc)));
        }
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
    if (!g_vboxrawOpts.fRW)
        rc = -EPERM;
    else if ((off_t)(offset + cbBuf) < offset)
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
        {
                rc2 = VDWrite(g_pVDisk, offset, pbBuf, cbBuf);
                if (RT_FAILURE(rc2))
                {
                    rc = -RTErrConvertToErrno(rc2);
                    LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                    return rc;
                }
        }
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
                    if (RT_FAILURE(rc2))
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
                    }
                }
                else
                {
                    rc = -RTErrConvertToErrno(rc2);
                    LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                    return rc;
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
                        if (RT_FAILURE(rc2))
                        {
                            rc = -RTErrConvertToErrno(rc2);
                            LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                            return rc;
                        }
                    }
                    else
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
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
                    if (RT_FAILURE(rc2))
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
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
                        if (RT_FAILURE(rc2))
                        {
                            rc = -RTErrConvertToErrno(rc2);
                            LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                            return rc;
                        }
                    }
                    else
                    {
                        rc = -RTErrConvertToErrno(rc2);
                        LogFlowFunc(("error: rc2=%d, rc=%d, %s\n", rc2, rc, strerror(rc)));
                        return rc;
                    }
                }
            }
        }

        /* convert the return code */
        if (RT_SUCCESS(rc2))
            rc = cbBuf;
        else
        {
            rc = -RTErrConvertToErrno(rc2);
            LogFlowFunc(("Error rc2=%d, rc=%d=%s\n", rc2, rc, strerror(rc)));
            return rc;
        }
    }
    return rc;
}

/** @copydoc fuse_operations::getattr */
static int
vboxrawOp_getattr(const char *pszPath, struct stat *stbuf)
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

        stbuf->st_size = VDGetSize(g_pVDisk, 0);
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
        stbuf->st_uid = 0;
        stbuf->st_gid = 0;
    } else
        rc = -ENOENT;

    return rc;
}

/** @copydoc fuse_operations::readdir */
static int
vboxrawOp_readdir(const char *pszPath, void *pvBuf, fuse_fill_dir_t pfnFiller,
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
static int
vboxrawOp_readlink(const char *pszPath, char *buf, size_t size)
{
    (void) pszPath;
    RTStrCopy(buf, size, g_pszBaseImagePath);
    return 0;
}

static void
listMedia(IMachine *pMachine)
{
    int rc = 0;
    com::SafeIfaceArray<IMediumAttachment> pMediumAttachments;

    CHECK_ERROR(pMachine, COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(pMediumAttachments)));
    for (size_t i = 0; i < pMediumAttachments.size(); i++)
    {
        ComPtr<IMedium> pMedium;
        DeviceType_T deviceType;
        Bstr pMediumUuid;
        Bstr pMediumName;
        Bstr pMediumPath;

        CHECK_ERROR(pMediumAttachments[i], COMGETTER(Type)(&deviceType));

        if (deviceType == DeviceType_HardDisk)
        {
            CHECK_ERROR(pMediumAttachments[i], COMGETTER(Medium)(pMedium.asOutParam()));
            if (pMedium.isNull())
                return;

            MediumState_T state;
            CHECK_ERROR(pMedium, COMGETTER(State)(&state));
            if (FAILED(rc))
                return;
            if (state == MediumState_Inaccessible)
            {
                CHECK_ERROR(pMedium, RefreshState(&state));
                if (FAILED(rc))
                return;
            }

            ComPtr<IMedium> pEarliestAncestor;
            CHECK_ERROR(pMedium, COMGETTER(Base)(pEarliestAncestor.asOutParam()));
            ComPtr<IMedium> pChild = pEarliestAncestor;
            uint32_t ancestorNumber = 0;
            if (pEarliestAncestor.isNull())
                return;
            RTPrintf("\n");
            do
            {
                com::SafeIfaceArray<IMedium> aMediumChildren;
                CHECK_ERROR(pChild, COMGETTER(Name)(pMediumName.asOutParam()));
                CHECK_ERROR(pChild, COMGETTER(Id)(pMediumUuid.asOutParam()));
                CHECK_ERROR(pChild, COMGETTER(Location)(pMediumPath.asOutParam()));

                if (ancestorNumber == 0)
                {
                    RTPrintf("   -----------------------\n");
                    RTPrintf("   HDD base:   \"%s\"\n",   CSTR(pMediumName));
                    RTPrintf("   UUID:       %s\n",       CSTR(pMediumUuid));
                    RTPrintf("   Location:   %s\n\n",     CSTR(pMediumPath));
                }
                else
                {
                    RTPrintf("     Diff %d:\n", ancestorNumber);
                    RTPrintf("          UUID:       %s\n",    CSTR(pMediumUuid));
                    RTPrintf("          Location:   %s\n\n",  CSTR(pMediumPath));
                }
                CHECK_ERROR_BREAK(pChild, COMGETTER(Children)(ComSafeArrayAsOutParam(aMediumChildren)));
                pChild = (aMediumChildren.size()) ? aMediumChildren[0] : NULL;
                ++ancestorNumber;
            } while(pChild);
        }
    }
}
/**
 * Display all registered VMs on the screen with some information about each
 *
 * @param virtualBox VirtualBox instance object.
 */
static void
listVMs(IVirtualBox *pVirtualBox)
{
    HRESULT rc = 0;
    com::SafeIfaceArray<IMachine> pMachines;
    CHECK_ERROR(pVirtualBox, COMGETTER(Machines)(ComSafeArrayAsOutParam(pMachines)));
    for (size_t i = 0; i < pMachines.size(); ++i)
    {
        ComPtr<IMachine> pMachine = pMachines[i];
        if (pMachine)
        {
            BOOL fAccessible;
            CHECK_ERROR(pMachines[i], COMGETTER(Accessible)(&fAccessible));
            if (fAccessible)
            {
                Bstr pMachineName;
                Bstr pMachineUuid;
                Bstr pDescription;
                Bstr pMachineLocation;

                CHECK_ERROR(pMachine, COMGETTER(Name)(pMachineName.asOutParam()));
                CHECK_ERROR(pMachine, COMGETTER(Id)(pMachineUuid.asOutParam()));
                CHECK_ERROR(pMachine, COMGETTER(Description)(pDescription.asOutParam()));
                CHECK_ERROR(pMachine, COMGETTER(SettingsFilePath)(pMachineLocation.asOutParam()));

                if (   g_vboxrawOpts.pszVm == NULL
                    || RTStrNCmp(CSTR(pMachineUuid), g_vboxrawOpts.pszVm, MAX_ID_LEN) == 0
                    || RTStrNCmp((const char *)pMachineName.raw(), g_vboxrawOpts.pszVm, MAX_ID_LEN) == 0)
                {
                    RTPrintf("------------------------------------------------------\n");
                    RTPrintf("VM Name:   \"%s\"\n", CSTR(pMachineName));
                    RTPrintf("UUID:      %s\n",     CSTR(pMachineUuid));
                    if (*pDescription.raw() != '\0')
                        RTPrintf("Description:  %s\n",      CSTR(pDescription));
                    RTPrintf("Location:  %s\n",      CSTR(pMachineLocation));

                    listMedia(pMachine);

                    RTPrintf("\n");
                }
            }
        }
    }
}

static void
searchForBaseImage(IVirtualBox *pVirtualBox, char *pszImageString, ComPtr<IMedium> *pBaseImage)
{
    int rc = 0;
    com::SafeIfaceArray<IMedium> aDisks;

    CHECK_ERROR(pVirtualBox, COMGETTER(HardDisks)(ComSafeArrayAsOutParam(aDisks)));
    for (size_t i = 0; i < aDisks.size() && aDisks[i]; i++)
    {
        if (aDisks[i])
        {
            Bstr pMediumUuid;
            Bstr pMediumName;

            CHECK_ERROR(aDisks[i], COMGETTER(Name)(pMediumName.asOutParam()));
            CHECK_ERROR(aDisks[i], COMGETTER(Id)(pMediumUuid.asOutParam()));

            if (   RTStrCmp(pszImageString, CSTR(pMediumUuid)) == 0
                || RTStrCmp(pszImageString, CSTR(pMediumName)) == 0)
            {
                *pBaseImage = aDisks[i];
                return;
            }
        }
    }
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
    if (g_vboxrawOpts.fAllowRoot)
        fuse_opt_add_arg(&args, "-oallow_root");

    if (rc == -1)
        return RTMsgErrorExitFailure("vboxraw: ERROR: Couldn't parse fuse options, rc=%Rrc\n", rc);

    /*
     * Initialize COM.
     */
    using namespace com;
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
    {
# ifdef VBOX_WITH_XPCOM
        if (hrc == NS_ERROR_FILE_ACCESS_DENIED)
        {
            char szHome[RTPATH_MAX] = "";
            com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
            return RTMsgErrorExit(RTEXITCODE_FAILURE,
                   "Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
        }
# endif
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM! (hrc=%Rhrc)", hrc);
    }

    /*
     * Get the remote VirtualBox object and create a local session object.
     */
    ComPtr<IVirtualBoxClient> pVirtualBoxClient;
    ComPtr<IVirtualBox> pVirtualBox;

    hrc = pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (SUCCEEDED(hrc))
        hrc = pVirtualBoxClient->COMGETTER(VirtualBox)(pVirtualBox.asOutParam());
    if (FAILED(hrc))
         return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to get IVirtualBox object! (hrc=%Rhrc)", hrc);

    if (g_vboxrawOpts.fVerbose)
        RTPrintf("vboxraw: VirtualBox XPCOM object created\n");

    if (g_vboxrawOpts.fList)
    {
         listVMs(pVirtualBox);
         return 0;
    }

    if (g_vboxrawOpts.pszImage == NULL)
    {
            RTMsgErrorExitFailure("vboxraw: ERROR: "
                "Must provide at at least --list or --image option\n");
            return 0;
    }
    ComPtr<IMedium> pBaseImageMedium = NULL;
    char    *pszFormat;
    VDTYPE  enmType;

    searchForBaseImage(pVirtualBox, g_vboxrawOpts.pszImage, &pBaseImageMedium);
    if (pBaseImageMedium == NULL)
    {
        /*
         * Try to locate base image pMedium via the VirtualBox API, given the user-provided path
         * resolving symlinks back to hard path.
         */
        int cbNameMax = pathconf(g_vboxrawOpts.pszImage, _PC_PATH_MAX);
        if (cbNameMax < 0)
            return cbNameMax;

        g_pszBaseImagePath = RTStrDup(g_vboxrawOpts.pszImage);
        if (g_pszBaseImagePath == NULL)
            return RTMsgErrorExitFailure("vboxraw: out of memory\n");

        if (access(g_pszBaseImagePath, F_OK) < 0)
            return RTMsgErrorExitFailure("vboxraw: ERROR: "
                    "Virtual disk image not found: \"%s\"\n", g_pszBaseImagePath);

        if (access(g_pszBaseImagePath, R_OK) < 0)
             return RTMsgErrorExitFailure("vboxraw: ERROR: "
                    "Virtual disk image not readable: \"%s\"\n", g_pszBaseImagePath);

        if ( g_vboxrawOpts.fRW && access(g_vboxrawOpts.pszImage, W_OK) < 0)
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
        searchForBaseImage(pVirtualBox, g_pszBaseImageName, &pBaseImageMedium);

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
        com::SafeIfaceArray<IMedium> aMediumChildren;
        ComPtr<IMedium> pChild = pBaseImageMedium;
        uint32_t diffNumber = 0; /* diff # 0 = base image */
        do
        {
            Bstr pMediumName;
            Bstr pMediumPath;

            CHECK_ERROR(pChild, COMGETTER(Name)(pMediumName.asOutParam()));
            CHECK_ERROR(pChild, COMGETTER(Location)(pMediumPath.asOutParam()));

            if (pChild == pBaseImageMedium)
            {
                free((void *)g_pszBaseImageName);
                g_pszBaseImageName = RTStrDup(CSTR(pMediumName));

                free((void *)g_pszBaseImagePath);
                g_pszBaseImagePath = RTStrDup(CSTR(pMediumPath));
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
            if ( g_vboxrawOpts.cHddImageDiffMax != 0 && diffNumber > g_vboxrawOpts.cHddImageDiffMax)
                break;

            if (g_vboxrawOpts.fVerbose)
            {
                if (diffNumber == 0)
                    RTPrintf("\nvboxraw: Opening base image into container:\n       %s\n",
                        g_pszBaseImagePath);
                else
                    RTPrintf("\nvboxraw: Opening difference image #%d into container:\n       %s\n",
                        diffNumber, g_pszBaseImagePath);
            }

            rc = VDOpen(g_pVDisk, pszFormat, g_pszBaseImagePath, 0, NULL /* pVDIfsImage */);
            if (RT_FAILURE(rc))
            {
                VDClose(g_pVDisk, false /* fDeletes */);
                return RTMsgErrorExitFailure("vboxraw: VDOpen(,,%s,,) failed, rc=%Rrc\n",
                   g_pszBaseImagePath, rc);
            }

            CHECK_ERROR(pChild, COMGETTER(Children)(ComSafeArrayAsOutParam(aMediumChildren)));

            if (aMediumChildren.size() != 0) {
                pChild = aMediumChildren[0];
            }

            aMediumChildren.setNull();

            ++diffNumber;


        } while(NS_SUCCEEDED(rc) && aMediumChildren.size());
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

