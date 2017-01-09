/* $Id$ */
/** @file
 * kAVLRemoveBestFit - Remove Best Fit routine for AVL trees.
 *                     Intended specially on heaps. The tree should allow duplicate keys.
 *
 */

/*
 * Copyright (C) 1999-2011 knut st. osmundsen (bird-src-spam@anduin.net)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
KAVL_DECL(PKAVLNODECORE) KAVL_FN(RemoveBestFit)(PPKAVLNODECORE ppTree, KAVLKEY Key, bool fAbove)
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
