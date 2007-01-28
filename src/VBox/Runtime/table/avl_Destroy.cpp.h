/* $Id$ */
/** @file
 * kAVLDestroy - Walk the tree calling a callback to destroy all the nodes.
 */

/*
 * Copyright (C) 1999-2004 knut st. osmundsen (bird-src-spam@anduin.net)
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

#ifndef _kAVLDestroy_h_
#define _kAVLDestroy_h_


/**
 * Iterates thru all nodes in the given tree so the caller can free resources
 * associated with each node.
 *
 * @returns     0 on success.
 * @returns     Return code from the callback on failure. The tree might be half
 *              destroyed at this point and will not behave correctly when any
 *              insert or remove operation is attempted.
 *
 * @param       ppTree          Pointer to the AVL-tree root node pointer.
 * @param       pfnCallBack     Pointer to callback function.
 * @param       pvParam         User parameter passed on to the callback function.
 */
RTDECL(int) KAVL_FN(Destroy)(PPKAVLNODECORE ppTree, PKAVLCALLBACK pfnCallBack, void *pvParam)
{
    KAVLSTACK2      AVLStack;
    if (*ppTree == KAVL_NULL)
        return 0;

    AVLStack.cEntries = 1;
    AVLStack.achFlags[0] = 0;
    AVLStack.aEntries[0] = KAVL_GET_POINTER(ppTree);
    while (AVLStack.cEntries > 0)
    {
        int             rc;
        PKAVLNODECORE   pNode = AVLStack.aEntries[AVLStack.cEntries - 1];

        if (!AVLStack.achFlags[AVLStack.cEntries - 1]++)
        {
            /* push left and recurse */
            if (pNode->pLeft != KAVL_NULL)
            {
                AVLStack.achFlags[AVLStack.cEntries] = 0; /* 0 first, 1 last */
                AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pLeft);
                continue;
            }
        }

        /* pop pNode */
        AVLStack.cEntries--;

        /* push right */
        if (pNode->pRight != KAVL_NULL)
        {
            AVLStack.achFlags[AVLStack.cEntries] = 0;
            AVLStack.aEntries[AVLStack.cEntries++] = KAVL_GET_POINTER(&pNode->pRight);
        }

        /* call destructor */
        pNode->pRight = pNode->pLeft = KAVL_NULL;
        rc = pfnCallBack(pNode, pvParam);
        if (rc)
            return rc;

    } /* while */

    *ppTree = KAVL_NULL;
    return 0;
}


#endif


