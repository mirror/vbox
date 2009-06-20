
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
    /** The segent offset. */
    RTUINTPTR                   off;
    /** The segment index. */
    RTDBGSEGIDX                 iSeg;
} RTDBGMODCONTAINERSYMBOL;
/** Pointer to a symbol entry in the debug info container. */
typedef RTDBGMODCONTAINERSYMBOL *PRTDBGMODCONTAINERSYMBOL;

/**
 * Segment entry.
 */
typedef struct RTDBGMODCONTAINERSEGMENT
{
    /** The segment offset. */
    RTUINTPTR                   off;
    /** The segment size. */
    RTUINTPTR                   cb;
    /** The segment name. */
    const char                 *pszName;
} RTDBGMODCONTAINERSEGMENT;
/** Pointer to a segment entry in the debug info container. */
typedef RTDBGMODCONTAINERSEGMENT *PRTDBGMODCONTAINERSEGMENT;

/**
 * Instance data.
 */
typedef struct RTDBGMODCONTAINER
{
    /** The name space. */
    RTSTRSPACE                  Names;
    /** The address space tree. */
    AVLRUINTPTRTREE             Addresses;
    /** Segment table. */
    PRTDBGMODCONTAINERSEGMENT   paSegs;
    /** The number of segments in the segment table. */
    RTDBGSEGIDX                 cSegs;
    /** The image size. 0 means unlimited. */
    RTUINTPTR                   cb;
} RTDBGMODCONTAINER;
/** Pointer to instance data for the debug info container. */
typedef RTDBGMODCONTAINER *PRTDBGMODCONTAINER;



/** @copydoc RTDBGMODVTDBG::pfnLineByAddr */
static DECLCALLBACK(int) rtDbgModContainer_LineByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGLINE pLine)
{
    /** @todo Make it possible to add line numbers. */
    return VERR_DBG_NO_LINE_NUMBERS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByAddr */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByName */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolAdd */
static DECLCALLBACK(int) rtDbgModContainer_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, RTDBGSEGIDX iSeg, RTGCUINTPTR off, uint32_t cbSymbol)
{
    PRTDBGMODCONTAINER pThis = (PRTDBGMODCONTAINER)pMod->pvDbgPriv;

    /*
     * Address validation. The other arguments have already been validated.
     */
    AssertMsgReturn(    (   iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                         && iSeg <= RTDBGSEGIDX_SPECIAL_LAST)
                    ||  iSeg < pThis->cSegs,
                    ("iSeg=%x cSegs=%x\n", pThis->cSegs),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(    iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                    ||  pThis->paSegs[iSeg].cb <= off + cbSymbol,
                    ("off=%RTptr cbSymbol=%RTptr cbSeg=%RTptr\n", off, cbSymbol, pThis->paSegs[iSeg].cb),
                    VERR_DBG_INVALID_SEGMENT_OFFSET);

    /*
     * Create a new entry.
     */
    PRTDBGMODCONTAINERSYMBOL pSymbol = (PRTDBGMODCONTAINERSYMBOL)RTMemAllocZ(sizeof(*pSymbol));
    if (!pSymbol)
        return VERR_NO_MEMORY;

#if 0 /* continue on the workstation */
    pSymbol->AddrCore.Key     = iSeg < RTDBGSEGIDX_SPECIAL_FIRST
                              ? pThis->paSegs->off + off
                              : off;
    pSymbol->AddrCore.KeyLast = pSymbol->AddrCore.Key + cbSymbol;
    pSymbol->off  = off;
    pSymbol->iSeg = iSeg;
    pSymbol->NameCore.pszString = RTStrCacheEnter(g_hDbgModStrCache, pszSymbol);
    if (pSymbol->NameCore.pszString)
    {
        if (RTStrSpaceInsert(&pThis->Names, &pSymbol->NameCore))
        {
            if (RTStrSpaceInsert(&pThis->Names, &pSymbol->NameCore))
            {

            }
        }
    }
#endif
int rc = VERR_NOT_IMPLEMENTED;

    return rc;
}


/** Destroy a symbol node. */
static DECLCALLBACK(int)  rtDbgModContainer_DestroyTreeNode(PAVLRUINTPTRNODECORE pNode, void *pvUser)
{
    PRTDBGMODCONTAINERSYMBOL pSym = RT_FROM_MEMBER(pNode, RTDBGMODCONTAINERSYMBOL, AddrCore);
    RTStrCacheRelease(g_hDbgModStrCache, pSym->NameCore.pszString);
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
    RTAvlrUIntPtrDestroy(&pThis->Addresses, rtDbgModContainer_DestroyTreeNode, NULL);
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
    /*.pfnSymbolAdd = */        rtDbgModContainer_SymbolAdd,
    /*.pfnSymbolByName = */     rtDbgModContainer_SymbolByName,
    /*.pfnSymbolByAddr = */     rtDbgModContainer_SymbolByAddr,
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
    pThis->Addresses = NULL;
    pThis->paSegs = NULL;
    pThis->cSegs = 0;
    pThis->cb = cb;

    pMod->pDbgVt = &g_rtDbgModVtDbgContainer;
    pMod->pvDbgPriv = pThis;
    return VINF_SUCCESS;
}

