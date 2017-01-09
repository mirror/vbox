/* $Id$ */
/** @file
 * kAVLDoWithAll - Do with all nodes routine for AVL trees.
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

#ifndef _kAVLDoWithAll_h_
#define _kAVLDoWithAll_h_


/**
 * Iterates thru all nodes in the given tree.
 * @returns   0 on success. Return from callback on failure.
 * @param     ppTree   Pointer to the AVL-tree root node pointer.
 * @param     fFromLeft    TRUE:  Left to right.
 *                         FALSE: Right to left.
 * @param     pfnCallBack  Pointer to callback function.
 * @param     pvParam      Userparameter passed on to the callback function.
 */
KAVL_DECL(int) KAVL_FN(DoWithAll)(PPKAVLNODECORE ppTree, int fFromLeft, PKAVLCALLBACK pfnCallBack, void * pvParam)
{
    KAVLSTACK2      AVLStack;
    PKAVLNODECORE   pNode;
#ifdef KAVL_EQUAL_ALLOWED
    PKAVLNODECORE   pEqual;
#endif
    int             rc;

    if (*ppTree == KAVL_NULL)
        return VINF_SUCCESS;

    AVLStack.cEntries = 1;
    AVLStack.achFlags[0] = 0;
    AVLStack.aEntries[0] = KAVL_GET_POINTER(ppTree);

    if (fFromLeft)
    {   /* from left */
        while (AVLStack.cEntries > 0)
        {
            pNode = AVLStack.aEntries[AVLStack.cEntries - 1];

            /* left */
            if (!AVLStack.achFlags[AVLStack.cEntries - 1]++)
            {
                if (pNode->pLeft != KAVL_NULL)
                {
                    AVLStack.achFlags[AVLStack.cEntries] = 0; /* 0 first, 1 last */
                    AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pLeft);
                    continue;
                }
            }

            /* center */
            rc = pfnCallBack(pNode, pvParam);
            if (rc != VINF_SUCCESS)
                return rc;
#ifdef KAVL_EQUAL_ALLOWED
            if (pNode->pList != KAVL_NULL)
                for (pEqual = KAVL_GET_POINTER(&pNode->pList); pEqual; pEqual = KAVL_GET_POINTER_NULL(&pEqual->pList))
                {
                    rc = pfnCallBack(pEqual, pvParam);
                    if (rc != VINF_SUCCESS)
                        return rc;
                }
#endif

            /* right */
            AVLStack.cEntries--;
            if (pNode->pRight != KAVL_NULL)
            {
                AVLStack.achFlags[AVLStack.cEntries] = 0;
                AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pRight);
            }
        } /* while */
    }
    else
    {   /* from right */
        while (AVLStack.cEntries > 0)
        {
            pNode = AVLStack.aEntries[AVLStack.cEntries - 1];

            /* right */
            if (!AVLStack.achFlags[AVLStack.cEntries - 1]++)
            {
                if (pNode->pRight != KAVL_NULL)
                {
                    AVLStack.achFlags[AVLStack.cEntries] = 0;  /* 0 first, 1 last */
                    AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pRight);
                    continue;
                }
            }

            /* center */
            rc = pfnCallBack(pNode, pvParam);
            if (rc != VINF_SUCCESS)
                return rc;
#ifdef KAVL_EQUAL_ALLOWED
            if (pNode->pList != KAVL_NULL)
                for (pEqual = KAVL_GET_POINTER(&pNode->pList); pEqual; pEqual = KAVL_GET_POINTER_NULL(&pEqual->pList))
                {
                    rc = pfnCallBack(pEqual, pvParam);
                    if (rc != VINF_SUCCESS)
                        return rc;
                }
#endif

            /* left */
            AVLStack.cEntries--;
            if (pNode->pLeft != KAVL_NULL)
            {
                AVLStack.achFlags[AVLStack.cEntries] = 0;
                AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pLeft);
            }
        } /* while */
    }

    return VINF_SUCCESS;
}


#endif

