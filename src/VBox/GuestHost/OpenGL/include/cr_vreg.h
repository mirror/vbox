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

VBOXVREGDECL(bool) VBoxVrCompositorEntryRemove(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsAdd(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, uint32_t *pfChangeFlags);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSubst(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsSet(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged);
VBOXVREGDECL(int) VBoxVrCompositorEntryRegionsTranslate(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, int32_t x, int32_t y, bool *pfChanged);
VBOXVREGDECL(void) VBoxVrCompositorVisit(PVBOXVR_COMPOSITOR pCompositor, PFNVBOXVRCOMPOSITOR_VISITOR pfnVisitor, void *pvVisitor);

RT_C_DECLS_END

#endif /* #ifndef ___cr_vreg_h_ */
