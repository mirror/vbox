/* $Id$ */
/** @file
 * kAVLGet - get routine for AVL trees.
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

#ifndef _kAVLGet_h_
#define _kAVLGet_h_


/**
 * Gets a node from the tree (does not remove it!)
 * @returns   Pointer to the node holding the given key.
 * @param     ppTree  Pointer to the AVL-tree root node pointer.
 * @param     Key     Key value of the node which is to be found.
 * @author    knut st. osmundsen
 */
RTDECL(PKAVLNODECORE) KAVL_FN(Get)(PPKAVLNODECORE ppTree, KAVLKEY Key)
{
    register PKAVLNODECORE  pNode = KAVL_GET_POINTER_NULL(ppTree);

    if (pNode)
    {
        while (KAVL_NE(pNode->Key, Key))
        {
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

    return pNode;
}


#endif
