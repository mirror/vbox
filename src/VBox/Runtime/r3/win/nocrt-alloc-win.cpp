
/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/nt/nt-and-windows.h>


#undef RTMemTmpFree
RTDECL(void) RTMemTmpFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemFree
RTDECL(void) RTMemFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemTmpAllocTag
RTDECL(void *) RTMemTmpAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemTmpAllocZTag
RTDECL(void *) RTMemTmpAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemAllocTag
RTDECL(void *) RTMemAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemAllocZTag
RTDECL(void *) RTMemAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemReallocTag
RTDECL(void *) RTMemReallocTag(void *pvOld, size_t cbNew, const char *pszTag)
{
    RT_NOREF(pszTag);
    if (pvOld)
        return HeapReAlloc(GetProcessHeap(), 0, pvOld, cbNew);
    return HeapAlloc(GetProcessHeap(), 0, cbNew);
}


#undef RTMemReallocZTag
RTDECL(void *) RTMemReallocZTag(void *pvOld, size_t cbOld, size_t cbNew, const char *pszTag)
{
    RT_NOREF(pszTag, cbOld);
    if (pvOld)
        return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pvOld, cbNew);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbNew);
}

