/* $Id$ */
/** @file
 * kAVLRemove2 - Remove specific node (by pointer) from an AVL tree.
 */

/*
 * Copyright (C) 2001-2012 knut st. osmundsen (bird-src-spam@anduin.net)
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


/**
 * Removes the specified node from the tree.
 *
 * @returns Pointer to the removed node (NULL if not in the tree)
 * @param   ppTree      Pointer to the AVL-tree root structure.
 * @param   pNode       Pointer to the node to be removed.
 *
 * @remark  This implementation isn't the most efficient, but it's relatively
 *          short and easier to manage.
 */
KAVL_DECL(PKAVLNODECORE) KAVL_FN(RemoveNode)(PPKAVLNODECORE ppTree, PKAVLNODECORE pNode)
{
#ifdef KAVL_EQUAL_ALLOWED
    /*
     * Find the right node by key together with the parent node.
     */
    KAVLKEY const   Key      = pNode->Key;
    PKAVLNODECORE   pParent  = NULL;
    PKAVLNODECORE   pCurNode = KAVL_GET_POINTER_NULL(ppTree);
    if (!pCurNode)
        return NULL;
    while (KAVL_NE(pCurNode->Key, Key))
    {
        pParent = pCurNode;
        if (KAVL_G(pCurNode->Key, Key))
        {
            if (pCurNode->pLeft != KAVL_NULL)
                pCurNode = KAVL_GET_POINTER(&pCurNode->pLeft);
            else
                return NULL;
        }
        else
        {
            if (pCurNode->pRight != KAVL_NULL)
                pCurNode = KAVL_GET_POINTER(&pCurNode->pRight);
            else
                return NULL;
        }
    }

    if (pCurNode != pNode)
    {
        /*
         * It's not the one we want, but it could be in the duplicate list.
         */
        while (pCurNode->pList != KAVL_NULL)
        {
            PKAVLNODECORE pNext = KAVL_GET_POINTER(&pCurNode->pList);
            if (pNext == pNode)
            {
                if (pNode->pList != KAVL_NULL)
                    KAVL_SET_POINTER(&pCurNode->pList, KAVL_GET_POINTER(&pNode->pList));
                else
                    pCurNode->pList = KAVL_NULL;
                pNode->pList = KAVL_NULL;
                return pNode;
            }
            pCurNode = pNext;
        }
        return NULL;
    }

    /*
     * Ok, it's the one we want alright.
     *
     * Simply remove it if it's the only one with they Key, if there are
     * duplicates we'll have to unlink it and  insert the first duplicate
     * in our place.
     */
    if (pNode->pList == KAVL_NULL)
        KAVL_FN(Remove)(ppTree, pNode->Key);
    else
    {
        PKAVLNODECORE pNewUs = KAVL_GET_POINTER(&pNode->pList);

        pNewUs->uchHeight = pNode->uchHeight;

        if (pNode->pLeft != KAVL_NULL)
            KAVL_SET_POINTER(&pNewUs->pLeft, KAVL_GET_POINTER(&pNode->pLeft));
        else
            pNewUs->pLeft = KAVL_NULL;

        if (pNode->pRight != KAVL_NULL)
            KAVL_SET_POINTER(&pNewUs->pRight, KAVL_GET_POINTER(&pNode->pRight));
        else
            pNewUs->pRight = KAVL_NULL;

        if (pParent)
        {
            if (KAVL_GET_POINTER_NULL(&pParent->pLeft) == pNode)
                KAVL_SET_POINTER(&pParent->pLeft, pNewUs);
            else
                KAVL_SET_POINTER(&pParent->pRight, pNewUs);
        }
        else
            KAVL_SET_POINTER(ppTree, pNewUs);
    }

    return pNode;

#else
    /*
     * Delete it, if we got the wrong one, reinsert it.
     *
     * This ASSUMS that the caller is NOT going to hand us a lot
     * of wrong nodes but just uses this API for his convenience.
     */
    KAVLNODE *pRemovedNode = KAVL_FN(Remove)(pRoot, pNode->Key);
    if (pRemovedNode == pNode)
        return pRemovedNode;

    KAVL_FN(Insert)(pRoot, pRemovedNode);
    return NULL;
#endif
}

