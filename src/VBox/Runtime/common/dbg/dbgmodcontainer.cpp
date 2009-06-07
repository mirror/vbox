
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>

#include <iprt/avl.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"



/** @copydoc RTDBGMODVTDBG::pfnTryOpen */
static DECLCALLBACK(int) rtDbgModContainer_TryOpen(PRTDBGMODINT pMod)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnClose */
static DECLCALLBACK(int) rtDbgModContainer_Close(PRTDBGMODINT pMod)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolAdd */
static DECLCALLBACK(int) rtDbgModContainer_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, uint32_t iSeg, RTGCUINTPTR off, RTUINT cbSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByName */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnSymbolByAddr */
static DECLCALLBACK(int) rtDbgModContainer_SymbolByAddr(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    return VINF_SUCCESS;
}


/** @copydoc RTDBGMODVTDBG::pfnLineByAddr */
static DECLCALLBACK(int) rtDbgModContainer_LineByAddr(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGLINE pLine)
{
    return VINF_SUCCESS;
}


/** Virtual function table for the debug info container. */
RTDBGMODVTDBG const g_rtDbgModVtDbgContainer =
{
    /*.u32Magic = */            RTDBGMODVTDBG_MAGIC,
    /*.fSupports = */           0, ///@todo iprt/types.h isn't up to date...
    /*.pszName = */             "CONTAINER",
    /*.pfnTryOpen = */          rtDbgModContainer_TryOpen,
    /*.pfnClose = */            rtDbgModContainer_Close,
    /*.pfnSymbolAdd = */        rtDbgModContainer_SymbolAdd,
    /*.pfnSymbolByName = */     rtDbgModContainer_SymbolByName,
    /*.pfnSymbolByAddr = */     rtDbgModContainer_SymbolByAddr,
    /*.pfnLineByAddr = */       rtDbgModContainer_LineByAddr,
    /*.u32EndMagic = */         RTDBGMODVTDBG_MAGIC
};

