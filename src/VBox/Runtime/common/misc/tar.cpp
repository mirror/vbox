/* $Id$ */
/** @file
 * IPRT - Tar archive I/O.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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

DECLINLINE(int) rtTarCalcChkSum(PRTTARRECORD pRecord, uint32_t *pChkSum)
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
        return VERR_TAR_END_OF_FILE;

    *pChkSum = check;
    return VINF_SUCCESS;
}

DECLINLINE(int) rtTarReadHeaderRecord(RTFILE hFile, PRTTARRECORD pRecord)
{
    int rc = RTFileRead(hFile, pRecord, sizeof(RTTARRECORD), NULL);
    /* Check for EOF. EOF is valid in this case, cause it indicates no more
     * data in the tar archive. */
    if (rc == VERR_EOF)
        return VERR_TAR_END_OF_FILE;
    /* Report any other errors */
    else if (RT_FAILURE(rc))
        return rc;

    /* Check for data integrity & an EOF record */
    uint32_t check = 0;
    rc = rtTarCalcChkSum(pRecord, &check);
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

DECLINLINE(void*) rtTarMemTmpAlloc(size_t *pcbSize)
{
    *pcbSize = 0;
    /* Allocate a reasonably large buffer, fall back on a tiny one. Note:
     * has to be 512 byte aligned and >= 512 byte. */
    size_t cbTmp = _1M;
    void *pvTmp = RTMemTmpAlloc(cbTmp);
    if (!pvTmp)
    {
        cbTmp = sizeof(RTTARRECORD);
        pvTmp = RTMemTmpAlloc(cbTmp);
    }
    *pcbSize = cbTmp;
    return pvTmp;
}

static int rtTarExtractFileToBuf(RTFILE hFile, void **ppvBuf, uint64_t *pcbSize, PRTTARRECORD pRecord, const uint64_t cbOverallSize, uint64_t &cbOverallWritten, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    int rc = VINF_SUCCESS;

    uint64_t cbToCopy = RTStrToUInt64(pRecord->h.size);

    *ppvBuf = 0;
    void *pvTmp = 0;
    do
    {
        /* Allocate the memory for the file content. */
        *ppvBuf = RTMemAlloc(cbToCopy);
        char* pcsTmp = (char*)*ppvBuf;
        if (!pcsTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        /* Allocate a temporary buffer for reading the tar content in blocks. */
        size_t cbTmp = 0;
        pvTmp = rtTarMemTmpAlloc(&cbTmp);
        if (!pvTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        uint64_t cbAllWritten = 0; /* Already copied */
        uint64_t cbRead = 0; /* Actually read in the last step */
        /* Copy the content from hFile over to the memory. This is done block
         * wise in 512 byte aligned steps. After this copying is finished hFile
         * will be on a 512 byte boundary, regardless if the file copied is 512
         * byte size aligned. */
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbOverallSize * cbOverallWritten), pvUser);
            /* Finished already? */
            if (cbAllWritten == cbToCopy)
                break;
            /* Read one block. This will be 512 aligned in any case. */
            cbRead = RT_ALIGN(RT_MIN(cbToCopy - cbAllWritten, cbTmp), sizeof(RTTARRECORD));
            rc = RTFileRead(hFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Check for the last block which has not to be 512 bytes in size. */
            if (cbAllWritten + cbRead > cbToCopy)
                cbRead = cbToCopy - cbAllWritten;
            /* Write the block */
            memcpy(pcsTmp, pvTmp, cbRead);
            /* Count how many bytes are written already */
            cbAllWritten += cbRead;
            cbOverallWritten += cbRead;
            pcsTmp += cbRead;
        }
    }
    while(0);

    if (pvTmp)
        RTMemTmpFree(pvTmp);

    if (   RT_FAILURE(rc)
        && *ppvBuf)
        RTMemFree(*ppvBuf);
    else
        *pcbSize = cbToCopy;

    return rc;
}

static int rtTarExtractFileToFile(RTFILE hFile, const char *pszTargetName, PRTTARRECORD pRecord, const uint64_t cbOverallSize, uint64_t &cbOverallWritten, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    RTFILE hNewFile;
    /* Open the target file */
    int rc = RTFileOpen(&hNewFile, pszTargetName, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    void *pvTmp = 0;
    do
    {
        /* Allocate a temporary buffer for reading the tar content in blocks. */
        size_t cbTmp = 0;
        pvTmp = rtTarMemTmpAlloc(&cbTmp);
        if (!pvTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        uint64_t cbToCopy = RTStrToUInt64(pRecord->h.size);
        uint64_t cbAllWritten = 0; /* Already copied */
        uint64_t cbRead = 0; /* Actually read in the last step */
        /* Copy the content from hFile over to pszTargetName. This is done block
         * wise in 512 byte aligned steps. After this copying is finished hFile
         * will be on a 512 byte boundary, regardless if the file copied is 512
         * byte size aligned. */
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbOverallSize * cbOverallWritten), pvUser);
            /* Finished already? */
            if (cbAllWritten == cbToCopy)
                break;
            /* Read one block. This will be 512 aligned in any case. */
            cbRead = RT_ALIGN(RT_MIN(cbToCopy - cbAllWritten, cbTmp), sizeof(RTTARRECORD));
            rc = RTFileRead(hFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Check for the last block which has not to be 512 bytes in size. */
            if (cbAllWritten + cbRead > cbToCopy)
                cbRead = cbToCopy - cbAllWritten;
            /* Write the block */
            rc = RTFileWrite(hNewFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Count how many bytes are written already */
            cbAllWritten += cbRead;
            cbOverallWritten += cbRead;
        }

    }
    while(0);

    /* Cleanup */
    if (pvTmp)
        RTMemTmpFree(pvTmp);

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

static int rtTarAppendFileFromFile(RTFILE hFile, const char *pszSrcName, const uint64_t cbOverallSize, uint64_t &cbOverallWritten, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    RTFILE hOldFile;
    /* Open the source file */
    int rc = RTFileOpen(&hOldFile, pszSrcName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    void *pvTmp = 0;
    do
    {
        /* Get the size of the source file */
        uint64_t cbToCopy;
        rc = RTFileGetSize(hOldFile, &cbToCopy);
        if (RT_FAILURE(rc))
            break;
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
        RTStrPrintf(record.h.size,  sizeof(record.h.size),  "%0.11o", cbToCopy);
        RTStrPrintf(record.h.mtime, sizeof(record.h.mtime), "%0.11o", mtime);
        RTStrPrintf(record.h.magic, sizeof(record.h.magic), "ustar  ");
        RTStrPrintf(record.h.uname, sizeof(record.h.uname), "someone");
        RTStrPrintf(record.h.gname, sizeof(record.h.gname), "someone");
        record.h.linkflag = LF_NORMAL;

        /* Create the checksum out of the new header */
        uint32_t chksum = 0;
        rc = rtTarCalcChkSum(&record, &chksum);
        if (RT_FAILURE(rc))
            break;
        /* Format the checksum */
        RTStrPrintf(record.h.chksum, sizeof(record.h.chksum), "%0.7o", chksum);
        /* Write the header first */
        rc = RTFileWrite(hFile, &record, sizeof(record), NULL);
        if (RT_FAILURE(rc))
            break;

        /* Allocate a temporary buffer for copying the tar content in blocks. */
        size_t cbTmp = 0;
        pvTmp = rtTarMemTmpAlloc(&cbTmp);
        if (!pvTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        uint64_t cbAllWritten = 0; /* Already copied */
        uint64_t cbRead = 0; /* Actually read in the last step */
        uint64_t cbWrite = 0; /* Actually write in the last step */
        /* Copy the content from pszSrcName over to hFile. This is done block
         * wise in 512 byte steps. After this copying is finished hFile will be
         * on a 512 byte boundary, regardless if the file copied is 512 byte
         * size aligned. */
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbOverallSize * cbOverallWritten), pvUser);
            if (cbAllWritten >= cbToCopy)
                break;
            /* Read one block. Either its the buffer size or the rest of the
             * file. */
            cbRead = RT_MIN(cbToCopy - cbAllWritten, cbTmp);
            /* Write one block. This will be 512 byte aligned in any case. */
            cbWrite = RT_ALIGN(cbRead, sizeof(RTTARRECORD));
            /* Last record? */
            if (cbRead != cbWrite)
                /* Initialize the rest with zeros */
                RT_BZERO(&((char*)pvTmp)[cbRead], cbWrite-cbRead);
            /* Read one block */
            rc = RTFileRead(hOldFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Write one block. Tar needs to be 512 byte aligned. */
            rc = RTFileWrite(hFile, pvTmp, cbWrite, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Count how many bytes (of the original file) are written already */
            cbAllWritten += cbRead;
            cbOverallWritten += cbRead;
        }
    }
    while(0);

    /* Cleanup */
    if (pvTmp)
        RTMemTmpFree(pvTmp);

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

static int rtTarGetFilesOverallSize(RTFILE hFile, const char * const *papszFiles, size_t cFiles, uint64_t *pcbOverallSize)
{
    int rc;
    size_t cFound = 0;
    RTTARRECORD record;
    for (;;)
    {
        /* Read & verify a header record */
        rc = rtTarReadHeaderRecord(hFile, &record);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (   record.h.linkflag == LF_OLDNORMAL
            || record.h.linkflag == LF_NORMAL)
        {
            for (size_t i = 0; i < cFiles; ++i)
            {
                if (!RTStrCmp(record.h.name, papszFiles[i]))
                {
                    uint64_t cbSize;
                    /* Get the file size */
                    rc = RTStrToUInt64Full(record.h.size, 8, &cbSize);
                    /* Sum up the overall size */
                    *pcbOverallSize += cbSize;
                    ++cFound;
                    break;
                }
            }
            if (   cFound == cFiles
                || RT_FAILURE(rc))
                break;
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }
    if (rc == VERR_TAR_END_OF_FILE)
        rc = VINF_SUCCESS;

    /* Make sure the file pointer is at the begin of the file again. */
    if (RT_SUCCESS(rc))
        rc = RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, 0);
    return rc;
}

/*******************************************************************************
*   Public Functions                                                         *
*******************************************************************************/

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
        /* Read & verify a header record */
        rc = rtTarReadHeaderRecord(hFile, &record);
        /* Check for error or EOF. */
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

    if (rc == VERR_TAR_END_OF_FILE)
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
        /* Read & verify a header record */
        rc = rtTarReadHeaderRecord(hFile, &record);
        /* Check for error or EOF. */
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

    if (rc == VERR_TAR_END_OF_FILE)
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

RTR3DECL(int) RTTarExtractFileToBuf(const char *pszTarFile, void **ppvBuf, uint64_t *pcbSize, const char *pszFile, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;

    /* Get the overall size of all files to extract out of the tar archive
       headers. Only necessary if there is a progress callback. */
    uint64_t cbOverallSize = 0;
    if (pfnProgressCallback)
        rc = rtTarGetFilesOverallSize(hFile, &pszFile, 1, &cbOverallSize);
    if (RT_SUCCESS(rc))
    {
        /* Iterate through the tar file record by record. */
        RTTARRECORD record;
        bool fFound = false;
        uint64_t cbOverallWritten = 0;
        for (;;)
        {
            /* Read & verify a header record */
            rc = rtTarReadHeaderRecord(hFile, &record);
            /* Check for error or EOF. */
            if (RT_FAILURE(rc))
                break;
            /* We support normal files only */
            if (   record.h.linkflag == LF_OLDNORMAL
                || record.h.linkflag == LF_NORMAL)
            {
                if (!RTStrCmp(record.h.name, pszFile))
                {
                    fFound = true;
                    rc = rtTarExtractFileToBuf(hFile, ppvBuf, pcbSize, &record, cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
                    /* We are finished */
                    break;
                }
                else
                {
                    rc = rtTarSkipData(hFile, &record);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
        }

        if (rc == VERR_TAR_END_OF_FILE)
            rc = VINF_SUCCESS;

        /* If we didn't found the file, indicate an error */
        if (!fFound && RT_SUCCESS(rc))
            rc = VERR_FILE_NOT_FOUND;
    }

    RTFileClose(hFile);
    return rc;
}

RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
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

    /* Get the overall size of all files to extract out of the tar archive
       headers. Only necessary if there is a progress callback. */
    uint64_t cbOverallSize = 0;
    if (pfnProgressCallback)
        rc = rtTarGetFilesOverallSize(hFile, papszFiles, cFiles, &cbOverallSize);
    if (RT_SUCCESS(rc))
    {
        /* Iterate through the tar file record by record. */
        RTTARRECORD record;
        char **paExtracted = (char **)RTMemTmpAllocZ(sizeof(char *) * cFiles);
        if (paExtracted)
        {
            size_t cExtracted = 0;
            uint64_t cbOverallWritten = 0;
            for (;;)
            {
                /* Read & verify a header record */
                rc = rtTarReadHeaderRecord(hFile, &record);
                /* Check for error or EOF. */
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
                                    rc = rtTarExtractFileToFile(hFile, pszTargetFile, &record, cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
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

            if (rc == VERR_TAR_END_OF_FILE)
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
    }

    RTFileClose(hFile);
    return rc;
}

RTR3DECL(int) RTTarExtractByIndex(const char *pszTarFile, const char *pszOutputDir, size_t iIndex, char **ppszFileName, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
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
        /* Read & verify a header record */
        rc = rtTarReadHeaderRecord(hFile, &record);
        /* Check for error or EOF. */
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
                    uint64_t cbOverallSize;
                    uint64_t cbOverallWritten = 0;
                    /* Get the file size */
                    rc = RTStrToUInt64Full(record.h.size, 8, &cbOverallSize);
                    if (RT_FAILURE(rc))
                        break;
                    rc = rtTarExtractFileToFile(hFile, pszTargetName, &record, cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
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

    if (rc == VERR_TAR_END_OF_FILE)
        rc = VINF_SUCCESS;

    /* If we didn't found the index, indicate an error */
    if (!fFound && RT_SUCCESS(rc))
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

RTR3DECL(int) RTTarExtractAll(const char *pszTarFile, const char *pszOutputDir, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszOutputDir, VERR_INVALID_POINTER);

    char **papszFiles;
    size_t cFiles;

    /* First fetch the files names contained in the tar file */
    int rc = RTTarList(pszTarFile, &papszFiles, &cFiles);
    if (RT_FAILURE(rc))
        return rc;

    /* Extract all files */
    return RTTarExtractFiles(pszTarFile, pszOutputDir, papszFiles, cFiles, pfnProgressCallback, pvUser);
}

RTR3DECL(int) RTTarCreate(const char *pszTarFile, const char * const *papszFiles, size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszTarFile, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    /* Get the overall size of all files to pack into the tar archive. Only
       necessary if there is a progress callback. */
    uint64_t cbOverallSize = 0;
    if (pfnProgressCallback)
        for (size_t i = 0; i < cFiles; ++i)
        {
            uint64_t cbSize;
            rc = RTFileQuerySize(papszFiles[i], &cbSize);
            if (RT_FAILURE(rc))
                break;
            cbOverallSize += cbSize;
        }

    if (RT_SUCCESS(rc))
    {
        uint64_t cbOverallWritten = 0;

        for (size_t i = 0; i < cFiles; ++i)
        {
            rc = rtTarAppendFileFromFile(hFile, papszFiles[i], cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
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
    }

    /* Time to close the new tar archive */
    RTFileClose(hFile);

    /* Delete the freshly created tar archive on failure */
    if (RT_FAILURE(rc))
        RTFileDelete(pszTarFile);

    return rc;
}

