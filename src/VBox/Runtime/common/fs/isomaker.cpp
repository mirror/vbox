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
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
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


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an ISO maker object name space node. */
typedef struct RTFSISOMAKERNAME *PRTFSISOMAKERNAME;
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
    PRTFSISOMAKERNAME      *papChildren;

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
    /** Length of pszRockRidgeNm. */
    uint16_t                cchRockRidgeNm;
    /** Length of pszTransNm. */
    uint16_t                cchTransNm;

    /** The depth in the namespace tree of this name. */
    uint8_t                 uDepth;
    /** Set if pszTransNm is allocated separately. */
    bool                    fTransNmAlloced : 1;
    /** Set if pszTransNm is allocated separately. */
    bool                    fRockRidgeNmAlloced : 1;

/** @todo more rock ridge info here.    */

    /** The name length. */
    uint16_t                cchName;
    /** The name. */
    char                    szName[RT_FLEXIBLE_ARRAY];
} RTFSISOMAKERNAME;


/**
 * Common base structure for the file system objects.
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

} RTFSISOMAKEROBJ;


/**
 * File source type.
 */
typedef enum RTFSISOMAKERSRCTYPE
{
    RTFSISOMAKERSRCTYPE_INVALID = 0,
    RTFSISOMAKERSRCTYPE_PATH,
    RTFSISOMAKERSRCTYPE_VFS_IO_STREAM,
    RTFSISOMAKERSRCTYPE_END,
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
        /** Path to the source file. */
        const char         *pszSrcPath;
        /** Source I/O stream (or file). */
        RTVFSIOSTREAM       hVfsIoStr;
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

    /** @name Name space configuration.
     * @{ */
    /** Set after we've been fed the first bit of content.
     * This means that the namespace configuration has been finalized and can no
     * longer be changed because it's simply too much work to do adjustments
     * after having started to add files. */
    bool                    fSeenContent;
    /** The ISO level (1-3), default is 3.
     * @todo support mkisofs level 4 (ISO-9660:1990, version 2). */
    uint8_t                 uIsoLevel;
    /** The ISO rock ridge level: 1 - enabled; 2 - with ER tag.
     * Linux behaves a little different when seeing the ER tag. */
    uint8_t                 uRockRidgeLevel;
    /** The joliet UCS level (1, 2, or 3), 0 if joliet is not enabled. */
    uint8_t                 uJolietLevel;
    /** The joliet rock ridge level: 1 - enabled; 2 - with ER tag.
     * @note Nobody seems to do rock ridge with joliet, so this is highly
     *       expermental and here just because we can. */
    uint8_t                 uJolietRockRidgeLevel;
    /** Enables UDF.
     * @remarks not yet implemented. */
    bool                    fUdf;
    /** Enables HFS
     * @remarks not yet implemented. */
    bool                    fHfs;
    /** @} */

    /** The root of the primary ISO-9660 name space. */
    PRTFSISOMAKERNAME       pPrimaryIsoRoot;
    /** The root of the joliet name space. */
    PRTFSISOMAKERNAME       pJolietRoot;
    /** The root of the UDF name space. */
    PRTFSISOMAKERNAME       pUdfRoot;
    /** The root of the hybrid HFS name space. */
    PRTFSISOMAKERNAME       pHfsRoot;

    /** The list of objects (RTFSISOMAKEROBJ). */
    RTLISTANCHOR            ObjectHead;

    /** The total image size. */
    uint64_t                cbTotal;

} RTFSISOMAKERINT;
/** Pointer to an ISO maker instance. */
typedef RTFSISOMAKERINT *PRTFSISOMAKERINT;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Help for iterating over namespaces.
 */
struct const struct
{
    /** The RTFSISOMAKERNAMESPACE_XXX indicator.  */
    uint32_t        fNamespace;
    /** Offset into RTFSISOMAKERINT of the root member. */
    uintptr_t       offRoot;
    /** Offset into RTFSISOMAKERNAMESPACE of the name member. */
    uintptr_t       offName;
    /** Namespace name for debugging purposes. */
    const char     *pszName;
} g_aRTFsIosNamespaces[] =
{
    {   RTFSISOMAKERNAMESPACE_ISO_9660, RT_OFFSETOF(RTFSISOMAKERINT, pPrimaryIsoRoot), RT_OFFSETOF(RTFSISOMAKERNAMESPACE, pPrimaryName), "iso-9660" },
    {   RTFSISOMAKERNAMESPACE_JOLIET,   RT_OFFSETOF(RTFSISOMAKERINT, pJolietRoot),     RT_OFFSETOF(RTFSISOMAKERNAMESPACE, pJolietName),  "joliet" },
    {   RTFSISOMAKERNAMESPACE_UDF,      RT_OFFSETOF(RTFSISOMAKERINT, pUdfRoot),        RT_OFFSETOF(RTFSISOMAKERNAMESPACE, pUdfName),     "udf" },
    {   RTFSISOMAKERNAMESPACE_HFS,      RT_OFFSETOF(RTFSISOMAKERINT, pHfsRoot),        RT_OFFSETOF(RTFSISOMAKERNAMESPACE, pHfsName),     "hfs" },
};


/**
 * Creates an ISO creator instance.
 *
 * @returns IPRT status code.
 * @param   phIsoMaker          Where to return the handle to the new ISO maker.
 */
RTDECL(int) RTFsIsoMakerCreate(PRTFSISOMAKER phIsoMaker)
{
    PRTFSISOMAKERINT pThis = (PRTFSISOMAKERINT)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->uMagic                   = RTFSISOMAKERINT_MAGIC;
        pThis->cRefs                    = 1;
        //pThis->fSeenContent             = false;
        pThis->uIsoLevel                = 3;
        pThis->uRockRidgeLevel          = 1;
        pThis->uJolietLevel             = 3;
        //pThis->uJolietRockRidgeLevel    = 0;
        RTListInit(&pThis->ObjectHead);
        pThis->cbTotal                  = _32K /* The system area size. */
                                        + RTFSISOMAKER_SECTOR_SIZE /* Primary volume descriptor. */
                                        + RTFSISOMAKER_SECTOR_SIZE /* Secondary volume descriptor for joliet. */
                                        + RTFSISOMAKER_SECTOR_SIZE /* Terminator descriptor. */;
        *phIsoMaker = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
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
            RTMemFree(pCur->pszRockRidgeNm);
            pCur->pszRockRidgeNm = NULL;
            RTMemFree(pCur->pszTransNm);
            pCur->pszTransNm = NULL;
            PRTFSISOMAKERNAME pNext = pCur->pParent;
            RTMemFree(pCur);

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
    rtFsIsoMakerDestroyTree(pThis->pPrimaryIsoRoot);
    rtFsIsoMakerDestroyTree(pThis->pJolietRoot);
    rtFsIsoMakerDestroyTree(pThis->pUdfRoot);
    rtFsIsoMakerDestroyTree(pThis->pHfsRoot);

    PRTFSISOMAKEROBJ pCur;
    PRTFSISOMAKEROBJ pNext;
    RTListForEachSafe(&pThis->ObjectHead, pCur, pNext, RTFSISOMAKEROBJ, Entry)
    {
        RTListNodeRemove(&pCur->Entry);
        RTMemFree(pCur);
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

    pThis->uIsoLevel = uIsoLevel;
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

    if (pThis->uJolietLevel != uJolietLevel)
    {
        if (uJolietLevel == 0)
            pThis->cbTotal -= RTFSISOMAKER_SECTOR_SIZE;
        else if (pThis->uJolietLevel == 0)
            pThis->cbTotal += RTFSISOMAKER_SECTOR_SIZE;
        pThis->uJolietLevel = uJolietLevel;
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

    pThis->uRockRidgeLevel = uLevel;
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

    pThis->uJolietRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}


/*
 *
 * Object level config
 * Object level config
 * Object level config
 *
 */


DECL_NO_INLINE(static, PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObj(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj;
    RTListForEachReverse(&pThis->ObjectHead, pObj, RTFSISOMAKEROBJ, Entry)
    {
        if (pObj->idxObj == idxObj)
            return pObj;
    }
    return NULL;
}


DECLINLINE(PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObj(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj = RTListGetLast(&pThis->ObjectHead, RTFSISOMAKEROBJ, Entry);
    if (!pObj || RT_LIKELY(pObj->idxObj == idxObj))
        return pObj;
    return rtFsIsoMakerIndexToObjSlow(pThis, idxObj)
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
 */
RTDECL(int) RTFsIsoMakerAddUnnamedDir(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertPtr(pidxObj);



    return VINF_SUCCESS;
}

static int rtTFsIsoMakerObjSetPathInOne(RTFSISOMAKER hIsoMaker, uint32_t idxEntry,
                                        uint32_t fNamespace, const char *pszPath)
{

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
 * @param   fNamespaces         The namespaces to apply the path to.
 * @param   pszPath             The ISO-9660 path.
 */
RTDECL(int) RTFsIsoMakerObjSetPath(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t fNamespaces, const char *pszPath)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);
    AssertReturn(!(fNamespaces & ~RTFSISOMAKERNAMESPACE_VALID_MASK), VERR_INVALID_FLAGS);
    AssertPtrReturn(pszPath);
    AssertReturn(RTPATH_IS_SLASH(pszPath == '/'), VERR_INVALID_NAME);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);

    /*
     * Execute requested actions.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIosNamespaces); i++)
        if (fNamespace & g_aRTFsIosNamespaces[i].fNamespace)
        {
            int rc2 = rtTFsIsoMakerObjSetPathInOne(pThis, pEntry, g_aRTFsIosNamespaces[i].fNamespace, pszPath,
                                                   (PRTFSISOMAKERNAME *)((uintptr_t)pThis + g_aRTFsIosNamespaces[i].offRoot),
                                                   (PRTFSISOMAKERNAMESPACE *)((uintptr_t)pEntry + g_aRTFsIosNamespaces[i].offName));
            if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
                continue;
            rc = rc2;
        }
    return rc;
}




/**
 * Adds a directory to the image.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszDir              The path (UTF-8) to the directory in the ISO.
 *
 * @param   pidxEntry           Where to return the configuration index of the
 *                              directory.  Optional.
 */
RTDECL(int) RTFsIsoMakerAddDir(RTFSISOMAKER hIsoMaker, const char *pszDir, uint32_t *pidxEntry)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSER_VALID_HANDLE_RET(pThis);


    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);
    AssertReturn(uLevel <= 2, VERR_INVALID_PARAMETER);

    pThis->uJolietRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}

