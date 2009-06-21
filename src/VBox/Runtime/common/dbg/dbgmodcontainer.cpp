
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>

#include <iprt/avl.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/strcache.h>
#include "internal/dbgmod.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Symbol entry.
 */
typedef struct RTDBGMODCONTAINERSYMBOL
{
    /** The address core. */
    AVLRUINTPTRNODECORE         AddrCore;
    /** The name space core. */
    RTSTRSPACECORE              NameCore;
    /** The segment index. */
    RTDBGSEGIDX                 iSeg;
    /** The symbol flags. */
    uint32_t                    fFlags;
    /** The symbol size.
     * This may be zero while the range in AddrCore indicates 0. */
    RTUINTPTR                   cb;
} RTDBGMODCONTAINERSYMBOL;
/** Pointer to a symbol entry in the debug info container. */
typedef RTDBGMODCONTAINERSYMBOL *PRTDBGMODCONTAINERSYMBOL;
/** Pointer to a const symbol entry in the debug info container. */
typedef RTDBGMODCONTAINERSYMBOL const *PCRTDBGMODCONTAINERSYMBOL;

/**
 * Line number entry.
 */
typedef struct RTDBGMODCONTAINERLINE
{
    /** The address core.
     * The Key is the address of the line number. */
    AVLUINTPTRNODECORE  AddrCore;
    /** Pointer to the file name (in string cache). */
    const char         *pszFile;
    /** The line number. */
    uint32_t            uLineNo;
} RTDBGMODCONTAINERLINE;
/** Pointer to a line number entry. */
typedef RTDBGMODCONTAINERLINE *PRTDBGMODCONTAINERLINE;
/** Pointer to const a line number entry. */
typedef RTDBGMODCONTAINERLINE const *PCRTDBGMODCONTAINERLINE;

/**
 * Segment entry.
 */
typedef struct RTDBGMODCONTAINERSEGMENT
{
    /** The symbol address space tree. */
    AVLRUINTPTRTREE             SymAddrTree;
    /** The line number address space tree. */
    AVLUINTPTRTREE              LineAddrTree;
    /** The segment offset. */
    RTUINTPTR                   off;
    /** The segment size. */
    RTUINTPTR                   cb;
    /** The segment name. */
    const char                 *pszName;
} RTDBGMODCONTAINERSEGMENT;
/** Pointer to a segment entry in the debug info container. */
typedef RTDBGMODCONTAINERSEGMENT *PRTDBGMODCONTAINERSEGMENT;
/** Pointer to a const segment entry in the debug info container. */
typedef RTDBGMODCONTAINERSEGMENT const *PCRTDBGMODCONTAINERSEGMENT;

/**
 * Instance data.
 */
typedef struct RTDBGMODCONTAINER
{
    /** The name space. */
    RTSTRSPACE                  Names;
    /** Tree containing any absolute addresses. */
    AVLRUINTPTRTREE             AbsAddrTree;
    /** Segment table. */
    PRTDBGMODCONTAINERSEGMENT   paSegs;
    /** The number of segments in the segment table. */
    RTDBGSEGIDX                 cSegs;
    /** The image size. 0 means unlimited. */
    RTUINTPTR                   cb;
} RTDBGMODCONTAINER;
/** Pointer to instance data for the debug info container. */
typedef RTDBGMODCONTAINER *PRTDBGMODCONTAINER;


/**
 * Fills in a RTDBGSYMBOL structure.
 *
 * @returns VINF_SUCCESS.
 * @param   pMySym          Our internal symbol representation.
 * @param   pExtSym         The external symbol representation.
 */
DECLINLINE(int) rtDbgModContainerReturnSymbol(PCRTDBGMODCONTAINERSYMBOL pMySym, PRTDBGSYMBOL pExtSym)
{
    pExtSym->Value  = pMySym->AddrCore.Key;
    pExtSym->offSeg = pMySym->AddrCore.Key;
    pExtSym->iSeg   = pMySym->iSeg;
    pExtSym->fFlags = pMySym->fFlags;
    pExtSym->cb     = pMySym->cb;
    Assert(pMySym->NameCore.cchString < sizeof(pExtSym->szName));
    memcpy(pExtSym->szName, pMySym->NameCore.pszString, pMySym->NameCore.cchString + 1);
    return VINF_SUCCESS;
}



/** @copydoc RTDBGMODVTDBG::pfnLineByAddr */
static DECLCALLBACK(int) rtDbgModContainer_LineByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off,
                                                      PRTINTPTR poffDisp, PRTDBGLINE pLine)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Validate the input address.
     */
    AssertMsgReturn(iSeg < pThis->cSegs,
                    ("iSeg=%#x cSegs=%#x\n", pThis->cSegs),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(pThis->paSegs[iSeg].cb < off,
                    ("off=%RTptr cbSeg=%RTptr\n", off, pThis->paSegs[iSeg].cb),
                    VERR_DBG_INVALID_SEGMENT_OFFSET);

    /*
     * Lookup the nearest line number with an address less or equal to the specified address.
     */
    PAVLUINTPTRNODECORE pAvlCore = RTAvlUIntPtrGetBestFit(&pThis->paSegs[iSeg].LineAddrTree, off, false /*fAbove*/);
    if (!pAvlCore)
        return VERR_SYMBOL_NOT_FOUND;
    PCRTDBGMODCONTAINERLINE pMyLine = RT_FROM_MEMBER(pAvlCore, RTDBGMODCONTAINERLINE const, AddrCore);
    if (poffDisp)
        *poffDisp = off - pMyLine->AddrCore.Key;
    pLine->Address = pMyLine->AddrCore.Key;
    pLine->offSeg  = pMyLine->AddrCore.Key;
    pLine->iSeg    = iSeg;
    pLine->uLineNo = pMyLine->uLineNo;
    strcpy(pLine->szFilename, pMyLine->pszFile);
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnLineAdd */
static DECLCALLBACK(int) rtDbgModContainer_LineAdd(PRTDBGMODINT pMod, const char *pszFile, size_t cchFile, uint32_t uLineNo,
                                                   uint32_t iSeg, RTUINTPTR off)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Validate the input address.
     */
    AssertMsgReturn(iSeg < pThis->cSegs,          ("iSeg=%#x cSegs=%#x\n", pThis->cSegs),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(pThis->paSegs[iSeg].cb < off, ("off=%RTptr cbSeg=%RTptr\n", off, pThis->paSegs[iSeg].cb),
                    VERR_DBG_INVALID_SEGMENT_OFFSET);

    /*
     * Create a new entry.
     */
    PRTDBGMODCONTAINERLINE pLine = (PRTDBGMODCONTAINERLINE)RTMemAllocZ(sizeof(*pLine));
    if (!pLine)
        return VERR_NO_MEMORY;
    pLine->AddrCore.Key = off;
    pLine->uLineNo = uLineNo;
    pLine->pszFile = RTStrCacheEnterN(g_hDbgModStrCache, pszFile, cchFile);
    int rc;
    if (pLine->pszFile)
    {
        if (RTAvlUIntPtrInsert(&pThis->paSegs[iSeg].LineAddrTree, &pLine->AddrCore))
            return VINF_SUCCESS;

        /* bail out */
        rc = VERR_DBG_ADDRESS_CONFLICT;
        RTStrCacheRelease(g_hDbgModStrCache, pLine->pszFile);
    }
    else
        rc = VERR_NO_MEMORY;
    RTMemFree(pLine);
    return rc;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByAddr */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off,
                                                        PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Validate the input address.
     */
    AssertMsgReturn(    iSeg == RTDBGSEGIDX_ABS
                    ||  iSeg < pThis->cSegs,
                    ("iSeg=%#x cSegs=%#x\n", pThis->cSegs),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(    iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                    ||  pThis->paSegs[iSeg].cb <= off,
                    ("off=%RTptr cbSeg=%RTptr\n", off, pThis->paSegs[iSeg].cb),
                    VERR_DBG_INVALID_SEGMENT_OFFSET);

    /*
     * Lookup the nearest symbol with an address less or equal to the specified address.
     */
    PAVLRUINTPTRNODECORE pAvlCore = RTAvlrUIntPtrGetBestFit(  iSeg == RTDBGSEGIDX_ABS
                                                            ? &pThis->AbsAddrTree
                                                            : &pThis->paSegs[iSeg].SymAddrTree,
                                                            off,
                                                            false /*fAbove*/);
    if (!pAvlCore)
        return VERR_SYMBOL_NOT_FOUND;
    PCRTDBGMODCONTAINERSYMBOL pMySym = RT_FROM_MEMBER(pAvlCore, RTDBGMODCONTAINERSYMBOL const, AddrCore);
    if (poffDisp)
        *poffDisp = off - pMySym->AddrCore.Key;
    return rtDbgModContainerReturnSymbol(pMySym, pSymbol);
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByName */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Look it up in the name space.
     */
    PRTSTRSPACECORE pStrCore = RTStrSpaceGet(&pThis->Names, pszSymbol);
    if (!pStrCore)
        return VERR_SYMBOL_NOT_FOUND;
    PCRTDBGMODCONTAINERSYMBOL pMySym = RT_FROM_MEMBER(pStrCore, RTDBGMODCONTAINERSYMBOL const, NameCore);
    return rtDbgModContainerReturnSymbol(pMySym, pSymbol);
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolAdd */
static DECLCALLBACK(int) rtDbgModContainer_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                                     RTDBGSEGIDX iSeg, RTUINTPTR off, RTUINTPTR cb, uint32_t fFlags)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Address validation. The other arguments have already been validated.
     */
    AssertMsgReturn(    iSeg == RTDBGSEGIDX_ABS
                    ||  iSeg < pThis->cSegs,
                    ("iSeg=%#x cSegs=%#x\n", pThis->cSegs),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(    iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                    ||  pThis->paSegs[iSeg].cb <= off + cb,
                    ("off=%RTptr cb=%RTptr cbSeg=%RTptr\n", off, cb, pThis->paSegs[iSeg].cb),
                    VERR_DBG_INVALID_SEGMENT_OFFSET);

    /*
     * Create a new entry.
     */
    PRTDBGMODCONTAINERSYMBOL pSymbol = (PRTDBGMODCONTAINERSYMBOL)RTMemAllocZ(sizeof(*pSymbol));
    if (!pSymbol)
        return VERR_NO_MEMORY;

    pSymbol->AddrCore.Key       = off;
    pSymbol->AddrCore.KeyLast   = off + RT_MIN(cb, 1);
    pSymbol->iSeg               = iSeg;
    pSymbol->cb                 = cb;
    pSymbol->fFlags             = fFlags;
    pSymbol->NameCore.pszString = RTStrCacheEnter(g_hDbgModStrCache, pszSymbol);
    int rc;
    if (pSymbol->NameCore.pszString)
    {
        if (RTStrSpaceInsert(&pThis->Names, &pSymbol->NameCore))
        {
            if (RTAvlrUIntPtrInsert(  iSeg == RTDBGSEGIDX_ABS
                                    ? &pThis->AbsAddrTree
                                    : &pThis->paSegs[iSeg].SymAddrTree,
                                    &pSymbol->AddrCore))
                return VINF_SUCCESS;

            /* bail out */
            rc = VERR_DBG_ADDRESS_CONFLICT;
            RTStrSpaceRemove(&pThis->Names, pSymbol->NameCore.pszString);
        }
        else
            rc = VERR_DBG_DUPLICATE_SYMBOL;
        RTStrCacheRelease(g_hDbgModStrCache, pSymbol->NameCore.pszString);
    }
    else
        rc = VERR_NO_MEMORY;
    RTMemFree(pSymbol);
    return rc;
}


/** @copydoc RTDBGMODVTDBG::pfnRvaToSegOff */
static DECLCALLBACK(RTDBGSEGIDX) rtDbgModContainer_RvaToSegOff(PRTDBGMODINT pMod, RTUINTPTR uRva, PRTUINTPTR poffSeg)
{
    PRTDBGMODCONTAINER          pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;
    PCRTDBGMODCONTAINERSEGMENT  paSeg = pThis->paSegs;
    uint32_t const              cSegs = pThis->cSegs;
    if (cSegs <= 7)
    {
        /*
         * Linear search.
         */
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            RTUINTPTR offSeg = uRva - paSeg[iSeg].off;
            if (offSeg < paSeg[iSeg].cb)
            {
                if (poffSeg)
                    *poffSeg = offSeg;
                return iSeg;
            }
        }
    }
    else
    {
        /*
         * Binary search.
         */
        uint32_t iFirst = 0;
        uint32_t iLast  = cSegs - 1;
        for (;;)
        {
            uint32_t    iSeg   = iFirst + (iFirst - iLast) / 2;
            RTUINTPTR   offSeg = uRva - paSeg[iSeg].off;
            if (offSeg < paSeg[iSeg].cb)
            {
                if (poffSeg)
                    *poffSeg = offSeg;
                return iSeg;
            }

            /* advance */
            if (uRva < paSeg[iSeg].off)
            {
                /* between iFirst and iSeg. */
                if (iSeg == iFirst)
                    break;
                iLast = iSeg - 1;
            }
            else
            {
                /* between iSeg and iLast. */
                if (iSeg == iLast)
                    break;
                iFirst = iSeg + 1;
            }
        }
    }

    /* Invalid. */
    return NIL_RTDBGSEGIDX;
}


/** Destroy a symbol node. */
static DECLCALLBACK(int)  rtDbgModContainer_DestroyTreeNode(PAVLRUINTPTRNODECORE pNode, void *pvUser)
{
    PRTDBGMODCONTAINERSYMBOL pSym = RT_FROM_MEMBER(pNode, RTDBGMODCONTAINERSYMBOL, AddrCore);
    RTStrCacheRelease(g_hDbgModStrCache, pSym->NameCore.pszString);
    pSym->NameCore.pszString = NULL;
    RTMemFree(pSym);
    return 0;
}


/** @copydoc RTDBGMODVTDBG::pfnClose */
static DECLCALLBACK(int) rtDbgModContainer_Close(PRTDBGMODINT pMod)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Destroy the symbols and instance data.
     */
    for (uint32_t iSeg = 0; iSeg < pThis->cSegs; iSeg++)
    {
        RTAvlrUIntPtrDestroy(&pThis->paSegs[iSeg].SymAddrTree, rtDbgModContainer_DestroyTreeNode, NULL);
        RTStrCacheRelease(g_hDbgModStrCache, pThis->paSegs[iSeg].pszName);
        pThis->paSegs[iSeg].pszName = NULL;
    }
    RTAvlrUIntPtrDestroy(&pThis->AbsAddrTree, rtDbgModContainer_DestroyTreeNode, NULL);
    pThis->Names = NULL;

    RTMemFree(pThis->paSegs);
    pThis->paSegs = NULL;
    RTMemFree(pThis);

    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnTryOpen */
static DECLCALLBACK(int) rtDbgModContainer_TryOpen(PRTDBGMODINT pMod)
{
    return VERR_INTERNAL_ERROR_5;
}



/** Virtual function table for the debug info container. */
static RTDBGMODVTDBG const g_rtDbgModVtDbgContainer =
{
    /*.u32Magic = */            RTDBGMODVTDBG_MAGIC,
    /*.fSupports = */           0, ///@todo iprt/types.h isn't up to date...
    /*.pszName = */             "container",
    /*.pfnTryOpen = */          rtDbgModContainer_TryOpen,
    /*.pfnClose = */            rtDbgModContainer_Close,
    /*.pfnRvaToSegOff = */      rtDbgModContainer_RvaToSegOff,
    /*.pfnSymbolAdd = */        rtDbgModContainer_SymbolAdd,
    /*.pfnSymbolByName = */     rtDbgModContainer_SymbolByName,
    /*.pfnSymbolByAddr = */     rtDbgModContainer_SymbolByAddr,
    /*.pfnLineAdd = */          rtDbgModContainer_LineAdd,
    /*.pfnLineByAddr = */       rtDbgModContainer_LineByAddr,
    /*.u32EndMagic = */         RTDBGMODVTDBG_MAGIC
};



/**
 * Creates a generic debug info container and associates it with the module.
 *
 * @returns IPRT status code.
 * @param   pMod        The module instance.
 * @param   cb          The module size.
 */
int rtDbgModContainerCreate(PRTDBGMODINT pMod, RTUINTPTR cb)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->Names = NULL;
    pThis->AbsAddrTree = NULL;
    pThis->paSegs = NULL;
    pThis->cSegs = 0;
    pThis->cb = cb;

    pMod->pDbgVt = &g_rtDbgModVtDbgContainer;
    pMod->pvDbgPriv = pThis;
    return VINF_SUCCESS;
}

