/* $Id$ */
/** @file
 * kAVLRange  - Range routines for AVL trees.
 */

/*
 * Copyright (C) 1999-2006 knut st. osmundsen (bird-src-spam@anduin.net)
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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
RTDECL(PKAVLNODECORE) KAVL_FN(RangeGet)(PPKAVLNODECORE ppTree, register KAVLKEY Key)
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
