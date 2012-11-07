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

typedef struct VBOXVR_REG
{
    RTLISTNODE ListEntry;
    RTRECT Rect;
} VBOXVR_REG, *PVBOXVR_REG;

#define PVBOXVR_REG_FROM_ENTRY(_pEntry) ((PVBOXVR_REG)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(VBOXVR_REG, ListEntry)))

#ifdef DEBUG_misha
//# define VBOXVDBG_VR_LAL_DISABLE
#endif

#ifndef VBOXVDBG_VR_LAL_DISABLE
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

void VBoxVrListClear(PVBOXVR_LIST pList)
{
    PVBOXVR_REG pReg, pRegNext;

    RTListForEachSafe(&pList->ListHead, pReg, pRegNext, VBOXVR_REG, ListEntry)
    {
        vboxVrRegTerm(pReg);
    }
    VBoxVrListInit(pList);
}

#define VBOXVR_MEMTAG 'vDBV'

int VBoxVrInit()
{
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

void VBoxVrTerm()
{
#ifndef VBOXVDBG_VR_LAL_DISABLE
    RTMemCacheDestroy(g_VBoxVrLookasideList);
#endif
}

typedef DECLCALLBACK(int) FNVBOXVR_CB_COMPARATOR(const PVBOXVR_REG pReg1, const PVBOXVR_REG pReg2);
typedef FNVBOXVR_CB_COMPARATOR *PFNVBOXVR_CB_COMPARATOR;

static DECLCALLBACK(int) vboxVrRegNonintersectedComparator(const RTRECT* pRect1, const RTRECT* pRect2)
{
    Assert(!VBoxRectRectIsIntersect(pRect1, pRect2));
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

#define VBOXVR_INVALID_COORD (~0UL)

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

static int vboxVrListRegIntersectSubstNoJoin(PRTLISTNODE pList1, PVBOXVR_REG pReg1, const RTRECT * pRect2)
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

    Assert(VBoxRectRectIsIntersect(&pReg1->Rect, pRect2));

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

    if (IsListEmpty(&List))
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

    Assert(VBoxRectRectIsIntersect(&pReg1->Rect, pRect2));

    /* NOTE: the pReg1 will be invalid after the vboxVrListRegIntersectSubstNoJoin call!!! */
    int rc = vboxVrListRegIntersectSubstNoJoin(pList, pReg1, pRect2);
    if (RC_SUCCESS(rc))
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

static int vboxVrListSubstNoJoin(PVBOXVR_LIST pList, uint32_t cRects, const PRTRECT aRects, bool *pfChanged)
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
static const PRTRECT vboxVrRectsOrder(uint32_t cRects, const PRTRECT aRects)
{
#ifdef DEBUG
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            RTRECT *pRectI = &aRects[i];
            for (uint32_t j = i + 1; j < cRects; ++j)
            {
                RTRECT *pRectJ = &aRects[j];
                Assert(!VBoxRectRectIsIntersect(pRectI, pRectJ));
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

void VBoxVrListTranslate(PVBOXVR_LIST pList, int32_t x, int32_t y)
{
    for (PRTLISTNODE pEntry1 = pList->ListHead.pNext; pEntry1 != &pList->ListHead; pEntry1 = pEntry1->pNext)
    {
        PVBOXVR_REG pReg1 = PVBOXVR_REG_FROM_ENTRY(pEntry1);
        VBoxRectRectTranslate(&pReg1->Rect, x, y);
    }
}

int VBoxVrListRectsSubst(PVBOXVR_LIST pList, uint32_t cRects, const PRTRECT aRects, bool *pfChanged)
{
#if 0
    const PRTRECT pRects = vboxVrRectsOrder(cRects, aRects);
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

int VBoxVrListRectsAdd(PRTLISTNODE pList, uint32_t cRects, const PRTRECT aRects, bool *pfChanged)
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
                Assert(!VBoxRectRectIsIntersect(pRectI, pRectJ));
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
            if (VBoxRectRectIsCoveres(&pReg1->Rect, &aRects[i]))
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
    PRTRECT pListRects = NULL;
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
            pListRects = (PRTRECT)RTMemAlloc(sizeof (RTRECT) * cAllocatedRects);
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

int VBoxVrListRectsGet(PVBOXVR_LIST pList, uint32_t cRects, PRTRECT aRects)
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


void VBoxVrCompositorInit(PVBOXVR_COMPOSITOR pCompositor)
{
    RTListInit(&pCompositor->List);
}

static DECLINLINE(void) vboxVrCompositorEntryAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    RTListPrepend(&pCompositor->List, &pEntry->Node);
}

static DECLINLINE(void) vboxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    RTListNodeRemove(&pEntry->Node);
}

void VBoxVrCompositorEntryInit(PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    VBoxVrListInit(&pEntry->Vr);
}

bool VBoxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    if (!VBoxVrCompositorEntryIsInList(pEntry))
        return false;
    VBoxVrListClear(&pEntry->Vr);
    vboxVrCompositorEntryRemove(pCompositor, pEntry);
    return true;
}

static int vboxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, RTRECT *paRects, bool *pfChanged)
{
    bool fChanged;
    int rc = VBoxVrListRectsSubst(&pEntry->Vr, cRects, paRects, &fChanged);
    if (RT_SUCCESS(rc))
    {
        if (VBoxVrListIsEmpty(&pEntry->Vr))
        {
            Assert(fChanged);
            vboxVrCompositorEntryRemove(pCompositor, pEntry);
        }
        if (pfChanged)
            *pfChanged = false;
        return VINF_SUCCESS;
    }

    WARN(("VBoxVrListRectsSubst failed, rc %d", rc));
    return rc;
}

int VBoxVrCompositorEntryRegionsAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, RTRECT *paRects, bool *pfChanged)
{
    bool fChanged = false, fCurChanged = false, fEntryInList = false;
    int rc = VINF_SUCCESS;
    PVBOXVR_COMPOSITOR_ENTRY pCur;

    if (!cRects)
    {
        if (pfChanged)
            *pfChanged = false;
        return VINF_SUCCESS;
    }

    RTListForEach(&pCompositor->List, pCur, VBOXVR_COMPOSITOR_ENTRY, Node)
    {
        Assert(!VBoxVrListIsEmpty(&pCur->Vr));
        if (pCur == pEntry)
        {
            fEntryInList = true;
            rc = VBoxVrListRectsAdd(&pCur->Vr, cRects, paRects, &fCurChanged);
            if (RT_SUCCESS(rc))
            {
                fChanged |= fCurChanged;
                Assert(!VBoxVrListIsEmpty(&pCur->Vr));
            }
            else
            {
                WARN(("VBoxVrListRectsAdd failed, rc %d", rc));
                return rc;
            }
        }
        else
        {
            rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pCur, cRects, paRects, fCurChanged);
            if (RT_SUCCESS(rc))
                fChanged |= fCurChanged;
            else
            {
                WARN(("vboxVrCompositorEntryRegionsSubst failed, rc %d", rc));
                return rc;
            }
        }
    }

    AssertRC(rc);

    if (pEntry && !fEntryInList)
    {
        Assert(VBoxVrListIsEmpty(&pEntry->Vr));
        rc = VBoxVrListRectsAdd(&pEntry->Vr, cRects, paRects, &fCurChanged);
        if (RT_SUCCESS(rc))
        {
            fChanged |= fCurChanged;
            if (!VBoxVrListIsEmpty(&pEntry->Vr))
            {
                Assert(fCurChanged);
                vboxVrCompositorEntryAdd(pCompositor, pEntry);
            }
        }
        else
        {
            WARN(("VBoxVrListRectsAdd failed, rc %d", rc));
            return rc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;
    return VINF_SUCCESS;
}

int VBoxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, bool *pfChanged)
{
    bool fChanged;
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

    rc = vboxVrCompositorEntryRegionsSubst(pCompositor, pEntry, cRects, paRects, pfChanged);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("pfChanged failed, rc %d", rc));
    return rc;
}

int VBoxVrCompositorEntryRegionsSet(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRects, const RTRECT *paRects, bool *pfChanged)
{
    if (!pEntry)
    {
        WARN(("VBoxVrCompositorEntryRegionsSet called with zero entry, unsupported!"));
        if (pfChanged)
            *pfChanged = false;
        return VERR_INVALID_PARAMETER;
    }

    bool fChanged = false, fCurChanged = false;
    int rc;
    fCurChanged = VBoxVrCompositorEntryRemove(pCompositor, pEntry);
    if (RT_SUCCESS(rc))
        fChanged |= fCurChanged;
    else
    {
        WARN(("VBoxVrCompositorEntryRegionsClear failed, rc %d", rc));
        return rc;
    }

    rc = VBoxVrCompositorEntryRegionsAdd(pCompositor, pEntry, cRects, paRects, fCurChanged);
    if (RT_SUCCESS(rc))
        fChanged |= fCurChanged;
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

int VBoxVrCompositorEntryRegionsTranslate(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, int32_t x, int32_t y, bool *pfChanged)
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
    uint32_t cRects;
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
            paRects = RTMemAlloc(cRects * sizeof (RTRECT));
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

void VBoxVrCompositorVisit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor)
{
    PVBOXVR_COMPOSITOR_ENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pCompositor->List, pEntry, pEntryNext, VBOXVR_COMPOSITOR_ENTRY, Node);
    {
        if (!pfnVisitor(pCompositor, pEntry, pvVisitor))
            return;
    }
}
