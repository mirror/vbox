/* $Id$ */
/** @file
 * IPRT - ISO Image Maker.
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
#include <iprt/fsvfs.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/formats/iso9660.h>

#include <internal/magics.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Asserts valid handle, returns VERR_INVALID_HANDLE if not. */
#define RTFSISOMAKER_ASSER_VALID_HANDLE_RET(a_pThis) \
    do { AssertPtrReturn(a_pThis, VERR_INVALID_HANDLE); \
         AssertPtrReturn((a_pThis)->uMagic == RTFSISOMAKERINT_MAGIC, VERR_INVALID_HANDLE); \
    } while (0)

/** The sector size. */
#define RTFSISOMAKER_SECTOR_SIZE            _2K
/** Maximum number of objects. */
#define RTFSISOMAKER_MAX_OBJECTS            _16M
/** Maximum number of objects per directory. */
#define RTFSISOMAKER_MAX_OBJECTS_PER_DIR    _256K /**< @todo check limit */

/** UTF-8 name buffer.  */
#define RTFSISOMAKER_MAX_NAME_BUF           768

/** Tests if @a a_ch is in the set of d-characters. */
#define RTFSISOMAKER_IS_IN_D_CHARS(a_ch)        (RT_C_IS_UPPER(a_ch) || RT_C_IS_DIGIT(a_ch) || (a_ch) == '_')

/** Tests if @a a_ch is in the set of d-characters when uppercased. */
#define RTFSISOMAKER_IS_UPPER_IN_D_CHARS(a_ch)  (RT_C_IS_ALNUM(a_ch) || (a_ch) == '_')



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an ISO maker object name space node. */
typedef struct RTFSISOMAKERNAME *PRTFSISOMAKERNAME;
/** Pointer to an ISO maker object name space node pointer. */
typedef PRTFSISOMAKERNAME *PPRTFSISOMAKERNAME;
/** Pointer to a common ISO image maker file system object. */
typedef struct RTFSISOMAKEROBJ *PRTFSISOMAKEROBJ;
/** Pointer to a ISO maker file object. */
typedef struct RTFSISOMAKERFILE *PRTFSISOMAKERFILE;


/** @name RTFSISOMAKER_NAMESPACE_XXX - Namespace selector.
 * @{
 */
#define RTFSISOMAKERNAMESPACE_ISO_9660      RT_BIT_32(0)            /**< The primary ISO-9660 namespace. */
#define RTFSISOMAKERNAMESPACE_JOLIET        RT_BIT_32(1)            /**< The joliet namespace. */
#define RTFSISOMAKERNAMESPACE_UDF           RT_BIT_32(2)            /**< The UDF namespace. */
#define RTFSISOMAKERNAMESPACE_HFS           RT_BIT_32(3)            /**< The HFS namespace */
#define RTFSISOMAKERNAMESPACE_ALL           UINT32_C(0x0000000f)    /**< All namespaces. */
#define RTFSISOMAKERNAMESPACE_VALID_MASK    UINT32_C(0x0000000f)    /**< Valid namespace bits. */
/** @} */


/**
 * Filesystem object type.
 */
typedef enum RTFSISOMAKEROBJTYPE
{
    RTFSISOMAKEROBJTYPE_INVALID = 0,
    RTFSISOMAKEROBJTYPE_DIR,
    RTFSISOMAKEROBJTYPE_FILE,
    //RTFSISOMAKEROBJTYPE_SYMLINK,
    RTFSISOMAKEROBJTYPE_END
} RTFSISOMAKEROBJTYPE;

/**
 * Extra name space information required for directories.
 */
typedef struct RTFSISOMAKERNAMEDIR
{
    /** The location of the directory data. */
    uint64_t                offDir;
    /** The size of the directory. */
    uint32_t                cbDir;
    /** Number of children. */
    uint32_t                cChildren;
    /** Sorted array of children. */
    PPRTFSISOMAKERNAME      papChildren;

    /** The translate table file. */
    PRTFSISOMAKERFILE       pTransTblFile;
} RTFSISOMAKERNAMEDIR;
/** Pointer to directory specfic name space node info. */
typedef RTFSISOMAKERNAMEDIR *PRTFSISOMAKERNAMEDIR;


/**
 * ISO maker object namespace node.
 */
typedef struct RTFSISOMAKERNAME
{
    /** Pointer to the file system object. */
    PRTFSISOMAKEROBJ        pObj;
    /** Pointer to the partent directory, NULL if root dir. */
    PRTFSISOMAKERNAME       pParent;

    /** Pointer to the directory information if this is a directory, NULL if not a
     * directory. This is allocated together with this structure, so it doesn't need
     * freeing. */
    PRTFSISOMAKERNAMEDIR    pDir;

    /** The name specified when creating this namespace node.  Helps navigating
     * the namespace when we mangle or otherwise change the names.
     * Allocated together with of this structure, no spearate free necessary. */
    const char             *pszSpecNm;

    /** Alternative rock ridge name. */
    char                   *pszRockRidgeNm;
    /** Alternative TRANS.TBL name. */
    char                   *pszTransNm;
    /** Length of pszSpecNm. */
    uint16_t                cchSpecNm;
    /** Length of pszRockRidgeNm. */
    uint16_t                cchRockRidgeNm;
    /** Length of pszTransNm. */
    uint16_t                cchTransNm;

    /** The depth in the namespace tree of this name. */
    uint8_t                 uDepth;
    /** Set if pszTransNm is allocated separately.  Normally same as pszSpecNm. */
    bool                    fRockRidgeNmAlloced : 1;
    /** Set if pszTransNm is allocated separately.  Normally same as pszSpecNm. */
    bool                    fTransNmAlloced : 1;

    /** The mode mask.
     * Starts out as a copy of RTFSISOMAKEROBJ::fMode. */
    RTFMODE                 fMode;
    /** The owner ID.
     * Starts out as a copy of RTFSISOMAKEROBJ::uid. */
    RTUID                   uid;
    /** The group ID.
     * Starts out as a copy of RTFSISOMAKEROBJ::gid. */
    RTGID                   gid;
    /** The device number if a character or block device.
     * This is for Rock Ridge.  */
    RTDEV                   Device;
    /** The number of hardlinks to report in the file stats.
     * This is for Rock Ridge.  */
    uint32_t                cHardlinks;
    /** The inode/serial number.
     * This is for Rock Ridge.  */
    uint64_t                INode;

    /** The name length. */
    uint16_t                cchName;
    /** The name. */
    char                    szName[RT_FLEXIBLE_ARRAY];
} RTFSISOMAKERNAME;

/**
 * A ISO maker namespace.
 */
typedef struct RTFSISOMAKERNAMESPACE
{
    /** The namespace root. */
    PRTFSISOMAKERNAME       pRoot;
    /** Total number of name nodes in the namespace. */
    uint32_t                cNames;
    /** Total number of directories in the namespace. */
    uint32_t                cDirs;
    /** The namespace selector (RTFSISOMAKER_NAMESPACE_XXX). */
    uint32_t                fNamespace;
    /** Offset into RTFSISOMAKERNAMESPACE of the name member. */
    uint32_t                offName;
    /** The configuration level for this name space.
     *     - For UDF and HFS namespaces this is either @c true or @c false.
     *     - For the primary ISO-9660 namespace this is 1, 2, or 3.
     *     - For the joliet namespace this 0 (joliet disabled), 1, 2, or 3. */
    uint8_t                 uLevel;
    /** The rock ridge level: 1 - enabled; 2 - with ER tag.
     * Linux behaves a little different when seeing the ER tag. */
    uint8_t                 uRockRidgeLevel;
    /** Set if TRANS.TBL files are to be generated for this namespace. */
    bool                    fTransTbl;
} RTFSISOMAKERNAMESPACE;
/** Pointer to a namespace. */
typedef RTFSISOMAKERNAMESPACE *PRTFSISOMAKERNAMESPACE;
/** Pointer to a const namespace. */
typedef RTFSISOMAKERNAMESPACE const *PCRTFSISOMAKERNAMESPACE;


/**
 * Common base structure for the file system objects.
 *
 * The times are shared across all namespaces, while the uid, gid and mode are
 * duplicates in each namespace.
 */
typedef struct RTFSISOMAKEROBJ
{
    /** The linear list entry of the image content. */
    RTLISTNODE              Entry;
    /** The object index. */
    uint32_t                idxObj;
    /** The type of this object. */
    RTFSISOMAKEROBJTYPE     enmType;

    /** The primary ISO-9660 name space name. */
    PRTFSISOMAKERNAME       pPrimaryName;
    /** The joliet name space name. */
    PRTFSISOMAKERNAME       pJolietName;
    /** The UDF name space name. */
    PRTFSISOMAKERNAME       pUdfName;
    /** The HFS name space name. */
    PRTFSISOMAKERNAME       pHfsName;

    /** Birth (creation) time. */
    RTTIMESPEC              BirthTime;
    /** Attribute change time. */
    RTTIMESPEC              ChangeTime;
    /** Modification time. */
    RTTIMESPEC              ModificationTime;
    /** Accessed time. */
    RTTIMESPEC              AccessedTime;

    /** Owner ID. */
    RTUID                   uid;
    /** Group ID. */
    RTGID                   gid;
    /** Attributes (unix permissions bits mainly). */
    RTFMODE                 fMode;

} RTFSISOMAKEROBJ;


/**
 * File source type.
 */
typedef enum RTFSISOMAKERSRCTYPE
{
    RTFSISOMAKERSRCTYPE_INVALID = 0,
    RTFSISOMAKERSRCTYPE_PATH,
    RTFSISOMAKERSRCTYPE_VFS_FILE,
    RTFSISOMAKERSRCTYPE_END
} RTFSISOMAKERSRCTYPE;

/**
 * ISO maker file object.
 */
typedef struct RTFSISOMAKERFILE
{
    /** The common bit. */
    RTFSISOMAKEROBJ         Core;
    /** The file data size. */
    uint64_t                cbData;
    /** Byte offset of the data in the image. */
    uint64_t                offData;

    /** The type of source object. */
    RTFSISOMAKERSRCTYPE     enmSrcType;
    /** The source data. */
    union
    {
        /** Path to the source file.
         * Allocated together with this structure.  */
        const char         *pszSrcPath;
        /** Source VFS file. */
        RTVFSFILE           hVfsFile;
    } u;
} RTFSISOMAKERFILE;


/**
 * ISO maker directory object.
 *
 * Unlike files, the allocation info is name space specific and lives in the
 * corresponding RTFSISOMAKERNAMEDIR structures.
 */
typedef struct RTFSISOMAKERDIR
{
    /** The common bit. */
    RTFSISOMAKEROBJ         Core;
} RTFSISOMAKERDIR;
/** Pointer to an ISO maker directory object.  */
typedef RTFSISOMAKERDIR *PRTFSISOMAKERDIR;



/**
 * Instance data for a ISO image maker.
 */
typedef struct RTFSISOMAKERINT
{
    /** Magic value (RTFSISOMAKERINT_MAGIC). */
    uint32_t                uMagic;
    /** Reference counter. */
    uint32_t volatile       cRefs;

    /** Set after we've been fed the first bit of content.
     * This means that the namespace configuration has been finalized and can no
     * longer be changed because it's simply too much work to do adjustments
     * after having started to add files. */
    bool                    fSeenContent;

    /** The primary ISO-9660 namespace. */
    RTFSISOMAKERNAMESPACE   PrimaryIso;
    /** The joliet namespace. */
    RTFSISOMAKERNAMESPACE   Joliet;
    /** The UDF namespace. */
    RTFSISOMAKERNAMESPACE   Udf;
    /** The hybrid HFS+ namespace. */
    RTFSISOMAKERNAMESPACE   Hfs;

    /** The list of objects (RTFSISOMAKEROBJ). */
    RTLISTANCHOR            ObjectHead;
    /** Number of objects in the image (ObjectHead).
     * This is used to number them, i.e. create RTFSISOMAKEROBJ::idxObj.  */
    uint32_t                cObjects;

    /** Amount of file data. */
    uint64_t                cbData;
    /** The total image size.
     * @todo not sure if this is desirable.  */
    uint64_t                cbTotal;

    /** The 'now' timestamp we use for the whole image.
     * This way we'll save lots of RTTimeNow calls and have similar timestamps
     * over the whole image. */
    RTTIMESPEC              ImageCreationTime;
    /** The default owner ID. */
    RTUID                   uidDefault;
    /** The default group ID. */
    RTGID                   gidDefault;
    /** The default file mode mask. */
    RTFMODE                 fDefaultFileMode;
    /** The default file mode mask. */
    RTFMODE                 fDefaultDirMode;

} RTFSISOMAKERINT;
/** Pointer to an ISO maker instance. */
typedef RTFSISOMAKERINT *PRTFSISOMAKERINT;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Help for iterating over namespaces.
 */
static const struct
{
    /** The RTFSISOMAKERNAMESPACE_XXX indicator.  */
    uint32_t        fNamespace;
    /** Offset into RTFSISOMAKERINT of the namespace member. */
    uintptr_t       offNamespace;
    /** Offset into RTFSISOMAKERNAMESPACE of the name member. */
    uintptr_t       offName;
    /** Namespace name for debugging purposes. */
    const char     *pszName;
} g_aRTFsIsoNamespaces[] =
{
    {   RTFSISOMAKERNAMESPACE_ISO_9660, RT_OFFSETOF(RTFSISOMAKERINT, PrimaryIso), RT_OFFSETOF(RTFSISOMAKEROBJ, pPrimaryName), "iso-9660" },
    {   RTFSISOMAKERNAMESPACE_JOLIET,   RT_OFFSETOF(RTFSISOMAKERINT, Joliet),     RT_OFFSETOF(RTFSISOMAKEROBJ, pJolietName),  "joliet" },
    {   RTFSISOMAKERNAMESPACE_UDF,      RT_OFFSETOF(RTFSISOMAKERINT, Udf),        RT_OFFSETOF(RTFSISOMAKEROBJ, pUdfName),     "udf" },
    {   RTFSISOMAKERNAMESPACE_HFS,      RT_OFFSETOF(RTFSISOMAKERINT, Hfs),        RT_OFFSETOF(RTFSISOMAKEROBJ, pHfsName),     "hfs" },
};

/**
 * Translates a single namespace flag (RTFSISOMAKERNAMESPACE_XXX) to an
 * index into g_aRTFsIsoNamespaces.
 */
static const uint8_t g_aidxRTFsIsoNamespaceFlagToIdx[] =
{
    /*[0]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKERNAMESPACE_ISO_9660] = */ 0,
    /*[2]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[3]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKERNAMESPACE_JOLIET]   = */ 1,
    /*[5]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[6]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[7]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKERNAMESPACE_UDF]      = */ 2,
    /*[9]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[10]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[11]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[12]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[13]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[14]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[15]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKERNAMESPACE_HFS]      = */ 3,
    /*[17]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[18]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[19]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[20]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[21]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[22]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[23]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[24]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[25]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[26]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[27]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[28]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[29]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[30]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[31]                             = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int rtFsIsoMakerAddUnnamedDirWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKERDIR *ppDir);

RTDECL(int) RTFsIsoMakerObjSetPath(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t fNamespaces, const char *pszPath);


/**
 * Creates an ISO maker instance.
 *
 * @returns IPRT status code.
 * @param   phIsoMaker          Where to return the handle to the new ISO maker.
 */
RTDECL(int) RTFsIsoMakerCreate(PRTFSISOMAKER phIsoMaker)
{
    /*
     * Do some integrity checks first.
     */
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKERNAMESPACE_ISO_9660]].fNamespace == RTFSISOMAKERNAMESPACE_ISO_9660,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKERNAMESPACE_JOLIET]].fNamespace   == RTFSISOMAKERNAMESPACE_JOLIET,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKERNAMESPACE_UDF]].fNamespace      == RTFSISOMAKERNAMESPACE_UDF,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKERNAMESPACE_HFS]].fNamespace      == RTFSISOMAKERNAMESPACE_HFS,
                 VERR_INTERNAL_ERROR_5);

    /*
     * Create the instance with defaults.
     */
    PRTFSISOMAKERINT pThis = (PRTFSISOMAKERINT)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->uMagic                       = RTFSISOMAKERINT_MAGIC;
        pThis->cRefs                        = 1;
        //pThis->fSeenContent               = false;

        pThis->PrimaryIso.fNamespace        = RTFSISOMAKERNAMESPACE_ISO_9660;
        pThis->PrimaryIso.offName           = RT_OFFSETOF(RTFSISOMAKEROBJ, pPrimaryName);
        pThis->PrimaryIso.uLevel            = 3; /* 30 char names, large files */
        pThis->PrimaryIso.uRockRidgeLevel   = 1;
        pThis->PrimaryIso.fTransTbl         = true;
        pThis->Joliet.fNamespace            = RTFSISOMAKERNAMESPACE_JOLIET;
        pThis->Joliet.offName               = RT_OFFSETOF(RTFSISOMAKEROBJ, pJolietName);
        pThis->Joliet.uLevel                = 3;
        //pThis->Joliet.uRockRidgeLevel     = 0;
        //pThis->Joliet.fTransTbl           = false;
        pThis->Udf.fNamespace               = RTFSISOMAKERNAMESPACE_UDF;
        pThis->Udf.offName                  = RT_OFFSETOF(RTFSISOMAKEROBJ, pUdfName);
        //pThis->Udf.uLevel                 = 0;
        //pThis->Udf.uRockRidgeLevel        = 0;
        //pThis->Udf.fTransTbl              = false;
        pThis->Hfs.fNamespace               = RTFSISOMAKERNAMESPACE_HFS;
        pThis->Hfs.offName                  = RT_OFFSETOF(RTFSISOMAKEROBJ, pHfsName);
        //pThis->Hfs.uLevel                 = 0;
        //pThis->Hfs.uRockRidgeLevel        = 0;
        //pThis->Hfs.fTransTbl              = false;

        RTListInit(&pThis->ObjectHead);
        //pThis->cObjects                   = 0;
        //pThis->cbData                     = 0;

        pThis->cbTotal                      = _32K /* The system area size. */
                                            + RTFSISOMAKER_SECTOR_SIZE /* Primary volume descriptor. */
                                            + RTFSISOMAKER_SECTOR_SIZE /* Secondary volume descriptor for joliet. */
                                            + RTFSISOMAKER_SECTOR_SIZE /* Terminator descriptor. */;

        //pThis->uidDefault                 = 0;
        //pThis->gidDefault                 = 0;
        pThis->fDefaultFileMode             = 0444 | RTFS_TYPE_FILE      | RTFS_DOS_ARCHIVED  | RTFS_DOS_READONLY;
        pThis->fDefaultDirMode              = 0555 | RTFS_TYPE_DIRECTORY | RTFS_DOS_DIRECTORY | RTFS_DOS_READONLY;

        RTTimeNow(&pThis->ImageCreationTime);

        *phIsoMaker = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Frees an object.
 *
 * This is a worker for rtFsIsoMakerDestroy and RTFsIsoMakerObjRemove.
 *
 * @param   pObj                The object to free.
 */
DECLINLINE(void) rtFsIsoMakerObjDestroy(PRTFSISOMAKEROBJ pObj)
{
    if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
    {
        PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
        switch (pFile->enmSrcType)
        {
            case RTFSISOMAKERSRCTYPE_PATH:
                /* do nothing. */
                break;

            case RTFSISOMAKERSRCTYPE_VFS_FILE:
                RTVfsFileRelease(pFile->u.hVfsFile);
                pFile->u.hVfsFile = NIL_RTVFSFILE;
                break;

            case RTFSISOMAKERSRCTYPE_INVALID:
            case RTFSISOMAKERSRCTYPE_END:
                AssertFailed();
                break;

            /* no default, want warnings */
        }
    }

    RTMemFree(pObj);
}


/**
 * Frees a namespace node.
 *
 * This is a worker for rtFsIsoMakerDestroyTree and rtFsIsoMakerObjUnsetName.
 *
 * @param   pName               The node to free.
 */
DECLINLINE(void) rtFsIsoMakerDestroyName(PRTFSISOMAKERNAME pName)
{
    if (pName->fRockRidgeNmAlloced)
    {
        RTMemFree(pName->pszRockRidgeNm);
        pName->pszRockRidgeNm = NULL;
    }
    if (pName->fTransNmAlloced)
    {
        RTMemFree(pName->pszTransNm);
        pName->pszTransNm = NULL;
    }
    RTMemFree(pName);
}


/**
 * Recursively destroy a name space tree.
 * @param   pRoot               The root node.
 */
static void rtFsIsoMakerDestroyTree(PRTFSISOMAKERNAME pRoot)
{
    Assert(!pRoot->pParent);
    PRTFSISOMAKERNAME pCur = pRoot;

    for (;;)
    {
        if (   pCur->pDir
            && pCur->pDir->cChildren)
            pCur = pCur->pDir->papChildren[pCur->pDir->cChildren - 1];
        else
        {
            PRTFSISOMAKERNAME pNext = pCur->pParent;
            rtFsIsoMakerDestroyName(pCur);

            /* Unlink from parent, we're the last entry. */
            if (pNext)
            {
                Assert(pNext->pDir->cChildren > 0);
                pNext->pDir->cChildren--;
                Assert(pNext->pDir->papChildren[pNext->pDir->cChildren] == pCur);
                pNext->pDir->papChildren[pNext->pDir->cChildren] = NULL;
                pCur = pNext;
            }
            else
            {
                Assert(pRoot == pCur);
                break;
            }
        }
    }
}


/**
 * Destroys an ISO maker instance.
 *
 * @param   pThis               The ISO maker instance to destroy.
 */
static void rtFsIsoMakerDestroy(PRTFSISOMAKERINT pThis)
{
    rtFsIsoMakerDestroyTree(pThis->PrimaryIso.pRoot);
    rtFsIsoMakerDestroyTree(pThis->Joliet.pRoot);
    rtFsIsoMakerDestroyTree(pThis->Udf.pRoot);
    rtFsIsoMakerDestroyTree(pThis->Hfs.pRoot);

    PRTFSISOMAKEROBJ pCur;
    PRTFSISOMAKEROBJ pNext;
    RTListForEachSafe(&pThis->ObjectHead, pCur, pNext, RTFSISOMAKEROBJ, Entry)
    {
        RTListNodeRemove(&pCur->Entry);
        rtFsIsoMakerObjDestroy(pCur);
    }
}


/**
 * Retains a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.
 */
RTDECL(uint32_t) RTFsIsoMakerRetain(RTFSISOMAKER hIsoMaker)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTFSISOMAKERINT_MAGIC, UINT32_MAX);
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < _64K);
    return cRefs;
}


/**
 * Releases a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.  NIL is ignored.
 */
RTDECL(uint32_t) RTFsIsoMakerRelease(RTFSISOMAKER hIsoMaker)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    uint32_t         cRefs;
    if (pThis == NIL_RTFSISOMAKER)
        cRefs = 0;
    else
    {
        AssertPtrReturn(pThis, UINT32_MAX);
        AssertReturn(pThis->uMagic == RTFSISOMAKERINT_MAGIC, UINT32_MAX);
        cRefs = ASMAtomicDecU32(&pThis->cRefs);
        Assert(cRefs < _64K);
        if (!cRefs)
            rtFsIsoMakerDestroy(pThis);
    }
    return cRefs;
}


/**
 * Sets the ISO-9660 level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uIsoLevel           The level, 1-3.
 */
RTDECL(int) RTFsIsoMakerSetIso9660Level(RTFSISOMAKER hIsoMaker, uint8_t uIsoLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(uIsoLevel <= 3, VERR_INVALID_PARAMETER);
    AssertReturn(uIsoLevel > 0, VERR_INVALID_PARAMETER); /* currently not possible to disable this */
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);

    pThis->PrimaryIso.uLevel = uIsoLevel;
    return VINF_SUCCESS;
}


/**
 * Sets the joliet level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uJolietLevel        The joliet UCS-2 level 1-3, or 0 to disable
 *                              joliet.
 */
RTDECL(int) RTFsIsoMakerSetJolietUcs2Level(RTFSISOMAKER hIsoMaker, uint8_t uJolietLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(uJolietLevel <= 3, VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);

    if (pThis->Joliet.uLevel != uJolietLevel)
    {
        if (uJolietLevel == 0)
            pThis->cbTotal -= RTFSISOMAKER_SECTOR_SIZE;
        else if (pThis->Joliet.uLevel == 0)
            pThis->cbTotal += RTFSISOMAKER_SECTOR_SIZE;
        pThis->Joliet.uLevel = uJolietLevel;
    }
    return VINF_SUCCESS;
}


/**
 * Sets the rock ridge support level (on the primary ISO-9660 namespace).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);
    AssertReturn(uLevel <= 2, VERR_INVALID_PARAMETER);

    pThis->PrimaryIso.uRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}


/**
 * Sets the rock ridge support level on the joliet namespace (experimental).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetJolietRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);
    AssertReturn(uLevel <= 2, VERR_INVALID_PARAMETER);

    pThis->Joliet.uRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}



/*
 *
 * Name space related internals.
 * Name space related internals.
 * Name space related internals.
 *
 */

#if 0
/**
 * Gets the pointer to the root member for the given namespace.
 *
 * @returns Pointer to root pointer.
 * @param   pThis               The ISO maker instance.
 * @param   fNamespace          The namespace which name to find.
 */
static PPRTFSISOMAKERNAME rtFsIsoMakerGetRootForNamespace(PRTFSISOMAKERINT pThis, uint32_t fNamespace)
{
    Assert(RT_IS_POWER_OF_TWO(fNamespace));
    Assert(fNamespace);
    Assert(fNamespace <= RTFSISOMAKERNAMESPACE_HFS);
    return (PPRTFSISOMAKERNAME)((uintptr_t)pThis + g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[fNamespace]].offRoot);
}
#endif


/**
 * Gets the pointer to the name member for the given namespace.
 *
 * @returns Pointer to name member.
 * @param   pObj                The object to find a name member in.
 * @param   pNamespace          The namespace which name to calculate.
 */
DECLINLINE(PPRTFSISOMAKERNAME) rtFsIsoMakerObjGetNameForNamespace(PRTFSISOMAKEROBJ pObj, PCRTFSISOMAKERNAMESPACE pNamespace)
{
    return (PPRTFSISOMAKERNAME)((uintptr_t)pObj + pNamespace->offName);
}


/**
 * Locates a child object by its namespace name.
 *
 * @returns Pointer to the child if found, NULL if not.
 * @param   pDirObj             The directory object to search.
 * @param   pszEntry            The (namespace) entry name.
 * @param   cchEntry            The length of the name.
 */
static PRTFSISOMAKERNAME rtFsIsoMakerFindObjInDir(PRTFSISOMAKERNAME pDirObj, const char *pszEntry, size_t cchEntry)
{
    if (pDirObj)
    {
        PRTFSISOMAKERNAMEDIR pDir = pDirObj->pDir;
        AssertReturn(pDir, NULL);

        uint32_t i = pDir->cChildren;
        while (i-- > 0)
        {
            PRTFSISOMAKERNAME pChild = pDir->papChildren[i];
            if (   pChild->cchName == cchEntry
                && RTStrNICmp(pChild->szName, pszEntry, cchEntry) == 0)
                return pChild;
        }
    }
    return NULL;
}


/**
 * Locates a child entry by its specified name.
 *
 * @returns Pointer to the child if found, NULL if not.
 * @param   pDirName            The directory name to search.
 * @param   pszEntry            The (specified) entry name.
 * @param   cchEntry            The length of the name.
 */
static PRTFSISOMAKERNAME rtFsIsoMakerFindEntryInDirBySpec(PRTFSISOMAKERNAME pDirName, const char *pszEntry, size_t cchEntry)
{
    if (pDirName)
    {
        PRTFSISOMAKERNAMEDIR pDir = pDirName->pDir;
        AssertReturn(pDir, NULL);

        uint32_t i = pDir->cChildren;
        while (i-- > 0)
        {
            PRTFSISOMAKERNAME pChild = pDir->papChildren[i];
            if (   pChild->cchSpecNm == cchEntry
                && RTStrNICmp(pChild->pszSpecNm, pszEntry, cchEntry) == 0)
                return pChild;
        }
    }
    return NULL;
}


/**
 * Locates a subdir object in any namespace by its specified name.
 *
 * This is used to avoid having one instance of RTFSISOMAKERDIR in each
 * namespace for the same directory.
 *
 * @returns Pointer to the subdir object if found, NULL if not.
 * @param   pDirObj             The directory object to search.
 * @param   pszEntry            The (specified) entry name.
 * @param   cchEntry            The length of the name.
 * @param   fSkipNamespaces     Namespaces to skip.
 * @sa      rtFsIsoMakerFindEntryInDirBySpec
 */
static PRTFSISOMAKERDIR rtFsIsoMakerFindSubdirBySpec(PRTFSISOMAKERDIR pDirObj, const char *pszEntry, size_t cchEntry,
                                                     uint32_t fSkipNamespaces)
{
    AssertReturn(pDirObj, NULL);
    AssertReturn(pDirObj->Core.enmType == RTFSISOMAKEROBJTYPE_DIR, NULL);
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (!(fSkipNamespaces & g_aRTFsIsoNamespaces[i].fNamespace))
        {
            PRTFSISOMAKERNAME pDirName = *(PPRTFSISOMAKERNAME)((uintptr_t)pDirObj + g_aRTFsIsoNamespaces[i].offName);
            if (pDirName)
            {
                PRTFSISOMAKERNAMEDIR pDir = pDirName->pDir;
                AssertStmt(pDir, continue);

                uint32_t i = pDir->cChildren;
                while (i-- > 0)
                {
                    PRTFSISOMAKERNAME pChild = pDir->papChildren[i];
                    if (   pChild->cchSpecNm == cchEntry
                        && pChild->pDir      != NULL
                        && RTStrNICmp(pChild->pszSpecNm, pszEntry, cchEntry) == 0)
                        return (PRTFSISOMAKERDIR)pChild->pObj;
                }
            }
        }
    return NULL;
}


/**
 * Copy and convert a name to valid ISO-9660 (d-characters only).
 *
 * Worker for rtFsIsoMakerNormalizeNameForNamespace.  ASSUMES it deals with
 * dots.
 *
 * @returns Length of the resulting string.
 * @param   pszDst      The output buffer.
 * @param   cchDstMax   The maximum number of (d-chars) to put in the output
 *                      buffer.
 * @param   pchSrc      The UTF-8 source string (not neccessarily terminated).
 * @param   cchSrc      The maximum number of chars to copy from the source
 *                      string.
 */
static size_t rtFsIsoMakerCopyIso9660Name(char *pszDst, size_t cchDstMax, const char *pchSrc, size_t cchSrc)
{
    const char *pchSrcIn = pchSrc;
    size_t      offDst = 0;
    while ((size_t)(pchSrc - pchSrcIn) < cchSrc)
    {
        RTUNICP uc;
        int rc = RTStrGetCpEx(&pchSrc, &uc);
        if (RT_SUCCESS(rc))
        {
            if (   uc < 128
                && RTFSISOMAKER_IS_UPPER_IN_D_CHARS((char)uc))
            {
                pszDst[offDst++] = RT_C_TO_UPPER((char)uc);
                if (offDst >= cchDstMax)
                    break;
            }
        }
    }
    pszDst[offDst] = '\0';
    return offDst;
}


/**
 * Normalizes a name for the primary ISO-9660 namespace.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pParent     The parent directory.  NULL if root.
 * @param   pchSrc      The specified name to normalize (not necessarily zero
 *                      terminated).
 * @param   cchSrc      The length of the specified name.
 * @param   fIsDir      Indicates whether it's a directory or file (like).
 * @param   pszDst      The output buffer.  Must be at least 32 bytes.
 * @param   cbDst       The size of the output buffer.
 * @param   pcchDst     Where to return the length of the returned string (i.e.
 *                      not counting the terminator).
 */
static int rtFsIsoMakerNormalizeNameForPrimaryIso9660(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAME pParent,
                                                      const char *pchSrc, size_t cchSrc, bool fIsDir,
                                                      char *pszDst, size_t cbDst, size_t *pcchDst)
{
    AssertReturn(cbDst > ISO9660_MAX_NAME_LEN, VERR_INTERNAL_ERROR_3);

    /* Skip leading dots. */
    while (cchSrc > 0 && *pchSrc == '.')
        pchSrc++, cchSrc--;
    if (!cchSrc)
    {
        pchSrc = "DOTS";
        cchSrc = 4;
    }

    /*
     * Produce a first name.
     */
    uint8_t const uIsoLevel = pThis->PrimaryIso.uLevel;
    size_t cchDst;
    size_t offDstDot;
    if (fIsDir)
        offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8,
                                                         pchSrc, cchSrc);
    else
    {
        /* Look for the last dot and try preserve the extension when doing the conversion. */
        size_t offLastDot = cchSrc;
        for (size_t off = 0; off < cchSrc; off++)
            if (pchSrc[off] == '.')
                offLastDot = off;

        if (offLastDot == cchSrc)
            offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8,
                                                             pchSrc, cchSrc);
        else
        {
            const char * const pchSrcExt = &pchSrc[offLastDot + 1];
            size_t       const cchSrcExt = cchSrc - offLastDot - 1;
            if (uIsoLevel < 2)
            {
                cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, 8, pchSrc, cchSrc);
                offDstDot = cchDst;
                pszDst[cchDst++] = '.';
                cchDst += rtFsIsoMakerCopyIso9660Name(&pszDst[cchDst], 3, pchSrcExt, cchSrcExt);
            }
            else
            {
                size_t cchDstExt = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN - 2, pchSrcExt, cchSrcExt);
                if (cchDstExt > 0)
                {
                    size_t cchBasename = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN - 2,
                                                                     pchSrc, offLastDot);
                    if (cchBasename + 1 + cchDstExt <= ISO9660_MAX_NAME_LEN)
                        cchDst = cchBasename;
                    else
                        cchDst = ISO9660_MAX_NAME_LEN - 1 - RT_MIN(cchDstExt, 4);
                    offDstDot = cchDst;
                    pszDst[cchDst++] = '.';
                    cchDst += rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN - 1 - cchDst,
                                                          pchSrcExt, cchSrcExt);
                }
                else
                    offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN, pchSrc, cchSrc);
            }
        }
    }

    /*
     * Unique name?
     */
    if (!rtFsIsoMakerFindObjInDir(pParent, pszDst, cchDst))
    {
        *pcchDst = cchDst;
        return VINF_SUCCESS;
    }

    /*
     * Mangle the name till we've got a unique one.
     */
    size_t const cchMaxBasename = (uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8) - (cchDst - offDstDot);
    size_t       cchInserted = 0;
    for (uint32_t i = 0; i < _32K; i++)
    {
        /* Add a numberic infix. */
        char szOrd[64];
        size_t cchOrd = RTStrFormatU32(szOrd, sizeof(szOrd), i + 1, 10, -1, -1, 0 /*fFlags*/);
        Assert((ssize_t)cchOrd > 0);

        /* Do we need to shuffle the suffix? */
        if (cchOrd > cchInserted)
        {
            if (offDstDot < cchMaxBasename)
            {
                memmove(&pszDst[offDstDot + 1], &pszDst[offDstDot], cchDst + 1 - offDstDot);
                cchDst++;
                offDstDot++;
            }
            cchInserted = cchOrd;
        }

        /* Insert the new infix and try again. */
        memcpy(&pszDst[offDstDot - cchOrd], szOrd, cchOrd);
        if (!rtFsIsoMakerFindObjInDir(pParent, pszDst, cchDst))
        {
            *pcchDst = cchDst;
            return VINF_SUCCESS;
        }
    }
    AssertFailed();
    return VERR_DUPLICATE;
}


/**
 * Normalizes a name for the specified name space.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace which rules to normalize it according to.
 * @param   pParent     The parent directory.  NULL if root.
 * @param   pchSrc      The specified name to normalize (not necessarily zero
 *                      terminated).
 * @param   cchSrc      The length of the specified name.
 * @param   fIsDir      Indicates whether it's a directory or file (like).
 * @param   pszDst      The output buffer.  Must be at least 32 bytes.
 * @param   cbDst       The size of the output buffer.
 * @param   pcchDst     Where to return the length of the returned string (i.e.
 *                      not counting the terminator).
 */
static int rtFsIsoMakerNormalizeNameForNamespace(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                                 PRTFSISOMAKERNAME pParent, const char *pchSrc, size_t cchSrc, bool fIsDir,
                                                 char *pszDst, size_t cbDst, size_t *pcchDst)
{
    if (cchSrc > 0)
    {
        /*
         * Check that the object doesn't already exist.
         */
        AssertReturn(!rtFsIsoMakerFindEntryInDirBySpec(pParent, pchSrc, cchSrc), VERR_ALREADY_EXISTS);
        switch (pNamespace->fNamespace)
        {
            /*
             * This one is a lot of work, so separate function.
             */
            case RTFSISOMAKERNAMESPACE_ISO_9660:
                return rtFsIsoMakerNormalizeNameForPrimaryIso9660(pThis, pParent, pchSrc, cchSrc, fIsDir, pszDst, cbDst, pcchDst);

            /*
             * At the moment we don't give darn about UCS-2 limitations here...
             */
            case RTFSISOMAKERNAMESPACE_JOLIET:
            {
/** @todo Joliet name limit and check for duplicates.   */
                AssertReturn(cbDst > cchSrc, VERR_BUFFER_OVERFLOW);
                memcpy(pszDst, pchSrc, cchSrc);
                pszDst[cchSrc] = '\0';
                *pcchDst = cchSrc;
                return VINF_SUCCESS;
            }

            case RTFSISOMAKERNAMESPACE_UDF:
            case RTFSISOMAKERNAMESPACE_HFS:
                AssertFailedReturn(VERR_NOT_IMPLEMENTED);

            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_2);
        }
    }
    else
    {
        /*
         * Root special case.
         */
        AssertReturn(pParent, VERR_INTERNAL_ERROR_3);
        *pszDst = '\0';
        *pcchDst = 0;
        return VINF_SUCCESS;
    }
}


/**
 * Sets the name of an object in a namespace.
 *
 * The object cannot currently have a name in that namespace.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace.
 * @param   pObj        The object to name.
 * @param   pParent     The parent namespace entry
 * @param   pchSpec     The specified name (not necessarily terminated).
 * @param   cchSpec     The specified name length.
 * @param   ppNewName   Where to return the name entry.  Optional.
 */
static int rtFsIsoMakerObjSetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj,
                                  PRTFSISOMAKERNAME pParent, const char *pchSpec, size_t cchSpec, PPRTFSISOMAKERNAME ppNewName)
{
    Assert(cchSpec < _32K);

    /*
     * To avoid need to revert anything, make sure papChildren in the parent is
     * large enough.  If root object, make sure we haven't got a root already.
     */
    if (pParent)
    {
        AssertReturn(pParent->pDir, VERR_INTERNAL_ERROR_5);
        uint32_t cChildren = pParent->pDir->cChildren;
        if (cChildren & 31)
        { /* likely */ }
        else
        {
            AssertReturn(cChildren < RTFSISOMAKER_MAX_OBJECTS_PER_DIR, VERR_TOO_MUCH_DATA);
            void *pvNew = RTMemRealloc(pParent->pDir->papChildren, (cChildren + 32) * sizeof(pParent->pDir->papChildren[0]));
            AssertReturn(pvNew, VERR_NO_MEMORY);
            pParent->pDir->papChildren = (PPRTFSISOMAKERNAME)pvNew;
        }
    }
    else
        AssertReturn(pNamespace->pRoot == NULL, VERR_INTERNAL_ERROR_5);

    /*
     * Normalize the name for this namespace.
     */
    size_t cchName;
    char   szName[RTFSISOMAKER_MAX_NAME_BUF];
    int rc = rtFsIsoMakerNormalizeNameForNamespace(pThis, pNamespace, pParent, pchSpec, cchSpec,
                                                   pObj->enmType == RTFSISOMAKEROBJTYPE_DIR,
                                                   szName, sizeof(szName), &cchName);
    if (RT_SUCCESS(rc))
    {
        size_t cbName = sizeof(RTFSISOMAKERNAME)
                      + cchName + 1
                      + cchSpec + 1;
        if (pObj->enmType == RTFSISOMAKEROBJTYPE_DIR)
            cbName = RT_ALIGN_Z(cbName, 8) + sizeof(RTFSISOMAKERNAMEDIR);
        PRTFSISOMAKERNAME pName = (PRTFSISOMAKERNAME)RTMemAllocZ(cbName);
        if (pName)
        {
            pName->pObj                 = pObj;
            pName->pParent              = pParent;
            pName->cchName              = (uint16_t)cchName;

            char *pszDst = &pName->szName[cchName + 1];
            memcpy(pszDst, pchSpec, cchSpec);
            pszDst[cchSpec] = '\0';
            pName->pszSpecNm            = pszDst;
            pName->pszRockRidgeNm       = pszDst;
            pName->pszTransNm           = pszDst;
            pName->cchSpecNm            = (uint16_t)cchSpec;
            pName->cchRockRidgeNm       = 0;
            pName->cchTransNm           = 0;
            pName->uDepth               = pParent->uDepth + 1;
            pName->fRockRidgeNmAlloced  = false;
            pName->fTransNmAlloced      = false;

            pName->fMode                = pObj->fMode;
            pName->uid                  = pObj->uid;
            pName->gid                  = pObj->gid;
            pName->Device               = 0;
            pName->cHardlinks           = 1;
            pName->INode                = pObj->idxObj;

            memcpy(pName->szName, szName, cchName);
            pName->szName[cchName] = '\0';

            if (pObj->enmType != RTFSISOMAKEROBJTYPE_DIR)
                pName->pDir             = NULL;
            else
            {
                size_t offDir = RT_UOFFSETOF(RTFSISOMAKERNAME, szName) + cchName + 1 + cchSpec + 1;
                offDir = RT_ALIGN_Z(offDir, 8);
                PRTFSISOMAKERNAMEDIR pDir = (PRTFSISOMAKERNAMEDIR)((uintptr_t)pName + offDir);
                pDir->offDir        = UINT64_MAX;
                pDir->cbDir         = 0;
                pDir->cChildren     = 0;
                pDir->papChildren   = NULL;
                pDir->pTransTblFile = NULL;
                /** @todo TRANS.TBL files. */

                pName->pDir = pDir;
            }

            /*
             * Do the linking and stats.
             */
            if (pParent)
                pParent->pDir->papChildren[pParent->pDir->cChildren++] = pName;
            else
                pNamespace->pRoot = pName;
            *rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace) = pName;
            pNamespace->cNames++;

            /*
             * Done.
             */
            if (ppNewName)
                *ppNewName = pName;
            return VINF_SUCCESS;
        }
    }
    return rc;
}


static int rtFsIsoMakerPathToParent(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, const char *pszPath,
                                    PPRTFSISOMAKERNAME ppParent, const char **ppszEntry, size_t *pcchEntry)
{
    int rc;
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INTERNAL_ERROR_4);

    /*
     * Deal with the special case of the root.
     */
    while (RTPATH_IS_SLASH(*pszPath))
        pszPath++;
    AssertReturn(*pszPath, VERR_INTERNAL_ERROR_4);

    PRTFSISOMAKERNAME pParent = pNamespace->pRoot;
    if (!pParent)
    {
        PRTFSISOMAKERDIR pDir;
        if (pThis->cObjects == 0)
        {
            rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pDir);
            AssertRCReturn(rc, rc);
        }
        else
        {
            pDir = RTListGetFirst(&pThis->ObjectHead, RTFSISOMAKERDIR, Core.Entry);
#ifdef RT_STRICT
            Assert(pDir);
            Assert(pDir->Core.idxObj == 0);
            Assert(pDir->Core.enmType == RTFSISOMAKEROBJTYPE_DIR);
            Assert(*rtFsIsoMakerObjGetNameForNamespace(&pDir->Core, pNamespace) == NULL);
#endif
        }

        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pDir->Core, NULL /*pParent*/, "", 0, &pParent);
        AssertRCReturn(rc, rc);
        pParent = pNamespace->pRoot;
        AssertReturn(pParent, VERR_INTERNAL_ERROR_4);
    }

    /*
     * Now, do the rest of the path.
     */
    for (;;)
    {
        /*
         * Find the end of the component and see if its the final one or not.
         */
        char ch;
        size_t cchComponent = 0;
        while ((ch = pszPath[cchComponent]) != '\0' && RTPATH_IS_SLASH(ch))
            cchComponent++;
        AssertReturn(cchComponent > 0, VERR_INTERNAL_ERROR_4);

        size_t offNext = cchComponent;
        while (RTPATH_IS_SLASH(ch))
            ch = pszPath[offNext++];

        if (ch == '\0')
        {
            /*
             * Final component.  Make sure it is not dot or dot-dot before returning.
             */
            AssertReturn(   pszPath[0] != '.'
                         || cchComponent > 2
                         || (   cchComponent == 2
                             && pszPath[1] != '.'),
                         VERR_INVALID_NAME);

            *ppParent  = pParent;
            *ppszEntry = pszPath;
            *pcchEntry = cchComponent;
            return VINF_SUCCESS;
        }

        /*
         * Deal with dot and dot-dot.
         */
        if (cchComponent == 1 && pszPath[0] == '.')
        { /* nothing to do */ }
        else if (cchComponent == 2 && pszPath[0] == '.' && pszPath[1] == '.')
        {
            if (pParent->pParent)
                pParent = pParent->pParent;
        }
        else
        {
            /*
             * Look it up.
             */
            PRTFSISOMAKERNAME pChild = rtFsIsoMakerFindEntryInDirBySpec(pParent, pszPath, cchComponent);
            if (pChild)
                pParent = pChild;
            else
            {
                /* Try see if we've got a directory with the same spec name in a different namespace.
                   (We don't want to waste heap by creating a directory instance per namespace.) */
                PRTFSISOMAKERDIR pChildObj = rtFsIsoMakerFindSubdirBySpec((PRTFSISOMAKERDIR)pParent->pObj,
                                                                           pszPath, cchComponent, pNamespace->fNamespace);
                if (pChildObj)
                {
                    PPRTFSISOMAKERNAME ppChildName = rtFsIsoMakerObjGetNameForNamespace(&pChildObj->Core, pNamespace);
                    if (!*ppChildName)
                    {
                        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pChildObj->Core, pParent, pszPath, cchComponent, &pChild);
                        if (RT_FAILURE(rc))
                            return rc;
                        AssertReturn(pChild != NULL, VERR_INTERNAL_ERROR_4);
                    }
                }
                /* If we didn't have luck in other namespaces, create a new directory. */
                if (!pChild)
                {
                    rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pChildObj);
                    if (RT_SUCCESS(rc))
                        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pChildObj->Core, pParent, pszPath, cchComponent, &pChild);
                    if (RT_FAILURE(rc))
                        return rc;
                    AssertReturn(pChild != NULL, VERR_INTERNAL_ERROR_4);
                }
            }
        }

        /*
         * Skip ahead in the path.
         */
        pszPath += offNext;
    }
}


/**
 * Worker for RTFsIsoMakerObjSetPath that operates on a single namespace.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO maker instance.
 * @param   pNamespace      The namespace to name it in.
 * @param   pObj            The filesystem object to name.
 * @param   pszPath         The path to the entry in the namespace.
 */
static int rtFsIsoMakerObjSetPathInOne(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                       PRTFSISOMAKEROBJ pObj, const char *pszPath)
{
    AssertReturn(*rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace) == NULL, VERR_WRONG_ORDER);
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INTERNAL_ERROR_5);

    /*
     * Figure out where the parent is.
     * This will create missing parent name space entries and directory nodes.
     */
    PRTFSISOMAKERNAME   pParent;
    const char         *pszEntry;
    size_t              cchEntry;
    int                 rc;
    if (pszPath[1] != '\0')
        rc = rtFsIsoMakerPathToParent(pThis, pNamespace, pszPath, &pParent, &pszEntry, &cchEntry);
    else
    {
        /*
         * Special case for the root directory.
         */
        Assert(pObj->enmType == RTFSISOMAKEROBJTYPE_DIR);
        AssertReturn(pNamespace->pRoot == NULL, VERR_WRONG_ORDER);
        pszEntry = "/";
        cchEntry = 0;
        pParent  = NULL;
        rc       = VINF_SUCCESS;
    }

    /*
     * Do the job on the final path component.
     */
    if (RT_SUCCESS(rc))
    {
        AssertReturn(!RTPATH_IS_SLASH(pszEntry[cchEntry]) || pObj->enmType == RTFSISOMAKEROBJTYPE_DIR,
                     VERR_NOT_A_DIRECTORY);
        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, pObj, pParent, pszEntry, cchEntry, NULL);
    }
    return rc;
}


/**
 * Removes an object from the given namespace.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace.
 * @param   pObj        The object to name.
 */
static int rtFsIsoMakerObjUnsetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj)
{
    /*
     * First check if there is anything to do here at all.
     */
    PPRTFSISOMAKERNAME ppName = rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace);
    PRTFSISOMAKERNAME  pName = *ppName;
    if (!pName)
        return VINF_SUCCESS;

    /*
     * We don't support this on the root.
     */
    AssertReturn(pName->pParent, VERR_ACCESS_DENIED);

    /*
     * If this is a directory, we're in for some real fun here as we need to
     * unset the names of all the children too.
     */
    PRTFSISOMAKERNAMEDIR pDir = pName->pDir;
    if (pDir)
    {
        uint32_t iChild = pDir->cChildren;
        while (iChild-- > 0)
        {
            int rc = rtFsIsoMakerObjUnsetName(pThis, pNamespace, pDir->papChildren[iChild]->pObj);
            if (RT_FAILURE(rc))
                return rc;
        }
        AssertReturn(pDir->cChildren == 0, VERR_DIR_NOT_EMPTY);
    }

    /*
     * Unlink the pName from the parent.
     */
    pDir = pName->pParent->pDir;
    uint32_t iChild = pDir->cChildren;
    while (iChild-- > 0)
        if (pDir->papChildren[iChild] == pName)
        {
            uint32_t cToMove = pDir->cChildren - iChild - 1;
            if (cToMove > 0)
                memmove(&pDir->papChildren[iChild], &pDir->papChildren[iChild + 1], cToMove * sizeof(pDir->papChildren[0]));
            pDir->cChildren--;
            pNamespace->cNames--;

            /*
             * NULL the name member in the object and free the structure.
             */
            *ppName = NULL;
            RTMemFree(pName);

            return VINF_SUCCESS;
        }

    /* Not found. This can't happen. */
    AssertFailed();
    return VERR_INTERNAL_ERROR_2;
}



/*
 *
 * Object level config
 * Object level config
 * Object level config
 *
 */


/**
 * Translates an object index number to an object pointer, slow path.
 *
 * @returns Pointer to object, NULL if not found.
 * @param   pThis               The ISO maker instance.
 * @param   idxObj              The object index too resolve.
 */
DECL_NO_INLINE(static, PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObjSlow(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj;
    RTListForEachReverse(&pThis->ObjectHead, pObj, RTFSISOMAKEROBJ, Entry)
    {
        if (pObj->idxObj == idxObj)
            return pObj;
    }
    return NULL;
}


/**
 * Translates an object index number to an object pointer.
 *
 * @returns Pointer to object, NULL if not found.
 * @param   pThis               The ISO maker instance.
 * @param   idxObj              The object index too resolve.
 */
DECLINLINE(PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObj(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj = RTListGetLast(&pThis->ObjectHead, RTFSISOMAKEROBJ, Entry);
    if (!pObj || RT_LIKELY(pObj->idxObj == idxObj))
        return pObj;
    return rtFsIsoMakerIndexToObjSlow(pThis, idxObj);
}


/**
 * Removes the specified object from the image.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   idxObj              The index of the object to remove.
 */
RTDECL(int) RTFsIsoMakerObjRemove(RTFSISOMAKER hIsoMaker, uint32_t idxObj)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);

    /*
     * Remove the object from all name spaces.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
    {
        PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
        int rc2 = rtFsIsoMakerObjUnsetName(pThis, pNamespace, pObj);
        if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
            continue;
            rc = rc2;
    }

    /*
     * If that succeeded, remove the object itself.
     */
    if (RT_SUCCESS(rc))
    {
        RTListNodeRemove(&pObj->Entry);
        if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
        {
            uint64_t cbData = ((PRTFSISOMAKERFILE)pObj)->cbData;
            pThis->cbData -= RT_ALIGN_64(cbData, RTFSISOMAKER_SECTOR_SIZE);
        }
        pThis->cObjects--;
        rtFsIsoMakerObjDestroy(pObj);
    }

    return rc;
}


/**
 * Sets the path (name) of an object in the selected namespaces.
 *
 * The name will be transformed as necessary.
 *
 * The initial implementation does not allow this function to be called more
 * than once on an object.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKERNAMESPACE_XXX).
 * @param   pszPath             The path.
 */
RTDECL(int) RTFsIsoMakerObjSetPath(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t fNamespaces, const char *pszPath)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(!(fNamespaces & ~RTFSISOMAKERNAMESPACE_VALID_MASK), VERR_INVALID_FLAGS);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INVALID_NAME);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);

    /*
     * Execute requested actions.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (fNamespaces & g_aRTFsIsoNamespaces[i].fNamespace)
        {
            PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
            if (pNamespace->uLevel > 0)
            {
                int rc2 = rtFsIsoMakerObjSetPathInOne(pThis, pNamespace, pObj, pszPath);
                if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
                    continue;
                rc = rc2;
            }
        }
    return rc;
}


/**
 * Sets the name of an object in the selected namespaces, placing it under the
 * given directory.
 *
 * The name will be transformed as necessary.
 *
 * The initial implementation does not allow this function to be called more
 * than once on an object.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   idxParentObj        The parent directory object.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKERNAMESPACE_XXX).
 * @param   pszName             The name.
 */
RTDECL(int) RTFsIsoMakerObjSetNameAndParent(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t idxParentObj,
                                            uint32_t fNamespaces, const char *pszName)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(!(fNamespaces & ~RTFSISOMAKERNAMESPACE_VALID_MASK), VERR_INVALID_FLAGS);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    size_t cchName = strlen(pszName);
    AssertReturn(cchName > 0, VERR_INVALID_NAME);
    AssertReturn(memchr(pszName, '/', cchName) == NULL, VERR_INVALID_NAME);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    PRTFSISOMAKEROBJ pParentObj = rtFsIsoMakerIndexToObj(pThis, idxParentObj);
    AssertReturn(pParentObj, VERR_OUT_OF_RANGE);

    /*
     * Execute requested actions.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (fNamespaces & g_aRTFsIsoNamespaces[i].fNamespace)
        {
            PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
            if (pNamespace->uLevel > 0)
            {
                PRTFSISOMAKERNAME pParentName = *rtFsIsoMakerObjGetNameForNamespace(pParentObj, pNamespace);
                if (pParentName)
                {
                    int rc2 = rtFsIsoMakerObjSetName(pThis, pNamespace, pObj, pParentName, pszName, cchName, NULL /*ppNewName*/);
                    if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
                        continue;
                    rc = rc2;
                }
            }
        }
    return rc;
}


/**
 * Initalizes the common part of a file system object and links it into global
 * chain.
 *
 * @returns IPRT status code
 * @param   pThis               The ISO maker instance.
 * @param   pObj                The common object.
 * @param   enmType             The object type.
 * @param   pObjInfo            The object information (typically source).
 *                              Optional.
 */
static int rtFsIsoMakerInitCommonObj(PRTFSISOMAKERINT pThis, PRTFSISOMAKEROBJ pObj,
                                     RTFSISOMAKEROBJTYPE enmType, PCRTFSOBJINFO pObjInfo)
{
    AssertReturn(pThis->cObjects < RTFSISOMAKER_MAX_OBJECTS, VERR_OUT_OF_RANGE);
    pObj->enmType       = enmType;
    pObj->pPrimaryName  = NULL;
    pObj->pJolietName   = NULL;
    pObj->pUdfName      = NULL;
    pObj->pHfsName      = NULL;
    pObj->idxObj        = pThis->cObjects++;
    if (pObjInfo)
    {
        pObj->BirthTime         = pObjInfo->BirthTime;
        pObj->ChangeTime        = pObjInfo->ChangeTime;
        pObj->ModificationTime  = pObjInfo->ModificationTime;
        pObj->AccessedTime      = pObjInfo->AccessTime;
        pObj->fMode             = pObjInfo->Attr.fMode;
        pObj->uid               = pObjInfo->Attr.u.Unix.uid != NIL_RTUID ? pObjInfo->Attr.u.Unix.uid : pThis->uidDefault;
        pObj->gid               = pObjInfo->Attr.u.Unix.gid != NIL_RTGID ? pObjInfo->Attr.u.Unix.gid : pThis->gidDefault;
    }
    else
    {
        pObj->BirthTime         = pThis->ImageCreationTime;
        pObj->ChangeTime        = pThis->ImageCreationTime;
        pObj->ModificationTime  = pThis->ImageCreationTime;
        pObj->AccessedTime      = pThis->ImageCreationTime;
        pObj->fMode             = enmType == RTFSISOMAKEROBJTYPE_DIR ? pThis->fDefaultDirMode : pThis->fDefaultFileMode;
        pObj->uid               = pThis->uidDefault;
        pObj->gid               = pThis->gidDefault;
    }

    RTListAppend(&pThis->ObjectHead, &pObj->Entry);
    return VINF_SUCCESS;
}


/**
 * Internal function for adding an unnamed directory.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO make instance.
 * @param   ppDir               Where to return the directory.
 */
static int rtFsIsoMakerAddUnnamedDirWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKERDIR *ppDir)
{
    PRTFSISOMAKERDIR pDir = (PRTFSISOMAKERDIR)RTMemAllocZ(sizeof(*pDir));
    AssertReturn(pDir, VERR_NO_MEMORY);
    int rc = rtFsIsoMakerInitCommonObj(pThis, &pDir->Core, RTFSISOMAKEROBJTYPE_DIR, NULL);
    if (RT_SUCCESS(rc))
    {
        *ppDir = pDir;
        return VINF_SUCCESS;
    }
    RTMemFree(pDir);
    return rc;

}


/**
 * Adds an unnamed directory to the image.
 *
 * The directory must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedDir(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);

    PRTFSISOMAKERDIR pDir;
    int rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pDir);
    *pidxObj = RT_SUCCESS(rc) ? pDir->Core.idxObj : UINT32_MAX;
    return rc;
}


/**
 * Adds a directory to the image in all namespaces and default attributes.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszDir              The path (UTF-8) to the directory in the ISO.
 *
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.  Optional.
 * @sa      RTFsIsoMakerAddUnnamedDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddDir(RTFSISOMAKER hIsoMaker, const char *pszDir, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszDir), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedDir(hIsoMaker, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKERNAMESPACE_ALL, pszDir);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}


/**
 * Internal function for adding an unnamed file.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO make instance.
 * @param   cbExtra             Extra space for additional data (e.g. source
 *                              path string copy).
 * @param   ppFile              Where to return the file.
 */
static int rtFsIsoMakerAddUnnamedFileWorker(PRTFSISOMAKERINT pThis, PCRTFSOBJINFO pObjInfo, size_t cbExtra,
                                            PRTFSISOMAKERFILE *ppFile)
{
    PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)RTMemAllocZ(sizeof(*pFile) + cbExtra);
    AssertReturn(pFile, VERR_NO_MEMORY);
    int rc = rtFsIsoMakerInitCommonObj(pThis, &pFile->Core, RTFSISOMAKEROBJTYPE_FILE, pObjInfo);
    if (RT_SUCCESS(rc))
    {
        pFile->cbData       = pObjInfo->cbObject;
        pThis->cbData += RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE);
        pFile->offData      = UINT64_MAX;
        pFile->enmSrcType   = RTFSISOMAKERSRCTYPE_INVALID;
        pFile->u.pszSrcPath = NULL;

        *ppFile = pFile;
        return VINF_SUCCESS;
    }
    RTMemFree(pFile);
    return rc;

}


/**
 * Adds an unnamed file to the image that's backed by a host file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszSrcFile          The source file path.  VFS chain spec allowed.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddFile, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszSrcFile, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;

    /*
     * Check that the source file exists and is a file.
     */
    uint32_t    offError = 0;
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsChainQueryInfo(pszSrcFile, &ObjInfo, RTFSOBJATTRADD_UNIX, RTPATH_F_FOLLOW_LINK, &offError, NULL);
    AssertMsgRCReturn(rc, ("%s -> %Rrc offError=%u\n", pszSrcFile, rc, offError), rc);
    AssertMsgReturn(RTFS_IS_FILE(ObjInfo.Attr.fMode), ("%#x - %s\n", ObjInfo.Attr.fMode, pszSrcFile), VERR_NOT_A_FILE);

    /*
     * Create a file object for it.
     */
    size_t const      cbSrcFile = strlen(pszSrcFile) + 1;
    PRTFSISOMAKERFILE pFile;
    rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, &ObjInfo, cbSrcFile, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType   = RTFSISOMAKERSRCTYPE_PATH;
        pFile->u.pszSrcPath = (char *)memcpy(pFile + 1, pszSrcFile, cbSrcFile);

        *pidxObj = pFile->Core.idxObj;
    }
    return rc;
}


/**
 * Adds an unnamed file to the image that's backed by a VFS file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   hVfsFileSrc         The source file handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddUnnamedFileWithSrcPath, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithVfsFile(RTFSISOMAKER hIsoMaker, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;

    /*
     * Get the VFS file info.  This implicitly validates the handle.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsFileQueryInfo(hVfsFileSrc, &ObjInfo, RTFSOBJATTRADD_UNIX);
    AssertMsgRCReturn(rc, ("RTVfsFileQueryInfo(%p) -> %Rrc\n", hVfsFileSrc, rc), rc);

    /*
     * Retain a reference to the file.
     */
    uint32_t cRefs = RTVfsFileRetain(hVfsFileSrc);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a file object for it.
     */
    PRTFSISOMAKERFILE pFile;
    rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, &ObjInfo, 0, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType = RTFSISOMAKERSRCTYPE_VFS_FILE;
        pFile->u.hVfsFile = hVfsFileSrc;

        *pidxObj = pFile->Core.idxObj;
    }
    else
        RTVfsFileRelease(hVfsFileSrc);
    return rc;
}


/**
 * Adds a file that's backed by a host file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   pszSrcFile      The source file path.  VFS chain spec allowed.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional
 * @sa      RTFsIsoMakerAddFileWithVfsFile,
 *          RTFsIsoMakerAddUnnamedFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszFile, const char *pszSrcFile, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszFile), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedFileWithSrcPath(hIsoMaker, pszSrcFile, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKERNAMESPACE_ALL, pszFile);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}


/**
 * Adds a file that's backed by a VFS file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   hVfsFileSrc     The source file handle.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional.
 * @sa      RTFsIsoMakerAddUnnamedFileWithVfsFile,
 *          RTFsIsoMakerAddFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithVfsFile(RTFSISOMAKER hIsoMaker, const char *pszFile, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszFile), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedFileWithVfsFile(hIsoMaker, hVfsFileSrc, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKERNAMESPACE_ALL, pszFile);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}

