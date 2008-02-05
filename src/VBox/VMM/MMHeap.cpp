/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Heap.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MM_HEAP
#include <VBox/mm.h>
#include <VBox/stam.h>
#include <VBox/pgm.h>
#include "MMInternal.h"
#include <VBox/vm.h>
#include <VBox/uvm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void *mmR3HeapAlloc(PMMHEAP pHeap, MMTAG enmTag, size_t cbSize, bool fZero);



/**
 * Allocate and initialize a heap structure and it's associated substructures.
 *
 * @returns VBox status.
 * @param   pVM     The handle to the VM the heap should be associated with.
 * @param   ppHeap  Where to store the heap pointer.
 */
int mmR3HeapCreateU(PUVM pUVM, PMMHEAP *ppHeap)
{
    PMMHEAP pHeap = (PMMHEAP)RTMemAllocZ(sizeof(MMHEAP) + sizeof(MMHEAPSTAT));
    if (pHeap)
    {
        int rc = RTCritSectInit(&pHeap->Lock);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * Initialize the global stat record.
             */
            pHeap->pUVM = pUVM;
            pHeap->Stat.pHeap = pHeap;
#ifdef MMR3HEAP_WITH_STATISTICS
            PMMHEAPSTAT pStat = &pHeap->Stat;
            STAMR3RegisterU(pUVM, &pStat->cAllocations,   STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cAllocations",     STAMUNIT_CALLS, "Number or MMR3HeapAlloc() calls.");
            STAMR3RegisterU(pUVM, &pStat->cReallocations, STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cReallocations",   STAMUNIT_CALLS, "Number of MMR3HeapRealloc() calls.");
            STAMR3RegisterU(pUVM, &pStat->cFrees,         STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cFrees",           STAMUNIT_CALLS, "Number of MMR3HeapFree() calls.");
            STAMR3RegisterU(pUVM, &pStat->cFailures,      STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cFailures",        STAMUNIT_COUNT, "Number of failures.");
            STAMR3RegisterU(pUVM, &pStat->cbCurAllocated, sizeof(pStat->cbCurAllocated) == sizeof(uint32_t) ? STAMTYPE_U32 : STAMTYPE_U64,
                                                                        STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cbCurAllocated",   STAMUNIT_BYTES, "Number of bytes currently allocated.");
            STAMR3RegisterU(pUVM, &pStat->cbAllocated,    STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cbAllocated",      STAMUNIT_BYTES, "Total number of bytes allocated.");
            STAMR3RegisterU(pUVM, &pStat->cbFreed,        STAMTYPE_U64, STAMVISIBILITY_ALWAYS, "/MM/R3Heap/cbFreed",          STAMUNIT_BYTES, "Total number of bytes freed.");
#endif
            *ppHeap = pHeap;
            return VINF_SUCCESS;
        }
        AssertRC(rc);
        RTMemFree(pHeap);
    }
    AssertMsgFailed(("failed to allocate heap structure\n"));
    return VERR_NO_MEMORY;
}


/**
 * Destroy a heap.
 *
 * @param   pHeap   Heap handle.
 */
void mmR3HeapDestroy(PMMHEAP pHeap)
{
    /*
     * Start by deleting the lock, that'll trap anyone
     * attempting to use the heap.
     */
    RTCritSectDelete(&pHeap->Lock);

    /*
     * Walk the node list and free all the memory.
     */
    PMMHEAPHDR  pHdr = pHeap->pHead;
    while (pHdr)
    {
        void *pv = pHdr;
        pHdr = pHdr->pNext;
        RTMemFree(pv);
    }

    /*
     * Free the stat nodes.
     */
    /** @todo free all nodes in a AVL tree. */
    RTMemFree(pHeap);
}


/**
 * Allocate memory associating it with the VM for collective cleanup.
 *
 * The memory will be allocated from the default heap but a header
 * is added in which we keep track of which VM it belongs to and chain
 * all the allocations together so they can be freed in one go.
 *
 * This interface is typically used for memory block which will not be
 * freed during the life of the VM.
 *
 * @returns Pointer to allocated memory.
 * @param   pUVM        Pointer to the user mode VM structure.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 */
MMR3DECL(void *) MMR3HeapAllocU(PUVM pUVM, MMTAG enmTag, size_t cbSize)
{
    Assert(pUVM->mm.s.pHeap);
    return mmR3HeapAlloc(pUVM->mm.s.pHeap, enmTag, cbSize, false);
}


/**
 * Allocate memory associating it with the VM for collective cleanup.
 *
 * The memory will be allocated from the default heap but a header
 * is added in which we keep track of which VM it belongs to and chain
 * all the allocations together so they can be freed in one go.
 *
 * This interface is typically used for memory block which will not be
 * freed during the life of the VM.
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 */
MMR3DECL(void *) MMR3HeapAlloc(PVM pVM, MMTAG enmTag, size_t cbSize)
{
    return mmR3HeapAlloc(pVM->pUVM->mm.s.pHeap, enmTag, cbSize, false);
}


/**
 * Same as MMR3HeapAllocU().
 *
 * @returns Pointer to allocated memory.
 * @param   pUVM        Pointer to the user mode VM structure.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 */
MMR3DECL(int) MMR3HeapAllocExU(PUVM pUVM, MMTAG enmTag, size_t cbSize, void **ppv)
{
    Assert(pUVM->mm.s.pHeap);
    void *pv = mmR3HeapAlloc(pUVM->mm.s.pHeap, enmTag, cbSize, false);
    if (pv)
    {
        *ppv = pv;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Same as MMR3HeapAlloc().
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 */
MMR3DECL(int) MMR3HeapAllocEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv)
{
    void *pv = mmR3HeapAlloc(pVM->pUVM->mm.s.pHeap, enmTag, cbSize, false);
    if (pv)
    {
        *ppv = pv;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Same as MMR3HeapAlloc() only the memory is zeroed.
 *
 * @returns Pointer to allocated memory.
 * @param   pUVM        Pointer to the user mode VM structure.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 */
MMR3DECL(void *) MMR3HeapAllocZU(PUVM pUVM, MMTAG enmTag, size_t cbSize)
{
    return mmR3HeapAlloc(pUVM->mm.s.pHeap, enmTag, cbSize, true);
}


/**
 * Same as MMR3HeapAlloc() only the memory is zeroed.
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 */
MMR3DECL(void *) MMR3HeapAllocZ(PVM pVM, MMTAG enmTag, size_t cbSize)
{
    return mmR3HeapAlloc(pVM->pUVM->mm.s.pHeap, enmTag, cbSize, true);
}


/**
 * Same as MMR3HeapAllocZ().
 *
 * @returns Pointer to allocated memory.
 * @param   pUVM        Pointer to the user mode VM structure.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 */
MMR3DECL(int) MMR3HeapAllocZExU(PUVM pUVM, MMTAG enmTag, size_t cbSize, void **ppv)
{
    Assert(pUVM->mm.s.pHeap);
    void *pv = mmR3HeapAlloc(pUVM->mm.s.pHeap, enmTag, cbSize, true);
    if (pv)
    {
        *ppv = pv;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Same as MMR3HeapAllocZ().
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 */
MMR3DECL(int) MMR3HeapAllocZEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv)
{
    void *pv = mmR3HeapAlloc(pVM->pUVM->mm.s.pHeap, enmTag, cbSize, true);
    if (pv)
    {
        *ppv = pv;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Allocate memory from the heap.
 *
 * @returns Pointer to allocated memory.
 * @param   pHeap       Heap handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   fZero       Whether or not to zero the memory block.
 */
void *mmR3HeapAlloc(PMMHEAP pHeap, MMTAG enmTag, size_t cbSize, bool fZero)
{
#ifdef MMR3HEAP_WITH_STATISTICS
    RTCritSectEnter(&pHeap->Lock);

    /*
     * Find/alloc statistics nodes.
     */
    pHeap->Stat.cAllocations++;
    PMMHEAPSTAT pStat = (PMMHEAPSTAT)RTAvlULGet(&pHeap->pStatTree, (AVLULKEY)enmTag);
    if (pStat)
    {
        pStat->cAllocations++;

        RTCritSectLeave(&pHeap->Lock);
    }
    else
    {
        pStat = (PMMHEAPSTAT)RTMemAllocZ(sizeof(MMHEAPSTAT));
        if (!pStat)
        {
            pHeap->Stat.cFailures++;
            AssertMsgFailed(("Failed to allocate heap stat record.\n"));
            RTCritSectLeave(&pHeap->Lock);
            return NULL;
        }
        pStat->Core.Key = (AVLULKEY)enmTag;
        pStat->pHeap    = pHeap;
        RTAvlULInsert(&pHeap->pStatTree, &pStat->Core);

        pStat->cAllocations++;
        RTCritSectLeave(&pHeap->Lock);

        /* register the statistics */
        PUVM pUVM = pHeap->pUVM;
        char szName[80];
        const char *pszTag = mmR3GetTagName(enmTag);
        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cAllocations", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cAllocations,   STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_CALLS, "Number or MMR3HeapAlloc() calls.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cReallocations", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cReallocations, STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_CALLS, "Number of MMR3HeapRealloc() calls.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cFrees", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cFrees,         STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_CALLS, "Number of MMR3HeapFree() calls.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cFailures", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cFailures,      STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_COUNT, "Number of failures.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cbCurAllocated", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cbCurAllocated, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_BYTES, "Number of bytes currently allocated.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cbAllocated", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cbAllocated,    STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_BYTES, "Total number of bytes allocated.");

        RTStrPrintf(szName, sizeof(szName), "/MM/R3Heap/%s/cbFreed", pszTag);
        STAMR3RegisterU(pUVM, &pStat->cbFreed,        STAMTYPE_U64, STAMVISIBILITY_ALWAYS, szName,  STAMUNIT_BYTES, "Total number of bytes freed.");
    }
#endif

    /*
     * Validate input.
     */
    if (cbSize == 0)
    {
#ifdef MMR3HEAP_WITH_STATISTICS
        RTCritSectEnter(&pHeap->Lock);
        pStat->cFailures++;
        pHeap->Stat.cFailures++;
        RTCritSectLeave(&pHeap->Lock);
#endif
        return NULL;
    }

    /*
     * Allocate heap block.
     */
    cbSize = RT_ALIGN_Z(cbSize, MMR3HEAP_SIZE_ALIGNMENT) + sizeof(MMHEAPHDR);
    PMMHEAPHDR  pHdr = (PMMHEAPHDR)(fZero ? RTMemAllocZ(cbSize) : RTMemAlloc(cbSize));
    if (!pHdr)
    {
        AssertMsgFailed(("Failed to allocate heap block %d, enmTag=%x(%.4s).\n", cbSize, enmTag, &enmTag));
#ifdef MMR3HEAP_WITH_STATISTICS
        RTCritSectEnter(&pHeap->Lock);
        pStat->cFailures++;
        pHeap->Stat.cFailures++;
        RTCritSectLeave(&pHeap->Lock);
#endif
        return NULL;
    }
    Assert(!((uintptr_t)pHdr & (RTMEM_ALIGNMENT - 1)));

    RTCritSectEnter(&pHeap->Lock);

    /*
     * Init and link in the header.
     */
    pHdr->pNext = NULL;
    pHdr->pPrev = pHeap->pTail;
    if (pHdr->pPrev)
        pHdr->pPrev->pNext = pHdr;
    else
        pHeap->pHead = pHdr;
    pHeap->pTail = pHdr;
#ifdef MMR3HEAP_WITH_STATISTICS
    pHdr->pStat  = pStat;
#else
    pHdr->pStat  = &pHeap->Stat;
#endif
    pHdr->cbSize = cbSize;

    /*
     * Update statistics
     */
#ifdef MMR3HEAP_WITH_STATISTICS
    pStat->cbAllocated          += cbSize;
    pStat->cbCurAllocated       += cbSize;
    pHeap->Stat.cbAllocated     += cbSize;
    pHeap->Stat.cbCurAllocated  += cbSize;
#endif

    RTCritSectLeave(&pHeap->Lock);

    return pHdr + 1;
}


/**
 * Reallocate memory allocated with MMR3HeapAlloc() or MMR3HeapRealloc().
 *
 * @returns Pointer to reallocated memory.
 * @param   pv          Pointer to the memory block to reallocate.
 *                      Must not be NULL!
 * @param   cbNewSize   New block size.
 */
MMR3DECL(void *) MMR3HeapRealloc(void *pv, size_t cbNewSize)
{
    AssertMsg(pv, ("Invalid pointer pv=%p\n", pv));
    if (!pv)
        return NULL;

    /*
     * If newsize is zero then this is a free.
     */
    if (!cbNewSize)
    {
        MMR3HeapFree(pv);
        return NULL;
    }

    /*
     * Validate header.
     */
    PMMHEAPHDR  pHdr = (PMMHEAPHDR)pv - 1;
    if (    pHdr->cbSize & (MMR3HEAP_SIZE_ALIGNMENT - 1)
        ||  (uintptr_t)pHdr & (RTMEM_ALIGNMENT - 1))
    {
        AssertMsgFailed(("Invalid heap header! pv=%p, size=%#x\n", pv, pHdr->cbSize));
        return NULL;
    }
    Assert(pHdr->pStat != NULL);
    Assert(!((uintptr_t)pHdr->pNext & (RTMEM_ALIGNMENT - 1)));
    Assert(!((uintptr_t)pHdr->pPrev & (RTMEM_ALIGNMENT - 1)));

    PMMHEAP pHeap = pHdr->pStat->pHeap;

#ifdef MMR3HEAP_WITH_STATISTICS
    RTCritSectEnter(&pHeap->Lock);
    pHdr->pStat->cReallocations++;
    pHeap->Stat.cReallocations++;
    RTCritSectLeave(&pHeap->Lock);
#endif

    /*
     * Rellocate the block.
     */
    cbNewSize = RT_ALIGN_Z(cbNewSize, MMR3HEAP_SIZE_ALIGNMENT) + sizeof(MMHEAPHDR);
    PMMHEAPHDR pHdrNew = (PMMHEAPHDR)RTMemRealloc(pHdr, cbNewSize);
    if (!pHdrNew)
    {
#ifdef MMR3HEAP_WITH_STATISTICS
        RTCritSectEnter(&pHeap->Lock);
        pHdr->pStat->cFailures++;
        pHeap->Stat.cFailures++;
        RTCritSectLeave(&pHeap->Lock);
#endif
        return NULL;
    }

    /*
     * Update pointers.
     */
    if (pHdrNew != pHdr)
    {
        RTCritSectEnter(&pHeap->Lock);
        if (pHdrNew->pPrev)
            pHdrNew->pPrev->pNext = pHdrNew;
        else
            pHeap->pHead = pHdrNew;

        if (pHdrNew->pNext)
            pHdrNew->pNext->pPrev = pHdrNew;
        else
            pHeap->pTail = pHdrNew;
        RTCritSectLeave(&pHeap->Lock);
    }

    /*
     * Update statistics.
     */
#ifdef MMR3HEAP_WITH_STATISTICS
    RTCritSectEnter(&pHeap->Lock);
    pHdrNew->pStat->cbAllocated += cbNewSize - pHdrNew->cbSize;
    pHeap->Stat.cbAllocated += cbNewSize - pHdrNew->cbSize;
    RTCritSectLeave(&pHeap->Lock);
#endif

    pHdrNew->cbSize = cbNewSize;

    return pHdrNew + 1;
}


/**
 * Duplicates the specified string.
 *
 * @returns Pointer to the duplicate.
 * @returns NULL on failure or when input NULL.
 * @param   pUVM        Pointer to the user mode VM structure.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   psz         The string to duplicate. NULL is allowed.
 */
MMR3DECL(char *) MMR3HeapStrDupU(PUVM pUVM, MMTAG enmTag, const char *psz)
{
    if (!psz)
        return NULL;
    AssertPtr(psz);

    size_t cch = strlen(psz) + 1;
    char *pszDup = (char *)MMR3HeapAllocU(pUVM, enmTag, cch);
    if (pszDup)
        memcpy(pszDup, psz, cch);
    return pszDup;
}


/**
 * Duplicates the specified string.
 *
 * @returns Pointer to the duplicate.
 * @returns NULL on failure or when input NULL.
 * @param   pVM         The VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   psz         The string to duplicate. NULL is allowed.
 */
MMR3DECL(char *) MMR3HeapStrDup(PVM pVM, MMTAG enmTag, const char *psz)
{
    return MMR3HeapStrDupU(pVM->pUVM, enmTag, psz);
}


/**
 * Releases memory allocated with MMR3HeapAlloc() or MMR3HeapRealloc().
 *
 * @param   pv          Pointer to the memory block to free.
 */
MMR3DECL(void) MMR3HeapFree(void *pv)
{
    /* Ignore NULL pointers. */
    if (!pv)
        return;

    /*
     * Validate header.
     */
    PMMHEAPHDR  pHdr = (PMMHEAPHDR)pv - 1;
    if (    pHdr->cbSize & (MMR3HEAP_SIZE_ALIGNMENT - 1)
        ||  (uintptr_t)pHdr & (RTMEM_ALIGNMENT - 1))
    {
        AssertMsgFailed(("Invalid heap header! pv=%p, size=%#x\n", pv, pHdr->cbSize));
        return;
    }
    Assert(pHdr->pStat != NULL);
    Assert(!((uintptr_t)pHdr->pNext & (RTMEM_ALIGNMENT - 1)));
    Assert(!((uintptr_t)pHdr->pPrev & (RTMEM_ALIGNMENT - 1)));

    /*
     * Update statistics
     */
    PMMHEAP pHeap = pHdr->pStat->pHeap;
    RTCritSectEnter(&pHeap->Lock);

#ifdef MMR3HEAP_WITH_STATISTICS
    pHdr->pStat->cFrees++;
    pHeap->Stat.cFrees++;
    pHdr->pStat->cbFreed            += pHdr->cbSize;
    pHeap->Stat.cbFreed             += pHdr->cbSize;
    pHdr->pStat->cbCurAllocated     -= pHdr->cbSize;
    pHeap->Stat.cbCurAllocated      -= pHdr->cbSize;
#endif

    /*
     * Unlink it.
     */
    if (pHdr->pPrev)
        pHdr->pPrev->pNext = pHdr->pNext;
    else
        pHeap->pHead = pHdr->pNext;

    if (pHdr->pNext)
        pHdr->pNext->pPrev = pHdr->pPrev;
    else
        pHeap->pTail = pHdr->pPrev;

    RTCritSectLeave(&pHeap->Lock);

    /*
     * Free the memory.
     */
    RTMemFree(pHdr);
}


/**
 * Gets the string name of a memory tag.
 *
 * @returns name of enmTag.
 * @param   enmTag      The tag.
 */
const char *mmR3GetTagName(MMTAG enmTag)
{
    switch (enmTag)
    {
        #define TAG2STR(tag) case MM_TAG_##tag: return #tag

        TAG2STR(CFGM);
        TAG2STR(CFGM_BYTES);
        TAG2STR(CFGM_STRING);
        TAG2STR(CFGM_USER);

        TAG2STR(CSAM);
        TAG2STR(CSAM_PATCH);

        TAG2STR(DBGF);
        TAG2STR(DBGF_INFO);
        TAG2STR(DBGF_LINE);
        TAG2STR(DBGF_LINE_DUP);
        TAG2STR(DBGF_STACK);
        TAG2STR(DBGF_SYMBOL);
        TAG2STR(DBGF_SYMBOL_DUP);
        TAG2STR(DBGF_MODULE);

        TAG2STR(EM);

        TAG2STR(IOM);
        TAG2STR(IOM_STATS);

        TAG2STR(MM);
        TAG2STR(MM_LOOKUP_GUEST);
        TAG2STR(MM_LOOKUP_PHYS);
        TAG2STR(MM_LOOKUP_VIRT);
        TAG2STR(MM_PAGE);

        TAG2STR(PATM);
        TAG2STR(PATM_PATCH);

        TAG2STR(PDM);
        TAG2STR(PDM_DEVICE);
        TAG2STR(PDM_DEVICE_USER);
        TAG2STR(PDM_DRIVER);
        TAG2STR(PDM_DRIVER_USER);
        TAG2STR(PDM_USB);
        TAG2STR(PDM_USB_USER);
        TAG2STR(PDM_LUN);
        TAG2STR(PDM_QUEUE);
        TAG2STR(PDM_THREAD);
        TAG2STR(PDM_ASYNC_COMPLETION);

        TAG2STR(PGM);
        TAG2STR(PGM_CHUNK_MAPPING);
        TAG2STR(PGM_HANDLERS);
        TAG2STR(PGM_PHYS);
        TAG2STR(PGM_POOL);

        TAG2STR(REM);

        TAG2STR(SELM);

        TAG2STR(SSM);

        TAG2STR(STAM);

        TAG2STR(TM);

        TAG2STR(TRPM);

        TAG2STR(VM);
        TAG2STR(VM_REQ);

        TAG2STR(VMM);

        TAG2STR(HWACCM);

        #undef TAG2STR

        default:
        {
            AssertMsgFailed(("Unknown tag %d! forgot to add it to the switch?\n", enmTag));
            static char sz[48];
            RTStrPrintf(sz, sizeof(sz), "%d", enmTag);
            return sz;
        }
    }
}

