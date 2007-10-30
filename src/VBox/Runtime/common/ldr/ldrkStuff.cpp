/* $Id$ */
/** @file
 * innotek Portable Runtime - Binary Image Loader, kLdr Interface.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include <iprt/file.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/err.h>
#include "internal/ldr.h"
#define KLDR_ALREADY_INCLUDE_STD_TYPES
#define KLDR_NO_KLDR_H_INCLUDES
#include <k/kLdr.h>
#include <k/kRdrAll.h>
#include <k/kErrors.h>
#include <k/kMagics.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * kLdr file provider.
 */
typedef struct RTKLDRRDR
{
    /** The core. */
    KRDR            Core;
    /** The IPRT bit reader. */
    PRTLDRREADER    pReader;
} RTKLDRRDR, *PRTKLDRRDR;

/**
 * IPRT module.
 */
typedef struct RTLDRMODKLDR
{
    /** The Core module structure. */
    RTLDRMODINTERNAL    Core;
    /** The kLdr module. */
    PKLDRMOD            pMod;
} RTLDRMODKLDR, *PRTLDRMODKLDR;


/**
 * Arguments for a RTLDRMODKLDR callback wrapper.
 */
typedef struct RTLDRMODKLDRARGS
{
    union
    {
        PFNRT            pfn;
        PFNRTLDRENUMSYMS pfnEnumSyms;
        PFNRTLDRIMPORT   pfnGetImport;
    }                   u;
    void               *pvUser;
    const void         *pvBits;
    PRTLDRMODKLDR       pMod;
} RTLDRMODKLDRARGS, *PRTLDRMODKLDRARGS;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int rtkldrConvertError(int krc);
static int rtkldrConvertErrorFromIPRT(int rc);


static int      rtkldrRdrCreate(  PPKRDR ppRdr, const char *pszFilename);
static int      rtkldrRdrDestroy( PKRDR pRdr);
static int      rtkldrRdrRead(    PKRDR pRdr, void *pvBuf, KSIZE cb, KFOFF off);
static int      rtkldrRdrAllMap(  PKRDR pRdr, const void **ppvBits);
static int      rtkldrRdrAllUnmap(PKRDR pRdr, const void *pvBits);
static KFOFF rtkldrRdrSize(    PKRDR pRdr);
static KFOFF rtkldrRdrTell(    PKRDR pRdr);
static const char * rtkldrRdrName(PKRDR pRdr);
static KIPTR    rtkldrRdrNativeFH(PKRDR pRdr);
static KSIZE    rtkldrRdrPageSize(PKRDR pRdr);
static int      rtkldrRdrMap(     PKRDR pRdr, void **ppvBase, KU32 cSegments, PCKLDRSEG paSegments, KBOOL fFixed);
static int      rtkldrRdrRefresh( PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments);
static int      rtkldrRdrProtect( PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments, KBOOL fUnprotectOrProtect);
static int      rtkldrRdrUnmap(   PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments);
static void     rtkldrRdrDone(    PKRDR pRdr);


static DECLCALLBACK(int) rtkldrClose(PRTLDRMODINTERNAL pMod);
static DECLCALLBACK(int) rtkldrDone(PRTLDRMODINTERNAL pMod);
static DECLCALLBACK(int) rtkldrEnumSymbols(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits,
                                           RTUINTPTR BaseAddress,PFNRTLDRENUMSYMS pfnCallback, void *pvUser);
static int rtkldrEnumSymbolsWrapper(PKLDRMOD pMod, uint32_t iSymbol,
                                    const char *pchSymbol, KSIZE cchSymbol, const char *pszVersion,
                                    KLDRADDR uValue, uint32_t fKind, void *pvUser);
static DECLCALLBACK(size_t) rtkldrGetImageSize(PRTLDRMODINTERNAL pMod);
static DECLCALLBACK(int) rtkldrGetBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress,
                                       PFNRTLDRIMPORT pfnGetImport, void *pvUser);
static DECLCALLBACK(int) rtkldrRelocate(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress,
                                        RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);
static int rtkldrGetImportWrapper(PKLDRMOD pMod, uint32_t iImport, uint32_t iSymbol, const char *pchSymbol, KSIZE cchSymbol,
                                  const char *pszVersion, PKLDRADDR puValue, uint32_t *pfKind, void *pvUser);

static DECLCALLBACK(int) rtkldrGetSymbolEx(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress,
                                           const char *pszSymbol, RTUINTPTR *pValue);



/**
 * Converts a kLdr error code to an IPRT one.
 */
static int rtkldrConvertError(int krc)
{
    if (!krc)
        return VINF_SUCCESS;
    switch (krc)
    {
        case KERR_INVALID_PARAMETER:                        return VERR_INVALID_PARAMETER;
        case KERR_INVALID_HANDLE:                           return VERR_INVALID_HANDLE;
        case KERR_NO_MEMORY:                                return VERR_NO_MEMORY;


        case KLDR_ERR_UNKNOWN_FORMAT:
        case KLDR_ERR_MZ_NOT_SUPPORTED:                     return VERR_MZ_EXE_NOT_SUPPORTED;
        case KLDR_ERR_NE_NOT_SUPPORTED:                     return VERR_NE_EXE_NOT_SUPPORTED;
        case KLDR_ERR_LX_NOT_SUPPORTED:                     return VERR_LX_EXE_NOT_SUPPORTED;
        case KLDR_ERR_LE_NOT_SUPPORTED:                     return VERR_LE_EXE_NOT_SUPPORTED;
        case KLDR_ERR_PE_NOT_SUPPORTED:                     return VERR_PE_EXE_NOT_SUPPORTED;
        case KLDR_ERR_ELF_NOT_SUPPORTED:                    return VERR_ELF_EXE_NOT_SUPPORTED;
        case KLDR_ERR_MACHO_NOT_SUPPORTED:                  return VERR_INVALID_EXE_SIGNATURE;
        case KLDR_ERR_AOUT_NOT_SUPPORTED:                   return VERR_AOUT_EXE_NOT_SUPPORTED;

        case KLDR_ERR_MODULE_NOT_FOUND:                     return VERR_MODULE_NOT_FOUND;
        case KLDR_ERR_PREREQUISITE_MODULE_NOT_FOUND:        return VERR_MODULE_NOT_FOUND;
        case KLDR_ERR_MAIN_STACK_ALLOC_FAILED:              return VERR_NO_MEMORY;
        case KERR_BUFFER_OVERFLOW:                          return VERR_BUFFER_OVERFLOW;
        case KLDR_ERR_SYMBOL_NOT_FOUND:                     return VERR_SYMBOL_NOT_FOUND;
        case KLDR_ERR_FORWARDER_SYMBOL:                     return VERR_BAD_EXE_FORMAT;
        case KLDR_ERR_BAD_FIXUP:                            return VERR_BAD_EXE_FORMAT;
        case KLDR_ERR_IMPORT_ORDINAL_OUT_OF_BOUNDS:         return VERR_BAD_EXE_FORMAT;
        case KLDR_ERR_NO_DEBUG_INFO:                        return VERR_FILE_NOT_FOUND;
        case KLDR_ERR_ALREADY_MAPPED:                       return VERR_WRONG_ORDER;
        case KLDR_ERR_NOT_MAPPED:                           return VERR_WRONG_ORDER;
        case KLDR_ERR_ADDRESS_OVERFLOW:                     return VERR_NUMBER_TOO_BIG;

        case KLDR_ERR_NOT_LOADED_DYNAMICALLY:
        case KCPU_ERR_ARCH_CPU_NOT_COMPATIBLE:
        case KLDR_ERR_TOO_LONG_FORWARDER_CHAIN:
        case KLDR_ERR_MODULE_TERMINATING:
        case KLDR_ERR_PREREQUISITE_MODULE_TERMINATING:
        case KLDR_ERR_MODULE_INIT_FAILED:
        case KLDR_ERR_PREREQUISITE_MODULE_INIT_FAILED:
        case KLDR_ERR_MODULE_INIT_FAILED_ALREADY:
        case KLDR_ERR_PREREQUISITE_MODULE_INIT_FAILED_ALREADY:
        case KLDR_ERR_PREREQUISITE_RECURSED_TOO_DEEPLY:
        case KLDR_ERR_THREAD_ATTACH_FAILED:
        case KRDR_ERR_TOO_MANY_MAPPINGS:
        case KLDR_ERR_NOT_DLL:
        case KLDR_ERR_NOT_EXE:
            return VERR_GENERAL_FAILURE;


        case KLDR_ERR_PE_UNSUPPORTED_MACHINE:
        case KLDR_ERR_PE_BAD_FILE_HEADER:
        case KLDR_ERR_PE_BAD_OPTIONAL_HEADER:
        case KLDR_ERR_PE_BAD_SECTION_HEADER:
        case KLDR_ERR_PE_BAD_FORWARDER:
        case KLDR_ERR_PE_FORWARDER_IMPORT_NOT_FOUND:
        case KLDR_ERR_PE_BAD_FIXUP:
        case KLDR_ERR_PE_BAD_IMPORT:
            return VERR_GENERAL_FAILURE;

        case KLDR_ERR_LX_BAD_HEADER:
        case KLDR_ERR_LX_BAD_LOADER_SECTION:
        case KLDR_ERR_LX_BAD_FIXUP_SECTION:
        case KLDR_ERR_LX_BAD_OBJECT_TABLE:
        case KLDR_ERR_LX_BAD_PAGE_MAP:
        case KLDR_ERR_LX_BAD_ITERDATA:
        case KLDR_ERR_LX_BAD_ITERDATA2:
        case KLDR_ERR_LX_BAD_BUNDLE:
        case KLDR_ERR_LX_NO_SONAME:
        case KLDR_ERR_LX_BAD_SONAME:
        case KLDR_ERR_LX_BAD_FORWARDER:
        case KLDR_ERR_LX_NRICHAIN_NOT_SUPPORTED:
            return VERR_GENERAL_FAILURE;

        default:
            if (RT_FAILURE(krc))
                return krc;
            AssertMsgFailed(("krc=%d (%#x)\n", krc, krc));
            return VERR_NO_TRANSLATION;
    }
}


/**
 * Converts a IPRT error code to an kLdr one.
 */
static int rtkldrConvertErrorFromIPRT(int rc)
{
    if (RT_SUCCESS(rc))
        return 0;
    switch (rc)
    {
        case VERR_NO_MEMORY:            return KERR_NO_MEMORY;
        case VERR_INVALID_PARAMETER:    return KERR_INVALID_PARAMETER;
        case VERR_INVALID_HANDLE:       return KERR_INVALID_HANDLE;
        case VERR_BUFFER_OVERFLOW:      return KERR_BUFFER_OVERFLOW;
        default:
            return rc;
    }
}





/**
 * The file reader operations.
 * We provide our own based on IPRT instead of using the kLdr ones.
 */
extern "C" const KRDROPS g_kLdrRdrFileOps;
extern "C" const KRDROPS g_kLdrRdrFileOps =
{
    /* .pszName = */        "IPRT",
    /* .pNext = */          NULL,
    /* .pfnCreate = */      rtkldrRdrCreate,
    /* .pfnDestroy = */     rtkldrRdrDestroy,
    /* .pfnRead = */        rtkldrRdrRead,
    /* .pfnAllMap = */      rtkldrRdrAllMap,
    /* .pfnAllUnmap = */    rtkldrRdrAllUnmap,
    /* .pfnSize = */        rtkldrRdrSize,
    /* .pfnTell = */        rtkldrRdrTell,
    /* .pfnName = */        rtkldrRdrName,
    /* .pfnNativeFH = */    rtkldrRdrNativeFH,
    /* .pfnPageSize = */    rtkldrRdrPageSize,
    /* .pfnMap = */         rtkldrRdrMap,
    /* .pfnRefresh = */     rtkldrRdrRefresh,
    /* .pfnProtect = */     rtkldrRdrProtect,
    /* .pfnUnmap =  */      rtkldrRdrUnmap,
    /* .pfnDone = */        rtkldrRdrDone,
    /* .u32Dummy = */       42
};


/** @copydoc KLDRRDROPS::pfnCreate
 * @remark This is a dummy which isn't used. */
static int      rtkldrRdrCreate(  PPKRDR ppRdr, const char *pszFilename)
{
    AssertReleaseFailed();
    return -1;
}


/** @copydoc KLDRRDROPS::pfnDestroy */
static int      rtkldrRdrDestroy( PKRDR pRdr)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    int rc = pReader->pfnDestroy(pReader);
    return rtkldrConvertErrorFromIPRT(rc);
}


/** @copydoc KLDRRDROPS::pfnRead */
static int      rtkldrRdrRead(    PKRDR pRdr, void *pvBuf, KSIZE cb, KFOFF off)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    int rc = pReader->pfnRead(pReader, pvBuf, cb, off);
    return rtkldrConvertErrorFromIPRT(rc);
}


/** @copydoc KLDRRDROPS::pfnAllMap */
static int      rtkldrRdrAllMap(  PKRDR pRdr, const void **ppvBits)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    int rc = pReader->pfnMap(pReader, ppvBits);
    return rtkldrConvertErrorFromIPRT(rc);
}


/** @copydoc KLDRRDROPS::pfnAllUnmap */
static int      rtkldrRdrAllUnmap(PKRDR pRdr, const void *pvBits)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    int rc = pReader->pfnUnmap(pReader, pvBits);
    return rtkldrConvertErrorFromIPRT(rc);
}


/** @copydoc KLDRRDROPS::pfnSize */
static KFOFF rtkldrRdrSize(    PKRDR pRdr)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    return (KFOFF)pReader->pfnSize(pReader);
}


/** @copydoc KLDRRDROPS::pfnTell */
static KFOFF rtkldrRdrTell(    PKRDR pRdr)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    return (KFOFF)pReader->pfnTell(pReader);
}


/** @copydoc KLDRRDROPS::pfnName */
static const char * rtkldrRdrName(PKRDR pRdr)
{
    PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    return pReader->pfnLogName(pReader);
}


/** @copydoc KLDRRDROPS::pfnNativeFH */
static KIPTR rtkldrRdrNativeFH(PKRDR pRdr)
{
    AssertFailed();
    return -1;
}


/** @copydoc KLDRRDROPS::pfnPageSize */
static KSIZE rtkldrRdrPageSize(PKRDR pRdr)
{
    return PAGE_SIZE;
}


/** @copydoc KLDRRDROPS::pfnMap */
static int      rtkldrRdrMap(     PKRDR pRdr, void **ppvBase, KU32 cSegments, PCKLDRSEG paSegments, KBOOL fFixed)
{
    //PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    AssertFailed();
    return -1;
}


/** @copydoc KLDRRDROPS::pfnRefresh */
static int      rtkldrRdrRefresh( PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments)
{
    //PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    AssertFailed();
    return -1;
}


/** @copydoc KLDRRDROPS::pfnProtect */
static int      rtkldrRdrProtect( PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments, KBOOL fUnprotectOrProtect)
{
    //PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    AssertFailed();
    return -1;
}


/** @copydoc KLDRRDROPS::pfnUnmap */
static int      rtkldrRdrUnmap(   PKRDR pRdr, void *pvBase, KU32 cSegments, PCKLDRSEG paSegments)
{
    //PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
    AssertFailed();
    return -1;
}

/** @copydoc KLDRRDROPS::pfnDone */
static void     rtkldrRdrDone(    PKRDR pRdr)
{
    //PRTLDRREADER pReader = ((PRTKLDRRDR)pRdr)->pReader;
}






/**
 * Operations for a kLdr module.
 */
static const RTLDROPS g_rtkldrOps =
{
    "kLdr",
    rtkldrClose,
    NULL,
    rtkldrDone,
    rtkldrEnumSymbols,
    /* ext */
    rtkldrGetImageSize,
    rtkldrGetBits,
    rtkldrRelocate,
    rtkldrGetSymbolEx,
    42
};


/** @copydoc RTLDROPS::pfnClose */
static DECLCALLBACK(int) rtkldrClose(PRTLDRMODINTERNAL pMod)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    int rc = kLdrModClose(pModkLdr);
    return rtkldrConvertError(rc);
}


/** @copydoc RTLDROPS::pfnDone */
static DECLCALLBACK(int) rtkldrDone(PRTLDRMODINTERNAL pMod)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    int rc = kLdrModMostlyDone(pModkLdr);
    return rtkldrConvertError(rc);
}


/** @copydoc RTLDROPS::pfnEnumSymbols */
static DECLCALLBACK(int) rtkldrEnumSymbols(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress,
                                           PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    RTLDRMODKLDRARGS Args;
    Args.pvUser = pvUser;
    Args.u.pfnEnumSyms = pfnCallback;
    Args.pMod = (PRTLDRMODKLDR)pMod;
    Args.pvBits = pvBits;
    int rc = kLdrModEnumSymbols(pModkLdr, pvBits, BaseAddress,
                                fFlags & RTLDR_ENUM_SYMBOL_FLAGS_ALL ? KLDRMOD_ENUM_SYMS_FLAGS_ALL : 0,
                                rtkldrEnumSymbolsWrapper, &Args);
    return rtkldrConvertError(rc);
}


/** @copydoc FNKLDRMODENUMSYMS */
static int rtkldrEnumSymbolsWrapper(PKLDRMOD pMod, uint32_t iSymbol,
                                    const char *pchSymbol, KSIZE cchSymbol, const char *pszVersion,
                                    KLDRADDR uValue, uint32_t fKind, void *pvUser)
{
    PRTLDRMODKLDRARGS pArgs = (PRTLDRMODKLDRARGS)pvUser;

    /* If not zero terminated we'll have to use a temporary buffer. */
    const char *pszSymbol = pchSymbol;
    if (pchSymbol && pchSymbol[cchSymbol])
    {
        char *psz = (char *)alloca(cchSymbol) + 1;
        memcpy(psz, pchSymbol, cchSymbol);
        psz[cchSymbol] = '\0';
        pszSymbol = psz;
    }

#if defined(RT_OS_OS2) || (defined(RT_OS_DARWIN) && defined(RT_ARCH_X86))
    /* skip the underscore prefix. */
    if (*pszSymbol == '_')
        pszSymbol++;
#endif

    int rc = pArgs->u.pfnEnumSyms(&pArgs->pMod->Core, pszSymbol, iSymbol, uValue, pArgs->pvUser);
    if (RT_FAILURE(rc))
        return rc; /* don't bother converting. */
    return 0;
}


/** @copydoc RTLDROPS::pfnGetImageSize */
static DECLCALLBACK(size_t) rtkldrGetImageSize(PRTLDRMODINTERNAL pMod)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    return kLdrModSize(pModkLdr);
}


/** @copydoc RTLDROPS::pfnGetBits */
static DECLCALLBACK(int) rtkldrGetBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress,
                                       PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    RTLDRMODKLDRARGS Args;
    Args.pvUser = pvUser;
    Args.u.pfnGetImport = pfnGetImport;
    Args.pMod = (PRTLDRMODKLDR)pMod;
    Args.pvBits = pvBits;
    int rc = kLdrModGetBits(pModkLdr, pvBits, BaseAddress, rtkldrGetImportWrapper, &Args);
    return rtkldrConvertError(rc);
}


/** @copydoc RTLDROPS::pfnRelocate */
static DECLCALLBACK(int) rtkldrRelocate(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress,
                                        RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    RTLDRMODKLDRARGS Args;
    Args.pvUser = pvUser;
    Args.u.pfnGetImport = pfnGetImport;
    Args.pMod = (PRTLDRMODKLDR)pMod;
    Args.pvBits = pvBits;
    int rc = kLdrModRelocateBits(pModkLdr, pvBits, NewBaseAddress, OldBaseAddress, rtkldrGetImportWrapper, &Args);
    return rtkldrConvertError(rc);
}


/** @copydoc FNKLDRMODGETIMPORT */
static int rtkldrGetImportWrapper(PKLDRMOD pMod, uint32_t iImport, uint32_t iSymbol, const char *pchSymbol, KSIZE cchSymbol,
                                  const char *pszVersion, PKLDRADDR puValue, uint32_t *pfKind, void *pvUser)
{
    PRTLDRMODKLDRARGS pArgs = (PRTLDRMODKLDRARGS)pvUser;

    /* If not zero terminated we'll have to use a temporary buffer. */
    const char *pszSymbol = pchSymbol;
    if (pchSymbol && pchSymbol[cchSymbol])
    {
        char *psz = (char *)alloca(cchSymbol) + 1;
        memcpy(psz, pchSymbol, cchSymbol);
        psz[cchSymbol] = '\0';
        pszSymbol = psz;
    }

#if defined(RT_OS_OS2) || (defined(RT_OS_DARWIN) && defined(RT_ARCH_X86))
    /* skip the underscore prefix. */
    if (*pszSymbol == '_')
        pszSymbol++;
#endif

    /* get the import module name - TODO: cache this */
    const char *pszModule = NULL;
    if (iImport != NIL_KLDRMOD_IMPORT)
    {
        char *pszBuf = (char *)alloca(64);
        int rc = kLdrModGetImport(pMod, pArgs->pvBits, iImport, pszBuf, 64);
        if (rc)
            return rc;
        pszModule = pszBuf;
    }

    /* do the query */
    RTUINTPTR Value;
    int rc = pArgs->u.pfnGetImport(&pArgs->pMod->Core, pszModule, pszSymbol, pszSymbol ? ~0 : iSymbol, &Value, pArgs->pvUser);
    if (RT_SUCCESS(rc))
    {
        *puValue = Value;
        return 0;
    }
    return rtkldrConvertErrorFromIPRT(rc);
}



/** @copydoc RTLDROPS::pfnGetSymbolEx */
static DECLCALLBACK(int) rtkldrGetSymbolEx(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress,
                                           const char *pszSymbol, RTUINTPTR *pValue)
{
    PKLDRMOD pModkLdr = ((PRTLDRMODKLDR)pMod)->pMod;
    KLDRADDR uValue;

#if defined(RT_OS_OS2) || (defined(RT_OS_DARWIN) && defined(RT_ARCH_X86))
    /*
     * Add underscore prefix.
     */
    if (pszSymbol)
    {
        size_t cch = strlen(pszSymbol);
        char *psz = (char *)alloca(cch + 2);
        memcpy(psz + 1, pszSymbol, cch + 1);
        *psz = '_';
        pszSymbol = psz;
    }
#endif

    int rc = kLdrModQuerySymbol(pModkLdr, pvBits, BaseAddress,
                                NIL_KLDRMOD_SYM_ORDINAL, pszSymbol, strlen(pszSymbol), NULL,
                                NULL, NULL, &uValue, NULL);
    if (!rc)
    {
        *pValue = uValue;
        return VINF_SUCCESS;
    }
    return rtkldrConvertError(rc);
}



/**
 * Open a image using kLdr.
 *
 * @returns iprt status code.
 * @param   pReader     The loader reader instance which will provide the raw image bits.
 * @param   phLdrMod    Where to store the handle.
 */
int rtldrkLdrOpen(PRTLDRREADER pReader, PRTLDRMOD phLdrMod)
{
    /* Create a rtkldrRdr instance. */
    PRTKLDRRDR pRdr = (PRTKLDRRDR)RTMemAllocZ(sizeof(*pRdr));
    if (!pRdr)
        return VERR_NO_MEMORY;
    pRdr->Core.u32Magic = KRDR_MAGIC;
    pRdr->Core.pOps = &g_kLdrRdrFileOps;
    pRdr->pReader = pReader;

    /* Try open it. */
    PKLDRMOD pMod;
    int krc = kLdrModOpenFromRdr(&pRdr->Core, &pMod);
    if (!krc)
    {
        /* Create a module wrapper for it. */
        PRTLDRMODKLDR pNewMod = (PRTLDRMODKLDR)RTMemAllocZ(sizeof(*pNewMod));
        if (pNewMod)
        {
            pNewMod->Core.u32Magic = RTLDRMOD_MAGIC;
            pNewMod->Core.eState = LDR_STATE_OPENED;
            pNewMod->Core.pOps = &g_rtkldrOps;
            pNewMod->pMod = pMod;
            *phLdrMod = &pNewMod->Core;
            return VINF_SUCCESS;
        }
        kLdrModClose(pMod);
        krc = KERR_NO_MEMORY;
    }
    return rtkldrConvertError(krc);
}

