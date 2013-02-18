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
#include <iprt/memcache.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#ifndef IN_RING0
#include <cr_error.h>
#define WARN(_m) do { crWarning _m ; } while (0)
#else
# error port me!
#endif

#ifdef DEBUG_misha
//# define VBOXVDBG_VR_LAL_DISABLE
#endif

#ifndef VBOXVDBG_VR_LAL_DISABLE
static volatile int32_t g_cVBoxVrInits = 0;
static RTMEMCACHE g_VBoxVrLookasideList;
#endif

static PVBOXVR_REG vboxVrRegCreate()
{
#ifndef VBOXVDBG_VR_LAL_DISABLE
    PVBOXVR_REG pReg = (PVBOXVR_REG)RTMemCacheAlloc(g_VBoxVrLookasideList);
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
    RTMemCacheFree(g_VBoxVrLookasideList, pReg);
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
    int rc = RTMemCacheCreate(&g_VBoxVrLookasideList, sizeof (VBOXVR_REG),
                            0, /* size_t cbAlignment */
                            UINT32_MAX, /* uint32_t cMaxObjects */
                            NULL, /* PFNMEMCACHECTOR pfnCtor*/
                            NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                            NULL, /* void *pvUser*/
                            0 /* uint32_t fFlags*/
                            );
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
    RTMemCacheDestroy(g_VBoxVrLookasideList);
#endif
}

typedef DECLCALLBACK(int) FNVBOXVR_CB_COMPARATOR(const PVBOXVR_REG pReg1, const PVBOXVR_REG pReg2);
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
            if (pReg1->Rect.yBottom <= pRect2->yTop)
                continue;
            else if (pRect2->yBottom <= pReg1->Rect.yTop)
                continue;
            /* y coords intersect */
            else if (pReg1->Rect.xRight <= pRect2->xLeft)
                continue;
            else if (pRect2->xRight <= pReg1->Rect.xLeft)
                continue;
            /* x coords intersect */

            /* the visitor can modify the list 1, apply necessary adjustments after it */
            PRTLISTNODE pEntry1 = pfnVisitor (pList1, pReg1, pRect2, pvVisitor, &pNext1);
            if (pEntry1 == &pList1->ListHead)
                break;
        }
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

                        vboxVrListRegAddOrder(pList, pReg2->ListEntry.pNext, pReg2);

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

                        vboxVrListRegAddOrder(pList, pReg2->ListEntry.pNext, pReg2);

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
    if (VBoxVrListIsEmpty(pList))
        return VINF_SUCCESS;

    VBOXVR_CBDATA_SUBST Data;
    Data.rc = VINF_SUCCESS;
    Data.fChanged = false;

    *pfChanged = false;

    vboxVrListVisitIntersected(pList, cRects, aRects, vboxVrListSubstNoJoinCb, &Data);
    if (!RT_SUCCESS(Data.rc))
    {
        WARN(("vboxVrListVisitIntersected failed!"));
        return Data.rc;
    }

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

    int rc = vboxVrListSubstNoJoin(pList, cRects, aRects, pfChanged);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vboxVrListSubstNoJoin failed!"));
        goto done;
    }

    if (!*pfChanged)
        goto done;

    vboxVrListJoinRects(pList);

done:
#if 0
    if (pRects != aRects)
        RTMemFree(pRects);
#endif
    return rc;
}

VBOXVREGDECL(int) VBoxVrListRectsAdd(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged)
{
    uint32_t cCovered = 0;

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
    {
        *pfChanged = false;
        return VINF_SUCCESS;
    }

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

VBOXVREGDECL(int) VBoxVrListCmp(PVBOXVR_LIST pList1, PVBOXVR_LIST pList2)
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

VBOXVREGDECL(void) VBoxVrCompositorInit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_ENTRY_REMOVED pfnEntryRemoved)
{
    RTListInit(&pCompositor->List);
    pCompositor->pfnEntryRemoved = pfnEntryRemoved;
}

VBOXVREGDECL(void) VBoxVrCompositorTerm(PVBOXVR_COMPOSITOR pCompositor)
{
    PVBOXVR_COMPOSITOR_ENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pCompositor->List, pEntry, pEntryNext, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        VBoxVrCompositorEntryRemove(pCompositor, pEntry);
    }
}

DECLINLINE(void) vboxVrCompositorEntryAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    RTListPrepend(&pCompositor->List, &pEntry->Node);
}

DECLINLINE(void) vboxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, PVBOXVR_COMPOSITOR_ENTRY pReplacingEntry)
{
    RTListNodeRemove(&pEntry->Node);
    if (pCompositor->pfnEntryRemoved)
        pCompositor->pfnEntryRemoved(pCompositor, pEntry, pReplacingEntry);
}

VBOXVREGDECL(void) VBoxVrCompositorEntryInit(PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    VBoxVrListInit(&pEntry->Vr);
}

VBOXVREGDECL(bool) VBoxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    if (!VBoxVrCompositorEntryIsInList(pEntry))
        return false;
    VBoxVrListClear(&pEntry->Vr);
    vboxVrCompositorEntryRemove(pCompositor, pEntry, NULL);
    return true;
}

static int vboxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT * paRects, bool *pfChanged)
{
    bool fChanged;
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
        return VINF_SUCCESS;
    }

    WARN(("VBoxVrListRectsSubst failed, rc %d", rc));
    return rc;
}

VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, uint32_t *pfChangeFlags)
{
    bool fOthersChanged = false, fCurChanged = false, fEntryChanged = false, fEntryInList = false, fEntryReplaces = false;
    PVBOXVR_COMPOSITOR_ENTRY pCur;
    int rc = VINF_SUCCESS;

    if (!cRects)
    {
        if (pfChangeFlags)
            *pfChangeFlags = 0;
        return VINF_SUCCESS;
    }

    if (pEntry)
    {
        fEntryInList = VBoxVrCompositorEntryIsInList(pEntry);
        rc = VBoxVrListRectsAdd(&pEntry->Vr, cRects, paRects, &fEntryChanged);
        if (RT_SUCCESS(rc))
        {
            if (VBoxVrListIsEmpty(&pEntry->Vr))
            {
                WARN(("Empty rectangles passed in, is it expected?"));
                if (pfChangeFlags)
                    *pfChangeFlags = 0;
                return VINF_SUCCESS;
            }
        }
        else
        {
            WARN(("VBoxVrListRectsAdd failed, rc %d", rc));
            return rc;
        }

        Assert(!VBoxVrListIsEmpty(&pEntry->Vr));
    }

    RTListForEach(&pCompositor->List, pCur, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        Assert(!VBoxVrListIsEmpty(&pCur->Vr));
        if (pCur == pEntry)
        {
            Assert(fEntryInList);
        }
        else
        {
            if (pEntry && !VBoxVrListCmp(&pCur->Vr, &pEntry->Vr))
            {
                VBoxVrListClear(&pCur->Vr);
                vboxVrCompositorEntryRemove(pCompositor, pCur, pEntry);
                fEntryReplaces = true;
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

    if (pEntry && !fEntryInList)
    {
        Assert(!VBoxVrListIsEmpty(&pEntry->Vr));
        vboxVrCompositorEntryAdd(pCompositor, pEntry);
    }

    if (pfChangeFlags)
    {
        uint32_t fFlags = 0;
        if (fOthersChanged)
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRIES_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_COMPOSITED_REGIONS_CHANGED;
        else if (fEntryReplaces)
        {
            Assert(fEntryChanged);
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRIES_REGIONS_CHANGED;
        }
        else if (fEntryChanged)
            fFlags = VBOXVR_COMPOSITOR_CF_ENTRIES_REGIONS_CHANGED | VBOXVR_COMPOSITOR_CF_COMPOSITED_REGIONS_CHANGED;
        if (!fEntryInList)
            fFlags |= VBOXVR_COMPOSITOR_CF_ENTRY_ADDED;

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

    if (VBoxVrListIsEmpty(&pEntry->Vr))
    {
        if (pfChanged)
            *pfChanged = false;
        return VINF_SUCCESS;

    }

    int rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pEntry, cRects, paRects, pfChanged);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("pfChanged failed, rc %d", rc));
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

    bool fChanged = false, fCurChanged = false;
    uint32_t fChangeFlags = 0;
    int rc;
    fCurChanged = VBoxVrCompositorEntryRemove(pCompositor, pEntry);
    fChanged |= fCurChanged;

    rc = VBoxVrCompositorEntryRegionsAdd(pCompositor, pEntry, cRects, paRects, &fChangeFlags);
    if (RT_SUCCESS(rc))
        fChanged |= !!fChangeFlags;
    else
    {
        WARN(("VBoxVrCompositorEntryRegionsAdd failed, rc %d", rc));
        return rc;
    }

    AssertRC(rc);

    if (pfChanged)
        *pfChanged = fChanged;
    return VINF_SUCCESS;
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

    if ((!x && !y)
            || !VBoxVrCompositorEntryIsInList(pEntry))
    {
        if (pfChanged)
            *pfChanged = false;
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
        Assert(pCompositor->paDstRects);
        RTMemFree(pCompositor->paDstRects);
    }

    pCompositor->paSrcRects = (PRTRECT)RTMemAlloc(sizeof (*pCompositor->paSrcRects) * cRects);
    if (pCompositor->paSrcRects)
    {
        pCompositor->paDstRects = (PRTRECT)RTMemAlloc(sizeof (*pCompositor->paDstRects) * cRects);
        if (pCompositor->paDstRects)
        {
            pCompositor->cRects = cRects;
            pCompositor->cRectsBuffer = cRects;
            return VINF_SUCCESS;
        }
        else
        {
            crWarning("RTMemAlloc failed!");
            RTMemFree(pCompositor->paSrcRects);
            pCompositor->paSrcRects = NULL;
        }
    }
    else
    {
        crWarning("RTMemAlloc failed!");
        pCompositor->paDstRects = NULL;
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
    uint32_t cRects;
} VBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER, *PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER;

static DECLCALLBACK(bool) crVrScrCompositorRectsAssignerCb(PVBOXVR_COMPOSITOR pCCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER pData = (PVBOXVR_SCR_COMPOSITOR_RECTS_ASSIGNER)pvVisitor;
    PVBOXVR_SCR_COMPOSITOR pCompositor = VBOXVR_SCR_COMPOSITOR_FROM_COMPOSITOR(pCCompositor);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pCEntry);
    pEntry->paSrcRects = pData->paSrcRects;
    pEntry->paDstRects = pData->paDstRects;
    uint32_t cRects = VBoxVrListRectsCount(&pCEntry->Vr);
    Assert(cRects);
    Assert(cRects >= pData->cRects);
    int rc = VBoxVrListRectsGet(&pCEntry->Vr, cRects, pEntry->paDstRects);
    AssertRC(rc);
    if (pCompositor->StretchX >= 1. && pCompositor->StretchY >= 1. /* <- stretching can not zero some rects */
            && !pEntry->Pos.x && !pEntry->Pos.y)
    {
        memcpy(pEntry->paSrcRects, pEntry->paDstRects, cRects * sizeof (*pEntry->paSrcRects));
    }
    else
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            pEntry->paSrcRects[i].xLeft = (int32_t)((pEntry->paDstRects[i].xLeft - pEntry->Pos.x) * pCompositor->StretchX);
            pEntry->paSrcRects[i].yTop = (int32_t)((pEntry->paDstRects[i].yTop - pEntry->Pos.y) * pCompositor->StretchY);
            pEntry->paSrcRects[i].xRight = (int32_t)((pEntry->paDstRects[i].xRight - pEntry->Pos.x) * pCompositor->StretchX);
            pEntry->paSrcRects[i].yBottom = (int32_t)((pEntry->paDstRects[i].yBottom - pEntry->Pos.y) * pCompositor->StretchY);
        }

        bool canZeroX = (pCompositor->StretchX < 1);
        bool canZeroY = (pCompositor->StretchY < 1);
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
    }

    pEntry->cRects = cRects;
    pData->paDstRects += cRects;
    pData->paSrcRects += cRects;
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
    AssignerData.cRects = pCompositor->cRects;
    VBoxVrCompositorVisit(&pCompositor->Compositor, crVrScrCompositorRectsAssignerCb, &AssignerData);
    Assert(!AssignerData.cRects);
    return VINF_SUCCESS;
}


static int crVrScrCompositorEntryRegionsAdd(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    uint32_t fChangedFlags = 0;
    int rc = VBoxVrCompositorEntryRegionsAdd(&pCompositor->Compositor, &pEntry->Ce, cRegions, paRegions, &fChangedFlags);
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrCompositorEntryRegionsAdd failed, rc %d", rc);
        return rc;
    }

    if (fChangedFlags & VBOXVR_COMPOSITOR_CF_COMPOSITED_REGIONS_CHANGED)
    {
        crVrScrCompositorRectsInvalidate(pCompositor);
    }

    CrVrScrCompositorEntrySetChanged(pEntry, true);

    if (pfChanged)
        *pfChanged = !!fChangedFlags;
    return VINF_SUCCESS;
}

static int crVrScrCompositorEntryRegionsSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    bool fChanged;
    int rc = VBoxVrCompositorEntryRegionsSet(&pCompositor->Compositor, &pEntry->Ce, cRegions, paRegions, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrCompositorEntryRegionsSet failed, rc %d", rc);
        return rc;
    }

    if (fChanged)
    {
        crVrScrCompositorRectsInvalidate(pCompositor);
    }

    CrVrScrCompositorEntrySetChanged(pEntry, true);

    if (pfChanged)
        *pfChanged = fChanged;
    return VINF_SUCCESS;
}

static int crVrScrCompositorEntryPositionSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, bool *pfChanged)
{
    if (pEntry && (pEntry->Pos.x != pPos->x || pEntry->Pos.y != pPos->y))
    {
        int rc = VBoxVrCompositorEntryRegionsTranslate(&pCompositor->Compositor, &pEntry->Ce, pPos->x - pEntry->Pos.x, pPos->y - pEntry->Pos.y, pfChanged);
        if (!RT_SUCCESS(rc))
        {
            crWarning("VBoxVrCompositorEntryRegionsTranslate failed rc %d", rc);
            return rc;
        }

        if (VBoxVrCompositorEntryIsInList(&pEntry->Ce))
        {
            crVrScrCompositorRectsInvalidate(pCompositor);
        }

        pEntry->Pos = *pPos;
        CrVrScrCompositorEntrySetChanged(pEntry, true);
    }
    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsAdd(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    int rc;
    if (pPos)
    {
        rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, NULL);
        if (!RT_SUCCESS(rc))
        {
            crWarning("RegionsAdd: crVrScrCompositorEntryPositionSet failed rc %d", rc);
            return rc;
        }
    }

    rc = crVrScrCompositorEntryRegionsAdd(pCompositor, pEntry, cRegions, paRegions, NULL);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crVrScrCompositorEntryRegionsAdd failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    int rc = CrVrScrCompositorEntryRemove(pCompositor, pEntry);
    if (!RT_SUCCESS(rc))
    {
        crWarning("RegionsSet: CrVrScrCompositorEntryRemove failed rc %d", rc);
        return rc;
    }

    if (pPos)
    {
        rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, NULL);
        if (!RT_SUCCESS(rc))
        {
            crWarning("RegionsSet: crVrScrCompositorEntryPositionSet failed rc %d", rc);
            return rc;
        }
    }

    rc = crVrScrCompositorEntryRegionsSet(pCompositor, pEntry, cRegions, paRegions, NULL);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crVrScrCompositorEntryRegionsSet failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryPosSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos)
{
    int rc = crVrScrCompositorEntryPositionSet(pCompositor, pEntry, pPos, NULL);
    if (!RT_SUCCESS(rc))
    {
        crWarning("RegionsSet: crVrScrCompositorEntryPositionSet failed rc %d", rc);
        return rc;
    }
    return VINF_SUCCESS;
}

/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions)
{
    int rc = crVrScrCompositorRectsCheckInit(pCompositor);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crVrScrCompositorRectsCheckInit failed, rc %d", rc);
        return rc;
    }

    Assert(pCompositor->cRects != VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED);

    *pcRegions = pEntry->cRects;
    if (ppaSrcRegions)
        *ppaSrcRegions = pEntry->paSrcRects;
    if (ppaDstRegions)
        *ppaDstRegions = pEntry->paDstRects;

    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorEntryRemove(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    if (!VBoxVrCompositorEntryRemove(&pCompositor->Compositor, &pEntry->Ce))
        return VINF_SUCCESS;

    crVrScrCompositorRectsInvalidate(pCompositor);
    return VINF_SUCCESS;
}

VBOXVREGDECL(int) CrVrScrCompositorInit(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    memset(pCompositor, 0, sizeof (*pCompositor));
    VBoxVrCompositorInit(&pCompositor->Compositor, NULL);
    pCompositor->StretchX = 1.0;
    pCompositor->StretchY = 1.0;
    return VINF_SUCCESS;
}

VBOXVREGDECL(void) CrVrScrCompositorTerm(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    VBoxVrCompositorTerm(&pCompositor->Compositor);
    if (pCompositor->paDstRects)
        RTMemFree(pCompositor->paDstRects);
    if (pCompositor->paSrcRects)
        RTMemFree(pCompositor->paSrcRects);
}


static DECLCALLBACK(bool) crVrScrCompositorEntrySetAllChangedCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry = VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pCEntry);
    CrVrScrCompositorEntrySetChanged(pEntry, !!pvVisitor);
    return true;
}

VBOXVREGDECL(void) CrVrScrCompositorEntrySetAllChanged(PVBOXVR_SCR_COMPOSITOR pCompositor, bool fChanged)
{
    VBoxVrCompositorVisit(&pCompositor->Compositor, crVrScrCompositorEntrySetAllChangedCb, (void*)fChanged);
}

VBOXVREGDECL(void) CrVrScrCompositorSetStretching(PVBOXVR_SCR_COMPOSITOR pCompositor, float StretchX, float StretchY)
{
    pCompositor->StretchX = StretchX;
    pCompositor->StretchY = StretchY;
    crVrScrCompositorRectsInvalidate(pCompositor);
    CrVrScrCompositorEntrySetAllChanged(pCompositor, true);
}

/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions)
{
    int rc = crVrScrCompositorRectsCheckInit(pCompositor);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crVrScrCompositorRectsCheckInit failed, rc %d", rc);
        return rc;
    }

    Assert(pCompositor->cRects != VBOXVR_SCR_COMPOSITOR_RECTS_UNDEFINED);

    *pcRegions = pCompositor->cRects;
    if (ppaSrcRegions)
        *ppaSrcRegions = pCompositor->paSrcRects;
    if (ppaDstRegions)
        *ppaDstRegions = pCompositor->paDstRects;

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
