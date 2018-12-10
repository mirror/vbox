/* $Id$ */
/** @file
 * vboximg-mount - Disk Image Flattening FUSE Program.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
#include <math.h>
//#include <stdarg.h>
#include <cstdarg>
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
#include <VBox/vd-ifs.h>
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
#include <iprt/assert.h>
#include <iprt/message.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/types.h>
#include <iprt/path.h>
#include <iprt/utf16.h>

#include "vboximg-mount.h"
#include "SelfSizingTable.h"

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

#if 0 /* unused */
const uint64_t KB = 1024;
const uint64_t MB = KB * KB;
const uint64_t GB = MB * KB;
const uint64_t TB = GB * KB;
const uint64_t PB = TB * KB;
#endif

enum { PARTITION_TABLE_MBR = 1, PARTITION_TABLE_GPT = 2 };

#define GPT_PTABLE_SIZE             32 * BLOCKSIZE   /** Max size we to read for GPT partition table */
#define MBR_PARTITIONS_MAX          4                /** Fixed number of partitions in Master Boot Record */
#define BASENAME_MAX                256              /** Maximum name for the basename of a path (for RTStrNLen()*/
#define VBOXIMG_PARTITION_MAX       256              /** How much storage to allocate to store partition info */
#define PARTITION_NAME_MAX          72               /** Maximum partition name size (accomodates GPT partition name) */
#define BLOCKSIZE                   512              /** Commonly used disk block size */
#define DOS_BOOT_RECORD_SIGNATURE   0xaa55           /** MBR and EBR (partition table) signature [EOT boundary] */
#define NULL_BOOT_RECORD_SIGNATURE  0x0000           /** MBR or EBR null signature value */
#define MAX_UUID_LEN                256              /** Max length of a UUID */
#define LBA(n)                      (n * BLOCKSIZE)
#define VD_SECTOR_SIZE              512              /** Virtual disk sector/blocksize */
#define VD_SECTOR_MASK              (VD_SECTOR_SIZE - 1)    /** Masks off a blocks worth of data */
#define VD_SECTOR_OUT_OF_BOUNDS_MASK  (~UINT64_C(VD_SECTOR_MASK))         /** Masks the overflow of a blocks worth of data */

#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)

#define PARTTYPE_IS_NULL(parType) ((uint8_t)parType == 0x00)
#define PARTTYPE_IS_GPT(parType)  ((uint8_t)parType == 0xee)
#define PARTTYPE_IS_EXT(parType)  ((    (uint8_t)parType) == 0x05  /* Extended           */ \
                                    || ((uint8_t)parType) == 0x0f  /* W95 Extended (LBA) */ \
                                    || ((uint8_t)parType) == 0x85) /* Linux Extended     */

#define SAFENULL(strPtr)   (strPtr ? strPtr : "")
#define CSTR(arg)     Utf8Str(arg).c_str()          /* Converts XPCOM string type to C string type */

static struct fuse_operations g_vboximgOps;         /** FUSE structure that defines allowed ops for this FS */

/* Global variables */

static PVDISK                g_pVDisk;              /** Handle for Virtual Disk in contet */
static char                 *g_pvDiskUuid;          /** UUID of image (if known, otherwise NULL) */
static off_t                 g_vDiskOffset;         /** Biases r/w from start of VD */
static off_t                 g_vDiskSize;           /** Limits r/w length for VD */
static int32_t               g_cReaders;            /** Number of readers for VD */
static int32_t               g_cWriters;            /** Number of writers for VD */
static RTFOFF                g_cbEntireVDisk;       /** Size of VD */
static char                 *g_pszBaseImageName;    /** Base filename for current VD image */
static char                 *g_pszBaseImagePath;    /** Full path to current VD image */
static PVDINTERFACE          g_pVdIfs;              /** @todo Remove when VD I/O becomes threadsafe */
static VDINTERFACETHREADSYNC g_VDIfThreadSync;      /** @todo Remove when VD I/O becomes threadsafe */
static RTCRITSECT            g_vdioLock;            /** @todo Remove when VD I/O becomes threadsafe */
static uint16_t              g_lastPartNbr;         /** Last partition number found in MBR + EBR chain */
static bool                  g_fGPT;                /** True if GPT type partition table was found */

/* Table entry containing partition info parsed out of GPT or MBR and EBR chain of specified VD */

typedef struct
{
    int            idxPartition;            /** partition number */
    char           *pszName;
    off_t          offPartition;            /** partition offset from start of disk, in bytes */
    uint64_t       cbPartition;             /** partition size in bytes */
    uint8_t        fBootable;               /** partition bootable */
    union
    {
        uint8_t    legacy;                  /** partition type MBR/EBR */
        uint128_t  gptGuidTypeSpecifier;    /** partition type GPT */
    } partitionType;                        /** uint8_t for MBR/EBR (legacy) and GUID for GPT */
    union
    {
        MBRPARTITIONENTRY mbrEntry;         /** MBR (also EBR partition entry) */
        GPTPARTITIONENTRY gptEntry;         /** GPT partition entry */
    } partitionEntry;
} PARTITIONINFO;

PARTITIONINFO g_aParsedPartitionInfo[VBOXIMG_PARTITION_MAX + 1]; /* Note: Element 0 reserved for EntireDisk partitionEntry */

static struct vboximgOpts {
     char         *pszVm;                   /** optional VM UUID */
     char         *pszImage;                /** Virtual Disk image UUID or path */
     int32_t       idxPartition;            /** Number of partition to constrain FUSE based FS to (optional) 0 - whole disk*/
     int32_t       offset;                  /** Offset to base virtual disk reads and writes from (altnerative to partition) */
     int32_t       size;                    /** Size of accessible disk region, starting at offset, default = offset 0 */
     uint32_t      cHddImageDiffMax;        /** Max number of differencing images (snapshots) to apply to image */
     uint32_t      fListMediaLong;          /** Flag to list virtual disks of all known VMs */
     uint32_t      fList;                   /** Flag to list virtual disks of all known VMs */
     uint32_t      fListParts;              /** Flag to summarily list partitions associated with pszImage */
     uint32_t      fAllowRoot;              /** Flag to allow root to access this FUSE FS */
     uint32_t      fRW;                     /** Flag to allow changes to FUSE-mounted Virtual Disk image */
     uint32_t      fBriefUsage;             /** Flag to display only FS-specific program usage options */
     uint32_t      fVerbose;                /** Add more info to lists and operations */
} g_vboximgOpts;

#define OPTION(fmt, pos, val) { fmt, offsetof(struct vboximgOpts, pos), val }

static struct fuse_opt vboximgOptDefs[] = {
    OPTION("-l",              fList,             1),
    OPTION("--list",          fList,             1),
    OPTION("--root",          fAllowRoot,        1),
    OPTION("--vm=%s",         pszVm,             0),
    OPTION("--maxdiff=%d",    cHddImageDiffMax,  1),
    OPTION("--diff=%d",       cHddImageDiffMax,  1),
    OPTION("--partition=%d",  idxPartition,      1),
    OPTION("-p %d",           idxPartition,      1),
    OPTION("--offset=%d",     offset,            1),
    OPTION("-o %d",           offset,            1),
    OPTION("--size=%d",       size,              1),
    OPTION("-s %d",           size,              1),
    OPTION("--image=%s",      pszImage,          0),
    OPTION("-i %s",           pszImage,          0),
    OPTION("--rw",            fRW,               1),
    OPTION("--verbose",       fVerbose,          1),
    OPTION("-v",              fVerbose,          1),
    OPTION("-h",              fBriefUsage,       1),
    FUSE_OPT_KEY("--help",    USAGE_FLAG),
    FUSE_OPT_KEY("-vm",       FUSE_OPT_KEY_NONOPT),
    FUSE_OPT_END
};

static void
briefUsage()
{
    RTPrintf("usage: vboximg-mount [options] <mountpoint>\n\n"
        "vboximg-mount options:\n\n"
        "  [ { -i | --image= } <specifier> ]  VirtualBox disk image (UUID, name or path)\n"
        "\n"
        "  [ { -l | --list } ]                If image specified, list its partitions, \n"
        "                                     otherwise, list registered VMs and\n"
        "                                     associated disks. In verbose mode,\n"
        "                                     VM/media list will be long format, i.e.\n"
        "                                     including snapshot images and file paths.\n"
        "\n"
        "  [ --vm=UUID ]                      Restrict media list to specified vm.\n"
        "\n"
        "  [ { -p | --partition= } <part #> ] Expose only specified partition via FUSE.\n"
        "\n"
        "  [ { -o | --offset= } <byte #> ]    Bias disk I/O by offset from disk start.\n"
        "                                     (incompatible with -p, --partition)\n"
        "\n"
        "  [ { -s | --size=<bytes> } ]        Specify size of mounted disk.\n"
        "                                     (incompatible with -p, --partition)\n"
        "\n"
        "  [ { --diff=<diff #> } ]            Limits default operation (of applying all\n"
        "                                     snapshots of virtual disk) to specified\n"
        "                                     disk differencing image #. Diffs will\n"
        "                                     be merged-in up to and including diff #.\n"
        "                                     (default: All diffs, 0 = No diffs)\n"
        "\n"
        "  [ --rw ]                           Make image writeable (default = readonly)\n"
        "  [ --root ]                         Same as -o allow_root.\n"
        "\n"
        "  [ { -v | --verbose }]              Log extra information.\n"
        "\n"
        "  [ -o opt[,opt...]]                 FUSE mount options.\n"
        "\n"
        "  [ { -h | -? } ]                    Display short usage info (no FUSE options).\n"
        "  [ --help ]                         Display long usage info (incl. FUSE opts).\n\n"
    );
    RTPrintf("\n");
    RTPrintf("When successful, the --image option instantiates a one-directory-deep FUSE\n");
    RTPrintf("filesystem rooted at the specified mountpoint.  Its contents are a\n");
    RTPrintf("symbolic link named as basename of the image path, pointing to full path of\n");
    RTPrintf("the virtual disk image. Also a regular file named 'vhdd', which is a device\n");
    RTPrintf("node through which a raw byte stream of the disk image (as synthesized by\n");
    RTPrintf("the VirtualBox runtime engine) can be accessed. It is the vhdd file that the\n");
    RTPrintf("user or a utility can subsequently mount on the host OS.\n");
}

static int
vboximgOptHandler(void *data, const char *arg, int optKey, struct fuse_args *outargs)
{
    (void) data;
    (void) arg;
    (void) optKey;
    (void) outargs;
    /*
     * Apparently this handler is only called for arguments FUSE can't parse,
     * and arguments that don't result in variable assignment such as "USAGE"
     * In this impl. that's always deemed a parsing error.
     */
    if (*arg != '-') /* could be user's mount point */
        return 1;

    return -1;
}

/** @copydoc fuse_operations::open */
static int vboximgOp_open(const char *pszPath, struct fuse_file_info *pInfo)
{
    RT_NOREF(pszPath, pInfo);
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

/** @todo Remove when VD I/O becomes threadsafe */
static DECLCALLBACK(int) vboximgThreadStartRead(void *pvUser)
{
    PRTCRITSECT vdioLock = (PRTCRITSECT)pvUser;
    return RTCritSectEnter(vdioLock);
}

static DECLCALLBACK(int) vboximgThreadFinishRead(void *pvUser)
{
    PRTCRITSECT vdioLock = (PRTCRITSECT)pvUser;
    return RTCritSectLeave(vdioLock);
}

static DECLCALLBACK(int) vboximgThreadStartWrite(void *pvUser)
{
    PRTCRITSECT vdioLock = (PRTCRITSECT)pvUser;
    return RTCritSectEnter(vdioLock);
}

static DECLCALLBACK(int) vboximgThreadFinishWrite(void *pvUser)
{
    PRTCRITSECT vdioLock = (PRTCRITSECT)pvUser;
    return RTCritSectLeave(vdioLock);
}
/** @todo (end of to do section) */

/** @copydoc fuse_operations::release */
static int vboximgOp_release(const char *pszPath, struct fuse_file_info *pInfo)
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

/**
 * VD read Sanitizer taking care of unaligned accesses.
 *
 * @return  VBox bootIndicator code.
 * @param   pDisk    VD disk container.
 * @param   off      Offset to start reading from.
 * @param   pvBuf    Pointer to the buffer to read into.
 * @param   cbRead   Amount of bytes to read.
 */
static int vdReadSanitizer(PVDISK pDisk, uint64_t off, void *pvBuf, size_t cbRead)
{
    int rc;

    uint64_t const cbMisalignmentOfStart = off & VD_SECTOR_MASK;
    uint64_t const cbMisalignmentOfEnd  = (off + cbRead) & VD_SECTOR_MASK;

    if (cbMisalignmentOfStart + cbMisalignmentOfEnd == 0) /* perfectly aligned request; just read it and done */
        rc = VDRead(pDisk, off, pvBuf, cbRead);
    else
    {
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        uint8_t abBuf[VD_SECTOR_SIZE];

        /* If offset not @ sector boundary, read whole sector, then copy unaligned
         * bytes (requested by user), only up to sector boundary, into user's buffer
         */
        if (cbMisalignmentOfStart)
        {
            rc = VDRead(pDisk, off - cbMisalignmentOfStart, abBuf, VD_SECTOR_SIZE);
            if (RT_SUCCESS(rc))
            {
                size_t const cbPartial = RT_MIN(VD_SECTOR_SIZE - cbMisalignmentOfStart, cbRead);
                memcpy(pbBuf, &abBuf[cbMisalignmentOfStart], cbPartial);
                pbBuf  += cbPartial;
                off    += cbPartial; /* Beginning of next sector or EOD */
                cbRead -= cbPartial; /* # left to read */
            }
        }
        else /* user's offset already aligned, did nothing */
            rc = VINF_SUCCESS;

        /* Read remaining aligned sectors, deferring any tail-skewed bytes */
        if (RT_SUCCESS(rc) && cbRead >= VD_SECTOR_SIZE)
        {
            Assert(!(off % VD_SECTOR_SIZE));

            size_t cbPartial = cbRead - cbMisalignmentOfEnd;
            Assert(!(cbPartial % VD_SECTOR_SIZE));
            rc = VDRead(pDisk, off, pbBuf, cbPartial);
            if (RT_SUCCESS(rc))
            {
                pbBuf  += cbPartial;
                off    += cbPartial;
                cbRead -= cbPartial;
            }
        }

        /* Unaligned buffered read of tail. */
        if (RT_SUCCESS(rc) && cbRead)
        {
            Assert(cbRead == cbMisalignmentOfEnd);
            Assert(cbRead < VD_SECTOR_SIZE);
            Assert(!(off % VD_SECTOR_SIZE));

            rc = VDRead(pDisk, off, abBuf, VD_SECTOR_SIZE);
            if (RT_SUCCESS(rc))
                memcpy(pbBuf, abBuf, cbRead);
        }
    }

    if (RT_FAILURE(rc))
    {
        int sysrc = -RTErrConvertToErrno(rc);
        LogFlowFunc(("error: %s (vbox err: %d)\n", strerror(sysrc), rc));
        rc = sysrc;
    }
    return cbRead;
}

/**
 * VD write Sanitizer taking care of unaligned accesses.
 *
 * @return  VBox bootIndicator code.
 * @param   pDisk    VD disk container.
 * @param   off      Offset to start writing to.
 * @param   pvSrc    Pointer to the buffer to read from.
 * @param   cbWrite  Amount of bytes to write.
 */
static int vdWriteSanitizer(PVDISK pDisk, uint64_t off, const void *pvSrc, size_t cbWrite)
{
    uint8_t const *pbSrc = (uint8_t const *)pvSrc;
    uint8_t        abBuf[4096];
    int rc;
    int cbRemaining = cbWrite;

    /*
     * Take direct route if the request is sector aligned.
     */
    uint64_t const cbMisalignmentOfStart = off & VD_SECTOR_MASK;
    size_t   const cbMisalignmentOfEnd  = (off + cbWrite) & VD_SECTOR_MASK;
    if (!cbMisalignmentOfStart && !cbMisalignmentOfEnd)
    {
          rc = VDWrite(pDisk, off, pbSrc, cbWrite);
          do
            {
                size_t cbThisWrite = RT_MIN(cbWrite, sizeof(abBuf));
                rc = VDWrite(pDisk, off, memcpy(abBuf, pbSrc, cbThisWrite), cbThisWrite);
                if (RT_SUCCESS(rc))
                {
                    pbSrc   += cbThisWrite;
                    off     += cbThisWrite;
                    cbRemaining -= cbThisWrite;
                }
                else
                    break;
            } while (cbRemaining > 0);
    }
    else
    {
        /*
         * Unaligned buffered read+write of head.  Aligns the offset.
         */
        if (cbMisalignmentOfStart)
        {
            rc = VDRead(pDisk, off - cbMisalignmentOfStart, abBuf, VD_SECTOR_SIZE);
            if (RT_SUCCESS(rc))
            {
                size_t const cbPartial = RT_MIN(VD_SECTOR_SIZE - cbMisalignmentOfStart, cbWrite);
                memcpy(&abBuf[cbMisalignmentOfStart], pbSrc, cbPartial);
                rc = VDWrite(pDisk, off - cbMisalignmentOfStart, abBuf, VD_SECTOR_SIZE);
                if (RT_SUCCESS(rc))
                {
                    pbSrc   += cbPartial;
                    off     += cbPartial;
                    cbRemaining -= cbPartial;
                }
            }
        }
        else
            rc = VINF_SUCCESS;

        /*
         * Aligned direct write.
         */
        if (RT_SUCCESS(rc) && cbWrite >= VD_SECTOR_SIZE)
        {
            Assert(!(off % VD_SECTOR_SIZE));
            size_t cbPartial = cbWrite - cbMisalignmentOfEnd;
            Assert(!(cbPartial % VD_SECTOR_SIZE));
            rc = VDWrite(pDisk, off, pbSrc, cbPartial);
            if (RT_SUCCESS(rc))
            {
                pbSrc   += cbPartial;
                off     += cbPartial;
                cbRemaining -= cbPartial;
            }
        }

        /*
         * Unaligned buffered read + write of tail.
         */
        if (   RT_SUCCESS(rc) && cbWrite > 0)
        {
            Assert(cbWrite == cbMisalignmentOfEnd);
            Assert(cbWrite < VD_SECTOR_SIZE);
            Assert(!(off % VD_SECTOR_SIZE));
            rc = VDRead(pDisk, off, abBuf, VD_SECTOR_SIZE);
            if (RT_SUCCESS(rc))
            {
                memcpy(abBuf, pbSrc, cbWrite);
                rc = VDWrite(pDisk, off, abBuf, VD_SECTOR_SIZE);
            }
        }
    }
    if (RT_FAILURE(rc))
    {
        int sysrc = -RTErrConvertToErrno(rc);
        LogFlowFunc(("error: %s (vbox err: %d)\n", strerror(sysrc), rc));
        return sysrc;
    }
    return cbWrite - cbRemaining;
}


/** @copydoc fuse_operations::read */
static int vboximgOp_read(const char *pszPath, char *pbBuf, size_t cbBuf,
                           off_t offset, struct fuse_file_info *pInfo)
{
    (void) pszPath;
    (void) pInfo;

    LogFlowFunc(("my offset=%#llx size=%#zx path=\"%s\"\n", (uint64_t)offset, cbBuf, pszPath));

    AssertReturn(offset >= 0, -EINVAL);
    AssertReturn((int)cbBuf >= 0, -EINVAL);
    AssertReturn((unsigned)cbBuf == cbBuf, -EINVAL);

    AssertReturn(offset + g_vDiskOffset >= 0, -EINVAL);
    int64_t adjOff = offset + g_vDiskOffset;

    int rc = 0;
    if ((off_t)(adjOff + cbBuf) < adjOff)
        rc = -EINVAL;
    else if (adjOff >= g_vDiskSize)
        return 0;
    else if (!cbBuf)
        return 0;

    if (rc >= 0)
        rc = vdReadSanitizer(g_pVDisk, adjOff, pbBuf, cbBuf);
    if (rc < 0)
        LogFlowFunc(("%s\n", strerror(rc)));
    return rc;
}

/** @copydoc fuse_operations::write */
static int vboximgOp_write(const char *pszPath, const char *pbBuf, size_t cbBuf,
                           off_t offset, struct fuse_file_info *pInfo)
{
    (void) pszPath;
    (void) pInfo;

    LogFlowFunc(("offset=%#llx size=%#zx path=\"%s\"\n", (uint64_t)offset, cbBuf, pszPath));

    AssertReturn(offset >= 0, -EINVAL);
    AssertReturn((int)cbBuf >= 0, -EINVAL);
    AssertReturn((unsigned)cbBuf == cbBuf, -EINVAL);
    AssertReturn(offset + g_vDiskOffset >= 0, -EINVAL);
    int64_t adjOff = offset + g_vDiskOffset;

    int rc = 0;
    if (!g_vboximgOpts.fRW) {
        LogFlowFunc(("WARNING: vboximg-mount (FUSE FS) --rw option not specified\n"
                     "              (write operation ignored w/o error!)\n"));
        return cbBuf;
    } else if ((off_t)(adjOff + cbBuf) < adjOff)
        rc = -EINVAL;
    else if (offset >= g_vDiskSize)
        return 0;
    else if (!cbBuf)
        return 0;

    if (rc >= 0)
        rc = vdWriteSanitizer(g_pVDisk, adjOff, pbBuf, cbBuf);
    if (rc < 0)
        LogFlowFunc(("%s\n", strerror(rc)));

    return rc;
}

/** @copydoc fuse_operations::getattr */
static int
vboximgOp_getattr(const char *pszPath, struct stat *stbuf)
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
        /*
         * st_size represents the size of the FUSE FS-mounted portion of the disk.
         * By default it is the whole disk, but can be a partition or specified
         * (or overridden) directly by the { -s | --size } option on the command line.
         */
        stbuf->st_size = g_vDiskSize;
        stbuf->st_nlink = 1;
    }
    else if (RTStrNCmp(pszPath + 1, g_pszBaseImageName, strlen(g_pszBaseImageName)) == 0)
    {
        /* When the disk is partitioned, the symbolic link named from `basename` of
         * resolved path to VBox disk image, has appended to it formatted text
         * representing the offset range of the partition.
         *
         *  $ vboximg-mount -i /stroll/along/the/path/simple_fixed_disk.vdi -p 1 /mnt/tmpdir
         *  $ ls /mnt/tmpdir
         *  simple_fixed_disk.vdi[20480:2013244928]    vhdd
         */
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
vboximgOp_readdir(const char *pszPath, void *pvBuf, fuse_fill_dir_t pfnFiller,
                              off_t offset, struct fuse_file_info *pInfo)

{
    (void) offset;
    (void) pInfo;

    if (RTStrCmp(pszPath, "/") != 0)
        return -ENOENT;

    /*
     *  mandatory '.', '..', ...
     */
    pfnFiller(pvBuf, ".", NULL, 0);
    pfnFiller(pvBuf, "..", NULL, 0);

    /*
     * Create FUSE FS dir entry that is depicted here (and exposed via stat()) as
     * a symbolic link back to the resolved path to the VBox virtual disk image,
     * whose symlink name is basename that path. This is a convenience so anyone
     * listing the dir can figure out easily what the vhdd FUSE node entry
     * represents.
     */

    if (g_vDiskOffset == 0 && (g_vDiskSize == 0 || g_vDiskSize == g_cbEntireVDisk))
        pfnFiller(pvBuf, g_pszBaseImageName, NULL, 0);
    else
    {
        char tmp[BASENAME_MAX];
        RTStrPrintf(tmp, sizeof (tmp), "%s[%d:%d]", g_pszBaseImageName, g_vDiskOffset, g_vDiskSize);
        pfnFiller(pvBuf, tmp, NULL, 0);
    }
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
vboximgOp_readlink(const char *pszPath, char *buf, size_t size)
{
    (void) pszPath;
    RTStrCopy(buf, size, g_pszBaseImagePath);
    return 0;
}

static void
listMedia(IMachine *pMachine, char *vmName, char *vmUuid)
{
    int rc = 0;
    com::SafeIfaceArray<IMediumAttachment> pMediumAttachments;

    CHECK_ERROR(pMachine, COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(pMediumAttachments)));
    int firstIteration = 1;
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
            do
            {
                com::SafeIfaceArray<IMedium> aMediumChildren;
                CHECK_ERROR(pChild, COMGETTER(Name)(pMediumName.asOutParam()));
                CHECK_ERROR(pChild, COMGETTER(Id)(pMediumUuid.asOutParam()));
                CHECK_ERROR(pChild, COMGETTER(Location)(pMediumPath.asOutParam()));

                if (ancestorNumber == 0)
                {
                    if (g_vboximgOpts.fVerbose)
                    {
                        RTPrintf("   -----------------------\n");
                        RTPrintf("   HDD base:   \"%s\"\n",   CSTR(pMediumName));
                        RTPrintf("   UUID:       %s\n",       CSTR(pMediumUuid));
                        RTPrintf("   Location:   %s\n\n",     CSTR(pMediumPath));
                    }
                    else
                    {
                        if (firstIteration)
                            RTPrintf("\nVM:    %s " ANSI_BOLD "%-20s" ANSI_RESET "\n",
                                vmUuid, vmName);
                        RTPrintf("  img: %s " ANSI_BOLD "  %s" ANSI_RESET "\n",
                            CSTR(pMediumUuid), CSTR(pMediumName));
                    }
                }
                else
                {
                    if (g_vboximgOpts.fVerbose)
                    {
                        RTPrintf("     Diff %d:\n", ancestorNumber);
                        RTPrintf("          UUID:       %s\n",    CSTR(pMediumUuid));
                        RTPrintf("          Location:   %s\n",  CSTR(pMediumPath));
                    }
                }
                CHECK_ERROR_BREAK(pChild, COMGETTER(Children)(ComSafeArrayAsOutParam(aMediumChildren)));
                pChild = (aMediumChildren.size()) ? aMediumChildren[0] : NULL;
                ++ancestorNumber;
                firstIteration = 0;
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


                if (   g_vboximgOpts.pszVm == NULL
                    || RTStrNCmp(CSTR(pMachineUuid), g_vboximgOpts.pszVm, MAX_UUID_LEN) == 0
                    || RTStrNCmp((const char *)pMachineName.raw(), g_vboximgOpts.pszVm, MAX_UUID_LEN) == 0)
                {
                    if (g_vboximgOpts.fVerbose)
                    {
                        RTPrintf("------------------------------------------------------\n");
                        RTPrintf("VM Name:   \"%s\"\n", CSTR(pMachineName));
                        RTPrintf("UUID:      %s\n",     CSTR(pMachineUuid));
                        if (*pDescription.raw() != '\0')
                            RTPrintf("Description:  %s\n",      CSTR(pDescription));
                        RTPrintf("Location:  %s\n",      CSTR(pMachineLocation));
                    }
                    listMedia(pMachine, RTStrDup(CSTR(pMachineName)), RTStrDup(CSTR(pMachineUuid)));
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

uint8_t
parsePartitionTable(void)
{
    MBR_t mbr;
    EBR_t ebr;
    PTH_t parTblHdr;

    ASSERT(sizeof (mbr) == 512);
    ASSERT(sizeof (ebr) == 512);
    /*
     * First entry describes entire disk as a single entity
     */
    g_aParsedPartitionInfo[0].idxPartition = 0;
    g_aParsedPartitionInfo[0].offPartition = 0;
    g_aParsedPartitionInfo[0].cbPartition = VDGetSize(g_pVDisk, 0);
    g_aParsedPartitionInfo[0].pszName = RTStrDup("EntireDisk");

    /*
     * Currently only DOS partitioned disks are supported. Ensure this one conforms
     */
    int rc = vdReadSanitizer(g_pVDisk, 0, &mbr, sizeof (mbr));
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("Error reading MBR block from disk\n");

    if (mbr.signature == NULL_BOOT_RECORD_SIGNATURE)
        return RTMsgErrorExitFailure("Unprt disk (null MBR signature)\n");

    if (mbr.signature != DOS_BOOT_RECORD_SIGNATURE)
        return RTMsgErrorExitFailure("Invalid MBR found on image with signature 0x%04hX\n",
            mbr.signature);
    /*
     * Parse the four physical partition entires in the MBR (any one, and only one, can be an EBR)
     */
    int idxEbrPartitionInMbr = 0;
    for (int idxPartition = 1;
             idxPartition <= MBR_PARTITIONS_MAX;
             idxPartition++)
    {
        MBRPARTITIONENTRY *pMbrPartitionEntry =
            &g_aParsedPartitionInfo[idxPartition].partitionEntry.mbrEntry;;
        memcpy (pMbrPartitionEntry, &mbr.partitionEntry[idxPartition - 1], sizeof (MBRPARTITIONENTRY));

        if (PARTTYPE_IS_NULL(pMbrPartitionEntry->type))
            continue;

        if (PARTTYPE_IS_EXT(pMbrPartitionEntry->type))
        {
            if (idxEbrPartitionInMbr)
                 return RTMsgErrorExitFailure("Multiple EBRs found found in MBR\n");
            idxEbrPartitionInMbr = idxPartition;
        }

        PARTITIONINFO *ppi = &g_aParsedPartitionInfo[idxPartition];

        ppi->idxPartition = idxPartition;
        ppi->offPartition = (off_t) pMbrPartitionEntry->partitionLba * BLOCKSIZE;
        ppi->cbPartition  = (off_t) pMbrPartitionEntry->partitionBlkCnt * BLOCKSIZE;
        ppi->fBootable    = pMbrPartitionEntry->bootIndicator == 0x80;
        ppi->partitionType.legacy = pMbrPartitionEntry->type;

        g_lastPartNbr = idxPartition;

        if (PARTTYPE_IS_GPT(pMbrPartitionEntry->type))
        {
            g_fGPT = true;
            break;
        }
    }

    if (g_fGPT)
    {
        g_lastPartNbr = 2;  /* from the 'protective MBR' */

        rc = vdReadSanitizer(g_pVDisk, LBA(1), &parTblHdr, sizeof (parTblHdr));
        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("Error reading Partition Table Header (LBA 1) from disk\n");

        uint8_t *pTblBuf = (uint8_t *)RTMemAlloc(GPT_PTABLE_SIZE);

        RTPrintf( "Virtual disk image:\n\n");
        RTPrintf("   Path: %s\n", g_pszBaseImagePath);
        if (g_pvDiskUuid)
            RTPrintf("   UUID: %s\n\n", g_pvDiskUuid);

        if (g_vboximgOpts.fVerbose)
        {
            RTPrintf("   GPT Partition Table Header:\n\n");
            if (RTStrCmp((const char *)&parTblHdr.signature, "EFI PART") == 0)
                RTPrintf(
                     "      Signature               \"EFI PART\" (0x%llx)\n", parTblHdr.signature);
            else
                RTPrintf(
                     "      Signature:              0x%llx\n",  parTblHdr.signature);
            RTPrintf("      Revision:               %-8.8x\n",  parTblHdr.revision);
            RTPrintf("      Current LBA:            %lld\n",    parTblHdr.headerLba);
            RTPrintf("      Backup LBA:             %lld\n",    parTblHdr.backupLba);
            RTPrintf("      Partition entries LBA:  %lld\n",    parTblHdr.partitionEntriesLba);
            RTPrintf("      # of partitions:        %d\n",      parTblHdr.cPartitionEntries);
            RTPrintf("      size of entry:          %d\n\n",    parTblHdr.cbPartitionEntry);
        }

        if (!pTblBuf)
            return RTMsgErrorExitFailure("Out of memory\n");

        rc = vdReadSanitizer(g_pVDisk, LBA(2), pTblBuf, GPT_PTABLE_SIZE);
        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("Error reading Partition Table blocks from disk\n");

        uint32_t cEntries = parTblHdr.cPartitionEntries;
        uint32_t cbEntry  = parTblHdr.cbPartitionEntry;
        if (cEntries * cbEntry > GPT_PTABLE_SIZE)
        {
            RTPrintf("Partition entries exceed GPT table read from disk (pruning!)\n");
            while (cEntries * cbEntry > GPT_PTABLE_SIZE && cEntries > 0)
                --cEntries;
        }
        uint8_t *pEntryRaw = pTblBuf;
        for (uint32_t i = 0; i < cEntries; i++)
        {
            GPTPARTITIONENTRY *pEntry = (GPTPARTITIONENTRY *)pEntryRaw;
            PARTITIONINFO *ppi = &g_aParsedPartitionInfo[g_lastPartNbr];
            memcpy(&(ppi->partitionEntry).gptEntry, pEntry, sizeof(GPTPARTITIONENTRY));
            if (!pEntry->firstLba)
                break;
            ppi->offPartition = pEntry->firstLba * BLOCKSIZE;
            ppi->cbPartition = (pEntry->lastLba - pEntry->firstLba) * BLOCKSIZE;
            ppi->fBootable = pEntry->attrFlags & (1 << GPT_LEGACY_BIOS_BOOTABLE);
            ppi->partitionType.gptGuidTypeSpecifier = pEntry->partitionTypeGuid;
            size_t cwName = sizeof (pEntry->partitionName) / 2;
            RTUtf16LittleToUtf8Ex((PRTUTF16)pEntry->partitionName, RTSTR_MAX, &ppi->pszName, cwName, NULL);
            ppi->idxPartition = g_lastPartNbr++;
            pEntryRaw += cbEntry;
        }
        return PARTITION_TABLE_GPT;
    }

    /*
     * Starting with EBR located in MBR, walk EBR chain to parse the logical partition entries
     */
    if (idxEbrPartitionInMbr)
    {
        uint32_t firstEbrLba
            = g_aParsedPartitionInfo[idxEbrPartitionInMbr].partitionEntry.mbrEntry.partitionLba;
        off_t    firstEbrOffset   = (off_t)firstEbrLba * BLOCKSIZE;
        off_t    chainedEbrOffset = 0;

        if (!firstEbrLba)
            return RTMsgErrorExitFailure("Inconsistency for logical partition start. Aborting\n");

        for (int idxPartition = 5;
                 idxPartition <= VBOXIMG_PARTITION_MAX;
                 idxPartition++)
        {

            off_t currentEbrOffset = firstEbrOffset + chainedEbrOffset;
            vdReadSanitizer(g_pVDisk, currentEbrOffset, &ebr, sizeof (ebr));

            if (ebr.signature != DOS_BOOT_RECORD_SIGNATURE)
                return RTMsgErrorExitFailure("Invalid EBR found on image with signature 0x%04hX\n",
                    ebr.signature);

            MBRPARTITIONENTRY *pEbrPartitionEntry =
                &g_aParsedPartitionInfo[idxPartition].partitionEntry.mbrEntry; /* EBR entry struct same as MBR */
            memcpy(pEbrPartitionEntry, &ebr.partitionEntry, sizeof (MBRPARTITIONENTRY));

            if (pEbrPartitionEntry->type == NULL_BOOT_RECORD_SIGNATURE)
                return RTMsgErrorExitFailure("Logical partition with type 0 encountered");

            if (!pEbrPartitionEntry->partitionLba)
                return RTMsgErrorExitFailure("Logical partition invalid partition start offset (LBA) encountered");

            PARTITIONINFO *ppi = &g_aParsedPartitionInfo[idxPartition];
            ppi->idxPartition         = idxPartition;
            ppi->offPartition         = currentEbrOffset + (off_t)pEbrPartitionEntry->partitionLba * BLOCKSIZE;
            ppi->cbPartition          = (off_t)pEbrPartitionEntry->partitionBlkCnt * BLOCKSIZE;
            ppi->fBootable            = pEbrPartitionEntry->bootIndicator == 0x80;
            ppi->partitionType.legacy = pEbrPartitionEntry->type;

            g_lastPartNbr = idxPartition;

            if (ebr.chainingPartitionEntry.type == 0) /* end of chain */
                break;

            if (!PARTTYPE_IS_EXT(ebr.chainingPartitionEntry.type))
                return RTMsgErrorExitFailure("Logical partition chain broken");

            chainedEbrOffset = ebr.chainingPartitionEntry.partitionLba * BLOCKSIZE;
        }
    }
    return PARTITION_TABLE_MBR;
}

const char *getClassicPartitionDesc(uint8_t type)
{
    for (uint32_t i = 0; i < sizeof (g_partitionDescTable) / sizeof (struct PartitionDesc); i++)
    {
        if (g_partitionDescTable[i].type == type)
            return g_partitionDescTable[i].desc;
    }
    return "????";
}

void
displayGptPartitionTable(void)
{

    void *colBoot = NULL;

    SELFSIZINGTABLE tbl(2);

    /* Note: Omitting partition name column because type/UUID seems suffcient */
    void *colPartNbr   = tbl.addCol("#",                 "%3d",       1);

    /* If none of the partitions supports legacy BIOS boot, don't show column */
    for (int idxPartition = 2; idxPartition <= g_lastPartNbr; idxPartition++)
        if (g_aParsedPartitionInfo[idxPartition].fBootable) {
            colBoot    = tbl.addCol("Boot",         "%c   ",     1);
            break;
        }

    void *colStart     = tbl.addCol("Start",             "%lld",      1);
    void *colSectors   = tbl.addCol("Sectors",           "%lld",     -1, 2);
    void *colSize      = tbl.addCol("Size",              "%d.%d%c",   1);
    void *colOffset    = tbl.addCol("Offset",            "%lld",      1);
    /* Need to see how other OSes with GPT schemes use this field. Seems like type covers it
    void *colName      = tbl.addCol("Name",              "%s",       -1); */
    void *colType      = tbl.addCol("Type",              "%s",       -1, 2);

    for (int idxPartition = 2; idxPartition <= g_lastPartNbr; idxPartition++)
    {
        PARTITIONINFO *ppi = &g_aParsedPartitionInfo[idxPartition];
        if (ppi->idxPartition)
        {
            uint8_t exp = log2((double)ppi->cbPartition);
            char scaledMagnitude = ((char []){ ' ', 'K', 'M', 'G', 'T', 'P' })[exp / 10];

             /* This workaround is because IPRT RT*Printf* funcs don't handle floating point format specifiers */
            double cbPartitionScaled = (double)ppi->cbPartition / pow(2, (double)(((uint8_t)(exp / 10)) * 10));
            uint8_t cbPartitionIntPart = cbPartitionScaled;
            uint8_t cbPartitionFracPart = (cbPartitionScaled - (double)cbPartitionIntPart) * 10;

            char abGuid[GUID_STRING_LENGTH * 2];
            RTStrPrintf(abGuid, sizeof(abGuid), "%RTuuid",  &ppi->partitionType.gptGuidTypeSpecifier);

            char *pszPartitionTypeDesc = NULL;
            for (uint32_t i = 0; i < sizeof(g_gptPartitionTypes) / sizeof(GPTPARTITIONTYPE); i++)
                if (RTStrNICmp(abGuid, g_gptPartitionTypes[i].gptPartitionUuid, GUID_STRING_LENGTH) == 0)
                {
                    pszPartitionTypeDesc =  (char *)g_gptPartitionTypes[i].gptPartitionTypeDesc;
                    break;
                }

            if (!pszPartitionTypeDesc)
                RTPrintf("Couldn't find GPT partitiontype for GUID: %s\n", abGuid);

            void *row = tbl.addRow();
            tbl.setCell(row, colPartNbr,    idxPartition - 1);
            if (colBoot)
                tbl.setCell(row, colBoot,   ppi->fBootable ? '*' : ' ');
            tbl.setCell(row, colStart,      ppi->offPartition / BLOCKSIZE);
            tbl.setCell(row, colSectors,    ppi->cbPartition / BLOCKSIZE);
            tbl.setCell(row, colSize,       cbPartitionIntPart, cbPartitionFracPart, scaledMagnitude);
            tbl.setCell(row, colOffset,     ppi->offPartition);
/*          tbl.setCell(row, colName,       ppi->pszName);    ... see comment for column definition */
            tbl.setCell(row, colType,       SAFENULL(pszPartitionTypeDesc));
        }
    }
    tbl.displayTable();
    RTPrintf ("\n");
}

void
displayLegacyPartitionTable(void)
{
    /*
     * Partition table is most readable and concise when headers and columns
     * are adapted to the actual data, to avoid insufficient or excessive whitespace.
     */
    RTPrintf( "Virtual disk image:\n\n");
    RTPrintf("   Path: %s\n", g_pszBaseImagePath);
    if (g_pvDiskUuid)
        RTPrintf("   UUID: %s\n\n", g_pvDiskUuid);

    SELFSIZINGTABLE tbl(2);

    void *colPartition = tbl.addCol("Partition",    "%s%d",     -1);
    void *colBoot      = tbl.addCol("Boot",         "%c   ",     1);
    void *colStart     = tbl.addCol("Start",        "%lld",      1);
    void *colSectors   = tbl.addCol("Sectors",      "%lld",     -1, 2);
    void *colSize      = tbl.addCol("Size",         "%d.%d%c",   1);
    void *colOffset    = tbl.addCol("Offset",       "%lld",      1);
    void *colId        = tbl.addCol("Id",           "%2x",       1);
    void *colType      = tbl.addCol("Type",         "%s",       -1, 2);

    for (int idxPartition = 1; idxPartition <= g_lastPartNbr; idxPartition++)
    {
        PARTITIONINFO *p = &g_aParsedPartitionInfo[idxPartition];
        if (p->idxPartition)
        {
            uint8_t exp = log2((double)p->cbPartition);
            char scaledMagnitude = ((char []){ ' ', 'K', 'M', 'G', 'T', 'P' })[exp / 10];

             /* This workaround is because IPRT RT*Printf* funcs don't handle floating point format specifiers */
            double  cbPartitionScaled = (double)p->cbPartition / pow(2, (double)(((uint8_t)(exp / 10)) * 10));
            uint8_t cbPartitionIntPart = cbPartitionScaled;
            uint8_t cbPartitionFracPart = (cbPartitionScaled - (double)cbPartitionIntPart) * 10;

            void *row = tbl.addRow();

            tbl.setCell(row, colPartition,  g_pszBaseImageName, idxPartition);
            tbl.setCell(row, colBoot,       p->fBootable ? '*' : ' ');
            tbl.setCell(row, colStart,      p->offPartition / BLOCKSIZE);
            tbl.setCell(row, colSectors,    p->cbPartition / BLOCKSIZE);
            tbl.setCell(row, colSize,       cbPartitionIntPart, cbPartitionFracPart, scaledMagnitude);
            tbl.setCell(row, colOffset,     p->offPartition);
            tbl.setCell(row, colId,         p->partitionType.legacy);
            tbl.setCell(row, colType,       getClassicPartitionDesc((p->partitionType).legacy));
        }
    }
    tbl.displayTable();
    RTPrintf ("\n");
}

int
main(int argc, char **argv)
{

    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("RTR3InitExe failed, rc=%Rrc\n", rc);

    rc = VDInit();
    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("VDInit failed, rc=%Rrc\n", rc);

    memset(&g_vboximgOps, 0, sizeof(g_vboximgOps));
    g_vboximgOps.open        = vboximgOp_open;
    g_vboximgOps.read        = vboximgOp_read;
    g_vboximgOps.write       = vboximgOp_write;
    g_vboximgOps.getattr     = vboximgOp_getattr;
    g_vboximgOps.release     = vboximgOp_release;
    g_vboximgOps.readdir     = vboximgOp_readdir;
    g_vboximgOps.readlink    = vboximgOp_readlink;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    memset(&g_vboximgOpts, 0, sizeof(g_vboximgOpts));

    rc = fuse_opt_parse(&args, &g_vboximgOpts, vboximgOptDefs, vboximgOptHandler);
    if (rc < 0 || argc < 2 || RTStrCmp(argv[1], "-?" ) == 0 || g_vboximgOpts.fBriefUsage)
    {
        briefUsage();
        return 0;
    }

    if (g_vboximgOpts.fAllowRoot)
        fuse_opt_add_arg(&args, "-oallow_root");

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

    if (g_vboximgOpts.fVerbose)
        RTPrintf("vboximg: VirtualBox XPCOM object created\n");

    if (g_vboximgOpts.fList && g_vboximgOpts.pszImage == NULL)
    {
        listVMs(pVirtualBox);
        return 0;
    }

    ComPtr<IMedium> pBaseImageMedium = NULL;
    char    *pszFormat;
    VDTYPE  enmType;
    searchForBaseImage(pVirtualBox, g_vboximgOpts.pszImage, &pBaseImageMedium);
    if (pBaseImageMedium == NULL)
    {
        /*
         * Try to locate base image pMedium via the VirtualBox API, given the user-provided path
         * resolving symlinks back to hard path.
         */
        int cbNameMax = pathconf(g_vboximgOpts.pszImage, _PC_PATH_MAX);
        if (cbNameMax < 0)
            return cbNameMax;

        g_pszBaseImagePath = RTStrDup(g_vboximgOpts.pszImage);
        if (g_pszBaseImagePath == NULL)
            return RTMsgErrorExitFailure("out of memory\n");

        if (access(g_pszBaseImagePath, F_OK) < 0)
            return RTMsgErrorExitFailure("Virtual disk image not found: \"%s\"\n", g_pszBaseImagePath);

        if (access(g_pszBaseImagePath, R_OK) < 0)
             return RTMsgErrorExitFailure(
                    "Virtual disk image not readable: \"%s\"\n", g_pszBaseImagePath);

        if (g_vboximgOpts.fRW && access(g_vboximgOpts.pszImage, W_OK) < 0)
             return RTMsgErrorExitFailure(
                    "Virtual disk image not writeable: \"%s\"\n", g_pszBaseImagePath);
        rc = RTPathSplit(g_pszBaseImagePath, &g_u.split, sizeof(g_u), 0);

        if (RT_FAILURE(rc))
             return RTMsgErrorExitFailure(
                    "RTPathSplit failed on '%s': %Rrc",  g_pszBaseImagePath);

        if (!(g_u.split.fProps & RTPATH_PROP_FILENAME))
             return RTMsgErrorExitFailure(
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
                return RTMsgErrorExitFailure("VDGetFormat(%s,) "
                    "failed, rc=%Rrc\n", g_pszBaseImagePath, rc);

            g_pVDisk = NULL;
            rc = VDCreate(NULL /* pVDIIfsDisk */, enmType, &g_pVDisk);
            if (RT_SUCCESS(rc))
            {
                rc = VDOpen(g_pVDisk, pszFormat, g_pszBaseImagePath, 0, NULL /* pVDIfsImage */);
                if (RT_FAILURE(rc))
                {
                    VDClose(g_pVDisk, false /* fDeletes */);
                    return RTMsgErrorExitFailure("VDCreate(,%s,%s,,,) failed,"
                        " rc=%Rrc\n", pszFormat, g_pszBaseImagePath, rc);
                }
            }
            else
                return RTMsgErrorExitFailure("VDCreate failed, rc=%Rrc\n", rc);
        }
    } else {
        Bstr pMediumUuid;
        CHECK_ERROR(pBaseImageMedium, COMGETTER(Id)(pMediumUuid.asOutParam()));
        g_pvDiskUuid = RTStrDup((char *)CSTR(pMediumUuid));
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
                    return RTMsgErrorExitFailure("out of memory\n");

                if (g_pszBaseImagePath == NULL)
                    return RTMsgErrorExitFailure("out of memory\n");
                /*
                 * Create HDD container to open base image and differencing images into
                 */
                rc = VDGetFormat(NULL /* pVDIIfsDisk */, NULL /* pVDIIfsImage*/,
                        g_pszBaseImagePath, &pszFormat, &enmType);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExitFailure("VDGetFormat(,,%s,,) "
                        "failed (during HDD container creation), rc=%Rrc\n", g_pszBaseImagePath, rc);
                if (g_vboximgOpts.fVerbose)
                    RTPrintf("Creating container for base image of format %s\n", pszFormat);
                /** @todo Remove I/O CB's and crit sect. when VDRead()/VDWrite() are made threadsafe */
                rc = RTCritSectInit(&g_vdioLock);
                if (RT_SUCCESS(rc))
                {
                    g_VDIfThreadSync.pfnStartRead   = vboximgThreadStartRead;
                    g_VDIfThreadSync.pfnFinishRead  = vboximgThreadFinishRead;
                    g_VDIfThreadSync.pfnStartWrite  = vboximgThreadStartWrite;
                    g_VDIfThreadSync.pfnFinishWrite = vboximgThreadFinishWrite;
                    rc = VDInterfaceAdd(&g_VDIfThreadSync.Core, "vboximg_ThreadSync", VDINTERFACETYPE_THREADSYNC,
                                        &g_vdioLock, sizeof(VDINTERFACETHREADSYNC), &g_pVdIfs);
                }
                else
                    return RTMsgErrorExitFailure("ERROR: Failed to create critsects "
                                                 "for virtual disk I/O, rc=%Rrc\n", rc);

                g_pVDisk = NULL;
                rc = VDCreate(g_pVdIfs, enmType, &g_pVDisk);
                if (NS_FAILED(rc))
                    return RTMsgErrorExitFailure("ERROR: Couldn't create virtual disk container\n");
            }
            /** @todo (end of to do section) */

            if ( g_vboximgOpts.cHddImageDiffMax != 0 && diffNumber > g_vboximgOpts.cHddImageDiffMax)
                break;

            if (g_vboximgOpts.fVerbose)
            {
                if (diffNumber == 0)
                    RTPrintf("\nvboximg-mount: Opening base image into container:\n       %s\n",
                        g_pszBaseImagePath);
                else
                    RTPrintf("\nvboximg-mount: Opening difference image #%d into container:\n       %s\n",
                        diffNumber, g_pszBaseImagePath);
            }

            rc = VDOpen(g_pVDisk, pszFormat, g_pszBaseImagePath, 0, NULL /* pVDIfsImage */);
            if (RT_FAILURE(rc))
            {
                VDClose(g_pVDisk, false /* fDeletes */);
                return RTMsgErrorExitFailure("VDOpen(,,%s,,) failed, rc=%Rrc\n",
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
    g_cbEntireVDisk  = VDGetSize(g_pVDisk, 0 /* base */);

    if (g_vboximgOpts.fList)
    {
        if (g_pVDisk == NULL)
            return RTMsgErrorExitFailure("No valid --image to list partitions from\n");

        RTPrintf("\n");

        rc = parsePartitionTable();
        switch(rc)
        {
            case PARTITION_TABLE_MBR:
                displayLegacyPartitionTable();
                break;
            case PARTITION_TABLE_GPT:
                displayGptPartitionTable();
                break;
            default:
                return rc;
        }
        return 0;
    }
    if (g_vboximgOpts.idxPartition >= 0)
    {
        if (g_vboximgOpts.offset)
            return RTMsgErrorExitFailure("--offset and --partition are mutually exclusive options\n");

        if (g_vboximgOpts.size)
            return RTMsgErrorExitFailure("--size and --partition are mutually exclusive options\n");

        /*
         * --partition option specified. That will set the global offset and limit
         * honored by the disk read and write sanitizers to constrain operations
         * to within the specified partion based on an initial parsing of the MBR
         */
        rc = parsePartitionTable();
        if (rc < 0)
            return RTMsgErrorExitFailure("Error parsing disk MBR/Partition table\n");
        int partNbr = g_vboximgOpts.idxPartition;

        if (partNbr < 0 || partNbr > g_lastPartNbr)
            return RTMsgErrorExitFailure("Non-valid partition number specified\n");

        if (partNbr == 0)
        {
            g_vDiskOffset = 0;
            g_vDiskSize = VDGetSize(g_pVDisk, 0);
            if (g_vboximgOpts.fVerbose)
                RTPrintf("Partition 0 specified - Whole disk will be accessible\n");
        } else {
            for (int i = 0; i < g_lastPartNbr; i++)
            {
                /* If GPT, display vboximg's representation of partition table starts at partition 2
                 * but the table is displayed calling it partition 1, because the protective MBR
                 * record is relatively pointless to display or reference in this context */

                if (g_aParsedPartitionInfo[i].idxPartition == partNbr + g_fGPT ? 1 : 0)
                {
                     g_vDiskOffset = g_aParsedPartitionInfo[i].offPartition;
                     g_vDiskSize = g_vDiskOffset + g_aParsedPartitionInfo[i].cbPartition;
                     if (g_vboximgOpts.fVerbose)
                        RTPrintf("Partition %d specified. Only sectors %llu to %llu of disk will be accessible\n",
                            g_vboximgOpts.idxPartition, g_vDiskOffset / BLOCKSIZE, g_vDiskSize / BLOCKSIZE);
                }
            }
        }
    } else {
        if (g_vboximgOpts.offset) {
            if (g_vboximgOpts.offset < 0 || g_vboximgOpts.offset + g_vboximgOpts.size > g_cbEntireVDisk)
                return RTMsgErrorExitFailure("User specified offset out of range of virtual disk\n");

            if (g_vboximgOpts.fVerbose)
                RTPrintf("Setting r/w bias (offset) to user requested value for sector %llu\n", g_vDiskOffset / BLOCKSIZE);

            g_vDiskOffset = g_vboximgOpts.offset;
        }
        if (g_vboximgOpts.size) {
            if (g_vboximgOpts.size < 0 || g_vboximgOpts.offset + g_vboximgOpts.size > g_cbEntireVDisk)
                return RTMsgErrorExitFailure("User specified size out of range of virtual disk\n");

            if (g_vboximgOpts.fVerbose)
                RTPrintf("Setting r/w size limit to user requested value %llu\n", g_vDiskSize / BLOCKSIZE);

            g_vDiskSize = g_vboximgOpts.size;
        }
    }
    if (g_vDiskSize == 0)
        g_vDiskSize = g_cbEntireVDisk - g_vDiskOffset;

    /*
     * Hand control over to libfuse.
     */
    if (g_vboximgOpts.fVerbose)
        RTPrintf("\nvboximg-mount: Going into background...\n");

    rc = fuse_main(args.argc, args.argv, &g_vboximgOps, NULL);

    int rc2 = VDClose(g_pVDisk, false /* fDelete */);
    AssertRC(rc2);
    RTPrintf("vboximg-mount: fuse_main -> %d\n", rc);
    return rc;
}

