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

#include "iprt/tar.h"

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/string.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

#define LF_OLDNORMAL '\0' /* Normal disk file, Unix compatible */
#define LF_NORMAL    '0'  /* Normal disk file */
#define LF_LINK      '1'  /* Link to previously dumped file */
#define LF_SYMLINK   '2'  /* Symbolic link */
#define LF_CHR       '3'  /* Character special file */
#define LF_BLK       '4'  /* Block special file */
#define LF_DIR       '5'  /* Directory */
#define LF_FIFO      '6'  /* FIFO special file */
#define LF_CONTIG    '7'  /* Contiguous file */

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

typedef struct RTTARFILELIST
{
    char *pszFilename;
    RTTARFILELIST *pNext;
} RTTARFILELIST;
typedef RTTARFILELIST *PRTTARFILELIST;

/*******************************************************************************
*   Private RTTar helper                                                       *
*******************************************************************************/

static int rtTarCalcChkSum(PRTTARRECORD pRecord, uint32_t *pChkSum)
{
    uint32_t check = 0;
    uint32_t zero = 0;
    for (size_t i=0; i < sizeof(RTTARRECORD); ++i)
    {
        /* Calculate the sum of every byte from the header. The checksum field
         * itself is counted as all blanks. */
        if (i < RT_UOFFSETOF(RTTARRECORD, h.chksum) ||
            i >= RT_UOFFSETOF(RTTARRECORD, h.linkflag))
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
    if (sum == check)
        return VINF_SUCCESS;
    else
        return VERR_TAR_CHKSUM_MISMATCH;
}

static int rtTarCopyFileFrom(RTFILE hFile, const char* pszTargetName, PRTTARRECORD pRecord)
{
    RTFILE hNewFile;
    /* Open the target file */
    int rc = RTFileOpen(&hNewFile, pszTargetName, RTFILE_O_CREATE | RTFILE_O_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    uint64_t cbToCopy = RTStrToUInt64(pRecord->h.size);
    size_t cbRead = 0;
    size_t cbWritten = 0;
    size_t cbAllWritten = 0;
    RTTARRECORD record;
    /* Copy the content from hFile over to pszTargetName. This is done block
     * wise in 512 byte steps. After this copying is finished hFile will be on
     * a 512 byte boundary, regardless if the file copied is 512 byte size
     * aligned. */
    do
    {
        /* Finished already? */
        if (cbAllWritten == cbToCopy)
            break;
        /* Read one block */
        rc = RTFileRead(hFile, &record, sizeof(record), &cbRead);
        if (RT_FAILURE(rc) ||
            cbRead != sizeof(record))
        {
            rc = VERR_FILE_IO_ERROR;
            break;
        }
        size_t cbToWrite = sizeof(record);
        /* Check for the last block which has not to be 512 bytes in size. */
        if (cbAllWritten + cbToWrite > cbToCopy)
            cbToWrite = cbToCopy - cbAllWritten;
        /* Write the block */
        rc = RTFileWrite(hNewFile, &record, cbToWrite, &cbWritten);
        if (RT_FAILURE(rc) ||
            cbWritten != cbToWrite)
        {
            rc = VERR_FILE_IO_ERROR;
            break;
        }
        /* Count how many bytes are written already */
        cbAllWritten += cbWritten;
    }
    while(1);

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

    RTFileClose(hNewFile);

    /* Delete the freshly created file in the case of an error */
    if (RT_FAILURE(rc))
        RTFileDelete(pszTargetName);

    return rc;
}

static int rtTarCopyFileTo(RTFILE hFile, const char* pszSrcName)
{
    RTFILE hOldFile;
    /* Open the source file */
    int rc = RTFileOpen(&hOldFile, pszSrcName, RTFILE_O_READ);
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
    ASMMemFill32(&record, sizeof(record), 0); /* Initialize with zeros */
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
    rtTarCalcChkSum(&record, &chksum);
    RTStrPrintf(record.h.chksum, sizeof(record.h.chksum), "%0.7o", chksum);

    /* Write the header first */
    rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
    if (RT_SUCCESS(rc))
    {
        size_t cbToRead = 0;
        size_t cbWritten = 0;
        size_t cbAllWritten = 0;
        /* Copy the content from pszSrcName over to hFile. This is done block
         * wise in 512 byte steps. After this copying is finished hFile will be
         * on a 512 byte boundary, regardless if the file copied is 512 byte
         * size aligned. */
        do
        {
            if (cbAllWritten >= cbSize)
                break;
            cbToRead = sizeof(record);
            /* Last record? */
            if (cbAllWritten + cbToRead > cbSize)
            {
                /* Initialize with zeros */
                ASMMemFill32(&record, sizeof(record), 0);
                cbToRead = cbSize - cbAllWritten;
            }
            /* Read one block */
            rc = RTFileRead(hOldFile, &record, cbToRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Write one block */
            rc = RTFileWrite(hFile, &record, sizeof(record), &cbWritten);
            if (RT_FAILURE(rc))
                break;
            /* Count how many bytes are written already */
            cbAllWritten += cbWritten;
        }
        while(1);
    }
    RTFileClose(hOldFile);

    return rc;
}

static int rtTarSkipData(RTFILE hFile, PRTTARRECORD pRecord)
{
    int rc = VINF_SUCCESS;
    uint64_t offAct;
    /* Seek over the data parts (512 bytes aligned) */
    int64_t offSeek = RT_ALIGN(RTStrToInt64(pRecord->h.size), sizeof(RTTARRECORD));
    if (offSeek > 0)
        rc = RTFileSeek(hFile, offSeek, RTFILE_SEEK_CURRENT, &offAct);
    return rc;
}

/*******************************************************************************
*   Public RTTar interface                                                     *
*******************************************************************************/

RTR3DECL(int) RTTarExists(const char *pszTarFile, const char *pszFile)
{
    /* Validate input */
    if (RT_UNLIKELY(!pszTarFile || !pszFile))
    {
        AssertMsgFailed(("Must supply pszTarFile, pszFile\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return rc;

    bool fFound = false;
    RTTARRECORD record;
    size_t cbRead;
    do
    {
        rc = RTFileRead(hFile, &record, sizeof(record), &cbRead);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (record.h.linkflag == LF_OLDNORMAL ||
            record.h.linkflag == LF_NORMAL)
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
    while(1);

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* Something found? */
    if (RT_SUCCESS(rc) &&
        fFound == false)
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

RTR3DECL(int) RTTarList(const char *pszTarFile, char ***ppapszFiles, size_t *pcFiles)
{
    /* Validate input */
    if (RT_UNLIKELY(!pszTarFile || !ppapszFiles || !pcFiles))
    {
        AssertMsgFailed(("Must supply pszTarFile, ppapszFiles, pcFiles!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return rc;

    /* Iterate through the tar file record by record. Skip data records as we
     * didn't need them. */
    RTTARRECORD record;
    size_t cbRead;
    PRTTARFILELIST pCurr = NULL;
    PRTTARFILELIST pFirst = NULL;
    size_t cCount = 0;
    do
    {
        rc = RTFileRead(hFile, &record, sizeof(record), &cbRead);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (record.h.linkflag == LF_OLDNORMAL ||
            record.h.linkflag == LF_NORMAL)
        {
            PRTTARFILELIST pNew = (PRTTARFILELIST)RTMemAllocZ(sizeof(RTTARFILELIST));
            ++cCount;
            /* Fill our linked list */
            if (pCurr)
                pCurr->pNext = pNew;
            else
                pFirst = pNew;
            pCurr = pNew;
            pCurr->pszFilename = RTStrDup(record.h.name);
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }
    while(1);

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* On success copy the filenames over to the user provided array pointer */
    if (RT_SUCCESS(rc) &&
        cCount > 0)
    {
        *ppapszFiles = (char**)RTMemAlloc(sizeof(char*)*cCount);
        pCurr = pFirst;
        size_t i = 0;
        while (pCurr)
        {
            (*ppapszFiles)[i++] = pCurr->pszFilename;
            PRTTARFILELIST pTmp = pCurr->pNext;
            /* Free the linked list node */
            RTMemFree(pCurr);
            pCurr = pTmp;
        }
        *pcFiles = cCount;
    }

    return rc;
}

RTR3DECL(int) RTTarExtract(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    if (RT_UNLIKELY(!pszTarFile || !pszOutputDir || !papszFiles))
    {
        AssertMsgFailed(("Must supply pszTarFile, pszOutputDir, papszFiles!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return rc;

    /* Iterate through the tar file record by record. */
    RTTARRECORD record;
    PRTTARFILELIST pCurr = NULL;
    PRTTARFILELIST pFirst = NULL;
    size_t cbRead = 0;
    size_t cCount = 0;
    do
    {
        rc = RTFileRead(hFile, &record, sizeof(record), &cbRead);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (record.h.linkflag == LF_OLDNORMAL ||
            record.h.linkflag == LF_NORMAL)
        {
            bool fFound = false;
            for (size_t i=0; i < cFiles; ++i)
            {
                if (!RTStrCmp(record.h.name, papszFiles[i]))
                {
                    ++cCount; fFound = true;
                    char *pszTargetName;
                    RTStrAPrintf(&pszTargetName, "%s/%s", pszOutputDir, papszFiles[i]);
                    rc = rtTarCopyFileFrom(hFile, pszTargetName, &record);
                    if (RT_FAILURE(rc))
                        break;
                    PRTTARFILELIST pNew = (PRTTARFILELIST)RTMemAllocZ(sizeof(RTTARFILELIST));
                    /* Fill our linked list */
                    if (pCurr)
                        pCurr->pNext = pNew;
                    else
                        pFirst = pNew;
                    pCurr = pNew;
                    pCurr->pszFilename = pszTargetName;
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
    while(1);

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* If we didn't found all files, indicate an error */
    if (cCount != cFiles)
        rc = VERR_FILE_NOT_FOUND;

    pCurr = pFirst;
    while (pCurr)
    {
        /* If there was a failure during extraction, delete all files which
         * were extracted already */
        if (RT_FAILURE(rc))
            RTFileDelete(pCurr->pszFilename);
        /* Delete the filename string and the list node */
        RTStrFree(pCurr->pszFilename);
        PRTTARFILELIST pTmp = pCurr->pNext;
        RTMemFree(pCurr);
        pCurr = pTmp;
    }

    return rc;
}

RTR3DECL(int) RTTarExtractIndex(const char *pszTarFile, const char *pszOutputDir, size_t iIndex, char** ppszFileName)
{
    /* Validate input */
    if (RT_UNLIKELY(!pszTarFile || !pszOutputDir))
    {
        AssertMsgFailed(("Must supply pszTarFile, pszOutputDir!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return rc;

    /* Iterate through the tar file record by record. */
    RTTARRECORD record;
    size_t cbRead = 0;
    size_t cCount = 0;
    bool fFound = false;
    do
    {
        rc = RTFileRead(hFile, &record, sizeof(record), &cbRead);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* Check for EOF & data integrity */
        rc = rtTarCheckHeader(&record);
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (record.h.linkflag == LF_OLDNORMAL ||
            record.h.linkflag == LF_NORMAL)
        {
            if (iIndex == cCount)
            {
                fFound = true;
                char *pszTargetName;
                RTStrAPrintf(&pszTargetName, "%s/%s", pszOutputDir, record.h.name);
                rc = rtTarCopyFileFrom(hFile, pszTargetName, &record);
                /* On success copy the filename */
                if (RT_SUCCESS(rc) &&
                    ppszFileName)
                    *ppszFileName = RTStrDup(record.h.name);
                break;
            }
            ++cCount;
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }
    while(1);

    RTFileClose(hFile);

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* If we didn't found the index, indicate an error */
    if (!fFound)
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

RTR3DECL(int) RTTarCreate(const char *pszTarFile, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    if (RT_UNLIKELY(!pszTarFile || !papszFiles))
    {
        AssertMsgFailed(("Must supply pszTarFile, papszFiles!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_CREATE | RTFILE_O_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i=0; i < cFiles; ++i)
    {
        rc = rtTarCopyFileTo(hFile, papszFiles[i]);
        if (RT_FAILURE(rc))
            break;
    }

    /* gtar gives a warning, but the documentation says EOF is indicated by a
     * zero block. Disabled for now. */
//    if (RT_SUCCESS(rc))
//    {
//        /* Append the EOF record which is filled all by zeros */
//        RTTARRECORD record;
//        ASMMemFill32(&record, sizeof(record), 0);
//        rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
//    }

    /* Time to close the new tar archive */
    RTFileClose(hFile);

    /* Delete the freshly created tar archive on failure */
    if (RT_FAILURE(rc))
        RTFileDelete(pszTarFile);

    return rc;
}

