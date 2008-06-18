/* $Id$ */
/** @file
 * IPRT - File System.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef RT_OS_WINDOWS
# define RTTIME_INCL_TIMESPEC
# include <sys/time.h>
#endif

#include <iprt/fs.h>
#include <iprt/assert.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/ctype.h>
#include "internal/fs.h"


/**
 * Converts dos-style attributes to Unix attributes.
 *
 * @returns
 * @param   fMode       The mode mask containing dos-style attibutes only.
 * @param   pszName     The filename which this applies to (exe check).
 * @param   cbName      The length of that filename. (optional, set 0)
 */
RTFMODE rtFsModeFromDos(RTFMODE fMode, const char *pszName, unsigned cbName)
{
    fMode &= ~((1 << RTFS_DOS_SHIFT) - 1);

    /* everything is readable. */
    fMode |= RTFS_UNIX_IRUSR | RTFS_UNIX_IRGRP | RTFS_UNIX_IROTH;
    if (fMode & RTFS_DOS_DIRECTORY)
        /* directories are executable. */
        fMode |= RTFS_TYPE_DIRECTORY | RTFS_UNIX_IXUSR | RTFS_UNIX_IXGRP | RTFS_UNIX_IXOTH;
    else
    {
        fMode |= RTFS_TYPE_FILE;
        if (!cbName && pszName)
            cbName = strlen(pszName);
        if (cbName >= 4 && pszName[cbName - 4] == '.')
        {
            /* check for executable extension. */
            const char *pszExt = &pszName[cbName - 3];
            char szExt[4];
            szExt[0] = tolower(pszExt[0]);
            szExt[1] = tolower(pszExt[1]);
            szExt[2] = tolower(pszExt[2]);
            szExt[3] = '\0';
            if (    !memcmp(szExt, "exe", 4)
                ||  !memcmp(szExt, "bat", 4)
                ||  !memcmp(szExt, "com", 4)
                ||  !memcmp(szExt, "cmd", 4)
                ||  !memcmp(szExt, "btm", 4)
               )
                fMode |= RTFS_UNIX_IXUSR | RTFS_UNIX_IXGRP | RTFS_UNIX_IXOTH;
        }
    }
    /* writable? */
    if (!(fMode & RTFS_DOS_READONLY))
        fMode |= RTFS_UNIX_IWUSR | RTFS_UNIX_IWGRP | RTFS_UNIX_IWOTH;
    return fMode;
}


/**
 * Converts Unix attributes to Dos-style attributes.
 *
 * @returns File mode mask.
 * @param   fMode       The mode mask containing dos-style attibutes only.
 * @param   pszName     The filename which this applies to (hidden check).
 * @param   cbName      The length of that filename. (optional, set 0)
 */
RTFMODE rtFsModeFromUnix(RTFMODE fMode, const char *pszName, unsigned cbName)
{
    fMode &= RTFS_UNIX_MASK;

    if (!(fMode & (RTFS_UNIX_IWUSR | RTFS_UNIX_IWGRP | RTFS_UNIX_IWOTH)))
        fMode |= RTFS_DOS_READONLY;
    if (RTFS_IS_DIRECTORY(fMode))
        fMode |= RTFS_DOS_DIRECTORY;
    if (!(fMode & RTFS_DOS_MASK))
        fMode |= RTFS_DOS_NT_NORMAL;
    if (!(fMode & RTFS_DOS_HIDDEN) && pszName)
    {
        pszName = RTPathFilename(pszName);
        if (pszName && *pszName == '.')
            fMode |= RTFS_DOS_HIDDEN;
    }
    return fMode;
}


/**
 * Converts dos-style attributes to Unix attributes.
 *
 * @returns Normalized file mode.
 * @param   fMode       The mode mask containing dos-style attibutes only.
 * @param   pszName     The filename which this applies to (exe check).
 * @param   cbName      The length of that filename. (optional, set 0)
 */
RTFMODE rtFsModeNormalize(RTFMODE fMode, const char *pszName, unsigned cbName)
{
    if (!(fMode & RTFS_UNIX_MASK))
        fMode = rtFsModeFromDos(fMode, pszName, cbName);
    else if (!(fMode & RTFS_DOS_MASK))
        fMode = rtFsModeFromUnix(fMode, pszName, cbName);
    else if (!(fMode & RTFS_TYPE_MASK))
        fMode |= fMode & RTFS_DOS_DIRECTORY ? RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE;
    else if (RTFS_IS_DIRECTORY(fMode))
        fMode |= RTFS_DOS_DIRECTORY;
    return fMode;
}


/**
 * Checks if the file mode is valid or not.
 *
 * @return  true if valid.
 * @return  false if invalid, done bitching.
 * @param   fMode       The file mode.
 */
bool rtFsModeIsValid(RTFMODE fMode)
{
    AssertMsgReturn(   (!RTFS_IS_DIRECTORY(fMode) && !(fMode & RTFS_DOS_DIRECTORY))
                    || (RTFS_IS_DIRECTORY(fMode) && (fMode & RTFS_DOS_DIRECTORY)),
                    ("%RTfmode\n", fMode), false);
    AssertMsgReturn(RTFS_TYPE_MASK & fMode,
                    ("%RTfmode\n", fMode), false);
    /** @todo more checks! */
    return true;
}


/**
 * Checks if the file mode is valid as a permission mask or not.
 *
 * @return  true if valid.
 * @return  false if invalid, done bitching.
 * @param   fMode       The file mode.
 */
bool rtFsModeIsValidPermissions(RTFMODE fMode)
{
    AssertMsgReturn(   (!RTFS_IS_DIRECTORY(fMode) && !(fMode & RTFS_DOS_DIRECTORY))
                    || (RTFS_IS_DIRECTORY(fMode) && (fMode & RTFS_DOS_DIRECTORY)),
                    ("%RTfmode\n", fMode), false);
    /** @todo more checks! */
    return true;
}


#ifndef RT_OS_WINDOWS
/**
 * Internal worker function which setups RTFSOBJINFO based on a UNIX stat struct.
 *
 * @param   pObjInfo        The file system object info structure to setup.
 * @param   pStat           The stat structure to use.
 * @param   pszName         The filename which this applies to (exe/hidden check).
 * @param   cbName          The length of that filename. (optional, set 0)
 */
void rtFsConvertStatToObjInfo(PRTFSOBJINFO pObjInfo, const struct stat *pStat, const char *pszName, unsigned cbName)
{
    pObjInfo->cbObject    = pStat->st_size;
    pObjInfo->cbAllocated = pStat->st_size;

#ifdef HAVE_STAT_NSEC
    RTTimeSpecAddNano(RTTimeSpecSetSeconds(&pObjInfo->AccessTime,       pStat->st_atime),     pStat->st_atimensec);
    RTTimeSpecAddNano(RTTimeSpecSetSeconds(&pObjInfo->ModificationTime, pStat->st_mtime),     pStat->st_mtimensec);
    RTTimeSpecAddNano(RTTimeSpecSetSeconds(&pObjInfo->ChangeTime,       pStat->st_ctime),     pStat->st_ctimensec);
#ifdef HAVE_STAT_BIRTHTIME
    RTTimeSpecAddNano(RTTimeSpecSetSeconds(&pObjInfo->BirthTime,        pStat->st_birthtime), pStat->st_birthtimensec);
#endif

#elif defined(HAVE_STAT_TIMESPEC_BRIEF)
    RTTimeSpecSetTimespec(&pObjInfo->AccessTime,       &pStat->st_atim);
    RTTimeSpecSetTimespec(&pObjInfo->ModificationTime, &pStat->st_mtim);
    RTTimeSpecSetTimespec(&pObjInfo->ChangeTime,       &pStat->st_ctim);
# ifdef HAVE_STAT_BIRTHTIME
    RTTimeSpecSetTimespec(&pObjInfo->BirthTime,        &pStat->st_birthtim);
# endif

#elif defined(HAVE_STAT_TIMESPEC)
    RTTimeSpecSetTimespec(&pObjInfo->AccessTime,       pStat->st_atimespec);
    RTTimeSpecSetTimespec(&pObjInfo->ModificationTime, pStat->st_mtimespec);
    RTTimeSpecSetTimespec(&pObjInfo->ChangeTime,       pStat->st_ctimespec);
# ifdef HAVE_STAT_BIRTHTIME
    RTTimeSpecSetTimespec(&pObjInfo->BirthTime,        pStat->st_birthtimespec);
# endif

#else /* just the normal stuff */
    RTTimeSpecSetSeconds(&pObjInfo->AccessTime,       pStat->st_atime);
    RTTimeSpecSetSeconds(&pObjInfo->ModificationTime, pStat->st_mtime);
    RTTimeSpecSetSeconds(&pObjInfo->ChangeTime,       pStat->st_ctime);
# ifdef HAVE_STAT_BIRTHTIME
    RTTimeSpecSetSeconds(&pObjInfo->BirthTime,        pStat->st_birthtime);
# endif
#endif
#ifndef HAVE_STAT_BIRTHTIME
    pObjInfo->BirthTime = pObjInfo->ChangeTime;
#endif


    /* the file mode */
    RTFMODE fMode = pStat->st_mode & RTFS_UNIX_MASK;
    Assert(RTFS_UNIX_ISUID == S_ISUID);
    Assert(RTFS_UNIX_ISGID == S_ISGID);
#ifdef S_ISTXT
    Assert(RTFS_UNIX_ISTXT == S_ISTXT);
#elif defined(S_ISVTX)
    Assert(RTFS_UNIX_ISTXT == S_ISVTX);
#else
#error "S_ISVTX / S_ISTXT isn't defined"
#endif
    Assert(RTFS_UNIX_IRWXU == S_IRWXU);
    Assert(RTFS_UNIX_IRUSR == S_IRUSR);
    Assert(RTFS_UNIX_IWUSR == S_IWUSR);
    Assert(RTFS_UNIX_IXUSR == S_IXUSR);
    Assert(RTFS_UNIX_IRWXG == S_IRWXG);
    Assert(RTFS_UNIX_IRGRP == S_IRGRP);
    Assert(RTFS_UNIX_IWGRP == S_IWGRP);
    Assert(RTFS_UNIX_IXGRP == S_IXGRP);
    Assert(RTFS_UNIX_IRWXO == S_IRWXO);
    Assert(RTFS_UNIX_IROTH == S_IROTH);
    Assert(RTFS_UNIX_IWOTH == S_IWOTH);
    Assert(RTFS_UNIX_IXOTH == S_IXOTH);
    Assert(RTFS_TYPE_FIFO == S_IFIFO);
    Assert(RTFS_TYPE_DEV_CHAR == S_IFCHR);
    Assert(RTFS_TYPE_DIRECTORY == S_IFDIR);
    Assert(RTFS_TYPE_DEV_BLOCK == S_IFBLK);
    Assert(RTFS_TYPE_FILE == S_IFREG);
    Assert(RTFS_TYPE_SYMLINK == S_IFLNK);
    Assert(RTFS_TYPE_SOCKET == S_IFSOCK);
#ifdef S_IFWHT
    Assert(RTFS_TYPE_WHITEOUT == S_IFWHT);
#endif
    Assert(RTFS_TYPE_MASK == S_IFMT);

    pObjInfo->Attr.fMode  = rtFsModeFromUnix(fMode, pszName, cbName);

    /* additional unix attribs */
    pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_UNIX;
    pObjInfo->Attr.u.Unix.uid             = pStat->st_uid;
    pObjInfo->Attr.u.Unix.gid             = pStat->st_gid;
    pObjInfo->Attr.u.Unix.cHardlinks      = pStat->st_nlink;
    pObjInfo->Attr.u.Unix.INodeIdDevice   = pStat->st_dev;
    pObjInfo->Attr.u.Unix.INodeId         = pStat->st_ino;
#ifdef HAVE_STAT_FLAGS
    pObjInfo->Attr.u.Unix.fFlags          = pStat->st_flags;
#else
    pObjInfo->Attr.u.Unix.fFlags          = 0;
#endif
#ifdef HAVE_STAT_GEN
    pObjInfo->Attr.u.Unix.GenerationId    = pStat->st_gen;
#else
    pObjInfo->Attr.u.Unix.GenerationId    = 0;
#endif
    pObjInfo->Attr.u.Unix.Device          = pStat->st_rdev;
}

#endif /* !RT_OS_WINDOWS */
