
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>

#include <iprt/avl.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
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
    /** The name. */
    char                       *pszName;
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
    AVLRUINTPTRTREE             AddrTree;
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
static DECLCALLBACK(int) rtDbgModContainer_LineByAddr(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGLINE pLine)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByAddr */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByAddr(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByName */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolAdd */
static DECLCALLBACK(int) rtDbgModContainer_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, uint32_t iSeg, RTGCUINTPTR off, RTUINT cbSymbol)
{
    return VINF_SUCCESS;
}


/** Destroy a symbol node. */
static DECLCALLBACK(int)  rtDbgModContainer_DestroyTreeNode(PAVLRUINTPTRNODECORE pNode, void *pvUser)
{
    PRTDBGMODCONTAINERSYMBOL pSym = RT_FROM_MEMBER(pNode, RTDBGMODCONTAINERSYMBOL, AddrCore);
    RTStrFree(pSym->pszName);
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
    RTAvlrUIntPtrDestroy(&pThis->AddrTree, rtDbgModContainer_DestroyTreeNode, NULL);
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
 * Creates a
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
    pThis->AddrTree = NULL;
    pThis->paSegs = NULL;
    pThis->cSegs = 0;
    pThis->cb = cb;

    pMod->pDbgVt = &g_rtDbgModVtDbgContainer;
    pMod->pvDbgPriv = pThis;
    return VINF_SUCCESS;
}

