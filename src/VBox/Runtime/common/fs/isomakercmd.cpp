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
#include <iprt/path.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/formats/iso9660.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Maximum number of name specifiers we allow.  */
#define RTFSISOMAKERCMD_MAX_NAMES                       8

/** @name Name specifiers
 * @{ */
#define RTFSISOMAKERCMDNAME_PRIMARY_ISO                 RTFSISOMAKER_NAMESPACE_ISO_9660
#define RTFSISOMAKERCMDNAME_JOLIET                      RTFSISOMAKER_NAMESPACE_JOLIET
#define RTFSISOMAKERCMDNAME_UDF                         RTFSISOMAKER_NAMESPACE_UDF
#define RTFSISOMAKERCMDNAME_HFS                         RTFSISOMAKER_NAMESPACE_HFS

#define RTFSISOMAKERCMDNAME_PRIMARY_ISO_ROCK_RIDGE      RT_BIT_32(16)
#define RTFSISOMAKERCMDNAME_JOLIET_ROCK_RIDGE           RT_BIT_32(17)

#define RTFSISOMAKERCMDNAME_JOLIET_TRANS_TBL            RT_BIT_32(20)
#define RTFSISOMAKERCMDNAME_PRIMARY_ISO_TRANS_TBL       RT_BIT_32(21)
#define RTFSISOMAKERCMDNAME_UDF_TRANS_TBL               RT_BIT_32(22)
#define RTFSISOMAKERCMDNAME_HFS_TRANS_TBL               RT_BIT_32(23)

#define RTFSISOMAKERCMDNAME_MAJOR_MASK \
        (RTFSISOMAKERCMDNAME_PRIMARY_ISO | RTFSISOMAKERCMDNAME_JOLIET | RTFSISOMAKERCMDNAME_UDF | RTFSISOMAKERCMDNAME_HFS)

#define RTFSISOMAKERCMDNAME_MINOR_MASK \
        ( RTFSISOMAKERCMDNAME_PRIMARY_ISO_ROCK_RIDGE | RTFSISOMAKERCMDNAME_PRIMARY_ISO_TRANS_TBL \
        | RTFSISOMAKERCMDNAME_JOLIET_ROCK_RIDGE      | RTFSISOMAKERCMDNAME_JOLIET_TRANS_TBL \
        | RTFSISOMAKERCMDNAME_UDF_TRANS_TBL \
        | RTFSISOMAKERCMDNAME_HFS_TRANS_TBL)
AssertCompile((RTFSISOMAKERCMDNAME_MAJOR_MASK & RTFSISOMAKERCMDNAME_MINOR_MASK) == 0);
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef enum RTFSISOMAKERCMDOPT
{
    RTFSISOMAKERCMD_OPT_FIRST = 1000,

    RTFSISOMAKERCMD_OPT_IPRT_ISO_MAKER_FILE_MARKER,
    RTFSISOMAKERCMD_OPT_OUTPUT_BUFFER_SIZE,
    RTFSISOMAKERCMD_OPT_RANDOM_OUTPUT_BUFFER_SIZE,
    RTFSISOMAKERCMD_OPT_NAME_SETUP,
    RTFSISOMAKERCMD_OPT_NO_JOLIET,

    RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_12,
    RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_144,
    RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_288,

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
    RTFSISOMAKERCMD_OPT_ELTORITO_PLATFORM_ID,
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

    /** @name Processing of inputs
     * @{ */
    /** The namespaces (RTFSISOMAKER_NAMESPACE_XXX) we're currently adding
     *  input to. */
    uint32_t            fDstNamespaces;
    /** The number of name specifiers we're currently operating with. */
    uint32_t            cNameSpecifiers;
    /** Name specifier configurations.
     * For instance given "name0=name1=name2=name3=source-file" we will add
     * source-file to the image with name0 as the name in the namespace and
     * sub-name specified by aNameSpecifiers[0], name1 in aNameSpecifiers[1],
     * and so on.  This allows exact control over which names a file will
     * have in each namespace (primary-iso, joliet, udf, hfs) and sub-namespace
     * (rock-ridge, trans.tbl).
     */
    uint32_t            afNameSpecifiers[RTFSISOMAKERCMD_MAX_NAMES];
    /** @} */

    /** @name Booting related options and state.
     * @{ */
    /** The current El Torito boot entry, UINT32_MAX if none active.
     * An entry starts when --eltorito-boot/-b is used the first time, or when
     * --eltorito-alt-boot is used, or when --elptorito-platform-id is used
     * with a different value after -b is specified. */
    uint32_t            idEltoritoEntry;
    /** The previous --eltorito-platform-id option. */
    const char         *pszEltoritoPlatformId;
    /** @} */

    /** Number of items (files, directories, images, whatever) we've added. */
    uint32_t            cItemsAdded;
} RTFSISOMAKERCMDOPTS;
typedef RTFSISOMAKERCMDOPTS *PRTFSISOMAKERCMDOPTS;
typedef RTFSISOMAKERCMDOPTS const *PCRTFSISOMAKERCMDOPTS;


/**
 * Parsed name.
 */
typedef struct RTFSISOMAKERCMDPARSEDNAME
{
    /** Copy of the corresponding RTFSISOMAKERCMDOPTS::afNameSpecifiers
     * value. */
    uint32_t            fNameSpecifiers;
    /** The length of the specified path. */
    uint32_t            cchPath;
    /** Specified path. */
    char                szPath[RTPATH_MAX];
} RTFSISOMAKERCMDPARSEDNAME;
/** Pointer to a parsed name. */
typedef RTFSISOMAKERCMDPARSEDNAME *PRTFSISOMAKERCMDPARSEDNAME;
/** Pointer to a const parsed name. */
typedef RTFSISOMAKERCMDPARSEDNAME const *PCRTFSISOMAKERCMDPARSEDNAME;


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
    { "--name-setup",                   RTFSISOMAKERCMD_OPT_NAME_SETUP,                     RTGETOPT_REQ_STRING  },
    { "--no-joliet",                    RTFSISOMAKERCMD_OPT_NO_JOLIET,                      RTGETOPT_REQ_NOTHING },

#define DD(a_szLong, a_chShort, a_fFlags) { a_szLong, a_chShort, a_fFlags  }, { "-" a_szLong, a_chShort, a_fFlags  }

    /*
     * genisoimage/mkisofs compatibility options we've implemented:
     *
     */
    { "--generic-boot",                 'G',                                                RTGETOPT_REQ_STRING  },
    DD("-eltorito-boot",                'b',                                                RTGETOPT_REQ_STRING  ),
    DD("-eltorito-alt-boot",            RTFSISOMAKERCMD_OPT_ELTORITO_ALT_BOOT,              RTGETOPT_REQ_NOTHING ),
    DD("-eltorito-platform-id",         RTFSISOMAKERCMD_OPT_ELTORITO_PLATFORM_ID,           RTGETOPT_REQ_STRING  ),
    DD("-hard-disk-boot",               RTFSISOMAKERCMD_OPT_ELTORITO_HARD_DISK_BOOT,        RTGETOPT_REQ_NOTHING ),
    DD("-no-emulation-boot",            RTFSISOMAKERCMD_OPT_ELTORITO_NO_EMULATION_BOOT,     RTGETOPT_REQ_NOTHING ),
    DD("-no-boot",                      RTFSISOMAKERCMD_OPT_ELTORITO_NO_BOOT,               RTGETOPT_REQ_NOTHING ),
    DD("-boot-load-seg",                RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SEG,              RTGETOPT_REQ_UINT16  ),
    DD("-boot-load-size",               RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SIZE,             RTGETOPT_REQ_UINT16  ),
    DD("-boot-info-table",              RTFSISOMAKERCMD_OPT_ELTORITO_INFO_TABLE,            RTGETOPT_REQ_STRING  ),
    { "--boot-catalog",                 'c',                                                RTGETOPT_REQ_STRING  },


    /*
     * genisoimage/mkisofs compatibility:
     */
    DD("-abstract",                      RTFSISOMAKERCMD_OPT_ABSTRACT_FILE_ID,               RTGETOPT_REQ_STRING ),
    { "--application-id",               'A',                                                RTGETOPT_REQ_STRING  },
    DD("-allow-limited-size",           RTFSISOMAKERCMD_OPT_ALLOW_LIMITED_SIZE,             RTGETOPT_REQ_NOTHING ),
    DD("-allow-leading-dots",           RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING ),
    DD("-ldots",                        RTFSISOMAKERCMD_OPT_ALLOW_LEADING_DOTS,             RTGETOPT_REQ_NOTHING ),
    DD("-allow-lowercase",              RTFSISOMAKERCMD_OPT_ALLOW_LOWERCASE,                RTGETOPT_REQ_NOTHING ),
    DD("-allow-multidot",               RTFSISOMAKERCMD_OPT_ALLOW_MULTI_DOT,                RTGETOPT_REQ_NOTHING ),
    DD("-biblio",                       RTFSISOMAKERCMD_OPT_BIBLIOGRAPHIC_FILE_ID,          RTGETOPT_REQ_STRING  ),
    DD("-cache-inodes",                 RTFSISOMAKERCMD_OPT_DETECT_HARDLINKS,               RTGETOPT_REQ_NOTHING ),
    DD("-no-cache-inodes",              RTFSISOMAKERCMD_OPT_NO_DETECT_HARDLINKS,            RTGETOPT_REQ_NOTHING ),

    DD("-alpha-boot",                   RTFSISOMAKERCMD_OPT_ALPHA_BOOT,                     RTGETOPT_REQ_STRING  ),
    DD("-hppa-bootloader",              RTFSISOMAKERCMD_OPT_HPPA_BOOTLOADER,                RTGETOPT_REQ_STRING  ),
    DD("-hppa-cmdline",                 RTFSISOMAKERCMD_OPT_HPPA_CMDLINE,                   RTGETOPT_REQ_STRING  ),
    DD("-hppa-kernel-32",               RTFSISOMAKERCMD_OPT_HPPA_KERNEL_32,                 RTGETOPT_REQ_STRING  ),
    DD("-hppa-kernel-64",               RTFSISOMAKERCMD_OPT_HPPA_KERNEL_64,                 RTGETOPT_REQ_STRING  ),
    DD("-hppa-ramdisk",                 RTFSISOMAKERCMD_OPT_HPPA_RAMDISK,                   RTGETOPT_REQ_STRING  ),
    DD("-mips-boot",                    RTFSISOMAKERCMD_OPT_MIPS_BOOT,                      RTGETOPT_REQ_STRING  ),
    DD("-mipsel-boot",                  RTFSISOMAKERCMD_OPT_MIPSEL_BOOT,                    RTGETOPT_REQ_STRING  ),
    DD("-sparc-boot",                   'B',                                                RTGETOPT_REQ_STRING  ),
    { "--cd-extra",                     'C',                                                RTGETOPT_REQ_STRING  },
    DD("-check-oldnames",                RTFSISOMAKERCMD_OPT_CHECK_OLD_NAMES,               RTGETOPT_REQ_NOTHING ),
    DD("-check-session",                 RTFSISOMAKERCMD_OPT_CHECK_SESSION,                 RTGETOPT_REQ_STRING  ),
    DD("-copyright",                     RTFSISOMAKERCMD_OPT_COPYRIGHT_FILE_ID,             RTGETOPT_REQ_STRING  ),
    { "--dont-append-dot",              'd',                                                RTGETOPT_REQ_NOTHING },
    { "--deep-directories",             'D',                                                RTGETOPT_REQ_NOTHING },
    DD("-dir-mode",                     RTFSISOMAKERCMD_OPT_DIR_MODE,                       RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT ),
    DD("-dvd-video",                    RTFSISOMAKERCMD_OPT_DVD_VIDEO,                      RTGETOPT_REQ_NOTHING ),
    DD("-follow-symlinks",              'f',                                                RTGETOPT_REQ_NOTHING ),
    DD("-file-mode",                    RTFSISOMAKERCMD_OPT_FILE_MODE,                      RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT ),
    DD("-gid",                          RTFSISOMAKERCMD_OPT_GID,                            RTGETOPT_REQ_UINT32  ),
    DD("-gui",                          RTFSISOMAKERCMD_OPT_GUI,                            RTGETOPT_REQ_NOTHING ),
    DD("-graft-points",                 RTFSISOMAKERCMD_OPT_GRAFT_POINTS,                   RTGETOPT_REQ_NOTHING ),
    DD("-hide",                         RTFSISOMAKERCMD_OPT_HIDE,                           RTGETOPT_REQ_STRING  ),
    DD("-hide-list",                    RTFSISOMAKERCMD_OPT_HIDE_LIST,                      RTGETOPT_REQ_STRING  ),
    DD("-hidden",                       RTFSISOMAKERCMD_OPT_HIDDEN,                         RTGETOPT_REQ_STRING  ),
    DD("-hidden-list",                  RTFSISOMAKERCMD_OPT_HIDDEN_LIST,                    RTGETOPT_REQ_STRING  ),
    DD("-hide-joliet",                  RTFSISOMAKERCMD_OPT_HIDE_JOLIET,                    RTGETOPT_REQ_STRING  ),
    DD("-hide-joliet-list",             RTFSISOMAKERCMD_OPT_HIDE_JOLIET_LIST,               RTGETOPT_REQ_STRING  ),
    DD("-hide-joliet-trans-tbl",        RTFSISOMAKERCMD_OPT_HIDE_JOLIET_TRANS_TBL,          RTGETOPT_REQ_NOTHING ),
    DD("-hide-rr-moved",                RTFSISOMAKERCMD_OPT_HIDE_RR_MOVED,                  RTGETOPT_REQ_NOTHING ),
    DD("-input-charset",                RTFSISOMAKERCMD_OPT_INPUT_CHARSET,                  RTGETOPT_REQ_STRING  ),
    DD("-output-charset",               RTFSISOMAKERCMD_OPT_OUTPUT_CHARSET,                 RTGETOPT_REQ_STRING  ),
    { "--iso-level",                    RTFSISOMAKERCMD_OPT_ISO_LEVEL,                      RTGETOPT_REQ_UINT8   },
    { "--joliet",                       'J',                                                RTGETOPT_REQ_NOTHING },
    DD("-joliet-long",                  RTFSISOMAKERCMD_OPT_JOLIET_LONG,                    RTGETOPT_REQ_NOTHING ),
    DD("-jcharset",                     RTFSISOMAKERCMD_OPT_JOLIET_CHARSET,                 RTGETOPT_REQ_STRING  ),
    { "--long-names",                   'l',                                                RTGETOPT_REQ_NOTHING },
    { "--leading-dot",                  'L',                                                RTGETOPT_REQ_NOTHING },
    DD("-jigdo-jigdo",                  RTFSISOMAKERCMD_OPT_JIGDO_JIGDO,                    RTGETOPT_REQ_STRING  ),
    DD("-jigdo-template",               RTFSISOMAKERCMD_OPT_JIGDO_TEMPLATE,                 RTGETOPT_REQ_STRING  ),
    DD("-jigdo-min-file-size",          RTFSISOMAKERCMD_OPT_JIGDO_MIN_FILE_SIZE,            RTGETOPT_REQ_UINT64  ),
    DD("-jigdo-force-md5",              RTFSISOMAKERCMD_OPT_JIGDO_FORCE_MD5,                RTGETOPT_REQ_STRING  ),
    DD("-jigdo-exclude",                RTFSISOMAKERCMD_OPT_JIGDO_EXCLUDE,                  RTGETOPT_REQ_STRING  ),
    DD("-jigdo-map",                    RTFSISOMAKERCMD_OPT_JIGDO_MAP,                      RTGETOPT_REQ_STRING  ),
    DD("-md5-list",                     RTFSISOMAKERCMD_OPT_JIGDO_MD5_LIST,                 RTGETOPT_REQ_STRING  ),
    DD("-jigdo-template-compress",      RTFSISOMAKERCMD_OPT_JIGDO_COMPRESS,                 RTGETOPT_REQ_STRING  ),
    DD("-log-file",                     RTFSISOMAKERCMD_OPT_LOG_FILE,                       RTGETOPT_REQ_STRING  ),
    { "--exclude",                      'm',                                                RTGETOPT_REQ_STRING  },
    { "--exclude",                      'x',                                                RTGETOPT_REQ_STRING  },
    DD("-exclude-list",                 RTFSISOMAKERCMD_OPT_EXCLUDE_LIST,                   RTGETOPT_REQ_STRING  ),
    DD("-max-iso9660-filenames",        RTFSISOMAKERCMD_OPT_MAX_ISO9660_FILENAMES,          RTGETOPT_REQ_NOTHING ),
    { "--merge",                        'M',                                                RTGETOPT_REQ_STRING  },
    DD("-dev",                          'M',                                                RTGETOPT_REQ_STRING  ),
    { "--omit-version-numbers",         'N',                                                RTGETOPT_REQ_NOTHING },
    DD("-new-dir-mode",                 RTFSISOMAKERCMD_OPT_NEW_DIR_MODE,                   RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT ),
    DD("-nobak",                        RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING ),
    DD("-no-bak",                       RTFSISOMAKERCMD_OPT_NO_BACKUP_FILES,                RTGETOPT_REQ_NOTHING ),
    DD("-force-rr",                     RTFSISOMAKERCMD_OPT_FORCE_RR,                       RTGETOPT_REQ_NOTHING ),
    DD("-no-rr",                        RTFSISOMAKERCMD_OPT_NO_RR,                          RTGETOPT_REQ_NOTHING ),
    DD("-no-split-symlink-components",  RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_COMPONENTS,    RTGETOPT_REQ_NOTHING ),
    DD("-no-split-symlink-fields",      RTFSISOMAKERCMD_OPT_NO_SPLIT_SYMLINK_FIELDS,        RTGETOPT_REQ_NOTHING ),
    { "--output",                       'o',                                                RTGETOPT_REQ_STRING  },
    DD("-pad",                          RTFSISOMAKERCMD_OPT_PAD,                            RTGETOPT_REQ_NOTHING ),
    DD("-no-pad",                       RTFSISOMAKERCMD_OPT_NO_PAD,                         RTGETOPT_REQ_NOTHING ),
    DD("-path-list",                    RTFSISOMAKERCMD_OPT_PATH_LIST,                      RTGETOPT_REQ_STRING  ),
    DD("-publisher",                    'P',                                                RTGETOPT_REQ_STRING  ),
    { "--preparer",                     'p',                                                RTGETOPT_REQ_STRING  },
    DD("-print-size",                   RTFSISOMAKERCMD_OPT_PRINT_SIZE,                     RTGETOPT_REQ_NOTHING ),
    DD("-quiet",                        RTFSISOMAKERCMD_OPT_QUIET,                          RTGETOPT_REQ_NOTHING ),
    { "--rock-ridge",                   'R',                                                RTGETOPT_REQ_NOTHING },
    { "--rock-ridge-relaxed",           'r',                                                RTGETOPT_REQ_NOTHING },
    DD("-relaxed-filenames",            RTFSISOMAKERCMD_OPT_RELAXED_FILENAMES,              RTGETOPT_REQ_NOTHING ),
    DD("-root",                         RTFSISOMAKERCMD_OPT_ROOT,                           RTGETOPT_REQ_STRING  ),
    DD("-old-root",                     RTFSISOMAKERCMD_OPT_OLD_ROOT,                       RTGETOPT_REQ_STRING  ),
    DD("-sort",                         RTFSISOMAKERCMD_OPT_SORT,                           RTGETOPT_REQ_STRING  ),
    DD("-sparc-boot",                   RTFSISOMAKERCMD_OPT_SPARC_BOOT,                     RTGETOPT_REQ_STRING  ),
    DD("-sparc-label",                  RTFSISOMAKERCMD_OPT_SPARC_LABEL,                    RTGETOPT_REQ_STRING  ),
    DD("-split-output",                 RTFSISOMAKERCMD_OPT_SPLIT_OUTPUT,                   RTGETOPT_REQ_NOTHING ),
    DD("-stream-media-size",            RTFSISOMAKERCMD_OPT_STREAM_MEDIA_SIZE,              RTGETOPT_REQ_UINT64  ),
    DD("-stream-file-name",             RTFSISOMAKERCMD_OPT_STREAM_FILE_NAME,               RTGETOPT_REQ_STRING  ),
    DD("-sunx86-boot",                  RTFSISOMAKERCMD_OPT_SUNX86_BOOT,                    RTGETOPT_REQ_STRING  ),
    DD("-sunx86-label",                 RTFSISOMAKERCMD_OPT_SUNX86_LABEL,                   RTGETOPT_REQ_STRING  ),
    DD("-sysid",                        RTFSISOMAKERCMD_OPT_SYSTEM_ID,                      RTGETOPT_REQ_STRING  ),
    { "--trans-tbl",                    'T',                                                RTGETOPT_REQ_NOTHING },
    DD("-table-name",                   RTFSISOMAKERCMD_OPT_TRANS_TBL_NAME,                 RTGETOPT_REQ_STRING  ),
    DD("-ucs-level",                    RTFSISOMAKERCMD_OPT_JOLIET_LEVEL,                   RTGETOPT_REQ_UINT8   ),
    DD("-udf",                          RTFSISOMAKERCMD_OPT_UDF,                            RTGETOPT_REQ_NOTHING ),
    DD("-uid",                          RTFSISOMAKERCMD_OPT_UID,                            RTGETOPT_REQ_UINT32  ),
    DD("-use-fileversion",              RTFSISOMAKERCMD_OPT_USE_FILE_VERSION,               RTGETOPT_REQ_NOTHING ),
    { "--untranslated-filenames",       'U',                                                RTGETOPT_REQ_NOTHING },
    DD("-no-iso-translate",             RTFSISOMAKERCMD_OPT_NO_ISO_TRANSLATE,               RTGETOPT_REQ_NOTHING ),
    { "--volume-id",                    'V',                                                RTGETOPT_REQ_STRING  },
    DD("-volset",                       RTFSISOMAKERCMD_OPT_VOLUME_SET_ID,                  RTGETOPT_REQ_STRING  ),
    DD("-volset-size",                  RTFSISOMAKERCMD_OPT_VOLUME_SET_SIZE,                RTGETOPT_REQ_UINT32  ),
    DD("-volset-seqno",                 RTFSISOMAKERCMD_OPT_VOLUME_SET_SEQ_NO,              RTGETOPT_REQ_UINT32  ),
    { "--transpared-compression",       'z',                                                RTGETOPT_REQ_NOTHING },

    /* HFS and ISO-9660 apple extensions. */
    DD("-hfs",                          RTFSISOMAKERCMD_OPT_HFS_ENABLE,                     RTGETOPT_REQ_NOTHING ),
    DD("-apple",                        RTFSISOMAKERCMD_OPT_APPLE,                          RTGETOPT_REQ_NOTHING ),
    DD("-map",                          RTFSISOMAKERCMD_OPT_HFS_MAP,                        RTGETOPT_REQ_STRING  ),
    DD("-magic",                        RTFSISOMAKERCMD_OPT_HFS_MAGIC,                      RTGETOPT_REQ_STRING  ),
    DD("-hfs-creator",                  RTFSISOMAKERCMD_OPT_HFS_CREATOR,                    RTGETOPT_REQ_STRING  ),
    DD("-hfs-type",                     RTFSISOMAKERCMD_OPT_HFS_TYPE,                       RTGETOPT_REQ_STRING  ),
    DD("-probe",                        RTFSISOMAKERCMD_OPT_HFS_PROBE,                      RTGETOPT_REQ_NOTHING ),
    DD("-no-desktop",                   RTFSISOMAKERCMD_OPT_HFS_NO_DESKTOP,                 RTGETOPT_REQ_NOTHING ),
    DD("-mac-name",                     RTFSISOMAKERCMD_OPT_HFS_MAC_NAME,                   RTGETOPT_REQ_NOTHING ),
    DD("-boot-hfs-file",                RTFSISOMAKERCMD_OPT_HFS_BOOT_FILE,                  RTGETOPT_REQ_STRING  ),
    DD("-part",                         RTFSISOMAKERCMD_OPT_HFS_PART,                       RTGETOPT_REQ_NOTHING ),
    DD("-auto",                         RTFSISOMAKERCMD_OPT_HFS_AUTO,                       RTGETOPT_REQ_STRING  ),
    DD("-cluster-size",                 RTFSISOMAKERCMD_OPT_HFS_CLUSTER_SIZE,               RTGETOPT_REQ_UINT32  ),
    DD("-hide-hfs",                     RTFSISOMAKERCMD_OPT_HFS_HIDE,                       RTGETOPT_REQ_STRING  ),
    DD("-hide-hfs-list",                RTFSISOMAKERCMD_OPT_HFS_HIDE_LIST,                  RTGETOPT_REQ_STRING  ),
    DD("-hfs-volid",                    RTFSISOMAKERCMD_OPT_HFS_VOL_ID,                     RTGETOPT_REQ_STRING  ),
    DD("-icon-position",                RTFSISOMAKERCMD_OPT_HFS_ICON_POSITION,              RTGETOPT_REQ_NOTHING ),
    DD("-root-info",                    RTFSISOMAKERCMD_OPT_HFS_ROOT_INFO,                  RTGETOPT_REQ_STRING  ),
    DD("-prep-boot",                    RTFSISOMAKERCMD_OPT_HFS_PREP_BOOT,                  RTGETOPT_REQ_STRING  ),
    DD("-chrp-boot",                    RTFSISOMAKERCMD_OPT_HFS_CHRP_BOOT,                  RTGETOPT_REQ_NOTHING ),
    DD("-input-hfs-charset",            RTFSISOMAKERCMD_OPT_HFS_INPUT_CHARSET,              RTGETOPT_REQ_STRING  ),
    DD("-output-hfs-charset",           RTFSISOMAKERCMD_OPT_HFS_OUTPUT_CHARSET,             RTGETOPT_REQ_STRING  ),
    DD("-hfs-unlock",                   RTFSISOMAKERCMD_OPT_HFS_UNLOCK,                     RTGETOPT_REQ_NOTHING ),
    DD("-hfs-bless",                    RTFSISOMAKERCMD_OPT_HFS_BLESS,                      RTGETOPT_REQ_STRING  ),
    DD("-hfs-parms",                    RTFSISOMAKERCMD_OPT_HFS_PARMS,                      RTGETOPT_REQ_STRING  ),
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
#undef DD
};


/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV.
 *
 * @returns @a rc
 * @param   pOpts               The ISO maker command instance.
 * @param   rc                  The return code.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFsIsoMakerCmdErrorRc(PRTFSISOMAKERCMDOPTS pOpts, int rc, const char *pszFormat, ...)
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
 * Wrapper around RTErrInfoSetV / RTMsgErrorV for doing the job of
 * RTVfsChainMsgError.
 *
 * @returns @a rc
 * @param   pOpts               The ISO maker command instance.
 * @param   pszFunction         The API called.
 * @param   pszSpec             The VFS chain specification or file path passed to the.
 * @param   rc                  The return code.
 * @param   offError            The error offset value returned (0 if not captured).
 * @param   pErrInfo            Additional error information.  Optional.
 */
static int rtFsIsoMakerCmdChainError(PRTFSISOMAKERCMDOPTS pOpts, const char *pszFunction, const char *pszSpec, int rc,
                                     uint32_t offError, PRTERRINFO pErrInfo)
{
    if (RTErrInfoIsSet(pErrInfo))
    {
        if (offError > 0)
            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc,
                                        "%s failed with rc=%Rrc: %s\n"
                                        "    '%s'\n"
                                        "     %*s^\n",
                                        pszFunction, rc, pErrInfo->pszMsg, pszSpec, offError, "");
        else
            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "%s failed to open '%s': %Rrc: %s\n",
                                        pszFunction, pszSpec, rc, pErrInfo->pszMsg);
    }
    else
    {
        if (offError > 0)
            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc,
                                        "%s failed with rc=%Rrc:\n"
                                        "    '%s'\n"
                                        "     %*s^\n",
                                        pszFunction, rc, pszSpec, offError, "");
        else
            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "%s failed to open '%s': %Rrc\n", pszFunction, pszSpec, rc);
    }
    return rc;
}


/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV for displaying syntax errors.
 *
 * @returns VERR_INVALID_PARAMETER
 * @param   pOpts               The ISO maker command instance.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFsIsoMakerCmdSyntaxError(PRTFSISOMAKERCMDOPTS pOpts, const char *pszFormat, ...)
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
 * @param   pOpts               The ISO maker command instance.
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
 * @param   pOpts               The ISO maker command instance to delete.
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
 * @param   pOpts               The ISO maker command instance.
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
                            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "Error %Rrc writing %#zx bytes at offset %#RX64 to '%s'",
                                                        rc, cbToCopy, offImage, pOpts->pszOutFile);
                            break;
                        }
                    }
                    else
                    {
                        rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "Error %Rrc read %#zx bytes at offset %#RX64", rc, cbToCopy, offImage);
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
                        rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "RTVfsFileFlush failed on '%s': %Rrc", pOpts->pszOutFile, rc);
                }

                RTVfsFileRelease(hVfsDstFile);
            }
            else
                rc = rtFsIsoMakerCmdChainError(pOpts, "RTVfsChainOpenFile", pOpts->pszOutFile, rc, offError, &ErrInfo.Core);

            RTMemTmpFree(pvBuf);
        }
        else
            rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "RTMemTmpAlloc(%zu) failed: %Rrc", pOpts->cbOutputReadBuffer, rc);
    }
    else
        rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "RTVfsFileGetSize failed: %Rrc", rc);
    return rc;
}


/**
 * Formats @a fNameSpecifiers into a '+' separated list of names.
 *
 * @returns pszDst
 * @param   fNameSpecifiers The name specifiers.
 * @param   pszDst          The destination bufer.
 * @param   cbDst           The size of the destination buffer.
 */
static char *rtFsIsoMakerCmdNameSpecifiersToString(uint32_t fNameSpecifiers, char *pszDst, size_t cbDst)
{
    static struct { const char *pszName; uint32_t cchName; uint32_t fSpec; } const s_aSpecs[] =
    {
        { RT_STR_TUPLE("primary"),           RTFSISOMAKERCMDNAME_PRIMARY_ISO            },
        { RT_STR_TUPLE("primary-rock"),      RTFSISOMAKERCMDNAME_PRIMARY_ISO_ROCK_RIDGE },
        { RT_STR_TUPLE("primary-trans-tbl"), RTFSISOMAKERCMDNAME_PRIMARY_ISO_TRANS_TBL  },
        { RT_STR_TUPLE("joliet"),            RTFSISOMAKERCMDNAME_JOLIET                 },
        { RT_STR_TUPLE("joliet-rock"),       RTFSISOMAKERCMDNAME_JOLIET_ROCK_RIDGE      },
        { RT_STR_TUPLE("joliet-trans-tbl"),  RTFSISOMAKERCMDNAME_JOLIET_TRANS_TBL       },
        { RT_STR_TUPLE("udf"),               RTFSISOMAKERCMDNAME_UDF                    },
        { RT_STR_TUPLE("udf-trans-tbl"),     RTFSISOMAKERCMDNAME_UDF_TRANS_TBL          },
        { RT_STR_TUPLE("hfs"),               RTFSISOMAKERCMDNAME_HFS                    },
        { RT_STR_TUPLE("hfs-trans-tbl"),     RTFSISOMAKERCMDNAME_HFS_TRANS_TBL          },
    };

    Assert(cbDst > 0);
    char *pszRet = pszDst;
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aSpecs); i++)
        if (s_aSpecs[i].fSpec & fNameSpecifiers)
        {
            if (pszDst != pszRet && cbDst > 1)
            {
                *pszDst++ = '+';
                cbDst--;
            }
            if (cbDst > s_aSpecs[i].cchName)
            {
                memcpy(pszDst, s_aSpecs[i].pszName, s_aSpecs[i].cchName);
                cbDst  -= s_aSpecs[i].cchName;
                pszDst += s_aSpecs[i].cchName;
            }
            else if (cbDst > 1)
            {
                memcpy(pszDst, s_aSpecs[i].pszName, cbDst - 1);
                pszDst += cbDst - 1;
                cbDst  = 1;
            }

            fNameSpecifiers &= ~s_aSpecs[i].fSpec;
            if (!fNameSpecifiers)
                break;
        }
    *pszDst = '\0';
    return pszRet;
}


/**
 * Parses the --name-setup option.
 *
 * @returns IPRT status code.
 * @param   pOpts               The ISO maker command instance.
 * @param   pszSpec             The name setup specification.
 */
static int rtFsIsoMakerCmdOptNameSetup(PRTFSISOMAKERCMDOPTS pOpts, const char *pszSpec)
{
    /*
     * Comma separated list of one or more specifiers.
     */
    uint32_t fNamespaces    = 0;
    uint32_t fPrevMajor     = 0;
    uint32_t iNameSpecifier = 0;
    uint32_t offSpec        = 0;
    do
    {
        /*
         * Parse up to the next colon or end of string.
         */
        uint32_t fNameSpecifier = 0;
        char     ch;
        while (  (ch = pszSpec[offSpec]) != '\0'
               && ch != ',')
        {
            if (RT_C_IS_SPACE(ch) || ch == '+' || ch == '|') /* space, '+' and '|' are allowed as name separators. */
                offSpec++;
            else
            {
                /* Find the end of the name. */
                uint32_t offEndSpec = offSpec + 1;
                while (  (ch = pszSpec[offEndSpec]) != '\0'
                       && ch != ','
                       && ch != '+'
                       && ch != '|'
                       && !RT_C_IS_SPACE(ch))
                    offEndSpec++;

#define IS_EQUAL(a_sz) (cchName == sizeof(a_sz) - 1U && strncmp(pchName, a_sz, sizeof(a_sz) - 1U) == 0)
                const char * const pchName = &pszSpec[offSpec];
                uint32_t const     cchName = offEndSpec - offSpec;
                /* major namespaces */
                if (   IS_EQUAL("iso")
                    || IS_EQUAL("primary")
                    || IS_EQUAL("iso9660")
                    || IS_EQUAL("iso-9660")
                    || IS_EQUAL("primary-iso")
                    || IS_EQUAL("iso-primary") )
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_PRIMARY_ISO;
                    fNamespaces |= fPrevMajor = RTFSISOMAKER_NAMESPACE_ISO_9660;
                }
                else if (IS_EQUAL("joliet"))
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_JOLIET;
                    fNamespaces |= fPrevMajor = RTFSISOMAKER_NAMESPACE_JOLIET;
                }
                else if (IS_EQUAL("udf"))
                {
#if 0
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_UDF;
                    fNamespaces |= fPrevMajor = RTFSISOMAKER_NAMESPACE_UDF;
#else
                    return rtFsIsoMakerCmdSyntaxError(pOpts, "UDF support is currently not implemented");
#endif
                }
                else if (IS_EQUAL("hfs") || IS_EQUAL("hfsplus"))
                {
#if 0
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_HFS;
                    fNamespaces |= fPrevMajor = RTFSISOMAKER_NAMESPACE_HFS;
#else
                    return rtFsIsoMakerCmdSyntaxError(pOpts, "Hybrid HFS+ support is currently not implemented");
#endif
                }
                /* rock ridge */
                else if (   IS_EQUAL("rr")
                         || IS_EQUAL("rock")
                         || IS_EQUAL("rock-ridge"))
                {
                    if (fPrevMajor == RTFSISOMAKERCMDNAME_PRIMARY_ISO)
                        fNameSpecifier |= RTFSISOMAKERCMDNAME_PRIMARY_ISO_ROCK_RIDGE;
                    else if (fPrevMajor == RTFSISOMAKERCMDNAME_JOLIET)
                        fNameSpecifier |= RTFSISOMAKERCMDNAME_JOLIET_ROCK_RIDGE;
                    else
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "unqualified rock-ridge name specifier");
                }
                else if (   IS_EQUAL("iso-rr")         || IS_EQUAL("iso-rock")         || IS_EQUAL("iso-rock-ridge")
                         || IS_EQUAL("primary-rr")     || IS_EQUAL("primary-rock")     || IS_EQUAL("primary-rock-ridge")
                         || IS_EQUAL("iso9660-rr")     || IS_EQUAL("iso9660-rock")     || IS_EQUAL("iso9660-rock-ridge")
                         || IS_EQUAL("iso-9660-rr")    || IS_EQUAL("iso-9660-rock")    || IS_EQUAL("iso-9660-rock-ridge")
                         || IS_EQUAL("primaryiso-rr")  || IS_EQUAL("primaryiso-rock")  || IS_EQUAL("primaryiso-rock-ridge")
                         || IS_EQUAL("primary-iso-rr") || IS_EQUAL("primary-iso-rock") || IS_EQUAL("primary-iso-rock-ridge") )
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_PRIMARY_ISO_ROCK_RIDGE;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_PRIMARY_ISO))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "iso-9660-rock-ridge must come after the iso-9660 name specifier");
                }
                else if (IS_EQUAL("joliet-rr") || IS_EQUAL("joliet-rock") || IS_EQUAL("joliet-rock-ridge"))
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_JOLIET_ROCK_RIDGE;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_JOLIET))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "joliet-rock-ridge must come after the joliet name specifier");
                }
                /* trans.tbl */
                else if (IS_EQUAL("trans") || IS_EQUAL("trans-tbl"))
                {
                    if (fPrevMajor == RTFSISOMAKERCMDNAME_PRIMARY_ISO)
                        fNameSpecifier |= RTFSISOMAKERCMDNAME_PRIMARY_ISO_TRANS_TBL;
                    else if (fPrevMajor == RTFSISOMAKERCMDNAME_JOLIET)
                        fNameSpecifier |= RTFSISOMAKERCMDNAME_JOLIET_TRANS_TBL;
                    else
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "unqualified trans-tbl name specifier");
                }
                else if (   IS_EQUAL("iso-trans")         || IS_EQUAL("iso-trans-tbl")
                         || IS_EQUAL("primary-trans")     || IS_EQUAL("primary-trans-tbl")
                         || IS_EQUAL("iso9660-trans")     || IS_EQUAL("iso9660-trans-tbl")
                         || IS_EQUAL("iso-9660-trans")    || IS_EQUAL("iso-9660-trans-tbl")
                         || IS_EQUAL("primaryiso-trans")  || IS_EQUAL("primaryiso-trans-tbl")
                         || IS_EQUAL("primary-iso-trans") || IS_EQUAL("primary-iso-trans-tbl") )
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_PRIMARY_ISO_TRANS_TBL;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_PRIMARY_ISO))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "iso-9660-trans-tbl must come after the iso-9660 name specifier");
                }
                else if (IS_EQUAL("joliet-trans") || IS_EQUAL("joliet-trans-tbl"))
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_JOLIET_TRANS_TBL;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_JOLIET))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "joliet-trans-tbl must come after the joliet name specifier");
                }
                else if (IS_EQUAL("udf-trans") || IS_EQUAL("udf-trans-tbl"))
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_UDF_TRANS_TBL;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_UDF))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "udf-trans-tbl must come after the udf name specifier");
                }
                else if (IS_EQUAL("hfs-trans") || IS_EQUAL("hfs-trans-tbl"))
                {
                    fNameSpecifier |= RTFSISOMAKERCMDNAME_HFS_TRANS_TBL;
                    if (!(fNamespaces & RTFSISOMAKERCMDNAME_HFS))
                        return rtFsIsoMakerCmdSyntaxError(pOpts, "hfs-trans-tbl must come after the hfs name specifier");
                }
                else
                    return rtFsIsoMakerCmdSyntaxError(pOpts, "unknown name specifier '%.*s'", cchName, pchName);
#undef IS_EQUAL
                offSpec = offEndSpec;
            }
        } /* while same specifier */

        /*
         * Check that it wasn't empty.
         */
        if (fNameSpecifier == 0)
            return rtFsIsoMakerCmdSyntaxError(pOpts, "name specifier #%u (0-based) is empty ", iNameSpecifier);

        /*
         * Complain if a major namespace name is duplicated.  The rock-ridge and
         * trans.tbl names are simple to replace, the others affect the two former
         * names and are therefore not allowed twice in the list.
         */
        uint32_t i = iNameSpecifier;
        while (i-- > 0)
        {
            uint32_t fRepeated = (fNameSpecifier             & RTFSISOMAKERCMDNAME_MAJOR_MASK)
                               & (pOpts->afNameSpecifiers[i] & RTFSISOMAKERCMDNAME_MAJOR_MASK);
            if (fRepeated)
            {
                char szTmp[128];
                return rtFsIsoMakerCmdSyntaxError(pOpts, "repeating name specifier%s: %s", RT_IS_POWER_OF_TWO(fRepeated) ? "" : "s",
                                                  rtFsIsoMakerCmdNameSpecifiersToString(fRepeated, szTmp, sizeof(szTmp)));
            }
        }

        /*
         * Add it.
         */
        if (iNameSpecifier >= RT_ELEMENTS(pOpts->afNameSpecifiers))
            return rtFsIsoMakerCmdSyntaxError(pOpts, "too many name specifiers (max %d)", RT_ELEMENTS(pOpts->afNameSpecifiers));
        pOpts->afNameSpecifiers[iNameSpecifier] = fNameSpecifier;
        iNameSpecifier++;

        /*
         * Next, if any.
         */
        if (pszSpec[offSpec] == ',')
            offSpec++;
    } while (pszSpec[offSpec] != '\0');

    pOpts->cNameSpecifiers = iNameSpecifier;
    pOpts->fDstNamespaces  = fNamespaces;

    return VINF_SUCCESS;
}


/**
 * Adds a file.
 *
 * @returns IPRT status code.
 * @param   pOpts               The ISO maker command instance.
 * @param   pszSrc              The path to the source file.
 * @param   paParsedNames       Array of parsed names, there are
 *                              pOpts->cNameSpecifiers entries in the array.
 */
static int rtFsIsoMakerCmdAddFile(PRTFSISOMAKERCMDOPTS pOpts, const char *pszSrc, PCRTFSISOMAKERCMDPARSEDNAME paParsedNames)
{
    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedFileWithSrcPath(pOpts->hIsoMaker, pszSrc, &idxObj);
    if (RT_SUCCESS(rc))
    {
        pOpts->cItemsAdded++;

        for (uint32_t i = 0; i < pOpts->cNameSpecifiers; i++)
            if (paParsedNames[i].cchPath > 0)
            {
                if (paParsedNames[i].fNameSpecifiers & RTFSISOMAKERCMDNAME_MAJOR_MASK)
                {
                    rc = RTFsIsoMakerObjSetPath(pOpts->hIsoMaker, idxObj,
                                                paParsedNames[i].fNameSpecifiers & RTFSISOMAKERCMDNAME_MAJOR_MASK,
                                                paParsedNames[i].szPath);
                    if (RT_FAILURE(rc))
                    {
                        rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "Error setting name '%s' on '%s': %Rrc",
                                                    paParsedNames[i].szPath, pszSrc, rc);
                        break;
                    }
                }
                if (paParsedNames[i].fNameSpecifiers & RTFSISOMAKERCMDNAME_MINOR_MASK)
                {
                    /** @todo add APIs for this.   */
                }
            }
    }
    else
        rc = rtFsIsoMakerCmdErrorRc(pOpts, rc, "Error adding '%s': %Rrc", pszSrc, rc);
    return rc;
}


/**
 * Processes a non-option argument.
 *
 * @returns IPRT status code.
 * @param   pOpts               The ISO maker command instance.
 * @param   pszSpec             The specification of what to add.
 */
static int rtFsIsoMakerCmdAddSomething(PRTFSISOMAKERCMDOPTS pOpts, const char *pszSpec)
{
    const char * const pszSpecIn = pszSpec;
    enum { kSpecialSrc_Normal, kSpecialSrc_Remove, kSpecialSrc_MustRemove } enmSpecialSrc = kSpecialSrc_Normal;

    /*
     * Split it up by '='.  Because of the source, which comes last,
     */
    RTFSISOMAKERCMDPARSEDNAME   aParsedNames[RTFSISOMAKERCMD_MAX_NAMES + 1];
    uint32_t                    cParsedNames = 0;
    for (;;)
    {
        const char *pszEqual   = strchr(pszSpec, '=');
        size_t      cchName    = pszEqual ? pszEqual - pszSpec : strlen(pszSpec);
        bool        fNeedSlash = pszEqual && !RTPATH_IS_SLASH(*pszSpec) && cchName > 0;
        if (cchName + fNeedSlash >= sizeof(aParsedNames[cParsedNames].szPath))
            return rtFsIsoMakerCmdSyntaxError(pOpts, "name #%u (0-based) is too long: %s", cParsedNames, pszSpecIn);
        if (cParsedNames >= pOpts->cNameSpecifiers + 1)
            return rtFsIsoMakerCmdSyntaxError(pOpts, "too many names specified (max %u + source): %s",
                                              pOpts->cNameSpecifiers,  pszSpecIn);
        if (!fNeedSlash)
            memcpy(aParsedNames[cParsedNames].szPath, pszSpec, cchName);
        else
        {
            memcpy(&aParsedNames[cParsedNames].szPath[1], pszSpec, cchName);
            aParsedNames[cParsedNames].szPath[0] = RTPATH_SLASH;
            cchName++;
        }
        aParsedNames[cParsedNames].szPath[cchName] = '\0';
        aParsedNames[cParsedNames].cchPath = (uint32_t)cchName;
        cParsedNames++;

        if (!pszEqual)
        {
            if (!cchName)
                return rtFsIsoMakerCmdSyntaxError(pOpts, "empty source file name: %s", pszSpecIn);
            if (cchName == 8 && strcmp(pszSpec, ":remove:") == 0)
                enmSpecialSrc = kSpecialSrc_Remove;
            else if (cchName == 13 && strcmp(pszSpec, ":must-remove:") == 0)
                enmSpecialSrc = kSpecialSrc_MustRemove;
            break;
        }
        pszSpec = pszEqual + 1;
    }

    /*
     * If there are too few names specified, move the source and repeat the penultimate name.
     */
    if (cParsedNames < pOpts->cNameSpecifiers + 1)
    {
        aParsedNames[pOpts->cNameSpecifiers] = aParsedNames[cParsedNames - 1];
        uint32_t iSrc = cParsedNames >= 2 ? cParsedNames - 2 : 0;

        /* If the source is a input file name specifier, reduce it to something that starts with a slash. */
        if (cParsedNames == 1)
        {
            if (RTVfsChainIsSpec(aParsedNames[iSrc].szPath))
            {
                uint32_t offError;
                char *pszFinalPath;
                int rc = RTVfsChainQueryFinalPath(aParsedNames[iSrc].szPath, &pszFinalPath, &offError);
                if (RT_FAILURE(rc))
                    return rtFsIsoMakerCmdChainError(pOpts, "RTVfsChainQueryFinalPath",
                                                     aParsedNames[iSrc].szPath, rc, offError, NULL);
                aParsedNames[iSrc].cchPath = (uint32_t)strlen(pszFinalPath);
                if (RTPATH_IS_SLASH(*pszFinalPath))
                    memcpy(aParsedNames[iSrc].szPath, pszFinalPath, aParsedNames[iSrc].cchPath + 1);
                else
                {
                    memcpy(&aParsedNames[iSrc].szPath[1], pszFinalPath, aParsedNames[iSrc].cchPath + 1);
                    aParsedNames[iSrc].szPath[0] = RTPATH_SLASH;
                    aParsedNames[iSrc].cchPath++;
                }
                RTStrFree(pszFinalPath);
            }
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
            else if (   RTPATH_IS_VOLSEP(aParsedNames[iSrc].szPath[1])
                     && RT_C_IS_ALPHA(aParsedNames[iSrc].szPath[0]))
            {
                if (RTPATH_IS_SLASH(aParsedNames[iSrc].szPath[2]))
                {
                    memmove(&aParsedNames[iSrc].szPath[0], &aParsedNames[iSrc].szPath[2], aParsedNames[iSrc].cchPath - 1);
                    aParsedNames[iSrc].cchPath -= 2;
                }
                else
                {
                    memmove(&aParsedNames[iSrc].szPath[1], &aParsedNames[iSrc].szPath[2], aParsedNames[iSrc].cchPath - 1);
                    aParsedNames[iSrc].szPath[0] = RTPATH_SLASH;
                    aParsedNames[iSrc].cchPath  -= 1;
                }
            }
#endif
            else if (!RTPATH_IS_SLASH(aParsedNames[iSrc].szPath[0]))
            {
                if (aParsedNames[iSrc].cchPath + 2 > sizeof(aParsedNames[iSrc].szPath))
                    return rtFsIsoMakerCmdSyntaxError(pOpts, "name too long: %s", pszSpecIn);
                memmove(&aParsedNames[iSrc].szPath[1], &aParsedNames[iSrc].szPath[0], aParsedNames[iSrc].cchPath + 1);
                aParsedNames[iSrc].szPath[0] = RTPATH_SLASH;
                aParsedNames[iSrc].cchPath++;
            }
        }

        for (uint32_t iDst = iSrc + 1; iDst < pOpts->cNameSpecifiers; iDst++)
            aParsedNames[iDst] = aParsedNames[iSrc];
        cParsedNames = pOpts->cNameSpecifiers + 1;
    }

    /*
     * Copy the specifier flags and check that the paths all starts with slashes.
     */
    for (uint32_t i = 0; i < pOpts->cNameSpecifiers; i++)
    {
        aParsedNames[i].fNameSpecifiers = pOpts->afNameSpecifiers[i];
        Assert(   aParsedNames[i].cchPath == 0
               || RTPATH_IS_SLASH(aParsedNames[i].szPath[0]));
    }

    /*
     * Deal with special source filenames used to remove/change stuff.
     */
    if (   enmSpecialSrc == kSpecialSrc_Remove
        || enmSpecialSrc == kSpecialSrc_MustRemove)
    {
        const char *pszFirstNm = NULL;
        uint32_t    cRemoved   = 0;
        for (uint32_t i = 0; i < pOpts->cNameSpecifiers; i++)
            if (   aParsedNames[i].cchPath > 0
                && (aParsedNames[i].fNameSpecifiers & RTFSISOMAKERCMDNAME_MAJOR_MASK))
            {
                pszFirstNm = aParsedNames[i].szPath;
                uint32_t idxObj = RTFsIsoMakerGetObjIdxForPath(pOpts->hIsoMaker,
                                                               aParsedNames[i].fNameSpecifiers & RTFSISOMAKERCMDNAME_MAJOR_MASK,
                                                               aParsedNames[i].szPath);
                if (idxObj != UINT32_MAX)
                {
                    int rc = RTFsIsoMakerObjRemove(pOpts->hIsoMaker, idxObj);
                    if (RT_FAILURE(rc))
                        return rtFsIsoMakerCmdErrorRc(pOpts, rc, "Failed to remove '%s': %Rrc", pszSpecIn, rc);
                    cRemoved++;
                }
            }
        if (   enmSpecialSrc == kSpecialSrc_MustRemove
            && cRemoved == 0)
            return rtFsIsoMakerCmdErrorRc(pOpts, VERR_NOT_FOUND, "Failed to locate '%s' for removal", pszSpecIn);
    }
    /*
     * Add regular source.
     */
    else
    {
        const char     *pszSrc = aParsedNames[cParsedNames - 1].szPath;
        RTFSOBJINFO     ObjInfo;
        uint32_t        offError;
        RTERRINFOSTATIC ErrInfo;
        int rc = RTVfsChainQueryInfo(pszSrc, &ObjInfo, RTFSOBJATTRADD_UNIX,
                                     RTPATH_F_FOLLOW_LINK, &offError, RTErrInfoInitStatic(&ErrInfo));
        if (RT_FAILURE(rc))
            return rtFsIsoMakerCmdChainError(pOpts, "RTVfsChainQueryInfo", pszSrc, rc, offError, &ErrInfo.Core);

        if (RTFS_IS_FILE(ObjInfo.Attr.fMode))
            return rtFsIsoMakerCmdAddFile(pOpts, pszSrc, aParsedNames);
        if (RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode))
            return rtFsIsoMakerCmdErrorRc(pOpts, VERR_NOT_IMPLEMENTED, "Adding directory '%s' failed: not implemented", pszSpecIn);
        if (RTFS_IS_SYMLINK(ObjInfo.Attr.fMode))
            return rtFsIsoMakerCmdErrorRc(pOpts, VERR_NOT_IMPLEMENTED, "Adding symlink '%s' failed: not implemented", pszSpecIn);
        return rtFsIsoMakerCmdErrorRc(pOpts, VERR_NOT_IMPLEMENTED, "Adding special file '%s' failed: not implemented", pszSpecIn);
    }

    return VINF_SUCCESS;
}


/**
 * Deals with: -G|--generic-boot {file}
 *
 * This concers content the first 16 sectors of the image.  We start loading the
 * file at byte 0 in the image and stops at 32KB.
 *
 * @returns IPRT status code
 * @param   pOpts               The ISO maker command instance.
 * @param   pszGenericBootImage The generic boot image source.
 */
static int rtFsIsoMakerCmdOptGenericBoot(PRTFSISOMAKERCMDOPTS pOpts, const char *pszGenericBootImage)
{
    RTERRINFOSTATIC ErrInfo;
    uint32_t        offError;
    RTVFSFILE       hVfsFile;
    int rc = RTVfsChainOpenFile(pszGenericBootImage, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hVfsFile,
                                &offError, RTErrInfoInitStatic(&ErrInfo));
    if (RT_FAILURE(rc))
        return rtFsIsoMakerCmdChainError(pOpts, "RTVfsChainOpenFile", pszGenericBootImage, rc, offError, &ErrInfo.Core);

    uint8_t abBuf[_32K];
    size_t  cbRead;
    rc = RTVfsFileReadAt(hVfsFile, 0, abBuf, sizeof(abBuf), &cbRead);
    RTVfsFileRelease(hVfsFile);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerCmdErrorRc(pOpts, rc, "Error reading 32KB from generic boot image '%s': %Rrc", pszGenericBootImage, rc);

    rc = RTFsIsoMakerSetSysAreaContent(pOpts->hIsoMaker, abBuf, cbRead, 0);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerCmdErrorRc(pOpts, rc, "RTFsIsoMakerSetSysAreaContent failed with a %zu bytes input: %Rrc", cbRead, rc);

    return VINF_SUCCESS;
}


static int rtFsIsoMakerCmdOptEltoritoBoot(PRTFSISOMAKERCMDOPTS pOpts, const char *pszBootImage)
{
    RT_NOREF(pOpts, pszBootImage);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoPlatformId(PRTFSISOMAKERCMDOPTS pOpts, const char *pszPlatformId)
{
    RT_NOREF(pOpts, pszPlatformId);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoSetNotBootable(PRTFSISOMAKERCMDOPTS pOpts)
{
    RT_NOREF(pOpts);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoSetMediaType(PRTFSISOMAKERCMDOPTS pOpts, uint8_t bMediaType)
{
    RT_NOREF(pOpts, bMediaType);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoSetLoadSegment(PRTFSISOMAKERCMDOPTS pOpts, uint16_t uSeg)
{
    RT_NOREF(pOpts, uSeg);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoSetLoadSectorCount(PRTFSISOMAKERCMDOPTS pOpts, uint16_t cSectors)
{
    RT_NOREF(pOpts, cSectors);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoEnableBootInfoTablePatching(PRTFSISOMAKERCMDOPTS pOpts)
{
    RT_NOREF(pOpts);
    return VERR_NOT_IMPLEMENTED;
}

static int rtFsIsoMakerCmdOptEltoritoSetBootCatalogPath(PRTFSISOMAKERCMDOPTS pOpts, const char *pszBootCat)
{
    RT_NOREF(pOpts, pszBootCat);
    return VERR_NOT_IMPLEMENTED;
}




/**
 * Extended ISO maker command.
 *
 * This can be used as a ISO maker command that produces a image file, or
 * alternatively for setting up a virtual ISO in memory.
 *
 * @returns IPRT status code
 * @param   cArgs               Number of arguments.
 * @param   papszArgs           Pointer to argument array.
 * @param   phVfsFile           Where to return the virtual ISO.  Pass NULL to
 *                              for normal operation (creates file on disk).
 * @param   pErrInfo            Where to return extended error information in
 *                              the virtual ISO mode.
 */
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
    Opts.cNameSpecifiers        = 1;
    Opts.afNameSpecifiers[0]    = RTFSISOMAKERCMDNAME_MAJOR_MASK;
    Opts.fDstNamespaces         = RTFSISOMAKERCMDNAME_MAJOR_MASK;
    Opts.idEltoritoEntry        = UINT32_MAX;
    if (phVfsFile)
        *phVfsFile = NIL_RTVFSFILE;

    /* Setup option parsing. */
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, g_aRtFsIsoMakerOptions, RT_ELEMENTS(g_aRtFsIsoMakerOptions),
                          1 /*iFirst*/, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerCmdErrorRc(&Opts, rc, "RTGetOpt failed: %Rrc", rc);

    /* Create the ISO creator instance. */
    rc = RTFsIsoMakerCreate(&Opts.hIsoMaker);
    if (RT_FAILURE(rc))
        return rtFsIsoMakerCmdErrorRc(&Opts, rc, "RTFsIsoMakerCreate failed: %Rrc", rc);

    /*
     * Parse parameters.  Parameters are position dependent.
     */
    RTGETOPTUNION ValueUnion;
    while (   RT_SUCCESS(rc)
           && (rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            /*
             * Files and directories.
             */
            case VINF_GETOPT_NOT_OPTION:
                rc = rtFsIsoMakerCmdAddSomething(&Opts, ValueUnion.psz);
                break;

            /*
             * Options specific to our ISO maker.
             */
            case RTFSISOMAKERCMD_OPT_NAME_SETUP:
                rc = rtFsIsoMakerCmdOptNameSetup(&Opts, ValueUnion.psz);
                break;

            /*
             * Joliet related options.
             */
            case RTFSISOMAKERCMD_OPT_NO_JOLIET:
                rc = RTFsIsoMakerSetJolietUcs2Level(Opts.hIsoMaker, 0);
                if (RT_FAILURE(rc))
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "Failed to disable joliet: %Rrc", rc);
                break;


            /*
             * Boot related options.
             */
            case 'G': /* --generic-boot <file> */
                rc = rtFsIsoMakerCmdOptGenericBoot(&Opts, ValueUnion.psz);
                break;

            case 'b': /* --eltorito-boot <boot.img> */
                rc = rtFsIsoMakerCmdOptEltoritoBoot(&Opts, ValueUnion.psz);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_ALT_BOOT:
                Opts.idEltoritoEntry = UINT32_MAX;
                break;


            case RTFSISOMAKERCMD_OPT_ELTORITO_PLATFORM_ID:
                rc = rtFsIsoMakerCmdOptEltoritoPlatformId(&Opts, ValueUnion.psz);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_NO_BOOT:
                rc = rtFsIsoMakerCmdOptEltoritoSetNotBootable(&Opts);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_12:
                rc = rtFsIsoMakerCmdOptEltoritoSetMediaType(&Opts, ISO9660_ELTORITO_BOOT_MEDIA_TYPE_FLOPPY_1_2_MB);
                break;
            case RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_144:
                rc = rtFsIsoMakerCmdOptEltoritoSetMediaType(&Opts, ISO9660_ELTORITO_BOOT_MEDIA_TYPE_FLOPPY_1_44_MB);
                break;
            case RTFSISOMAKERCMD_OPT_ELTORITO_FLOPPY_288:
                rc = rtFsIsoMakerCmdOptEltoritoSetMediaType(&Opts, ISO9660_ELTORITO_BOOT_MEDIA_TYPE_FLOPPY_2_88_MB);
                break;
            case RTFSISOMAKERCMD_OPT_ELTORITO_HARD_DISK_BOOT:
                rc = rtFsIsoMakerCmdOptEltoritoSetMediaType(&Opts, ISO9660_ELTORITO_BOOT_MEDIA_TYPE_HARD_DISK);
                break;
            case RTFSISOMAKERCMD_OPT_ELTORITO_NO_EMULATION_BOOT:
                rc = rtFsIsoMakerCmdOptEltoritoSetMediaType(&Opts, ISO9660_ELTORITO_BOOT_MEDIA_TYPE_NO_EMULATION);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SEG:
                rc = rtFsIsoMakerCmdOptEltoritoSetLoadSegment(&Opts, ValueUnion.u16);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_LOAD_SIZE:
                rc = rtFsIsoMakerCmdOptEltoritoSetLoadSectorCount(&Opts, ValueUnion.u16);
                break;

            case RTFSISOMAKERCMD_OPT_ELTORITO_INFO_TABLE:
                rc = rtFsIsoMakerCmdOptEltoritoEnableBootInfoTablePatching(&Opts);
                break;

            case 'c': /* --boot-catalog <cd-path> */
                rc = rtFsIsoMakerCmdOptEltoritoSetBootCatalogPath(&Opts, ValueUnion.psz);
                break;


            /*
             * Options compatible with other ISO makers.
             */
            case 'o':
                if (Opts.fVirtualImageMaker)
                    return rtFsIsoMakerCmdSyntaxError(&Opts, "The --output option is not allowed");
                if (Opts.pszOutFile)
                    return rtFsIsoMakerCmdSyntaxError(&Opts, "The --output option is specified more than once");
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
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, VERR_GETOPT_UNKNOWN_OPTION, "Unhandled option: -%c", rc);
                else if (rc > 0)
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, VERR_GETOPT_UNKNOWN_OPTION, "Unhandled option: %i (%#x)", rc, rc);
                else if (rc == VERR_GETOPT_UNKNOWN_OPTION)
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "Unknown option: '%s'", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "%s: %Rrs\n", ValueUnion.pDef->pszLong, rc);
                else
                    rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "%Rrs\n", rc);
                return rtFsIsoMakerCmdDeleteState(&Opts, rc);
        }
    }

    /*
     * Check for mandatory options.
     */
    if (RT_SUCCESS(rc))
    {
        if (!Opts.cItemsAdded)
            rc = rtFsIsoMakerCmdErrorRc(&Opts, VERR_NO_DATA, "Cowardly refuses to create empty ISO image");
        else if (!Opts.pszOutFile && !Opts.fVirtualImageMaker)
            rc = rtFsIsoMakerCmdErrorRc(&Opts, VERR_INVALID_PARAMETER, "No output file specified (--output <file>)");
    }
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
                rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "RTFsIsoMakerCreateVfsOutputFile failed: %Rrc", rc);
        }
        else
            rc = rtFsIsoMakerCmdErrorRc(&Opts, rc, "RTFsIsoMakerFinalize failed: %Rrc", rc);
    }

    return rtFsIsoMakerCmdDeleteState(&Opts, rc);
}


/**
 * ISO maker command (creates image file on disk).
 *
 * @returns IPRT status code
 * @param   cArgs               Number of arguments.
 * @param   papszArgs           Pointer to argument array.
 */
RTDECL(RTEXITCODE) RTFsIsoMakerCmd(unsigned cArgs, char **papszArgs)
{
    int rc = RTFsIsoMakerCmdEx(cArgs, papszArgs, NULL, NULL);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

