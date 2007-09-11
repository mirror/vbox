/* $Id$ */
/** @file
 * kAVLRemoveBestFit - Remove Best Fit routine for AVL trees.
 *                     Intended specially on heaps. The tree should allow duplicate keys.
 *
 */

/*
 * Copyright (C) 1999-2002 knut st. osmundsen (bird-src-spam@anduin.net)
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _kAVLRemoveBestFit_h_
#define _kAVLRemoveBestFit_h_


/**
 * Finds the best fitting node in the tree for the given Key value.
 * And removes it.
 * @returns   Pointer to the best fitting node found.
 * @param     ppTree  Pointer to Pointer to the tree root node.
 * @param     Key     The Key of which is to be found a best fitting match for..
 * @param     fAbove  TRUE:  Returned node is have the closest key to Key from above.
 *                    FALSE: Returned node is have the closest key to Key from below.
 * @sketch    The best fitting node is always located in the searchpath above you.
 *            >= (above): The node where you last turned left.
 *            <= (below): the node where you last turned right.
 * @remark    This implementation should be speeded up slightly!
 */
RTDECL(PKAVLNODECORE) KAVL_FN(RemoveBestFit)(PPKAVLNODECORE ppTree, KAVLKEY Key, bool fAbove)
{
    /*
     * If we find anything we'll have to remove the node and return it.
     * But, if duplicate keys are allowed we'll have to check for multiple
     * nodes first and return one of them before doing an expensive remove+insert.
     */
    PKAVLNODECORE   pNode = KAVL_FN(GetBestFit)(ppTree, Key, fAbove);
    if (pNode != NULL)
    {
#ifdef KAVL_EQUAL_ALLOWED
        if (pNode->pList != KAVL_NULL)
        {
            PKAVLNODECORE pRet = KAVL_GET_POINTER(&pNode->pList);
            KAVL_SET_POINTER_NULL(&pNode->pList, &pRet->pList);
            return pRet;
        }
#endif
        pNode = KAVL_FN(Remove)(ppTree, pNode->Key);
    }
    return pNode;
}


#endif
