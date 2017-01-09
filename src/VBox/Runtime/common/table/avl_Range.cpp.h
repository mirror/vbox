/* $Id$ */
/** @file
 * kAVLRange  - Range routines for AVL trees.
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

#ifndef _kAVLRange_h_
#define _kAVLRange_h_

/**
 * Finds the range containing the specified key.
 *
 * @returns   Pointer to the matching range.
 *
 * @param     ppTree  Pointer to Pointer to the tree root node.
 * @param     Key     The Key to find matching range for.
 */
KAVL_DECL(PKAVLNODECORE) KAVL_FN(RangeGet)(PPKAVLNODECORE ppTree, register KAVLKEY Key)
{
    register PKAVLNODECORE  pNode = KAVL_GET_POINTER_NULL(ppTree);
    if (pNode)
    {
        for (;;)
        {
            if (KAVL_R_IS_IN_RANGE(pNode->Key, pNode->KeyLast, Key))
                return pNode;
            if (KAVL_G(pNode->Key, Key))
            {
                if (pNode->pLeft != KAVL_NULL)
                    pNode = KAVL_GET_POINTER(&pNode->pLeft);
                else
                    return NULL;
            }
            else
            {
                if (pNode->pRight != KAVL_NULL)
                    pNode = KAVL_GET_POINTER(&pNode->pRight);
                else
                    return NULL;
            }
        }
    }

    return NULL;
}


/**
 * Removes the range containing the specified key.
 *
 * @returns   Pointer to the matching range.
 *
 * @param     ppTree  Pointer to Pointer to the tree root node.
 * @param     Key     The Key to remove matching range for.
 */
RTDECL(PKAVLNODECORE) KAVL_FN(RangeRemove)(PPKAVLNODECORE ppTree, KAVLKEY Key)
{
    PKAVLNODECORE pNode = KAVL_FN(RangeGet)(ppTree, Key);
    if (pNode)
        return KAVL_FN(Remove)(ppTree, pNode->Key);
    return NULL;
}

#endif
