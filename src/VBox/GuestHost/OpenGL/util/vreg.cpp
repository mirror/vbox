/* $Id$ */

/** @file
 * Visible Regions processing API implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <cr_vreg.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include <cr_error.h>
#define WARN(_m) do { crWarning _m ; } while (0)

#ifndef IN_RING0
#include <iprt/memcache.h>
#ifndef VBOXVDBG_VR_LAL_DISABLE
static RTMEMCACHE g_VBoxVrLookasideList;
#define vboxVrRegLaAlloc(_c) RTMemCacheAlloc((_c))
#define vboxVrRegLaFree(_c, _e) RTMemCacheFree((_c), (_e))
DECLINLINE(int) vboxVrLaCreate(RTMEMCACHE *pCache, size_t cbElement)
{
    int rc = RTMemCacheCreate(pCache, cbElement,
                            0, /* size_t cbAlignment */
                            UINT32_MAX, /* uint32_t cMaxObjects */
                            NULL, /* PFNMEMCACHECTOR pfnCtor*/
                            NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                            NULL, /* void *pvUser*/
                            0 /* uint32_t fFlags*/
                            );
    if (!RT_SUCCESS(rc))
    {
        WARN(("RTMemCacheCreate failed rc %d", rc));
        return rc;
    }
    return VINF_SUCCESS;
}
#define vboxVrLaDestroy(_c) RTMemCacheDestroy((_c))
#endif
#else
# ifdef RT_OS_WINDOWS
#  ifdef PAGE_SIZE
#    undef PAGE_SIZE
#  endif
#  ifdef PAGE_SHIFT
#    undef PAGE_SHIFT
#  endif
#  define VBOX_WITH_WORKAROUND_MISSING_PACK
#  if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#    define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#    define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#    define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#    define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#    define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#    define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#    define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#    define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#    pragma warning(disable : 4163)
#    ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#      pragma warning(disable : 4103)
#    endif
#    include <ntddk.h>
#    pragma warning(default : 4163)
#    ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#      pragma pack()
#      pragma warning(default : 4103)
#    endif
#    undef  _InterlockedExchange
#    undef  _InterlockedExchangeAdd
#    undef  _InterlockedCompareExchange
#    undef  _InterlockedAddLargeStatistic
#    undef  _interlockedbittestandset
#    undef  _interlockedbittestandreset
#    undef  _interlockedbittestandset64
#    undef  _interlockedbittestandreset64
#  else
#    include <ntddk.h>
#  endif
#ifndef VBOXVDBG_VR_LAL_DISABLE
static LOOKASIDE_LIST_EX g_VBoxVrLookasideList;
#define vboxVrRegLaAlloc(_c) ExAllocateFromLookasideListEx(&(_c))
#define vboxVrRegLaFree(_c, _e) ExFreeToLookasideListEx(&(_c), (_e))
#define VBOXWDDMVR_MEMTAG 'vDBV'
DECLINLINE(int) vboxVrLaCreate(LOOKASIDE_LIST_EX *pCache, size_t cbElement)
{
    NTSTATUS Status = ExInitializeLookasideListEx(pCache,
                                NULL, /* PALLOCATE_FUNCTION_EX Allocate */
                                NULL, /* PFREE_FUNCTION_EX Free */
                                NonPagedPool,
                                0, /* ULONG Flags */
                                cbElement,
                                VBOXWDDMVR_MEMTAG,
                                0 /* USHORT Depth - reserved, must be null */
                                );
    if (!NT_SUCCESS(Status))
    {
        WARN(("ExInitializeLookasideListEx failed, Status (0x%x)", Status));
        return VERR_GENERAL_FAILURE;
    }

    return VINF_SUCCESS;
}
#define vboxVrLaDestroy(_c) ExDeleteLookasideListEx(&(_c))
#endif
# else
#  error "port me!"
# endif
#endif

#ifdef DEBUG_misha
//# define VBOXVDBG_VR_LAL_DISABLE
#endif

#ifndef VBOXVDBG_VR_LAL_DISABLE
static volatile int32_t g_cVBoxVrInits = 0;
#endif

static PVBOXVR_REG vboxVrRegCreate()
{
#ifndef VBOXVDBG_VR_LAL_DISABLE
    PVBOXVR_REG pReg = (PVBOXVR_REG)vboxVrRegLaAlloc(g_VBoxVrLookasideList);
    if (!pReg)
    {
        WARN(("ExAllocateFromLookasideListEx failed!"));
    }
    return pReg;
#else
    return (PVBOXVR_REG)RTMemAlloc(sizeof (VBOXVR_REG));
#endif
}

static void vboxVrRegTerm(PVBOXVR_REG pReg)
{
#ifndef VBOXVDBG_VR_LAL_DISABLE
    vboxVrRegLaFree(g_VBoxVrLookasideList, pReg);
#else
    RTMemFree(pReg);
#endif
}

VBOXVREGDECL(void) VBoxVrListClear(PVBOXVR_LIST pList)
{
    PVBOXVR_REG pReg, pRegNext;

    RTListForEachSafe(&pList->ListHead, pReg, pRegNext, VBOXVR_REG, ListEntry)
    {
        vboxVrRegTerm(pReg);
    }
    VBoxVrListInit(pList);
}

#define VBOXVR_MEMTAG 'vDBV'

VBOXVREGDECL(int) VBoxVrInit()
{
    int32_t cNewRefs = ASMAtomicIncS32(&g_cVBoxVrInits);
    Assert(cNewRefs >= 1);
    Assert(cNewRefs == 1); /* <- debugging */
    if (cNewRefs > 1)
        return VINF_SUCCESS;

#ifndef VBOXVDBG_VR_LAL_DISABLE
    int rc = vboxVrLaCreate(&g_VBoxVrLookasideList, sizeof (VBOXVR_REG));
    if (!RT_SUCCESS(rc))
    {
        WARN(("ExInitializeLookasideListEx failed, rc (%d)", rc));
        return rc;
    }
#endif

    return VINF_SUCCESS;
}

VBOXVREGDECL(void) VBoxVrTerm()
{
    int32_t cNewRefs = ASMAtomicDecS32(&g_cVBoxVrInits);
    Assert(cNewRefs >= 0);
    if (cNewRefs > 0)
        return;

#ifndef VBOXVDBG_VR_LAL_DISABLE
    vboxVrLaDestroy(g_VBoxVrLookasideList);
#endif
}

typedef DECLCALLBACK(int) FNVBOXVR_CB_COMPARATOR(const VBOXVR_REG *pReg1, const VBOXVR_REG *pReg2);
typedef FNVBOXVR_CB_COMPARATOR *PFNVBOXVR_CB_COMPARATOR;

static DECLCALLBACK(int) vboxVrRegNonintersectedComparator(const RTRECT* pRect1, const RTRECT* pRect2)
{
    Assert(!VBoxRectIsIntersect(pRect1, pRect2));
    if (pRect1->yTop != pRect2->yTop)
        return pRect1->yTop - pRect2->yTop;
    return pRect1->xLeft - pRect2->xLeft;
}

#ifdef DEBUG_misha
static void vboxVrDbgListDoVerify(PVBOXVR_LIST pList)
{
    PVBOXVR_REG pReg1, pReg2;
    RTListForEach(&pList->ListHead, pReg1, VBOXVR_REG, ListEntry)
    {
        Assert(!VBoxRectIsZero(&pReg1->Rect));
        for (RTLISTNODE *pEntry2 = pReg1->ListEntry.pNext; pEntry2 != &pList->ListHead; pEntry2 = pEntry2->pNext)
        {
            pReg2 = PVBOXVR_REG_FROM_ENTRY(pEntry2);
            Assert(vboxVrRegNonintersectedComparator(&pReg1->Rect, &pReg2->Rect) < 0);
        }
    }
}

#define vboxVrDbgListVerify vboxVrDbgListDoVerify
#else
#define vboxVrDbgListVerify(_p) do {} while (0)
#endif

static int vboxVrListUniteIntersection(PVBOXVR_LIST pList, PVBOXVR_LIST pIntersection);

#define VBOXVR_INVALID_COORD (~0U)

DECLINLINE(void) vboxVrListRegAdd(PVBOXVR_LIST pList, PVBOXVR_REG pReg, PRTLISTNODE pPlace, bool fAfter)
{
    if (fAfter)
        RTListPrepend(pPlace, &pReg->ListEntry);
    else
        RTListAppend(pPlace, &pReg->ListEntry);
    ++pList->cEntries;
    vboxVrDbgListVerify(pList);
}

DECLINLINE(void) vboxVrListRegRemove(PVBOXVR_LIST pList, PVBOXVR_REG pReg)
{
    RTListNodeRemove(&pReg->ListEntry);
    --pList->cEntries;
    vboxVrDbgListVerify(pList);
}

static void vboxVrListRegAddOrder(PVBOXVR_LIST pList, PRTLISTNODE pMemberEntry, PVBOXVR_REG pReg)
{
    do
    {
        if (pMemberEntry != &pList->ListHead)
        {
            PVBOXVR_REG pMemberReg = PVBOXVR_REG_FROM_ENTRY(pMemberEntry);
            if (vboxVrRegNonintersectedComparator(&pMemberReg->Rect, &pReg->Rect) < 0)
            {
                pMemberEntry = pMemberEntry->pNext;
                continue;
            }
        }
        vboxVrListRegAdd(pList, pReg, pMemberEntry, false);
        break;
    } while (1);
}

static void vboxVrListAddNonintersected(PVBOXVR_LIST pList1, PVBOXVR_LIST pList2)
{
    PRTLISTNODE pEntry1 = pList1->ListHead.pNext;

    for (PRTLISTNODE pEntry2 = pList2->ListHead.pNext; pEntry2 != &pList2->ListHead; pEntry2 = pList2->ListHead.pNext)
    {
        PVBOXVR_REG pReg2 = PVBOXVR_REG_FROM_ENTRY(pEntry2);
        do {
            if (pEntry1 != &pList1->ListHead)
            {
                PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
                if (vboxVrRegNonintersectedComparator(&pReg1->Rect, &pReg2->Rect) < 0)
                {
                    pEntry1 = pEntry1->pNext;
                    continue;
                }
            }
            vboxVrListRegRemove(pList2, pReg2);
            vboxVrListRegAdd(pList1, pReg2, pEntry1, false);
            break;
        } while (1);
    }

    Assert(VBoxVrListIsEmpty(pList2));
}

static int vboxVrListRegIntersectSubstNoJoin(PVBOXVR_LIST pList1, PVBOXVR_REG pReg1, const RTRECT * pRect2)
{
    uint32_t topLim = VBOXVR_INVALID_COORD;
    uint32_t bottomLim = VBOXVR_INVALID_COORD;
    RTLISTNODE List;
    PVBOXVR_REG pBottomReg = NULL;
#ifdef DEBUG_misha
    RTRECT tmpRect = pReg1->Rect;
    vboxVrDbgListVerify(pList1);
#endif
    Assert(!VBoxRectIsZero(pRect2));

    RTListInit(&List);

    Assert(VBoxRectIsIntersect(&pReg1->Rect, pRect2));

    if (pReg1->Rect.yTop < pRect2->yTop)
    {
        Assert(pRect2->yTop < pReg1->Rect.yBottom);
        PVBOXVR_REG pRegResult = vboxVrRegCreate();
        pRegResult->Rect.yTop = pReg1->Rect.yTop;
        pRegResult->Rect.xLeft = pReg1->Rect.xLeft;
        pRegResult->Rect.yBottom = pRect2->yTop;
        pRegResult->Rect.xRight = pReg1->Rect.xRight;
        topLim = pRect2->yTop;
        RTListAppend(&List, &pRegResult->ListEntry);
    }

    if (pReg1->Rect.yBottom > pRect2->yBottom)
    {
        Assert(pRect2->yBottom > pReg1->Rect.yTop);
        PVBOXVR_REG pRegResult = vboxVrRegCreate();
        pRegResult->Rect.yTop = pRect2->yBottom;
        pRegResult->Rect.xLeft = pReg1->Rect.xLeft;
        pRegResult->Rect.yBottom = pReg1->Rect.yBottom;
        pRegResult->Rect.xRight = pReg1->Rect.xRight;
        bottomLim = pRect2->yBottom;
        pBottomReg = pRegResult;
    }

    if (pReg1->Rect.xLeft < pRect2->xLeft)
    {
        Assert(pRect2->xLeft < pReg1->Rect.xRight);
        PVBOXVR_REG pRegResult = vboxVrRegCreate();
        pRegResult->Rect.yTop = topLim == VBOXVR_INVALID_COORD ? pReg1->Rect.yTop : topLim;
        pRegResult->Rect.xLeft = pReg1->Rect.xLeft;
        pRegResult->Rect.yBottom = bottomLim == VBOXVR_INVALID_COORD ? pReg1->Rect.yBottom : bottomLim;
        pRegResult->Rect.xRight = pRect2->xLeft;
        RTListAppend(&List, &pRegResult->ListEntry);
    }

    if (pReg1->Rect.xRight > pRect2->xRight)
    {
        Assert(pRect2->xRight > pReg1->Rect.xLeft);
        PVBOXVR_REG pRegResult = vboxVrRegCreate();
        pRegResult->Rect.yTop = topLim == VBOXVR_INVALID_COORD ? pReg1->Rect.yTop : topLim;
        pRegResult->Rect.xLeft = pRect2->xRight;
        pRegResult->Rect.yBottom = bottomLim == VBOXVR_INVALID_COORD ? pReg1->Rect.yBottom : bottomLim;
        pRegResult->Rect.xRight = pReg1->Rect.xRight;
        RTListAppend(&List, &pRegResult->ListEntry);
    }

    if (pBottomReg)
        RTListAppend(&List, &pBottomReg->ListEntry);

    PRTLISTNODE pMemberEntry = pReg1->ListEntry.pNext;
    vboxVrListRegRemove(pList1, pReg1);
    vboxVrRegTerm(pReg1);

    if (RTListIsEmpty(&List))
        return VINF_SUCCESS; /* the region is covered by the pRect2 */

    PRTLISTNODE pEntry = List.pNext, pNext;
    for (; pEntry != &List; pEntry = pNext)
    {
        pNext = pEntry->pNext;
        PVBOXVR_REG pReg = PVBOXVR_REG_FROM_ENTRY(pEntry);

        vboxVrListRegAddOrder(pList1, pMemberEntry, pReg);
        pMemberEntry = pEntry->pNext; /* the following elements should go after the given pEntry since they are ordered already */
    }
    return VINF_SUCCESS;
}

/* @returns Entry to be used for continuing the rectangles iterations being made currently on the callback call.
 *          ListHead is returned to break the current iteration
 * @param ppNext specifies next reg entry to be used for iteration. the default is pReg1->ListEntry.pNext */
typedef DECLCALLBACK(PRTLISTNODE) FNVBOXVR_CB_INTERSECTED_VISITOR(PVBOXVR_LIST pList1, PVBOXVR_REG pReg1, const RTRECT * pRect2, void *pvContext, PRTLISTNODE *ppNext);
typedef FNVBOXVR_CB_INTERSECTED_VISITOR *PFNVBOXVR_CB_INTERSECTED_VISITOR;

static void vboxVrListVisitIntersected(PVBOXVR_LIST pList1, uint32_t cRects, const RTRECT *aRects, PFNVBOXVR_CB_INTERSECTED_VISITOR pfnVisitor, void* pvVisitor)
{
    PRTLISTNODE pEntry1 = pList1->ListHead.pNext;
    PRTLISTNODE pNext1;
    uint32_t iFirst2 = 0;

    for (; pEntry1 != &pList1->ListHead; pEntry1 = pNext1)
    {
        pNext1 = pEntry1->pNext;
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        for (uint32_t i = iFirst2; i < cRects; ++i)
        {
            const RTRECT *pRect2 = &aRects[i];
            if (VBoxRectIsZero(pRect2))
                continue;

            if (!VBoxRectIsIntersect(&pReg1->Rect, pRect2))
                continue;

            /* the visitor can modify the list 1, apply necessary adjustments after it */
            pEntry1 = pfnVisitor (pList1, pReg1, pRect2, pvVisitor, &pNext1);
            if (pEntry1 == &pList1->ListHead)
                break;
            else
                pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        }
    }
}

/* @returns Entry to be iterated next. ListHead is returned to break the iteration
 *
 */
typedef DECLCALLBACK(PRTLISTNODE) FNVBOXVR_CB_NONINTERSECTED_VISITOR(PVBOXVR_LIST pList1, PVBOXVR_REG pReg1, void *pvContext);
typedef FNVBOXVR_CB_NONINTERSECTED_VISITOR *PFNVBOXVR_CB_NONINTERSECTED_VISITOR;

static void vboxVrListVisitNonintersected(PVBOXVR_LIST pList1, uint32_t cRects, const RTRECT *aRects, PFNVBOXVR_CB_NONINTERSECTED_VISITOR pfnVisitor, void* pvVisitor)
{
    PRTLISTNODE pEntry1 = pList1->ListHead.pNext;
    PRTLISTNODE pNext1;
    uint32_t iFirst2 = 0;

    for (; pEntry1 != &pList1->ListHead; pEntry1 = pNext1)
    {
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        uint32_t i = iFirst2;
        for (; i < cRects; ++i)
        {
            const RTRECT *pRect2 = &aRects[i];
            if (VBoxRectIsZero(pRect2))
                continue;

            if (VBoxRectIsIntersect(&pReg1->Rect, pRect2))
                break;
        }

        if (i == cRects)
            pNext1 = pfnVisitor(pList1, pReg1, pvVisitor);
        else
            pNext1 = pEntry1->pNext;
    }
}

static void vboxVrListJoinRectsHV(PVBOXVR_LIST pList, bool fHorizontal)
{
    PRTLISTNODE pNext1, pNext2;

    for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pNext1)
    {
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        pNext1 = pEntry1->pNext;
        for (PRTLISTNODE pEntry2 = pEntry1->pNext; pEntry2 != &pList->ListHead; pEntry2 = pNext2)
        {
            PVBOXVR_REG pReg2 = PVBOXVR_REG_FROM_ENTRY(pEntry2);
            pNext2 = pEntry2->pNext;
            if (fHorizontal)
            {
                if (pReg1->Rect.yTop == pReg2->Rect.yTop)
                {
                    if (pReg1->Rect.xRight == pReg2->Rect.xLeft)
                    {
                        /* join rectangles */
                        vboxVrListRegRemove(pList, pReg2);
                        if (pReg1->Rect.yBottom > pReg2->Rect.yBottom)
                        {
                            int32_t oldRight1 = pReg1->Rect.xRight;
                            int32_t oldBottom1 = pReg1->Rect.yBottom;
                            pReg1->Rect.xRight = pReg2->Rect.xRight;
                            pReg1->Rect.yBottom = pReg2->Rect.yBottom;

                            vboxVrDbgListVerify(pList);

                            pReg2->Rect.xLeft = pReg1->Rect.xLeft;
                            pReg2->Rect.yTop = pReg1->Rect.yBottom;
                            pReg2->Rect.xRight = oldRight1;
                            pReg2->Rect.yBottom = oldBottom1;
                            vboxVrListRegAddOrder(pList, pReg1->ListEntry.pNext, pReg2);
                            /* restart the pNext1 & pNext2 since regs are splitted into smaller ones in y dimension
                             * and thus can match one of the previous rects */
                            pNext1 = pList->ListHead.pNext;
                            break;
                        }
                        else if (pReg1->Rect.yBottom < pReg2->Rect.yBottom)
                        {
                            pReg1->Rect.xRight = pReg2->Rect.xRight;
                            vboxVrDbgListVerify(pList);
                            pReg2->Rect.yTop = pReg1->Rect.yBottom;
                            vboxVrListRegAddOrder(pList, pReg1->ListEntry.pNext, pReg2);
                            /* restart the pNext1 & pNext2 since regs are splitted into smaller ones in y dimension
                             * and thus can match one of the previous rects */
                            pNext1 = pList->ListHead.pNext;
                            break;
                        }
                        else
                        {
                            pReg1->Rect.xRight = pReg2->Rect.xRight;
                            vboxVrDbgListVerify(pList);
                            /* reset the pNext1 since it could be the pReg2 being destroyed */
                            pNext1 = pEntry1->pNext;
                            /* pNext2 stays the same since it is pReg2->ListEntry.pNext, which is kept intact */
                            vboxVrRegTerm(pReg2);
                        }
                    }
                    continue;
                }
                else if (pReg1->Rect.yBottom == pReg2->Rect.yBottom)
                {
                    Assert(pReg1->Rect.yTop < pReg2->Rect.yTop); /* <- since pReg1 > pReg2 && pReg1->Rect.yTop != pReg2->Rect.yTop*/
                    if (pReg1->Rect.xRight == pReg2->Rect.xLeft)
                    {
                        /* join rectangles */
                        vboxVrListRegRemove(pList, pReg2);

                        pReg1->Rect.yBottom = pReg2->Rect.yTop;
                        vboxVrDbgListVerify(pList);
                        pReg2->Rect.xLeft = pReg1->Rect.xLeft;

                        vboxVrListRegAddOrder(pList, pNext2, pReg2);

                        /* restart the pNext1 & pNext2 since regs are splitted into smaller ones in y dimension
                         * and thus can match one of the previous rects */
                        pNext1 = pList->ListHead.pNext;
                        break;
                    }
                    else if (pReg1->Rect.xLeft == pReg2->Rect.xRight)
                    {
                        /* join rectangles */
                        vboxVrListRegRemove(pList, pReg2);

                        pReg1->Rect.yBottom = pReg2->Rect.yTop;
                        vboxVrDbgListVerify(pList);
                        pReg2->Rect.xRight = pReg1->Rect.xRight;

                        vboxVrListRegAddOrder(pList, pNext2, pReg2);

                        /* restart the pNext1 & pNext2 since regs are splitted into smaller ones in y dimension
                         * and thus can match one of the previous rects */
                        pNext1 = pList->ListHead.pNext;
                        break;
                    }
                    continue;
                }
            }
            else
            {
                if (pReg1->Rect.yBottom == pReg2->Rect.yTop)
                {
                    if (pReg1->Rect.xLeft == pReg2->Rect.xLeft)
                    {
                        if (pReg1->Rect.xRight == pReg2->Rect.xRight)
                        {
                            /* join rects */
                            vboxVrListRegRemove(pList, pReg2);

                            pReg1->Rect.yBottom = pReg2->Rect.yBottom;
                            vboxVrDbgListVerify(pList);

                            /* reset the pNext1 since it could be the pReg2 being destroyed */
                            pNext1 = pEntry1->pNext;
                            /* pNext2 stays the same since it is pReg2->ListEntry.pNext, which is kept intact */
                            vboxVrRegTerm(pReg2);
                            continue;
                        }
                        /* no more to be done for for pReg1 */
                        break;
                    }
                    else if (pReg1->Rect.xRight > pReg2->Rect.xLeft)
                    {
                        /* no more to be done for for pReg1 */
                        break;
                    }

                    continue;
                }
                else if (pReg1->Rect.yBottom < pReg2->Rect.yTop)
                {
                    /* no more to be done for for pReg1 */
                    break;
                }
            }
        }
    }
}

static void vboxVrListJoinRects(PVBOXVR_LIST pList)
{
    vboxVrListJoinRectsHV(pList, true);
    vboxVrListJoinRectsHV(pList, false);
}

typedef struct VBOXVR_CBDATA_SUBST
{
    int rc;
    bool fChanged;
} VBOXVR_CBDATA_SUBST, *PVBOXVR_CBDATA_SUBST;

static DECLCALLBACK(PRTLISTNODE) vboxVrListSubstNoJoinCb(PVBOXVR_LIST pList, PVBOXVR_REG pReg1, const RTRECT *pRect2, void *pvContext, PRTLISTNODE *ppNext)
{
    PVBOXVR_CBDATA_SUBST pData = (PVBOXVR_CBDATA_SUBST)pvContext;
    /* store the prev to get the new pNext out of it*/
    PRTLISTNODE pPrev = pReg1->ListEntry.pPrev;
    pData->fChanged = true;

    Assert(VBoxRectIsIntersect(&pReg1->Rect, pRect2));

    /* NOTE: the pReg1 will be invalid after the vboxVrListRegIntersectSubstNoJoin call!!! */
    int rc = vboxVrListRegIntersectSubstNoJoin(pList, pReg1, pRect2);
    if (RT_SUCCESS(rc))
    {
        *ppNext = pPrev->pNext;
        return &pList->ListHead;
    }
    WARN(("vboxVrListRegIntersectSubstNoJoin failed!"));
    Assert(!RT_SUCCESS(rc));
    pData->rc = rc;
    *ppNext = &pList->ListHead;
    return &pList->ListHead;
}

static int vboxVrListSubstNoJoin(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
    if (pfChanged)
        *pfChanged = false;

    if (VBoxVrListIsEmpty(pList))
        return VINF_SUCCESS;

    VBOXVR_CBDATA_SUBST Data;
    Data.rc = VINF_SUCCESS;
    Data.fChanged = false;

    vboxVrListVisitIntersected(pList, cRects, aRects, vboxVrListSubstNoJoinCb, &Data);
    if (!RT_SUCCESS(Data.rc))
    {
        WARN(("vboxVrListVisitIntersected failed!"));
        return Data.rc;
    }

    if (pfChanged)
        *pfChanged = Data.fChanged;

    return VINF_SUCCESS;
}

#if 0
static const RTRECT * vboxVrRectsOrder(uint32_t cRects, const RTRECT * aRects)
{
#ifdef DEBUG
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            RTRECT *pRectI = &aRects[i];
            for (uint32_t j = i + 1; j < cRects; ++j)
            {
                RTRECT *pRectJ = &aRects[j];
                Assert(!VBoxRectIsIntersect(pRectI, pRectJ));
            }
        }
    }
#endif

    RTRECT * pRects = (RTRECT *)aRects;
    /* check if rects are ordered already */
    for (uint32_t i = 0; i < cRects - 1; ++i)
    {
        RTRECT *pRect1 = &pRects[i];
        RTRECT *pRect2 = &pRects[i+1];
        if (vboxVrRegNonintersectedComparator(pRect1, pRect2) < 0)
            continue;

        WARN(("rects are unoreded!"));

        if (pRects == aRects)
        {
            pRects = (RTRECT *)RTMemAlloc(sizeof (RTRECT) * cRects);
            if (!pRects)
            {
                WARN(("RTMemAlloc failed!"));
                return NULL;
            }

            memcpy(pRects, aRects, sizeof (RTRECT) * cRects);
        }

        Assert(pRects != aRects);

        int j = (int)i - 1;
        do {
            RTRECT Tmp = *pRect1;
            *pRect1 = *pRect2;
            *pRect2 = Tmp;

            if (j < 0)
                break;

            if (vboxVrRegNonintersectedComparator(pRect1, pRect1-1) > 0)
                break;

            pRect2 = pRect1--;
            --j;
        } while (1);
    }

    return pRects;
}
#endif

VBOXVREGDECL(void) VBoxVrListTranslate(PVBOXVR_LIST pList, int32_t x, int32_t y)
{
    for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pEntry1->pNext)
    {
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        VBoxRectTranslate(&pReg1->Rect, x, y);
    }
}

static DECLCALLBACK(PRTLISTNODE) vboxVrListIntersectNoJoinNonintersectedCb(PVBOXVR_LIST pList1, PVBOXVR_REG pReg1, void *pvContext)
{
    VBOXVR_CBDATA_SUBST *pData = (VBOXVR_CBDATA_SUBST*)pvContext;

    PRTLISTNODE pNext = pReg1->ListEntry.pNext;

    vboxVrDbgListVerify(pList1);

    vboxVrListRegRemove(pList1, pReg1);
    vboxVrRegTerm(pReg1);

    vboxVrDbgListVerify(pList1);

    pData->fChanged = true;

    return pNext;
}

static DECLCALLBACK(PRTLISTNODE) vboxVrListIntersectNoJoinIntersectedCb(PVBOXVR_LIST pList1, PVBOXVR_REG pReg1, const RTRECT *pRect2, void *pvContext, PRTLISTNODE *ppNext)
{
    PVBOXVR_CBDATA_SUBST pData = (PVBOXVR_CBDATA_SUBST)pvContext;
    pData->fChanged = true;

    vboxVrDbgListVerify(pList1);

    PRTLISTNODE pMemberEntry = pReg1->ListEntry.pNext;

    Assert(VBoxRectIsIntersect(&pReg1->Rect, pRect2));
    Assert(!VBoxRectIsZero(pRect2));

    vboxVrListRegRemove(pList1, pReg1);
    VBoxRectIntersect(&pReg1->Rect, pRect2);
    Assert(!VBoxRectIsZero(&pReg1->Rect));

    vboxVrListRegAddOrder(pList1, pMemberEntry, pReg1);

    vboxVrDbgListVerify(pList1);

    return &pReg1->ListEntry;
}

static int vboxVrListIntersectNoJoin(PVBOXVR_LIST pList, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    bool fChanged = false;
    *pfChanged = false;

    if (VBoxVrListIsEmpty(pList))
        return VINF_SUCCESS;

    if (VBoxVrListIsEmpty(pList2))
    {
        if (pfChanged)
            *pfChanged = true;

        VBoxVrListClear(pList);
        return VINF_SUCCESS;
    }

    PRTLISTNODE pNext1;

    for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pNext1)
    {
        pNext1 = pEntry1->pNext;
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        RTRECT RegRect1 = pReg1->Rect;
        PRTLISTNODE pMemberEntry = pReg1->ListEntry.pNext;

        for (const RTLISTNODE *pEntry2 = pList2->ListHead.pNext; pEntry2 != &pList2->ListHead; pEntry2 = pEntry2->pNext)
        {
            const VBOXVR_REG *pReg2 = PVBOXVR_REG_FROM_ENTRY(pEntry2);
            const RTRECT *pRect2 = &pReg2->Rect;

            if (!VBoxRectIsIntersect(&RegRect1, pRect2))
                continue;

            if (pReg1)
            {
                if (!VBoxRectCmp(&pReg1->Rect, pRect2))
                {
                    /* no change, and we can break the iteration here */

                    /* zero up the pReg1 to mark it as intersected (see the code after this inner loop) */
                    pReg1 = NULL;
                    break;
                }
                /* @todo: this can have false-alarming sometimes if the separated rects will then be joind into the original rect,
                 * so far this should not be a problem for VReg clients, so keep it this way for now  */
                fChanged = true;

                /* re-use the reg entry */
                vboxVrListRegRemove(pList, pReg1);
                VBoxRectIntersect(&pReg1->Rect, pRect2);
                Assert(!VBoxRectIsZero(&pReg1->Rect));

                vboxVrListRegAddOrder(pList, pMemberEntry, pReg1);
                pReg1 = NULL;
            }
            else
            {
                Assert(fChanged); /* <- should be set by the if branch above */
                PVBOXVR_REG pReg = vboxVrRegCreate();
                if (!pReg)
                {
                    WARN(("vboxVrRegCreate failed!"));
                    return VERR_NO_MEMORY;
                }
                VBoxRectIntersected(&RegRect1, pRect2, &pReg->Rect);
                Assert(!VBoxRectIsZero(&pReg->Rect));
                vboxVrListRegAddOrder(pList, pList->ListHead.pNext, pReg);
            }
        }

        if (pReg1)
        {
            /* the region has no intersections, remove it */
            vboxVrListRegRemove(pList, pReg1);
            vboxVrRegTerm(pReg1);
            fChanged = true;
        }
    }

    *pfChanged = fChanged;
    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrListIntersect(PVBOXVR_LIST pList, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    if (pfChanged)
        *pfChanged = false;

    int rc = vboxVrListIntersectNoJoin(pList, pList2, pfChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vboxVrListSubstNoJoin failed!"));
        return rc;
    }

    if (*pfChanged)
    {
        vboxVrListJoinRects(pList);
    }

    return rc;
}

VBOXVREGDECL(int) VBoxVrListRectsIntersect(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
    if (pfChanged)
        *pfChanged = false;

    if (VBoxVrListIsEmpty(pList))
        return VINF_SUCCESS;

    if (!cRects)
    {
        if (pfChanged)
            *pfChanged = true;

        VBoxVrListClear(pList);
        return VINF_SUCCESS;
    }

    /* we perform intersection using lists because the algorythm axpects the rects to be non-intersected,
     * which list guaranties to us */

    VBOXVR_LIST TmpList;
    VBoxVrListInit(&TmpList);

    int rc = VBoxVrListRectsAdd(&TmpList, cRects, aRects, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxVrListIntersect(pList, &TmpList, pfChanged);
        if (!RT_SUCCESS(rc))
        {
            WARN(("VBoxVrListIntersect failed! rc %d", rc));
        }
    }
    else
    {
        WARN(("VBoxVrListRectsAdd failed, rc %d", rc));
    }
    VBoxVrListClear(&TmpList);

    return rc;
}

VBOXVREGDECL(int) VBoxVrListRectsSubst(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
#if 0
    const RTRECT * pRects = vboxVrRectsOrder(cRects, aRects);
    if (!pRects)
    {
        WARN(("vboxVrRectsOrder failed!"));
        return VERR_NO_MEMORY;
    }
#endif

    bool fChanged = false;

    int rc = vboxVrListSubstNoJoin(pList, cRects, aRects, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vboxVrListSubstNoJoin failed!"));
        goto done;
    }

    if (fChanged)
        goto done;

    vboxVrListJoinRects(pList);

done:
#if 0
    if (pRects != aRects)
        RTMemFree(pRects);
#endif

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) VBoxVrListRectsSet(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
    if (pfChanged)
        *pfChanged = false;

    if (!cRects && VBoxVrListIsEmpty(pList))
    {
        return VINF_SUCCESS;
    }

    /* @todo: fChanged will have false alarming here, fix if needed */
    VBoxVrListClear(pList);

    int rc = VBoxVrListRectsAdd(pList, cRects, aRects, NULL);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxVrListRectsSet failed rc %d", rc));
        return rc;
    }

    if (pfChanged)
        *pfChanged = true;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrListRectsAdd(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
    uint32_t cCovered = 0;

    if (pfChanged)
        *pfChanged = false;

#if 0
#ifdef DEBUG
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            RTRECT *pRectI = &aRects[i];
            for (uint32_t j = i + 1; j < cRects; ++j)
            {
                RTRECT *pRectJ = &aRects[j];
                Assert(!VBoxRectIsIntersect(pRectI, pRectJ));
            }
        }
    }
#endif
#endif

    /* early sort out the case when there are no new rects */
    for (uint32_t i = 0; i < cRects; ++i)
    {
        if (VBoxRectIsZero(&aRects[i]))
        {
            cCovered++;
            continue;
        }

        for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pEntry1->pNext)
        {
            PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);

            if (VBoxRectIsCoveres(&pReg1->Rect, &aRects[i]))
            {
                cCovered++;
                break;
            }
        }
    }

    if (cCovered == cRects)
        return VINF_SUCCESS;

    /* rects are not covered, need to go the slow way */

    VBOXVR_LIST DiffList;
    VBoxVrListInit(&DiffList);
    RTRECT * pListRects = NULL;
    uint32_t cAllocatedRects = 0;
    bool fNeedRectreate = true;
    bool fChanged = false;
    int rc = VINF_SUCCESS;

    for (uint32_t i = 0; i < cRects; ++i)
    {
        if (VBoxRectIsZero(&aRects[i]))
            continue;

        PVBOXVR_REG pReg = vboxVrRegCreate();
        if (!pReg)
        {
            WARN(("vboxVrRegCreate failed!"));
            rc = VERR_NO_MEMORY;
            break;
        }
        pReg->Rect = aRects[i];

        uint32_t cListRects = VBoxVrListRectsCount(pList);
        if (!cListRects)
        {
            vboxVrListRegAdd(pList, pReg, &pList->ListHead, false);
            fChanged = true;
            continue;
        }
        else
        {
            Assert(VBoxVrListIsEmpty(&DiffList));
            vboxVrListRegAdd(&DiffList, pReg, &DiffList.ListHead, false);
        }

        if (cAllocatedRects < cListRects)
        {
            cAllocatedRects = cListRects + cRects;
            Assert(fNeedRectreate);
            if (pListRects)
                RTMemFree(pListRects);
            pListRects = (RTRECT *)RTMemAlloc(sizeof (RTRECT) * cAllocatedRects);
            if (!pListRects)
            {
                WARN(("RTMemAlloc failed!"));
                rc = VERR_NO_MEMORY;
                break;
            }
        }


        if (fNeedRectreate)
        {
            rc = VBoxVrListRectsGet(pList, cListRects, pListRects);
            Assert(rc == VINF_SUCCESS);
            fNeedRectreate = false;
        }

        bool fDummyChanged = false;
        rc = vboxVrListSubstNoJoin(&DiffList, cListRects, pListRects, &fDummyChanged);
        if (!RT_SUCCESS(rc))
        {
            WARN(("vboxVrListSubstNoJoin failed!"));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (!VBoxVrListIsEmpty(&DiffList))
        {
            vboxVrListAddNonintersected(pList, &DiffList);
            fNeedRectreate = true;
            fChanged = true;
        }

        Assert(VBoxVrListIsEmpty(&DiffList));
    }

    if (pListRects)
        RTMemFree(pListRects);

    Assert(VBoxVrListIsEmpty(&DiffList) || rc != VINF_SUCCESS);
    VBoxVrListClear(&DiffList);

    if (fChanged)
        vboxVrListJoinRects(pList);

    if (pfChanged)
        *pfChanged = fChanged;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrListRectsGet(PVBOXVR_LIST pList, uint32_t cRects, RTRECT * aRects)
{
    if (cRects < VBoxVrListRectsCount(pList))
        return VERR_BUFFER_OVERFLOW;

    uint32_t i = 0;
    for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pEntry1->pNext, ++i)
    {
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        aRects[i] = pReg1->Rect;
    }
    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrListCmp(const VBOXVR_LIST *pList1, const VBOXVR_LIST *pList2)
{
    int cTmp = pList1->cEntries - pList2->cEntries;
    if (cTmp)
        return cTmp;

    PVBOXVR_REG pReg1, pReg2;

    for (pReg1 = RTListNodeGetNext(&pList1->ListHead, VBOXVR_REG, ListEntry),
            pReg2 = RTListNodeGetNext(&pList2->ListHead, VBOXVR_REG, ListEntry);
            !RTListNodeIsDummy(&pList1->ListHead, pReg1, VBOXVR_REG, ListEntry);
            pReg1 = RT_FROM_MEMBER(pReg1->ListEntry.pNext, VBOXVR_REG, ListEntry),
            pReg2 = RT_FROM_MEMBER(pReg2->ListEntry.pNext, VBOXVR_REG, ListEntry))
    {
        Assert(!RTListNodeIsDummy(&pList2->ListHead, pReg2, VBOXVR_REG, ListEntry));
        cTmp = VBoxRectCmp(&pReg1->Rect, &pReg2->Rect);
        if (cTmp)
            return cTmp;
    }
    Assert(RTListNodeIsDummy(&pList2->ListHead, pReg2, VBOXVR_REG, ListEntry));
    return 0;
}

VBOXVREGDECL(int) VBoxVrListClone(const VBOXVR_LIST *pList, VBOXVR_LIST *pDstList)
{
    VBoxVrListInit(pDstList);
    const VBOXVR_REG *pReg;
    RTListForEach(&pList->ListHead, pReg, const VBOXVR_REG, ListEntry)
    {
        PVBOXVR_REG pDstReg = (PVBOXVR_REG)vboxVrRegLaAlloc(g_VBoxVrLookasideList);
        if (!pDstReg)
        {
            WARN(("vboxVrRegLaAlloc failed"));
            VBoxVrListClear(pDstList);
            return VERR_NO_MEMORY;
        }
        pDstReg->Rect = pReg->Rect;
        vboxVrListRegAdd(pDstList, pDstReg, &pDstList->ListHead, true /*bool fAfter*/);
    }

    Assert(pDstList->cEntries == pList->cEntries);

    return VINF_SUCCESS;
}

VBOXVREGDECL(void) VBoxVrCompositorInit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_ENTRY_RELEASED pfnEntryReleased)
{
    RTListInit(&pCompositor->List);
    pCompositor->pfnEntryReleased = pfnEntryReleased;
}

VBOXVREGDECL(void) VBoxVrCompositorRegionsClear(PVBOXVR_COMPOSITOR pCompositor, bool *pfChanged)
{
    bool fChanged = false;
    PVBOXVR_COMPOSITOR_ENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pCompositor->List, pEntry, pEntryNext, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        VBoxVrCompositorEntryRemove(pCompositor, pEntry);
        fChanged = true;
    }

    if (pfChanged)
        *pfChanged = fChanged;
}

VBOXVREGDECL(void) VBoxVrCompositorClear(PVBOXVR_COMPOSITOR pCompositor)
{
    VBoxVrCompositorRegionsClear(pCompositor, NULL);
}

DECLINLINE(void) vboxVrCompositorEntryRelease(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, PVBOXVR_COMPOSITOR_ENTRY pReplacingEntry)
{
    if (--pEntry->cRefs)
    {
        Assert(pEntry->cRefs < UINT32_MAX/2);
        return;
    }

    Assert(!VBoxVrCompositorEntryIsInList(pEntry));

    if (pCompositor->pfnEntryReleased)
        pCompositor->pfnEntryReleased(pCompositor, pEntry, pReplacingEntry);
}

DECLINLINE(void) vboxVrCompositorEntryAcquire(PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    ++pEntry->cRefs;
}

DECLINLINE(void) vboxVrCompositorEntryAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    RTListPrepend(&pCompositor->List, &pEntry->Node);
    vboxVrCompositorEntryAcquire(pEntry);
}

DECLINLINE(void) vboxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, PVBOXVR_COMPOSITOR_ENTRY pReplacingEntry)
{
    RTListNodeRemove(&pEntry->Node);
    vboxVrCompositorEntryRelease(pCompositor, pEntry, pReplacingEntry);
}

VBOXVREGDECL(void) VBoxVrCompositorEntryInit(PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    VBoxVrListInit(&pEntry->Vr);
    pEntry->cRefs = 0;
}

VBOXVREGDECL(bool) VBoxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    vboxVrCompositorEntryAcquire(pEntry);

    if (!VBoxVrCompositorEntryIsInList(pEntry))
    {
        vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
        return false;
    }
    VBoxVrListClear(&pEntry->Vr);
    vboxVrCompositorEntryRemove(pCompositor, pEntry, NULL);
    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
    return true;
}

static int vboxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT * paRects, bool *pfChanged)
{
    bool fChanged;
    vboxVrCompositorEntryAcquire(pEntry);

    int rc = VBoxVrListRectsSubst(&pEntry->Vr, cRects, paRects, &fChanged);
    if (RT_SUCCESS(rc))
    {
        if (VBoxVrListIsEmpty(&pEntry->Vr))
        {
            Assert(fChanged);
            vboxVrCompositorEntryRemove(pCompositor, pEntry, NULL);
        }
        if (pfChanged)
            *pfChanged = false;
    }
    else
        WARN(("VBoxVrListRectsSubst failed, rc %d", rc));

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, PVBOXVR_COMPOSITOR_ENTRY *ppReplacedEntry, uint32_t *pfChangeFlags)
{
    bool fOthersChanged = false, fCurChanged = false, fEntryChanged = false, fEntryWasInList = false, fEntryReplaced = false;
    PVBOXVR_COMPOSITOR_ENTRY pCur, pNext;
    int rc = VINF_SUCCESS;

    if (pEntry)
        vboxVrCompositorEntryAcquire(pEntry);

    if (!cRects)
    {
        if (pfChangeFlags)
            *pfChangeFlags = 0;
        if (pEntry)
            vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
        return VINF_SUCCESS;
    }

    if (pEntry)
    {
        fEntryWasInList = VBoxVrCompositorEntryIsInList(pEntry);
        rc = VBoxVrListRectsAdd(&pEntry->Vr, cRects, paRects, &fEntryChanged);
        if (RT_SUCCESS(rc))
        {
            if (VBoxVrListIsEmpty(&pEntry->Vr))
            {
//                WARN(("Empty rectangles passed in, is it expected?"));
                if (pfChangeFlags)
                    *pfChangeFlags = 0;
                vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
                return VINF_SUCCESS;
            }
        }
        else
        {
            WARN(("VBoxVrListRectsAdd failed, rc %d", rc));
            vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
            return rc;
        }

        Assert(!VBoxVrListIsEmpty(&pEntry->Vr));
    }
    else
    {
        fEntryChanged = true;
    }

    RTListForEachSafe(&pCompositor->List, pCur, pNext, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        Assert(!VBoxVrListIsEmpty(&pCur->Vr));
        if (pCur == pEntry)
        {
            Assert(fEntryWasInList);
        }
        else
        {
            if (pEntry && !VBoxVrListCmp(&pCur->Vr, &pEntry->Vr))
            {
                VBoxVrListClear(&pCur->Vr);
                vboxVrCompositorEntryRemove(pCompositor, pCur, pEntry);
                if (ppReplacedEntry)
                    *ppReplacedEntry = pCur;
                fEntryReplaced = true;
                break;
            }
            else
            {
                rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pCur, cRects, paRects, &fCurChanged);
                if (RT_SUCCESS(rc))
                    fOthersChanged |= fCurChanged;
                else
                {
                    WARN(("vboxVrCompositorEntryRegionsSubst failed, rc %d", rc));
                    return rc;
                }
            }
        }
    }

    AssertRC(rc);

    if (pEntry)
    {
        if (!fEntryWasInList)
        {
            Assert(!VBoxVrListIsEmpty(&pEntry->Vr));
            vboxVrCompositorEntryAdd(pCompositor, pEntry);
        }
        vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
    }

    if (pfChangeFlags)
    {
        uint32_t fFlags = 0;
        if (fOthersChanged)
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_OTHER_ENTRIES_REGIONS_CHANGED;
        else if (fEntryReplaced)
        {
            Assert(fEntryChanged);
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED;
        }
        else if (fEntryChanged)
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED;

        if (!fEntryWasInList)
            Assert(fEntryChanged);

        *pfChangeFlags = fFlags;
    }

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT * paRects, bool *pfChanged)
{
    if (!pEntry)
    {
        WARN(("VBoxVrCompositorEntryRegionsSubst called with zero entry, unsupported!"));
        if (pfChanged)
            *pfChanged = false;
        return VERR_INVALID_PARAMETER;
    }

    vboxVrCompositorEntryAcquire(pEntry);

    if (VBoxVrListIsEmpty(&pEntry->Vr))
    {
        if (pfChanged)
            *pfChanged = false;
        vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
        return VINF_SUCCESS;

    }

    int rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pEntry, cRects, paRects, pfChanged);
    if (!RT_SUCCESS(rc))
        WARN(("pfChanged failed, rc %d", rc));

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);

    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSet(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, bool *pfChanged)
{
    if (!pEntry)
    {
        WARN(("VBoxVrCompositorEntryRegionsSet called with zero entry, unsupported!"));
        if (pfChanged)
            *pfChanged = false;
        return VERR_INVALID_PARAMETER;
    }

    vboxVrCompositorEntryAcquire(pEntry);

    bool fChanged = false, fCurChanged = false;
    uint32_t fChangeFlags = 0;
    int rc;
    fCurChanged = VBoxVrCompositorEntryRemove(pCompositor, pEntry);
    fChanged |= fCurChanged;

    rc = VBoxVrCompositorEntryRegionsAdd(pCompositor, pEntry, cRects, paRects, NULL, &fChangeFlags);
    if (RT_SUCCESS(rc))
    {
        fChanged |= !!fChangeFlags;
        if (pfChanged)
            *pfChanged = fChanged;
    }
    else
        WARN(("VBoxVrCompositorEntryRegionsAdd failed, rc %d", rc));

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryListIntersect(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    vboxVrCompositorEntryAcquire(pEntry);

    if (VBoxVrCompositorEntryIsInList(pEntry))
    {
        rc = VBoxVrListIntersect(&pEntry->Vr, pList2, &fChanged);
        if (RT_SUCCESS(rc))
        {
            if (VBoxVrListIsEmpty(&pEntry->Vr))
            {
                Assert(fChanged);
                vboxVrCompositorEntryRemove(pCompositor, pEntry, NULL);
            }
        }
        else
        {
            WARN(("VBoxVrListRectsIntersect failed, rc %d", rc));
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);

    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsIntersect(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, bool *pfChanged)
{
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    vboxVrCompositorEntryAcquire(pEntry);

    if (VBoxVrCompositorEntryIsInList(pEntry))
    {
        rc = VBoxVrListRectsIntersect(&pEntry->Vr, cRects, paRects, &fChanged);
        if (RT_SUCCESS(rc))
        {
            if (VBoxVrListIsEmpty(&pEntry->Vr))
            {
                Assert(fChanged);
                vboxVrCompositorEntryRemove(pCompositor, pEntry, NULL);
            }
        }
        else
        {
            WARN(("VBoxVrListRectsIntersect failed, rc %d", rc));
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);

    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryListIntersectAll(PVBOXVR_COMPOSITOR pCompositor, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    VBOXVR_COMPOSITOR_ITERATOR Iter;
    VBoxVrCompositorIterInit(pCompositor, &Iter);
    PVBOXVR_COMPOSITOR_ENTRY pEntry;
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    while ((pEntry = VBoxVrCompositorIterNext(&Iter)) != NULL)
    {
        bool fTmpChanged = false;
        int tmpRc = VBoxVrCompositorEntryListIntersect(pCompositor, pEntry, pList2, &fTmpChanged);
        if (RT_SUCCESS(tmpRc))
        {
            fChanged |= fChanged;
        }
        else
        {
            WARN(("VBoxVrCompositorEntryRegionsIntersect failed, rc %d", tmpRc));
            rc = tmpRc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsIntersectAll(PVBOXVR_COMPOSITOR pCompositor, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    VBOXVR_COMPOSITOR_ITERATOR Iter;
    VBoxVrCompositorIterInit(pCompositor, &Iter);
    PVBOXVR_COMPOSITOR_ENTRY pEntry;
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    while ((pEntry = VBoxVrCompositorIterNext(&Iter)) != NULL)
    {
        bool fTmpChanged = false;
        int tmpRc = VBoxVrCompositorEntryRegionsIntersect(pCompositor, pEntry, cRegions, paRegions, &fTmpChanged);
        if (RT_SUCCESS(tmpRc))
        {
            fChanged |= fChanged;
        }
        else
        {
            WARN(("VBoxVrCompositorEntryRegionsIntersect failed, rc %d", tmpRc));
            rc = tmpRc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsTranslate(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, int32_t x, int32_t y, bool *pfChanged)
{
    if (!pEntry)
    {
        WARN(("VBoxVrCompositorEntryRegionsTranslate called with zero entry, unsupported!"));
        if (pfChanged)
            *pfChanged = false;
        return VERR_INVALID_PARAMETER;
    }

    vboxVrCompositorEntryAcquire(pEntry);

    if ((!x && !y)
            || !VBoxVrCompositorEntryIsInList(pEntry))
    {
        if (pfChanged)
            *pfChanged = false;

        vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);
        return VINF_SUCCESS;
    }

    VBoxVrListTranslate(&pEntry->Vr, x, y);

    Assert(!VBoxVrListIsEmpty(&pEntry->Vr));

    PVBOXVR_COMPOSITOR_ENTRY pCur;
    uint32_t cRects = 0;
    RTRECT *paRects = NULL;
    int rc = VINF_SUCCESS;
    RTListForEach(&pCompositor->List, pCur, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        Assert(!VBoxVrListIsEmpty(&pCur->Vr));

        if (pCur == pEntry)
            continue;

        if (!paRects)
        {
            cRects = VBoxVrListRectsCount(&pEntry->Vr);
            Assert(cRects);
            paRects = (RTRECT*)RTMemAlloc(cRects * sizeof (RTRECT));
            if (!paRects)
            {
                WARN(("RTMemAlloc failed!"));
                rc = VERR_NO_MEMORY;
                break;
            }

            rc = VBoxVrListRectsGet(&pEntry->Vr, cRects, paRects);
            if (!RT_SUCCESS(rc))
            {
                WARN(("VBoxVrListRectsGet failed! rc %d", rc));
                break;
            }
        }

        rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pCur, cRects, paRects, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("vboxVrCompositorEntryRegionsSubst failed! rc %d", rc));
            break;
        }
    }

    if (pfChanged)
        *pfChanged = true;

    if (paRects)
        RTMemFree(paRects);

    vboxVrCompositorEntryRelease(pCompositor, pEntry, NULL);

    return rc;
}

VBOXVREGDECL(void) VBoxVrCompositorVisit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor)
{
    PVBOXVR_COMPOSITOR_ENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pCompositor->List, pEntry, pEntryNext, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        if (!pfnVisitor(pCompositor, pEntry, pvVisitor))
            return;
    }
}



#define VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED UINT32_MAX

static int crVrScrCompositorRectsAssignBuffer(PVBOXVR_SCR_COMPOSITOR pCompositor, uint32_t cRects)
{
    Assert(cRects);

    if (pCompositor->cRectsBuffer >= cRects)
    {
        pCompositor->cRects = cRects;
        return VINF_SUCCESS;
    }

    if (pCompositor->cRectsBuffer)
    {
        Assert(pCompositor->paSrcRects);
        RTMemFree(pCompositor->paSrcRects);
        pCompositor->paSrcRects = NULL;
        Assert(pCompositor->paDstRects);
        RTMemFree(pCompositor->paDstRects);
        pCompositor->paDstRects = NULL;
        Assert(pCompositor->paDstUnstretchedRects);
        RTMemFree(pCompositor->paDstUnstretchedRects);
        pCompositor->paDstUnstretchedRects = NULL;
    }
    else
    {
        Assert(!pCompositor->paSrcRects);
        Assert(!pCompositor->paDstRects);
        Assert(!pCompositor->paDstUnstretchedRects);
    }

    pCompositor->paSrcRects = (PRTRECT)RTMemAlloc(sizeof (*pCompositor->paSrcRects) * cRects);
    if (pCompositor->paSrcRects)
    {
        pCompositor->paDstRects = (PRTRECT)RTMemAlloc(sizeof (*pCompositor->paDstRects) * cRects);
        if (pCompositor->paDstRects)
        {
            pCompositor->paDstUnstretchedRects = (PRTRECT)RTMemAlloc(sizeof (*pCompositor->paDstUnstretchedRects) * cRects);
            if (pCompositor->paDstUnstretchedRects)
            {
                pCompositor->cRects = cRects;
                pCompositor->cRectsBuffer = cRects;
                return VINF_SUCCESS;
            }

            RTMemFree(pCompositor->paDstRects);
            pCompositor->paDstRects = NULL;
        }
        else
        {
            WARN(("RTMemAlloc failed!"));
        }
        RTMemFree(pCompositor->paSrcRects);
        pCompositor->paSrcRects = NULL;
    }
    else
    {
        WARN(("RTMemAlloc failed!"));
    }

    pCompositor->cRects = VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED;
    pCompositor->cRectsBuffer = 0;

    return VERR_NO_MEMORY;
}

static void crVrScrCompositorRectsInvalidate(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    pCompositor->cRects = VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED;
}

static DECLCALLBACK(bool) crVrScrCompositorRectsCounterCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, void *pvVisitor)
{
    uint32_t* pCounter = (uint32_t*)pvVisitor;
    Assert(VBoxVrListRectsCount(&pEntry->Vr));
    *pCounter += VBoxVrListRectsCount(&pEntry->Vr);
    return true;
}

typedef struct VBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER
{
    PRTRECT paSrcRects;
    PRTRECT paDstRects;
    PRTRECT paDstUnstretchedRects;
    uint32_t cRects;
} VBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER, *PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER;

static DECLCALLBACK(bool) crVrScrCompositorRectsAssignerCb(PVBOXVR_COMPOSITOR pCCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER pData = (PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER)pvVisitor;
    PVBOXVR_SCR_COMPOSITOR pCompositor = VBOXVR_SCR_COMPOSITOR_FROM_COMPOSITOR(pCCompositor);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pCEntry);
    pEntry->paSrcRects = pData->paSrcRects;
    pEntry->paDstRects = pData->paDstRects;
    pEntry->paDstUnstretchedRects = pData->paDstUnstretchedRects;
    uint32_t cRects = VBoxVrListRectsCount(&pCEntry->Vr);
    Assert(cRects);
    Assert(cRects <= pData->cRects);
    int rc = VBoxVrListRectsGet(&pCEntry->Vr, cRects, pEntry->paDstRects);
    AssertRC(rc);

    memcpy(pEntry->paDstUnstretchedRects, pEntry->paDstRects, cRects * sizeof (*pEntry->paDstUnstretchedRects));

    if (!pEntry->Pos.x && !pEntry->Pos.y)
    {
        memcpy(pEntry->paSrcRects, pEntry->paDstRects, cRects * sizeof (*pEntry->paSrcRects));
    }
    else
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            pEntry->paSrcRects[i].xLeft = (int32_t)((pEntry->paDstRects[i].xLeft - pEntry->Pos.x));
            pEntry->paSrcRects[i].yTop = (int32_t)((pEntry->paDstRects[i].yTop - pEntry->Pos.y));
            pEntry->paSrcRects[i].xRight = (int32_t)((pEntry->paDstRects[i].xRight - pEntry->Pos.x));
            pEntry->paSrcRects[i].yBottom = (int32_t)((pEntry->paDstRects[i].yBottom - pEntry->Pos.y));
        }
    }

#ifndef IN_RING0
    if (pCompositor->StretchX != 1. || pCompositor->StretchY != 1.)
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            if (pCompositor->StretchX != 1.)
            {
                pEntry->paDstRects[i].xLeft = (int32_t)(pEntry->paDstRects[i].xLeft * pCompositor->StretchX);
                pEntry->paDstRects[i].xRight = (int32_t)(pEntry->paDstRects[i].xRight * pCompositor->StretchX);
            }
            if (pCompositor->StretchY != 1.)
            {
                pEntry->paDstRects[i].yTop = (int32_t)(pEntry->paDstRects[i].yTop * pCompositor->StretchY);
                pEntry->paDstRects[i].yBottom = (int32_t)(pEntry->paDstRects[i].yBottom * pCompositor->StretchY);
            }
        }
    }

    bool canZeroX = (pCompositor->StretchX < 1.);
    bool canZeroY = (pCompositor->StretchY < 1.);
    if (canZeroX && canZeroY)
    {
        /* filter out zero rectangles*/
        uint32_t iOrig, iNew;
        for (iOrig = 0, iNew = 0; iOrig < cRects; ++iOrig)
        {
            PRTRECT pOrigRect = &pEntry->paSrcRects[iOrig];
            if (pOrigRect->xLeft == pOrigRect->xRight
                    || pOrigRect->yTop == pOrigRect->yBottom)
                continue;

            if (iNew != iOrig)
            {
                PRTRECT pNewRect = &pEntry->paSrcRects[iNew];
                *pNewRect = *pOrigRect;
            }

            ++iNew;
        }

        Assert(iNew <= iOrig);

        uint32_t cDiff = iOrig - iNew;

        if (cDiff)
        {
            pCompositor->cRects -= cDiff;
            cRects -= cDiff;
        }
    }
#endif

    pEntry->cRects = cRects;
    pData->paDstRects += cRects;
    pData->paSrcRects += cRects;
    pData->paDstUnstretchedRects += cRects;
    pData->cRects -= cRects;
    return true;
}

static int crVrScrCompositorRectsCheckInit(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    if (pCompositor->cRects != VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED)
        return VINF_SUCCESS;

    uint32_t cRects = 0;
    VBoxVrCompositorVisit(&pCompositor->Compositor, crVrScrCompositorRectsCounterCb, &cRects);

    if (!cRects)
    {
        pCompositor->cRects = 0;
        return VINF_SUCCESS;
    }

    int rc = crVrScrCompositorRectsAssignBuffer(pCompositor, cRects);
    if (!RT_SUCCESS(rc))
        return rc;

    VBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER AssignerData;
    AssignerData.paSrcRects = pCompositor->paSrcRects;
    AssignerData.paDstRects = pCompositor->paDstRects;
    AssignerData.paDstUnstretchedRects = pCompositor->paDstUnstretchedRects;
    AssignerData.cRects = pCompositor->cRects;
    VBoxVrCompositorVisit(&pCompositor->Compositor, crVrScrCompositorRectsAssignerCb, &AssignerData);
    Assert(!AssignerData.cRects);
    return VINF_SUCCESS;
}


static int crVrScrCompositorEntryRegionsAdd(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, uint32_t *pfChangedFlags)
{
    uint32_t fChangedFlags = 0;
    PVBOXVR_COMPOSITOR_ENTRY pReplacedEntry;
    int rc = VBoxVrCompositorEntryRegionsAdd(&pCompositor->Compositor, pEntry ? &pEntry->Ce : NULL, cRegions, paRegions, &pReplacedEntry, &fChangedFlags);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxVrCompositorEntryRegionsAdd failed, rc %d", rc));
        return rc;
    }

    VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacedScrEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pReplacedEntry);

    if (fChangedFlags & VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED)
    {
        crVrScrCompositorRectsInvalidate(pCompositor);
    }
    else if (fChangedFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED)
    {
        Assert(!CrVrScrCompositorEntryIsInList(pReplacedScrEntry));
        Assert(CrVrScrCompositorEntryIsInList(pEntry));
        pEntry->cRects = pReplacedScrEntry->cRects;
        pEntry->paSrcRects = pReplacedScrEntry->paSrcRects;
        pEntry->paDstRects = pReplacedScrEntry->paDstRects;
        pEntry->paDstUnstretchedRects = pReplacedScrEntry->paDstUnstretchedRects;
    }

    if (fChangedFlags & VBOXVR_COMPOSITOR_CF_OTHER_ENTRIES_REGIONS_CHANGED)
    {
        CrVrScrCompositorEntrySetAllChanged(pCompositor, true);
    }
    else if ((fChangedFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED) && pEntry)
    {
        CrVrScrCompositorEntrySetChanged(pEntry, true);
    }

    if (pfChangedFlags)
        *pfChangedFlags = fChangedFlags;
    return VINF_SUCCESS;
}

static int crVrScrCompositorEntryRegionsSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    bool fChanged;
    CrVrScrCompositorEntryIsInList(pEntry);
    int rc = VBoxVrCompositorEntryRegionsSet(&pCompositor->Compositor, &pEntry->Ce, cRegions, paRegions, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxVrCompositorEntryRegionsSet failed, rc %d", rc));
        return rc;
    }

    if (fChanged)
    {
        CrVrScrCompositorEntrySetAllChanged(pCompositor, true);
        if (!CrVrScrCompositorEntryIsInList(pEntry))
        {
            pEntry->cRects = 0;
            pEntry->paSrcRects = NULL;
            pEntry->paDstRects = NULL;
            pEntry->paDstUnstretchedRects = NULL;
        }
        crVrScrCompositorRectsInvalidate(pCompositor);
    }


    if (pfChanged)
        *pfChanged = fChanged;
    return VINF_SUCCESS;
}

static int crVrScrCompositorEntryPositionSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, bool *pfChanged)
{
    if (pfChanged)
        *pfChanged = false;
    if (pEntry && (pEntry->Pos.x != pPos->x || pEntry->Pos.y != pPos->y))
    {
        if (VBoxVrCompositorEntryIsInList(&pEntry->Ce))
        {
            int rc = VBoxVrCompositorEntryRegionsTranslate(&pCompositor->Compositor, &pEntry->Ce, pPos->x - pEntry->Pos.x, pPos->y - pEntry->Pos.y, pfChanged);
            if (!RT_SUCCESS(rc))
            {
                WARN(("VBoxVrCompositorEntryRegionsTranslate failed rc %d", rc));
                return rc;
            }

            crVrScrCompositorRectsInvalidate(pCompositor);
        }

        pEntry->Pos = *pPos;
        CrVrScrCompositorEntrySetChanged(pEntry, true);

        if (pfChanged)
            *pfChanged = true;
    }
    return VINF_SUCCESS;
}

static int crVrScrCompositorEntryEnsureRegionsInTex(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, bool *pfChanged)
{
    RTRECT Rect;
    Rect.xLeft = pEntry->Pos.x;
    Rect.yTop = pEntry->Pos.y;
    Rect.xRight = pEntry->Pos.x + pEntry->Tex.width;
    Rect.yBottom = pEntry->Pos.y + pEntry->Tex.height;
    bool fChanged = false;

    if (pfChanged)
        *pfChanged = false;

    int rc = CrVrScrCompositorEntryRegionsIntersect(pCompositor, pEntry, 1, &Rect, &fChanged);
    if (!RT_SUCCESS(rc))
        WARN(("CrVrScrCompositorEntryRegionsIntersect failed, rc %d", rc));

    if (fChanged)
    {
        CrVrScrCompositorEntrySetChanged(pEntry, true);
        crVrScrCompositorRectsInvalidate(pCompositor);
    }

    if (pfChanged)
        *pfChanged = fChanged;
    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsAdd(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, bool fPosRelated, uint32_t *pfChangeFlags)
{
    int rc;
    uint32_t fChangeFlags = 0;
    bool fPosChanged = false;
    RTRECT *paTranslatedRects = NULL;
    if (pPos)
    {
        rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, &fPosChanged);
        if (!RT_SUCCESS(rc))
        {
            WARN(("RegionsAdd: crVrScrCompositorEntryPositionSet failed rc %d", rc));
            return rc;
        }
    }

    if (fPosRelated)
    {
        if (!pEntry)
        {
            WARN(("Entry is expected to be specified for pos-related regions"));
            return VERR_INVALID_PARAMETER;
        }

        if (cRegions && (pEntry->Pos.x || pEntry->Pos.y))
        {
            paTranslatedRects = (RTRECT*)RTMemAlloc(sizeof (RTRECT) * cRegions);
            if (!paTranslatedRects)
            {
                WARN(("RTMemAlloc failed"));
                return VERR_NO_MEMORY;
            }
            memcpy (paTranslatedRects, paRegions, sizeof (RTRECT) * cRegions);
            for (uint32_t i = 0; i < cRegions; ++i)
            {
                VBoxRectTranslate(&paTranslatedRects[i], pEntry->Pos.x, pEntry->Pos.y);
                paRegions = paTranslatedRects;
            }
        }
    }

    rc = crVrScrCompositorEntryRegionsAdd(pCompositor, pEntry, cRegions, paRegions, &fChangeFlags);
    if (!RT_SUCCESS(rc))
    {
        WARN(("crVrScrCompositorEntryRegionsAdd failed, rc %d", rc));
        goto done;
    }

    if ((fPosChanged || (fChangeFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED)) && pEntry)
    {
        bool fAdjusted = false;
        rc = crVrScrCompositorEntryEnsureRegionsInTex(pCompositor, pEntry, &fAdjusted);
        if (!RT_SUCCESS(rc))
        {
            WARN(("crVrScrCompositorEntryEnsureRegionsInTex failed, rc %d", rc));
            goto done;
        }

        if (fAdjusted)
        {
            fChangeFlags &= ~VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED;
            fChangeFlags |= VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED;
        }
    }

    if (fChangeFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED)
        fPosChanged = false;

    if (pfChangeFlags)
    {
        if (fPosChanged)
        {
            /* means entry was in list and was moved, so regions changed */
            *pfChangeFlags = VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_ENTRY_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_OTHER_ENTRIES_REGIONS_CHANGED;
        }
        else
            *pfChangeFlags = fChangeFlags;
    }

done:

    if (paTranslatedRects)
        RTMemFree(paTranslatedRects);

    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryTexUpdate(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const VBOXVR_TEXTURE *pTex)
{
    bool fCompositorChanged = CrVrScrCompositorEntryIsUsed(pEntry) && (pEntry->Tex.width != pTex->width || pEntry->Tex.height != pTex->height);
    pEntry->Tex = *pTex;
    CrVrScrCompositorEntrySetChanged(pEntry, true);
    if (fCompositorChanged)
    {
        int rc = crVrScrCompositorEntryEnsureRegionsInTex(pCompositor, pEntry, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("crVrScrCompositorEntryEnsureRegionsInTex failed rc %d", rc));
            return rc;
        }
    }
    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, bool fPosRelated, bool *pfChanged)
{
    /* @todo: the fChanged sate calculation is really rough now, this is enough for now though */
    bool fChanged = false, fPosChanged = false;
    bool fWasInList = CrVrScrCompositorEntryIsInList(pEntry);
    RTRECT *paTranslatedRects = NULL;
    int rc = CrVrScrCompositorEntryRemove(pCompositor, pEntry);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RegionsSet: CrVrScrCompositorEntryRemove failed rc %d", rc));
        return rc;
    }

    if (pPos)
    {
        rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, &fPosChanged);
        if (!RT_SUCCESS(rc))
        {
            WARN(("RegionsSet: crVrScrCompositorEntryPositionSet failed rc %d", rc));
            return rc;
        }
    }

    if (fPosRelated)
    {
        if (!pEntry)
        {
            WARN(("Entry is expected to be specified for pos-related regions"));
            return VERR_INVALID_PARAMETER;
        }

        if (cRegions && (pEntry->Pos.x || pEntry->Pos.y))
        {
            paTranslatedRects = (RTRECT*)RTMemAlloc(sizeof (RTRECT) * cRegions);
            if (!paTranslatedRects)
            {
                WARN(("RTMemAlloc failed"));
                return VERR_NO_MEMORY;
            }
            memcpy (paTranslatedRects, paRegions, sizeof (RTRECT) * cRegions);
            for (uint32_t i = 0; i < cRegions; ++i)
            {
                VBoxRectTranslate(&paTranslatedRects[i], pEntry->Pos.x, pEntry->Pos.y);
                paRegions = paTranslatedRects;
            }
        }
    }

    rc = crVrScrCompositorEntryRegionsSet(pCompositor, pEntry, cRegions, paRegions, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("crVrScrCompositorEntryRegionsSet failed, rc %d", rc));
        return rc;
    }

    if (fChanged && CrVrScrCompositorEntryIsUsed(pEntry))
    {
        rc = crVrScrCompositorEntryEnsureRegionsInTex(pCompositor, pEntry, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("crVrScrCompositorEntryEnsureRegionsInTex failed, rc %d", rc));
            return rc;
        }
    }

    if (pfChanged)
        *pfChanged = fPosChanged || fChanged || fWasInList;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryListIntersect(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    bool fChanged = false;
    int rc = VBoxVrCompositorEntryListIntersect(&pCompositor->Compositor, &pEntry->Ce, pList2, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RegionsIntersect: VBoxVrCompositorEntryRegionsIntersect failed rc %d", rc));
        return rc;
    }

    if (fChanged)
    {
        CrVrScrCompositorEntrySetChanged(pEntry, true);
        crVrScrCompositorRectsInvalidate(pCompositor);
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsIntersect(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    bool fChanged = false;
    int rc = VBoxVrCompositorEntryRegionsIntersect(&pCompositor->Compositor, &pEntry->Ce, cRegions, paRegions, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RegionsIntersect: VBoxVrCompositorEntryRegionsIntersect failed rc %d", rc));
        return rc;
    }

    if (fChanged)
    {
        CrVrScrCompositorEntrySetChanged(pEntry, true);
        crVrScrCompositorRectsInvalidate(pCompositor);
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryListIntersectAll(PVBOXVR_SCR_COMPOSITOR pCompositor, const VBOXVR_LIST *pList2, bool *pfChanged)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR Iter;
    CrVrScrCompositorIterInit(pCompositor, &Iter);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    while ((pEntry = CrVrScrCompositorIterNext(&Iter)) != NULL)
    {
        bool fTmpChanged = false;
        int tmpRc = CrVrScrCompositorEntryListIntersect(pCompositor, pEntry, pList2, &fTmpChanged);
        if (RT_SUCCESS(tmpRc))
        {
            fChanged |= fTmpChanged;
        }
        else
        {
            WARN(("CrVrScrCompositorEntryRegionsIntersect failed, rc %d", tmpRc));
            rc = tmpRc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsIntersectAll(PVBOXVR_SCR_COMPOSITOR pCompositor, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR Iter;
    CrVrScrCompositorIterInit(pCompositor, &Iter);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    int rc = VINF_SUCCESS;
    bool fChanged = false;

    while ((pEntry = CrVrScrCompositorIterNext(&Iter)) != NULL)
    {
        bool fTmpChanged = false;
        int tmpRc = CrVrScrCompositorEntryRegionsIntersect(pCompositor, pEntry, cRegions, paRegions, &fTmpChanged);
        if (RT_SUCCESS(tmpRc))
        {
            fChanged |= fTmpChanged;
        }
        else
        {
            WARN(("CrVrScrCompositorEntryRegionsIntersect failed, rc %d", tmpRc));
            rc = tmpRc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryPosSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos)
{
    int rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, NULL);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RegionsSet: crVrScrCompositorEntryPositionSet failed rc %d", rc));
        return rc;
    }
    return VINF_SUCCESS;
}

/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions, const RTRECT **ppaDstUnstretchedRects)
{
    int rc = crVrScrCompositorRectsCheckInit(pCompositor);
    if (!RT_SUCCESS(rc))
    {
        WARN(("crVrScrCompositorRectsCheckInit failed, rc %d", rc));
        return rc;
    }

    Assert(pCompositor->cRects != VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED);

    *pcRegions = pEntry->cRects;
    if (ppaSrcRegions)
        *ppaSrcRegions = pEntry->paSrcRects;
    if (ppaDstRegions)
        *ppaDstRegions = pEntry->paDstRects;
    if (ppaDstUnstretchedRects)
        *ppaDstUnstretchedRects = pEntry->paDstUnstretchedRects;

    return VINF_SUCCESS;
}

VBOXVREGDECL(uint32_t) CrVrScrCompositorEntryFlagsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    return CRBLT_FOP_COMBINE(pCompositor->fFlags, pEntry->fFlags);
}

VBOXVREGDECL(void) CrVrScrCompositorEntryFlagsSet(PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t fFlags)
{
    if (pEntry->fFlags == fFlags)
        return;

    pEntry->fFlags = fFlags;
    CrVrScrCompositorEntrySetChanged(pEntry, true);
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRemove(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    if (!VBoxVrCompositorEntryRemove(&pCompositor->Compositor, &pEntry->Ce))
        return VINF_SUCCESS;

    CrVrScrCompositorEntrySetChanged(pEntry, true);
    pEntry->cRects = 0;
    pEntry->paSrcRects = NULL;
    pEntry->paDstRects = NULL;
    pEntry->paDstUnstretchedRects = NULL;

    crVrScrCompositorRectsInvalidate(pCompositor);
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) crVrScrCompositorEntryReleasedCB(const struct VBOXVR_COMPOSITOR *pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, PVBOXVR_COMPOSITOR_ENTRY pReplacingEntry)
{
    PVBOXVR_SCR_COMPOSITOR_ENTRY pCEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pEntry);
    CrVrScrCompositorEntrySetChanged(pCEntry, true);

    if (pCEntry->pfnEntryReleased)
    {
        PVBOXVR_SCR_COMPOSITOR_ENTRY pCReplacingEntry = pReplacingEntry ? VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pReplacingEntry) : NULL;
        PVBOXVR_SCR_COMPOSITOR pCConpositor = VBOXVR_SCR_COMPOSITOR_FROM_COMPOSITOR(pCompositor);
        pCEntry->pfnEntryReleased(pCConpositor, pCEntry, pCReplacingEntry);
    }
}

VBOXVREGDECL(void) CrVrScrCompositorInit(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    memset(pCompositor, 0, sizeof (*pCompositor));
    VBoxVrCompositorInit(&pCompositor->Compositor, crVrScrCompositorEntryReleasedCB);
    pCompositor->fFlags = CRBLT_F_LINEAR | CRBLT_F_INVERT_YCOORDS;
#ifndef IN_RING0
    pCompositor->StretchX = 1.0;
    pCompositor->StretchY = 1.0;
#endif
}

VBOXVREGDECL(void) CrVrScrCompositorRegionsClear(PVBOXVR_SCR_COMPOSITOR pCompositor, bool *pfChanged)
{
    /* set changed flag first, while entries are in the list and we have them */
    CrVrScrCompositorEntrySetAllChanged(pCompositor, true);
    VBoxVrCompositorRegionsClear(&pCompositor->Compositor, pfChanged);
    crVrScrCompositorRectsInvalidate(pCompositor);
}

VBOXVREGDECL(void) CrVrScrCompositorClear(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    CrVrScrCompositorRegionsClear(pCompositor, NULL);
    if (pCompositor->paDstRects)
    {
        RTMemFree(pCompositor->paDstRects);
        pCompositor->paDstRects = NULL;
    }
    if (pCompositor->paSrcRects)
    {
        RTMemFree(pCompositor->paSrcRects);
        pCompositor->paSrcRects = NULL;
    }
    if (pCompositor->paDstUnstretchedRects)
    {
        RTMemFree(pCompositor->paDstUnstretchedRects);
        pCompositor->paDstUnstretchedRects = NULL;
    }

    pCompositor->cRects = 0;
    pCompositor->cRectsBuffer = 0;
}

VBOXVREGDECL(void) CrVrScrCompositorEntrySetAllChanged(PVBOXVR_SCR_COMPOSITOR pCompositor, bool fChanged)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pCurEntry;
    CrVrScrCompositorIterInit(pCompositor, &CIter);

    while ((pCurEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        CrVrScrCompositorEntrySetChanged(pCurEntry, fChanged);
    }
}

#ifndef IN_RING0
VBOXVREGDECL(void) CrVrScrCompositorSetStretching(PVBOXVR_SCR_COMPOSITOR pCompositor, float StretchX, float StretchY)
{
    if (pCompositor->StretchX == StretchX && pCompositor->StretchY == StretchY)
        return;

    pCompositor->StretchX = StretchX;
    pCompositor->StretchY = StretchY;
    crVrScrCompositorRectsInvalidate(pCompositor);
    CrVrScrCompositorEntrySetAllChanged(pCompositor, true);
}
#endif

/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions, const RTRECT **ppaDstUnstretchedRects)
{
    int rc = crVrScrCompositorRectsCheckInit(pCompositor);
    if (!RT_SUCCESS(rc))
    {
        WARN(("crVrScrCompositorRectsCheckInit failed, rc %d", rc));
        return rc;
    }

    Assert(pCompositor->cRects != VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED);

    *pcRegions = pCompositor->cRects;
    if (ppaSrcRegions)
        *ppaSrcRegions = pCompositor->paSrcRects;
    if (ppaDstRegions)
        *ppaDstRegions = pCompositor->paDstRects;
    if (ppaDstUnstretchedRects)
        *ppaDstUnstretchedRects = pCompositor->paDstUnstretchedRects;

    return VINF_SUCCESS;
}

typedef struct VBOXVR_SCR_COMPOSITOR_VISITOR_CB
{
    PFNVBOXVRSCRCOMPOSITOR_VISITOR pfnVisitor;
    void *pvVisitor;
} VBOXVR_SCR_COMPOSITOR_VISITOR_CB, *PVBOXVR_SCR_COMPOSITOR_VISITOR_CB;

static DECLCALLBACK(bool) crVrScrCompositorVisitCb(PVBOXVR_COMPOSITOR pCCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    PVBOXVR_SCR_COMPOSITOR_VISITOR_CB pData = (PVBOXVR_SCR_COMPOSITOR_VISITOR_CB)pvVisitor;
    PVBOXVR_SCR_COMPOSITOR pCompositor = VBOXVR_SCR_COMPOSITOR_FROM_COMPOSITOR(pCCompositor);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pCEntry);
    return pData->pfnVisitor(pCompositor, pEntry, pData->pvVisitor);
}

VBOXVREGDECL(void) CrVrScrCompositorVisit(PVBOXVR_SCR_COMPOSITOR pCompositor, PFNVBOXVRSCRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor)
{
    VBOXVR_SCR_COMPOSITOR_VISITOR_CB Data;
    Data.pfnVisitor = pfnVisitor;
    Data.pvVisitor = pvVisitor;
    VBoxVrCompositorVisit(&pCompositor->Compositor, crVrScrCompositorVisitCb, &Data);
}

VBOXVREGDECL(int) CrVrScrCompositorClone(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR pDstCompositor, PFNVBOXVR_SCR_COMPOSITOR_ENTRY_FOR pfnEntryFor, void* pvEntryFor)
{
    /* for simplicity just copy from one to another */
    CrVrScrCompositorInit(pDstCompositor);
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    CrVrScrCompositorIterInit(pCompositor, &CIter);
    int rc = VINF_SUCCESS;
    uint32_t cRects;
    const RTRECT *pRects;

    while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        /* get source rects, that will be non-stretched and entry pos - pased */
        rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRects, NULL, NULL, &pRects);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorEntryRegionsGet failed, rc %d", rc));
            return rc;
        }

        PVBOXVR_SCR_COMPOSITOR_ENTRY pDstEntry = pfnEntryFor(pEntry, pvEntryFor);
        if (!pDstEntry)
        {
            WARN(("pfnEntryFor failed"));
            return VERR_INVALID_STATE;
        }

        rc = CrVrScrCompositorEntryRegionsSet(pDstCompositor, pDstEntry, CrVrScrCompositorEntryPosGet(pEntry), cRects, pRects, false, NULL);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc);
            return rc;
        }
    }

    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorIntersectList(PVBOXVR_SCR_COMPOSITOR pCompositor, const VBOXVR_LIST *pVr, bool *pfChanged)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    CrVrScrCompositorIterInit(pCompositor, &CIter);
    int rc = VINF_SUCCESS;
    uint32_t cRects;
    const RTRECT *pRects;
    bool fChanged = false;

    while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        bool fCurChanged = false;

        /* get source rects, that will be non-stretched and entry pos - pased */
        rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRects, NULL, NULL, &pRects);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorEntryRegionsGet failed, rc %d", rc));
            break;
        }

        rc = CrVrScrCompositorEntryListIntersect(pCompositor, pEntry, pVr, &fCurChanged);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc);
            break;
        }

        fChanged |= fCurChanged;
    }

    if (pfChanged)
        *pfChanged = fChanged;

    return rc;
}

VBOXVREGDECL(int) CrVrScrCompositorIntersectedList(PVBOXVR_SCR_COMPOSITOR pCompositor, const VBOXVR_LIST *pVr, PVBOXVR_SCR_COMPOSITOR pDstCompositor, PFNVBOXVR_SCR_COMPOSITOR_ENTRY_FOR pfnEntryFor, void* pvEntryFor, bool *pfChanged)
{
    int rc  = CrVrScrCompositorClone(pCompositor, pDstCompositor, pfnEntryFor, pvEntryFor);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrVrScrCompositorClone failed, rc %d", rc));
        return rc;
    }

    rc = CrVrScrCompositorIntersectList(pDstCompositor, pVr, pfChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrVrScrCompositorIntersectList failed, rc %d", rc));
        CrVrScrCompositorClear(pDstCompositor);
        return rc;
    }

    return VINF_SUCCESS;
}
