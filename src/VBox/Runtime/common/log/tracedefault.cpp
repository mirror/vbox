
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
    ASMAtomicXchgHandle(&g_hDefaultTraceBuf, hTraceBuf, &hOldTraceBuf);

    if (    hOldTraceBuf != NIL_RTTRACEBUF
        &&  hOldTraceBuf != hTraceBuf)
    {
        /* Race prevention kludge. */
        RTThreadSleep(33);
        RTTraceBufRelease(hOldTraceBuf);
    }

    return VINF_SUCCESS;
}


RTDECL(RTTRACEBUF)  RTTraceGetDefaultBuf(void)
{
    return g_hDefaultTraceBuf;
}


