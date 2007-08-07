/* $Id$ */
/** @file
 * Enumeration routines for AVL trees.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _kAVLEnum_h_
#define _kAVLEnum_h_


/**
 * Gets the root node.
 *
 * @returns Pointer to the root node.
 * @returns NULL if the tree is empty.
 *
 * @param   ppTree      Pointer to pointer to the tree root node.
 */
RTDECL(PKAVLNODECORE) KAVL_FN(GetRoot)(PPKAVLNODECORE ppTree)
{
    return KAVL_GET_POINTER_NULL(ppTree);
}


/**
 * Gets the right node.
 *
 * @returns Pointer to the right node.
 * @returns NULL if no right node.
 *
 * @param   pNode       The current node.
 */
RTDECL(PKAVLNODECORE)    KAVL_FN(GetRight)(PKAVLNODECORE pNode)
{
    if (pNode)
        return KAVL_GET_POINTER_NULL(&pNode->pRight);
    return NULL;
}


/**
 * Gets the left node.
 *
 * @returns Pointer to the left node.
 * @returns NULL if no left node.
 *
 * @param   pNode       The current node.
 */
RTDECL(PKAVLNODECORE) KAVL_FN(GetLeft)(PKAVLNODECORE pNode)
{
    if (pNode)
        return KAVL_GET_POINTER_NULL(&pNode->pLeft);
    return NULL;
}


# ifdef KAVL_EQUAL_ALLOWED
/**
 * Gets the next node with an equal (start) key.
 *
 * @returns Pointer to the next equal node.
 * @returns NULL if the current node was the last one with this key.
 *
 * @param   pNode       The current node.
 */
RTDECL(PKAVLNODECORE) KAVL_FN(GetNextEqual)(PKAVLNODECORE pNode)
{
    if (pNode)
        return KAVL_GET_POINTER_NULL(&pNode->pList);
    return NULL;
}
# endif /* KAVL_EQUAL_ALLOWED */

#endif

