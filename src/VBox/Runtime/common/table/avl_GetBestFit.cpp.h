/* $Id$ */
/** @file
 * kAVLGetBestFit - Get Best Fit routine for AVL trees.
 *                  Intended specially on heaps. The tree should allow duplicate keys.
 *
 */

/*
 * Copyright (C) 1999-2012 knut st. osmundsen (bird-src-spam@anduin.net)
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

#ifndef _kAVLGetBestFit_h_
#define _kAVLGetBestFit_h_


/**
 * Finds the best fitting node in the tree for the given Key value.
 * @returns   Pointer to the best fitting node found.
 * @param     ppTree  Pointer to Pointer to the tree root node.
 * @param     Key     The Key of which is to be found a best fitting match for..
 * @param     fAbove  TRUE:  Returned node is have the closest key to Key from above.
 *                    FALSE: Returned node is have the closest key to Key from below.
 * @sketch    The best fitting node is always located in the searchpath above you.
 *            >= (above): The node where you last turned left.
 *            <= (below): the node where you last turned right.
 */
KAVL_DECL(PKAVLNODECORE) KAVL_FN(GetBestFit)(PPKAVLNODECORE ppTree, KAVLKEY Key, bool fAbove)
{
    register PKAVLNODECORE  pNode = KAVL_GET_POINTER_NULL(ppTree);
    if (pNode)
    {
        PKAVLNODECORE           pNodeLast = NULL;
        if (fAbove)
        {   /* pNode->Key >= Key */
            while (KAVL_NE(pNode->Key, Key))
            {
                if (KAVL_G(pNode->Key, Key))
                {
                    if (pNode->pLeft != KAVL_NULL)
                    {
                        pNodeLast = pNode;
                        pNode = KAVL_GET_POINTER(&pNode->pLeft);
                    }
                    else
                        return pNode;
                }
                else
                {
                    if (pNode->pRight != KAVL_NULL)
                        pNode = KAVL_GET_POINTER(&pNode->pRight);
                    else
                        return pNodeLast;
                }
            }
        }
        else
        {   /* pNode->Key <= Key */
            while (KAVL_NE(pNode->Key, Key))
            {
                if (KAVL_G(pNode->Key, Key))
                {
                    if (pNode->pLeft != KAVL_NULL)
                        pNode = KAVL_GET_POINTER(&pNode->pLeft);
                    else
                        return pNodeLast;
                }
                else
                {
                    if (pNode->pRight != KAVL_NULL)
                    {
                        pNodeLast = pNode;
                        pNode = KAVL_GET_POINTER(&pNode->pRight);
                    }
                    else
                        return pNode;
                }
            }
        }
    }

    /* perfect match or nothing. */
    return pNode;
}


#endif
