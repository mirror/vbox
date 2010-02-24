/* $Id$ */
/** @file
 * IPRT Testcase - List interface.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/list.h>

typedef struct LISTELEM
{
    /** Test data */
    unsigned    idx;
    /** Node */
    RTLISTNODE  Node;
} LISTELEM, *PLISTELEM;

static void tstRTListOrder(RTTEST hTest, PRTLISTNODE pList, unsigned cElements, unsigned idxStart, unsigned idxEnd, unsigned idxStep)
{
    RTTEST_CHECK(hTest, RTListIsEmpty(pList) == false);
    RTTEST_CHECK(hTest, RTListNodeGetFirst(pList, LISTELEM, Node) != NULL);
    RTTEST_CHECK(hTest, RTListNodeGetLast(pList, LISTELEM, Node) != NULL);
    if (cElements > 1)
        RTTEST_CHECK(hTest, RTListNodeGetLast(pList, LISTELEM, Node) != RTListNodeGetFirst(pList, LISTELEM, Node));
    else
        RTTEST_CHECK(hTest, RTListNodeGetLast(pList, LISTELEM, Node) == RTListNodeGetFirst(pList, LISTELEM, Node));

    /* Check that the order is right. */
    PLISTELEM pNode = RTListNodeGetFirst(pList, LISTELEM, Node);
    for (unsigned i = idxStart; i < idxEnd; i += idxStep)
    {
        RTTEST_CHECK(hTest, pNode->idx == i);
        pNode = RTListNodeGetNext(&pNode->Node, LISTELEM, Node);
    }

    RTTEST_CHECK(hTest, pNode->idx == idxEnd);
    RTTEST_CHECK(hTest, RTListNodeGetLast(pList, LISTELEM, Node) == pNode);
    RTTEST_CHECK(hTest, RTListNodeIsLast(pList, &pNode->Node) == true);

    /* Check reverse order */
    pNode = RTListNodeGetLast(pList, LISTELEM, Node);
    for (unsigned i = idxEnd; i > idxStart; i -= idxStep)
    {
        RTTEST_CHECK(hTest, pNode->idx == i);
        pNode = RTListNodeGetPrev(&pNode->Node, LISTELEM, Node);
    }

    RTTEST_CHECK(hTest, pNode->idx == idxStart);
    RTTEST_CHECK(hTest, RTListNodeGetFirst(pList, LISTELEM, Node) == pNode);
    RTTEST_CHECK(hTest, RTListNodeIsFirst(pList, &pNode->Node) == true);
}

static void tstRTListCreate(RTTEST hTest, unsigned cElements)
{
    RTTestISubF("RTList - Test with %u elements", cElements);

    RTLISTNODE ListHead;

    RTListInit(&ListHead);
    RTTEST_CHECK(hTest, RTListIsEmpty(&ListHead) == true);
    RTTEST_CHECK(hTest, RTListNodeGetFirst(&ListHead, LISTELEM, Node) == NULL);
    RTTEST_CHECK(hTest, RTListNodeGetLast(&ListHead, LISTELEM, Node) == NULL);

    /* Create the list */
    for (unsigned i = 0; i< cElements; i++)
    {
        PLISTELEM pNode = (PLISTELEM)RTMemAlloc(sizeof(LISTELEM));

        pNode->idx = i;
        pNode->Node.pPrev = NULL;
        pNode->Node.pNext = NULL;
        RTListAppend(&ListHead, &pNode->Node);
    }

    tstRTListOrder(hTest, &ListHead, cElements, 0, cElements-1, 1);

    /* Remove elements now  */
    if (cElements > 1)
    {
        /* Remove every second */
        RTTestISub("Remove every second node");

        PLISTELEM pNode = RTListNodeGetFirst(&ListHead, LISTELEM, Node);
        for (unsigned i = 0; i < cElements; i++)
        {
            PLISTELEM pNext = RTListNodeGetNext(&pNode->Node, LISTELEM, Node);

            if (!(pNode->idx % 2))
            {
                RTListNodeRemove(&pNode->Node);
                RTMemFree(pNode);
            }

            pNode = pNext;
        }

        bool fElementsEven = (cElements % 2) == 0;
        unsigned idxEnd = fElementsEven ? cElements - 1 : cElements - 2;

        cElements /= 2;
        tstRTListOrder(hTest, &ListHead, cElements, 1, idxEnd, 2);
    }

    /* Remove the rest now. */
    RTTestISub("Remove all nodes");
    PLISTELEM pNode = RTListNodeGetFirst(&ListHead, LISTELEM, Node);
    for (unsigned i = 0; i < cElements; i++)
    {
        PLISTELEM pNext = RTListNodeGetNext(&pNode->Node, LISTELEM, Node);

        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);
        pNode = pNext;
    }

    /* List should be empty again */
    RTTEST_CHECK(hTest, RTListIsEmpty(&ListHead) == true);
    RTTEST_CHECK(hTest, RTListNodeGetFirst(&ListHead, LISTELEM, Node) == NULL);
    RTTEST_CHECK(hTest, RTListNodeGetLast(&ListHead, LISTELEM, Node) == NULL);
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTList", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstRTListCreate(hTest,   1);
    tstRTListCreate(hTest,   2);
    tstRTListCreate(hTest,   3);
    tstRTListCreate(hTest,  99);
    tstRTListCreate(hTest, 100);
    tstRTListCreate(hTest, 101);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

