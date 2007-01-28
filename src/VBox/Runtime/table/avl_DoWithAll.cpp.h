/* $Id$ */
/** @file
 * kAVLDoWithAll - Do with all nodes routine for AVL trees.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef _kAVLDoWithAll_h_
#define _kAVLDoWithAll_h_


/**
 * Iterates tru all nodes in the given tree.
 * @returns   0 on success. Return from callback on failure.
 * @param     ppTree   Pointer to the AVL-tree root node pointer.
 * @param     fFromLeft    TRUE:  Left to right.
 *                         FALSE: Right to left.
 * @param     pfnCallBack  Pointer to callback function.
 * @param     pvParam      Userparameter passed on to the callback function.
 */
RTDECL(int) KAVL_FN(DoWithAll)(PPKAVLNODECORE ppTree, int fFromLeft, PKAVLCALLBACK pfnCallBack, void * pvParam)
{
    KAVLSTACK2      AVLStack;
    PKAVLNODECORE   pNode;
    int             rc;

    if (*ppTree == KAVL_NULL)
        return 0;

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
            if (rc)
                return rc;

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
            if (rc)
                return rc;

            /* left */
            AVLStack.cEntries--;
            if (pNode->pLeft != KAVL_NULL)
            {
                AVLStack.achFlags[AVLStack.cEntries] = 0;
                AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pLeft);
            }
        } /* while */
    }

    return 0;
}


#endif

