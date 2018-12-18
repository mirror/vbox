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
#include <VBox/vd-plugin.h>
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
#include <iprt/base64.h>

#include "vboximg-mount.h"
#include "vboximgCrypto.h"
#include "vboximgMedia.h"
#include "SelfSizingTable.h"
#include "vboximgOpts.h"

using namespace com;

enum {
     USAGE_FLAG,
};

enum { PARTITION_TABLE_MBR = 1, PARTITION_TABLE_GPT = 2 };

#define VBOX_EXTPACK                "Oracle VM VirtualBox Extension Pack"
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
#define VERBOSE                     g_vboximgOpts.fVerbose
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
static char                 *g_pszDiskUuid;          /** UUID of image (if known, otherwise NULL) */
static off_t                 g_vDiskOffset;         /** Biases r/w from start of VD */
static off_t                 g_vDiskSize;           /** Limits r/w length for VD */
static int32_t               g_cReaders;            /** Number of readers for VD */
static int32_t               g_cWriters;            /** Number of writers for VD */
static RTFOFF                g_cbEntireVDisk;       /** Size of VD */
static PVDINTERFACE          g_pVdIfs;              /** @todo Remove when VD I/O becomes threadsafe */
static VDINTERFACETHREADSYNC g_VDIfThreadSync;      /** @todo Remove when VD I/O becomes threadsafe */
static RTCRITSECT            g_vdioLock;            /** @todo Remove when VD I/O becomes threadsafe */
static uint16_t              g_lastPartNbr;         /** Last partition number found in MBR + EBR chain */
static bool                  g_fGPT;                /** True if GPT type partition table was found */
static char                 *g_pszImageName;        /** Base filename for current VD image */
static char                 *g_pszImagePath;        /** Full path to current VD image */
static char                 *g_pszBaseImagePath;    /** Base image known after parsing */
static char                 *g_pszBaseImageName;    /** Base image known after parsing */
static uint32_t              g_cImages;             /** Number of images in diff chain */

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

VBOXIMGOPTS g_vboximgOpts;

#define OPTION(fmt, pos, val) { fmt, offsetof(struct vboximgOpts, pos), val }

static struct fuse_opt vboximgOptDefs[] = {
    OPTION("--image=%s",      pszImageUuidOrPath,   0),
    OPTION("-i %s",           pszImageUuidOrPath,   0),
    OPTION("--rw",            fRW,                  1),
    OPTION("--root",          fAllowRoot,           0),
    OPTION("--vm=%s",         pszVm,                0),
    OPTION("--partition=%d",  idxPartition,         1),
    OPTION("-p %d",           idxPartition,         1),
    OPTION("--offset=%d",     offset,               1),
    OPTION("-o %d",           offset,               1),
    OPTION("--size=%d",       size,                 1),
    OPTION("-s %d",           size,                 1),
    OPTION("-l",              fList,                1),
    OPTION("--list",          fList,                1),
    OPTION("--verbose",       fVerbose,             1),
    OPTION("-v",              fVerbose,             1),
    OPTION("--wide",          fWide,                1),
    OPTION("-w",              fWide,                1),
    OPTION("-lv",             fVerboseList,         1),
    OPTION("-vl",             fVerboseList,         1),
    OPTION("-lw",             fWideList,            1),
    OPTION("-wl",             fWideList,            1),
    OPTION("-h",              fBriefUsage,          1),
    FUSE_OPT_KEY("--help",    USAGE_FLAG),
    FUSE_OPT_KEY("-vm",       FUSE_OPT_KEY_NONOPT),
    FUSE_OPT_END
};

typedef struct IMAGELIST
{
    struct IMAGELIST *next;
    struct IMAGELIST *prev;
    ComPtr<IToken> pLockToken;
    bool   fWriteable;
    ComPtr<IMedium> pImage;
    Bstr   pImageName;
    Bstr   pImagePath;
} IMAGELIST;

IMAGELIST listHeadLockList;  /* flink & blink intentionally left NULL */

static void
briefUsage()
{
    RTPrintf("usage: vboximg-mount [options] <mount point directory path>\n\n"
        "vboximg-mount options:\n\n"
        "  [ { -i | --image= } <specifier> ]  VirtualBox disk base or snapshot image,\n"
        "                                     specified by UUID, or fully-qualified path\n"
        "\n"
        "  [ { -l | --list } ]                If --image specified, list its partitions,\n"
        "                                     otherwise, list registered VMs and their\n"
        "                                     attached virtual HDD disk media. In verbose\n"
        "                                     mode, VM/media list will be long format,\n"
        "                                     i.e. including snapshot images and paths.\n"
        "\n"
        " [ { -w | --wide } ]                 List media in wide / tabular format\n"
        "                                     (reduces vertical scrolling but requires\n"
        "                                     wider than standard 80 column window\n)"
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
        "  [ --rw ]                           Make image writeable (default = readonly)\n"
        "\n"
        "  [ --root ]                         Same as -o allow_root.\n"
        "\n"
        "  [ { -v | --verbose } ]              Log extra information.\n"
        "\n"
        "  [ -o opt[,opt...]]                 FUSE mount options.\n"
        "\n"
        "  [ { -h | -? } ]                    Display short usage info (no FUSE options).\n"
        "  [ --help ]                         Display long usage info (incl. FUSE opts).\n\n"
    );
    RTPrintf("\n"
      "vboximg-mount is a utility to make VirtualBox disk images available to the host\n"
      "operating system in a root or non-root accessible way. The user determines the\n"
      "historical representation of the disk by choosing either the base image or a\n"
      "snapshot, to establish the desired level of currency of the mounted disk.\n"
      "\n"
      "The disk image is mounted through this utility inside a FUSE-based filesystem\n"
      "that overlays the user-provided mount point. The FUSE filesystem presents a\n"
      "a directory that contains two files: an HDD pseudo device node and a symbolic\n"
      "link. The device node is named 'vhdd' and is the access point to the synthesized\n"
      "state of the virtual disk. It is the entity that can be mounted or otherwise\n"
      "accessed through the host OS. The symbolic link is given the same name as the\n"
      "base image, as determined from '--image' option argument. The link equates\n"
      "to the specified image's location (path).\n"
      "\n"
      "If the user provides a base image UUID/path with the --image option, only\n"
      "the base image will be exposed via vhdd, disregarding any snapshots.\n"
      "Alternatively, if a snapshot (e.g. disk differencing image) is provided,\n"
      "the chain of snapshots is calculated from that \"leaf\" snapshot\n"
      "to the base image and the whole chain of images is merged to form the exposed\n"
      "state of the FUSE-mounted disk.\n"
      "\n"

    );
}

static int
vboximgOptHandler(void *data, const char *arg, int optKey, struct fuse_args *outargs)
{
    NOREF(data);
    NOREF(arg);
    NOREF(optKey);
    NOREF(outargs);

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
    RT_NOREF(pszPath, pInfo);;
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
    NOREF(pszPath);

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
    NOREF(pszPath);
    NOREF(pInfo);

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
    NOREF(pszPath);
    NOREF(pInfo);

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

    LogFlowFunc(("pszPath=%s, stat(\"%s\")\n", pszPath, g_pszImagePath));

    memset(stbuf, 0, sizeof(struct stat));

    if (RTStrCmp(pszPath, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if (RTStrCmp(pszPath + 1, "vhdd") == 0)
    {
        rc = stat(g_pszImagePath, stbuf);
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
    else if (RTStrNCmp(pszPath + 1, g_pszImageName, strlen(g_pszImageName)) == 0)
    {
        /* When the disk is partitioned, the symbolic link named from `basename` of
         * resolved path to VBox disk image, has appended to it formatted text
         * representing the offset range of the partition.
         *
         *  $ vboximg-mount -i /stroll/along/the/path/simple_fixed_disk.vdi -p 1 /mnt/tmpdir
         *  $ ls /mnt/tmpdir
         *  simple_fixed_disk.vdi[20480:2013244928]    vhdd
         */
        rc = stat(g_pszImagePath, stbuf);
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
    NOREF(offset);
    NOREF(pInfo);

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
        pfnFiller(pvBuf, g_pszImageName, NULL, 0);
    else
    {
        char tmp[BASENAME_MAX];
        RTStrPrintf(tmp, sizeof (tmp), "%s[%jd:%jd]", g_pszImageName, g_vDiskOffset, g_vDiskSize);
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
    NOREF(pszPath);
    RTStrCopy(buf, size, g_pszImagePath);
    return 0;
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

    RTPrintf( "Virtual disk image:\n\n");
    RTPrintf("   Base: %s\n", g_pszBaseImagePath);
    if (g_cImages > 1)
        RTPrintf("   Diff: %s\n", g_pszImagePath);
    if (g_pszDiskUuid)
            RTPrintf("   UUID: %s\n\n", g_pszDiskUuid);

    void *colBoot = NULL;

    SELFSIZINGTABLE tbl(2);

    /* Note: Omitting partition name column because type/UUID seems suffcient */
    void *colPartNbr   = tbl.addCol("#",                 "%3d",       1);

    /* If none of the partitions supports legacy BIOS boot, don't show that column */
    for (int idxPartition = 2; idxPartition <= g_lastPartNbr; idxPartition++)
        if (g_aParsedPartitionInfo[idxPartition].fBootable) {
            colBoot    = tbl.addCol("Boot",         "%c   ",     1);
            break;
        }

    void *colStart     = tbl.addCol("Start",             "%lld",      1);
    void *colSectors   = tbl.addCol("Sectors",           "%lld",     -1, 2);
    void *colSize      = tbl.addCol("Size",              "%s",        1);
    void *colOffset    = tbl.addCol("Offset",            "%lld",      1);
    void *colType      = tbl.addCol("Type",              "%s",       -1, 2);

#if 0 /* need to see how other OSes w/GPT use 'Name' field, right now 'Type' seems to suffice */
    void *colName      = tbl.addCol("Name",              "%s",       -1); */
#endif

    for (int idxPartition = 2; idxPartition <= g_lastPartNbr; idxPartition++)
    {
        PARTITIONINFO *ppi = &g_aParsedPartitionInfo[idxPartition];
        if (ppi->idxPartition)
        {
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
            tbl.setCell(row, colSize,       vboximgScaledSize(ppi->cbPartition));
            tbl.setCell(row, colOffset,     ppi->offPartition);
            tbl.setCell(row, colType,       SAFENULL(pszPartitionTypeDesc));

#if 0 /* see comment for stubbed-out 'Name' column definition above */
            tbl.setCell(row, colName,       ppi->pszName);
#endif

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
    RTPrintf("   Base: %s\n", g_pszBaseImagePath);
    if (g_cImages > 1)
        RTPrintf("   Diff: %s\n", g_pszImagePath);
    if (g_pszDiskUuid)
            RTPrintf("   UUID: %s\n\n", g_pszDiskUuid);

    SELFSIZINGTABLE tbl(2);

    void *colPartition = tbl.addCol("Partition",    "%s(%d)",     -1);
    void *colBoot      = tbl.addCol("Boot",         "%c   ",     1);
    void *colStart     = tbl.addCol("Start",        "%lld",      1);
    void *colSectors   = tbl.addCol("Sectors",      "%lld",     -1, 2);
    void *colSize      = tbl.addCol("Size",         "%s",        1);
    void *colOffset    = tbl.addCol("Offset",       "%lld",      1);
    void *colId        = tbl.addCol("Id",           "%2x",       1);
    void *colType      = tbl.addCol("Type",         "%s",       -1, 2);

    for (int idxPartition = 1; idxPartition <= g_lastPartNbr; idxPartition++)
    {
        PARTITIONINFO *p = &g_aParsedPartitionInfo[idxPartition];
        if (p->idxPartition)
        {
            void *row = tbl.addRow();
            tbl.setCell(row, colPartition,  g_pszBaseImageName, idxPartition);
            tbl.setCell(row, colBoot,       p->fBootable ? '*' : ' ');
            tbl.setCell(row, colStart,      p->offPartition / BLOCKSIZE);
            tbl.setCell(row, colSectors,    p->cbPartition / BLOCKSIZE);
            tbl.setCell(row, colSize,       vboximgScaledSize(p->cbPartition));
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

    g_vboximgOpts.idxPartition = -1;

    rc = fuse_opt_parse(&args, &g_vboximgOpts, vboximgOptDefs, vboximgOptHandler);
    if (rc < 0 || argc < 2 || RTStrCmp(argv[1], "-?" ) == 0 || g_vboximgOpts.fBriefUsage)
    {
        briefUsage();
        return 0;
    }

    if (g_vboximgOpts.fAllowRoot)
        fuse_opt_add_arg(&args, "-oallow_root");

    /*
     * FUSE doesn't seem to like combining options with one hyphen, as traditional UNIX
     * command line utilities allow. The following flags, fWideList and fVerboseList,
     * and their respective option definitions give the appearance of combined opts,
     * so that -vl, -lv, -wl, -lw options are allowed, since those in particular would
     * tend to conveniently facilitate some of the most common use cases.
     */
    if (g_vboximgOpts.fWideList)
    {
        g_vboximgOpts.fWide = true;
        g_vboximgOpts.fList = true;
    }
    if (g_vboximgOpts.fVerboseList)
    {
        g_vboximgOpts.fVerbose = true;
        g_vboximgOpts.fList    = true;
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

    if (g_vboximgOpts.fList && g_vboximgOpts.pszImageUuidOrPath == NULL)
    {
        vboximgListVMs(pVirtualBox);
        return VINF_SUCCESS;
    }

    Bstr    pMediumUuid;
    ComPtr<IMedium> pVDiskMedium = NULL;
    char   *pszFormat;
    VDTYPE  enmType;

    /*
     * Open chain of images from what is provided on command line, to base image
     */
    if (g_vboximgOpts.pszImageUuidOrPath)
    {
        /* compiler was too fussy about access mode's data type in conditional expr, so... */
        if (g_vboximgOpts.fRW)
            CHECK_ERROR(pVirtualBox, OpenMedium(Bstr(g_vboximgOpts.pszImageUuidOrPath).raw(), DeviceType_HardDisk,
                AccessMode_ReadWrite, false /* forceNewUuid */, pVDiskMedium.asOutParam()));

        else
            CHECK_ERROR(pVirtualBox, OpenMedium(Bstr(g_vboximgOpts.pszImageUuidOrPath).raw(), DeviceType_HardDisk,
                AccessMode_ReadOnly, false /* forceNewUuid */, pVDiskMedium.asOutParam()));

        if (FAILED(rc))
            return RTMsgErrorExitFailure("\nCould't find specified VirtualBox base or snapshot disk image:\n%s",
                 g_vboximgOpts.pszImageUuidOrPath);


        CHECK_ERROR(pVDiskMedium, COMGETTER(Id)(pMediumUuid.asOutParam()));
        g_pszDiskUuid = RTStrDup((char *)CSTR(pMediumUuid));

        /*
         * Lock & cache the disk image media chain (from leaf to base).
         * Only leaf can be rw (and only if media is being mounted in non-default writable (rw) mode)
         *
         * Note: Failure to acquire lock is intentionally fatal (e.g. program termination)
         */

        if (VERBOSE)
            RTPrintf("\nAttempting to lock medium chain from leaf image to base image\n");

        bool fLeaf = true;
        g_cImages = 0;

        do
        {
            ++g_cImages;
            IMAGELIST *pNewEntry= new IMAGELIST();
            pNewEntry->pImage = pVDiskMedium;
            CHECK_ERROR(pVDiskMedium, COMGETTER(Name)((pNewEntry->pImageName).asOutParam()));
            CHECK_ERROR(pVDiskMedium, COMGETTER(Location)((pNewEntry->pImagePath).asOutParam()));

            if (VERBOSE)
                RTPrintf("  %s", CSTR(pNewEntry->pImageName));

            if (fLeaf && g_vboximgOpts.fRW)
            {
                if (VERBOSE)
                    RTPrintf(" ... Locking for write\n");
                CHECK_ERROR_RET(pVDiskMedium, LockWrite((pNewEntry->pLockToken).asOutParam()), rc);
                pNewEntry->fWriteable = true;
            }
            else
            {
                if (VERBOSE)
                    RTPrintf(" ... Locking for read\n");
                CHECK_ERROR_RET(pVDiskMedium, LockRead((pNewEntry->pLockToken).asOutParam()), rc);
            }

            IMAGELIST *pCurImageEntry = &listHeadLockList;
            while (pCurImageEntry->next)
                pCurImageEntry = pCurImageEntry->next;
            pCurImageEntry->next = pNewEntry;
            pNewEntry->prev = pCurImageEntry;
            listHeadLockList.prev = pNewEntry;

            CHECK_ERROR(pVDiskMedium, COMGETTER(Parent)(pVDiskMedium.asOutParam()));
            fLeaf = false;
        }
        while(pVDiskMedium);
    }

    ComPtr<IMedium> pVDiskBaseMedium = listHeadLockList.prev->pImage;
    Bstr pVDiskBaseImagePath = listHeadLockList.prev->pImagePath;
    Bstr pVDiskBaseImageName = listHeadLockList.prev->pImageName;

    g_pszBaseImagePath = RTStrDup((char *)CSTR(pVDiskBaseImagePath));
    g_pszBaseImageName = RTStrDup((char *)CSTR(pVDiskBaseImageName));

    g_pszImagePath = RTStrDup((char *)CSTR(listHeadLockList.next->pImagePath));
    g_pszImageName = RTStrDup((char *)CSTR(listHeadLockList.next->pImageName));

    /*
     * Attempt to VDOpen media (base and any snapshots), handling encryption,
     * if that property is set for base media
     */
    Bstr pBase64EncodedKeyStore;

    rc = pVDiskBaseMedium->GetProperty(Bstr("CRYPT/KeyStore").raw(), pBase64EncodedKeyStore.asOutParam());
    if (SUCCEEDED(rc) && strlen(CSTR(pBase64EncodedKeyStore)) != 0)
    {
        RTPrintf("\nvboximgMount: Encrypted disks not supported in this version\n\n");
        return -1;
    }


/* ***************** BEGIN IFDEF'D (STUBBED-OUT) CODE ************** */
/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

#if 0 /* The following encrypted disk related code is stubbed out until it can be finished.
       * What is here is an attempt to port the VBoxSVC specific code in i_openForIO to
       * a client's proximity. It is supplemented by code in vboximgCrypto.cpp and
       * vboximageCrypt.h that was lifed from SecretKeyStore.cpp along with the setup
       * task function.
       *
       * The ultimate solution may be to use a simpler but less efficient COM interface,
       * or to use VD encryption interfaces and key containers entirely. The keystore
       * handling/filter approach that is here may be a bumbling hybrid approach
       * that is broken (trying to bridge incompatible disk encryption mechanisms) or otherwise
       * doesn't make sense. */

    Bstr pKeyId;
    ComPtr<IExtPackManager> pExtPackManager;
    ComPtr<IExtPack> pExtPack;
    com::SafeIfaceArray<IExtPackPlugIn> pExtPackPlugIns;

    if (SUCCEEDED(rc))
    {
        RTPrintf("Got GetProperty(\"CRYPT/KeyStore\") = %s\n", CSTR(pBase64EncodedKeyStore));
        if (strlen(CSTR(pBase64EncodedKeyStore)) == 0)
            return RTMsgErrorExitFailure("Image '%s' is configured for encryption but "
                    "there is no key store to retrieve the password from", CSTR(pVDiskBaseImageName));

        SecretKeyStore keyStore(false);
        RTBase64Decode(CSTR(pBase64EncodedKeyStore), &keyStore, sizeof (SecretKeyStore), NULL, NULL);

        rc = pVDiskBaseMedium->GetProperty(Bstr("CRYPT/KeyId").raw(), pKeyId.asOutParam());
        if (SUCCEEDED(rc) && strlen(CSTR(pKeyId)) == 0)
            return RTMsgErrorExitFailure("Image '%s' is configured for encryption but "
                "doesn't have a key identifier set", CSTR(pVDiskBaseImageName));

        RTPrintf("        key id: %s\n", CSTR(pKeyId));

#ifndef VBOX_WITH_EXTPACK
        return RTMsgErrorExitFailure(
            "Encryption is not supported because extension pack support is not built in");
#endif

        CHECK_ERROR(pVirtualBox, COMGETTER(ExtensionPackManager)(pExtPackManager.asOutParam()));
        BOOL fExtPackUsable;
        CHECK_ERROR(pExtPackManager, IsExtPackUsable((PRUnichar *)VBOX_EXTPACK, &fExtPackUsable));
        if (fExtPackUsable)
        {
            /* Load the PlugIn */

            CHECK_ERROR(pExtPackManager, Find((PRUnichar *)VBOX_EXTPACK, pExtPack.asOutParam()));
            if (RT_FAILURE(rc))
                return RTMsgErrorExitFailure(
                    "Encryption is not supported because the extension pack '%s' is missing",
                    VBOX_EXTPACK);

            CHECK_ERROR(pExtPack, COMGETTER(PlugIns)(ComSafeArrayAsOutParam(pExtPackPlugIns)));

            Bstr pPlugInPath;
            size_t iPlugIn;
            for (iPlugIn = 0; iPlugIn < pExtPackPlugIns.size(); iPlugIn++)
            {
                Bstr pPlugInName;
                CHECK_ERROR(pExtPackPlugIns[iPlugIn], COMGETTER(Name)(pPlugInName.asOutParam()));
                if (RTStrCmp(CSTR(pPlugInName), "VDPlugInCrypt") == 0)
                {
                    CHECK_ERROR(pExtPackPlugIns[iPlugIn], COMGETTER(ModulePath)(pPlugInPath.asOutParam()));
                    break;
                }
            }
            if (iPlugIn == pExtPackPlugIns.size())
                return RTMsgErrorExitFailure("Encryption is not supported because the extension pack '%s' "
                    "is missing the encryption PlugIn (old extension pack installed?)", VBOX_EXTPACK);

            rc = VDPluginLoadFromFilename(CSTR(pPlugInPath));
            if (RT_FAILURE(rc))
                return RTMsgErrorExitFailure("Retrieving encryption settings of the image failed "
                    "because the encryption PlugIn could not be loaded\n");
        }

        SecretKey *pKey = NULL;
        rc = keyStore.retainSecretKey(Utf8Str(pKeyId), &pKey);
        if (RT_FAILURE(rc))
                return RTMsgErrorExitFailure(
                    "Failed to retrieve the secret key with ID \"%s\" from the store (%Rrc)",
                    CSTR(pKeyId), rc);

        VDISKCRYPTOSETTINGS vdiskCryptoSettings, *pVDiskCryptoSettings = &vdiskCryptoSettings;

        vboxImageCryptoSetup(pVDiskCryptoSettings, NULL,
            (const char *)CSTR(pBase64EncodedKeyStore), (const char *)pKey->getKeyBuffer(), false);

        rc = VDFilterAdd(g_pVDisk, "CRYPT", VD_FILTER_FLAGS_DEFAULT, pVDiskCryptoSettings->vdFilterIfaces);
        keyStore.releaseSecretKey(Utf8Str(pKeyId));

        if (rc == VERR_VD_PASSWORD_INCORRECT)
            return RTMsgErrorExitFailure("The password to decrypt the image is incorrect");

        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("Failed to load the decryption filter: %Rrc", rc);
    }
#endif

/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* **************** END IFDEF'D (STUBBED-OUT) CODE ***************** */

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

   /*
     * Create HDD container to open base image and differencing images into
     */
    rc = VDGetFormat(NULL /* pVDIIfsDisk */, NULL /* pVDIIfsImage*/,
            CSTR(pVDiskBaseImagePath), &pszFormat, &enmType);

    if (RT_FAILURE(rc))
        return RTMsgErrorExitFailure("VDGetFormat(,,%s,,) "
            "failed (during HDD container creation), rc=%Rrc\n", g_pszImagePath, rc);

    if (VERBOSE)
        RTPrintf("\nCreating container for base image of format %s\n", pszFormat);

    g_pVDisk = NULL;
    rc = VDCreate(g_pVdIfs, enmType, &g_pVDisk);
    if ((rc))
        return RTMsgErrorExitFailure("ERROR: Couldn't create virtual disk container\n");

    /* Open all virtual disk media from leaf snapshot (if any) to base image*/

    if (VERBOSE)
        RTPrintf("\nOpening medium chain\n");

    IMAGELIST *pCurMedium = listHeadLockList.prev;  /* point to base image */
    while (pCurMedium != &listHeadLockList)
    {
        if (VERBOSE)
            RTPrintf("  Open: %s\n", CSTR(pCurMedium->pImagePath));

        rc = VDOpen(g_pVDisk,
                    CSTR(pszFormat),
                    CSTR(pCurMedium->pImagePath),
                    pCurMedium->fWriteable,
                    g_pVdIfs);

        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("Could not open the medium storage unit '%s' %Rrc",
                CSTR(pCurMedium->pImagePath), rc);

        pCurMedium = pCurMedium->prev;
    }

    g_cReaders  = VDIsReadOnly(g_pVDisk) ? INT32_MAX / 2 : 0;
    g_cWriters  = 0;
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
            if (VERBOSE)
                RTPrintf("\nPartition 0 specified - Whole disk will be accessible\n");
        } else {
            int fFoundPartition = false;
            for (int i = 1; i < g_lastPartNbr + 1; i++)
            {
                /* If GPT, display vboximg's representation of partition table starts at partition 2
                 * but the table is displayed calling it partition 1, because the protective MBR
                 * record is relatively pointless to display or reference in this context */
                if (g_aParsedPartitionInfo[i].idxPartition == partNbr + (g_fGPT ? 1 : 0))
                {
                     fFoundPartition = true;
                     g_vDiskOffset = g_aParsedPartitionInfo[i].offPartition;
                     g_vDiskSize = g_vDiskOffset + g_aParsedPartitionInfo[i].cbPartition;
                     if (VERBOSE)
                        RTPrintf("\nPartition %d specified. Only sectors %llu to %llu of disk will be accessible\n",
                            g_vboximgOpts.idxPartition, g_vDiskOffset / BLOCKSIZE, g_vDiskSize / BLOCKSIZE);
                }
            }
            if (!fFoundPartition)
                return RTMsgErrorExitFailure("Couldn't find partition %d in partition table\n", partNbr);
        }
    } else {
        if (g_vboximgOpts.offset) {
            if (g_vboximgOpts.offset < 0 || g_vboximgOpts.offset + g_vboximgOpts.size > g_cbEntireVDisk)
                return RTMsgErrorExitFailure("User specified offset out of range of virtual disk\n");

            if (VERBOSE)
                RTPrintf("Setting r/w bias (offset) to user requested value for sector %llu\n", g_vDiskOffset / BLOCKSIZE);

            g_vDiskOffset = g_vboximgOpts.offset;
        }
        if (g_vboximgOpts.size) {
            if (g_vboximgOpts.size < 0 || g_vboximgOpts.offset + g_vboximgOpts.size > g_cbEntireVDisk)
                return RTMsgErrorExitFailure("User specified size out of range of virtual disk\n");

            if (VERBOSE)
                RTPrintf("Setting r/w size limit to user requested value %llu\n", g_vDiskSize / BLOCKSIZE);

            g_vDiskSize = g_vboximgOpts.size;
        }
    }
    if (g_vDiskSize == 0)
        g_vDiskSize = g_cbEntireVDisk - g_vDiskOffset;

    /*
     * Hand control over to libfuse.
     */
    if (VERBOSE)
        RTPrintf("\nvboximg-mount: Going into background...\n");

    rc = fuse_main(args.argc, args.argv, &g_vboximgOps, NULL);

    int rc2 = VDClose(g_pVDisk, false /* fDelete */);
    AssertRC(rc2);
    RTPrintf("vboximg-mount: fuse_main -> %d\n", rc);
    return rc;
}

