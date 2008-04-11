/** $Id$ */
/** @file
 * VMDK Disk image, Core Code.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_VMDK
#include "VBoxHDD-newInternal.h"
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/rand.h>


/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/** Maximum encoded string size (including NUL) we allow for VMDK images.
 * Deliberately not set high to avoid running out of descriptor space. */
#define VMDK_ENCODED_COMMENT_MAX 1024

/** VMDK descriptor DDB entry for PCHS cylinders. */
#define VMDK_DDB_GEO_PCHS_CYLINDERS "ddb.geometry.cylinders"

/** VMDK descriptor DDB entry for PCHS heads. */
#define VMDK_DDB_GEO_PCHS_HEADS "ddb.geometry.heads"

/** VMDK descriptor DDB entry for PCHS sectors. */
#define VMDK_DDB_GEO_PCHS_SECTORS "ddb.geometry.sectors"

/** VMDK descriptor DDB entry for LCHS cylinders. */
#define VMDK_DDB_GEO_LCHS_CYLINDERS "ddb.geometry.biosCylinders"

/** VMDK descriptor DDB entry for LCHS heads. */
#define VMDK_DDB_GEO_LCHS_HEADS "ddb.geometry.biosHeads"

/** VMDK descriptor DDB entry for LCHS sectors. */
#define VMDK_DDB_GEO_LCHS_SECTORS "ddb.geometry.biosSectors"

/** VMDK descriptor DDB entry for image UUID. */
#define VMDK_DDB_IMAGE_UUID "ddb.uuid.image"

/** VMDK descriptor DDB entry for image modification UUID. */
#define VMDK_DDB_MODIFICATION_UUID "ddb.uuid.modification"

/** VMDK descriptor DDB entry for parent image UUID. */
#define VMDK_DDB_PARENT_UUID "ddb.uuid.parent"

/** VMDK descriptor DDB entry for parent image modification UUID. */
#define VMDK_DDB_PARENT_MODIFICATION_UUID "ddb.uuid.parentmodification"

/**
 * Magic number for hosted images created by VMware Workstation 4, VMware
 * Workstation 5, VMware Server or VMware Player.
 */
#define VMDK_SPARSE_MAGICNUMBER 0x564d444b /* 'V' 'M' 'D' 'K' */

/** VMDK hosted sparse extent header. */
#pragma pack(1)
typedef struct SparseExtentHeader
{
    uint32_t    magicNumber;
    uint32_t    version;
    uint32_t    flags;
    uint64_t    capacity;
    uint64_t    grainSize;
    uint64_t    descriptorOffset;
    uint64_t    descriptorSize;
    uint32_t    numGTEsPerGT;
    uint64_t    rgdOffset;
    uint64_t    gdOffset;
    uint64_t    overHead;
    bool        uncleanShutdown;
    char        singleEndLineChar;
    char        nonEndLineChar;
    char        doubleEndLineChar1;
    char        doubleEndLineChar2;
    uint8_t     pad[435];
} SparseExtentHeader;
#pragma pack()

/** VMDK capacity for a single chunk when 2G splitting is turned on. Should be
 * divisible by the default grain size (64K) */
#define VMDK_2G_SPLIT_SIZE (2047 * 1024 * 1024)


#ifdef VBOX_WITH_VMDK_ESX

/** @todo the ESX code is not tested, not used, and lacks error messages. */

/**
 * Magic number for images created by VMware GSX Server 3 or ESX Server 3.
 */
#define VMDK_ESX_SPARSE_MAGICNUMBER 0x44574f43 /* 'C' 'O' 'W' 'D' */

#pragma pack(1)
typedef struct COWDisk_Header
{
    uint32_t    magicNumber;
    uint32_t    version;
    uint32_t    flags;
    uint32_t    numSectors;
    uint32_t    grainSize;
    uint32_t    gdOffset;
    uint32_t    numGDEntries;
    uint32_t    freeSector;
    /* The spec incompletely documents quite a few further fields, but states
     * that they are unused by the current format. Replace them by padding. */
    char        reserved1[1604];
    uint32_t    savedGeneration;
    char        reserved2[8];
    uint32_t    uncleanShutdown;
    char        padding[396];
} COWDisk_Header;
#pragma pack()
#endif /* VBOX_WITH_VMDK_ESX */


/** Convert sector number/size to byte offset/size. */
#define VMDK_SECTOR2BYTE(u) ((u) << 9)

/** Convert byte offset/size to sector number/size. */
#define VMDK_BYTE2SECTOR(u) ((u) >> 9)

/**
 * VMDK extent type.
 */
typedef enum VMDKETYPE
{
    /** Hosted sparse extent. */
    VMDKETYPE_HOSTED_SPARSE = 1,
    /** Flat extent. */
    VMDKETYPE_FLAT,
    /** Zero extent. */
    VMDKETYPE_ZERO
#ifdef VBOX_WITH_VMDK_ESX
    ,
    /** ESX sparse extent. */
    VMDKETYPE_ESX_SPARSE
#endif /* VBOX_WITH_VMDK_ESX */
} VMDKETYPE, *PVMDKETYPE;

/**
 * VMDK access type for a extent.
 */
typedef enum VMDKACCESS
{
    /** No access allowed. */
    VMDKACCESS_NOACCESS = 0,
    /** Read-only access. */
    VMDKACCESS_READONLY,
    /** Read-write access. */
    VMDKACCESS_READWRITE
} VMDKACCESS, *PVMDKACCESS;

/**
 * VMDK extent data structure.
 */
typedef struct VMDKEXTENT
{
    /** File handle. */
    RTFILE      File;
    /** Base name of the image extent. */
    const char  *pszBasename;
    /** Full name of the image extent. */
    const char  *pszFullname;
    /** Number of sectors in this extent. */
    uint64_t    cSectors;
    /** Number of sectors per block (grain in VMDK speak). */
    uint64_t    cSectorsPerGrain;
    /** Starting sector number of descriptor. */
    uint64_t    uDescriptorSector;
    /** Size of descriptor in sectors. */
    uint64_t    cDescriptorSectors;
    /** Starting sector number of grain directory. */
    uint64_t    uSectorGD;
    /** Starting sector number of redundant grain directory. */
    uint64_t    uSectorRGD;
    /** Total number of metadata sectors. */
    uint64_t    cOverheadSectors;
    /** Nominal size (i.e. as described by the descriptor) of this extent. */
    uint64_t    cNominalSectors;
    /** Sector offset (i.e. as described by the descriptor) of this extent. */
    uint64_t    uSectorOffset;
    /** Number of entries in a grain table. */
    uint32_t    cGTEntries;
    /** Number of sectors reachable via a grain directory entry. */
    uint32_t    cSectorsPerGDE;
    /** Number of entries in the grain directory. */
    uint32_t    cGDEntries;
    /** Pointer to the next free sector. Legacy information. Do not use. */
    uint32_t    uFreeSector;
    /** Number of this extent in the list of images. */
    uint32_t    uExtent;
    /** Pointer to the descriptor (NULL if no descriptor in this extent). */
    char        *pDescData;
    /** Pointer to the grain directory. */
    uint32_t    *pGD;
    /** Pointer to the redundant grain directory. */
    uint32_t     *pRGD;
    /** Type of this extent. */
    VMDKETYPE   enmType;
    /** Access to this extent. */
    VMDKACCESS  enmAccess;
    /** Flag whether this extent is marked as unclean. */
    bool        fUncleanShutdown;
    /** Flag whether the metadata in the extent header needs to be updated. */
    bool        fMetaDirty;
    /** Reference to the image in which this extent is used. Do not use this
     * on a regular basis to avoid passing pImage references to functions
     * explicitly. */
    struct VMDKIMAGE *pImage;
} VMDKEXTENT, *PVMDKEXTENT;

/**
 * Extents files entry. Used for opening a particular file only once.
 */
typedef struct VMDKFILE
{
    /** Pointer to filename. Local copy. */
    const char *pszFilename;
    /** File open flags for consistency checking. */
    unsigned    fOpen;
    /** File handle. */
    RTFILE      File;
    /** Reference counter. */
    unsigned    uReferences;
    /** Flag whether the file should be deleted on last close. */
    bool        fDelete;
    /** Pointer to next file descriptor. Singly linked list is fast enough. */
    struct VMDKFILE *pNext;
} VMDKFILE, *PVMDKFILE;

/**
 * Grain table cache size. Allocated per image.
 */
#define VMDK_GT_CACHE_SIZE 256

/**
 * Grain table block size. Smaller than an actual grain table block to allow
 * more grain table blocks to be cached without having to allocate excessive
 * amounts of memory for the cache.
 */
#define VMDK_GT_CACHELINE_SIZE 128


/**
 * Maximum number of lines in a descriptor file. Not worth the effort of
 * making it variable. Descriptor files are generally very short (~20 lines).
 */
#define VMDK_DESCRIPTOR_LINES_MAX   100U

/**
 * Parsed descriptor information. Allows easy access and update of the
 * descriptor (whether separate file or not). Free form text files suck.
 */
typedef struct VMDKDESCRIPTOR
{
    /** Line number of first entry of the disk descriptor. */
    unsigned    uFirstDesc;
    /** Line number of first entry in the extent description. */
    unsigned    uFirstExtent;
    /** Line number of first disk database entry. */
    unsigned    uFirstDDB;
    /** Total number of lines. */
    unsigned    cLines;
    /** Total amount of memory available for the descriptor. */
    size_t      cbDescAlloc;
    /** Set if descriptor has been changed and not yet written to disk. */
    bool        fDirty;
    /** Array of pointers to the data in the descriptor. */
    char        *aLines[VMDK_DESCRIPTOR_LINES_MAX];
    /** Array of line indices pointing to the next non-comment line. */
    unsigned    aNextLines[VMDK_DESCRIPTOR_LINES_MAX];
} VMDKDESCRIPTOR, *PVMDKDESCRIPTOR;


/**
 * Cache entry for translating extent/sector to a sector number in that
 * extent.
 */
typedef struct VMDKGTCACHEENTRY
{
    /** Extent number for which this entry is valid. */
    uint32_t    uExtent;
    /** GT data block number. */
    uint64_t    uGTBlock;
    /** Data part of the cache entry. */
    uint32_t    aGTData[VMDK_GT_CACHELINE_SIZE];
} VMDKGTCACHEENTRY, *PVMDKGTCACHEENTRY;

/**
 * Cache data structure for blocks of grain table entries. For now this is a
 * fixed size direct mapping cache, but this should be adapted to the size of
 * the sparse image and maybe converted to a set-associative cache. The
 * implementation below implements a write-through cache with write allocate.
 */
typedef struct VMDKGTCACHE
{
    /** Cache entries. */
    VMDKGTCACHEENTRY    aGTCache[VMDK_GT_CACHE_SIZE];
    /** Number of cache entries (currently unused). */
    unsigned            cEntries;
} VMDKGTCACHE, *PVMDKGTCACHE;

/**
 * Complete VMDK image data structure. Mainly a collection of extents and a few
 * extra global data fields.
 */
typedef struct VMDKIMAGE
{
    /** Pointer to the image extents. */
    PVMDKEXTENT     pExtents;
    /** Number of image extents. */
    unsigned        cExtents;
    /** Pointer to the files list, for opening a file referenced multiple
     * times only once (happens mainly with raw partition access). */
    PVMDKFILE       pFiles;

    /** Base image name. */
    const char      *pszFilename;
    /** Descriptor file if applicable. */
    RTFILE          File;

    /** Error callback. */
    PFNVDERROR      pfnError;
    /** Opaque data for error callback. */
    void            *pvErrorUser;

    /** Open flags passed by VBoxHD layer. */
    unsigned        uOpenFlags;
    /** Image type. */
    VDIMAGETYPE     enmImageType;
    /** Image flags defined during creation or determined during open. */
    unsigned        uImageFlags;
    /** Total size of the image. */
    uint64_t        cbSize;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY   PCHSGeometry;
    /** Logical geometry of this image. */
    PDMMEDIAGEOMETRY   LCHSGeometry;
    /** Image UUID. */
    RTUUID          ImageUuid;
    /** Image modification UUID. */
    RTUUID          ModificationUuid;
    /** Parent image UUID. */
    RTUUID          ParentUuid;
    /** Parent image modification UUID. */
    RTUUID          ParentModificationUuid;

    /** Pointer to grain table cache, if this image contains sparse extents. */
    PVMDKGTCACHE    pGTCache;
    /** Pointer to the descriptor (NULL if no separate descriptor file). */
    char            *pDescData;
    /** Allocation size of the descriptor file. */
    size_t          cbDescAlloc;
    /** Parsed descriptor file content. */
    VMDKDESCRIPTOR  Descriptor;
} VMDKIMAGE, *PVMDKIMAGE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static void vmdkFreeGrainDirectory(PVMDKEXTENT pExtent);

static void vmdkFreeExtentData(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                               bool fDelete);

static int vmdkCreateExtents(PVMDKIMAGE pImage, unsigned cExtents);
static int vmdkFlushImage(PVMDKIMAGE pImage);
static int vmdkSetImageComment(PVMDKIMAGE pImage, const char *pszComment);
static void vmdkFreeImage(PVMDKIMAGE pImage, bool fDelete);


/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) vmdkError(PVMDKIMAGE pImage, int rc, RT_SRC_POS_DECL,
                          const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pfnError)
        pImage->pfnError(pImage->pvErrorUser, rc, RT_SRC_POS_ARGS,
                         pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * Internal: open a file (using a file descriptor cache to ensure each file
 * is only opened once - anything else can cause locking problems).
 */
static int vmdkFileOpen(PVMDKIMAGE pImage, PRTFILE pFile,
                        const char *pszFilename, unsigned fOpen)
{
    int rc = VINF_SUCCESS;
    PVMDKFILE pVmdkFile;

    for (pVmdkFile = pImage->pFiles;
         pVmdkFile != NULL;
         pVmdkFile = pVmdkFile->pNext)
    {
        if (!strcmp(pszFilename, pVmdkFile->pszFilename))
        {
            Assert(fOpen == pVmdkFile->fOpen);
            pVmdkFile->uReferences++;
            *pFile = pVmdkFile->File;
            return rc;
        }
    }

    /* If we get here, there's no matching entry in the cache. */
    pVmdkFile = (PVMDKFILE)RTMemAllocZ(sizeof(VMDKFILE));
    if (!VALID_PTR(pVmdkFile))
    {
        *pFile = NIL_RTFILE;
        return VERR_NO_MEMORY;
    }

    pVmdkFile->pszFilename = RTStrDup(pszFilename);
    if (!VALID_PTR(pVmdkFile->pszFilename))
    {
        RTMemFree(pVmdkFile);
        *pFile = NIL_RTFILE;
        return VERR_NO_MEMORY;
    }
    pVmdkFile->fOpen = fOpen;
    rc = RTFileOpen(&pVmdkFile->File, pszFilename, fOpen);
    if (VBOX_SUCCESS(rc))
    {
        pVmdkFile->uReferences = 1;
        pVmdkFile->pNext = pImage->pFiles;
        pImage->pFiles = pVmdkFile;
        *pFile = pVmdkFile->File;
    }
    else
    {
        RTStrFree((char *)(void *)pVmdkFile->pszFilename);
        RTMemFree(pVmdkFile);
        *pFile = NIL_RTFILE;
    }

    return rc;
}

/**
 * Internal: close a file, updating the file descriptor cache.
 */
static int vmdkFileClose(PVMDKIMAGE pImage, PRTFILE pFile, bool fDelete)
{
    int rc = VINF_SUCCESS;
    RTFILE File;
    PVMDKFILE pVmdkFile, pPrev;

    Assert(VALID_PTR(pFile) && *pFile != NIL_RTFILE);
    File = *pFile;

    pPrev = NULL;
    for (pVmdkFile = pImage->pFiles;
         pVmdkFile != NULL;
         pVmdkFile = pVmdkFile->pNext)
    {
        if (File == pVmdkFile->File)
        {
            pVmdkFile->fDelete |= fDelete;
            Assert(pVmdkFile->uReferences);
            pVmdkFile->uReferences--;
            if (pVmdkFile->uReferences == 0)
            {
                /* Unchain the element from the list. */
                if (pPrev == NULL)
                    pImage->pFiles = pVmdkFile->pNext;
                else
                    pPrev->pNext = pVmdkFile->pNext;
                rc = RTFileClose(File);
                *pFile = NIL_RTFILE;
                if (VBOX_SUCCESS(rc) && pVmdkFile->fDelete)
                    rc = RTFileDelete(pVmdkFile->pszFilename);
                RTStrFree((char *)(void *)pVmdkFile->pszFilename);
                RTMemFree(pVmdkFile);
            }
            return rc;
        }
        pPrev = pVmdkFile;
    }

    AssertMsgFailed(("trying to close unknown file %#p", File));
    return VERR_INVALID_PARAMETER;
}

/**
 * Internal: check if all files are closed, prevent leaking resources.
 */
static int vmdkFileCheckAllClose(PVMDKIMAGE pImage)
{
    int rc = VINF_SUCCESS, rc2;
    PVMDKFILE pVmdkFile;

    Assert(pImage->pFiles == NULL);
    for (pVmdkFile = pImage->pFiles;
         pVmdkFile != NULL;
         pVmdkFile = pVmdkFile->pNext)
    {
        LogRel(("VMDK: leaking reference to file \"%s\"\n",
                pVmdkFile->pszFilename));
        pImage->pFiles = pVmdkFile->pNext;
        rc2 = RTFileClose(pVmdkFile->File);
        if (VBOX_SUCCESS(rc) && pVmdkFile->fDelete)
            rc2 = RTFileDelete(pVmdkFile->pszFilename);
        RTStrFree((char *)(void *)pVmdkFile->pszFilename);
        RTMemFree(pVmdkFile);
        if (VBOX_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}

/**
 * Internal: truncate a string (at a UTF8 code point boundary) and encode the
 * critical non-ASCII characters.
 */
static char *vmdkEncodeString(const char *psz)
{
    char szEnc[VMDK_ENCODED_COMMENT_MAX + 3];
    char *pszDst = szEnc;

    Assert(VALID_PTR(psz));

    for (; *psz; psz = RTStrNextCp(psz))
    {
        char *pszDstPrev = pszDst;
        RTUNICP Cp = RTStrGetCp(psz);
        if (Cp == '\\')
        {
            pszDst = RTStrPutCp(pszDst, Cp);
            pszDst = RTStrPutCp(pszDst, Cp);
        }
        else if (Cp == '\n')
        {
            pszDst = RTStrPutCp(pszDst, '\\');
            pszDst = RTStrPutCp(pszDst, 'n');
        }
        else if (Cp == '\r')
        {
            pszDst = RTStrPutCp(pszDst, '\\');
            pszDst = RTStrPutCp(pszDst, 'r');
        }
        else
            pszDst = RTStrPutCp(pszDst, Cp);
        if (pszDst - szEnc >= VMDK_ENCODED_COMMENT_MAX - 1)
        {
            pszDst = pszDstPrev;
            break;
        }
    }
    *pszDst = '\0';
    return RTStrDup(szEnc);
}

/**
 * Internal: decode a string and store it into the specified string.
 */
static int vmdkDecodeString(const char *pszEncoded, char *psz, size_t cb)
{
    int rc = VINF_SUCCESS;
    char szBuf[4];

    if (!cb)
        return VERR_BUFFER_OVERFLOW;

    Assert(VALID_PTR(psz));

    for (; *pszEncoded; pszEncoded = RTStrNextCp(pszEncoded))
    {
        char *pszDst = szBuf;
        RTUNICP Cp = RTStrGetCp(pszEncoded);
        if (Cp == '\\')
        {
            pszEncoded = RTStrNextCp(pszEncoded);
            RTUNICP CpQ = RTStrGetCp(pszEncoded);
            if (CpQ == 'n')
                RTStrPutCp(pszDst, '\n');
            else if (CpQ == 'r')
                RTStrPutCp(pszDst, '\r');
            else if (CpQ == '\0')
            {
                rc = VERR_VDI_INVALID_HEADER;
                break;
            }
            else
                RTStrPutCp(pszDst, CpQ);
        }
        else
            pszDst = RTStrPutCp(pszDst, Cp);

        /* Need to leave space for terminating NUL. */
        if ((size_t)(pszDst - szBuf) + 1 >= cb)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }
        memcpy(psz, szBuf, pszDst - szBuf);
        psz += pszDst - szBuf;
    }
    *psz = '\0';
    return rc;
}

static int vmdkReadGrainDirectory(PVMDKEXTENT pExtent)
{
    int rc = VINF_SUCCESS;
    unsigned i;
    uint32_t *pGD = NULL, *pRGD = NULL, *pGDTmp, *pRGDTmp;
    size_t cbGD = pExtent->cGDEntries * sizeof(uint32_t);

    pGD = (uint32_t *)RTMemAllocZ(cbGD);
    if (!pGD)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pExtent->pGD = pGD;
    rc = RTFileReadAt(pExtent->File, VMDK_SECTOR2BYTE(pExtent->uSectorGD),
                      pGD, cbGD, NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: could not read grain directory in '%s'"), pExtent->pszFullname);
        goto out;
    }
    for (i = 0, pGDTmp = pGD; i < pExtent->cGDEntries; i++, pGDTmp++)
        *pGDTmp = RT_LE2H_U32(*pGDTmp);

    if (pExtent->uSectorRGD)
    {
        pRGD = (uint32_t *)RTMemAllocZ(cbGD);
        if (!pRGD)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }
        pExtent->pRGD = pRGD;
        rc = RTFileReadAt(pExtent->File, VMDK_SECTOR2BYTE(pExtent->uSectorRGD),
                          pRGD, cbGD, NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
        {
            rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: could not read redundant grain directory in '%s'"), pExtent->pszFullname);
            goto out;
        }
        for (i = 0, pRGDTmp = pRGD; i < pExtent->cGDEntries; i++, pRGDTmp++)
            *pRGDTmp = RT_LE2H_U32(*pRGDTmp);

        /* Check grain table and redundant grain table for consistency. */
        size_t cbGT = pExtent->cGTEntries;
        uint32_t *pTmpGT1 = (uint32_t *)RTMemTmpAlloc(cbGT);
        if (!pTmpGT1)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }
        uint32_t *pTmpGT2 = (uint32_t *)RTMemTmpAlloc(cbGT);
        if (!pTmpGT2)
        {
            RTMemTmpFree(pTmpGT1);
            rc = VERR_NO_MEMORY;
            goto out;
        }

        for (i = 0, pGDTmp = pGD, pRGDTmp = pRGD;
             i < pExtent->cGDEntries;
             i++, pGDTmp++, pRGDTmp++)
        {
            /* If no grain table is allocated skip the entry. */
            if (*pGDTmp == 0 && *pRGDTmp == 0)
                continue;

            if (*pGDTmp == 0 || *pRGDTmp == 0 || *pGDTmp == *pRGDTmp)
            {
                /* Just one grain directory entry refers to a not yet allocated
                 * grain table or both grain directory copies refer to the same
                 * grain table. Not allowed. */
                RTMemTmpFree(pTmpGT1);
                RTMemTmpFree(pTmpGT2);
                rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: inconsistent references to grain directory in '%s'"), pExtent->pszFullname);
                goto out;
            }
            rc = RTFileReadAt(pExtent->File, VMDK_SECTOR2BYTE(*pGDTmp),
                              pTmpGT1, cbGT, NULL);
            if (VBOX_FAILURE(rc))
            {
                rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error reading grain table in '%s'"), pExtent->pszFullname);
                RTMemTmpFree(pTmpGT1);
                RTMemTmpFree(pTmpGT2);
                goto out;
            }
            rc = RTFileReadAt(pExtent->File, VMDK_SECTOR2BYTE(*pRGDTmp),
                              pTmpGT2, cbGT, NULL);
            if (VBOX_FAILURE(rc))
            {
                rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error reading backup grain table in '%s'"), pExtent->pszFullname);
                RTMemTmpFree(pTmpGT1);
                RTMemTmpFree(pTmpGT2);
                goto out;
            }
            if (memcmp(pTmpGT1, pTmpGT2, cbGT))
            {
                RTMemTmpFree(pTmpGT1);
                RTMemTmpFree(pTmpGT2);
                rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: inconsistency between grain table and backup grain table in '%s'"), pExtent->pszFullname);
                goto out;
            }
        }

        /** @todo figure out what to do for unclean VMDKs. */
    }

out:
    if (VBOX_FAILURE(rc))
        vmdkFreeGrainDirectory(pExtent);
    return rc;
}

static int vmdkCreateGrainDirectory(PVMDKEXTENT pExtent, uint64_t uStartSector,
                                    bool fPreAlloc)
{
    int rc = VINF_SUCCESS;
    unsigned i;
    uint32_t *pGD = NULL, *pRGD = NULL;
    size_t cbGD = pExtent->cGDEntries * sizeof(uint32_t);
    size_t cbGDRounded = RT_ALIGN_64(pExtent->cGDEntries * sizeof(uint32_t), 512);
    size_t cbGTRounded;
    uint64_t cbOverhead;

    if (fPreAlloc)
        cbGTRounded = RT_ALIGN_64(pExtent->cGDEntries * pExtent->cGTEntries * sizeof(uint32_t), 512);
    else
        cbGTRounded = 0;

    pGD = (uint32_t *)RTMemAllocZ(cbGD);
    if (!pGD)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pExtent->pGD = pGD;
    pRGD = (uint32_t *)RTMemAllocZ(cbGD);
    if (!pRGD)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pExtent->pRGD = pRGD;

    cbOverhead = RT_ALIGN_64(VMDK_SECTOR2BYTE(uStartSector) + 2 * (cbGDRounded + cbGTRounded), VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
    rc = RTFileSetSize(pExtent->File, cbOverhead);
    if (VBOX_FAILURE(rc))
        goto out;
    pExtent->uSectorRGD = uStartSector;
    pExtent->uSectorGD = uStartSector + VMDK_BYTE2SECTOR(cbGDRounded + cbGTRounded);

    if (fPreAlloc)
    {
        uint32_t uGTSectorLE;
        uint64_t uOffsetSectors;

        uOffsetSectors = pExtent->uSectorRGD + VMDK_BYTE2SECTOR(cbGDRounded);
        for (i = 0; i < pExtent->cGDEntries; i++)
        {
            pRGD[i] = uOffsetSectors;
            uGTSectorLE = RT_H2LE_U64(uOffsetSectors);
            /* Write the redundant grain directory entry to disk. */
            rc = RTFileWriteAt(pExtent->File,
                               VMDK_SECTOR2BYTE(pExtent->uSectorRGD) + i * sizeof(uGTSectorLE),
                               &uGTSectorLE, sizeof(uGTSectorLE), NULL);
            if (VBOX_FAILURE(rc))
                return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write new redundant grain directory entry in '%s'"), pExtent->pszFullname);
            uOffsetSectors += VMDK_BYTE2SECTOR(pExtent->cGTEntries * sizeof(uint32_t));
        }

        uOffsetSectors = pExtent->uSectorGD + VMDK_BYTE2SECTOR(cbGDRounded);
        for (i = 0; i < pExtent->cGDEntries; i++)
        {
            pGD[i] = uOffsetSectors;
            uGTSectorLE = RT_H2LE_U64(uOffsetSectors);
            /* Write the grain directory entry to disk. */
            rc = RTFileWriteAt(pExtent->File,
                               VMDK_SECTOR2BYTE(pExtent->uSectorGD) + i * sizeof(uGTSectorLE),
                               &uGTSectorLE, sizeof(uGTSectorLE), NULL);
            if (VBOX_FAILURE(rc))
                return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write new grain directory entry in '%s'"), pExtent->pszFullname);
            uOffsetSectors += VMDK_BYTE2SECTOR(pExtent->cGTEntries * sizeof(uint32_t));
        }
    }
    pExtent->cOverheadSectors = VMDK_BYTE2SECTOR(cbOverhead);

out:
    if (VBOX_FAILURE(rc))
        vmdkFreeGrainDirectory(pExtent);
    return rc;
}

static void vmdkFreeGrainDirectory(PVMDKEXTENT pExtent)
{
    if (pExtent->pGD)
    {
        RTMemFree(pExtent->pGD);
        pExtent->pGD = NULL;
    }
    if (pExtent->pRGD)
    {
        RTMemFree(pExtent->pRGD);
        pExtent->pRGD = NULL;
    }
}

static int vmdkStringUnquote(PVMDKIMAGE pImage, const char *pszStr,
                             char **ppszUnquoted, char **ppszNext)
{
    char *pszQ;
    char *pszUnquoted;

    /* Skip over whitespace. */
    while (*pszStr == ' ' || *pszStr == '\t')
        pszStr++;
    if (*pszStr++ != '"')
        return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrectly quoted value in descriptor in '%s'"), pImage->pszFilename);

    pszQ = (char *)strchr(pszStr, '"');
    if (pszQ == NULL)
        return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrectly quoted value in descriptor in '%s'"), pImage->pszFilename);
    pszUnquoted = (char *)RTMemTmpAlloc(pszQ - pszStr + 1);
    if (!pszUnquoted)
        return VERR_NO_MEMORY;
    memcpy(pszUnquoted, pszStr, pszQ - pszStr);
    pszUnquoted[pszQ - pszStr] = '\0';
    *ppszUnquoted = pszUnquoted;
    if (ppszNext)
        *ppszNext = pszQ + 1;
    return VINF_SUCCESS;
}

static int vmdkDescInitStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                           const char *pszLine)
{
    char *pEnd = pDescriptor->aLines[pDescriptor->cLines];
    ssize_t cbDiff = strlen(pszLine) + 1;

    if (    pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1
        &&  pEnd - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff)
        return vmdkError(pImage, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

    memcpy(pEnd, pszLine, cbDiff);
    pDescriptor->cLines++;
    pDescriptor->aLines[pDescriptor->cLines] = pEnd + cbDiff;
    pDescriptor->fDirty = true;

    return VINF_SUCCESS;
}

static bool vmdkDescGetStr(PVMDKDESCRIPTOR pDescriptor, unsigned uStart,
                           const char *pszKey, const char **ppszValue)
{
    size_t cbKey = strlen(pszKey);
    const char *pszValue;

    while (uStart != 0)
    {
        if (!strncmp(pDescriptor->aLines[uStart], pszKey, cbKey))
        {
            /* Key matches, check for a '=' (preceded by whitespace). */
            pszValue = pDescriptor->aLines[uStart] + cbKey;
            while (*pszValue == ' ' || *pszValue == '\t')
                pszValue++;
            if (*pszValue == '=')
            {
                *ppszValue = pszValue + 1;
                break;
            }
        }
        uStart = pDescriptor->aNextLines[uStart];
    }
    return !!uStart;
}

static int vmdkDescSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                          unsigned uStart,
                          const char *pszKey, const char *pszValue)
{
    char *pszTmp;
    size_t cbKey = strlen(pszKey);
    unsigned uLast = 0;

    while (uStart != 0)
    {
        if (!strncmp(pDescriptor->aLines[uStart], pszKey, cbKey))
        {
            /* Key matches, check for a '=' (preceded by whitespace). */
            pszTmp = pDescriptor->aLines[uStart] + cbKey;
            while (*pszTmp == ' ' || *pszTmp == '\t')
                pszTmp++;
            if (*pszTmp == '=')
            {
                pszTmp++;
                while (*pszTmp == ' ' || *pszTmp == '\t')
                    pszTmp++;
                break;
            }
        }
        if (!pDescriptor->aNextLines[uStart])
            uLast = uStart;
        uStart = pDescriptor->aNextLines[uStart];
    }
    if (uStart)
    {
        if (pszValue)
        {
            /* Key already exists, replace existing value. */
            size_t cbOldVal = strlen(pszTmp);
            size_t cbNewVal = strlen(pszValue);
            ssize_t cbDiff = cbNewVal - cbOldVal;
            /* Check for buffer overflow. */
            if (    pDescriptor->aLines[pDescriptor->cLines]
                -   pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff)
                return vmdkError(pImage, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

            memmove(pszTmp + cbNewVal, pszTmp + cbOldVal,
                    pDescriptor->aLines[pDescriptor->cLines] - pszTmp - cbOldVal);
            memcpy(pszTmp, pszValue, cbNewVal + 1);
            for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
                pDescriptor->aLines[i] += cbDiff;
        }
        else
        {
            memmove(pDescriptor->aLines[uStart], pDescriptor->aLines[uStart+1],
                    pDescriptor->aLines[pDescriptor->cLines] - pDescriptor->aLines[uStart+1] + 1);
            for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
            {
                pDescriptor->aLines[i-1] = pDescriptor->aLines[i];
                if (pDescriptor->aNextLines[i])
                    pDescriptor->aNextLines[i-1] = pDescriptor->aNextLines[i] - 1;
                else
                    pDescriptor->aNextLines[i-1] = 0;
            }
            pDescriptor->cLines--;
            /* Adjust starting line numbers of following descriptor sections. */
            if (uStart < pDescriptor->uFirstExtent)
                pDescriptor->uFirstExtent--;
            if (uStart < pDescriptor->uFirstDDB)
                pDescriptor->uFirstDDB--;
        }
    }
    else
    {
        /* Key doesn't exist, append after the last entry in this category. */
        if (!pszValue)
        {
            /* Key doesn't exist, and it should be removed. Simply a no-op. */
            return VINF_SUCCESS;
        }
        size_t cbKey = strlen(pszKey);
        size_t cbValue = strlen(pszValue);
        ssize_t cbDiff = cbKey + 1 + cbValue + 1;
        /* Check for buffer overflow. */
        if (   (pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1)
            || (  pDescriptor->aLines[pDescriptor->cLines]
                - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff))
            return vmdkError(pImage, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);
        for (unsigned i = pDescriptor->cLines + 1; i > uLast + 1; i--)
        {
            pDescriptor->aLines[i] = pDescriptor->aLines[i - 1];
            if (pDescriptor->aNextLines[i - 1])
                pDescriptor->aNextLines[i] = pDescriptor->aNextLines[i - 1] + 1;
            else
                pDescriptor->aNextLines[i] = 0;
        }
        uStart = uLast + 1;
        pDescriptor->aNextLines[uLast] = uStart;
        pDescriptor->aNextLines[uStart] = 0;
        pDescriptor->cLines++;
        pszTmp = pDescriptor->aLines[uStart];
        memmove(pszTmp + cbDiff, pszTmp,
                pDescriptor->aLines[pDescriptor->cLines] - pszTmp);
        memcpy(pDescriptor->aLines[uStart], pszKey, cbKey);
        pDescriptor->aLines[uStart][cbKey] = '=';
        memcpy(pDescriptor->aLines[uStart] + cbKey + 1, pszValue, cbValue + 1);
        for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
            pDescriptor->aLines[i] += cbDiff;

        /* Adjust starting line numbers of following descriptor sections. */
        if (uStart <= pDescriptor->uFirstExtent)
            pDescriptor->uFirstExtent++;
        if (uStart <= pDescriptor->uFirstDDB)
            pDescriptor->uFirstDDB++;
    }
    pDescriptor->fDirty = true;
    return VINF_SUCCESS;
}

static int vmdkDescBaseGetU32(PVMDKDESCRIPTOR pDescriptor, const char *pszKey,
                              uint32_t *puValue)
{
    const char *pszValue;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDesc, pszKey,
                        &pszValue))
        return VERR_VDI_VALUE_NOT_FOUND;
    return RTStrToUInt32Ex(pszValue, NULL, 10, puValue);
}

static int vmdkDescBaseGetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, const char **ppszValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDesc, pszKey,
                        &pszValue))
        return VERR_VDI_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (VBOX_FAILURE(rc))
        return rc;
    *ppszValue = pszValueUnquoted;
    return rc;
}

static int vmdkDescBaseSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, const char *pszValue)
{
    char *pszValueQuoted;

    int rc = RTStrAPrintf(&pszValueQuoted, "\"%s\"", pszValue);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc, pszKey,
                        pszValueQuoted);
    RTStrFree(pszValueQuoted);
    return rc;
}

static void vmdkDescExtRemoveDummy(PVMDKIMAGE pImage,
                                   PVMDKDESCRIPTOR pDescriptor)
{
    unsigned uEntry = pDescriptor->uFirstExtent;
    ssize_t cbDiff;

    if (!uEntry)
        return;

    cbDiff = strlen(pDescriptor->aLines[uEntry]) + 1;
    /* Move everything including \0 in the entry marking the end of buffer. */
    memmove(pDescriptor->aLines[uEntry], pDescriptor->aLines[uEntry + 1],
            pDescriptor->aLines[pDescriptor->cLines] - pDescriptor->aLines[uEntry + 1] + 1);
    for (unsigned i = uEntry + 1; i <= pDescriptor->cLines; i++)
    {
        pDescriptor->aLines[i - 1] = pDescriptor->aLines[i] - cbDiff;
        if (pDescriptor->aNextLines[i])
            pDescriptor->aNextLines[i - 1] = pDescriptor->aNextLines[i] - 1;
        else
            pDescriptor->aNextLines[i - 1] = 0;
    }
    pDescriptor->cLines--;
    if (pDescriptor->uFirstDDB)
        pDescriptor->uFirstDDB--;

    return;
}

static int vmdkDescExtInsert(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             VMDKACCESS enmAccess, uint64_t cNominalSectors,
                             VMDKETYPE enmType, const char *pszBasename,
                             uint64_t uSectorOffset)
{
    static const char *apszAccess[] = { "NOACCESS", "RDONLY", "RW" };
    static const char *apszType[] = { "", "SPARSE", "FLAT", "ZERO" };
    char *pszTmp;
    unsigned uStart = pDescriptor->uFirstExtent, uLast = 0;
    char szExt[1024];
    ssize_t cbDiff;

    /* Find last entry in extent description. */
    while (uStart)
    {
        if (!pDescriptor->aNextLines[uStart])
            uLast = uStart;
        uStart = pDescriptor->aNextLines[uStart];
    }

    if (enmType == VMDKETYPE_ZERO)
    {
        RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s ", apszAccess[enmAccess],
                    cNominalSectors, apszType[enmType]);
    }
    else
    {
        if (!uSectorOffset)
            RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s \"%s\"",
                        apszAccess[enmAccess], cNominalSectors,
                        apszType[enmType], pszBasename);
        else
            RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s \"%s\" %llu",
                        apszAccess[enmAccess], cNominalSectors,
                        apszType[enmType], pszBasename, uSectorOffset);
    }
    cbDiff = strlen(szExt) + 1;

    /* Check for buffer overflow. */
    if (   (pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1)
        || (  pDescriptor->aLines[pDescriptor->cLines]
            - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff))
        return vmdkError(pImage, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

    for (unsigned i = pDescriptor->cLines + 1; i > uLast + 1; i--)
    {
        pDescriptor->aLines[i] = pDescriptor->aLines[i - 1];
        if (pDescriptor->aNextLines[i - 1])
            pDescriptor->aNextLines[i] = pDescriptor->aNextLines[i - 1] + 1;
        else
            pDescriptor->aNextLines[i] = 0;
    }
    uStart = uLast + 1;
    pDescriptor->aNextLines[uLast] = uStart;
    pDescriptor->aNextLines[uStart] = 0;
    pDescriptor->cLines++;
    pszTmp = pDescriptor->aLines[uStart];
    memmove(pszTmp + cbDiff, pszTmp,
            pDescriptor->aLines[pDescriptor->cLines] - pszTmp);
    memcpy(pDescriptor->aLines[uStart], szExt, cbDiff);
    for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
        pDescriptor->aLines[i] += cbDiff;

    /* Adjust starting line numbers of following descriptor sections. */
    if (uStart <= pDescriptor->uFirstDDB)
        pDescriptor->uFirstDDB++;

    pDescriptor->fDirty = true;
    return VINF_SUCCESS;
}

static int vmdkDescDDBGetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, const char **ppszValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VDI_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (VBOX_FAILURE(rc))
        return rc;
    *ppszValue = pszValueUnquoted;
    return rc;
}

static int vmdkDescDDBGetU32(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, uint32_t *puValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VDI_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = RTStrToUInt32Ex(pszValueUnquoted, NULL, 10, puValue);
    RTMemTmpFree(pszValueUnquoted);
    return rc;
}

static int vmdkDescDDBGetUuid(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, PRTUUID pUuid)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VDI_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = RTUuidFromStr(pUuid, pszValueUnquoted);
    RTMemTmpFree(pszValueUnquoted);
    return rc;
}

static int vmdkDescDDBSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, const char *pszVal)
{
    int rc;
    char *pszValQuoted;

    if (pszVal)
    {
        rc = RTStrAPrintf(&pszValQuoted, "\"%s\"", pszVal);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    else
        pszValQuoted = NULL;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        pszValQuoted);
    if (pszValQuoted)
        RTStrFree(pszValQuoted);
    return rc;
}

static int vmdkDescDDBSetUuid(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, PCRTUUID pUuid)
{
    char *pszUuid;

    int rc = RTStrAPrintf(&pszUuid, "\"%Vuuid\"", pUuid);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        pszUuid);
    RTStrFree(pszUuid);
    return rc;
}

static int vmdkDescDDBSetU32(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, uint32_t uValue)
{
    char *pszValue;

    int rc = RTStrAPrintf(&pszValue, "\"%d\"", uValue);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        pszValue);
    RTStrFree(pszValue);
    return rc;
}

static int vmdkPreprocessDescriptor(PVMDKIMAGE pImage, char *pDescData,
                                    size_t cbDescData,
                                    PVMDKDESCRIPTOR pDescriptor)
{
    int rc = VINF_SUCCESS;
    unsigned cLine = 0, uLastNonEmptyLine = 0;
    char *pTmp = pDescData;

    pDescriptor->cbDescAlloc = cbDescData;
    while (*pTmp != '\0')
    {
        pDescriptor->aLines[cLine++] = pTmp;
        if (cLine >= VMDK_DESCRIPTOR_LINES_MAX)
        {
            rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);
            goto out;
        }

        while (*pTmp != '\0' && *pTmp != '\n')
        {
            if (*pTmp == '\r')
            {
                if (*(pTmp + 1) != '\n')
                {
                    rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: unsupported end of line in descriptor in '%s'"), pImage->pszFilename);
                    goto out;
                }
                else
                {
                    /* Get rid of CR character. */
                    *pTmp = '\0';
                }
            }
            pTmp++;
        }
        /* Get rid of LF character. */
        if (*pTmp == '\n')
        {
            *pTmp = '\0';
            pTmp++;
        }
    }
    pDescriptor->cLines = cLine;
    /* Pointer right after the end of the used part of the buffer. */
    pDescriptor->aLines[cLine] = pTmp;

    if (strcmp(pDescriptor->aLines[0], "# Disk DescriptorFile"))
    {
        rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: descriptor does not start as expected in '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Initialize those, because we need to be able to reopen an image. */
    pDescriptor->uFirstDesc = 0;
    pDescriptor->uFirstExtent = 0;
    pDescriptor->uFirstDDB = 0;
    for (unsigned i = 0; i < cLine; i++)
    {
        if (*pDescriptor->aLines[i] != '#' && *pDescriptor->aLines[i] != '\0')
        {
            if (    !strncmp(pDescriptor->aLines[i], "RW", 2)
                ||  !strncmp(pDescriptor->aLines[i], "RDONLY", 6)
                ||  !strncmp(pDescriptor->aLines[i], "NOACCESS", 8) )
            {
                /* An extent descriptor. */
                if (!pDescriptor->uFirstDesc || pDescriptor->uFirstDDB)
                {
                    /* Incorrect ordering of entries. */
                    rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                    goto out;
                }
                if (!pDescriptor->uFirstExtent)
                {
                    pDescriptor->uFirstExtent = i;
                    uLastNonEmptyLine = 0;
                }
            }
            else if (!strncmp(pDescriptor->aLines[i], "ddb.", 4))
            {
                /* A disk database entry. */
                if (!pDescriptor->uFirstDesc || !pDescriptor->uFirstExtent)
                {
                    /* Incorrect ordering of entries. */
                    rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                    goto out;
                }
                if (!pDescriptor->uFirstDDB)
                {
                    pDescriptor->uFirstDDB = i;
                    uLastNonEmptyLine = 0;
                }
            }
            else
            {
                /* A normal entry. */
                if (pDescriptor->uFirstExtent || pDescriptor->uFirstDDB)
                {
                    /* Incorrect ordering of entries. */
                    rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                    goto out;
                }
                if (!pDescriptor->uFirstDesc)
                {
                    pDescriptor->uFirstDesc = i;
                    uLastNonEmptyLine = 0;
                }
            }
            if (uLastNonEmptyLine)
                pDescriptor->aNextLines[uLastNonEmptyLine] = i;
            uLastNonEmptyLine = i;
        }
    }

out:
    return rc;
}

static int vmdkDescSetPCHSGeometry(PVMDKIMAGE pImage,
                                   PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    int rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_CYLINDERS,
                           pPCHSGeometry->cCylinders);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_HEADS,
                           pPCHSGeometry->cHeads);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_SECTORS,
                           pPCHSGeometry->cSectors);
    return rc;
}

static int vmdkDescSetLCHSGeometry(PVMDKIMAGE pImage,
                                   PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    int rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_CYLINDERS,
                           pLCHSGeometry->cCylinders);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_HEADS,
                           pLCHSGeometry->cHeads);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_SECTORS,
                           pLCHSGeometry->cSectors);
    return rc;
}

static int vmdkCreateDescriptor(PVMDKIMAGE pImage, char *pDescData,
                                size_t cbDescData, PVMDKDESCRIPTOR pDescriptor)
{
    int rc;

    pDescriptor->uFirstDesc = 0;
    pDescriptor->uFirstExtent = 0;
    pDescriptor->uFirstDDB = 0;
    pDescriptor->cLines = 0;
    pDescriptor->cbDescAlloc = cbDescData;
    pDescriptor->fDirty = false;
    pDescriptor->aLines[pDescriptor->cLines] = pDescData;
    memset(pDescriptor->aNextLines, '\0', sizeof(pDescriptor->aNextLines));

    rc = vmdkDescInitStr(pImage, pDescriptor, "# Disk DescriptorFile");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "version=1");
    if (VBOX_FAILURE(rc))
        goto out;
    pDescriptor->uFirstDesc = pDescriptor->cLines - 1;
    rc = vmdkDescInitStr(pImage, pDescriptor, "");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "# Extent description");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "NOACCESS 0 ZERO ");
    if (VBOX_FAILURE(rc))
        goto out;
    pDescriptor->uFirstExtent = pDescriptor->cLines - 1;
    rc = vmdkDescInitStr(pImage, pDescriptor, "");
    if (VBOX_FAILURE(rc))
        goto out;
    /* The trailing space is created by VMware, too. */
    rc = vmdkDescInitStr(pImage, pDescriptor, "# The disk Data Base ");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "#DDB");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "");
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescInitStr(pImage, pDescriptor, "ddb.virtualHWVersion = \"4\"");
    if (VBOX_FAILURE(rc))
        goto out;
    pDescriptor->uFirstDDB = pDescriptor->cLines - 1;

    /* Now that the framework is in place, use the normal functions to insert
     * the remaining keys. */
    char szBuf[9];
    RTStrPrintf(szBuf, sizeof(szBuf), "%08x", RTRandU32());
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc,
                        "CID", szBuf);
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc,
                        "parentCID", "ffffffff");
    if (VBOX_FAILURE(rc))
        goto out;

    rc = vmdkDescDDBSetStr(pImage, pDescriptor, "ddb.adapterType", "ide");
    if (VBOX_FAILURE(rc))
        goto out;

out:
    return rc;
}

static int vmdkParseDescriptor(PVMDKIMAGE pImage, char *pDescData,
                               size_t cbDescData)
{
    int rc;
    unsigned cExtents;
    unsigned uLine;

    rc = vmdkPreprocessDescriptor(pImage, pDescData, cbDescData,
                                  &pImage->Descriptor);
    if (VBOX_FAILURE(rc))
        return rc;

    /* Check version, must be 1. */
    uint32_t uVersion;
    rc = vmdkDescBaseGetU32(&pImage->Descriptor, "version", &uVersion);
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error finding key 'version' in descriptor in '%s'"), pImage->pszFilename);
    if (uVersion != 1)
        return vmdkError(pImage, VERR_VDI_UNSUPPORTED_VERSION, RT_SRC_POS, N_("VMDK: unsupported format version in descriptor in '%s'"), pImage->pszFilename);

    /* Get image creation type and determine image flags. */
    const char *pszCreateType;
    rc = vmdkDescBaseGetStr(pImage, &pImage->Descriptor, "createType",
                            &pszCreateType);
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: cannot get image type from descriptor in '%s'"), pImage->pszFilename);
    if (    !strcmp(pszCreateType, "twoGbMaxExtentSparse")
        ||  !strcmp(pszCreateType, "twoGbMaxExtentFlat"))
        pImage->uImageFlags = VD_VMDK_IMAGE_FLAGS_SPLIT_2G;
    if (    !strcmp(pszCreateType, "partitionedDevice")
        ||  !strcmp(pszCreateType, "fullDevice"))
        pImage->uImageFlags = VD_VMDK_IMAGE_FLAGS_RAWDISK;
    else
        pImage->uImageFlags = 0;
    RTStrFree((char *)(void *)pszCreateType);

    /* Count the number of extent config entries. */
    for (uLine = pImage->Descriptor.uFirstExtent, cExtents = 0;
         uLine != 0;
         uLine = pImage->Descriptor.aNextLines[uLine], cExtents++)
        /* nothing */;

    if (!pImage->pDescData && cExtents != 1)
    {
        /* Monolithic image, must have only one extent (already opened). */
        return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: monolithic image may only have one extent in '%s'"), pImage->pszFilename);
    }

    if (pImage->pDescData)
    {
        /* Non-monolithic image, extents need to be allocated. */
        rc = vmdkCreateExtents(pImage, cExtents);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    for (unsigned i = 0, uLine = pImage->Descriptor.uFirstExtent;
         i < cExtents; i++, uLine = pImage->Descriptor.aNextLines[uLine])
    {
        char *pszLine = pImage->Descriptor.aLines[uLine];

        /* Access type of the extent. */
        if (!strncmp(pszLine, "RW", 2))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_READWRITE;
            pszLine += 2;
        }
        else if (!strncmp(pszLine, "RDONLY", 6))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_READONLY;
            pszLine += 6;
        }
        else if (!strncmp(pszLine, "NOACCESS", 8))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_NOACCESS;
            pszLine += 8;
        }
        else
            return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        if (*pszLine++ != ' ')
            return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

        /* Nominal size of the extent. */
        rc = RTStrToUInt64Ex(pszLine, &pszLine, 10,
                             &pImage->pExtents[i].cNominalSectors);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        if (*pszLine++ != ' ')
            return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

        /* Type of the extent. */
#ifdef VBOX_WITH_VMDK_ESX
        /** @todo Add the ESX extent types. Not necessary for now because
         * the ESX extent types are only used inside an ESX server. They are
         * automatically converted if the VMDK is exported. */
#endif /* VBOX_WITH_VMDK_ESX */
        if (!strncmp(pszLine, "SPARSE", 6))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_HOSTED_SPARSE;
            pszLine += 6;
        }
        else if (!strncmp(pszLine, "FLAT", 4))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_FLAT;
            pszLine += 4;
        }
        else if (!strncmp(pszLine, "ZERO", 4))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_ZERO;
            pszLine += 4;
        }
        else
            return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        if (pImage->pExtents[i].enmType == VMDKETYPE_ZERO)
        {
            /* This one has no basename or offset. */
            if (*pszLine == ' ')
                pszLine++;
            if (*pszLine != '\0')
                return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
            pImage->pExtents[i].pszBasename = NULL;
        }
        else
        {
            /* All other extent types have basename and optional offset. */
            if (*pszLine++ != ' ')
                return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

            /* Basename of the image. Surrounded by quotes. */
            char *pszBasename;
            rc = vmdkStringUnquote(pImage, pszLine, &pszBasename, &pszLine);
            if (VBOX_FAILURE(rc))
                return rc;
            pImage->pExtents[i].pszBasename = pszBasename;
            if (*pszLine == ' ')
            {
                pszLine++;
                if (*pszLine != '\0')
                {
                    /* Optional offset in extent specified. */
                    rc = RTStrToUInt64Ex(pszLine, &pszLine, 10,
                                         &pImage->pExtents[i].uSectorOffset);
                    if (VBOX_FAILURE(rc))
                        return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
                }
            }

            if (*pszLine != '\0')
                return vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        }
    }

    /* Determine PCHS geometry (autogenerate if necessary). */
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_CYLINDERS,
                           &pImage->PCHSGeometry.cCylinders);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cCylinders = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_HEADS,
                           &pImage->PCHSGeometry.cHeads);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cHeads = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_SECTORS,
                           &pImage->PCHSGeometry.cSectors);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cSectors = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    if (    pImage->PCHSGeometry.cCylinders == 0
        ||  pImage->PCHSGeometry.cHeads == 0
        ||  pImage->PCHSGeometry.cHeads > 16
        ||  pImage->PCHSGeometry.cSectors == 0
        ||  pImage->PCHSGeometry.cSectors > 63)
    {
        /* Mark PCHS geometry as not yet valid (can't do the calculation here
         * as the total image size isn't known yet). */
        pImage->PCHSGeometry.cCylinders = 0;
        pImage->PCHSGeometry.cHeads = 16;
        pImage->PCHSGeometry.cSectors = 63;
    }

    /* Determine LCHS geometry (set to 0 if not specified). */
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_CYLINDERS,
                           &pImage->LCHSGeometry.cCylinders);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cCylinders = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_HEADS,
                           &pImage->LCHSGeometry.cHeads);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cHeads = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_SECTORS,
                           &pImage->LCHSGeometry.cSectors);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cSectors = 0;
    else if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    if (    pImage->LCHSGeometry.cCylinders == 0
        ||  pImage->LCHSGeometry.cHeads == 0
        ||  pImage->LCHSGeometry.cSectors == 0)
    {
        pImage->LCHSGeometry.cCylinders = 0;
        pImage->LCHSGeometry.cHeads = 0;
        pImage->LCHSGeometry.cSectors = 0;
    }

    /* Get image UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor, VMDK_DDB_IMAGE_UUID,
                            &pImage->ImageUuid);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ImageUuid);
        else
        {
            rc = RTUuidCreate(&pImage->ImageUuid);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_IMAGE_UUID, &pImage->ImageUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing image UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (VBOX_FAILURE(rc))
        return rc;

    /* Get image modification UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_MODIFICATION_UUID,
                            &pImage->ModificationUuid);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ModificationUuid);
        else
        {
            rc = RTUuidCreate(&pImage->ModificationUuid);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_MODIFICATION_UUID,
                                    &pImage->ModificationUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing image modification UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (VBOX_FAILURE(rc))
        return rc;

    /* Get UUID of parent image. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor, VMDK_DDB_PARENT_UUID,
                            &pImage->ParentUuid);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ParentUuid);
        else
        {
            rc = RTUuidClear(&pImage->ParentUuid);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_UUID, &pImage->ParentUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (VBOX_FAILURE(rc))
        return rc;

    /* Get parent image modification UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_PARENT_MODIFICATION_UUID,
                            &pImage->ParentModificationUuid);
    if (rc == VERR_VDI_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ParentModificationUuid);
        else
        {
            rc = RTUuidCreate(&pImage->ParentModificationUuid);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_MODIFICATION_UUID,
                                    &pImage->ParentModificationUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent modification UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

/**
 * Internal: write/update the descriptor part of the image.
 */
static int vmdkWriteDescriptor(PVMDKIMAGE pImage)
{
    int rc = VINF_SUCCESS;
    uint64_t cbLimit;
    uint64_t uOffset;
    RTFILE DescFile;

    if (pImage->pDescData)
    {
        /* Separate descriptor file. */
        uOffset = 0;
        cbLimit = 0;
        DescFile = pImage->File;
    }
    else
    {
        /* Embedded descriptor file. */
        uOffset = VMDK_SECTOR2BYTE(pImage->pExtents[0].uDescriptorSector);
        cbLimit = VMDK_SECTOR2BYTE(pImage->pExtents[0].cDescriptorSectors);
        cbLimit += uOffset;
        DescFile = pImage->pExtents[0].File;
    }
    for (unsigned i = 0; i < pImage->Descriptor.cLines; i++)
    {
        const char *psz = pImage->Descriptor.aLines[i];
        size_t cb = strlen(psz);

        if (cbLimit && uOffset + cb + 1 > cbLimit)
            return vmdkError(pImage, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too long in '%s'"), pImage->pszFilename);
        rc = RTFileWriteAt(DescFile, uOffset, psz, cb, NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error writing descriptor in '%s'"), pImage->pszFilename);
        uOffset += cb;
        rc = RTFileWriteAt(DescFile, uOffset, "\n", 1, NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error writing descriptor in '%s'"), pImage->pszFilename);
        uOffset++;
    }
    if (cbLimit)
    {
        /* Inefficient, but simple. */
        while (uOffset < cbLimit)
        {
            rc = RTFileWriteAt(DescFile, uOffset, "", 1, NULL);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error writing descriptor in '%s'"), pImage->pszFilename);
            uOffset++;
        }
    }
    else
    {
        rc = RTFileSetSize(DescFile, uOffset);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error truncating descriptor in '%s'"), pImage->pszFilename);
    }
    pImage->Descriptor.fDirty = false;
    return rc;
}

/**
 * Internal: read metadata belonging to a sparse extent.
 */
static int vmdkReadMetaSparseExtent(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    SparseExtentHeader Header;
    uint64_t cbExtentSize, cSectorsPerGDE;

    int rc = RTFileReadAt(pExtent->File, 0, &Header, sizeof(Header), NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error reading extent header in '%s'"), pExtent->pszFullname);
        goto out;
    }
    if (    RT_LE2H_U32(Header.magicNumber) != VMDK_SPARSE_MAGICNUMBER
        ||  RT_LE2H_U32(Header.version) != 1)
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect magic/version in extent header in '%s'"), pExtent->pszFullname);
        goto out;
    }
    /* The image must be a multiple of a sector in size. If not, it means the
     * image is at least truncated, or even seriously garbled. */
    rc = RTFileGetSize(pExtent->File, &cbExtentSize);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error getting size in '%s'"), pExtent->pszFullname);
        goto out;
    }
    if (    (RT_LE2H_U32(Header.flags) & 1)
        &&  (   Header.singleEndLineChar != '\n'
             || Header.nonEndLineChar != ' '
             || Header.doubleEndLineChar1 != '\r'
             || Header.doubleEndLineChar2 != '\n') )
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: corrupted by CR/LF translation in '%s'"), pExtent->pszFullname);
        goto out;
    }
    pExtent->enmType = VMDKETYPE_HOSTED_SPARSE;
    pExtent->cSectors = RT_LE2H_U64(Header.capacity);
    pExtent->cSectorsPerGrain = RT_LE2H_U64(Header.grainSize);
    /* The spec says that this must be a power of two and greater than 8,
     * but probably they meant not less than 8. */
    if (    (pExtent->cSectorsPerGrain & (pExtent->cSectorsPerGrain - 1))
        ||  pExtent->cSectorsPerGrain < 8)
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: invalid extent grain size %u in '%s'"), pExtent->cSectorsPerGrain, pExtent->pszFullname);
        goto out;
    }
    pExtent->uDescriptorSector = RT_LE2H_U64(Header.descriptorOffset);
    pExtent->cDescriptorSectors = RT_LE2H_U64(Header.descriptorSize);
    if (pExtent->uDescriptorSector && !pExtent->cDescriptorSectors)
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: inconsistent embedded descriptor config in '%s'"), pExtent->pszFullname);
        goto out;
    }
    pExtent->cGTEntries = RT_LE2H_U32(Header.numGTEsPerGT);
    /* This code requires that a grain table must hold a power of two multiple
     * of the number of entries per GT cache entry. */
    if (    (pExtent->cGTEntries & (pExtent->cGTEntries - 1))
        ||  pExtent->cGTEntries < VMDK_GT_CACHELINE_SIZE)
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: grain table cache size problem in '%s'"), pExtent->pszFullname);
        goto out;
    }
    if (RT_LE2H_U32(Header.flags) & 2)
    {
        pExtent->uSectorRGD = RT_LE2H_U64(Header.rgdOffset);
        pExtent->uSectorGD = RT_LE2H_U64(Header.gdOffset);
    }
    else
    {
        /** @todo this is just guesswork, the spec doesn't document this
         * properly and I don't have a vmdk without RGD. */
        pExtent->uSectorGD = RT_LE2H_U64(Header.rgdOffset);
        pExtent->uSectorRGD = 0;
    }
    pExtent->cOverheadSectors = RT_LE2H_U64(Header.overHead);
    pExtent->fUncleanShutdown = !!Header.uncleanShutdown;
    cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
    if (!cSectorsPerGDE || cSectorsPerGDE > UINT32_MAX)
    {
        rc = vmdkError(pExtent->pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect grain directory size in '%s'"), pExtent->pszFullname);
        goto out;
    }
    pExtent->cSectorsPerGDE = cSectorsPerGDE;
    pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;

    rc = vmdkReadGrainDirectory(pExtent);

out:
    if (VBOX_FAILURE(rc))
        vmdkFreeExtentData(pImage, pExtent, false);

    return rc;
}

/**
 * Internal: write/update the metadata for a sparse extent.
 */
static int vmdkWriteMetaSparseExtent(PVMDKEXTENT pExtent)
{
    SparseExtentHeader Header;

    memset(&Header, '\0', sizeof(Header));
    Header.magicNumber = RT_H2LE_U32(VMDK_SPARSE_MAGICNUMBER);
    Header.version = RT_H2LE_U32(1);
    Header.flags = RT_H2LE_U32(1 | ((pExtent->pRGD) ? 2 : 0));
    Header.capacity = RT_H2LE_U64(pExtent->cSectors);
    Header.grainSize = RT_H2LE_U64(pExtent->cSectorsPerGrain);
    Header.descriptorOffset = RT_H2LE_U64(pExtent->uDescriptorSector);
    Header.descriptorSize = RT_H2LE_U64(pExtent->cDescriptorSectors);
    Header.numGTEsPerGT = RT_H2LE_U32(pExtent->cGTEntries);
    if (pExtent->pRGD)
    {
        Header.rgdOffset = RT_H2LE_U64(pExtent->uSectorRGD);
        Header.gdOffset = RT_H2LE_U64(pExtent->uSectorGD);
    }
    else
    {
        /** @todo this is just guesswork, the spec doesn't document this
         * properly and I don't have a vmdk without RGD. */
        Header.rgdOffset = RT_H2LE_U64(pExtent->uSectorGD);
    }
    Header.overHead = RT_H2LE_U64(pExtent->cOverheadSectors);
    Header.uncleanShutdown = pExtent->fUncleanShutdown;
    Header.singleEndLineChar = '\n';
    Header.nonEndLineChar = ' ';
    Header.doubleEndLineChar1 = '\r';
    Header.doubleEndLineChar2 = '\n';

    int rc = RTFileWriteAt(pExtent->File, 0, &Header, sizeof(Header), NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        rc = vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error writing extent header in '%s'"), pExtent->pszFullname);
    return rc;
}

#ifdef VBOX_WITH_VMDK_ESX
/**
 * Internal: unused code to read the metadata of a sparse ESX extent.
 *
 * Such extents never leave ESX server, so this isn't ever used.
 */
static int vmdkReadMetaESXSparseExtent(PVMDKEXTENT pExtent)
{
    COWDisk_Header Header;
    uint64_t cSectorsPerGDE;

    int rc = RTFileReadAt(pExtent->File, 0, &Header, sizeof(Header), NULL);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        goto out;
    if (    RT_LE2H_U32(Header.magicNumber) != VMDK_ESX_SPARSE_MAGICNUMBER
        ||  RT_LE2H_U32(Header.version) != 1
        ||  RT_LE2H_U32(Header.flags) != 3)
    {
        rc = VERR_VDI_INVALID_HEADER;
        goto out;
    }
    pExtent->enmType = VMDKETYPE_ESX_SPARSE;
    pExtent->cSectors = RT_LE2H_U32(Header.numSectors);
    pExtent->cSectorsPerGrain = RT_LE2H_U32(Header.grainSize);
    /* The spec says that this must be between 1 sector and 1MB. This code
     * assumes it's a power of two, so check that requirement, too. */
    if (    (pExtent->cSectorsPerGrain & (pExtent->cSectorsPerGrain - 1))
        ||  pExtent->cSectorsPerGrain == 0
        ||  pExtent->cSectorsPerGrain > 2048)
    {
        rc = VERR_VDI_INVALID_HEADER;
        goto out;
    }
    pExtent->uDescriptorSector = 0;
    pExtent->cDescriptorSectors = 0;
    pExtent->uSectorGD = RT_LE2H_U32(Header.gdOffset);
    pExtent->uSectorRGD = 0;
    pExtent->cOverheadSectors = 0;
    pExtent->cGTEntries = 4096;
    cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
    if (!cSectorsPerGDE || cSectorsPerGDE > UINT32_MAX)
    {
        rc = VERR_VDI_INVALID_HEADER;
        goto out;
    }
    pExtent->cSectorsPerGDE = cSectorsPerGDE;
    pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;
    if (pExtent->cGDEntries != RT_LE2H_U32(Header.numGDEntries))
    {
        /* Inconsistency detected. Computed number of GD entries doesn't match
         * stored value. Better be safe than sorry. */
        rc = VERR_VDI_INVALID_HEADER;
        goto out;
    }
    pExtent->uFreeSector = RT_LE2H_U32(Header.freeSector);
    pExtent->fUncleanShutdown = !!Header.uncleanShutdown;

    rc = vmdkReadGrainDirectory(pExtent);

out:
    if (VBOX_FAILURE(rc))
        vmdkFreeExtentData(pImage, pExtent, false);

    return rc;
}
#endif /* VBOX_WITH_VMDK_ESX */

/**
 * Internal: free the memory used by the extent data structure, optionally
 * deleting the referenced files.
 */
static void vmdkFreeExtentData(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                               bool fDelete)
{
    vmdkFreeGrainDirectory(pExtent);
    if (pExtent->pDescData)
    {
        RTMemFree(pExtent->pDescData);
        pExtent->pDescData = NULL;
    }
    if (pExtent->File != NIL_RTFILE)
    {
        vmdkFileClose(pImage, &pExtent->File,
                         fDelete
                      && pExtent->pszFullname
                      && strcmp(pExtent->pszFullname, pExtent->pszBasename));
    }
    if (pExtent->pszBasename)
    {
        RTMemTmpFree((void *)pExtent->pszBasename);
        pExtent->pszBasename = NULL;
    }
    if (pExtent->pszFullname)
    {
        RTStrFree((char *)(void *)pExtent->pszFullname);
        pExtent->pszFullname = NULL;
    }
}

/**
 * Internal: allocate grain table cache if necessary for this image.
 */
static int vmdkAllocateGrainTableCache(PVMDKIMAGE pImage)
{
    PVMDKEXTENT pExtent;

    /* Allocate grain table cache if any sparse extent is present. */
    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        pExtent = &pImage->pExtents[i];
        if (    pExtent->enmType == VMDKETYPE_HOSTED_SPARSE
#ifdef VBOX_WITH_VMDK_ESX
            ||  pExtent->enmType == VMDKETYPE_ESX_SPARSE
#endif /* VBOX_WITH_VMDK_ESX */
           )
        {
            /* Allocate grain table cache. */
            pImage->pGTCache = (PVMDKGTCACHE)RTMemAllocZ(sizeof(VMDKGTCACHE));
            if (!pImage->pGTCache)
                return VERR_NO_MEMORY;
            for (unsigned i = 0; i < VMDK_GT_CACHE_SIZE; i++)
            {
                PVMDKGTCACHEENTRY pGCE = &pImage->pGTCache->aGTCache[i];
                pGCE->uExtent = UINT32_MAX;
            }
            pImage->pGTCache->cEntries = VMDK_GT_CACHE_SIZE;
            break;
        }
    }

    return VINF_SUCCESS;
}

/**
 * Internal: allocate the given number of extents.
 */
static int vmdkCreateExtents(PVMDKIMAGE pImage, unsigned cExtents)
{
    int rc = VINF_SUCCESS;
    PVMDKEXTENT pExtents = (PVMDKEXTENT)RTMemAllocZ(cExtents * sizeof(VMDKEXTENT));
    if (pImage)
    {
        for (unsigned i = 0; i < cExtents; i++)
        {
            pExtents[i].File = NIL_RTFILE;
            pExtents[i].pszBasename = NULL;
            pExtents[i].pszFullname = NULL;
            pExtents[i].pGD = NULL;
            pExtents[i].pRGD = NULL;
            pExtents[i].pDescData = NULL;
            pExtents[i].uExtent = i;
            pExtents[i].pImage = pImage;
        }
        pImage->pExtents = pExtents;
        pImage->cExtents = cExtents;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int vmdkOpenImage(PVMDKIMAGE pImage, unsigned uOpenFlags)
{
    int rc;
    uint32_t u32Magic;
    RTFILE File;
    PVMDKEXTENT pExtent;

    pImage->uOpenFlags = uOpenFlags;

    /*
     * Open the image.
     */
    rc = vmdkFileOpen(pImage, &File, pImage->pszFilename,
                      uOpenFlags & VD_OPEN_FLAGS_READONLY
                       ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                       : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }
    pImage->File = File;

    /* Read magic (if present). */
    rc = RTFileReadAt(File, 0, &u32Magic, sizeof(u32Magic), NULL);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error reading the magic number in '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Handle the file according to its magic number. */
    if (RT_LE2H_U32(u32Magic) == VMDK_SPARSE_MAGICNUMBER)
    {
        /* It's a hosted sparse single-extent image. */
        rc = vmdkCreateExtents(pImage, 1);
        if (VBOX_FAILURE(rc))
            goto out;
        /* The opened file is passed to the extent. No separate descriptor
         * file, so no need to keep anything open for the image. */
        pExtent = &pImage->pExtents[0];
        pExtent->File = File;
        pImage->File = NIL_RTFILE;
        rc = vmdkReadMetaSparseExtent(pImage, pExtent);
        if (VBOX_FAILURE(rc))
            goto out;
        /* As we're dealing with a monolithic sparse image here, there must
         * be a descriptor embedded in the image file. */
        if (!pExtent->uDescriptorSector || !pExtent->cDescriptorSectors)
        {
            rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: monolithic image without descriptor in '%s'"), pImage->pszFilename);
            goto out;
        }
        /* Read the descriptor from the extent. */
        pExtent->pDescData = (char *)RTMemAllocZ(VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors));
        if (!pExtent->pDescData)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }
        rc = RTFileReadAt(pExtent->File,
                          VMDK_SECTOR2BYTE(pExtent->uDescriptorSector),
                          pExtent->pDescData,
                          VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors), NULL);
        AssertRC(rc);
        if (VBOX_FAILURE(rc))
        {
            rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: read error for descriptor in '%s'"), pExtent->pszFullname);
            goto out;
        }

        rc = vmdkParseDescriptor(pImage, pExtent->pDescData,
                                 VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors));
        if (VBOX_FAILURE(rc))
            goto out;

        /* Mark the extent as unclean if opened in read-write mode. */
        if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pExtent->fUncleanShutdown = true;
            pExtent->fMetaDirty = true;
        }
    }
    else
    {
        pImage->cbDescAlloc = VMDK_SECTOR2BYTE(20);
        pImage->pDescData = (char *)RTMemAllocZ(pImage->cbDescAlloc);
        if (!pImage->pDescData)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }

        size_t cbRead;
        rc = RTFileReadAt(pImage->File, 0, pImage->pDescData,
                          pImage->cbDescAlloc, &cbRead);
        if (VBOX_FAILURE(rc))
        {
            rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: read error for descriptor in '%s'"), pImage->pszFilename);
            goto out;
        }
        if (cbRead == pImage->cbDescAlloc)
        {
            /* Likely the read is truncated. Better fail a bit too early
             * (normally the descriptor is much smaller than our buffer). */
            rc = vmdkError(pImage, VERR_VDI_INVALID_HEADER, RT_SRC_POS, N_("VMDK: cannot read descriptor in '%s'"), pImage->pszFilename);
            goto out;
        }

        rc = vmdkParseDescriptor(pImage, pImage->pDescData,
                                 pImage->cbDescAlloc);
        if (VBOX_FAILURE(rc))
            goto out;

        for (unsigned i = 0; i < pImage->cExtents; i++)
        {
            PVMDKEXTENT pExtent = &pImage->pExtents[i];

            if (pExtent->pszBasename)
            {
                /* Hack to figure out whether the specified name in the
                 * extent descriptor is absolute. Doesn't always work, but
                 * should be good enough for now. */
                char *pszFullname;
                /** @todo implement proper path absolute check. */
                if (pExtent->pszBasename[0] == RTPATH_SLASH)
                {
                    pszFullname = RTStrDup(pExtent->pszBasename);
                    if (!pszFullname)
                    {
                        rc = VERR_NO_MEMORY;
                        goto out;
                    }
                }
                else
                {
                    size_t cbDirname;
                    char *pszDirname = RTStrDup(pImage->pszFilename);
                    if (!pszDirname)
                    {
                        rc = VERR_NO_MEMORY;
                        goto out;
                    }
                    RTPathStripFilename(pszDirname);
                    cbDirname = strlen(pszDirname);
                    rc = RTStrAPrintf(&pszFullname, "%s%c%s", pszDirname,
                                      RTPATH_SLASH, pExtent->pszBasename);
                    RTStrFree(pszDirname);
                    if (VBOX_FAILURE(rc))
                        goto out;
                }
                pExtent->pszFullname = pszFullname;
            }
            else
                pExtent->pszFullname = NULL;

            switch (pExtent->enmType)
            {
                case VMDKETYPE_HOSTED_SPARSE:
                    rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                                      uOpenFlags & VD_OPEN_FLAGS_READONLY
                                        ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                                        : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                    if (VBOX_FAILURE(rc))
                    {
                        /* Do NOT signal an appropriate error here, as the VD
                         * layer has the choice of retrying the open if it
                         * failed. */
                        goto out;
                    }
                    rc = vmdkReadMetaSparseExtent(pImage, pExtent);
                    if (VBOX_FAILURE(rc))
                        goto out;

                    /* Mark extent as unclean if opened in read-write mode. */
                    if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
                    {
                        pExtent->fUncleanShutdown = true;
                        pExtent->fMetaDirty = true;
                    }
                    break;
                case VMDKETYPE_FLAT:
                    rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                                      uOpenFlags & VD_OPEN_FLAGS_READONLY
                                        ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                                        : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                    if (VBOX_FAILURE(rc))
                    {
                        /* Do NOT signal an appropriate error here, as the VD
                         * layer has the choice of retrying the open if it
                         * failed. */
                        goto out;
                    }
                    break;
                case VMDKETYPE_ZERO:
                    /* Nothing to do. */
                    break;
                default:
                    AssertMsgFailed(("unknown vmdk extent type %d\n", pExtent->enmType));
            }
        }
    }

    /* Make sure this is not reached accidentally with an error status. */
    AssertRC(rc);

    /* Determine PCHS geometry if not set. */
    if (pImage->PCHSGeometry.cCylinders == 0)
    {
        uint64_t cCylinders =   VMDK_BYTE2SECTOR(pImage->cbSize)
                              / pImage->PCHSGeometry.cHeads
                              / pImage->PCHSGeometry.cSectors;
        pImage->PCHSGeometry.cCylinders = (unsigned)RT_MIN(cCylinders, 16383);
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            rc = vmdkDescSetPCHSGeometry(pImage, &pImage->PCHSGeometry);
            AssertRC(rc);
        }
    }

    /* Update the image metadata now in case has changed. */
    rc = vmdkFlushImage(pImage);
    if (VBOX_FAILURE(rc))
        goto out;

    /* Figure out a few per-image constants from the extents. */
    pImage->cbSize = 0;
    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        pExtent = &pImage->pExtents[i];
        if (    pExtent->enmType == VMDKETYPE_HOSTED_SPARSE
#ifdef VBOX_WITH_VMDK_ESX
            ||  pExtent->enmType == VMDKETYPE_ESX_SPARSE
#endif /* VBOX_WITH_VMDK_ESX */
           )
        {
            /* Here used to be a check whether the nominal size of an extent
             * is a multiple of the grain size. The spec says that this is
             * always the case, but unfortunately some files out there in the
             * wild violate the spec (e.g. ReactOS 0.3.1). */
        }
        pImage->cbSize += VMDK_SECTOR2BYTE(pExtent->cNominalSectors);
    }

    pImage->enmImageType = VD_IMAGE_TYPE_NORMAL;
    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        pExtent = &pImage->pExtents[i];
        if (    pImage->pExtents[i].enmType == VMDKETYPE_FLAT
            ||  pImage->pExtents[i].enmType == VMDKETYPE_ZERO)
        {
            pImage->enmImageType = VD_IMAGE_TYPE_FIXED;
            break;
        }
    }

    rc = vmdkAllocateGrainTableCache(pImage);
    if (VBOX_FAILURE(rc))
        goto out;

out:
    if (VBOX_FAILURE(rc))
        vmdkFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: create VMDK images for raw disk/partition access.
 */
static int vmdkCreateRawImage(PVMDKIMAGE pImage, const PVBOXHDDRAW pRaw,
                              uint64_t cbSize)
{
    int rc = VINF_SUCCESS;
    PVMDKEXTENT pExtent;

    if (pRaw->fRawDisk)
    {
        /* Full raw disk access. This requires setting up a descriptor
         * file and open the (flat) raw disk. */
        rc = vmdkCreateExtents(pImage, 1);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);
        pExtent = &pImage->pExtents[0];
        /* Create raw disk descriptor file. */
        rc = vmdkFileOpen(pImage, &pImage->File, pImage->pszFilename,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_NOT_CONTENT_INDEXED);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pImage->pszFilename);

        /* Set up basename for extent description. Cannot use StrDup. */
        size_t cbBasename = strlen(pRaw->pszRawDisk) + 1;
        char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
        if (!pszBasename)
            return VERR_NO_MEMORY;
        memcpy(pszBasename, pRaw->pszRawDisk, cbBasename);
        pExtent->pszBasename = pszBasename;
        /* For raw disks the full name is identical to the base name. */
        pExtent->pszFullname = RTStrDup(pszBasename);
        if (!pExtent->pszFullname)
            return VERR_NO_MEMORY;
        pExtent->enmType = VMDKETYPE_FLAT;
        pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbSize);
        pExtent->uSectorOffset = 0;
        pExtent->enmAccess = VMDKACCESS_READWRITE;
        pExtent->fMetaDirty = false;

        /* Open flat image, the raw disk. */
        rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                          RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not open raw disk file '%s'"), pExtent->pszFullname);
    }
    else
    {
        /* Raw partition access. This requires setting up a descriptor
         * file, write the partition information to a flat extent and
         * open all the (flat) raw disk partitions. */

        /* First pass over the partitions to determine how many
         * extents we need. One partition can require up to 4 extents.
         * One to skip over unpartitioned space, one for the
         * partitioning data, one to skip over unpartitioned space
         * and one for the partition data. */
        unsigned cExtents = 0;
        uint64_t uStart = 0;
        for (unsigned i = 0; i < pRaw->cPartitions; i++)
        {
            PVBOXHDDRAWPART pPart = &pRaw->pPartitions[i];
            if (pPart->cbPartitionData)
            {
                if (uStart > pPart->uPartitionDataStart)
                    return vmdkError(pImage, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("VMDK: cannot go backwards for partitioning information in '%s'"), pImage->pszFilename);
                else if (uStart != pPart->uPartitionDataStart)
                    cExtents++;
                uStart = pPart->uPartitionDataStart + pPart->cbPartitionData;
                cExtents++;
            }
            if (pPart->cbPartition)
            {
                if (uStart > pPart->uPartitionStart)
                    return vmdkError(pImage, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("VMDK: cannot go backwards for partition data in '%s'"), pImage->pszFilename);
                else if (uStart != pPart->uPartitionStart)
                    cExtents++;
                uStart = pPart->uPartitionStart + pPart->cbPartition;
                cExtents++;
            }
        }
        /* Another extent for filling up the rest of the image. */
        if (uStart != cbSize)
            cExtents++;

        rc = vmdkCreateExtents(pImage, cExtents);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);

        /* Create raw partition descriptor file. */
        rc = vmdkFileOpen(pImage, &pImage->File, pImage->pszFilename,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_NOT_CONTENT_INDEXED);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pImage->pszFilename);

        /* Create base filename for the partition table extent. */
        /** @todo remove fixed buffer without creating memory leaks. */
        char pszPartition[1024];
        const char *pszBase = RTPathFilename(pImage->pszFilename);
        const char *pszExt = RTPathExt(pszBase);
        if (pszExt == NULL)
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: invalid filename '%s'"), pImage->pszFilename);
        char *pszBaseBase = RTStrDup(pszBase);
        if (!pszBaseBase)
            return VERR_NO_MEMORY;
        RTPathStripExt(pszBaseBase);
        RTStrPrintf(pszPartition, sizeof(pszPartition), "%s-pt%s",
                    pszBaseBase, pszExt);
        RTStrFree(pszBaseBase);

        /* Second pass over the partitions, now define all extents. */
        uint64_t uPartOffset = 0;
        cExtents = 0;
        uStart = 0;
        for (unsigned i = 0; i < pRaw->cPartitions; i++)
        {
            PVBOXHDDRAWPART pPart = &pRaw->pPartitions[i];
            if (pPart->cbPartitionData)
            {
                if (uStart != pPart->uPartitionDataStart)
                {
                    pExtent = &pImage->pExtents[cExtents++];
                    pExtent->pszBasename = NULL;
                    pExtent->pszFullname = NULL;
                    pExtent->enmType = VMDKETYPE_ZERO;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->uPartitionDataStart - uStart);
                    pExtent->uSectorOffset = 0;
                    pExtent->enmAccess = VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;
                }
                uStart = pPart->uPartitionDataStart + pPart->cbPartitionData;
                pExtent = &pImage->pExtents[cExtents++];
                /* Set up basename for extent description. Can't use StrDup. */
                size_t cbBasename = strlen(pszPartition) + 1;
                char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
                if (!pszBasename)
                    return VERR_NO_MEMORY;
                memcpy(pszBasename, pszPartition, cbBasename);
                pExtent->pszBasename = pszBasename;

                /* Set up full name for partition extent. */
                size_t cbDirname;
                char *pszDirname = RTStrDup(pImage->pszFilename);
                if (!pszDirname)
                    return VERR_NO_MEMORY;
                RTPathStripFilename(pszDirname);
                cbDirname = strlen(pszDirname);
                char *pszFullname;
                rc = RTStrAPrintf(&pszFullname, "%s%c%s", pszDirname,
                                  RTPATH_SLASH, pExtent->pszBasename);
                RTStrFree(pszDirname);
                if (VBOX_FAILURE(rc))
                    return rc;
                pExtent->pszFullname = pszFullname;
                pExtent->enmType = VMDKETYPE_FLAT;
                pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbPartitionData);
                pExtent->uSectorOffset = uPartOffset;
                pExtent->enmAccess = VMDKACCESS_READWRITE;
                pExtent->fMetaDirty = false;

                /* Create partition table flat image. */
                rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                                  RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_NOT_CONTENT_INDEXED);
                if (VBOX_FAILURE(rc))
                    return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new partition data file '%s'"), pExtent->pszFullname);
                rc = RTFileWriteAt(pExtent->File,
                                   VMDK_SECTOR2BYTE(uPartOffset),
                                   pPart->pvPartitionData,
                                   pPart->cbPartitionData, NULL);
                if (VBOX_FAILURE(rc))
                    return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not write partition data to '%s'"), pExtent->pszFullname);
                uPartOffset += VMDK_BYTE2SECTOR(pPart->cbPartitionData);
            }
            if (pPart->cbPartition)
            {
                if (uStart != pPart->uPartitionStart)
                {
                    pExtent = &pImage->pExtents[cExtents++];
                    pExtent->pszBasename = NULL;
                    pExtent->pszFullname = NULL;
                    pExtent->enmType = VMDKETYPE_ZERO;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->uPartitionStart - uStart);
                    pExtent->uSectorOffset = 0;
                    pExtent->enmAccess = VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;
                }
                uStart = pPart->uPartitionStart + pPart->cbPartition;
                pExtent = &pImage->pExtents[cExtents++];
                if (pPart->pszRawDevice)
                {
                    /* Set up basename for extent descr. Can't use StrDup. */
                    size_t cbBasename = strlen(pPart->pszRawDevice) + 1;
                    char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
                    if (!pszBasename)
                        return VERR_NO_MEMORY;
                    memcpy(pszBasename, pPart->pszRawDevice, cbBasename);
                    pExtent->pszBasename = pszBasename;
                    /* For raw disks full name is identical to base name. */
                    pExtent->pszFullname = RTStrDup(pszBasename);
                    if (!pExtent->pszFullname)
                        return VERR_NO_MEMORY;
                    pExtent->enmType = VMDKETYPE_FLAT;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbPartition);
                    pExtent->uSectorOffset = VMDK_BYTE2SECTOR(pPart->uPartitionStartOffset);
                    pExtent->enmAccess = VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;

                    /* Open flat image, the raw partition. */
                    rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                                      RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                    if (VBOX_FAILURE(rc))
                        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not open raw partition file '%s'"), pExtent->pszFullname);
                }
                else
                {
                    pExtent->pszBasename = NULL;
                    pExtent->pszFullname = NULL;
                    pExtent->enmType = VMDKETYPE_ZERO;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbPartition);
                    pExtent->uSectorOffset = 0;
                    pExtent->enmAccess = VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;
                }
            }
        }
        /* Another extent for filling up the rest of the image. */
        if (uStart != cbSize)
        {
            pExtent = &pImage->pExtents[cExtents++];
            pExtent->pszBasename = NULL;
            pExtent->pszFullname = NULL;
            pExtent->enmType = VMDKETYPE_ZERO;
            pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbSize - uStart);
            pExtent->uSectorOffset = 0;
            pExtent->enmAccess = VMDKACCESS_READWRITE;
            pExtent->fMetaDirty = false;
        }
    }

    rc = vmdkDescBaseSetStr(pImage, &pImage->Descriptor, "createType",
                            pRaw->fRawDisk ?
                            "fullDevice" : "partitionedDevice");
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not set the image type in '%s'"), pImage->pszFilename);
    return rc;
}

/**
 * Internal: create a regular (i.e. file-backed) VMDK image.
 */
static int vmdkCreateRegularImage(PVMDKIMAGE pImage, VDIMAGETYPE enmType,
                                  uint64_t cbSize, unsigned uImageFlags,
                                  PFNVMPROGRESS pfnProgress, void *pvUser,
                                  unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc = VINF_SUCCESS;
    unsigned cExtents = 1;
    uint64_t cbOffset = 0;
    uint64_t cbRemaining = cbSize;

    if (uImageFlags & VD_VMDK_IMAGE_FLAGS_SPLIT_2G)
    {
        cExtents = cbSize / VMDK_2G_SPLIT_SIZE;
        /* Do proper extent computation: need one smaller extent if the total
         * size isn't evenly divisible by the split size. */
        if (cbSize % VMDK_2G_SPLIT_SIZE)
            cExtents++;
    }
    rc = vmdkCreateExtents(pImage, cExtents);
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);

    /* Basename strings needed for constructing the extent names. */
    char *pszBasenameSubstr = RTPathFilename(pImage->pszFilename);
    Assert(pszBasenameSubstr);
    size_t cbBasenameSubstr = strlen(pszBasenameSubstr) + 1;

    /* Create searate descriptor file if necessary. */
    if (cExtents != 1 || enmType == VD_IMAGE_TYPE_FIXED)
    {
        rc = vmdkFileOpen(pImage, &pImage->File, pImage->pszFilename,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_NOT_CONTENT_INDEXED);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new sparse descriptor file '%s'"), pImage->pszFilename);
        pImage->pszFilename = RTStrDup(pImage->pszFilename);
    }
    else
        pImage->File = NIL_RTFILE;

    /* Set up all extents. */
    for (unsigned i = 0; i < cExtents; i++)
    {
        PVMDKEXTENT pExtent = &pImage->pExtents[i];
        uint64_t cbExtent = cbRemaining;

        /* Set up fullname/basename for extent description. Cannot use StrDup
         * for basename, as it is not guaranteed that the memory can be freed
         * with RTMemTmpFree, which must be used as in other code paths
         * StrDup is not usable. */
        if (cExtents == 1 && enmType != VD_IMAGE_TYPE_FIXED)
        {
            char *pszBasename = (char *)RTMemTmpAlloc(cbBasenameSubstr);
            if (!pszBasename)
                return VERR_NO_MEMORY;
            memcpy(pszBasename, pszBasenameSubstr, cbBasenameSubstr);
            pExtent->pszBasename = pszBasename;
        }
        else
        {
            char *pszBasenameExt = RTPathExt(pszBasenameSubstr);
            char *pszBasenameBase = RTStrDup(pszBasenameSubstr);
            RTPathStripExt(pszBasenameBase);
            char *pszTmp;
            size_t cbTmp;
            if (enmType == VD_IMAGE_TYPE_FIXED)
            {
                if (cExtents == 1)
                    rc = RTStrAPrintf(&pszTmp, "%s-flat%s", pszBasenameBase,
                                      pszBasenameExt);
                else
                    rc = RTStrAPrintf(&pszTmp, "%s-f%03d%s", pszBasenameBase,
                                      i+1, pszBasenameExt);
            }
            else
                rc = RTStrAPrintf(&pszTmp, "%s-s%03d%s", pszBasenameBase, i+1,
                                  pszBasenameExt);
            RTStrFree(pszBasenameBase);
            if (VBOX_FAILURE(rc))
                return rc;
            cbTmp = strlen(pszTmp) + 1;
            char *pszBasename = (char *)RTMemTmpAlloc(cbTmp);
            if (!pszBasename)
                return VERR_NO_MEMORY;
            memcpy(pszBasename, pszTmp, cbTmp);
            RTStrFree(pszTmp);
            pExtent->pszBasename = pszBasename;
            cbExtent = RT_MIN(cbRemaining, VMDK_2G_SPLIT_SIZE);
        }
        char *pszBasedirectory = RTStrDup(pImage->pszFilename);
        RTPathStripFilename(pszBasedirectory);
        char *pszFN;
        rc = RTStrAPrintf(&pszFN, "%s%c%s", pszBasedirectory, RTPATH_SLASH,
                          pExtent->pszBasename);
        RTStrFree(pszBasedirectory);
        if (VBOX_FAILURE(rc))
            return rc;
        pExtent->pszFullname = pszFN;

        /* Create file for extent. */
        rc = vmdkFileOpen(pImage, &pExtent->File, pExtent->pszFullname,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_NOT_CONTENT_INDEXED);
        if (VBOX_FAILURE(rc))
            return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pExtent->pszFullname);
        if (enmType == VD_IMAGE_TYPE_FIXED)
        {
            rc = RTFileSetSize(pExtent->File, cbExtent);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not set size of new file '%s'"), pExtent->pszFullname);
        }

        /* Place descriptor file information (where integrated). */
        if (cExtents == 1 && enmType != VD_IMAGE_TYPE_FIXED)
        {
            pExtent->uDescriptorSector = 1;
            pExtent->cDescriptorSectors = VMDK_BYTE2SECTOR(pImage->cbDescAlloc);
            /* The descriptor is part of the (only) extent. */
            pExtent->pDescData = pImage->pDescData;
            pImage->pDescData = NULL;
        }

        if (enmType == VD_IMAGE_TYPE_NORMAL)
        {
            uint64_t cSectorsPerGDE, cSectorsPerGD;
            pExtent->enmType = VMDKETYPE_HOSTED_SPARSE;
            pExtent->cSectors = VMDK_BYTE2SECTOR(RT_ALIGN_64(cbExtent, 65536));
            pExtent->cSectorsPerGrain = VMDK_BYTE2SECTOR(65536);
            pExtent->cGTEntries = 512;
            cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
            pExtent->cSectorsPerGDE = cSectorsPerGDE;
            pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;
            cSectorsPerGD = (pExtent->cGDEntries + (512 / sizeof(uint32_t) - 1)) / (512 / sizeof(uint32_t));
        }
        else
            pExtent->enmType = VMDKETYPE_FLAT;

        pExtent->enmAccess = VMDKACCESS_READWRITE;
        pExtent->fUncleanShutdown = true;
        pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbExtent);
        pExtent->uSectorOffset = VMDK_BYTE2SECTOR(cbOffset);
        pExtent->fMetaDirty = true;

        if (enmType == VD_IMAGE_TYPE_NORMAL)
        {
            rc = vmdkCreateGrainDirectory(pExtent,
                                            pExtent->uDescriptorSector
                                          + pExtent->cDescriptorSectors,
                                          true);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new grain directory in '%s'"), pExtent->pszFullname);
        }

        if (VBOX_SUCCESS(rc) && pfnProgress)
            pfnProgress(NULL /* WARNING! pVM=NULL  */,
                        uPercentStart + i * uPercentSpan / cExtents,
                        pvUser);

        cbRemaining -= cbExtent;
        cbOffset += cbExtent;
    }

    const char *pszDescType = NULL;
    if (enmType == VD_IMAGE_TYPE_FIXED)
    {
        pszDescType =   (cExtents == 1)
                      ? "monolithicFlat" : "twoGbMaxExtentFlat";
    }
    else if (enmType == VD_IMAGE_TYPE_NORMAL)
    {
        pszDescType =   (cExtents == 1)
                      ? "monolithicSparse" : "twoGbMaxExtentSparse";
    }
    else
        AssertMsgFailed(("invalid image type %d\n", enmType));
    rc = vmdkDescBaseSetStr(pImage, &pImage->Descriptor, "createType",
                            pszDescType);
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not set the image type in '%s'"), pImage->pszFilename);
    return rc;
}

/**
 * Internal: The actual code for creating any VMDK variant currently in
 * existence on hosted environments.
 */
static int vmdkCreateImage(PVMDKIMAGE pImage, VDIMAGETYPE enmType,
                           uint64_t cbSize, unsigned uImageFlags,
                           const char *pszComment,
                           PCPDMMEDIAGEOMETRY pPCHSGeometry,
                           PCPDMMEDIAGEOMETRY pLCHSGeometry,
                           PFNVMPROGRESS pfnProgress, void *pvUser,
                           unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;

    pImage->uImageFlags = uImageFlags;
    rc = vmdkCreateDescriptor(pImage, pImage->pDescData, pImage->cbDescAlloc,
                              &pImage->Descriptor);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not create new descriptor in '%s'"), pImage->pszFilename);
        goto out;
    }

    if (    enmType == VD_IMAGE_TYPE_FIXED
        &&  (uImageFlags & VD_VMDK_IMAGE_FLAGS_RAWDISK))
    {
        /* Raw disk image (includes raw partition). */
        const PVBOXHDDRAW pRaw = (const PVBOXHDDRAW)pszComment;
        /* As the comment is misused, zap it so that no garbage comment
         * is set below. */
        pszComment = NULL;
        rc = vmdkCreateRawImage(pImage, pRaw, cbSize);
    }
    else if (   enmType == VD_IMAGE_TYPE_FIXED
             || enmType == VD_IMAGE_TYPE_NORMAL)
    {
        /* Regular fixed or sparse image (monolithic or split). */
        rc = vmdkCreateRegularImage(pImage, enmType, cbSize, uImageFlags,
                                    pfnProgress, pvUser, uPercentStart,
                                    uPercentSpan * 95 / 100);
    }
    else
    {
        /* Unknown/invalid image type. */
        rc = VERR_NOT_IMPLEMENTED;
    }

    if (VBOX_FAILURE(rc))
        goto out;

    if (VBOX_SUCCESS(rc) && pfnProgress)
        pfnProgress(NULL /* WARNING! pVM=NULL  */,
                    uPercentStart + uPercentSpan * 98 / 100, pvUser);

    pImage->enmImageType = enmType;
    pImage->cbSize = cbSize;

    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        PVMDKEXTENT pExtent = &pImage->pExtents[i];

        rc = vmdkDescExtInsert(pImage, &pImage->Descriptor, pExtent->enmAccess,
                               pExtent->cNominalSectors, pExtent->enmType,
                               pExtent->pszBasename, pExtent->uSectorOffset);
        if (VBOX_FAILURE(rc))
        {
            rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: could not insert the extent list into descriptor in '%s'"), pImage->pszFilename);
            goto out;
        }
    }
    vmdkDescExtRemoveDummy(pImage, &pImage->Descriptor);

    if (    pPCHSGeometry->cCylinders == 0
        ||  pPCHSGeometry->cHeads == 0
        ||  pPCHSGeometry->cSectors == 0)
    {
        rc = vmdkDescSetPCHSGeometry(pImage, pPCHSGeometry);
        if (VBOX_FAILURE(rc))
            goto out;
    }
    if (    pLCHSGeometry->cCylinders == 0
        ||  pLCHSGeometry->cHeads == 0
        ||  pLCHSGeometry->cSectors == 0)
    {
        rc = vmdkDescSetLCHSGeometry(pImage, pLCHSGeometry);
        if (VBOX_FAILURE(rc))
            goto out;
    }

    pImage->LCHSGeometry = *pLCHSGeometry;
    pImage->PCHSGeometry = *pPCHSGeometry;

    rc = RTUuidCreate(&pImage->ImageUuid);
    if (VBOX_FAILURE(rc))
        goto out;
    rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_IMAGE_UUID, &pImage->ImageUuid);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing image UUID in new descriptor in '%s'"), pImage->pszFilename);
        goto out;
    }
    RTUuidClear(&pImage->ParentUuid);
    rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_PARENT_UUID, &pImage->ParentUuid);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent image UUID in new descriptor in '%s'"), pImage->pszFilename);
        goto out;
    }
    RTUuidClear(&pImage->ModificationUuid);
    rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_MODIFICATION_UUID,
                            &pImage->ModificationUuid);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing modification UUID in new descriptor in '%s'"), pImage->pszFilename);
        goto out;
    }
    RTUuidClear(&pImage->ParentModificationUuid);
    rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_PARENT_MODIFICATION_UUID,
                            &pImage->ParentModificationUuid);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent modification UUID in new descriptor in '%s'"), pImage->pszFilename);
        goto out;
    }

    rc = vmdkAllocateGrainTableCache(pImage);
    if (VBOX_FAILURE(rc))
        goto out;

    rc = vmdkSetImageComment(pImage, pszComment);
    if (VBOX_FAILURE(rc))
    {
        rc = vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: cannot set image comment in '%s'"), pImage->pszFilename);
        goto out;
    }

    if (VBOX_SUCCESS(rc) && pfnProgress)
        pfnProgress(NULL /* WARNING! pVM=NULL  */,
                    uPercentStart + uPercentSpan * 99 / 100, pvUser);

    rc = vmdkFlushImage(pImage);

out:
    if (VBOX_SUCCESS(rc) && pfnProgress)
        pfnProgress(NULL /* WARNING! pVM=NULL  */,
                    uPercentStart + uPercentSpan, pvUser);

    if (VBOX_FAILURE(rc))
        vmdkFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/**
 * Internal: Update image comment.
 */
static int vmdkSetImageComment(PVMDKIMAGE pImage, const char *pszComment)
{
    char *pszCommentEncoded;
    if (pszComment)
    {
        pszCommentEncoded = vmdkEncodeString(pszComment);
        if (!pszCommentEncoded)
            return VERR_NO_MEMORY;
    }
    else
        pszCommentEncoded = NULL;
    int rc = vmdkDescDDBSetStr(pImage, &pImage->Descriptor,
                          "ddb.comment", pszCommentEncoded);
    if (pszComment)
        RTStrFree(pszCommentEncoded);
    if (VBOX_FAILURE(rc))
        return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing image comment in descriptor in '%s'"), pImage->pszFilename);
    return VINF_SUCCESS;
}

/**
 * Internal. Free all allocated space for representing an image, and optionally
 * delete the image from disk.
 */
static void vmdkFreeImage(PVMDKIMAGE pImage, bool fDelete)
{
    Assert(pImage);

    if (pImage->enmImageType)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            /* Mark all extents as clean. */
            for (unsigned i = 0; i < pImage->cExtents; i++)
            {
                if ((   pImage->pExtents[i].enmType == VMDKETYPE_HOSTED_SPARSE
#ifdef VBOX_WITH_VMDK_ESX
                     || pImage->pExtents[i].enmType == VMDKETYPE_ESX_SPARSE
#endif /* VBOX_WITH_VMDK_ESX */
                    )
                    &&  pImage->pExtents[i].fUncleanShutdown)
                {
                    pImage->pExtents[i].fUncleanShutdown = false;
                    pImage->pExtents[i].fMetaDirty = true;
                }
            }
        }
        (void)vmdkFlushImage(pImage);
    }
    if (pImage->pExtents != NULL)
    {
        for (unsigned i = 0 ; i < pImage->cExtents; i++)
            vmdkFreeExtentData(pImage, &pImage->pExtents[i], fDelete);
        RTMemFree(pImage->pExtents);
        pImage->pExtents = NULL;
    }
    if (pImage->File != NIL_RTFILE)
        vmdkFileClose(pImage, &pImage->File, fDelete);
    vmdkFileCheckAllClose(pImage);
}

/**
 * Internal. Flush image data (and metadata) to disk.
 */
static int vmdkFlushImage(PVMDKIMAGE pImage)
{
    PVMDKEXTENT pExtent;
    int rc = VINF_SUCCESS;

    /* Update descriptor if changed. */
    if (pImage->Descriptor.fDirty)
    {
        rc = vmdkWriteDescriptor(pImage);
        if (VBOX_FAILURE(rc))
            goto out;
    }

    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        pExtent = &pImage->pExtents[i];
        if (pExtent->File != NIL_RTFILE && pExtent->fMetaDirty)
        {
            switch (pExtent->enmType)
            {
                case VMDKETYPE_HOSTED_SPARSE:
                    rc = vmdkWriteMetaSparseExtent(pExtent);
                    if (VBOX_FAILURE(rc))
                        goto out;
                    break;
#ifdef VBOX_WITH_VMDK_ESX
                case VMDKETYPE_ESX_SPARSE:
                    /** @todo update the header. */
                    break;
#endif /* VBOX_WITH_VMDK_ESX */
                case VMDKETYPE_FLAT:
                    /* Nothing to do. */
                    break;
                case VMDKETYPE_ZERO:
                default:
                    AssertMsgFailed(("extent with type %d marked as dirty\n",
                                     pExtent->enmType));
                    break;
            }
        }
        switch (pExtent->enmType)
        {
            case VMDKETYPE_HOSTED_SPARSE:
#ifdef VBOX_WITH_VMDK_ESX
            case VMDKETYPE_ESX_SPARSE:
#endif /* VBOX_WITH_VMDK_ESX */
            case VMDKETYPE_FLAT:
                /** @todo implement proper path absolute check. */
                if (   pExtent->File != NIL_RTFILE
                    && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                    && !(pExtent->pszBasename[0] == RTPATH_SLASH))
                    rc = RTFileFlush(pExtent->File);
                break;
            case VMDKETYPE_ZERO:
                /* No need to do anything for this extent. */
                break;
            default:
                AssertMsgFailed(("unknown extent type %d\n", pExtent->enmType));
                break;
        }
    }

out:
    return rc;
}

/**
 * Internal. Find extent corresponding to the sector number in the disk.
 */
static int vmdkFindExtent(PVMDKIMAGE pImage, uint64_t offSector,
                          PVMDKEXTENT *ppExtent, uint64_t *puSectorInExtent)
{
    PVMDKEXTENT pExtent = NULL;
    int rc = VINF_SUCCESS;

    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        if (offSector < pImage->pExtents[i].cNominalSectors)
        {
            pExtent = &pImage->pExtents[i];
            *puSectorInExtent = offSector + pImage->pExtents[i].uSectorOffset;
            break;
        }
        offSector -= pImage->pExtents[i].cNominalSectors;
    }

    if (pExtent)
        *ppExtent = pExtent;
    else
        rc = VERR_IO_SECTOR_NOT_FOUND;

    return rc;
}

/**
 * Internal. Hash function for placing the grain table hash entries.
 */
static uint32_t vmdkGTCacheHash(PVMDKGTCACHE pCache, uint64_t uSector,
                                unsigned uExtent)
{
    /** @todo this hash function is quite simple, maybe use a better one which
     * scrambles the bits better. */
    return (uSector + uExtent) % pCache->cEntries;
}

/**
 * Internal. Get sector number in the extent file from the relative sector
 * number in the extent.
 */
static int vmdkGetSector(PVMDKGTCACHE pCache, PVMDKEXTENT pExtent,
                         uint64_t uSector, uint64_t *puExtentSector)
{
    uint64_t uGDIndex, uGTSector, uGTBlock;
    uint32_t uGTHash, uGTBlockIndex;
    PVMDKGTCACHEENTRY pGTCacheEntry;
    uint32_t aGTDataTmp[VMDK_GT_CACHELINE_SIZE];
    int rc;

    uGDIndex = uSector / pExtent->cSectorsPerGDE;
    if (uGDIndex >= pExtent->cGDEntries)
        return VERR_OUT_OF_RANGE;
    uGTSector = pExtent->pGD[uGDIndex];
    if (!uGTSector)
    {
        /* There is no grain table referenced by this grain directory
         * entry. So there is absolutely no data in this area. */
        *puExtentSector = 0;
        return VINF_SUCCESS;
    }

    uGTBlock = uSector / (pExtent->cSectorsPerGrain * VMDK_GT_CACHELINE_SIZE);
    uGTHash = vmdkGTCacheHash(pCache, uGTBlock, pExtent->uExtent);
    pGTCacheEntry = &pCache->aGTCache[uGTHash];
    if (    pGTCacheEntry->uExtent != pExtent->uExtent
        ||  pGTCacheEntry->uGTBlock != uGTBlock)
    {
        /* Cache miss, fetch data from disk. */
        rc = RTFileReadAt(pExtent->File,
                          VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                          aGTDataTmp, sizeof(aGTDataTmp), NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot read grain table entry in '%s'"), pExtent->pszFullname);
        pGTCacheEntry->uExtent = pExtent->uExtent;
        pGTCacheEntry->uGTBlock = uGTBlock;
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            pGTCacheEntry->aGTData[i] = RT_LE2H_U32(aGTDataTmp[i]);
    }
    uGTBlockIndex = (uSector / pExtent->cSectorsPerGrain) % VMDK_GT_CACHELINE_SIZE;
    uint64_t uGrainSector = pGTCacheEntry->aGTData[uGTBlockIndex];
    if (uGrainSector)
        *puExtentSector = uGrainSector + uSector % pExtent->cSectorsPerGrain;
    else
        *puExtentSector = 0;
    return VINF_SUCCESS;
}

/**
 * Internal. Allocates a new grain table (if necessary), writes the grain
 * and updates the grain table. The cache is also updated by this operation.
 * This is separate from vmdkGetSector, because that should be as fast as
 * possible. Most code from vmdkGetSector also appears here.
 */
static int vmdkAllocGrain(PVMDKGTCACHE pCache, PVMDKEXTENT pExtent,
                          uint64_t uSector, const void *pvBuf,
                          uint64_t cbWrite)
{
    uint64_t uGDIndex, uGTSector, uRGTSector, uGTBlock;
    uint64_t cbExtentSize;
    uint32_t uGTHash, uGTBlockIndex;
    PVMDKGTCACHEENTRY pGTCacheEntry;
    uint32_t aGTDataTmp[VMDK_GT_CACHELINE_SIZE];
    int rc;

    uGDIndex = uSector / pExtent->cSectorsPerGDE;
    if (uGDIndex >= pExtent->cGDEntries)
        return VERR_OUT_OF_RANGE;
    uGTSector = pExtent->pGD[uGDIndex];
    uRGTSector = pExtent->pRGD[uGDIndex];
    if (!uGTSector)
    {
        /* There is no grain table referenced by this grain directory
         * entry. So there is absolutely no data in this area. Allocate
         * a new grain table and put the reference to it in the GDs. */
        rc = RTFileGetSize(pExtent->File, &cbExtentSize);
        if (VBOX_FAILURE(rc))
            return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error getting size in '%s'"), pExtent->pszFullname);
        Assert(!(cbExtentSize % 512));
        uGTSector = VMDK_BYTE2SECTOR(cbExtentSize);
        /* Normally the grain table is preallocated for hosted sparse extents
         * that support more than 32 bit sector numbers. So this shouldn't
         * ever happen on a valid extent. */
        if (uGTSector > UINT32_MAX)
            return VERR_VDI_INVALID_HEADER;
        /* Write grain table by writing the required number of grain table
         * cache chunks. Avoids dynamic memory allocation, but is a bit
         * slower. But as this is a pretty infrequently occurring case it
         * should be acceptable. */
        memset(aGTDataTmp, '\0', sizeof(aGTDataTmp));
        for (unsigned i = 0;
             i < pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE;
             i++)
        {
            rc = RTFileWriteAt(pExtent->File,
                               VMDK_SECTOR2BYTE(uGTSector) + i * sizeof(aGTDataTmp),
                               aGTDataTmp, sizeof(aGTDataTmp), NULL);
            if (VBOX_FAILURE(rc))
                return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write grain table allocation in '%s'"), pExtent->pszFullname);
        }
        if (pExtent->pRGD)
        {
            rc = RTFileGetSize(pExtent->File, &cbExtentSize);
            if (VBOX_FAILURE(rc))
                return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error getting size in '%s'"), pExtent->pszFullname);
            Assert(!(cbExtentSize % 512));
            uRGTSector = VMDK_BYTE2SECTOR(cbExtentSize);
            /* Write backup grain table by writing the required number of grain
             * table cache chunks. Avoids dynamic memory allocation, but is a
             * bit slower. But as this is a pretty infrequently occurring case
             * it should be acceptable. */
            for (unsigned i = 0;
                 i < pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE;
                 i++)
            {
                rc = RTFileWriteAt(pExtent->File,
                                   VMDK_SECTOR2BYTE(uRGTSector) + i * sizeof(aGTDataTmp),
                                   aGTDataTmp, sizeof(aGTDataTmp), NULL);
                if (VBOX_FAILURE(rc))
                    return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write backup grain table allocation in '%s'"), pExtent->pszFullname);
            }
        }

        /* Update the grain directory on disk (doing it before writing the
         * grain table will result in a garbled extent if the operation is
         * aborted for some reason. Otherwise the worst that can happen is
         * some unused sectors in the extent. */
        uint32_t uGTSectorLE = RT_H2LE_U64(uGTSector);
        rc = RTFileWriteAt(pExtent->File,
                           VMDK_SECTOR2BYTE(pExtent->uSectorGD) + uGDIndex * sizeof(uGTSectorLE),
                           &uGTSectorLE, sizeof(uGTSectorLE), NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write grain directory entry in '%s'"), pExtent->pszFullname);
        if (pExtent->pRGD)
        {
            uint32_t uRGTSectorLE = RT_H2LE_U64(uRGTSector);
            rc = RTFileWriteAt(pExtent->File,
                               VMDK_SECTOR2BYTE(pExtent->uSectorRGD) + uGDIndex * sizeof(uRGTSectorLE),
                               &uRGTSectorLE, sizeof(uRGTSectorLE), NULL);
            if (VBOX_FAILURE(rc))
                return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write backup grain directory entry in '%s'"), pExtent->pszFullname);
        }

        /* As the final step update the in-memory copy of the GDs. */
        pExtent->pGD[uGDIndex] = uGTSector;
        if (pExtent->pRGD)
            pExtent->pRGD[uGDIndex] = uRGTSector;
    }

    rc = RTFileGetSize(pExtent->File, &cbExtentSize);
    if (VBOX_FAILURE(rc))
        return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: error getting size in '%s'"), pExtent->pszFullname);
    Assert(!(cbExtentSize % 512));

    /* Write the data. */
    rc = RTFileWriteAt(pExtent->File, cbExtentSize, pvBuf, cbWrite, NULL);
    if (VBOX_FAILURE(rc))
        return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write allocated data block in '%s'"), pExtent->pszFullname);

    /* Update the grain table (and the cache). */
    uGTBlock = uSector / (pExtent->cSectorsPerGrain * VMDK_GT_CACHELINE_SIZE);
    uGTHash = vmdkGTCacheHash(pCache, uGTBlock, pExtent->uExtent);
    pGTCacheEntry = &pCache->aGTCache[uGTHash];
    if (    pGTCacheEntry->uExtent != pExtent->uExtent
        ||  pGTCacheEntry->uGTBlock != uGTBlock)
    {
        /* Cache miss, fetch data from disk. */
        rc = RTFileReadAt(pExtent->File,
                          VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                          aGTDataTmp, sizeof(aGTDataTmp), NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot read allocated grain table entry in '%s'"), pExtent->pszFullname);
        pGTCacheEntry->uExtent = pExtent->uExtent;
        pGTCacheEntry->uGTBlock = uGTBlock;
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            pGTCacheEntry->aGTData[i] = RT_LE2H_U32(aGTDataTmp[i]);
    }
    else
    {
        /* Cache hit. Convert grain table block back to disk format, otherwise
         * the code below will write garbage for all but the updated entry. */
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            aGTDataTmp[i] = RT_H2LE_U32(pGTCacheEntry->aGTData[i]);
    }
    uGTBlockIndex = (uSector / pExtent->cSectorsPerGrain) % VMDK_GT_CACHELINE_SIZE;
    aGTDataTmp[uGTBlockIndex] = RT_H2LE_U32(VMDK_BYTE2SECTOR(cbExtentSize));
    pGTCacheEntry->aGTData[uGTBlockIndex] = VMDK_BYTE2SECTOR(cbExtentSize);
    /* Update grain table on disk. */
    rc = RTFileWriteAt(pExtent->File,
                       VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                       aGTDataTmp, sizeof(aGTDataTmp), NULL);
    if (VBOX_FAILURE(rc))
        return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write updated grain table in '%s'"), pExtent->pszFullname);
    if (pExtent->pRGD)
    {
        /* Update backup grain table on disk. */
        rc = RTFileWriteAt(pExtent->File,
                           VMDK_SECTOR2BYTE(uRGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                           aGTDataTmp, sizeof(aGTDataTmp), NULL);
        if (VBOX_FAILURE(rc))
            return vmdkError(pExtent->pImage, rc, RT_SRC_POS, N_("VMDK: cannot write updated backup grain table in '%s'"), pExtent->pszFullname);
    }
#ifdef VBOX_WITH_VMDK_ESX
    if (VBOX_SUCCESS(rc) && pExtent->enmType == VMDKETYPE_ESX_SPARSE)
    {
        pExtent->uFreeSector = uGTSector + VMDK_BYTE2SECTOR(cbWrite);
        pExtent->fMetaDirty = true;
    }
#endif /* VBOX_WITH_VMDK_ESX */
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int vmdkCheckIfValid(const char *pszFilename)
{
    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));
    int rc = VINF_SUCCESS;
    PVMDKIMAGE pImage;

    if (   !pszFilename
        || !*pszFilename
        || strchr(pszFilename, '"'))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PVMDKIMAGE)RTMemAllocZ(sizeof(VMDKIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->File = NIL_RTFILE;
    pImage->pExtents = NULL;
    pImage->pFiles = NULL;
    pImage->pGTCache = NULL;
    pImage->pDescData = NULL;
    pImage->pfnError = NULL;
    pImage->pvErrorUser = NULL;
    /** @todo speed up this test open (VD_OPEN_FLAGS_INFO) by skipping as
     * much as possible in vmdkOpenImage. */
    rc = vmdkOpenImage(pImage, VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_READONLY);
    vmdkFreeImage(pImage, false);

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int vmdkOpen(const char *pszFilename, unsigned uOpenFlags,
                    PFNVDERROR pfnError, void *pvErrorUser,
                    void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x ppBackendData=%#p\n", pszFilename, uOpenFlags, ppBackendData));
    int rc;
    PVMDKIMAGE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || strchr(pszFilename, '"'))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }


    pImage = (PVMDKIMAGE)RTMemAllocZ(sizeof(VMDKIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->File = NIL_RTFILE;
    pImage->pExtents = NULL;
    pImage->pFiles = NULL;
    pImage->pGTCache = NULL;
    pImage->pDescData = NULL;
    pImage->pfnError = pfnError;
    pImage->pvErrorUser = pvErrorUser;

    rc = vmdkOpenImage(pImage, uOpenFlags);
    if (VBOX_SUCCESS(rc))
        *ppBackendData = pImage;

out:
    LogFlowFunc(("returns %Vrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int vmdkCreate(const char *pszFilename, VDIMAGETYPE enmType,
                      uint64_t cbSize, unsigned uImageFlags,
                      const char *pszComment,
                      PCPDMMEDIAGEOMETRY pPCHSGeometry,
                      PCPDMMEDIAGEOMETRY pLCHSGeometry,
                      unsigned uOpenFlags, PFNVMPROGRESS pfnProgress,
                      void *pvUser, unsigned uPercentStart,
                      unsigned uPercentSpan, PFNVDERROR pfnError,
                      void *pvErrorUser, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" enmType=%d cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p uOpenFlags=%#x pfnProgress=%#p pvUser=%#p uPercentStart=%u uPercentSpan=%u pfnError=%#p pvErrorUser=%#p ppBackendData=%#p", pszFilename, enmType, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, uOpenFlags, pfnProgress, pvUser, uPercentStart, uPercentSpan, pfnError, pvErrorUser, ppBackendData));
    int rc;
    PVMDKIMAGE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || strchr(pszFilename, '"')
        || (enmType != VD_IMAGE_TYPE_NORMAL && enmType != VD_IMAGE_TYPE_FIXED)
        || !VALID_PTR(pPCHSGeometry)
        || !VALID_PTR(pLCHSGeometry))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PVMDKIMAGE)RTMemAllocZ(sizeof(VMDKIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->File = NIL_RTFILE;
    pImage->pExtents = NULL;
    pImage->pFiles = NULL;
    pImage->pGTCache = NULL;
    pImage->pDescData = NULL;
    pImage->pfnError = pfnError;
    pImage->pvErrorUser = pvErrorUser;
    pImage->cbDescAlloc = VMDK_SECTOR2BYTE(20);
    pImage->pDescData = (char *)RTMemAllocZ(pImage->cbDescAlloc);
    if (!pImage->pDescData)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    rc = vmdkCreateImage(pImage, enmType, cbSize, uImageFlags, pszComment,
                         pPCHSGeometry, pLCHSGeometry,
                         pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (VBOX_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            vmdkFreeImage(pImage, false);
            rc = vmdkOpenImage(pImage, uOpenFlags);
            if (VBOX_FAILURE(rc))
                goto out;
        }
        *ppBackendData = pImage;
    }

out:
    LogFlowFunc(("returns %Vrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int vmdkRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int vmdkClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
        vmdkFreeImage(pImage, fDelete);

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int vmdkRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                    size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    PVMDKEXTENT pExtent;
    uint64_t uSectorExtentRel;
    uint64_t uSectorExtentAbs;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = vmdkFindExtent(pImage, VMDK_BYTE2SECTOR(uOffset),
                        &pExtent, &uSectorExtentRel);
    if (VBOX_FAILURE(rc))
        goto out;

    /* Check access permissions as defined in the extent descriptor. */
    if (pExtent->enmAccess == VMDKACCESS_NOACCESS)
    {
        rc = VERR_VDI_INVALID_STATE;
        goto out;
    }

    /* Clip read range to remain in this extent. */
    cbToRead = RT_MIN(cbToRead, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));

    /* Handle the read according to the current extent type. */
    switch (pExtent->enmType)
    {
        case VMDKETYPE_HOSTED_SPARSE:
#ifdef VBOX_WITH_VMDK_ESX
        case VMDKETYPE_ESX_SPARSE:
#endif /* VBOX_WITH_VMDK_ESX */
            rc = vmdkGetSector(pImage->pGTCache, pExtent, uSectorExtentRel,
                               &uSectorExtentAbs);
            if (VBOX_FAILURE(rc))
                goto out;
            /* Clip read range to at most the rest of the grain. */
            cbToRead = RT_MIN(cbToRead, VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain - uSectorExtentRel % pExtent->cSectorsPerGrain));
            Assert(!(cbToRead % 512));
            if (uSectorExtentAbs == 0)
                rc = VERR_VDI_BLOCK_FREE;
            else
                rc = RTFileReadAt(pExtent->File,
                                  VMDK_SECTOR2BYTE(uSectorExtentAbs),
                                  pvBuf, cbToRead, NULL);
            break;
        case VMDKETYPE_FLAT:
            rc = RTFileReadAt(pExtent->File,
                              VMDK_SECTOR2BYTE(uSectorExtentRel),
                              pvBuf, cbToRead, NULL);
            break;
        case VMDKETYPE_ZERO:
            memset(pvBuf, '\0', cbToRead);
            break;
    }
    *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int vmdkWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                     size_t cbToWrite, size_t *pcbWriteProcess,
                     size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n", pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    PVMDKEXTENT pExtent;
    uint64_t uSectorExtentRel;
    uint64_t uSectorExtentAbs;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VDI_IMAGE_READ_ONLY;
        goto out;
    }

    if (cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* No size check here, will do that later when the extent is located.
     * There are sparse images out there which according to the spec are
     * invalid, because the total size is not a multiple of the grain size.
     * Also for sparse images which are stitched together in odd ways (not at
     * grain boundaries, and with the nominal size not being a multiple of the
     * grain size), this would prevent writing to the last grain. */

    rc = vmdkFindExtent(pImage, VMDK_BYTE2SECTOR(uOffset),
                        &pExtent, &uSectorExtentRel);
    if (VBOX_FAILURE(rc))
        goto out;

    /* Check access permissions as defined in the extent descriptor. */
    if (pExtent->enmAccess != VMDKACCESS_READWRITE)
    {
        rc = VERR_VDI_INVALID_STATE;
        goto out;
    }

    /** @todo implement suppressing of zero data writes (a bit tricky in this
     * case, as VMDK has no marker for zero blocks). We somehow need to get the
     * information whether the information in this area is all zeroes as of the
     * parent image. Then (based on the assumption that parent images are
     * immutable) the write can be ignored. */

    /* Handle the write according to the current extent type. */
    switch (pExtent->enmType)
    {
        case VMDKETYPE_HOSTED_SPARSE:
#ifdef VBOX_WITH_VMDK_ESX
        case VMDKETYPE_ESX_SPARSE:
#endif /* VBOX_WITH_VMDK_ESX */
            rc = vmdkGetSector(pImage->pGTCache, pExtent, uSectorExtentRel,
                               &uSectorExtentAbs);
            if (VBOX_FAILURE(rc))
                goto out;
            /* Clip write range to at most the rest of the grain. */
            cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain - uSectorExtentRel % pExtent->cSectorsPerGrain));
            if (uSectorExtentAbs == 0)
            {
                if (cbToWrite == VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain))
                {
                    /* Full block write to a previously unallocated block.
                     * Allocate GT and find out where to store the grain. */
                    rc = vmdkAllocGrain(pImage->pGTCache, pExtent,
                                        uSectorExtentRel, pvBuf, cbToWrite);
                    *pcbPreRead = 0;
                    *pcbPostRead = 0;
                }
                else
                {
                    /* Clip write range to remain in this extent. */
                    cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));
                    *pcbPreRead = VMDK_SECTOR2BYTE(uSectorExtentRel % pExtent->cSectorsPerGrain);
                    *pcbPostRead = VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain) - cbToWrite - *pcbPreRead;
                    rc = VERR_VDI_BLOCK_FREE;
                }
            }
            else
                rc = RTFileWriteAt(pExtent->File,
                                   VMDK_SECTOR2BYTE(uSectorExtentAbs),
                                   pvBuf, cbToWrite, NULL);
            break;
        case VMDKETYPE_FLAT:
            /* Clip write range to remain in this extent. */
            cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));
            rc = RTFileWriteAt(pExtent->File,
                               VMDK_SECTOR2BYTE(uSectorExtentRel),
                               pvBuf, cbToWrite, NULL);
            break;
        case VMDKETYPE_ZERO:
            /* Clip write range to remain in this extent. */
            cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));
            break;
    }
    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int vmdkFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    rc = vmdkFlushImage(pImage);
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned vmdkGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return VMDK_IMAGE_VERSION;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageType */
static int vmdkGetImageType(void *pBackendData, PVDIMAGETYPE penmImageType)
{
    LogFlowFunc(("pBackendData=%#p penmImageType=%#p\n", pBackendData, penmImageType));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    Assert(pImage);
    Assert(penmImageType);

    if (pImage && pImage->cExtents != 0)
        *penmImageType = pImage->enmImageType;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc enmImageType=%u\n", rc, *penmImageType));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t vmdkGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t vmdkGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    uint64_t cb = 0;

    Assert(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->File != NIL_RTFILE)
        {
            int rc = RTFileGetSize(pImage->File, &cbFile);
            if (VBOX_SUCCESS(rc))
                cb += cbFile;
            for (unsigned i = 0; i <= pImage->cExtents; i++)
            {
                rc = RTFileGetSize(pImage->File, &cbFile);
                if (VBOX_SUCCESS(rc))
                    cb += cbFile;
            }
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int vmdkGetPCHSGeometry(void *pBackendData,
                               PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->PCHSGeometry.cCylinders)
        {
            *pPCHSGeometry = pImage->PCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int vmdkSetPCHSGeometry(void *pBackendData,
                               PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VDI_IMAGE_READ_ONLY;
            goto out;
        }
        rc = vmdkDescSetPCHSGeometry(pImage, pPCHSGeometry);
        if (VBOX_FAILURE(rc))
            goto out;

        pImage->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int vmdkGetLCHSGeometry(void *pBackendData,
                               PPDMMEDIAGEOMETRY pLCHSGeometry)
{
     LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->LCHSGeometry.cCylinders)
        {
            *pLCHSGeometry = pImage->LCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int vmdkSetLCHSGeometry(void *pBackendData,
                               PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VDI_IMAGE_READ_ONLY;
            goto out;
        }
        rc = vmdkDescSetLCHSGeometry(pImage, pLCHSGeometry);
        if (VBOX_FAILURE(rc))
            goto out;

        pImage->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned vmdkGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    unsigned uImageFlags;

    Assert(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned vmdkGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    unsigned uOpenFlags;

    Assert(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int vmdkSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;
    const char *pszFilename;

    /* Image must be opened and the new flags must be valid. Just readonly flag
     * is supported. */
    if (!pImage || uOpenFlags & ~VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    pszFilename = pImage->pszFilename;
    vmdkFreeImage(pImage, false);
    rc = vmdkOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int vmdkGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        const char *pszCommentEncoded = NULL;
        rc = vmdkDescDDBGetStr(pImage, &pImage->Descriptor,
                              "ddb.comment", &pszCommentEncoded);
        if (rc == VERR_VDI_VALUE_NOT_FOUND)
            pszCommentEncoded = NULL;
        else if (VBOX_FAILURE(rc))
            goto out;

        if (pszComment)
            rc = vmdkDecodeString(pszCommentEncoded, pszComment, cbComment);
        else
            rc = VINF_SUCCESS;
        if (pszCommentEncoded)
            RTStrFree((char *)(void *)pszCommentEncoded);
    }
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int vmdkSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VDI_IMAGE_READ_ONLY;
        goto out;
    }

    if (pImage)
        rc = vmdkSetImageComment(pImage, pszComment);
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int vmdkGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        *pUuid = pImage->ImageUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int vmdkSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%Vuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pImage->ImageUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_IMAGE_UUID, pUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing image UUID in descriptor in '%s'"), pImage->pszFilename);
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int vmdkGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        *pUuid = pImage->ModificationUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int vmdkSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pImage->ModificationUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_MODIFICATION_UUID, pUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing modification UUID in descriptor in '%s'"), pImage->pszFilename);
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int vmdkGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        *pUuid = pImage->ParentUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int vmdkSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pImage->ParentUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_UUID, pUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent image UUID in descriptor in '%s'"), pImage->pszFilename);
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int vmdkGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        *pUuid = pImage->ParentModificationUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int vmdkSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pImage->ParentModificationUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_MODIFICATION_UUID, pUuid);
            if (VBOX_FAILURE(rc))
                return vmdkError(pImage, rc, RT_SRC_POS, N_("VMDK: error storing parent image UUID in descriptor in '%s'"), pImage->pszFilename);
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnDump */
static void vmdkDump(void *pBackendData)
{
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    Assert(pImage);
    if (pImage)
    {
        RTLogPrintf("Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                    pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                    pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                    VMDK_BYTE2SECTOR(pImage->cbSize));
        RTLogPrintf("Header: uuidCreation={%Vuuid}\n", &pImage->ImageUuid);
        RTLogPrintf("Header: uuidModification={%Vuuid}\n", &pImage->ModificationUuid);
        RTLogPrintf("Header: uuidParent={%Vuuid}\n", &pImage->ParentUuid);
        RTLogPrintf("Header: uuidParentModification={%Vuuid}\n", &pImage->ParentModificationUuid);
    }
}


VBOXHDDBACKEND g_VmdkBackend =
{
    /* pszBackendName */
    "VMDK",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
      VD_CAP_UUID | VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC
    | VD_CAP_CREATE_SPLIT_2G | VD_CAP_DIFF,
    /* pfnCheckIfValid */
    vmdkCheckIfValid,
    /* pfnOpen */
    vmdkOpen,
    /* pfnCreate */
    vmdkCreate,
    /* pfnRename */
    vmdkRename,
    /* pfnClose */
    vmdkClose,
    /* pfnRead */
    vmdkRead,
    /* pfnWrite */
    vmdkWrite,
    /* pfnFlush */
    vmdkFlush,
    /* pfnGetVersion */
    vmdkGetVersion,
    /* pfnGetImageType */
    vmdkGetImageType,
    /* pfnGetSize */
    vmdkGetSize,
    /* pfnGetFileSize */
    vmdkGetFileSize,
    /* pfnGetPCHSGeometry */
    vmdkGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    vmdkSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    vmdkGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    vmdkSetLCHSGeometry,
    /* pfnGetImageFlags */
    vmdkGetImageFlags,
    /* pfnGetOpenFlags */
    vmdkGetOpenFlags,
    /* pfnSetOpenFlags */
    vmdkSetOpenFlags,
    /* pfnGetComment */
    vmdkGetComment,
    /* pfnSetComment */
    vmdkSetComment,
    /* pfnGetUuid */
    vmdkGetUuid,
    /* pfnSetUuid */
    vmdkSetUuid,
    /* pfnGetModificationUuid */
    vmdkGetModificationUuid,
    /* pfnSetModificationUuid */
    vmdkSetModificationUuid,
    /* pfnGetParentUuid */
    vmdkGetParentUuid,
    /* pfnSetParentUuid */
    vmdkSetParentUuid,
    /* pfnGetParentModificationUuid */
    vmdkGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    vmdkSetParentModificationUuid,
    /* pfnDump */
    vmdkDump
};
