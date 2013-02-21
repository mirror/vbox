/* $Id$ */

/** @file
 * Visible Regions processing API
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
#ifndef ___cr_vreg_h_
#define ___cr_vreg_h_

#include <iprt/list.h>
#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>

#ifndef IN_RING0
# define VBOXVREGDECL(_type) DECLEXPORT(_type)
#else
# define VBOXVREGDECL(_type) RTDECL(_type)
#endif



RT_C_DECLS_BEGIN

typedef struct VBOXVR_LIST
{
    RTLISTNODE ListHead;
    uint32_t cEntries;
} VBOXVR_LIST, *PVBOXVR_LIST;

DECLINLINE(int) VBoxRectCmp(const RTRECT * pRect1, const RTRECT * pRect2)
{
    return memcmp(pRect1, pRect2, sizeof (*pRect1));
}

DECLINLINE(void) VBoxRectTranslate(RTRECT * pRect, int32_t x, int32_t y)
{
    pRect->xLeft   += x;
    pRect->yTop    += y;
    pRect->xRight  += x;
    pRect->yBottom += y;
}

DECLINLINE(void) VBoxRectMove(RTRECT * pRect, int32_t x, int32_t y)
{
    int32_t w = pRect->xRight - pRect->xLeft;
    int32_t h = pRect->yBottom - pRect->yTop;
    pRect->xLeft   = x;
    pRect->yTop    = y;
    pRect->xRight  = w + x;
    pRect->yBottom = h + y;
}

DECLINLINE(bool) VBoxRectIsCoveres(const RTRECT *pRect, const RTRECT *pCovered)
{
    Assert(pRect);
    Assert(pCovered);
    if (pRect->xLeft > pCovered->xLeft)
        return false;
    if (pRect->yTop > pCovered->yTop)
        return false;
    if (pRect->xRight < pCovered->xRight)
        return false;
    if (pRect->yBottom < pCovered->yBottom)
        return false;
    return true;
}

DECLINLINE(bool) VBoxRectIsIntersect(const RTRECT * pRect1, const RTRECT * pRect2)
{
    return !((pRect1->xLeft < pRect2->xLeft && pRect1->xRight <= pRect2->xLeft)
            || (pRect2->xLeft < pRect1->xLeft && pRect2->xRight <= pRect1->xLeft)
            || (pRect1->yTop < pRect2->yTop && pRect1->yBottom <= pRect2->yTop)
            || (pRect2->yTop < pRect1->yTop && pRect2->yBottom <= pRect1->yTop));
}

DECLINLINE(uint32_t) VBoxVrListRectsCount(PVBOXVR_LIST pList)
{
    return pList->cEntries;
}

DECLINLINE(bool) VBoxVrListIsEmpty(const PVBOXVR_LIST pList)
{
    return !VBoxVrListRectsCount(pList);
}

DECLINLINE(void) VBoxVrListInit(PVBOXVR_LIST pList)
{
    RTListInit(&pList->ListHead);
    pList->cEntries = 0;
}

VBOXVREGDECL(void) VBoxVrListClear(PVBOXVR_LIST pList);

VBOXVREGDECL(void) VBoxVrListTranslate(PVBOXVR_LIST pList, int32_t x, int32_t y);

VBOXVREGDECL(int) VBoxVrListCmp(PVBOXVR_LIST pList1, PVBOXVR_LIST pList2);

VBOXVREGDECL(int) VBoxVrListRectsAdd(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrListRectsSubst(PVBOXVR_LIST pList, uint32_t cRects, const RTRECT * aRects, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrListRectsGet(PVBOXVR_LIST pList, uint32_t cRects, RTRECT * aRects);

VBOXVREGDECL(int) VBoxVrInit();
VBOXVREGDECL(void) VBoxVrTerm();

typedef struct VBOXVR_LIST_ITERATOR
{
    PVBOXVR_LIST pList;
    PRTLISTNODE pNextEntry;
} VBOXVR_LIST_ITERATOR, *PVBOXVR_LIST_ITERATOR;

DECLINLINE(void) VBoxVrListIterInit(PVBOXVR_LIST pList, PVBOXVR_LIST_ITERATOR pIter)
{
    pIter->pList = pList;
    pIter->pNextEntry = pList->ListHead.pNext;
}

typedef struct VBOXVR_REG
{
    RTLISTNODE ListEntry;
    RTRECT Rect;
} VBOXVR_REG, *PVBOXVR_REG;

#define PVBOXVR_REG_FROM_ENTRY(_pEntry) ((PVBOXVR_REG)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(VBOXVR_REG, ListEntry)))

DECLINLINE(PCRTRECT) VBoxVrListIterNext(PVBOXVR_LIST_ITERATOR pIter)
{
    PRTLISTNODE pNextEntry = pIter->pNextEntry;
    if (pNextEntry != &pIter->pList->ListHead)
    {
        PCRTRECT pRect = &(PVBOXVR_REG_FROM_ENTRY(pNextEntry)->Rect);
        pIter->pNextEntry = pNextEntry->pNext;
        return pRect;
    }
    return NULL;
}

typedef struct VBOXVR_COMPOSITOR_ENTRY
{
    RTLISTNODE Node;
    VBOXVR_LIST Vr;
} VBOXVR_COMPOSITOR_ENTRY, *PVBOXVR_COMPOSITOR_ENTRY;

struct VBOXVR_COMPOSITOR;

typedef DECLCALLBACK(void) FNVBOXVRCOMPOSITOR_ENTRY_REMOVED(const struct VBOXVR_COMPOSITOR *pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, PVBOXVR_COMPOSITOR_ENTRY pReplacingEntry);
typedef FNVBOXVRCOMPOSITOR_ENTRY_REMOVED *PFNVBOXVRCOMPOSITOR_ENTRY_REMOVED;

typedef struct VBOXVR_COMPOSITOR
{
    RTLISTNODE List;
    PFNVBOXVRCOMPOSITOR_ENTRY_REMOVED pfnEntryRemoved;
} VBOXVR_COMPOSITOR, *PVBOXVR_COMPOSITOR;

typedef DECLCALLBACK(bool) FNVBOXVRCOMPOSITOR_VISITOR(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, void *pvVisitor);
typedef FNVBOXVRCOMPOSITOR_VISITOR *PFNVBOXVRCOMPOSITOR_VISITOR;

VBOXVREGDECL(void) VBoxVrCompositorInit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_ENTRY_REMOVED pfnEntryRemoved);
VBOXVREGDECL(void) VBoxVrCompositorTerm(PVBOXVR_COMPOSITOR pCompositor);
VBOXVREGDECL(void) VBoxVrCompositorEntryInit(PVBOXVR_COMPOSITOR_ENTRY pEntry);
DECLINLINE(bool) VBoxVrCompositorEntryIsInList(const PVBOXVR_COMPOSITOR_ENTRY pEntry)
{
    return !VBoxVrListIsEmpty(&pEntry->Vr);
}

#define VBOXVR_COMPOSITOR_CF_ENTRIES_REGIONS_CHANGED    0x00000001
#define VBOXVR_COMPOSITOR_CF_COMPOSITED_REGIONS_CHANGED 0x00000002
#define VBOXVR_COMPOSITOR_CF_ENTRY_ADDED                0x00000004


VBOXVREGDECL(bool) VBoxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, uint32_t *pfChangeFlags);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSet(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsTranslate(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, int32_t x, int32_t y, bool *pfChanged);
VBOXVREGDECL(void) VBoxVrCompositorVisit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor);

typedef struct VBOXVR_COMPOSITOR_ITERATOR
{
    PVBOXVR_COMPOSITOR pCompositor;
    PRTLISTNODE pNextEntry;
} VBOXVR_COMPOSITOR_ITERATOR ,*PVBOXVR_COMPOSITOR_ITERATOR;

DECLINLINE(void) VBoxVrCompositorIterInit(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ITERATOR pIter)
{
    pIter->pCompositor = pCompositor;
    pIter->pNextEntry = pCompositor->List.pNext;
}

#define VBOXVR_COMPOSITOR_ENTRY_FROM_NODE(_p) ((PVBOXVR_COMPOSITOR_ENTRY)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXVR_COMPOSITOR_ENTRY, Node)))

DECLINLINE(PVBOXVR_COMPOSITOR_ENTRY) VBoxVrCompositorIterNext(PVBOXVR_COMPOSITOR_ITERATOR pIter)
{
    PRTLISTNODE pNextEntry = pIter->pNextEntry;
    if (pNextEntry != &pIter->pCompositor->List)
    {
        PVBOXVR_COMPOSITOR_ENTRY pEntry = VBOXVR_COMPOSITOR_ENTRY_FROM_NODE(pNextEntry);
        pIter->pNextEntry = pNextEntry->pNext;
        return pEntry;
    }
    return NULL;
}

/* Compositor with Stretching & Cached Rectangles info */

typedef struct VBOXVR_TEXTURE
{
    int32_t width;
    int32_t height;
    uint32_t target;
    uint32_t hwid;
} VBOXVR_TEXTURE, *PVBOXVR_TEXTURE;

typedef struct VBOXVR_SCR_COMPOSITOR_ENTRY
{
    VBOXVR_COMPOSITOR_ENTRY Ce;
    VBOXVR_TEXTURE Tex;
    RTPOINT Pos;
    uint32_t fChanged;
    uint32_t cRects;
    PRTRECT paSrcRects;
    PRTRECT paDstRects;
} VBOXVR_SCR_COMPOSITOR_ENTRY, *PVBOXVR_SCR_COMPOSITOR_ENTRY;

typedef struct VBOXVR_SCR_COMPOSITOR
{
    VBOXVR_COMPOSITOR Compositor;
    float StretchX;
    float StretchY;
    uint32_t cRects;
    uint32_t cRectsBuffer;
    PRTRECT paSrcRects;
    PRTRECT paDstRects;
    RTCRITSECT CritSect;
} VBOXVR_SCR_COMPOSITOR, *PVBOXVR_SCR_COMPOSITOR;

typedef DECLCALLBACK(bool) FNVBOXVRSCRCOMPOSITOR_VISITOR(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, void *pvVisitor);
typedef FNVBOXVRSCRCOMPOSITOR_VISITOR *PFNVBOXVRSCRCOMPOSITOR_VISITOR;


DECLINLINE(void) CrVrScrCompositorEntryInit(PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const PVBOXVR_TEXTURE pTex)
{
    VBoxVrCompositorEntryInit(&pEntry->Ce);
    pEntry->Tex = *pTex;
    memset(&pEntry->Pos, 0, sizeof (VBOXVR_SCR_COMPOSITOR_ENTRY) - RT_OFFSETOF(VBOXVR_SCR_COMPOSITOR_ENTRY, Pos));
}

DECLINLINE(bool) CrVrScrCompositorEntryIsUsed(const PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    return VBoxVrCompositorEntryIsInList(&pEntry->Ce);
}

DECLINLINE(void) CrVrScrCompositorEntrySetChanged(PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, bool fChanged)
{
    pEntry->fChanged = !!fChanged;
}

DECLINLINE(void) CrVrScrCompositorEntryTexNameUpdate(PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t hwid)
{
    pEntry->Tex.hwid = hwid;
    CrVrScrCompositorEntrySetChanged(pEntry, true);
}

DECLINLINE(bool) CrVrScrCompositorEntryIsChanged(const PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    return !!pEntry->fChanged;
}

VBOXVREGDECL(void) CrVrScrCompositorVisit(PVBOXVR_SCR_COMPOSITOR pCompositor, PFNVBOXVRSCRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor);
VBOXVREGDECL(void) CrVrScrCompositorEntrySetAllChanged(PVBOXVR_SCR_COMPOSITOR pCompositor, bool fChanged);
DECLINLINE(bool) CrVrScrCompositorEntryIsInList(const PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry)
{
    return VBoxVrCompositorEntryIsInList(&pEntry->Ce);
}
VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsAdd(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions);
VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions);
VBOXVREGDECL(int) CrVrScrCompositorEntryPosSet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, const RTPOINT *pPos);

/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorEntryRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions);
VBOXVREGDECL(int) CrVrScrCompositorEntryRemove(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry);
VBOXVREGDECL(int) CrVrScrCompositorInit(PVBOXVR_SCR_COMPOSITOR pCompositor);
VBOXVREGDECL(void) CrVrScrCompositorTerm(PVBOXVR_SCR_COMPOSITOR pCompositor);
VBOXVREGDECL(void) CrVrScrCompositorSetStretching(PVBOXVR_SCR_COMPOSITOR pCompositor, float StretchX, float StretchY);
/* regions are valid until the next CrVrScrCompositor call */
VBOXVREGDECL(int) CrVrScrCompositorRegionsGet(PVBOXVR_SCR_COMPOSITOR pCompositor, uint32_t *pcRegions, const RTRECT **ppaSrcRegions, const RTRECT **ppaDstRegions);

#define VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(_p) ((PVBOXVR_SCR_COMPOSITOR_ENTRY)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXVR_SCR_COMPOSITOR_ENTRY, Ce)))
#define VBOXVR_SCR_COMPOSITOR_FROM_COMPOSITOR(_p) ((PVBOXVR_SCR_COMPOSITOR)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXVR_SCR_COMPOSITOR, Compositor)))

typedef struct VBOXVR_SCR_COMPOSITOR_ITERATOR
{
    VBOXVR_COMPOSITOR_ITERATOR Base;
} VBOXVR_SCR_COMPOSITOR_ITERATOR ,*PVBOXVR_SCR_COMPOSITOR_ITERATOR;

DECLINLINE(void) CrVrScrCompositorIterInit(PVBOXVR_SCR_COMPOSITOR pCompositor, PVBOXVR_SCR_COMPOSITOR_ITERATOR pIter)
{
    VBoxVrCompositorIterInit(&pCompositor->Compositor, &pIter->Base);
}

DECLINLINE(PVBOXVR_SCR_COMPOSITOR_ENTRY) CrVrScrCompositorIterNext(PVBOXVR_SCR_COMPOSITOR_ITERATOR pIter)
{
    PVBOXVR_COMPOSITOR_ENTRY pCe = VBoxVrCompositorIterNext(&pIter->Base);
    if (pCe)
    {
        return VBOXVR_SCR_COMPOSITOR_ENTRY_FROM_ENTRY(pCe);
    }
    return NULL;
}

DECLINLINE(int) CrVrScrCompositorLock(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    int rc = RTCritSectEnter(&pCompositor->CritSect);
    AssertRC(rc);
    return rc;
}

DECLINLINE(int) CrVrScrCompositorUnlock(PVBOXVR_SCR_COMPOSITOR pCompositor)
{
    int rc = RTCritSectLeave(&pCompositor->CritSect);
    AssertRC(rc);
    return rc;
}

RT_C_DECLS_END

#endif /* #ifndef ___cr_vreg_h_ */
