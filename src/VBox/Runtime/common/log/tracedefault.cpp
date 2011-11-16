
/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/trace.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The default trace buffer handle. */
static RTTRACEBUF   g_hDefaultTraceBuf = NIL_RTTRACEBUF;



RTDECL(int)         RTTraceSetDefaultBuf(RTTRACEBUF hTraceBuf)
{
    /* Retain the new buffer. */
    if (hTraceBuf != NIL_RTTRACEBUF)
    {
        uint32_t cRefs = RTTraceBufRetain(hTraceBuf);
        if (cRefs >= _1M)
            return VERR_INVALID_HANDLE;
    }

    RTTRACEBUF hOldTraceBuf;
#ifdef IN_RC
    hOldTraceBuf = (RTTRACEBUF)ASMAtomicXchgPtr((void **)&g_hDefaultTraceBuf, hTraceBuf);
#else
    ASMAtomicXchgHandle(&g_hDefaultTraceBuf, hTraceBuf, &hOldTraceBuf);
#endif

    if (    hOldTraceBuf != NIL_RTTRACEBUF
        &&  hOldTraceBuf != hTraceBuf)
    {
        /* Race prevention kludge. */
#ifndef IN_RC
        RTThreadSleep(33);
#endif
        RTTraceBufRelease(hOldTraceBuf);
    }

    return VINF_SUCCESS;
}


RTDECL(RTTRACEBUF)  RTTraceGetDefaultBuf(void)
{
    return g_hDefaultTraceBuf;
}

