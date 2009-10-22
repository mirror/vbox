/* $Id$ */
/** @file
 * IPRT - Tar archive I/O.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "internal/iprt.h"
#include <iprt/tar.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** @name RTTARRECORD::h::linkflag
 * @{  */
#define LF_OLDNORMAL '\0' /**< Normal disk file, Unix compatible */
#define LF_NORMAL    '0'  /**< Normal disk file */
#define LF_LINK      '1'  /**< Link to previously dumped file */
#define LF_SYMLINK   '2'  /**< Symbolic link */
#define LF_CHR       '3'  /**< Character special file */
#define LF_BLK       '4'  /**< Block special file */
#define LF_DIR       '5'  /**< Directory */
#define LF_FIFO      '6'  /**< FIFO special file */
#define LF_CONTIG    '7'  /**< Contiguous file */
/** @} */

typedef union RTTARRECORD
{
    char   d[512];
    struct h
    {
        char name[100];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char chksum[8];
        char linkflag;
        char linkname[100];
        char magic[8];
        char uname[32];
        char gname[32];
        char devmajor[8];
        char devminor[8];
    } h;
} RTTARRECORD;
typedef RTTARRECORD *PRTTARRECORD;
AssertCompileSize(RTTARRECORD, 512);
AssertCompileMemberOffset(RTTARRECORD, h.size, 100+8*3);

#if 0 /* not currently used */
typedef struct RTTARFILELIST
{
    char *pszFilename;
    RTTARFILELIST *pNext;
} RTTARFILELIST;
typedef RTTARFILELIST *PRTTARFILELIST;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int rtTarCalcChkSum(PRTTARRECORD pRecord, uint32_t *pChkSum)
{
    uint32_t check = 0;
    uint32_t zero = 0;
    for (size_t i = 0; i < sizeof(RTTARRECORD); ++i)
    {
        /* Calculate the sum of every byte from the header. The checksum field
         * itself is counted as all blanks. */
        if (   i <  RT_UOFFSETOF(RTTARRECORD, h.chksum)
            || i >= RT_UOFFSETOF(RTTARRECORD, h.linkflag))
            check += pRecord->d[i];
        else
            check += ' ';
        /* Additional check if all fields are zero, which indicate EOF. */
        zero += pRecord->d[i];
    }

    /* EOF? */
    if (!zero)
        return VERR_EOF;

    *pChkSum = check;
    return VINF_SUCCESS;
}

static int rtTarCheckHeader(PRTTARRECORD pRecord)
{
    uint32_t check;
    int rc = rtTarCalcChkSum(pRecord, &check);
    /* EOF? */
    if (RT_FAILURE(rc))
        return rc;

    /* Verify the checksum */
    uint32_t sum;
    rc = RTStrToUInt32Full(pRecord->h.chksum, 8, &sum);
    if (RT_SUCCESS(rc) && sum == check)
        return VINF_SUCCESS;
    return VERR_TAR_CHKSUM_MISMATCH;
}

static int rtTarCopyFileFrom(RTFILE hFile, const char *pszTargetName, PRTTARRECORD pRecord)
{
    RTFILE hNewFile;
    /* Open the target file */
    int rc = RTFileOpen(&hNewFile, pszTargetName, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

/**@todo r=bird: Use a bigger buffer here, see comment in rtTarCopyFileTo. */

    uint64_t cbToCopy = RTStrToUInt64(pRecord->h.size);
    size_t cbAllWritten = 0;
    RTTARRECORD record;
    /* Copy the content from hFile over to pszTargetName. This is done block
     * wise in 512 byte steps. After this copying is finished hFile will be on
     * a 512 byte boundary, regardless if the file copied is 512 byte size
     * aligned. */
    for (;;)
    {
        /* Finished already? */
        if (cbAllWritten == cbToCopy)
            break;
        /* Read one block */
        rc = RTFileRead(hFile, &record, sizeof(record), NULL);
        if (RT_FAILURE(rc))
            break;
        size_t cbToWrite = sizeof(record);
        /* Check for the last block which has not to be 512 bytes in size. */
        if (cbAllWritten + cbToWrite > cbToCopy)
            cbToWrite = cbToCopy - cbAllWritten;
        /* Write the block */
        rc = RTFileWrite(hNewFile, &record, cbToWrite, NULL);
        if (RT_FAILURE(rc))
            break;
        /* Count how many bytes are written already */
        cbAllWritten += cbToWrite;
    }

    /* Now set all file attributes */
    if (RT_SUCCESS(rc))
    {
        int32_t mode;
        rc = RTStrToInt32Full(pRecord->h.mode, 8, &mode);
        if (RT_SUCCESS(rc))
        {
            mode |= RTFS_TYPE_FILE; /* For now we support regular files only */
            /* Set the mode */
            rc = RTFileSetMode(hNewFile, mode);
        }
    }
    /* Make sure the called doesn't mix truncated tar files with the official
     * end indicated by rtTarCalcChkSum. */
    else if (rc == VERR_EOF)
        rc = VERR_FILE_IO_ERROR;

    RTFileClose(hNewFile);

    /* Delete the freshly created file in the case of an error */
    if (RT_FAILURE(rc))
        RTFileDelete(pszTargetName);

    return rc;
}

static int rtTarCopyFileTo(RTFILE hFile, const char *pszSrcName)
{
    RTFILE hOldFile;
    /* Open the source file */
    int rc = RTFileOpen(&hOldFile, pszSrcName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    /* Get the size of the source file */
    uint64_t cbSize;
    rc = RTFileGetSize(hOldFile, &cbSize);
    if (RT_FAILURE(rc))
    {
        RTFileClose(hOldFile);
        return rc;
    }
    /* Get some info from the source file */
    RTFSOBJINFO info;
    RTUID uid = 0;
    RTGID gid = 0;
    RTFMODE fmode = 0600; /* Make some save default */
    int64_t mtime = 0;
    /* This isn't critical. Use the defaults if it fails. */
    rc = RTFileQueryInfo(hOldFile, &info, RTFSOBJATTRADD_UNIX);
    if (RT_SUCCESS(rc))
    {
        fmode = info.Attr.fMode & RTFS_UNIX_MASK;
        uid = info.Attr.u.Unix.uid;
        gid = info.Attr.u.Unix.gid;
        mtime = RTTimeSpecGetSeconds(&info.ModificationTime);
    }

    /* Fill the header record */
    RTTARRECORD record;
    RT_ZERO(record);
    RTStrPrintf(record.h.name,  sizeof(record.h.name),  "%s",     RTPathFilename(pszSrcName));
    RTStrPrintf(record.h.mode,  sizeof(record.h.mode),  "%0.7o",  fmode);
    RTStrPrintf(record.h.uid,   sizeof(record.h.uid),   "%0.7o",  uid);
    RTStrPrintf(record.h.gid,   sizeof(record.h.gid),   "%0.7o",  gid);
    RTStrPrintf(record.h.size,  sizeof(record.h.size),  "%0.11o", cbSize);
    RTStrPrintf(record.h.mtime, sizeof(record.h.mtime), "%0.11o", mtime);
    RTStrPrintf(record.h.magic, sizeof(record.h.magic), "ustar  ");
    RTStrPrintf(record.h.uname, sizeof(record.h.uname), "someone");
    RTStrPrintf(record.h.gname, sizeof(record.h.gname), "someone");
    record.h.linkflag = LF_NORMAL;

    /* Create the checksum out of the new header */
    uint32_t chksum;
    rc = rtTarCalcChkSum(&record, &chksum);
    if (RT_SUCCESS(rc))
    {
        RTStrPrintf(record.h.chksum, sizeof(record.h.chksum), "%0.7o", chksum);

        /* Write the header first */
        rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
        if (RT_SUCCESS(rc))
        {
/** @todo r=bird: using a 64KB buffer here instead of 0.5KB would probably be
 *        a good thing. */
            uint64_t cbAllWritten = 0;
            /* Copy the content from pszSrcName over to hFile. This is done block
             * wise in 512 byte steps. After this copying is finished hFile will be
             * on a 512 byte boundary, regardless if the file copied is 512 byte
             * size aligned. */
            for (;;)
            {
                if (cbAllWritten >= cbSize)
                    break;
                size_t cbToRead = sizeof(record);
                /* Last record? */
                if (cbAllWritten + cbToRead > cbSize)
                {
                    /* Initialize with zeros */
                    RT_ZERO(record);
                    cbToRead = cbSize - cbAllWritten;
                }
                /* Read one block */
                rc = RTFileRead(hOldFile, &record, cbToRead, NULL);
                if (RT_FAILURE(rc))
                    break;
                /* Write one block */
                rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
                if (RT_FAILURE(rc))
                    break;
                /* Count how many bytes are written already */
                cbAllWritten += sizeof(record);
            }

            /* Make sure the called doesn't mix truncated tar files with the
             * official end indicated by rtTarCalcChkSum. */
            if (rc == VERR_EOF)
                rc == VERR_FILE_IO_ERROR;
        }
    }

    RTFileClose(hOldFile);
    return rc;
}

static int rtTarSkipData(RTFILE hFile, PRTTARRECORD pRecord)
{
    int rc = VINF_SUCCESS;
    /* Seek over the data parts (512 bytes aligned) */
    int64_t offSeek = RT_ALIGN(RTStrToInt64(pRecord->h.size), sizeof(RTTARRECORD));
    if (offSeek > 0)
        rc = RTFileSeek(hFile, offSeek, RTFILE_SEEK_CURRENT, NULL);
    return rc;
}


RTR3DECL(int) RTTarQueryFileExists(const char *pszTarFile, const char *pszFile)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;

    bool fFound = false;
    RTTARRECORD record;
    for (;;)
    {
/** @todo r=bird: the reading, validation and EOF check done here should be
 *        moved to a separate helper function. That would make it easiser to
 *        distinguish genuine-end-of-tar-file and VERR_EOF caused by a
 *        trunacted file. That said, rtTarSkipData won't return VERR_EOF, at
 *        least not on unix, since it's not a sin to seek beyond the end of a
 *        file. */
        rc = RTFileRead(hFile, &record, sizeof(record), NULL);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (   record.h.linkflag == LF_OLDNORMAL
            || record.h.linkflag == LF_NORMAL)
        {
            if (!RTStrCmp(record.h.name, pszFile))
            {
                fFound = true;
                break;
            }
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* Something found? */
    if (    RT_SUCCESS(rc)
        &&  !fFound)
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

RTR3DECL(int) RTTarList(const char *pszTarFile, char ***ppapszFiles, size_t *pcFiles)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppapszFiles, VERR_INVALID_POINTER);
    AssertPtrReturn(pcFiles, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;

    /* Initialize the file name array with one slot */
    size_t cFilesAlloc = 1;
    char **papszFiles = (char**)RTMemAlloc(sizeof(char *));
    if (!papszFiles)
    {
        RTFileClose(hFile);
        return VERR_NO_MEMORY;
    }

    /* Iterate through the tar file record by record. Skip data records as we
     * didn't need them. */
    RTTARRECORD record;
    size_t cFiles = 0;
    for (;;)
    {
        rc = RTFileRead(hFile, &record, sizeof(record), NULL);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (   record.h.linkflag == LF_OLDNORMAL
            || record.h.linkflag == LF_NORMAL)
        {
            if (cFiles >= cFilesAlloc)
            {
                /* Double the array size, make sure the size doesn't wrap. */
                void  *pvNew = NULL;
                size_t cbNew = cFilesAlloc * sizeof(char *) * 2;
                if (cbNew / sizeof(char *) / 2 == cFilesAlloc)
                    pvNew = RTMemRealloc(papszFiles, cbNew);
                if (!pvNew)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                papszFiles = (char **)pvNew;
                cFilesAlloc *= 2;
            }

            /* Duplicate the name */
            papszFiles[cFiles] = RTStrDup(record.h.name);
            if (!papszFiles[cFiles])
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            cFiles++;
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* Return the file array on success, dispose of it on failure. */
    if (RT_SUCCESS(rc))
    {
        *pcFiles = cFiles;
        *ppapszFiles = papszFiles;
    }
    else
    {
        while (cFiles-- > 0)
            RTStrFree(papszFiles[cFiles]);
        RTMemFree(papszFiles);
    }
    return rc;
}

RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszOutputDir, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;

    /* Iterate through the tar file record by record. */
    RTTARRECORD record;
    char **paExtracted = (char **)RTMemTmpAllocZ(sizeof(char *) * cFiles);
    if (paExtracted)
    {
        size_t cExtracted = 0;
        for (;;)
        {
            rc = RTFileRead(hFile, &record, sizeof(record), NULL);
            /* Check for error or EOF. */
            if (RT_FAILURE(rc))
                break;
            /* Check for EOF & data integrity */
            rc = rtTarCheckHeader(&record);
            if (RT_FAILURE(rc))
                break;
            /* We support normal files only */
            if (   record.h.linkflag == LF_OLDNORMAL
                || record.h.linkflag == LF_NORMAL)
            {
                bool fFound = false;
                for (size_t i = 0; i < cFiles; ++i)
                {
                    if (!RTStrCmp(record.h.name, papszFiles[i]))
                    {
                        fFound = true;
                        if (cExtracted < cFiles)
                        {
                            char *pszTargetFile;
                            rc = RTStrAPrintf(&pszTargetFile, "%s/%s", pszOutputDir, papszFiles[i]);
                            if (rc > 0)
                            {
                                rc = rtTarCopyFileFrom(hFile, pszTargetFile, &record);
                                if (RT_SUCCESS(rc))
                                    paExtracted[cExtracted++] = pszTargetFile;
                                else
                                    RTStrFree(pszTargetFile);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                        else
                            rc = VERR_ALREADY_EXISTS;
                        break;
                    }
                }
                if (RT_FAILURE(rc))
                    break;
                /* If the current record isn't a file in the file list we have to
                 * skip the data */
                if (!fFound)
                {
                    rc = rtTarSkipData(hFile, &record);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
        }

        if (rc == VERR_EOF)
            rc = VINF_SUCCESS;

        /* If we didn't found all files, indicate an error */
        if (cExtracted != cFiles && RT_SUCCESS(rc))
            rc = VERR_FILE_NOT_FOUND;

        /* Cleanup the names of the extracted files, deleting them on failure. */
        while (cExtracted-- > 0)
        {
            if (RT_FAILURE(rc))
                RTFileDelete(paExtracted[cExtracted]);
            RTStrFree(paExtracted[cExtracted]);
        }
        RTMemTmpFree(paExtracted);
    }
    else
        rc = VERR_NO_TMP_MEMORY;

    RTFileClose(hFile);
    return rc;
}

RTR3DECL(int) RTTarExtractByIndex(const char *pszTarFile, const char *pszOutputDir, size_t iIndex, char **ppszFileName)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszOutputDir, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;

    /* Iterate through the tar file record by record. */
    RTTARRECORD record;
    size_t iFile = 0;
    bool fFound = false;
    for (;;)
    {
        rc = RTFileRead(hFile, &record, sizeof(record), NULL);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (   record.h.linkflag == LF_OLDNORMAL
            || record.h.linkflag == LF_NORMAL)
        {
            if (iIndex == iFile)
            {
                fFound = true;
                char *pszTargetName;
                rc = RTStrAPrintf(&pszTargetName, "%s/%s", pszOutputDir, record.h.name);
                if (rc > 0)
                {
                    rc = rtTarCopyFileFrom(hFile, pszTargetName, &record);
                    /* On success pass on the filename if requested. */
                    if (    RT_SUCCESS(rc)
                        &&  ppszFileName)
                        *ppszFileName = pszTargetName;
                    else
                        RTStrFree(pszTargetName);
                }
                else
                    rc = VERR_NO_MEMORY;
                break;
            }
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
        ++iFile;
    }

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* If we didn't found the index, indicate an error */
    if (!fFound && RT_SUCCESS(rc))
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

RTR3DECL(int) RTTarCreate(const char *pszTarFile, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i = 0; i < cFiles; ++i)
    {
        rc = rtTarCopyFileTo(hFile, papszFiles[i]);
        if (RT_FAILURE(rc))
            break;
    }

    /* gtar gives a warning, but the documentation says EOF is indicated by a
     * zero block. Disabled for now. */
#if 0
    if (RT_SUCCESS(rc))
    {
        /* Append the EOF record which is filled all by zeros */
        RTTARRECORD record;
        ASMMemFill32(&record, sizeof(record), 0);
        rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
    }
#endif

    /* Time to close the new tar archive */
    RTFileClose(hFile);

    /* Delete the freshly created tar archive on failure */
    if (RT_FAILURE(rc))
        RTFileDelete(pszTarFile);

    return rc;
}

