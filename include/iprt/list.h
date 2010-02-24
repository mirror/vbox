/** @file
 * IPRT - List handling functions.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ___iprt_list_h
#define ___iprt_list_h

#include <iprt/types.h>
#include <iprt/cdefs.h>

/** @defgroup grp_rt_list    RTList - Generic List Interface.
 * @ingroup grp_rt
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * A list node of a double linked list.
 */
typedef struct RTLISTNODE
{
    /** Pointer to the next list node. */
    struct RTLISTNODE *pNext;
    /** Pointer to the previous list node. */
    struct RTLISTNODE *pPrev;
} RTLISTNODE;
/** Pointer to a list node. */
typedef RTLISTNODE *PRTLISTNODE;
/** Pointer to a list node pointer. */
typedef PRTLISTNODE *PPRTLISTNODE;

/**
 * Initialize a list.
 *
 * @returns nothing.
 * @param   pList    Pointer to an unitialised list.
 */
DECLINLINE(void) RTListInit(PRTLISTNODE pList)
{
    pList->pNext = pList;
    pList->pPrev = pList;
}

/**
 * Append a node to the end of the list
 *
 * @returns nothing.
 * @param   pList    The list to append the node to.
 * @param   pNode    The node to append.
 */
DECLINLINE(void) RTListAppend(PRTLISTNODE pList, PRTLISTNODE pNode)
{
    pList->pPrev->pNext = pNode;
    pNode->pPrev        = pList->pPrev;
    pNode->pNext        = pList;
    pList->pPrev        = pNode;
}

/**
 * Add a node as the first element of the list
 *
 * @returns nothing.
 * @param   pList    The list to prepend the node to.
 * @param   pNode    The node to prepend.
 */
DECLINLINE(void) RTListPrepend(PRTLISTNODE pList, PRTLISTNODE pNode)
{
    pList->pNext->pPrev = pNode;
    pNode->pNext        = pList->pNext;
    pNode->pPrev        = pList;
    pList->pNext        = pNode;
}

/**
 * Remove a node from a list.
 *
 * @returns nothing.
 * @param   pNode    The node to remove.
 */
DECLINLINE(void) RTListNodeRemove(PRTLISTNODE pNode)
{
    PRTLISTNODE pPrev = pNode->pPrev;
    PRTLISTNODE pNext = pNode->pNext;

    pPrev->pNext = pNext;
    pNext->pPrev = pPrev;
}

/**
 * Checks if a node is the last element in the list
 *
 * @returns true if the node is the last element in the list.
 *          false otherwise
 * @param   list    The list.
 * @param   node    The node to check.
 */
#define RTListNodeIsLast(list, node) ((node)->pNext == list)

/**
 * Checks if a node is the first element in the list
 *
 * @returns true if the node is the first element in the list.
 *          false otherwise
 * @param   list    The list.
 * @param   node    The node to check.
 */
#define RTListNodeIsFirst(list, node) ((node)->pPrev == list)

/**
 * Checks if a list is empty.
 *
 * @returns true if the list is empty
 *          false otherwise
 * @param   list    The list to check.
 */
#define RTListIsEmpty(list) ((list)->pPrev == list)

/**
 * Returns the next node in the list.
 *
 * @returns Next node.
 * @param   node   The current node.
 * @param   type   Structure the list node is a member of.
 * @param   member The list node member.
 */
#define RTListNodeGetNext(node, type, member) ((type *)((uint8_t *)((node)->pNext) - RT_OFFSETOF(type, member)))

/**
 * Returns the previous node in the list.
 *
 * @returns Next node.
 * @param   node   The current node.
 * @param   type   Structure the list node is a member of.
 * @param   member The list node member.
 */
#define RTListNodeGetPrev(node, type, member) ((type *)((uint8_t *)((node)->pPrev) - RT_OFFSETOF(type, member)))

/**
 * Returns the first element in the list
 * or NULL if the list is empty.
 *
 * @returns Pointer to the first list element
 *          or NULL if the list is empty.
 * @param   list    List to get the first element from.
 * @param   type   Structure the list node is a member of.
 * @param   member The list node member.
 */
#define RTListNodeGetFirst(list, type, member) (RTListIsEmpty(list) ? NULL : RTListNodeGetNext(list, type, member))

/**
 * Returns the last element in the list
 * or NULL if the list is empty.
 *
 * @returns Pointer to the last list element
 *          or NULL if the list is empty.
 * @param   list    List to get the first element from.
 * @param   type   Structure the list node is a member of.
 * @param   member The list node member.
 */
#define RTListNodeGetLast(list, type, member) (RTListIsEmpty(list) ? NULL : RTListNodeGetPrev(list, type, member))

RT_C_DECLS_END

/** @} */

#endif
