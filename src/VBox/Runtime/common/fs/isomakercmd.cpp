/* $Id$ */
/** @file
 * IPRT - ISO Image Maker Command.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include "internal/iprt.h"
#include <iprt/fsisomaker.h>

#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef enum RTFSISOMAKERCMDOPT
{
    RTFSISOMAKERCMD_OPT_FIRST = 1000,

    RTFSISOMAKERCMD_OPT_IPRT_ISO_MAKER_FILE_MARKER,
    RTFSISOMAKERCMD_OPT_OUTPUT_BUFFER_SIZE,
    RTFSISOMAKERCMD_OPT_RANDOM_OUTPUT_BUFFER_SIZE,

    /*
     * Compatibility options:
     */
    RTFSISOMAKERCMD_OPT_ABSTRACT_FILE_ID,
    RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,
    RTFSISOMAKERCMD_OPT_ALLOW_LIMITED_SIZE,
    RTFSISOMAKERCMD_OPT_ALLOW_LOWERCASE,
    RTFSISOMAKERCMD_OPT_ALLOW_MULTI_DOT,
    RTFSISOMAKERCMD_OPT_ALPHA_BOOT,
    RTFSISOMAKERCMD_OPT_APPLE,
    RTFSISOMAKERCMD_OPT_BIBLIOGRAPHIC_FILE_ID,
    RTFSISOMAKERCMD_OPT_CHECK_OLD_NAMES,
    RTFSISOMAKERCMD_OPT_CHECK_SESSION,
    RTFSISOMAKERCMD_OPT_COPYRIGHT_FILE_ID,
    RTFSISOMAKERCMD_OPT_DETECT_HARDLINKS,
    RTFSISOMAKERCMD_OPT_DIR_MODE,
    RTFSISOMAKERCMD_OPT_DVD_VIDEO,
    RTFSISOMAKERCMD_OPT_ELTORITO_ALT_BOOT,
    RTFSISOMAKERCMD_OPT_ELTORITO_HARD_DISK_BOOT,
    RTFSISOMAKERCMD_OPT_ELTORITO_INFO_TABLE,
    RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SEG,
    RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SIZE,
    RTFSISOMAKERCMD_OPT_ELTORITO_NO_BOOT,
    RTFSISOMAKERCMD_OPT_ELTORITO_NO_EMULATION_BOOT,
    RTFSISOMAKERCMD_OPT_EXCLUDE_LIST,
    RTFSISOMAKERCMD_OPT_FILE_MODE,
    RTFSISOMAKERCMD_OPT_FORCE_RR,
    RTFSISOMAKERCMD_OPT_GID,
    RTFSISOMAKERCMD_OPT_GRAFT_POINTS,
    RTFSISOMAKERCMD_OPT_GUI,
    RTFSISOMAKERCMD_OPT_HFS_AUTO,
    RTFSISOMAKERCMD_OPT_HFS_BLESS,
    RTFSISOMAKERCMD_OPT_HFS_BOOT_FILE,
    RTFSISOMAKERCMD_OPT_HFS_CAP,
    RTFSISOMAKERCMD_OPT_HFS_CHRP_BOOT,
    RTFSISOMAKERCMD_OPT_HFS_CLUSTER_SIZE,
    RTFSISOMAKERCMD_OPT_HFS_CREATOR,
    RTFSISOMAKERCMD_OPT_HFS_DAVE,
    RTFSISOMAKERCMD_OPT_HFS_DOUBLE,
    RTFSISOMAKERCMD_OPT_HFS_ENABLE,
    RTFSISOMAKERCMD_OPT_HFS_ETHERSHARE,
    RTFSISOMAKERCMD_OPT_HFS_EXCHANGE,
    RTFSISOMAKERCMD_OPT_HFS_HIDE,
    RTFSISOMAKERCMD_OPT_HFS_HIDE_LIST,
    RTFSISOMAKERCMD_OPT_HFS_ICON_POSITION,
    RTFSISOMAKERCMD_OPT_HFS_INPUT_CHARSET,
    RTFSISOMAKERCMD_OPT_HFS_MAC_NAME,
    RTFSISOMAKERCMD_OPT_HFS_MACBIN,
    RTFSISOMAKERCMD_OPT_HFS_MAGIC,
    RTFSISOMAKERCMD_OPT_HFS_MAP,
    RTFSISOMAKERCMD_OPT_HFS_NETATALK,
    RTFSISOMAKERCMD_OPT_HFS_NO_DESKTOP,
    RTFSISOMAKERCMD_OPT_HFS_OSX_DOUBLE,
    RTFSISOMAKERCMD_OPT_HFS_OSX_HFS,
    RTFSISOMAKERCMD_OPT_HFS_OUTPUT_CHARSET,
    RTFSISOMAKERCMD_OPT_HFS_PARMS,
    RTFSISOMAKERCMD_OPT_HFS_PART,
    RTFSISOMAKERCMD_OPT_HFS_PREP_BOOT,
    RTFSISOMAKERCMD_OPT_HFS_PROBE,
    RTFSISOMAKERCMD_OPT_HFS_ROOT_INFO,
    RTFSISOMAKERCMD_OPT_HFS_SFM,
    RTFSISOMAKERCMD_OPT_HFS_SGI,
    RTFSISOMAKERCMD_OPT_HFS_SINGLE,
    RTFSISOMAKERCMD_OPT_HFS_TYPE,
    RTFSISOMAKERCMD_OPT_HFS_UNLOCK,
    RTFSISOMAKERCMD_OPT_HFS_USHARE,
    RTFSISOMAKERCMD_OPT_HFS_VOL_ID,
    RTFSISOMAKERCMD_OPT_HFS_XINET,
    RTFSISOMAKERCMD_OPT_HIDDEN,
    RTFSISOMAKERCMD_OPT_HIDDEN_LIST,
    RTFSISOMAKERCMD_OPT_HIDE,
    RTFSISOMAKERCMD_OPT_HIDE_JOLIET,
    RTFSISOMAKERCMD_OPT_HIDE_JOLIET_LIST,
    RTFSISOMAKERCMD_OPT_HIDE_JOLIET_TRANS_TBL,
    RTFSISOMAKERCMD_OPT_HIDE_LIST,
    RTFSISOMAKERCMD_OPT_HIDE_RR_MOVED,
    RTFSISOMAKERCMD_OPT_HPPA_BOOTLOADER,
    RTFSISOMAKERCMD_OPT_HPPA_CMDLINE,
    RTFSISOMAKERCMD_OPT_HPPA_KERNEL_32,
    RTFSISOMAKERCMD_OPT_HPPA_KERNEL_64,
    RTFSISOMAKERCMD_OPT_HPPA_RAMDISK,
    RTFSISOMAKERCMD_OPT_INPUT_CHARSET,
    RTFSISOMAKERCMD_OPT_ISO_LEVEL,
    RTFSISOMAKERCMD_OPT_JIGDO_COMPRESS,
    RTFSISOMAKERCMD_OPT_JIGDO_EXCLUDE,
    RTFSISOMAKERCMD_OPT_JIGDO_FORCE_MD5,
    RTFSISOMAKERCMD_OPT_JIGDO_JIGDO,
    RTFSISOMAKERCMD_OPT_JIGDO_MAP,
    RTFSISOMAKERCMD_OPT_JIGDO_MD5_LIST,
    RTFSISOMAKERCMD_OPT_JIGDO_MIN_FILE_SIZE,
    RTFSISOMAKERCMD_OPT_JIGDO_TEMPLATE,
    RTFSISOMAKERCMD_OPT_JOLIET_CHARSET,
    RTFSISOMAKERCMD_OPT_JOLIET_LEVEL,
    RTFSISOMAKERCMD_OPT_JOLIET_LONG,
    RTFSISOMAKERCMD_OPT_LOG_FILE,
    RTFSISOMAKERCMD_OPT_MAX_ISO9660_FILENAMES,
    RTFSISOMAKERCMD_OPT_MIPS_BOOT,
    RTFSISOMAKERCMD_OPT_MIPSEL_BOOT,
    RTFSISOMAKERCMD_OPT_NEW_DIR_MODE,
    RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,
    RTFSISOMAKERCMD_OPT_NO_DETECT_HARDLINKS,
    RTFSISOMAKERCMD_OPT_NO_ISO_TRANSLATE,
    RTFSISOMAKERCMD_OPT_NO_PAD,
    RTFSISOMAKERCMD_OPT_NO_RR,
    RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_COMPONENTS,
    RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_FIELDS,
    RTFSISOMAKERCMD_OPT_OLD_ROOT,
    RTFSISOMAKERCMD_OPT_OUTPUT_CHARSET,
    RTFSISOMAKERCMD_OPT_PAD,
    RTFSISOMAKERCMD_OPT_PATH_LIST,
    RTFSISOMAKERCMD_OPT_PRINT_SIZE,
    RTFSISOMAKERCMD_OPT_QUIET,
    RTFSISOMAKERCMD_OPT_RELAXED_FILENAMES,
    RTFSISOMAKERCMD_OPT_ROOT,
    RTFSISOMAKERCMD_OPT_SORT,
    RTFSISOMAKERCMD_OPT_SPARC_BOOT,
    RTFSISOMAKERCMD_OPT_SPARC_LABEL,
    RTFSISOMAKERCMD_OPT_SPLIT_OUTPUT,
    RTFSISOMAKERCMD_OPT_STREAM_FILE_NAME,
    RTFSISOMAKERCMD_OPT_STREAM_MEDIA_SIZE,
    RTFSISOMAKERCMD_OPT_SUNX86_BOOT,
    RTFSISOMAKERCMD_OPT_SUNX86_LABEL,
    RTFSISOMAKERCMD_OPT_SYSTEM_ID,
    RTFSISOMAKERCMD_OPT_TRANS_TBL_NAME,
    RTFSISOMAKERCMD_OPT_UDF,
    RTFSISOMAKERCMD_OPT_UID,
    RTFSISOMAKERCMD_OPT_USE_FILE_VERSION,
    RTFSISOMAKERCMD_OPT_VOLUME_SET_ID,
    RTFSISOMAKERCMD_OPT_VOLUME_SET_SEQ_NO,
    RTFSISOMAKERCMD_OPT_VOLUME_SET_SIZE,
    RTFSISOMAKERCMD_OPT_END
} RTFSISOMAKERCMDOPT;

/**
 * ISO maker command options & state.
 */
typedef struct RTFSISOMAKERCMDOPTS
{
    /** The handle to the ISO maker. */
    RTFSISOMAKER        hIsoMaker;
    /** Set if we're creating a virtual image maker, i.e. producing something
     * that is going to be read from only and not written to disk. */
    bool                fVirtualImageMaker;
    /** Extended error info.  This is a stderr alternative for the
     *  fVirtualImageMaker case (stdout goes to LogRel). */
    PRTERRINFO          pErrInfo;

    /** The output file.
     * This is NULL when fVirtualImageMaker is set. */
    const char         *pszOutFile;
    /** Special buffer size to use for testing the ISO maker code reading. */
    uint32_t            cbOutputReadBuffer;
    /** Use random output read buffer size.  cbOutputReadBuffer works as maximum
     * when this is enabled. */
    bool                fRandomOutputReadBufferSize;


    /** Number of items (files, directories, images, whatever) we've added. */
    uint32_t            cItemsAdded;
} RTFSISOMAKERCMDOPTS;
typedef RTFSISOMAKERCMDOPTS *PRTFSISOMAKERCMDOPTS;
typedef RTFSISOMAKERCMDOPTS const *PCRTFSISOMAKERCMDOPTS;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/*
 * Parse the command line.  This is similar to genisoimage and mkisofs,
 * thus the single dash long name aliases.
 */
static const RTGETOPTDEF g_aRtFsIsoMakerOptions[] =
{
    /*
     * Unquie IPRT ISO maker options.
     */
    { "--iprt-iso-maker-file-marker",   RTFSISOMAKERCMD_OPT_IPRT_ISO_MAKER_FILE_MARKER,     RTGETOPT_REQ_NOTHING },
    { "--output-buffer-size",           RTFSISOMAKERCMD_OPT_OUTPUT_BUFFER_SIZE,             RTGETOPT_REQ_UINT32  },
    { "--random-output-buffer-size",    RTFSISOMAKERCMD_OPT_RANDOM_OUTPUT_BUFFER_SIZE,      RTGETOPT_REQ_NOTHING },


    /*
     * genisoimage/mkisofs compatibility:
     */
    { "--abstract",                     RTFSISOMAKERCMD_OPT_ABSTRACT_FILE_ID,               RTGETOPT_REQ_STRING  },
    { "-abstract",                      RTFSISOMAKERCMD_OPT_ABSTRACT_FILE_ID,               RTGETOPT_REQ_STRING  },
    { "--application-id",               'A',                                                RTGETOPT_REQ_STRING  },
    { "--allow-limited-size",           RTFSISOMAKERCMD_OPT_ALLOW_LIMITED_SIZE,             RTGETOPT_REQ_NOTHING },
    { "-allow-limited-size",            RTFSISOMAKERCMD_OPT_ALLOW_LIMITED_SIZE,             RTGETOPT_REQ_NOTHING },
    { "--allow-leading-dots",           RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING },
    { "-allow-leading-dots",            RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING },
    { "--ldots",                        RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING },
    { "-ldots",                         RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING },
    { "--allow-lowercase",              RTFSISOMAKERCMD_OPT_ALLOW_LOWERCASE,                RTGETOPT_REQ_NOTHING },
    { "-allow-lowercase",               RTFSISOMAKERCMD_OPT_ALLOW_LOWERCASE,                RTGETOPT_REQ_NOTHING },
    { "--allow-multidot",               RTFSISOMAKERCMD_OPT_ALLOW_MULTI_DOT,                RTGETOPT_REQ_NOTHING },
    { "-allow-multidot",                RTFSISOMAKERCMD_OPT_ALLOW_MULTI_DOT,                RTGETOPT_REQ_NOTHING },
    { "--biblio",                       RTFSISOMAKERCMD_OPT_BIBLIOGRAPHIC_FILE_ID,          RTGETOPT_REQ_STRING  },
    { "-biblio",                        RTFSISOMAKERCMD_OPT_BIBLIOGRAPHIC_FILE_ID,          RTGETOPT_REQ_STRING  },
    { "--cache-inodes",                 RTFSISOMAKERCMD_OPT_DETECT_HARDLINKS,               RTGETOPT_REQ_NOTHING },
    { "-cache-inodes",                  RTFSISOMAKERCMD_OPT_DETECT_HARDLINKS,               RTGETOPT_REQ_NOTHING },
    { "--no-cache-inodes",              RTFSISOMAKERCMD_OPT_NO_DETECT_HARDLINKS,            RTGETOPT_REQ_NOTHING },
    { "-no-cache-inodes",               RTFSISOMAKERCMD_OPT_NO_DETECT_HARDLINKS,            RTGETOPT_REQ_NOTHING },
    { "--alpha-boot",                   RTFSISOMAKERCMD_OPT_ALPHA_BOOT,                     RTGETOPT_REQ_STRING  },
    { "-alpha-boot",                    RTFSISOMAKERCMD_OPT_ALPHA_BOOT,                     RTGETOPT_REQ_STRING  },
    { "--hppa-bootloader",              RTFSISOMAKERCMD_OPT_HPPA_BOOTLOADER,                RTGETOPT_REQ_STRING  },
    { "-hppa-bootloader",               RTFSISOMAKERCMD_OPT_HPPA_BOOTLOADER,                RTGETOPT_REQ_STRING  },
    { "--hppa-cmdline",                 RTFSISOMAKERCMD_OPT_HPPA_CMDLINE,                   RTGETOPT_REQ_STRING  },
    { "-hppa-cmdline",                  RTFSISOMAKERCMD_OPT_HPPA_CMDLINE,                   RTGETOPT_REQ_STRING  },
    { "--hppa-kernel-32",               RTFSISOMAKERCMD_OPT_HPPA_KERNEL_32,                 RTGETOPT_REQ_STRING  },
    { "-hppa-kernel-32",                RTFSISOMAKERCMD_OPT_HPPA_KERNEL_32,                 RTGETOPT_REQ_STRING  },
    { "--hppa-kernel-64",               RTFSISOMAKERCMD_OPT_HPPA_KERNEL_64,                 RTGETOPT_REQ_STRING  },
    { "-hppa-kernel-64",                RTFSISOMAKERCMD_OPT_HPPA_KERNEL_64,                 RTGETOPT_REQ_STRING  },
    { "--hppa-ramdisk",                 RTFSISOMAKERCMD_OPT_HPPA_RAMDISK,                   RTGETOPT_REQ_STRING  },
    { "-hppa-ramdisk",                  RTFSISOMAKERCMD_OPT_HPPA_RAMDISK,                   RTGETOPT_REQ_STRING  },
    { "--mips-boot",                    RTFSISOMAKERCMD_OPT_MIPS_BOOT,                      RTGETOPT_REQ_STRING  },
    { "-mips-boot",                     RTFSISOMAKERCMD_OPT_MIPS_BOOT,                      RTGETOPT_REQ_STRING  },
    { "--mipsel-boot",                  RTFSISOMAKERCMD_OPT_MIPSEL_BOOT,                    RTGETOPT_REQ_STRING  },
    { "-mipsel-boot",                   RTFSISOMAKERCMD_OPT_MIPSEL_BOOT,                    RTGETOPT_REQ_STRING  },
    { "--sparc-boot",                   'B',                                                RTGETOPT_REQ_STRING  },
    { "-sparc-boot",                    'B',                                                RTGETOPT_REQ_STRING  },
    { "--generic-boot",                 'G',                                                RTGETOPT_REQ_STRING  },
    { "--eltorito-boot",                'b',                                                RTGETOPT_REQ_STRING  },
    { "--eltorito-alt-boot",            RTFSISOMAKERCMD_OPT_ELTORITO_ALT_BOOT,              RTGETOPT_REQ_NOTHING },
    { "--hard-disk-boot",               RTFSISOMAKERCMD_OPT_ELTORITO_HARD_DISK_BOOT,        RTGETOPT_REQ_NOTHING },
    { "-hard-disk-boot",                RTFSISOMAKERCMD_OPT_ELTORITO_HARD_DISK_BOOT,        RTGETOPT_REQ_NOTHING },
    { "--no-emulation-boot",            RTFSISOMAKERCMD_OPT_ELTORITO_NO_EMULATION_BOOT,     RTGETOPT_REQ_NOTHING },
    { "-no-emulation-boot",             RTFSISOMAKERCMD_OPT_ELTORITO_NO_EMULATION_BOOT,     RTGETOPT_REQ_NOTHING },
    { "--no-boot",                      RTFSISOMAKERCMD_OPT_ELTORITO_NO_BOOT,               RTGETOPT_REQ_NOTHING },
    { "-no-boot",                       RTFSISOMAKERCMD_OPT_ELTORITO_NO_BOOT,               RTGETOPT_REQ_NOTHING },
    { "--boot-load-seg",                RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SEG,              RTGETOPT_REQ_UINT32  },
    { "-boot-load-seg",                 RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SEG,              RTGETOPT_REQ_UINT32  },
    { "--boot-load-size",               RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SIZE,             RTGETOPT_REQ_UINT32  },
    { "-boot-load-size",                RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SIZE,             RTGETOPT_REQ_UINT32  },
    { "--boot-info-table",              RTFSISOMAKERCMD_OPT_ELTORITO_INFO_TABLE,            RTGETOPT_REQ_STRING  },
    { "-boot-info-table",               RTFSISOMAKERCMD_OPT_ELTORITO_INFO_TABLE,            RTGETOPT_REQ_STRING  },
    { "--cd-extra",                     'C',                                                RTGETOPT_REQ_STRING  },
    { "--boot-catalog",                 'c',                                                RTGETOPT_REQ_STRING  },
    { "--check-oldnames",               RTFSISOMAKERCMD_OPT_CHECK_OLD_NAMES,                RTGETOPT_REQ_NOTHING },
    { "-check-oldnames",                RTFSISOMAKERCMD_OPT_CHECK_OLD_NAMES,                RTGETOPT_REQ_NOTHING },
    { "--check-session",                RTFSISOMAKERCMD_OPT_CHECK_SESSION,                  RTGETOPT_REQ_STRING  },
    { "-check-session",                 RTFSISOMAKERCMD_OPT_CHECK_SESSION,                  RTGETOPT_REQ_STRING  },
    { "--copyright",                    RTFSISOMAKERCMD_OPT_COPYRIGHT_FILE_ID,              RTGETOPT_REQ_STRING  },
    { "-copyright",                     RTFSISOMAKERCMD_OPT_COPYRIGHT_FILE_ID,              RTGETOPT_REQ_STRING  },
    { "--dont-append-dot",              'd',                                                RTGETOPT_REQ_NOTHING },
    { "--deep-directories",             'D',                                                RTGETOPT_REQ_NOTHING },
    { "--dir-mode",                     RTFSISOMAKERCMD_OPT_DIR_MODE,                       RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "-dir-mode",                      RTFSISOMAKERCMD_OPT_DIR_MODE,                       RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "--dvd-video",                    RTFSISOMAKERCMD_OPT_DVD_VIDEO,                      RTGETOPT_REQ_NOTHING },
    { "-dvd-video",                     RTFSISOMAKERCMD_OPT_DVD_VIDEO,                      RTGETOPT_REQ_NOTHING },
    { "--follow-symlinks",              'f',                                                RTGETOPT_REQ_NOTHING },
    { "-follow-symlinks",               'f',                                                RTGETOPT_REQ_NOTHING },
    { "--file-mode",                    RTFSISOMAKERCMD_OPT_FILE_MODE,                      RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "-file-mode",                     RTFSISOMAKERCMD_OPT_FILE_MODE,                      RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "--gid",                          RTFSISOMAKERCMD_OPT_GID,                            RTGETOPT_REQ_UINT32  },
    { "-gid",                           RTFSISOMAKERCMD_OPT_GID,                            RTGETOPT_REQ_UINT32  },
    { "--gui",                          RTFSISOMAKERCMD_OPT_GUI,                            RTGETOPT_REQ_NOTHING },
    { "-gui",                           RTFSISOMAKERCMD_OPT_GUI,                            RTGETOPT_REQ_NOTHING },
    { "--graft-points",                 RTFSISOMAKERCMD_OPT_GRAFT_POINTS,                   RTGETOPT_REQ_NOTHING },
    { "-graft-points",                  RTFSISOMAKERCMD_OPT_GRAFT_POINTS,                   RTGETOPT_REQ_NOTHING },
    { "--hide",                         RTFSISOMAKERCMD_OPT_HIDE,                           RTGETOPT_REQ_STRING  },
    { "-hide",                          RTFSISOMAKERCMD_OPT_HIDE,                           RTGETOPT_REQ_STRING  },
    { "--hide-list",                    RTFSISOMAKERCMD_OPT_HIDE_LIST,                      RTGETOPT_REQ_STRING  },
    { "-hide-list",                     RTFSISOMAKERCMD_OPT_HIDE_LIST,                      RTGETOPT_REQ_STRING  },
    { "--hidden",                       RTFSISOMAKERCMD_OPT_HIDDEN,                         RTGETOPT_REQ_STRING  },
    { "-hidden",                        RTFSISOMAKERCMD_OPT_HIDDEN,                         RTGETOPT_REQ_STRING  },
    { "--hidden-list",                  RTFSISOMAKERCMD_OPT_HIDDEN_LIST,                    RTGETOPT_REQ_STRING  },
    { "-hidden-list",                   RTFSISOMAKERCMD_OPT_HIDDEN_LIST,                    RTGETOPT_REQ_STRING  },
    { "--hide-joliet",                  RTFSISOMAKERCMD_OPT_HIDE_JOLIET,                    RTGETOPT_REQ_STRING  },
    { "-hide-joliet",                   RTFSISOMAKERCMD_OPT_HIDE_JOLIET,                    RTGETOPT_REQ_STRING  },
    { "--hide-joliet-list",             RTFSISOMAKERCMD_OPT_HIDE_JOLIET_LIST,               RTGETOPT_REQ_STRING  },
    { "-hide-joliet-list",              RTFSISOMAKERCMD_OPT_HIDE_JOLIET_LIST,               RTGETOPT_REQ_STRING  },
    { "--hide-joliet-trans-tbl",        RTFSISOMAKERCMD_OPT_HIDE_JOLIET_TRANS_TBL,          RTGETOPT_REQ_NOTHING },
    { "-hide-joliet-trans-tbl",         RTFSISOMAKERCMD_OPT_HIDE_JOLIET_TRANS_TBL,          RTGETOPT_REQ_NOTHING },
    { "--hide-rr-moved",                RTFSISOMAKERCMD_OPT_HIDE_RR_MOVED,                  RTGETOPT_REQ_NOTHING },
    { "-hide-rr-moved",                 RTFSISOMAKERCMD_OPT_HIDE_RR_MOVED,                  RTGETOPT_REQ_NOTHING },
    { "--input-charset",                RTFSISOMAKERCMD_OPT_INPUT_CHARSET,                  RTGETOPT_REQ_STRING  },
    { "-input-charset",                 RTFSISOMAKERCMD_OPT_INPUT_CHARSET,                  RTGETOPT_REQ_STRING  },
    { "--output-charset",               RTFSISOMAKERCMD_OPT_OUTPUT_CHARSET,                 RTGETOPT_REQ_STRING  },
    { "-output-charset",                RTFSISOMAKERCMD_OPT_OUTPUT_CHARSET,                 RTGETOPT_REQ_STRING  },
    { "--iso-level",                    RTFSISOMAKERCMD_OPT_ISO_LEVEL,                      RTGETOPT_REQ_UINT8   },
    { "--joliet",                       'J',                                                RTGETOPT_REQ_NOTHING },
    { "--joliet-long",                  RTFSISOMAKERCMD_OPT_JOLIET_LONG,                    RTGETOPT_REQ_NOTHING },
    { "-joliet-long",                   RTFSISOMAKERCMD_OPT_JOLIET_LONG,                    RTGETOPT_REQ_NOTHING },
    { "--jcharset",                     RTFSISOMAKERCMD_OPT_JOLIET_CHARSET,                 RTGETOPT_REQ_STRING  },
    { "-jcharset",                      RTFSISOMAKERCMD_OPT_JOLIET_CHARSET,                 RTGETOPT_REQ_STRING  },
    { "--long-names",                   'l',                                                RTGETOPT_REQ_NOTHING },
    { "--leading-dot",                  'L',                                                RTGETOPT_REQ_NOTHING },
    { "--jigdo-jigdo",                  RTFSISOMAKERCMD_OPT_JIGDO_JIGDO,                    RTGETOPT_REQ_STRING  },
    { "-jigdo-jigdo",                   RTFSISOMAKERCMD_OPT_JIGDO_JIGDO,                    RTGETOPT_REQ_STRING  },
    { "--jigdo-template",               RTFSISOMAKERCMD_OPT_JIGDO_TEMPLATE,                 RTGETOPT_REQ_STRING  },
    { "-jigdo-template",                RTFSISOMAKERCMD_OPT_JIGDO_TEMPLATE,                 RTGETOPT_REQ_STRING  },
    { "--jigdo-min-file-size",          RTFSISOMAKERCMD_OPT_JIGDO_MIN_FILE_SIZE,            RTGETOPT_REQ_UINT64  },
    { "-jigdo-min-file-size",           RTFSISOMAKERCMD_OPT_JIGDO_MIN_FILE_SIZE,            RTGETOPT_REQ_UINT64  },
    { "--jigdo-force-md5",              RTFSISOMAKERCMD_OPT_JIGDO_FORCE_MD5,                RTGETOPT_REQ_STRING  },
    { "-jigdo-force-md5",               RTFSISOMAKERCMD_OPT_JIGDO_FORCE_MD5,                RTGETOPT_REQ_STRING  },
    { "--jigdo-exclude",                RTFSISOMAKERCMD_OPT_JIGDO_EXCLUDE,                  RTGETOPT_REQ_STRING  },
    { "-jigdo-exclude",                 RTFSISOMAKERCMD_OPT_JIGDO_EXCLUDE,                  RTGETOPT_REQ_STRING  },
    { "--jigdo-map",                    RTFSISOMAKERCMD_OPT_JIGDO_MAP,                      RTGETOPT_REQ_STRING  },
    { "-jigdo-map",                     RTFSISOMAKERCMD_OPT_JIGDO_MAP,                      RTGETOPT_REQ_STRING  },
    { "--md5-list",                     RTFSISOMAKERCMD_OPT_JIGDO_MD5_LIST,                 RTGETOPT_REQ_STRING  },
    { "-md5-list",                      RTFSISOMAKERCMD_OPT_JIGDO_MD5_LIST,                 RTGETOPT_REQ_STRING  },
    { "--jigdo-template-compress",      RTFSISOMAKERCMD_OPT_JIGDO_COMPRESS,                 RTGETOPT_REQ_STRING  },
    { "-jigdo-template-compress",       RTFSISOMAKERCMD_OPT_JIGDO_COMPRESS,                 RTGETOPT_REQ_STRING  },
    { "--log-file",                     RTFSISOMAKERCMD_OPT_LOG_FILE,                       RTGETOPT_REQ_STRING  },
    { "-log-file",                      RTFSISOMAKERCMD_OPT_LOG_FILE,                       RTGETOPT_REQ_STRING  },
    { "--exclude",                      'm',                                                RTGETOPT_REQ_STRING  },
    { "--exclude",                      'x',                                                RTGETOPT_REQ_STRING  },
    { "--exclude-list",                 RTFSISOMAKERCMD_OPT_EXCLUDE_LIST,                   RTGETOPT_REQ_STRING  },
    { "-exclude-list",                  RTFSISOMAKERCMD_OPT_EXCLUDE_LIST,                   RTGETOPT_REQ_STRING  },
    { "--max-iso9660-filenames",        RTFSISOMAKERCMD_OPT_MAX_ISO9660_FILENAMES,          RTGETOPT_REQ_NOTHING },
    { "-max-iso9660-filenames",         RTFSISOMAKERCMD_OPT_MAX_ISO9660_FILENAMES,          RTGETOPT_REQ_NOTHING },
    { "--merge",                        'M',                                                RTGETOPT_REQ_STRING  },
    { "--dev",                          'M',                                                RTGETOPT_REQ_STRING  },
    { "-dev",                           'M',                                                RTGETOPT_REQ_STRING  },
    { "--omit-version-numbers",         'N',                                                RTGETOPT_REQ_NOTHING },
    { "--new-dir-mode",                 RTFSISOMAKERCMD_OPT_NEW_DIR_MODE,                   RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "-new-dir-mode",                  RTFSISOMAKERCMD_OPT_NEW_DIR_MODE,                   RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT },
    { "--nobak",                        RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING },
    { "-nobak",                         RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING },
    { "--no-bak",                       RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING },
    { "-no-bak",                        RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING },
    { "--force-rr",                     RTFSISOMAKERCMD_OPT_FORCE_RR,                       RTGETOPT_REQ_NOTHING },
    { "-force-rr",                      RTFSISOMAKERCMD_OPT_FORCE_RR,                       RTGETOPT_REQ_NOTHING },
    { "--no-rr",                        RTFSISOMAKERCMD_OPT_NO_RR,                          RTGETOPT_REQ_NOTHING },
    { "-no-rr",                         RTFSISOMAKERCMD_OPT_NO_RR,                          RTGETOPT_REQ_NOTHING },
    { "--no-split-symlink-components",  RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_COMPONENTS,    RTGETOPT_REQ_NOTHING },
    { "-no-split-symlink-components",   RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_COMPONENTS,    RTGETOPT_REQ_NOTHING },
    { "--no-split-symlink-fields",      RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_FIELDS,        RTGETOPT_REQ_NOTHING },
    { "-no-split-symlink-fields",       RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_FIELDS,        RTGETOPT_REQ_NOTHING },
    { "--output",                       'o',                                                RTGETOPT_REQ_STRING  },
    { "--pad",                          RTFSISOMAKERCMD_OPT_PAD,                            RTGETOPT_REQ_NOTHING },
    { "-pad",                           RTFSISOMAKERCMD_OPT_PAD,                            RTGETOPT_REQ_NOTHING },
    { "--no-pad",                       RTFSISOMAKERCMD_OPT_NO_PAD,                         RTGETOPT_REQ_NOTHING },
    { "-no-pad",                        RTFSISOMAKERCMD_OPT_NO_PAD,                         RTGETOPT_REQ_NOTHING },
    { "--path-list",                    RTFSISOMAKERCMD_OPT_PATH_LIST,                      RTGETOPT_REQ_STRING  },
    { "-path-list",                     RTFSISOMAKERCMD_OPT_PATH_LIST,                      RTGETOPT_REQ_STRING  },
    { "--publisher",                    'P',                                                RTGETOPT_REQ_STRING  },
    { "-publisher",                     'P',                                                RTGETOPT_REQ_STRING  },
    { "--preparer",                     'p',                                                RTGETOPT_REQ_STRING  },
    { "--print-size",                   RTFSISOMAKERCMD_OPT_PRINT_SIZE,                     RTGETOPT_REQ_NOTHING },
    { "-print-size",                    RTFSISOMAKERCMD_OPT_PRINT_SIZE,                     RTGETOPT_REQ_NOTHING },
    { "--quiet",                        RTFSISOMAKERCMD_OPT_QUIET,                          RTGETOPT_REQ_NOTHING },
    { "-quiet",                         RTFSISOMAKERCMD_OPT_QUIET,                          RTGETOPT_REQ_NOTHING },
    { "--rock-ridge",                   'R',                                                RTGETOPT_REQ_NOTHING },
    { "--rock-ridge-relaxed",           'r',                                                RTGETOPT_REQ_NOTHING },
    { "--relaxed-filenames",            RTFSISOMAKERCMD_OPT_RELAXED_FILENAMES,              RTGETOPT_REQ_NOTHING },
    { "-relaxed-filenames",             RTFSISOMAKERCMD_OPT_RELAXED_FILENAMES,              RTGETOPT_REQ_NOTHING },
    { "--root",                         RTFSISOMAKERCMD_OPT_ROOT,                           RTGETOPT_REQ_STRING  },
    { "-root",                          RTFSISOMAKERCMD_OPT_ROOT,                           RTGETOPT_REQ_STRING  },
    { "--old-root",                     RTFSISOMAKERCMD_OPT_OLD_ROOT,                       RTGETOPT_REQ_STRING  },
    { "-old-root",                      RTFSISOMAKERCMD_OPT_OLD_ROOT,                       RTGETOPT_REQ_STRING  },
    { "--sort",                         RTFSISOMAKERCMD_OPT_SORT,                           RTGETOPT_REQ_STRING  },
    { "-sort",                          RTFSISOMAKERCMD_OPT_SORT,                           RTGETOPT_REQ_STRING  },
    { "--sparc-boot",                   RTFSISOMAKERCMD_OPT_SPARC_BOOT,                     RTGETOPT_REQ_STRING  },
    { "-sparc-boot",                    RTFSISOMAKERCMD_OPT_SPARC_BOOT,                     RTGETOPT_REQ_STRING  },
    { "--sparc-label",                  RTFSISOMAKERCMD_OPT_SPARC_LABEL,                    RTGETOPT_REQ_STRING  },
    { "-sparc-label",                   RTFSISOMAKERCMD_OPT_SPARC_LABEL,                    RTGETOPT_REQ_STRING  },
    { "--split-output",                 RTFSISOMAKERCMD_OPT_SPLIT_OUTPUT,                   RTGETOPT_REQ_NOTHING },
    { "-split-output",                  RTFSISOMAKERCMD_OPT_SPLIT_OUTPUT,                   RTGETOPT_REQ_NOTHING },
    { "--stream-media-size",            RTFSISOMAKERCMD_OPT_STREAM_MEDIA_SIZE,              RTGETOPT_REQ_UINT64  },
    { "-stream-media-size",             RTFSISOMAKERCMD_OPT_STREAM_MEDIA_SIZE,              RTGETOPT_REQ_UINT64  },
    { "--stream-file-name",             RTFSISOMAKERCMD_OPT_STREAM_FILE_NAME,               RTGETOPT_REQ_STRING  },
    { "-stream-file-name",              RTFSISOMAKERCMD_OPT_STREAM_FILE_NAME,               RTGETOPT_REQ_STRING  },
    { "--sunx86-boot",                  RTFSISOMAKERCMD_OPT_SUNX86_BOOT,                    RTGETOPT_REQ_STRING  },
    { "-sunx86-boot",                   RTFSISOMAKERCMD_OPT_SUNX86_BOOT,                    RTGETOPT_REQ_STRING  },
    { "--sunx86-label",                 RTFSISOMAKERCMD_OPT_SUNX86_LABEL,                   RTGETOPT_REQ_STRING  },
    { "-sunx86-label",                  RTFSISOMAKERCMD_OPT_SUNX86_LABEL,                   RTGETOPT_REQ_STRING  },
    { "--sysid",                        RTFSISOMAKERCMD_OPT_SYSTEM_ID,                      RTGETOPT_REQ_STRING  },
    { "-sysid",                         RTFSISOMAKERCMD_OPT_SYSTEM_ID,                      RTGETOPT_REQ_STRING  },
    { "--trans-tbl",                    'T',                                                RTGETOPT_REQ_NOTHING },
    { "--table-name",                   RTFSISOMAKERCMD_OPT_TRANS_TBL_NAME,                 RTGETOPT_REQ_STRING  },
    { "-table-name",                    RTFSISOMAKERCMD_OPT_TRANS_TBL_NAME,                 RTGETOPT_REQ_STRING  },
    { "--ucs-level",                    RTFSISOMAKERCMD_OPT_JOLIET_LEVEL,                   RTGETOPT_REQ_UINT8   },
    { "-ucs-level",                     RTFSISOMAKERCMD_OPT_JOLIET_LEVEL,                   RTGETOPT_REQ_UINT8   },
    { "--udf",                          RTFSISOMAKERCMD_OPT_UDF,                            RTGETOPT_REQ_NOTHING },
    { "-udf",                           RTFSISOMAKERCMD_OPT_UDF,                            RTGETOPT_REQ_NOTHING },
    { "--uid",                          RTFSISOMAKERCMD_OPT_UID,                            RTGETOPT_REQ_UINT32  },
    { "-uid",                           RTFSISOMAKERCMD_OPT_UID,                            RTGETOPT_REQ_UINT32  },
    { "--use-fileversion",              RTFSISOMAKERCMD_OPT_USE_FILE_VERSION,               RTGETOPT_REQ_NOTHING },
    { "-use-fileversion",               RTFSISOMAKERCMD_OPT_USE_FILE_VERSION,               RTGETOPT_REQ_NOTHING },
    { "--untranslated-filenames",       'U',                                                RTGETOPT_REQ_NOTHING },
    { "--no-iso-translate",             RTFSISOMAKERCMD_OPT_NO_ISO_TRANSLATE,               RTGETOPT_REQ_NOTHING },
    { "-no-iso-translate",              RTFSISOMAKERCMD_OPT_NO_ISO_TRANSLATE,               RTGETOPT_REQ_NOTHING },
    { "--volume-id",                    'V',                                                RTGETOPT_REQ_STRING  },
    { "--volset",                       RTFSISOMAKERCMD_OPT_VOLUME_SET_ID,                  RTGETOPT_REQ_STRING  },
    { "-volset",                        RTFSISOMAKERCMD_OPT_VOLUME_SET_ID,                  RTGETOPT_REQ_STRING  },
    { "--volset-size",                  RTFSISOMAKERCMD_OPT_VOLUME_SET_SIZE,                RTGETOPT_REQ_UINT32  },
    { "-volset-size",                   RTFSISOMAKERCMD_OPT_VOLUME_SET_SIZE,                RTGETOPT_REQ_UINT32  },
    { "--volset-seqno",                 RTFSISOMAKERCMD_OPT_VOLUME_SET_SEQ_NO,              RTGETOPT_REQ_UINT32  },
    { "-volset-seqno",                  RTFSISOMAKERCMD_OPT_VOLUME_SET_SEQ_NO,              RTGETOPT_REQ_UINT32  },
    { "--transpared-compression",       'z',                                                RTGETOPT_REQ_NOTHING },

    /* HFS and ISO-9660 apple extensions. */
    { "--hfs",                          RTFSISOMAKERCMD_OPT_HFS_ENABLE,                     RTGETOPT_REQ_NOTHING },
    { "-hfs",                           RTFSISOMAKERCMD_OPT_HFS_ENABLE,                     RTGETOPT_REQ_NOTHING },
    { "--apple",                        RTFSISOMAKERCMD_OPT_APPLE,                          RTGETOPT_REQ_NOTHING },
    { "-apple",                         RTFSISOMAKERCMD_OPT_APPLE,                          RTGETOPT_REQ_NOTHING },
    { "--map",                          RTFSISOMAKERCMD_OPT_HFS_MAP,                        RTGETOPT_REQ_STRING  },
    { "-map",                           RTFSISOMAKERCMD_OPT_HFS_MAP,                        RTGETOPT_REQ_STRING  },
    { "--magic",                        RTFSISOMAKERCMD_OPT_HFS_MAGIC,                      RTGETOPT_REQ_STRING  },
    { "-magic",                         RTFSISOMAKERCMD_OPT_HFS_MAGIC,                      RTGETOPT_REQ_STRING  },
    { "--hfs-creator",                  RTFSISOMAKERCMD_OPT_HFS_CREATOR,                    RTGETOPT_REQ_STRING  },
    { "-hfs-creator",                   RTFSISOMAKERCMD_OPT_HFS_CREATOR,                    RTGETOPT_REQ_STRING  },
    { "--hfs-type",                     RTFSISOMAKERCMD_OPT_HFS_TYPE,                       RTGETOPT_REQ_STRING  },
    { "-hfs-type",                      RTFSISOMAKERCMD_OPT_HFS_TYPE,                       RTGETOPT_REQ_STRING  },
    { "--probe",                        RTFSISOMAKERCMD_OPT_HFS_PROBE,                      RTGETOPT_REQ_NOTHING },
    { "-probe",                         RTFSISOMAKERCMD_OPT_HFS_PROBE,                      RTGETOPT_REQ_NOTHING },
    { "--no-desktop",                   RTFSISOMAKERCMD_OPT_HFS_NO_DESKTOP,                 RTGETOPT_REQ_NOTHING },
    { "-no-desktop",                    RTFSISOMAKERCMD_OPT_HFS_NO_DESKTOP,                 RTGETOPT_REQ_NOTHING },
    { "--mac-name",                     RTFSISOMAKERCMD_OPT_HFS_MAC_NAME,                   RTGETOPT_REQ_NOTHING },
    { "-mac-name",                      RTFSISOMAKERCMD_OPT_HFS_MAC_NAME,                   RTGETOPT_REQ_NOTHING },
    { "--boot-hfs-file",                RTFSISOMAKERCMD_OPT_HFS_BOOT_FILE,                  RTGETOPT_REQ_STRING  },
    { "-boot-hfs-file",                 RTFSISOMAKERCMD_OPT_HFS_BOOT_FILE,                  RTGETOPT_REQ_STRING  },
    { "--part",                         RTFSISOMAKERCMD_OPT_HFS_PART,                       RTGETOPT_REQ_NOTHING },
    { "-part",                          RTFSISOMAKERCMD_OPT_HFS_PART,                       RTGETOPT_REQ_NOTHING },
    { "--auto",                         RTFSISOMAKERCMD_OPT_HFS_AUTO,                       RTGETOPT_REQ_STRING  },
    { "-auto",                          RTFSISOMAKERCMD_OPT_HFS_AUTO,                       RTGETOPT_REQ_STRING  },
    { "--cluster-size",                 RTFSISOMAKERCMD_OPT_HFS_CLUSTER_SIZE,               RTGETOPT_REQ_UINT32  },
    { "-cluster-size",                  RTFSISOMAKERCMD_OPT_HFS_CLUSTER_SIZE,               RTGETOPT_REQ_UINT32  },
    { "--hide-hfs",                     RTFSISOMAKERCMD_OPT_HFS_HIDE,                       RTGETOPT_REQ_STRING  },
    { "-hide-hfs",                      RTFSISOMAKERCMD_OPT_HFS_HIDE,                       RTGETOPT_REQ_STRING  },
    { "--hide-hfs-list",                RTFSISOMAKERCMD_OPT_HFS_HIDE_LIST,                  RTGETOPT_REQ_STRING  },
    { "-hide-hfs-list",                 RTFSISOMAKERCMD_OPT_HFS_HIDE_LIST,                  RTGETOPT_REQ_STRING  },
    { "--hfs-volid",                    RTFSISOMAKERCMD_OPT_HFS_VOL_ID,                     RTGETOPT_REQ_STRING  },
    { "-hfs-volid",                     RTFSISOMAKERCMD_OPT_HFS_VOL_ID,                     RTGETOPT_REQ_STRING  },
    { "--icon-position",                RTFSISOMAKERCMD_OPT_HFS_ICON_POSITION,              RTGETOPT_REQ_NOTHING },
    { "-icon-position",                 RTFSISOMAKERCMD_OPT_HFS_ICON_POSITION,              RTGETOPT_REQ_NOTHING },
    { "--root-info",                    RTFSISOMAKERCMD_OPT_HFS_ROOT_INFO,                  RTGETOPT_REQ_STRING  },
    { "-root-info",                     RTFSISOMAKERCMD_OPT_HFS_ROOT_INFO,                  RTGETOPT_REQ_STRING  },
    { "--prep-boot",                    RTFSISOMAKERCMD_OPT_HFS_PREP_BOOT,                  RTGETOPT_REQ_STRING  },
    { "-prep-boot",                     RTFSISOMAKERCMD_OPT_HFS_PREP_BOOT,                  RTGETOPT_REQ_STRING  },
    { "--chrp-boot",                    RTFSISOMAKERCMD_OPT_HFS_CHRP_BOOT,                  RTGETOPT_REQ_NOTHING },
    { "-chrp-boot",                     RTFSISOMAKERCMD_OPT_HFS_CHRP_BOOT,                  RTGETOPT_REQ_NOTHING },
    { "--input-hfs-charset",            RTFSISOMAKERCMD_OPT_HFS_INPUT_CHARSET,              RTGETOPT_REQ_STRING  },
    { "-input-hfs-charset",             RTFSISOMAKERCMD_OPT_HFS_INPUT_CHARSET,              RTGETOPT_REQ_STRING  },
    { "--output-hfs-charset",           RTFSISOMAKERCMD_OPT_HFS_OUTPUT_CHARSET,             RTGETOPT_REQ_STRING  },
    { "-output-hfs-charset",            RTFSISOMAKERCMD_OPT_HFS_OUTPUT_CHARSET,             RTGETOPT_REQ_STRING  },
    { "--hfs-unlock",                   RTFSISOMAKERCMD_OPT_HFS_UNLOCK,                     RTGETOPT_REQ_NOTHING },
    { "-hfs-unlock",                    RTFSISOMAKERCMD_OPT_HFS_UNLOCK,                     RTGETOPT_REQ_NOTHING },
    { "--hfs-bless",                    RTFSISOMAKERCMD_OPT_HFS_BLESS,                      RTGETOPT_REQ_STRING  },
    { "-hfs-bless",                     RTFSISOMAKERCMD_OPT_HFS_BLESS,                      RTGETOPT_REQ_STRING  },
    { "--hfs-parms",                    RTFSISOMAKERCMD_OPT_HFS_PARMS,                      RTGETOPT_REQ_STRING  },
    { "-hfs-parms",                     RTFSISOMAKERCMD_OPT_HFS_PARMS,                      RTGETOPT_REQ_STRING  },
    { "--cap",                          RTFSISOMAKERCMD_OPT_HFS_CAP,                        RTGETOPT_REQ_NOTHING },
    { "--netatalk",                     RTFSISOMAKERCMD_OPT_HFS_NETATALK,                   RTGETOPT_REQ_NOTHING },
    { "--double",                       RTFSISOMAKERCMD_OPT_HFS_DOUBLE,                     RTGETOPT_REQ_NOTHING },
    { "--ethershare",                   RTFSISOMAKERCMD_OPT_HFS_ETHERSHARE,                 RTGETOPT_REQ_NOTHING },
    { "--ushare",                       RTFSISOMAKERCMD_OPT_HFS_USHARE,                     RTGETOPT_REQ_NOTHING },
    { "--exchange",                     RTFSISOMAKERCMD_OPT_HFS_EXCHANGE,                   RTGETOPT_REQ_NOTHING },
    { "--sgi",                          RTFSISOMAKERCMD_OPT_HFS_SGI,                        RTGETOPT_REQ_NOTHING },
    { "--xinet",                        RTFSISOMAKERCMD_OPT_HFS_XINET,                      RTGETOPT_REQ_NOTHING },
    { "--macbin",                       RTFSISOMAKERCMD_OPT_HFS_MACBIN,                     RTGETOPT_REQ_NOTHING },
    { "--single",                       RTFSISOMAKERCMD_OPT_HFS_SINGLE,                     RTGETOPT_REQ_NOTHING },
    { "--dave",                         RTFSISOMAKERCMD_OPT_HFS_DAVE,                       RTGETOPT_REQ_NOTHING },
    { "--sfm",                          RTFSISOMAKERCMD_OPT_HFS_SFM,                        RTGETOPT_REQ_NOTHING },
    { "--osx-double",                   RTFSISOMAKERCMD_OPT_HFS_OSX_DOUBLE,                 RTGETOPT_REQ_NOTHING },
    { "--osx-hfs",                      RTFSISOMAKERCMD_OPT_HFS_OSX_HFS,                    RTGETOPT_REQ_NOTHING },
};


/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV.
 *
 * @returns @a rc
 * @param   pOpts               The CMD instance.
 * @param   rc                  The return code.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFsIsoMakerErrorRc(PRTFSISOMAKERCMDOPTS pOpts, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pOpts->pErrInfo)
        RTErrInfoSetV(pOpts->pErrInfo, rc, pszFormat, va);
    else
        RTMsgErrorV(pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV for displaying syntax errors.
 *
 * @returns VERR_INVALID_PARAMETER
 * @param   pOpts               The CMD instance.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFsIsoMakerSyntaxError(PRTFSISOMAKERCMDOPTS pOpts, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pOpts->pErrInfo)
        RTErrInfoSetV(pOpts->pErrInfo, VERR_INVALID_PARAMETER, pszFormat, va);
    else
        RTMsgErrorV(pszFormat, va);
    va_end(va);
    return VERR_INVALID_PARAMETER;
}


/**
 * Wrapper around RTPrintfV / RTLogRelPrintfV.
 *
 * @param   pOpts               The CMD instance.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static void rtFsIsoMakerPrintf(PRTFSISOMAKERCMDOPTS pOpts, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pOpts->pErrInfo)
        RTLogRelPrintfV(pszFormat, va);
    else
        RTPrintfV(pszFormat, va);
    va_end(va);
}

/**
 * Deletes the state and returns @a rc.
 *
 * @returns @a rc.
 * @param   pOpts               The CMD instance to delete.
 * @param   rc                  The status code to return.
 */
static int rtFsIsoMakerCmdDeleteState(PRTFSISOMAKERCMDOPTS pOpts, int rc)
{
    if (pOpts->hIsoMaker != NIL_RTFSISOMAKER)
    {
        RTFsIsoMakerRelease(pOpts->hIsoMaker);
        pOpts->hIsoMaker = NIL_RTFSISOMAKER;
    }

    return rc;
}


static void rtFsIsoMakerCmdUsage(PRTFSISOMAKERCMDOPTS pOpts, const char *pszProgName)
{
    rtFsIsoMakerPrintf(pOpts, "usage: %s [options] <file>=<cdfile>\n", pszProgName);
}


/**
 * Writes the image to file.
 *
 * @returns IPRT status code.
 * @param   pOpts               The CMD instance.
 * @param   hVfsSrcFile         The source file from the ISO maker.
 */
static int rtFsIsoMakerCmdWriteImage(PRTFSISOMAKERCMDOPTS pOpts, RTVFSFILE hVfsSrcFile)
{
    /*
     * Get the image size and setup the copy buffer.
     */
    uint64_t cbImage;
    int rc = RTVfsFileGetSize(hVfsSrcFile, &cbImage);
    if (RT_SUCCESS(rc))
    {
        rtFsIsoMakerPrintf(pOpts, "Image size: %'RU64 (%#RX64) bytes\n", cbImage, cbImage);

        uint32_t cbBuf = pOpts->cbOutputReadBuffer == 0 ? _1M : pOpts->cbOutputReadBuffer;
        void    *pvBuf = RTMemTmpAlloc(cbBuf);
        if (pvBuf)
        {
            /*
             * Open the output file.
             */
            RTVFSFILE       hVfsDstFile;
            uint32_t        offError;
            RTERRINFOSTATIC ErrInfo;
            rc = RTVfsChainOpenFile(pOpts->pszOutFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE,
                                    &hVfsDstFile, &offError, RTErrInfoInitStatic(&ErrInfo));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Copy the virtual image bits to the destination file.
                 */
                uint64_t offImage = 0;
                while (offImage < cbImage)
                {
                    /* Figure out how much to copy this time. */
                    size_t cbToCopy = cbBuf;
                    if (pOpts->fRandomOutputReadBufferSize)
                        cbToCopy = RTRandU32Ex(1, (uint32_t)cbBuf - 1);
                    if (offImage + cbToCopy < cbImage)
                    { /* likely */ }
                    else
                        cbToCopy = (size_t)(cbImage - offImage);

                    /* Do the copying. */
                    rc = RTVfsFileReadAt(hVfsSrcFile, offImage, pvBuf, cbToCopy, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTVfsFileWriteAt(hVfsDstFile, offImage, pvBuf, cbToCopy, NULL);
                        if (RT_SUCCESS(rc))
                            offImage += cbToCopy;
                        else
                        {
                            rc = rtFsIsoMakerErrorRc(pOpts, rc, "Error %Rrc writing %#zx bytes at offset %#RX64 to '%s'",
                                                     rc, cbToCopy, offImage, pOpts->pszOutFile);
                            break;
                        }
                    }
                    else
                    {
                        rc = rtFsIsoMakerErrorRc(pOpts, rc, "Error %Rrc read %#zx bytes at offset %#RX64", rc, cbToCopy, offImage);
                        break;
                    }
                }

                /*
                 * Flush the output file before releasing it.
                 */
                if (RT_SUCCESS(rc))
                {
                    rc = RTVfsFileFlush(hVfsDstFile);
                    if (RT_FAILURE(rc))
                        rc = rtFsIsoMakerErrorRc(pOpts, rc, "RTVfsFileFlush failed on '%s': %Rrc", pOpts->pszOutFile, rc);
                }

                RTVfsFileRelease(hVfsDstFile);
            }
            else if (RTErrInfoIsSet(&ErrInfo.Core))
                rc = rtFsIsoMakerErrorRc(pOpts, rc, "RTVfsChainOpenFile(%s) failed: %Rrc - %s",
                                         pOpts->cbOutputReadBuffer, rc, ErrInfo.Core.pszMsg);
            else
                rc = rtFsIsoMakerErrorRc(pOpts, rc, "RTVfsChainOpenFile(%s) failed: %Rrc", pOpts->cbOutputReadBuffer, rc);

            RTMemTmpFree(pvBuf);
        }
        else
            rc = rtFsIsoMakerErrorRc(pOpts, rc, "RTMemTmpAlloc(%zu) failed: %Rrc", pOpts->cbOutputReadBuffer, rc);
    }
    else
        rc = rtFsIsoMakerErrorRc(pOpts, rc, "RTVfsFileGetSize failed: %Rrc", rc);
    return rc;
}



RTDECL(int) RTFsIsoMakerCmdEx(unsigned cArgs, char **papszArgs, PRTVFSFILE phVfsFile, PRTERRINFO pErrInfo)
{
    /*
     * Create instance.
     */
    RTFSISOMAKERCMDOPTS Opts;
    RT_ZERO(Opts);
    Opts.hIsoMaker              = NIL_RTFSISOMAKER;
    Opts.pErrInfo               = pErrInfo;
    Opts.fVirtualImageMaker     = phVfsFile != NULL;
    if (phVfsFile)
        *phVfsFile = NIL_RTVFSFILE;

    /* Setup option parsing. */
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, g_aRtFsIsoMakerOptions, RT_ELEMENTS(g_aRtFsIsoMakerOptions),
                          1 /*iFirst*/, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerErrorRc(&Opts, rc, "RTGetOpt failed: %Rrc", rc);

    /* Create the ISO creator instance. */
    rc = RTFsIsoMakerCreate(&Opts.hIsoMaker);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerErrorRc(&Opts, rc, "RTFsIsoMakerCreate failed: %Rrc", rc);

    /*
     * Parse parameters.  Parameters are position dependent.
     */
    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            /*
             * Files and directories.
             */
            case VINF_GETOPT_NOT_OPTION:
                break;

            /*
             * Options specific to our ISO maker.
             */


            /*
             * Options compatible with other ISO makers.
             */
            case 'o':
                if (Opts.fVirtualImageMaker)
                    return rtFsIsoMakerSyntaxError(&Opts, "The --output option is not allowed");
                if (Opts.pszOutFile)
                    return rtFsIsoMakerSyntaxError(&Opts, "The --output option is specified more than once");
                Opts.pszOutFile = ValueUnion.psz;
                break;

            /*
             * Standard bits.
             */
            case 'h':
                rtFsIsoMakerCmdUsage(&Opts, papszArgs[0]);
                return rtFsIsoMakerCmdDeleteState(&Opts, Opts.fVirtualImageMaker ? VERR_NOT_FOUND : VINF_SUCCESS);

            case 'V':
                rtFsIsoMakerPrintf(&Opts, "%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                return rtFsIsoMakerCmdDeleteState(&Opts, Opts.fVirtualImageMaker ? VERR_NOT_FOUND : VINF_SUCCESS);

            default:
                if (rc > 0 && RT_C_IS_GRAPH(rc))
                    rc = rtFsIsoMakerErrorRc(&Opts, VERR_GETOPT_UNKNOWN_OPTION, "Unhandled option: -%c", rc);
                else if (rc > 0)
                    rc = rtFsIsoMakerErrorRc(&Opts, VERR_GETOPT_UNKNOWN_OPTION, "Unhandled option: %i (%#x)", rc, rc);
                else if (rc == VERR_GETOPT_UNKNOWN_OPTION)
                    rc = rtFsIsoMakerErrorRc(&Opts, rc, "Unknown option: '%s'", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    rc = rtFsIsoMakerErrorRc(&Opts, rc, "%s: %Rrs\n", ValueUnion.pDef->pszLong, rc);
                else
                    rc = rtFsIsoMakerErrorRc(&Opts, rc, "%Rrs\n", rc);
                return rtFsIsoMakerCmdDeleteState(&Opts, rc);
        }
    }

    /*
     * Check for mandatory options.
     */
    if (!Opts.cItemsAdded)
        rc = rtFsIsoMakerErrorRc(&Opts, VERR_NO_DATA, "Cowardly refuses to create empty ISO image");
    else if (!Opts.pszOutFile && !Opts.fVirtualImageMaker)
        rc = rtFsIsoMakerErrorRc(&Opts, VERR_INVALID_PARAMETER, "No output file specified (--output <file>)");
    if (RT_SUCCESS(rc))
    {
        /*
         * Finalize the image and get the virtual file.
         */
        rc = RTFsIsoMakerFinalize(Opts.hIsoMaker);
        if (RT_SUCCESS(rc))
        {
            RTVFSFILE hVfsFile;
            rc = RTFsIsoMakerCreateVfsOutputFile(Opts.hIsoMaker, &hVfsFile);
            if (RT_SUCCESS(rc))
            {
                /*
                 * We're done now if we're only setting up a virtual image.
                 */
                if (Opts.fVirtualImageMaker)
                    *phVfsFile = hVfsFile;
                else
                {
                    rc = rtFsIsoMakerCmdWriteImage(&Opts, hVfsFile);
                    RTVfsFileRelease(hVfsFile);
                }
            }
            else
                rc = rtFsIsoMakerErrorRc(&Opts, rc, "RTFsIsoMakerCreateVfsOutputFile failed: %Rrc", rc);
        }
        else
            rc = rtFsIsoMakerErrorRc(&Opts, rc, "RTFsIsoMakerFinalize failed: %Rrc", rc);
    }

    return rtFsIsoMakerCmdDeleteState(&Opts, rc);
}


RTDECL(RTEXITCODE) RTFsIsoMakerCmd(unsigned cArgs, char **papszArgs)
{
    int rc = RTFsIsoMakerCmdEx(cArgs, papszArgs, NULL, NULL);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

