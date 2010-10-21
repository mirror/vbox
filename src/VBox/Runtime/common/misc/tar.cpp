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


/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/
#include "internal/iprt.h"
#include "internal/magics.h"
#include <iprt/tar.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>

/******************************************************************************
 *   Structures and Typedefs                                                  *
 ******************************************************************************/

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

typedef struct RTTARFILEINTERNAL* PRTTARFILEINTERNAL;
typedef struct RTTARINTERNAL
{
    uint32_t        u32Magic;
    RTFILE          hTarFile;
    bool            fFileOpenForWrite;
    uint32_t        fOpenMode;
    bool            fStreamMode;
    PRTTARFILEINTERNAL pFileCache;
} RTTARINTERNAL;
typedef RTTARINTERNAL* PRTTARINTERNAL;

typedef struct RTTARFILEINTERNAL
{
    uint32_t        u32Magic;
    PRTTARINTERNAL  pTar;
    char           *pszFilename;
    uint64_t        uStart;
    uint64_t        cbSize;
    uint64_t        cbSetSize;
    uint64_t        uCurrentPos;
    uint32_t        fOpenMode;
} RTTARFILEINTERNAL;
typedef RTTARFILEINTERNAL* PRTTARFILEINTERNAL;

/******************************************************************************
 *   Defined Constants And Macros                                             *
 ******************************************************************************/

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
/* RTTAR */
#define RTTAR_VALID_RETURN_RC(hHandle, rc) \
    do { \
        AssertPtrReturn((hHandle), (rc)); \
        AssertReturn((hHandle)->u32Magic == RTTAR_MAGIC, (rc)); \
    } while (0)
/* RTTARFILE */
#define RTTARFILE_VALID_RETURN_RC(hHandle, rc) \
    do { \
        AssertPtrReturn((hHandle), (rc)); \
        AssertReturn((hHandle)->u32Magic == RTTARFILE_MAGIC, (rc)); \
    } while (0)

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
/* RTTAR */
#define RTTAR_VALID_RETURN(hHandle) RTTAR_VALID_RETURN_RC((hHandle), VERR_INVALID_HANDLE)
/* RTTARFILE */
#define RTTARFILE_VALID_RETURN(hHandle) RTTARFILE_VALID_RETURN_RC((hHandle), VERR_INVALID_HANDLE)

/** Validates a handle and returns (void) if not valid. */
/* RTTAR */
#define RTTAR_VALID_RETURN_VOID(hHandle) \
    do { \
        AssertPtrReturnVoid(hHandle); \
        AssertReturnVoid((hHandle)->u32Magic == RTTAR_MAGIC); \
    } while (0)
/* RTTARFILE */
#define RTTARFILE_VALID_RETURN_VOID(hHandle) \
    do { \
        AssertPtrReturnVoid(hHandle); \
        AssertReturnVoid((hHandle)->u32Magic == RTTARFILE_MAGIC); \
    } while (0)

/******************************************************************************
 *   Internal Functions                                                       *
 ******************************************************************************/

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

DECLINLINE(int) rtTarCreateHeaderRecord(PRTTARRECORD pRecord, const char *pszSrcName, uint64_t cbSize, RTUID uid, RTGID gid, RTFMODE fmode, int64_t mtime)
{
    /* Fill the header record */
//    RT_ZERO(pRecord);
    RTStrPrintf(pRecord->h.name,  sizeof(pRecord->h.name),  "%s",     pszSrcName);
    RTStrPrintf(pRecord->h.mode,  sizeof(pRecord->h.mode),  "%0.7o",  fmode);
    RTStrPrintf(pRecord->h.uid,   sizeof(pRecord->h.uid),   "%0.7o",  uid);
    RTStrPrintf(pRecord->h.gid,   sizeof(pRecord->h.gid),   "%0.7o",  gid);
    RTStrPrintf(pRecord->h.size,  sizeof(pRecord->h.size),  "%0.11o", cbSize);
    RTStrPrintf(pRecord->h.mtime, sizeof(pRecord->h.mtime), "%0.11o", mtime);
    RTStrPrintf(pRecord->h.magic, sizeof(pRecord->h.magic), "ustar  ");
    RTStrPrintf(pRecord->h.uname, sizeof(pRecord->h.uname), "someone");
    RTStrPrintf(pRecord->h.gname, sizeof(pRecord->h.gname), "someone");
    pRecord->h.linkflag = LF_NORMAL;

    /* Create the checksum out of the new header */
    uint32_t chksum = 0;
    int rc = rtTarCalcChkSum(pRecord, &chksum);
    if (RT_FAILURE(rc))
        return rc;
    /* Format the checksum */
    RTStrPrintf(pRecord->h.chksum, sizeof(pRecord->h.chksum), "%0.7o", chksum);

    return VINF_SUCCESS;
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

DECLINLINE(int) rtTarAppendZeros(RTTARFILE hFile, uint64_t cbSize)
{
    /* Allocate a temporary buffer for copying the tar content in blocks. */
    size_t cbTmp = 0;
    void *pvTmp = rtTarMemTmpAlloc(&cbTmp);
    if (!pvTmp)
        return VERR_NO_MEMORY;
    RT_BZERO(pvTmp, cbTmp);

    int rc = VINF_SUCCESS;
    uint64_t cbAllWritten = 0;
    size_t cbWritten = 0;
    for (;;)
    {
        if (cbAllWritten >= cbSize)
            break;
        size_t cbToWrite = RT_MIN(cbSize - cbAllWritten, cbTmp);
        rc = RTTarFileWrite(hFile, pvTmp, cbToWrite, &cbWritten);
        if (RT_FAILURE(rc))
            break;
        cbAllWritten += cbWritten;
    }

    RTMemTmpFree(pvTmp);

    return rc;
}

DECLINLINE(PRTTARFILEINTERNAL) rtCreateTarFileInternal(PRTTARINTERNAL pInt, const char *pszFilename, uint32_t fOpen)
{
    PRTTARFILEINTERNAL pFileInt = (PRTTARFILEINTERNAL)RTMemAllocZ(sizeof(RTTARFILEINTERNAL));
    if (!pFileInt)
        return NULL;

    pFileInt->u32Magic = RTTARFILE_MAGIC;
    pFileInt->pTar = pInt;
    pFileInt->pszFilename = RTStrDup(pszFilename);
    pFileInt->fOpenMode = fOpen;

    return pFileInt;
}

DECLINLINE(PRTTARFILEINTERNAL) rtCopyTarFileInternal(PRTTARFILEINTERNAL pInt)
{
    PRTTARFILEINTERNAL pNewInt = (PRTTARFILEINTERNAL)RTMemAllocZ(sizeof(RTTARFILEINTERNAL));
    if (!pNewInt)
        return NULL;

    memcpy(pNewInt, pInt, sizeof(RTTARFILEINTERNAL));
    pNewInt->pszFilename = RTStrDup(pInt->pszFilename);

    return pNewInt;
}

DECLINLINE(void) rtDeleteTarFileInternal(PRTTARFILEINTERNAL pInt)
{
    if (pInt)
    {
        if (pInt->pszFilename)
            RTStrFree(pInt->pszFilename);
        pInt->u32Magic = RTTARFILE_MAGIC_DEAD;
        RTMemFree(pInt);
    }
}

static int rtTarExtractFileToFile(RTTARFILE hFile, const char *pszTargetName, const uint64_t cbOverallSize, uint64_t &cbOverallWritten, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
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
        /* Get the size of the source file */
        uint64_t cbToCopy = 0;
        rc = RTTarFileGetSize(hFile, &cbToCopy);
        if (RT_FAILURE(rc))
            break;
        /* Copy the content from hFile over to pszTargetName. */
        uint64_t cbAllWritten = 0; /* Already copied */
        uint64_t cbRead = 0; /* Actually read in the last step */
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbOverallSize * cbOverallWritten), pvUser);
            /* Finished already? */
            if (cbAllWritten == cbToCopy)
                break;
            /* Read one block. */
            cbRead = RT_MIN(cbToCopy - cbAllWritten, cbTmp);
            rc = RTTarFileRead(hFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
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
        uint32_t mode;
        rc = RTTarFileGetMode(hFile, &mode);
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

static int rtTarAppendFileFromFile(RTTAR hTar, const char *pszSrcName, const uint64_t cbOverallSize, uint64_t &cbOverallWritten, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    RTFILE hOldFile;
    /* Open the source file */
    int rc = RTFileOpen(&hOldFile, pszSrcName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    RTTARFILE hFile = NIL_RTTARFILE;
    void *pvTmp = 0;
    do
    {
        /* Get the size of the source file */
        uint64_t cbToCopy;
        rc = RTFileGetSize(hOldFile, &cbToCopy);
        if (RT_FAILURE(rc))
            break;

        rc = RTTarFileOpen(hTar, &hFile, RTPathFilename(pszSrcName), RTFILE_O_OPEN | RTFILE_O_WRITE);
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
        /* Set the mode from the other file */
        rc = RTTarFileSetMode(hFile, fmode);
        if (RT_FAILURE(rc))
            break;
        /* Set the modification time from the other file */
        RTTIMESPEC time;
        RTTimeSpecSetSeconds(&time, mtime);
        rc = RTTarFileSetTime(hFile, &time);
        if (RT_FAILURE(rc))
            break;
        /* Set the owner from the other file */
        rc = RTTarFileSetOwner(hFile, uid, gid);
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
            /* Read one block */
            rc = RTFileRead(hOldFile, pvTmp, cbRead, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Write one block. */
            rc = RTTarFileWriteAt(hFile, cbAllWritten, pvTmp, cbRead, NULL);
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

    if (hFile)
        RTTarFileClose(hFile);

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

static int rtTarFindFile(RTFILE hFile, const char *pszFile, uint64_t *puOffset, uint64_t *pcbSize)
{
    /* Assume we are on the file head. */
    int rc = VINF_SUCCESS;
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
                /* Get the file size */
                rc = RTStrToUInt64Full(record.h.size, 8, pcbSize);
                if (RT_FAILURE(rc))
                    break;
                /* Seek back, to positionate the file pointer at the start of the header. */
                rc = RTFileSeek(hFile, -(int64_t)sizeof(RTTARRECORD), RTFILE_SEEK_CURRENT, puOffset);
                fFound = true;
                break;
            }
        }
        rc = rtTarSkipData(hFile, &record);
        if (RT_FAILURE(rc))
            break;
    }

    if (rc == VERR_TAR_END_OF_FILE)
        rc = VINF_SUCCESS;

    /* Something found? */
    if (    RT_SUCCESS(rc)
        &&  !fFound)
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static int rtTarGetFilesOverallSize(RTFILE hFile, const char * const *papszFiles, size_t cFiles, uint64_t *pcbOverallSize)
{
    int rc = VINF_SUCCESS;
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

/******************************************************************************
 *   Public Functions                                                         *
 ******************************************************************************/

RTR3DECL(int) RTTarOpen(PRTTAR phTar, const char* pszTarname, uint32_t fMode, bool fStream)
{
    PRTTARINTERNAL pInt = (PRTTARINTERNAL)RTMemAllocZ(sizeof(RTTARINTERNAL));
    if (!pInt)
        return VERR_NO_MEMORY;

    pInt->u32Magic = RTTAR_MAGIC;
    pInt->fOpenMode = fMode;
    pInt->fStreamMode = fStream && (fMode & RTFILE_O_READ);

    int rc = VINF_SUCCESS;
    do
    {
        /* Open the tar file. */
        rc = RTFileOpen(&pInt->hTarFile, pszTarname, fMode);
        if (RT_FAILURE(rc))
            break;
    }
    while(0);

    if (RT_FAILURE(rc))
    {
        /* Todo: remove if created by us */
        if (pInt->hTarFile)
            RTFileClose(pInt->hTarFile);
        RTMemFree(pInt);
    }else
        *phTar = (RTTAR)pInt;

    return rc;
}

RTR3DECL(int) RTTarClose(RTTAR hTar)
{
    if (hTar == NIL_RTTAR)
        return VINF_SUCCESS;

    PRTTARINTERNAL pInt = hTar;
    RTTAR_VALID_RETURN(pInt);

    int rc = VINF_SUCCESS;

    /* gtar gives a warning, but the documentation says EOF is indicated by a
     * zero block. Disabled for now. */
#if 0
    {
        /* Append the EOF record which is filled all by zeros */
        RTTARRECORD record;
        RT_ZERO(record);
        rc = RTFileWrite(pInt->hTarFile, &record, sizeof(record), NULL);
    }
#endif

    if (pInt->hTarFile)
        rc = RTFileClose(pInt->hTarFile);

    /* Delete any remaining cached file headers. */
    if (pInt->pFileCache)
    {
        rtDeleteTarFileInternal(pInt->pFileCache);
        pInt->pFileCache = 0;
    }

    pInt->u32Magic = RTTAR_MAGIC_DEAD;

    RTMemFree(pInt);

    return rc;
}

RTR3DECL(int) RTTarFileOpen(RTTAR hTar, PRTTARFILE phFile, const char *pszFilename, uint32_t fOpen)
{
    AssertReturn((fOpen & RTFILE_O_READ) || (fOpen & RTFILE_O_WRITE), VERR_INVALID_PARAMETER);

    PRTTARINTERNAL pInt = hTar;
    RTTAR_VALID_RETURN(pInt);

    if (!pInt->hTarFile)
        return VERR_INVALID_HANDLE;

    if (pInt->fStreamMode)
        return VERR_INVALID_STATE;

    if (fOpen & RTFILE_O_WRITE)
    {
        if (!(pInt->fOpenMode & RTFILE_O_WRITE))
            return VERR_WRITE_PROTECT;
        if (pInt->fFileOpenForWrite)
            return VERR_TOO_MANY_OPEN_FILES;
    }

    PRTTARFILEINTERNAL pFileInt = rtCreateTarFileInternal(pInt, pszFilename, fOpen);
    if (!pFileInt)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    do
    {
        if (pFileInt->fOpenMode & RTFILE_O_WRITE)
        {
            pInt->fFileOpenForWrite = true;
            /* If we are in write mode, we also in append mode. Add an dummy
             * header at the end of the current file. It will be filled by the
             * close operation. */
            rc = RTFileSeek(pFileInt->pTar->hTarFile, 0, RTFILE_SEEK_END, &pFileInt->uStart);
            if (RT_FAILURE(rc))
                break;
            RTTARRECORD record;
            RT_ZERO(record);
            rc = RTFileWrite(pFileInt->pTar->hTarFile, &record, sizeof(RTTARRECORD), NULL);
            if (RT_FAILURE(rc))
                break;
        }
        else if (pFileInt->fOpenMode & RTFILE_O_READ)
        {
            /* We need to be on the start of the file */
            rc = RTFileSeek(pFileInt->pTar->hTarFile, 0, RTFILE_SEEK_BEGIN, NULL);
            if (RT_FAILURE(rc))
                break;
            /* Search for the file. */
            rc = rtTarFindFile(pFileInt->pTar->hTarFile, pszFilename, &pFileInt->uStart, &pFileInt->cbSize);
            if (RT_FAILURE(rc))
                break;
        }
        else
        {
        }

    }
    while(0);

    /* Cleanup on failure */
    if (RT_FAILURE(rc))
    {
        if (pFileInt->pszFilename)
            RTStrFree(pFileInt->pszFilename);
        RTMemFree(pFileInt);
    }
    else
        *phFile = (RTTARFILE)pFileInt;

    return rc;
}

RTR3DECL(int) RTTarFileClose(RTTARFILE hFile)
{
    /* Already closed? */
    if (hFile == NIL_RTTARFILE)
        return VINF_SUCCESS;

    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    int rc = VINF_SUCCESS;

    /* In write mode: */
    if (pFileInt->fOpenMode & RTFILE_O_READ)
    {
        /* In read mode, we want to make sure to stay at the aligned end of this
         * file, so the next file could be read immediately. */
        uint64_t uCurPos = RTFileTell(pFileInt->pTar->hTarFile);
        /* Check that the file pointer is somewhere within the last open file.
         * If we are at the beginning (nothing read yet) nothing will be done.
         * A user could open/close a file more than once, without reading
         * something. */
        if (pFileInt->uStart + sizeof(RTTARRECORD) < uCurPos && uCurPos < RT_ALIGN(pFileInt->uStart + sizeof(RTTARRECORD) + pFileInt->cbSize, sizeof(RTTARRECORD)))
        {
            /* Seek to the next file header. */
            uint64_t uNextPos = RT_ALIGN(pFileInt->uStart + sizeof(RTTARRECORD) + pFileInt->cbSize, sizeof(RTTARRECORD));
            rc = RTFileSeek(pFileInt->pTar->hTarFile, uNextPos - uCurPos, RTFILE_SEEK_CURRENT, NULL);
        }
    }
    else if (pFileInt->fOpenMode & RTFILE_O_WRITE)
    {
        pFileInt->pTar->fFileOpenForWrite = false;
        do
        {
            /* If the user has called RTTarFileSetSize in the meantime, we have
               to make sure the file has the right size. */
            if (pFileInt->cbSetSize > pFileInt->cbSize)
            {
                rc = rtTarAppendZeros(hFile, pFileInt->cbSetSize - pFileInt->cbSize);
                if (RT_FAILURE(rc))
                    break;
            }
            /* If the written size isn't 512 byte aligned, we need to fix this. */
            RTTARRECORD record;
            RT_ZERO(record);
            uint64_t cbSizeAligned = RT_ALIGN(pFileInt->cbSize, sizeof(RTTARRECORD));
            if (cbSizeAligned != pFileInt->cbSize)
            {
                /* Note the RTFile method. We didn't increase the cbSize or cbCurrentPos here. */
                rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + sizeof(RTTARRECORD) + pFileInt->cbSize, &record, cbSizeAligned - pFileInt->cbSize, NULL);
                if (RT_FAILURE(rc))
                    break;
            }
            /* Create a header record for the file */
            /* Todo: mode, gid, uid, mtime should be setable (or detected myself) */
            RTTIMESPEC time;
            RTTimeNow(&time);
            rc = rtTarCreateHeaderRecord(&record, pFileInt->pszFilename, pFileInt->cbSize, 0, 0, 0600, RTTimeSpecGetSeconds(&time));
            if (RT_FAILURE(rc))
                break;
            /* Write this at the start of the file data */
            rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart, &record, sizeof(RTTARRECORD), NULL);
            if (RT_FAILURE(rc))
                break;
        }
        while(0);
    }

    /* Now cleanup and delete the handle */
    rtDeleteTarFileInternal(pFileInt);

    return rc;
}

RTR3DECL(int) RTTarFileSeek(RTTARFILE hFile, uint64_t uOffset,  unsigned uMethod, uint64_t *poffActual)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if (pFileInt->pTar->fStreamMode)
        return VERR_INVALID_STATE;

    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
        {
            if (uOffset > pFileInt->cbSize)
                return VERR_SEEK_ON_DEVICE;
            pFileInt->uCurrentPos = uOffset;
            break;
        }
        case RTFILE_SEEK_CURRENT:
        {
            if (pFileInt->uCurrentPos + uOffset > pFileInt->cbSize)
                return VERR_SEEK_ON_DEVICE;
            pFileInt->uCurrentPos += uOffset;
            break;
        }
        case RTFILE_SEEK_END:
        {
            if ((int64_t)pFileInt->cbSize - (int64_t)uOffset < 0)
                return VERR_NEGATIVE_SEEK;
            pFileInt->uCurrentPos = pFileInt->cbSize - uOffset;
            break;
        }
        default: AssertFailedReturn(VERR_INVALID_PARAMETER); break;
    }

    return VINF_SUCCESS;
}

RTR3DECL(uint64_t) RTTarFileTell(RTTARFILE hFile)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN_RC(pFileInt, ~0ULL);

    return pFileInt->uCurrentPos;
}

RTR3DECL(int) RTTarFileRead(RTTARFILE hFile, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Todo: optimize this, by checking the current pos */
    return RTTarFileReadAt(hFile, pFileInt->uCurrentPos, pvBuf, cbToRead, pcbRead);
}

RTR3DECL(int) RTTarFileReadAt(RTTARFILE hFile, uint64_t uOffset, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Check that we not read behind the end of file. If so return immediately. */
    if (uOffset > pFileInt->cbSize)
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS; /* ??? VERR_EOF */
    }

    size_t cbToCopy = RT_MIN(pFileInt->cbSize - uOffset, cbToRead);
    size_t cbTmpRead = 0;
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->uStart + 512 + uOffset, pvBuf, cbToCopy, &cbTmpRead);
    pFileInt->uCurrentPos = uOffset + cbTmpRead;
    if (pcbRead)
        *pcbRead = cbTmpRead;

    return rc;
}

RTR3DECL(int) RTTarFileWrite(RTTARFILE hFile, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Todo: optimize this, by checking the current pos */
    return RTTarFileWriteAt(hFile, pFileInt->uCurrentPos, pvBuf, cbToWrite, pcbWritten);
}

RTR3DECL(int) RTTarFileWriteAt(RTTARFILE hFile, uint64_t uOffset, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    size_t cbTmpWritten = 0;
    int rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + 512 + uOffset, pvBuf, cbToWrite, &cbTmpWritten);
    pFileInt->cbSize += cbTmpWritten;
    pFileInt->uCurrentPos = uOffset + cbTmpWritten;
    if (pcbWritten)
        *pcbWritten = cbTmpWritten;

    return rc;
}

RTR3DECL(int) RTTarFileGetSize(RTTARFILE hFile, uint64_t *pcbSize)
{
    /* Validate input */
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);

    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    *pcbSize = RT_MAX(pFileInt->cbSetSize, pFileInt->cbSize);

    return VINF_SUCCESS;
}

RTR3DECL(int) RTTarFileSetSize(RTTARFILE hFile, uint64_t cbSize)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    /* Todo: if cbSize is smaller than pFileInt->cbSize we have to truncate the
       current file. */
    pFileInt->cbSetSize = cbSize;

    return VINF_SUCCESS;
}

RTR3DECL(int) RTTarFileGetMode(RTTARFILE hFile, uint32_t *pfMode)
{
    /* Validate input */
    AssertPtrReturn(pfMode, VERR_INVALID_POINTER);

    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Read the mode out of the header entry */
    char mode[8];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.mode), mode, 8, NULL);
    if (RT_FAILURE(rc))
        return rc;
    /* Convert it to an integer */
    return RTStrToUInt32Full(mode, 8, pfMode);
}

RTR3DECL(int) RTTarFileSetMode(RTTARFILE hFile, uint32_t fMode)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    /* Convert the mode to an string. */
    char mode[8];
    RTStrPrintf(mode, sizeof(mode), "%0.7o", fMode);
    /* Write it directly into the header */
    return RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.mode), mode, 8, NULL);
}

RTR3DECL(int) RTTarFileGetTime(RTTARFILE hFile, PRTTIMESPEC pTime)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Read the time out of the header entry */
    char mtime[12];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.mtime), mtime, 12, NULL);
    if (RT_FAILURE(rc))
        return rc;
    /* Convert it to an integer */
    int64_t tmpSeconds;
    rc = RTStrToInt64Full(mtime, 12, &tmpSeconds);
    /* And back to our time structure */
    if (RT_SUCCESS(rc))
        RTTimeSpecSetSeconds(pTime, tmpSeconds);

    return rc;
}

RTR3DECL(int) RTTarFileSetTime(RTTARFILE hFile, PRTTIMESPEC pTime)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    /* Convert the time to an string. */
    char mtime[12];
    RTStrPrintf(mtime, sizeof(mtime), "%0.11o", RTTimeSpecGetSeconds(pTime));
    /* Write it directly into the header */
    return RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.mtime), mtime, 12, NULL);
}

RTR3DECL(int) RTTarFileGetOwner(RTTARFILE hFile, uint32_t *pUid, uint32_t *pGid)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Read the uid and gid out of the header entry */
    char uid[8];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.uid), uid, 8, NULL);
    if (RT_FAILURE(rc))
        return rc;
    char gid[8];
    rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.gid), gid, 8, NULL);
    if (RT_FAILURE(rc))
        return rc;
    /* Convert it to integer */
    rc = RTStrToUInt32Full(uid, 8, pUid);
    if (RT_FAILURE(rc))
        return rc;

    return RTStrToUInt32Full(gid, 8, pGid);
}

RTR3DECL(int) RTTarFileSetOwner(RTTARFILE hFile, uint32_t uid, uint32_t gid)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    int rc = VINF_SUCCESS;

    if (uid != (uint32_t)-1)
    {
        /* Convert the uid to an string. */
        char tmpUid[8];
        RTStrPrintf(tmpUid, sizeof(tmpUid), "%0.7o", uid);
        /* Write it directly into the header */
        rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.uid), tmpUid, 8, NULL);
        if (RT_FAILURE(rc))
            return rc;
    }
    if (gid != (uint32_t)-1)
    {
        /* Convert the gid to an string. */
        char tmpGid[8];
        RTStrPrintf(tmpGid, sizeof(tmpGid), "%0.7o", gid);
        /* Write it directly into the header */
        rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->uStart + RT_OFFSETOF(RTTARRECORD, h.gid), tmpGid, 8, NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    return rc;
}

/******************************************************************************
 *   Convenience Functions                                                    *
 ******************************************************************************/

RTR3DECL(int) RTTarFileExists(const char *pszTarFile, const char *pszFile)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTTAR hTar;
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false);
    if (RT_FAILURE(rc))
        return rc;

    /* Just try to open that file readonly. If this succeed the file exists. */
    RTTARFILE hFile;
    rc = RTTarFileOpen(hTar, &hFile, pszFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
        RTTarFileClose(hFile);

    RTTarClose(hTar);

    return rc;
}

RTR3DECL(int) RTTarList(const char *pszTarFile, char ***ppapszFiles, size_t *pcFiles)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppapszFiles, VERR_INVALID_POINTER);
    AssertPtrReturn(pcFiles, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTTAR hTar;
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false);
    if (RT_FAILURE(rc))
        return rc;

    /* This is done by internal methods, cause we didn't have a RTTARDIR
     * interface, yet. This should be fixed somedays. */

    PRTTARINTERNAL pInt = hTar;
    char **papszFiles = 0;
    size_t cFiles = 0;
    do
    {
        /* Initialize the file name array with one slot */
        size_t cFilesAlloc = 1;
        papszFiles = (char**)RTMemAlloc(sizeof(char *));
        if (!papszFiles)
        {
            return VERR_NO_MEMORY;
            break;
        }

        /* Iterate through the tar file record by record. Skip data records as we
         * didn't need them. */
        RTTARRECORD record;
        for (;;)
        {
            /* Read & verify a header record */
            rc = rtTarReadHeaderRecord(pInt->hTarFile, &record);
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
            rc = rtTarSkipData(pInt->hTarFile, &record);
            if (RT_FAILURE(rc))
                break;
        }
    }
    while(0);

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

    RTTarClose(hTar);

    return rc;
}

RTR3DECL(int) RTTarExtractFileToBuf(const char *pszTarFile, void **ppvBuf, size_t *pcbSize, const char *pszFile, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pvUser, VERR_INVALID_POINTER);

    /* Todo: progress bar */

    int rc = VINF_SUCCESS;
    RTTAR hTar = NIL_RTTAR;
    RTTARFILE hFile = NIL_RTTARFILE;
    char *pvTmp = 0;
    uint64_t cbToCopy = 0;
    do
    {
        rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false);
        if (RT_FAILURE(rc))
            break;
        rc = RTTarFileOpen(hTar, &hFile, pszFile, RTFILE_O_OPEN | RTFILE_O_READ);
        if (RT_FAILURE(rc))
            break;
        rc = RTTarFileGetSize(hFile, &cbToCopy);
        if (RT_FAILURE(rc))
            break;
        /* Allocate the memory for the file content. */
        pvTmp = (char*)RTMemAlloc(cbToCopy);
        if (!pvTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        size_t cbRead = 0;
        size_t cbAllRead = 0;
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbToCopy * cbAllRead), pvUser);
            if (cbAllRead == cbToCopy)
                break;
            rc = RTTarFileReadAt(hFile, 0, &pvTmp[cbAllRead], cbToCopy - cbAllRead, &cbRead);
            if (RT_FAILURE(rc))
                break;
            cbAllRead += cbRead;
        }
    }
    while(0);

    /* Set output values on success */
    if (RT_SUCCESS(rc))
    {
        *pcbSize = cbToCopy;
        *ppvBuf = pvTmp;
    }

    /* Cleanup */
    if (   RT_FAILURE(rc)
        && pvTmp)
        RTMemFree(pvTmp);
    if (hFile)
        RTTarFileClose(hFile);
    if (hTar)
        RTTarClose(hTar);

    return rc;
}

RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszOutputDir, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);
    AssertReturn(cFiles, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pvUser, VERR_INVALID_POINTER);

    /* Open the tar file */
    RTTAR hTar;
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false);
    if (RT_FAILURE(rc))
        return rc;

    do
    {
        /* Get the overall size of all files to extract out of the tar archive
           headers. Only necessary if there is a progress callback. */
        uint64_t cbOverallSize = 0;
        if (pfnProgressCallback)
        {
//            rc = rtTarGetFilesOverallSize(hFile, papszFiles, cFiles, &cbOverallSize);
//            if (RT_FAILURE(rc))
//                break;
        }

        uint64_t cbOverallWritten = 0;
        for(size_t i=0; i < cFiles; ++i)
        {
            RTTARFILE hFile;
            rc = RTTarFileOpen(hTar, &hFile, papszFiles[i], RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
            if (RT_FAILURE(rc))
                break;
            char *pszTargetFile;
            rc = RTStrAPrintf(&pszTargetFile, "%s/%s", pszOutputDir, papszFiles[i]);
            if (RT_FAILURE(rc))
                break;
            rc = rtTarExtractFileToFile(hFile, pszTargetFile, cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
            RTStrFree(pszTargetFile);
            RTTarFileClose(hFile);
            if (RT_FAILURE(rc))
                break;
        }
    }
    while(0);

    RTTarClose(hTar);

    return rc;
}

RTR3DECL(int) RTTarExtractAll(const char *pszTarFile, const char *pszOutputDir, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pszOutputDir, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pvUser, VERR_INVALID_POINTER);

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
    AssertReturn(cFiles, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pvUser, VERR_INVALID_POINTER);

    RTTAR hTar;
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_CREATE | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE, false);
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
    uint64_t cbOverallWritten = 0;
    for (size_t i = 0; i < cFiles; ++i)
    {
        rc = rtTarAppendFileFromFile(hTar, papszFiles[i], cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
        if (RT_FAILURE(rc))
            break;
    }

    /* Cleanup */
    RTTarClose(hTar);

    return rc;
}

/******************************************************************************
 *   Streaming Functions                                                      *
 ******************************************************************************/

RTR3DECL(int) RTTarCurrentFile(RTTAR hTar, char **ppszFilename)
{
    /* Validate input. */
    AssertPtrNullReturn(ppszFilename, VERR_INVALID_POINTER);

    PRTTARINTERNAL pInt = hTar;
    RTTAR_VALID_RETURN(pInt);

    /* Open and close the file on the current position. This makes sure the
     * cache is filled in case we never read something before. On success it
     * will return the current filename. */
    RTTARFILE hFile;
    int rc = RTTarFileOpenCurrentFile(hTar, &hFile, ppszFilename, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
        RTTarFileClose(hFile);

    return rc;
}

RTR3DECL(int) RTTarSeekNextFile(RTTAR hTar)
{
    PRTTARINTERNAL pInt = hTar;
    RTTAR_VALID_RETURN(pInt);

    int rc = VINF_SUCCESS;

    if (!pInt->fStreamMode)
        return VERR_INVALID_STATE;

    /* If there is nothing in the cache, it means we never read something. Just
     * ask for the current filename to fill the cache. */
    if (!pInt->pFileCache)
    {
        rc = RTTarCurrentFile(hTar, 0);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Check that the file pointer is somewhere within the last open file.
     * If not we are somehow busted. */
    uint64_t uCurPos = RTFileTell(pInt->hTarFile);
    if (!(pInt->pFileCache->uStart <= uCurPos && uCurPos < pInt->pFileCache->uStart + sizeof(RTTARRECORD) + pInt->pFileCache->cbSize))
        return VERR_INVALID_STATE;

    /* Seek to the next file header. */
    uint64_t uNextPos = RT_ALIGN(pInt->pFileCache->uStart + sizeof(RTTARRECORD) + pInt->pFileCache->cbSize, sizeof(RTTARRECORD));
    rc = RTFileSeek(pInt->hTarFile, uNextPos - uCurPos, RTFILE_SEEK_CURRENT, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /* Again check the current filename to fill the cache with the new value. */
    return RTTarCurrentFile(hTar, 0);
}

RTR3DECL(int) RTTarFileOpenCurrentFile(RTTAR hTar, PRTTARFILE phFile, char **ppszFilename, uint32_t fOpen)
{
    /* Validate input. */
    AssertPtrReturn(phFile, VERR_INVALID_POINTER);
    AssertPtrNullReturn(ppszFilename, VERR_INVALID_POINTER);
    AssertReturn((fOpen & RTFILE_O_READ), VERR_INVALID_PARAMETER); /* Only valid in read mode. */

    PRTTARINTERNAL pInt = hTar;
    RTTAR_VALID_RETURN(pInt);

    if (!pInt->fStreamMode)
        return VERR_INVALID_STATE;

    int rc = VINF_SUCCESS;

    /* Is there some cached entry? */
    if (pInt->pFileCache)
    {
        /* Are we still direct behind that header? */
        if (pInt->pFileCache->uStart + sizeof(RTTARRECORD) == RTFileTell(pInt->hTarFile))
        {
            /* Yes, so the streaming can start. Just return the cached file
             * structure to the caller. */
            *phFile = rtCopyTarFileInternal(pInt->pFileCache);
            if (ppszFilename)
                *ppszFilename = RTStrDup(pInt->pFileCache->pszFilename);
            return VINF_SUCCESS;
        }else
        {
            /* Else delete the last open file cache. Might be recreated below. */
            rtDeleteTarFileInternal(pInt->pFileCache);
            pInt->pFileCache = 0;
        }
    }

    PRTTARFILEINTERNAL pFileInt = 0;
    do
    {
        /* Try to read a header entry from the current position. If we aren't
         * on a header record, the header checksum will show and an error will
         * be returned. */
        RTTARRECORD record;
        /* Read & verify a header record */
        rc = rtTarReadHeaderRecord(pInt->hTarFile, &record);
        /* Check for error or EOF. */
        if (RT_FAILURE(rc))
            break;
        /* We support normal files only */
        if (   record.h.linkflag == LF_OLDNORMAL
            || record.h.linkflag == LF_NORMAL)
        {
            pFileInt = rtCreateTarFileInternal(pInt, record.h.name, fOpen);
            if (!pFileInt)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            /* Get the file size */
            rc = RTStrToUInt64Full(record.h.size, 8, &pFileInt->cbSize);
            if (RT_FAILURE(rc))
                break;
            /* The start is -512 from here. */
            pFileInt->uStart = RTFileTell(pInt->hTarFile) - sizeof(RTTARRECORD);
            /* Copy the new file structure to our cache. */
            pInt->pFileCache = rtCopyTarFileInternal(pFileInt);
            if (ppszFilename)
                *ppszFilename = RTStrDup(pFileInt->pszFilename);
        }
    }while (0);

    if (RT_FAILURE(rc))
    {
        if (pFileInt)
            rtDeleteTarFileInternal(pFileInt);
    }
    else
        *phFile = pFileInt;

    return rc;
}

