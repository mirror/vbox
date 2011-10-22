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
#include <iprt/tar.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include "internal/magics.h"


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

/**
 * A tar file header.
 */
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
AssertCompileSize(RTTARRECORD, 512);
AssertCompileMemberOffset(RTTARRECORD, h.size, 100+8*3);
/** Pointer to a tar file header. */
typedef RTTARRECORD *PRTTARRECORD;


#if 0 /* not currently used */
typedef struct RTTARFILELIST
{
    char *pszFilename;
    RTTARFILELIST *pNext;
} RTTARFILELIST;
typedef RTTARFILELIST *PRTTARFILELIST;
#endif

/** Pointer to a tar file handle. */
typedef struct RTTARFILEINTERNAL *PRTTARFILEINTERNAL;

/**
 * The internal data of a tar handle.
 */
typedef struct RTTARINTERNAL
{
    /** The magic (RTTAR_MAGIC). */
    uint32_t            u32Magic;
    /** The handle to the tar file. */
    RTFILE              hTarFile;
    /** The open mode for hTarFile. */
    uint32_t            fOpenMode;
    /** Whether a file within the archive is currently open for writing.
     * Only one can be open.  */
    bool                fFileOpenForWrite;
    /** Whether operating in stream mode. */
    bool                fStreamMode;
    /** The file cache of one file.  */
    PRTTARFILEINTERNAL  pFileCache;
} RTTARINTERNAL;
/** Pointer to a the internal data of a tar handle.  */
typedef RTTARINTERNAL* PRTTARINTERNAL;

/**
 * The internal data of a file within a tar file.
 */
typedef struct RTTARFILEINTERNAL
{
    /** The magic (RTTARFILE_MAGIC). */
    uint32_t        u32Magic;
    /** Pointer to back to the tar file. */
    PRTTARINTERNAL  pTar;
    /** The name of the file. */
    char           *pszFilename;
    /** The offset into the archive where the file header starts. */
    uint64_t        offStart;
    /** The size of the file. */
    uint64_t        cbSize;
    /** The size set by RTTarFileSetSize(). */
    uint64_t        cbSetSize;
    /** The current offset within this file. */
    uint64_t        offCurrent;
    /** The open mode. */
    uint32_t        fOpenMode;
} RTTARFILEINTERNAL;
/** Pointer to the internal data of a tar file.  */
typedef RTTARFILEINTERNAL *PRTTARFILEINTERNAL;


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

DECLINLINE(void) rtTarSizeToRec(PRTTARRECORD pRecord, uint64_t cbSize)
{
    /*
     * Small enough for the standard octal string encoding?
     *
     * Note! We could actually use the terminator character as well if we liked,
     *       but let not do that as it's easier to test this way.
     */
    if (cbSize < _4G * 2U)
        RTStrPrintf(pRecord->h.size, sizeof(pRecord->h.size), "%0.11llo", cbSize);
    else
    {
        /*
         * Base 256 extension. Set the highest bit of the left most character.
         * We don't deal with negatives here, cause the size have to be greater
         * than zero.
         *
         * Note! The base-256 extension are never used by gtar or libarchive
         *       with the "ustar  \0" format version, only the later
         *       "ustar\000" version.  However, this shouldn't cause much
         *       trouble as they are not picky about what they read.
         */
        size_t cchField = sizeof(pRecord->h.size) - 1;
        unsigned char *puchField = (unsigned char*)pRecord->h.size;
        puchField[0] = 0x80;
        do
        {
            puchField[cchField--] = cbSize & 0xff;
            cbSize >>= 8;
        } while (cchField);
    }
}

DECLINLINE(uint64_t) rtTarRecToSize(PRTTARRECORD pRecord)
{
    int64_t cbSize = 0;
    if (pRecord->h.size[0] & 0x80)
    {
        size_t cchField = sizeof(pRecord->h.size);
        unsigned char const *puchField = (unsigned char const *)pRecord->h.size;

        /*
         * The first byte has the bit 7 set to indicate base-256, while bit 6
         * is the signed bit. Bits 5:0 are the most significant value bits.
         */
        cbSize = !(0x40 & *puchField) ? 0 : -1;
        cbSize = (cbSize << 6) | (*puchField & 0x3f);
        cchField--;
        puchField++;

        /*
         * The remaining bytes are used in full.
         */
        while (cchField-- > 0)
        {
            if (RT_UNLIKELY(   cbSize > INT64_MAX / 256
                            || cbSize < INT64_MIN / 256))
            {
                cbSize = cbSize < 0 ? INT64_MIN : INT64_MAX;
                break;
            }
            cbSize = (cbSize << 8) | *puchField++;
        }
    }
    else
        RTStrToInt64Full(pRecord->h.size, 8, &cbSize);

    if (cbSize < 0)
        cbSize = 0;

    return (uint64_t)cbSize;
}

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
    {
        /* Make sure the strings are zero terminated. */
        pRecord->h.name[sizeof(pRecord->h.name) - 1]         = 0;
        pRecord->h.linkname[sizeof(pRecord->h.linkname) - 1] = 0;
        pRecord->h.magic[sizeof(pRecord->h.magic) - 1]       = 0;
        pRecord->h.uname[sizeof(pRecord->h.uname) - 1]       = 0;
        pRecord->h.gname[sizeof(pRecord->h.gname) - 1]       = 0;
    }
    else
        rc = VERR_TAR_CHKSUM_MISMATCH;

    return rc;
}

DECLINLINE(int) rtTarCreateHeaderRecord(PRTTARRECORD pRecord, const char *pszSrcName, uint64_t cbSize,
                                        RTUID uid, RTGID gid, RTFMODE fmode, int64_t mtime)
{
    /** @todo check for field overflows. */
    /* Fill the header record */
//    RT_ZERO(pRecord); - done by the caller.
    RTStrPrintf(pRecord->h.name,  sizeof(pRecord->h.name),  "%s",       pszSrcName);
    RTStrPrintf(pRecord->h.mode,  sizeof(pRecord->h.mode),  "%0.7o",    fmode);
    RTStrPrintf(pRecord->h.uid,   sizeof(pRecord->h.uid),   "%0.7o",    uid);
    RTStrPrintf(pRecord->h.gid,   sizeof(pRecord->h.gid),   "%0.7o",    gid);
    rtTarSizeToRec(pRecord, cbSize);
    RTStrPrintf(pRecord->h.mtime, sizeof(pRecord->h.mtime), "%0.11o",   mtime);
    RTStrPrintf(pRecord->h.magic, sizeof(pRecord->h.magic), "ustar  ");
    RTStrPrintf(pRecord->h.uname, sizeof(pRecord->h.uname), "someone");
    RTStrPrintf(pRecord->h.gname, sizeof(pRecord->h.gname), "someone");
    pRecord->h.linkflag = LF_NORMAL;

    /* Create the checksum out of the new header */
    uint32_t uChkSum = 0;
    int rc = rtTarCalcChkSum(pRecord, &uChkSum);
    if (RT_FAILURE(rc))
        return rc;
    /* Format the checksum */
    RTStrPrintf(pRecord->h.chksum, sizeof(pRecord->h.chksum), "%0.7o", uChkSum);

    return VINF_SUCCESS;
}

DECLINLINE(void *) rtTarMemTmpAlloc(size_t *pcbSize)
{
    *pcbSize = 0;
    /* Allocate a reasonably large buffer, fall back on a tiny one.
     * Note: has to be 512 byte aligned and >= 512 byte. */
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
    pFileInt->fOpenMode = fOpen;
    pFileInt->pszFilename = RTStrDup(pszFilename);
    if (!pFileInt->pszFilename)
    {
        RTMemFree(pFileInt);
        return NULL;
    }

    return pFileInt;
}

DECLINLINE(PRTTARFILEINTERNAL) rtCopyTarFileInternal(PRTTARFILEINTERNAL pInt)
{
    PRTTARFILEINTERNAL pNewInt = (PRTTARFILEINTERNAL)RTMemAllocZ(sizeof(RTTARFILEINTERNAL));
    if (!pNewInt)
        return NULL;

    memcpy(pNewInt, pInt, sizeof(RTTARFILEINTERNAL));
    pNewInt->pszFilename = RTStrDup(pInt->pszFilename);
    if (!pNewInt->pszFilename)
    {
        RTMemFree(pNewInt);
        return NULL;
    }

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
    /* Open the target file */
    RTFILE hNewFile;
    int rc = RTFileOpen(&hNewFile, pszTargetName, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    void *pvTmp = NULL;
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

    } while(0);

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
    /* Open the source file */
    RTFILE hOldFile;
    int rc = RTFileOpen(&hOldFile, pszSrcName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    RTTARFILE hFile = NIL_RTTARFILE;
    void *pvTmp = NULL;
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

        /* Copy the content from pszSrcName over to hFile. This is done block
         * wise in 512 byte steps. After this copying is finished hFile will be
         * on a 512 byte boundary, regardless if the file copied is 512 byte
         * size aligned. */
        uint64_t cbAllWritten = 0; /* Already copied */
        uint64_t cbRead       = 0; /* Actually read in the last step */
        for (;;)
        {
            if (pfnProgressCallback)
                pfnProgressCallback((unsigned)(100.0 / cbOverallSize * cbOverallWritten), pvUser);
            if (cbAllWritten >= cbToCopy)
                break;

            /* Read one block. Either its the buffer size or the rest of the
             * file. */
            cbRead = RT_MIN(cbToCopy - cbAllWritten, cbTmp);
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
    } while (0);

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
    int64_t offSeek = RT_ALIGN(rtTarRecToSize(pRecord), sizeof(RTTARRECORD));
    if (offSeek > 0)
        rc = RTFileSeek(hFile, offSeek, RTFILE_SEEK_CURRENT, NULL);
    return rc;
}

static int rtTarFindFile(RTFILE hFile, const char *pszFile, uint64_t *poff, uint64_t *pcbSize)
{
    /* Assume we are on the file head. */
    int         rc      = VINF_SUCCESS;
    bool        fFound  = false;
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
                *pcbSize = rtTarRecToSize(&record);
                /* Seek back, to position the file pointer at the start of the header. */
                rc = RTFileSeek(hFile, -(int64_t)sizeof(RTTARRECORD), RTFILE_SEEK_CURRENT, poff);
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

#ifdef SOME_UNUSED_FUNCTION
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
                    /* Sum up the overall size */
                    *pcbOverallSize += rtTarRecToSize(&record);
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
#endif /* SOME_UNUSED_FUNCTION */

/******************************************************************************
 *   Public Functions                                                         *
 ******************************************************************************/

RTR3DECL(int) RTTarOpen(PRTTAR phTar, const char *pszTarname, uint32_t fMode, bool fStream)
{
    /*
     * Create a tar instance.
     */
    PRTTARINTERNAL pThis = (PRTTARINTERNAL)RTMemAllocZ(sizeof(RTTARINTERNAL));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic    = RTTAR_MAGIC;
    pThis->fOpenMode   = fMode;
    pThis->fStreamMode = fStream && (fMode & RTFILE_O_READ);

    /*
     * Open the tar file.
     */
    int rc = RTFileOpen(&pThis->hTarFile, pszTarname, fMode);
    if (RT_SUCCESS(rc))
    {
        *phTar = pThis;
        return VINF_SUCCESS;
    }

    RTMemFree(pThis);
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

    if (pInt->hTarFile != NIL_RTFILE)
        rc = RTFileClose(pInt->hTarFile);

    /* Delete any remaining cached file headers. */
    if (pInt->pFileCache)
    {
        rtDeleteTarFileInternal(pInt->pFileCache);
        pInt->pFileCache = NULL;
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
    do /* break loop */
    {
        if (pFileInt->fOpenMode & RTFILE_O_WRITE)
        {
            pInt->fFileOpenForWrite = true;

            /* If we are in write mode, we also in append mode. Add an dummy
             * header at the end of the current file. It will be filled by the
             * close operation. */
            rc = RTFileSeek(pFileInt->pTar->hTarFile, 0, RTFILE_SEEK_END, &pFileInt->offStart);
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
            rc = rtTarFindFile(pFileInt->pTar->hTarFile, pszFilename, &pFileInt->offStart, &pFileInt->cbSize);
            if (RT_FAILURE(rc))
                break;
        }
        else
        {
            /** @todo is something missing here? */
        }

    } while (0);

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
        uint64_t offCur = RTFileTell(pFileInt->pTar->hTarFile);

        /* Check that the file pointer is somewhere within the last open file.
         * If we are at the beginning (nothing read yet) nothing will be done.
         * A user could open/close a file more than once, without reading
         * something. */
        if (   pFileInt->offStart + sizeof(RTTARRECORD) < offCur
            && offCur < RT_ALIGN(pFileInt->offStart + sizeof(RTTARRECORD) + pFileInt->cbSize, sizeof(RTTARRECORD)))
        {
            /* Seek to the next file header. */
            uint64_t offNext = RT_ALIGN(pFileInt->offStart + sizeof(RTTARRECORD) + pFileInt->cbSize, sizeof(RTTARRECORD));
            rc = RTFileSeek(pFileInt->pTar->hTarFile, offNext - offCur, RTFILE_SEEK_CURRENT, NULL);
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
                rc = RTFileWriteAt(pFileInt->pTar->hTarFile,
                                   pFileInt->offStart + sizeof(RTTARRECORD) + pFileInt->cbSize,
                                   &record,
                                   cbSizeAligned - pFileInt->cbSize,
                                   NULL);
                if (RT_FAILURE(rc))
                    break;
            }

            /* Create a header record for the file */
            /* Todo: mode, gid, uid, mtime should be setable (or detected myself) */
            RTTIMESPEC time;
            RTTimeNow(&time);
            rc = rtTarCreateHeaderRecord(&record, pFileInt->pszFilename, pFileInt->cbSize,
                                         0, 0, 0600, RTTimeSpecGetSeconds(&time));
            if (RT_FAILURE(rc))
                break;

            /* Write this at the start of the file data */
            rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->offStart, &record, sizeof(RTTARRECORD), NULL);
            if (RT_FAILURE(rc))
                break;
        }
        while(0);
    }

    /* Now cleanup and delete the handle */
    rtDeleteTarFileInternal(pFileInt);

    return rc;
}

RTR3DECL(int) RTTarFileSeek(RTTARFILE hFile, uint64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if (pFileInt->pTar->fStreamMode)
        return VERR_INVALID_STATE;

    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
        {
            if (offSeek > pFileInt->cbSize)
                return VERR_SEEK_ON_DEVICE;
            pFileInt->offCurrent = offSeek;
            break;
        }
        case RTFILE_SEEK_CURRENT:
        {
            if (pFileInt->offCurrent + offSeek > pFileInt->cbSize)
                return VERR_SEEK_ON_DEVICE;
            pFileInt->offCurrent += offSeek;
            break;
        }
        case RTFILE_SEEK_END:
        {
            if ((int64_t)pFileInt->cbSize - (int64_t)offSeek < 0)
                return VERR_NEGATIVE_SEEK;
            pFileInt->offCurrent = pFileInt->cbSize - offSeek;
            break;
        }
        default: AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    if (poffActual)
        *poffActual = pFileInt->offCurrent;

    return VINF_SUCCESS;
}

RTR3DECL(uint64_t) RTTarFileTell(RTTARFILE hFile)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN_RC(pFileInt, UINT64_MAX);

    return pFileInt->offCurrent;
}

RTR3DECL(int) RTTarFileRead(RTTARFILE hFile, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Todo: optimize this, by checking the current pos */
    return RTTarFileReadAt(hFile, pFileInt->offCurrent, pvBuf, cbToRead, pcbRead);
}

RTR3DECL(int) RTTarFileReadAt(RTTARFILE hFile, uint64_t off, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Check that we not read behind the end of file. If so return immediately. */
    if (off > pFileInt->cbSize)
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS; /* ??? VERR_EOF */
    }

    size_t cbToCopy = RT_MIN(pFileInt->cbSize - off, cbToRead);
    size_t cbTmpRead = 0;
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile, pFileInt->offStart + 512 + off, pvBuf, cbToCopy, &cbTmpRead);
    pFileInt->offCurrent = off + cbTmpRead;
    if (pcbRead)
        *pcbRead = cbTmpRead;

    return rc;
}

RTR3DECL(int) RTTarFileWrite(RTTARFILE hFile, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /** @todo Optimize this, by checking the current pos */
    return RTTarFileWriteAt(hFile, pFileInt->offCurrent, pvBuf, cbToWrite, pcbWritten);
}

RTR3DECL(int) RTTarFileWriteAt(RTTARFILE hFile, uint64_t off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    size_t cbTmpWritten = 0;
    int rc = RTFileWriteAt(pFileInt->pTar->hTarFile, pFileInt->offStart + 512 + off, pvBuf, cbToWrite, &cbTmpWritten);
    pFileInt->cbSize += cbTmpWritten;
    pFileInt->offCurrent = off + cbTmpWritten;
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

    /** @todo If cbSize is smaller than pFileInt->cbSize we have to
     * truncate the current file. */
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
    char szMode[RT_SIZEOFMEMB(RTTARRECORD, h.mode)+1];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile,
                          pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.mode),
                          szMode,
                          RT_SIZEOFMEMB(RTTARRECORD, h.mode),
                          NULL);
    if (RT_FAILURE(rc))
        return rc;
    szMode[sizeof(szMode) - 1] = '\0';

    /* Convert it to an integer */
    return RTStrToUInt32Full(szMode, 8, pfMode);
}

RTR3DECL(int) RTTarFileSetMode(RTTARFILE hFile, uint32_t fMode)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    /* Convert the mode to an string. */
    char szMode[RT_SIZEOFMEMB(RTTARRECORD, h.mode)];
    RTStrPrintf(szMode, sizeof(szMode), "%0.7o", fMode);

    /* Write it directly into the header */
    return RTFileWriteAt(pFileInt->pTar->hTarFile,
                         pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.mode),
                         szMode,
                         RT_SIZEOFMEMB(RTTARRECORD, h.mode),
                         NULL);
}

RTR3DECL(int) RTTarFileGetTime(RTTARFILE hFile, PRTTIMESPEC pTime)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Read the time out of the header entry */
    char szModTime[RT_SIZEOFMEMB(RTTARRECORD, h.mtime) + 1];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile,
                          pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.mtime),
                          szModTime,
                          RT_SIZEOFMEMB(RTTARRECORD, h.mtime),
                          NULL);
    if (RT_FAILURE(rc))
        return rc;
    szModTime[sizeof(szModTime) - 1] = '\0';

    /* Convert it to an integer */
    int64_t cSeconds;
    rc = RTStrToInt64Full(szModTime, 12, &cSeconds);

    /* And back to our time structure */
    if (RT_SUCCESS(rc))
        RTTimeSpecSetSeconds(pTime, cSeconds);

    return rc;
}

RTR3DECL(int) RTTarFileSetTime(RTTARFILE hFile, PRTTIMESPEC pTime)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;

    /* Convert the time to an string. */
    char szModTime[RT_SIZEOFMEMB(RTTARRECORD, h.mtime)];
    RTStrPrintf(szModTime, sizeof(szModTime), "%0.11o", RTTimeSpecGetSeconds(pTime));

    /* Write it directly into the header */
    return RTFileWriteAt(pFileInt->pTar->hTarFile,
                         pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.mtime),
                         szModTime,
                         RT_SIZEOFMEMB(RTTARRECORD, h.mtime),
                         NULL);
}

RTR3DECL(int) RTTarFileGetOwner(RTTARFILE hFile, uint32_t *pUid, uint32_t *pGid)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    /* Read the uid and gid out of the header entry */
    AssertCompileAdjacentMembers(RTTARRECORD, h.uid, h.gid);
    char szUidGid[RT_SIZEOFMEMB(RTTARRECORD, h.uid) + RT_SIZEOFMEMB(RTTARRECORD, h.gid) + 1];
    int rc = RTFileReadAt(pFileInt->pTar->hTarFile,
                          pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.uid),
                          szUidGid,
                          sizeof(szUidGid) - 1,
                          NULL);
    if (RT_FAILURE(rc))
        return rc;
    szUidGid[sizeof(szUidGid) - 1] = '\0';

    /* Convert it to integer */
    rc = RTStrToUInt32Full(&szUidGid[RT_SIZEOFMEMB(RTTARRECORD, h.uid)], 8, pGid);
    if (RT_SUCCESS(rc))
    {
        szUidGid[RT_SIZEOFMEMB(RTTARRECORD, h.uid)] = '\0';
        rc = RTStrToUInt32Full(szUidGid, 8, pUid);
    }
    return rc;
}

RTR3DECL(int) RTTarFileSetOwner(RTTARFILE hFile, uint32_t uid, uint32_t gid)
{
    PRTTARFILEINTERNAL pFileInt = hFile;
    RTTARFILE_VALID_RETURN(pFileInt);

    if ((pFileInt->fOpenMode & RTFILE_O_WRITE) != RTFILE_O_WRITE)
        return VERR_WRITE_ERROR;
    AssertReturn(uid == (uint32_t)-1 || uid <= 07777777, VERR_OUT_OF_RANGE);
    AssertReturn(gid == (uint32_t)-1 || gid <= 07777777, VERR_OUT_OF_RANGE);

    int rc = VINF_SUCCESS;

    if (uid != (uint32_t)-1)
    {
        /* Convert the uid to an string. */
        char szUid[RT_SIZEOFMEMB(RTTARRECORD, h.uid)];
        RTStrPrintf(szUid, sizeof(szUid), "%0.7o", uid);

        /* Write it directly into the header */
        rc = RTFileWriteAt(pFileInt->pTar->hTarFile,
                           pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.uid),
                           szUid,
                           RT_SIZEOFMEMB(RTTARRECORD, h.uid),
                           NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    if (gid != (uint32_t)-1)
    {
        /* Convert the gid to an string. */
        char szGid[RT_SIZEOFMEMB(RTTARRECORD, h.gid)];
        RTStrPrintf(szGid, sizeof(szGid), "%0.7o", gid);

        /* Write it directly into the header */
        rc = RTFileWriteAt(pFileInt->pTar->hTarFile,
                           pFileInt->offStart + RT_OFFSETOF(RTTARRECORD, h.gid),
                           szGid,
                           RT_SIZEOFMEMB(RTTARRECORD, h.gid),
                           NULL);
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
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false /*fStream*/);
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
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false /*fStream*/);
    if (RT_FAILURE(rc))
        return rc;

    /* This is done by internal methods, cause we didn't have a RTTARDIR
     * interface, yet. This should be fixed someday. */

    PRTTARINTERNAL pInt = hTar;
    char **papszFiles = NULL;
    size_t cFiles = 0;
    do /* break loop */
    {
        /* Initialize the file name array with one slot */
        size_t cFilesAlloc = 1;
        papszFiles = (char **)RTMemAlloc(sizeof(char *));
        if (!papszFiles)
        {
            rc = VERR_NO_MEMORY;
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
    } while(0);

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

RTR3DECL(int) RTTarExtractFileToBuf(const char *pszTarFile, void **ppvBuf, size_t *pcbSize, const char *pszFile,
                                    PFNRTPROGRESS pfnProgressCallback, void *pvUser)
{
    /*
     * Validate input
     */
    AssertPtrReturn(pszTarFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pvUser, VERR_INVALID_POINTER);

    /** @todo progress bar - is this TODO still valid? */

    int         rc      = VINF_SUCCESS;
    RTTAR       hTar    = NIL_RTTAR;
    RTTARFILE   hFile   = NIL_RTTARFILE;
    char       *pvTmp   = NULL;
    uint64_t    cbToCopy= 0;
    do /* break loop */
    {
        rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false /*fStream*/);
        if (RT_FAILURE(rc))
            break;
        rc = RTTarFileOpen(hTar, &hFile, pszFile, RTFILE_O_OPEN | RTFILE_O_READ);
        if (RT_FAILURE(rc))
            break;
        rc = RTTarFileGetSize(hFile, &cbToCopy);
        if (RT_FAILURE(rc))
            break;

        /* Allocate the memory for the file content. */
        pvTmp = (char *)RTMemAlloc(cbToCopy);
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
    } while (0);

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

RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles,
                                size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser)
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
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, false /*fStream*/);
    if (RT_FAILURE(rc))
        return rc;

    do /* break loop */
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
        for (size_t i = 0; i < cFiles; ++i)
        {
            RTTARFILE hFile;
            rc = RTTarFileOpen(hTar, &hFile, papszFiles[i], RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
            if (RT_FAILURE(rc))
                break;
            char *pszTargetFile = RTPathJoinA(pszOutputDir, papszFiles[i]);
            if (pszTargetFile)
                rc = rtTarExtractFileToFile(hFile, pszTargetFile, cbOverallSize, cbOverallWritten, pfnProgressCallback, pvUser);
            else
                rc = VERR_NO_STR_MEMORY;
            RTStrFree(pszTargetFile);
            RTTarFileClose(hFile);
            if (RT_FAILURE(rc))
                break;
        }
    } while (0);

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
    int rc = RTTarOpen(&hTar, pszTarFile, RTFILE_O_CREATE | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE, false /*fStream*/);
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
        rc = RTTarCurrentFile(hTar, NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Check that the file pointer is somewhere within the last open file.
     * If not we are somehow busted. */
    uint64_t offCur = RTFileTell(pInt->hTarFile);
    if (!(   pInt->pFileCache->offStart <= offCur
          && offCur < pInt->pFileCache->offStart + sizeof(RTTARRECORD) + pInt->pFileCache->cbSize))
        return VERR_INVALID_STATE;

    /* Seek to the next file header. */
    uint64_t offNext = RT_ALIGN(pInt->pFileCache->offStart + sizeof(RTTARRECORD) + pInt->pFileCache->cbSize, sizeof(RTTARRECORD));
    rc = RTFileSeek(pInt->hTarFile, offNext - offCur, RTFILE_SEEK_CURRENT, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /* Again check the current filename to fill the cache with the new value. */
    return RTTarCurrentFile(hTar, NULL);
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
        if (pInt->pFileCache->offStart + sizeof(RTTARRECORD) == RTFileTell(pInt->hTarFile))
        {
            /* Yes, so the streaming can start. Just return the cached file
             * structure to the caller. */
            *phFile = rtCopyTarFileInternal(pInt->pFileCache);
            if (ppszFilename)
                *ppszFilename = RTStrDup(pInt->pFileCache->pszFilename);
            return VINF_SUCCESS;
        }

        /* Else delete the last open file cache. Might be recreated below. */
        rtDeleteTarFileInternal(pInt->pFileCache);
        pInt->pFileCache = NULL;
    }

    PRTTARFILEINTERNAL pFileInt = NULL;
    do /* break loop */
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
            pFileInt->cbSize = rtTarRecToSize(&record);
            /* The start is -512 from here. */
            pFileInt->offStart = RTFileTell(pInt->hTarFile) - sizeof(RTTARRECORD);

            /* Copy the new file structure to our cache. */
            pInt->pFileCache = rtCopyTarFileInternal(pFileInt);
            if (ppszFilename)
                *ppszFilename = RTStrDup(pFileInt->pszFilename);
        }
    } while (0);

    if (RT_FAILURE(rc))
    {
        if (pFileInt)
            rtDeleteTarFileInternal(pFileInt);
    }
    else
        *phFile = pFileInt;

    return rc;
}

