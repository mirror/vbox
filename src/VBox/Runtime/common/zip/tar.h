/* $Id$ */
/** @file
 * IPRT - TAR Virtual Filesystem.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef __common_zip_tar_h
#define __common_zip_tar_h

#include <iprt/assert.h>

/** @name RTZIPTARHDRPOSIX::typeflag
 * @{  */
#define RTZIPTAR_TF_OLDNORMAL '\0' /**< Normal disk file, Unix compatible */
#define RTZIPTAR_TF_NORMAL    '0'  /**< Normal disk file */
#define RTZIPTAR_TF_LINK      '1'  /**< Link to previously dumped file */
#define RTZIPTAR_TF_SYMLINK   '2'  /**< Symbolic link */
#define RTZIPTAR_TF_CHR       '3'  /**< Character special file */
#define RTZIPTAR_TF_BLK       '4'  /**< Block special file */
#define RTZIPTAR_TF_DIR       '5'  /**< Directory */
#define RTZIPTAR_TF_FIFO      '6'  /**< FIFO special file */
#define RTZIPTAR_TF_CONTIG    '7'  /**< Contiguous file */
/** @} */

/**
 * The posix header (according to SuS).
 */
typedef struct RTZIPTARHDRPOSIX
{
    char    name[100];
    char    mode[8];
    char    uid[8];
    char    gid[8];
    char    size[12];
    char    mtime[12];
    char    chksum[8];
    char    typeflag;
    char    linkname[100];
    char    magic[6];
    char    version[2];
    char    uname[32];
    char    gname[32];
    char    devmajor[8];
    char    devminor[8];
    char    prefix[155];
    char    unused[12];
} RTZIPTARHDRPOSIX;
AssertCompileSize(RTZIPTARHDRPOSIX, 512);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, name,        0);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, mode,      100);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, uid,       108);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, gid,       116);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, size,      124);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, mtime,     136);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, chksum,    148);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, typeflag,  156);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, linkname,  157);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, magic,     257);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, version,   263);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, uname,     265);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, gname,     297);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, devmajor,  329);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, devminor,  337);
AssertCompileMemberOffset(RTZIPTARHDRPOSIX, prefix,    345);


/**
 * Tar header union.
 */
typedef union RTZIPTARHDR
{
    /** Byte view. */
    char            ab[512];
    /** The standard header. */
    RTZIPTARHDRPOSIX   Hdr;
} RTZIPTARHDR;
AssertCompileSize(RTZIPTARHDR, 512);
/** Pointer to a tar file header. */
typedef RTZIPTARHDR *PRTZIPTARHDR;
/** Pointer to a const tar file header. */
typedef RTZIPTARHDR const *PCRTZIPTARHDR;

#endif

