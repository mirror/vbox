/** @file
 * IPRT - AVL Trees.
 */

/*
 * Copyright (C) 1999-2003 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_avl_h
#define ___iprt_avl_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_avl    RTAvl - AVL Trees
 * @ingroup grp_rt
 * @{
 */


/** AVL tree of void pointers.
 * @{
 */

/**
 * AVL key type
 */
typedef void *  AVLPVKEY;

/**
 * AVL Core node.
 */
typedef struct _AVLPVNodeCore
{
    AVLPVKEY                Key;        /** Key value. */
    struct _AVLPVNodeCore  *pLeft;      /** Pointer to left leaf node. */
    struct _AVLPVNodeCore  *pRight;     /** Pointer to right leaf node. */
    unsigned char           uchHeight;  /** Height of this tree: max(height(left), height(right)) + 1 */
} AVLPVNODECORE, *PAVLPVNODECORE, **PPAVLPVNODECORE;

/** A tree with void pointer keys. */
typedef PAVLPVNODECORE     AVLPVTREE;
/** Pointer to a tree with void pointer keys. */
typedef PPAVLPVNODECORE    PAVLPVTREE;

/** Callback function for AVLPVDoWithAll(). */
typedef DECLCALLBACK(int) AVLPVCALLBACK(PAVLPVNODECORE, void *);
/** Pointer to callback function for AVLPVDoWithAll(). */
typedef AVLPVCALLBACK *PAVLPVCALLBACK;

/*
 * Functions.
 */
RTDECL(bool)            RTAvlPVInsert(PAVLPVTREE ppTree, PAVLPVNODECORE pNode);
RTDECL(PAVLPVNODECORE)  RTAvlPVRemove(PAVLPVTREE ppTree, AVLPVKEY Key);
RTDECL(PAVLPVNODECORE)  RTAvlPVGet(PAVLPVTREE ppTree, AVLPVKEY Key);
RTDECL(PAVLPVNODECORE)  RTAvlPVGetBestFit(PAVLPVTREE ppTree, AVLPVKEY Key, bool fAbove);
RTDECL(PAVLPVNODECORE)  RTAvlPVRemoveBestFit(PAVLPVTREE ppTree, AVLPVKEY Key, bool fAbove);
RTDECL(int)             RTAvlPVDoWithAll(PAVLPVTREE ppTree, int fFromLeft, PAVLPVCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)             RTAvlPVDestroy(PAVLPVTREE ppTree, PAVLPVCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of unsigned long.
 * @{
 */

/**
 * AVL key type
 */
typedef unsigned long   AVLULKEY;

/**
 * AVL Core node.
 */
typedef struct _AVLULNodeCore
{
    AVLULKEY                Key;        /** Key value. */
    struct _AVLULNodeCore  *pLeft;      /** Pointer to left leaf node. */
    struct _AVLULNodeCore  *pRight;     /** Pointer to right leaf node. */
    unsigned char           uchHeight;  /** Height of this tree: max(height(left), height(right)) + 1 */
} AVLULNODECORE, *PAVLULNODECORE, **PPAVLULNODECORE;


/** Callback function for AVLULDoWithAll(). */
typedef DECLCALLBACK(int) AVLULCALLBACK(PAVLULNODECORE, void*);
/** Pointer to callback function for AVLULDoWithAll(). */
typedef AVLULCALLBACK *PAVLULCALLBACK;


/*
 * Functions.
 */
RTDECL(bool)            RTAvlULInsert(PPAVLULNODECORE ppTree, PAVLULNODECORE pNode);
RTDECL(PAVLULNODECORE)  RTAvlULRemove(PPAVLULNODECORE ppTree, AVLULKEY Key);
RTDECL(PAVLULNODECORE)  RTAvlULGet(PPAVLULNODECORE ppTree, AVLULKEY Key);
RTDECL(PAVLULNODECORE)  RTAvlULGetBestFit(PPAVLULNODECORE ppTree, AVLULKEY Key, bool fAbove);
RTDECL(PAVLULNODECORE)  RTAvlULRemoveBestFit(PPAVLULNODECORE ppTree, AVLULKEY Key, bool fAbove);
RTDECL(int)             RTAvlULDoWithAll(PPAVLULNODECORE ppTree, int fFromLeft, PAVLULCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)             RTAvlULDestroy(PPAVLULNODECORE pTree, PAVLULCALLBACK pfnCallBack, void *pvParam);

/** @} */



/** AVL tree of uint32_t
 * @{
 */

/** AVL key type. */
typedef uint32_t    AVLU32KEY;

/** AVL Core node. */
typedef struct _AVLU32NodeCore
{
    AVLU32KEY               Key;        /**< Key value. */
    struct _AVLU32NodeCore *pLeft;      /**< Pointer to left leaf node. */
    struct _AVLU32NodeCore *pRight;     /**< Pointer to right leaf node. */
    unsigned char           uchHeight;  /**< Height of this tree: max(height(left), height(right)) + 1 */
} AVLU32NODECORE, *PAVLU32NODECORE, **PPAVLU32NODECORE;

/** Callback function for AVLU32DoWithAll() & AVLU32Destroy(). */
typedef DECLCALLBACK(int) AVLU32CALLBACK(PAVLU32NODECORE, void*);
/** Pointer to callback function for AVLU32DoWithAll() & AVLU32Destroy(). */
typedef AVLU32CALLBACK *PAVLU32CALLBACK;


/*
 * Functions.
 */
RTDECL(bool)            RTAvlU32Insert(PPAVLU32NODECORE ppTree, PAVLU32NODECORE pNode);
RTDECL(PAVLU32NODECORE) RTAvlU32Remove(PPAVLU32NODECORE ppTree, AVLU32KEY Key);
RTDECL(PAVLU32NODECORE) RTAvlU32Get(PPAVLU32NODECORE ppTree, AVLU32KEY Key);
RTDECL(PAVLU32NODECORE) RTAvlU32GetBestFit(PPAVLU32NODECORE ppTree, AVLU32KEY Key, bool fAbove);
RTDECL(PAVLU32NODECORE) RTAvlU32RemoveBestFit(PPAVLU32NODECORE ppTree, AVLU32KEY Key, bool fAbove);
RTDECL(int)             RTAvlU32DoWithAll(PPAVLU32NODECORE ppTree, int fFromLeft, PAVLU32CALLBACK pfnCallBack, void *pvParam);
RTDECL(int)             RTAvlU32Destroy(PPAVLU32NODECORE pTree, PAVLU32CALLBACK pfnCallBack, void *pvParam);

/** @} */

/**
 * AVL uint32_t type for the relative offset pointer scheme.
 */
typedef int32_t     AVLOU32;

typedef uint32_t     AVLOU32KEY;

/**
 * AVL Core node.
 */
typedef struct _AVLOU32NodeCore
{
    /** Key value. */
    AVLOU32KEY          Key;
    /** Offset to the left leaf node, relative to this field. */
    AVLOU32             pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLOU32             pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLOU32NODECORE, *PAVLOU32NODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLOU32         AVLOU32TREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLOU32TREE    *PAVLOU32TREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLOU32TREE    *PPAVLOU32NODECORE;

/** Callback function for RTAvloU32DoWithAll(). */
typedef DECLCALLBACK(int)   AVLOU32CALLBACK(PAVLOU32NODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvloU32DoWithAll(). */
typedef AVLOU32CALLBACK *PAVLOU32CALLBACK;

RTDECL(bool)                  RTAvloU32Insert(PAVLOU32TREE pTree, PAVLOU32NODECORE pNode);
RTDECL(PAVLOU32NODECORE)      RTAvloU32Remove(PAVLOU32TREE pTree, AVLOU32KEY Key);
RTDECL(PAVLOU32NODECORE)      RTAvloU32Get(PAVLOU32TREE pTree, AVLOU32KEY Key);
RTDECL(int)                   RTAvloU32DoWithAll(PAVLOU32TREE pTree, int fFromLeft, PAVLOU32CALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLOU32NODECORE)      RTAvloU32GetBestFit(PAVLOU32TREE ppTree, AVLOU32KEY Key, bool fAbove);
RTDECL(PAVLOU32NODECORE)      RTAvloU32RemoveBestFit(PAVLOU32TREE ppTree, AVLOU32KEY Key, bool fAbove);
RTDECL(int)                   RTAvloU32Destroy(PAVLOU32TREE pTree, PAVLOU32CALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of uint32_t, list duplicates.
 * @{
 */

/** AVL key type. */
typedef uint32_t    AVLLU32KEY;

/** AVL Core node. */
typedef struct _AVLLU32NodeCore
{
    AVLLU32KEY                  Key;        /**< Key value. */
    unsigned char               uchHeight;  /**< Height of this tree: max(height(left), height(right)) + 1 */
    struct _AVLLU32NodeCore    *pLeft;      /**< Pointer to left leaf node. */
    struct _AVLLU32NodeCore    *pRight;     /**< Pointer to right leaf node. */
    struct _AVLLU32NodeCore    *pList;      /**< Pointer to next node with the same key. */
} AVLLU32NODECORE, *PAVLLU32NODECORE, **PPAVLLU32NODECORE;

/** Callback function for RTAvllU32DoWithAll() & RTAvllU32Destroy(). */
typedef DECLCALLBACK(int) AVLLU32CALLBACK(PAVLLU32NODECORE, void*);
/** Pointer to callback function for RTAvllU32DoWithAll() & RTAvllU32Destroy(). */
typedef AVLLU32CALLBACK *PAVLLU32CALLBACK;


/*
 * Functions.
 */
RTDECL(bool)                RTAvllU32Insert(PPAVLLU32NODECORE ppTree, PAVLLU32NODECORE pNode);
RTDECL(PAVLLU32NODECORE)    RTAvllU32Remove(PPAVLLU32NODECORE ppTree, AVLLU32KEY Key);
RTDECL(PAVLLU32NODECORE)    RTAvllU32Get(PPAVLLU32NODECORE ppTree, AVLLU32KEY Key);
RTDECL(PAVLLU32NODECORE)    RTAvllU32GetBestFit(PPAVLLU32NODECORE ppTree, AVLLU32KEY Key, bool fAbove);
RTDECL(PAVLLU32NODECORE)    RTAvllU32RemoveBestFit(PPAVLLU32NODECORE ppTree, AVLLU32KEY Key, bool fAbove);
RTDECL(int)                 RTAvllU32DoWithAll(PPAVLLU32NODECORE ppTree, int fFromLeft, PAVLLU32CALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                 RTAvllU32Destroy(PPAVLLU32NODECORE pTree, PAVLLU32CALLBACK pfnCallBack, void *pvParam);

/** @} */



/** AVL tree of RTGCPHYSes - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLOGCPHYS;

/**
 * AVL Core node.
 */
typedef struct _AVLOGCPhysNodeCore
{
    /** Key value. */
    RTGCPHYS            Key;
    /** Offset to the left leaf node, relative to this field. */
    AVLOGCPHYS          pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLOGCPHYS          pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
    /** Padding */
    unsigned char       Padding[7];
} AVLOGCPHYSNODECORE, *PAVLOGCPHYSNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLOGCPHYS         AVLOGCPHYSTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLOGCPHYSTREE    *PAVLOGCPHYSTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLOGCPHYSTREE    *PPAVLOGCPHYSNODECORE;

/** Callback function for RTAvloGCPhysDoWithAll() and RTAvloGCPhysDestroy(). */
typedef DECLCALLBACK(int)   AVLOGCPHYSCALLBACK(PAVLOGCPHYSNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvloGCPhysDoWithAll() and RTAvloGCPhysDestroy(). */
typedef AVLOGCPHYSCALLBACK *PAVLOGCPHYSCALLBACK;

RTDECL(bool)                    RTAvloGCPhysInsert(PAVLOGCPHYSTREE pTree, PAVLOGCPHYSNODECORE pNode);
RTDECL(PAVLOGCPHYSNODECORE)     RTAvloGCPhysRemove(PAVLOGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(PAVLOGCPHYSNODECORE)     RTAvloGCPhysGet(PAVLOGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(int)                     RTAvloGCPhysDoWithAll(PAVLOGCPHYSTREE pTree, int fFromLeft, PAVLOGCPHYSCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLOGCPHYSNODECORE)     RTAvloGCPhysGetBestFit(PAVLOGCPHYSTREE ppTree, RTGCPHYS Key, bool fAbove);
RTDECL(PAVLOGCPHYSNODECORE)     RTAvloGCPhysRemoveBestFit(PAVLOGCPHYSTREE ppTree, RTGCPHYS Key, bool fAbove);
RTDECL(int)                     RTAvloGCPhysDestroy(PAVLOGCPHYSTREE pTree, PAVLOGCPHYSCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of RTGCPHYS ranges - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLROGCPHYS;

/**
 * AVL Core node.
 */
typedef struct _AVLROGCPhysNodeCore
{
    /** First key value in the range (inclusive). */
    RTGCPHYS            Key;
    /** Last key value in the range (inclusive). */
    RTGCPHYS            KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    AVLROGCPHYS         pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLROGCPHYS         pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
    /** Padding */
    unsigned char       Padding[7];
} AVLROGCPHYSNODECORE, *PAVLROGCPHYSNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLROGCPHYS         AVLROGCPHYSTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLROGCPHYSTREE    *PAVLROGCPHYSTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLROGCPHYSTREE    *PPAVLROGCPHYSNODECORE;

/** Callback function for RTAvlroGCPhysDoWithAll() and RTAvlroGCPhysDestroy(). */
typedef DECLCALLBACK(int)   AVLROGCPHYSCALLBACK(PAVLROGCPHYSNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlroGCPhysDoWithAll() and RTAvlroGCPhysDestroy(). */
typedef AVLROGCPHYSCALLBACK *PAVLROGCPHYSCALLBACK;

RTDECL(bool)                    RTAvlroGCPhysInsert(PAVLROGCPHYSTREE pTree, PAVLROGCPHYSNODECORE pNode);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysRemove(PAVLROGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysGet(PAVLROGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysRangeGet(PAVLROGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysRangeRemove(PAVLROGCPHYSTREE pTree, RTGCPHYS Key);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysGetBestFit(PAVLROGCPHYSTREE ppTree, RTGCPHYS Key, bool fAbove);
RTDECL(int)                     RTAvlroGCPhysDoWithAll(PAVLROGCPHYSTREE pTree, int fFromLeft, PAVLROGCPHYSCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                     RTAvlroGCPhysDestroy(PAVLROGCPHYSTREE pTree, PAVLROGCPHYSCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysGetRoot(PAVLROGCPHYSTREE pTree);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysGetLeft(PAVLROGCPHYSNODECORE pNode);
RTDECL(PAVLROGCPHYSNODECORE)    RTAvlroGCPhysGetRight(PAVLROGCPHYSNODECORE pNode);

/** @} */


/** AVL tree of RTGCPTRs.
 * @{
 */

/**
 * AVL Core node.
 */
typedef struct _AVLGCPtrNodeCore
{
    /** Key value. */
    RTGCPTR             Key;
    /** Pointer to the left node. */
    struct _AVLGCPtrNodeCore *pLeft;
    /** Pointer to the right node. */
    struct _AVLGCPtrNodeCore *pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLGCPTRNODECORE, *PAVLGCPTRNODECORE, **PPAVLGCPTRNODECORE;

/** A tree of RTGCPTR keys. */
typedef PAVLGCPTRNODECORE     AVLGCPTRTREE;
/** Pointer to a tree of RTGCPTR keys. */
typedef PPAVLGCPTRNODECORE    PAVLGCPTRTREE;

/** Callback function for RTAvlGCPtrDoWithAll(). */
typedef DECLCALLBACK(int)   AVLGCPTRCALLBACK(PAVLGCPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlGCPtrDoWithAll(). */
typedef AVLGCPTRCALLBACK *PAVLGCPTRCALLBACK;

RTDECL(bool)                    RTAvlGCPtrInsert(PAVLGCPTRTREE pTree, PAVLGCPTRNODECORE pNode);
RTDECL(PAVLGCPTRNODECORE)       RTAvlGCPtrRemove(PAVLGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLGCPTRNODECORE)       RTAvlGCPtrGet(PAVLGCPTRTREE pTree, RTGCPTR Key);
RTDECL(int)                     RTAvlGCPtrDoWithAll(PAVLGCPTRTREE pTree, int fFromLeft, PAVLGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLGCPTRNODECORE)       RTAvlGCPtrGetBestFit(PAVLGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(PAVLGCPTRNODECORE)       RTAvlGCPtrRemoveBestFit(PAVLGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(int)                     RTAvlGCPtrDestroy(PAVLGCPTRTREE pTree, PAVLGCPTRCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of RTGCPTRs - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLOGCPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLOGCPtrNodeCore
{
    /** Key value. */
    RTGCPTR             Key;
    /** Offset to the left leaf node, relative to this field. */
    AVLOGCPTR           pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLOGCPTR           pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
    unsigned char       padding[GC_ARCH_BITS == 64 ? 7 : 3];
} AVLOGCPTRNODECORE, *PAVLOGCPTRNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLOGCPTR         AVLOGCPTRTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLOGCPTRTREE    *PAVLOGCPTRTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLOGCPTRTREE    *PPAVLOGCPTRNODECORE;

/** Callback function for RTAvloGCPtrDoWithAll(). */
typedef DECLCALLBACK(int)   AVLOGCPTRCALLBACK(PAVLOGCPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvloGCPtrDoWithAll(). */
typedef AVLOGCPTRCALLBACK *PAVLOGCPTRCALLBACK;

RTDECL(bool)                    RTAvloGCPtrInsert(PAVLOGCPTRTREE pTree, PAVLOGCPTRNODECORE pNode);
RTDECL(PAVLOGCPTRNODECORE)      RTAvloGCPtrRemove(PAVLOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLOGCPTRNODECORE)      RTAvloGCPtrGet(PAVLOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(int)                     RTAvloGCPtrDoWithAll(PAVLOGCPTRTREE pTree, int fFromLeft, PAVLOGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLOGCPTRNODECORE)      RTAvloGCPtrGetBestFit(PAVLOGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(PAVLOGCPTRNODECORE)      RTAvloGCPtrRemoveBestFit(PAVLOGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(int)                     RTAvloGCPtrDestroy(PAVLOGCPTRTREE pTree, PAVLOGCPTRCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of RTGCPTR ranges.
 * @{
 */

/**
 * AVL Core node.
 */
typedef struct _AVLRGCPtrNodeCore
{
    /** First key value in the range (inclusive). */
    RTGCPTR             Key;
    /** Last key value in the range (inclusive). */
    RTGCPTR             KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    struct _AVLRGCPtrNodeCore  *pLeft;
    /** Offset to the right leaf node, relative to this field. */
    struct _AVLRGCPtrNodeCore  *pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLRGCPTRNODECORE, *PAVLRGCPTRNODECORE;

/** A offset base tree with RTGCPTR keys. */
typedef PAVLRGCPTRNODECORE AVLRGCPTRTREE;
/** Pointer to a offset base tree with RTGCPTR keys. */
typedef AVLRGCPTRTREE    *PAVLRGCPTRTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLRGCPTRTREE    *PPAVLRGCPTRNODECORE;

/** Callback function for RTAvlrGCPtrDoWithAll() and RTAvlrGCPtrDestroy(). */
typedef DECLCALLBACK(int)   AVLRGCPTRCALLBACK(PAVLRGCPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlrGCPtrDoWithAll() and RTAvlrGCPtrDestroy(). */
typedef AVLRGCPTRCALLBACK *PAVLRGCPTRCALLBACK;

RTDECL(bool)                   RTAvlrGCPtrInsert(       PAVLRGCPTRTREE pTree, PAVLRGCPTRNODECORE pNode);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrRemove(       PAVLRGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrGet(          PAVLRGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrGetBestFit(   PAVLRGCPTRTREE pTree, RTGCPTR Key, bool fAbove);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrRangeGet(     PAVLRGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrRangeRemove(  PAVLRGCPTRTREE pTree, RTGCPTR Key);
RTDECL(int)                    RTAvlrGCPtrDoWithAll(    PAVLRGCPTRTREE pTree, int fFromLeft, PAVLRGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                    RTAvlrGCPtrDestroy(      PAVLRGCPTRTREE pTree, PAVLRGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrGetRoot(      PAVLRGCPTRTREE pTree);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrGetLeft(      PAVLRGCPTRNODECORE pNode);
RTDECL(PAVLRGCPTRNODECORE)     RTAvlrGCPtrGetRight(     PAVLRGCPTRNODECORE pNode);

/** @} */


/** AVL tree of RTGCPTR ranges - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLROGCPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLROGCPtrNodeCore
{
    /** First key value in the range (inclusive). */
    RTGCPTR             Key;
    /** Last key value in the range (inclusive). */
    RTGCPTR             KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    AVLROGCPTR          pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLROGCPTR          pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
    unsigned char       padding[GC_ARCH_BITS == 64 ? 7 : 7];
} AVLROGCPTRNODECORE, *PAVLROGCPTRNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLROGCPTR         AVLROGCPTRTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLROGCPTRTREE    *PAVLROGCPTRTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLROGCPTRTREE    *PPAVLROGCPTRNODECORE;

/** Callback function for RTAvlroGCPtrDoWithAll() and RTAvlroGCPtrDestroy(). */
typedef DECLCALLBACK(int)   AVLROGCPTRCALLBACK(PAVLROGCPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlroGCPtrDoWithAll() and RTAvlroGCPtrDestroy(). */
typedef AVLROGCPTRCALLBACK *PAVLROGCPTRCALLBACK;

RTDECL(bool)                    RTAvlroGCPtrInsert(PAVLROGCPTRTREE pTree, PAVLROGCPTRNODECORE pNode);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrRemove(PAVLROGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrGet(PAVLROGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrGetBestFit(PAVLROGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrRangeGet(PAVLROGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrRangeRemove(PAVLROGCPTRTREE pTree, RTGCPTR Key);
RTDECL(int)                     RTAvlroGCPtrDoWithAll(PAVLROGCPTRTREE pTree, int fFromLeft, PAVLROGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                     RTAvlroGCPtrDestroy(PAVLROGCPTRTREE pTree, PAVLROGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrGetRoot(PAVLROGCPTRTREE pTree);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrGetLeft(PAVLROGCPTRNODECORE pNode);
RTDECL(PAVLROGCPTRNODECORE)     RTAvlroGCPtrGetRight(PAVLROGCPTRNODECORE pNode);

/** @} */


/** AVL tree of RTGCPTR ranges (overlapping supported) - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLROOGCPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLROOGCPtrNodeCore
{
    /** First key value in the range (inclusive). */
    RTGCPTR             Key;
    /** Last key value in the range (inclusive). */
    RTGCPTR             KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    AVLROOGCPTR         pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLROOGCPTR         pRight;
    /** Pointer to the list of string with the same key. Don't touch. */
    AVLROOGCPTR         pList;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLROOGCPTRNODECORE, *PAVLROOGCPTRNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLROOGCPTR         AVLROOGCPTRTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLROOGCPTRTREE    *PAVLROOGCPTRTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLROOGCPTRTREE    *PPAVLROOGCPTRNODECORE;

/** Callback function for RTAvlrooGCPtrDoWithAll() and RTAvlrooGCPtrDestroy(). */
typedef DECLCALLBACK(int)   AVLROOGCPTRCALLBACK(PAVLROOGCPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlrooGCPtrDoWithAll() and RTAvlrooGCPtrDestroy(). */
typedef AVLROOGCPTRCALLBACK *PAVLROOGCPTRCALLBACK;

RTDECL(bool)                    RTAvlrooGCPtrInsert(PAVLROOGCPTRTREE pTree, PAVLROOGCPTRNODECORE pNode);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrRemove(PAVLROOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGet(PAVLROOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGetBestFit(PAVLROOGCPTRTREE ppTree, RTGCPTR Key, bool fAbove);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrRangeGet(PAVLROOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrRangeRemove(PAVLROOGCPTRTREE pTree, RTGCPTR Key);
RTDECL(int)                     RTAvlrooGCPtrDoWithAll(PAVLROOGCPTRTREE pTree, int fFromLeft, PAVLROOGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                     RTAvlrooGCPtrDestroy(PAVLROOGCPTRTREE pTree, PAVLROOGCPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGetRoot(PAVLROOGCPTRTREE pTree);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGetLeft(PAVLROOGCPTRNODECORE pNode);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGetRight(PAVLROOGCPTRNODECORE pNode);
RTDECL(PAVLROOGCPTRNODECORE)    RTAvlrooGCPtrGetNextEqual(PAVLROOGCPTRNODECORE pNode);

/** @} */


/** AVL tree of RTUINTPTR ranges.
 * @{
 */

/**
 * AVL Core node.
 */
typedef struct _AVLRUIntPtrNodeCore
{
    /** First key value in the range (inclusive). */
    RTUINTPTR           Key;
    /** Last key value in the range (inclusive). */
    RTUINTPTR           KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    struct _AVLRUIntPtrNodeCore *pLeft;
    /** Offset to the right leaf node, relative to this field. */
    struct _AVLRUIntPtrNodeCore *pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLRUINTPTRNODECORE, *PAVLRUINTPTRNODECORE;

/** A offset base tree with RTUINTPTR keys. */
typedef PAVLRUINTPTRNODECORE AVLRUINTPTRTREE;
/** Pointer to a offset base tree with RTUINTPTR keys. */
typedef AVLRUINTPTRTREE     *PAVLRUINTPTRTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLRUINTPTRTREE     *PPAVLRUINTPTRNODECORE;

/** Callback function for RTAvlrUIntPtrDoWithAll() and RTAvlrUIntPtrDestroy(). */
typedef DECLCALLBACK(int)    AVLRUINTPTRCALLBACK(PAVLRUINTPTRNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlrUIntPtrDoWithAll() and RTAvlrUIntPtrDestroy(). */
typedef AVLRUINTPTRCALLBACK *PAVLRUINTPTRCALLBACK;

RTDECL(bool)                   RTAvlrUIntPtrInsert(     PAVLRUINTPTRTREE pTree, PAVLRUINTPTRNODECORE pNode);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrRemove(     PAVLRUINTPTRTREE pTree, RTUINTPTR Key);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrGet(        PAVLRUINTPTRTREE pTree, RTUINTPTR Key);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrGetBestFit( PAVLRUINTPTRTREE pTree, RTUINTPTR Key, bool fAbove);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrRangeGet(   PAVLRUINTPTRTREE pTree, RTUINTPTR Key);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrRangeRemove(PAVLRUINTPTRTREE pTree, RTUINTPTR Key);
RTDECL(int)                    RTAvlrUIntPtrDoWithAll(  PAVLRUINTPTRTREE pTree, int fFromLeft, PAVLRUINTPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                    RTAvlrUIntPtrDestroy(    PAVLRUINTPTRTREE pTree, PAVLRUINTPTRCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrGetRoot(    PAVLRUINTPTRTREE pTree);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrGetLeft(    PAVLRUINTPTRNODECORE pNode);
RTDECL(PAVLRUINTPTRNODECORE)   RTAvlrUIntPtrGetRight(   PAVLRUINTPTRNODECORE pNode);

/** @} */


/** AVL tree of RTHCPHYSes - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLOHCPHYS;

/**
 * AVL Core node.
 */
typedef struct _AVLOHCPhysNodeCore
{
    /** Key value. */
    RTHCPHYS            Key;
    /** Offset to the left leaf node, relative to this field. */
    AVLOHCPHYS          pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLOHCPHYS          pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
#if HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64
    unsigned char       Padding[7]; /**< Alignment padding. */
#endif
} AVLOHCPHYSNODECORE, *PAVLOHCPHYSNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLOHCPHYS         AVLOHCPHYSTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLOHCPHYSTREE    *PAVLOHCPHYSTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLOHCPHYSTREE    *PPAVLOHCPHYSNODECORE;

/** Callback function for RTAvloHCPhysDoWithAll() and RTAvloHCPhysDestroy(). */
typedef DECLCALLBACK(int)   AVLOHCPHYSCALLBACK(PAVLOHCPHYSNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvloHCPhysDoWithAll() and RTAvloHCPhysDestroy(). */
typedef AVLOHCPHYSCALLBACK *PAVLOHCPHYSCALLBACK;

RTDECL(bool)                    RTAvloHCPhysInsert(PAVLOHCPHYSTREE pTree, PAVLOHCPHYSNODECORE pNode);
RTDECL(PAVLOHCPHYSNODECORE)     RTAvloHCPhysRemove(PAVLOHCPHYSTREE pTree, RTHCPHYS Key);
RTDECL(PAVLOHCPHYSNODECORE)     RTAvloHCPhysGet(PAVLOHCPHYSTREE pTree, RTHCPHYS Key);
RTDECL(int)                     RTAvloHCPhysDoWithAll(PAVLOHCPHYSTREE pTree, int fFromLeft, PAVLOHCPHYSCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLOHCPHYSNODECORE)     RTAvloHCPhysGetBestFit(PAVLOHCPHYSTREE ppTree, RTHCPHYS Key, bool fAbove);
RTDECL(PAVLOHCPHYSNODECORE)     RTAvloHCPhysRemoveBestFit(PAVLOHCPHYSTREE ppTree, RTHCPHYS Key, bool fAbove);
RTDECL(int)                     RTAvloHCPhysDestroy(PAVLOHCPHYSTREE pTree, PAVLOHCPHYSCALLBACK pfnCallBack, void *pvParam);

/** @} */



/** AVL tree of RTIOPORTs - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLOIOPORTPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLOIOPortNodeCore
{
    /** Offset to the left leaf node, relative to this field. */
    AVLOIOPORTPTR       pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLOIOPORTPTR       pRight;
    /** Key value. */
    RTIOPORT            Key;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLOIOPORTNODECORE, *PAVLOIOPORTNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLOIOPORTPTR      AVLOIOPORTTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLOIOPORTTREE    *PAVLOIOPORTTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLOIOPORTTREE    *PPAVLOIOPORTNODECORE;

/** Callback function for RTAvloIOPortDoWithAll() and RTAvloIOPortDestroy(). */
typedef DECLCALLBACK(int)   AVLOIOPORTCALLBACK(PAVLOIOPORTNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvloIOPortDoWithAll() and RTAvloIOPortDestroy(). */
typedef AVLOIOPORTCALLBACK *PAVLOIOPORTCALLBACK;

RTDECL(bool)                    RTAvloIOPortInsert(PAVLOIOPORTTREE pTree, PAVLOIOPORTNODECORE pNode);
RTDECL(PAVLOIOPORTNODECORE)     RTAvloIOPortRemove(PAVLOIOPORTTREE pTree, RTIOPORT Key);
RTDECL(PAVLOIOPORTNODECORE)     RTAvloIOPortGet(PAVLOIOPORTTREE pTree, RTIOPORT Key);
RTDECL(int)                     RTAvloIOPortDoWithAll(PAVLOIOPORTTREE pTree, int fFromLeft, PAVLOIOPORTCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLOIOPORTNODECORE)     RTAvloIOPortGetBestFit(PAVLOIOPORTTREE ppTree, RTIOPORT Key, bool fAbove);
RTDECL(PAVLOIOPORTNODECORE)     RTAvloIOPortRemoveBestFit(PAVLOIOPORTTREE ppTree, RTIOPORT Key, bool fAbove);
RTDECL(int)                     RTAvloIOPortDestroy(PAVLOIOPORTTREE pTree, PAVLOIOPORTCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of RTIOPORT ranges - using relative offsets internally.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef int32_t     AVLROIOPORTPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLROIOPortNodeCore
{
    /** First key value in the range (inclusive). */
    RTIOPORT            Key;
    /** Last key value in the range (inclusive). */
    RTIOPORT            KeyLast;
    /** Offset to the left leaf node, relative to this field. */
    AVLROIOPORTPTR      pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLROIOPORTPTR      pRight;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLROIOPORTNODECORE, *PAVLROIOPORTNODECORE;

/** A offset base tree with uint32_t keys. */
typedef AVLROIOPORTPTR      AVLROIOPORTTREE;
/** Pointer to a offset base tree with uint32_t keys. */
typedef AVLROIOPORTTREE    *PAVLROIOPORTTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLROIOPORTTREE    *PPAVLROIOPORTNODECORE;

/** Callback function for RTAvlroIOPortDoWithAll() and RTAvlroIOPortDestroy(). */
typedef DECLCALLBACK(int)   AVLROIOPORTCALLBACK(PAVLROIOPORTNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlroIOPortDoWithAll() and RTAvlroIOPortDestroy(). */
typedef AVLROIOPORTCALLBACK *PAVLROIOPORTCALLBACK;

RTDECL(bool)                    RTAvlroIOPortInsert(PAVLROIOPORTTREE pTree, PAVLROIOPORTNODECORE pNode);
RTDECL(PAVLROIOPORTNODECORE)    RTAvlroIOPortRemove(PAVLROIOPORTTREE pTree, RTIOPORT Key);
RTDECL(PAVLROIOPORTNODECORE)    RTAvlroIOPortGet(PAVLROIOPORTTREE pTree, RTIOPORT Key);
RTDECL(PAVLROIOPORTNODECORE)    RTAvlroIOPortRangeGet(PAVLROIOPORTTREE pTree, RTIOPORT Key);
RTDECL(PAVLROIOPORTNODECORE)    RTAvlroIOPortRangeRemove(PAVLROIOPORTTREE pTree, RTIOPORT Key);
RTDECL(int)                     RTAvlroIOPortDoWithAll(PAVLROIOPORTTREE pTree, int fFromLeft, PAVLROIOPORTCALLBACK pfnCallBack, void *pvParam);
RTDECL(int)                     RTAvlroIOPortDestroy(PAVLROIOPORTTREE pTree, PAVLROIOPORTCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** AVL tree of RTHCPHYSes.
 * @{
 */

/**
 * AVL 'pointer' type for the relative offset pointer scheme.
 */
typedef struct _AVLHCPhysNodeCore  *AVLHCPHYSPTR;

/**
 * AVL Core node.
 */
typedef struct _AVLHCPhysNodeCore
{
    /** Offset to the left leaf node, relative to this field. */
    AVLHCPHYSPTR        pLeft;
    /** Offset to the right leaf node, relative to this field. */
    AVLHCPHYSPTR        pRight;
    /** Key value. */
    RTHCPHYS            Key;
    /** Height of this tree: max(height(left), height(right)) + 1 */
    unsigned char       uchHeight;
} AVLHCPHYSNODECORE, *PAVLHCPHYSNODECORE;

/** A offset base tree with RTHCPHYS keys. */
typedef AVLHCPHYSPTR      AVLHCPHYSTREE;
/** Pointer to a offset base tree with RTHCPHYS keys. */
typedef AVLHCPHYSTREE    *PAVLHCPHYSTREE;

/** Pointer to an internal tree pointer.
 * In this case it's a pointer to a relative offset. */
typedef AVLHCPHYSTREE    *PPAVLHCPHYSNODECORE;

/** Callback function for RTAvlHCPhysDoWithAll() and RTAvlHCPhysDestroy(). */
typedef DECLCALLBACK(int)   AVLHCPHYSCALLBACK(PAVLHCPHYSNODECORE pNode, void *pvUser);
/** Pointer to callback function for RTAvlHCPhysDoWithAll() and RTAvlHCPhysDestroy(). */
typedef AVLHCPHYSCALLBACK *PAVLHCPHYSCALLBACK;

RTDECL(bool)                    RTAvlHCPhysInsert(PAVLHCPHYSTREE pTree, PAVLHCPHYSNODECORE pNode);
RTDECL(PAVLHCPHYSNODECORE)      RTAvlHCPhysRemove(PAVLHCPHYSTREE pTree, RTHCPHYS Key);
RTDECL(PAVLHCPHYSNODECORE)      RTAvlHCPhysGet(PAVLHCPHYSTREE pTree, RTHCPHYS Key);
RTDECL(int)                     RTAvlHCPhysDoWithAll(PAVLHCPHYSTREE pTree, int fFromLeft, PAVLHCPHYSCALLBACK pfnCallBack, void *pvParam);
RTDECL(PAVLHCPHYSNODECORE)      RTAvlHCPhysGetBestFit(PAVLHCPHYSTREE ppTree, RTHCPHYS Key, bool fAbove);
RTDECL(PAVLHCPHYSNODECORE)      RTAvlHCPhysRemoveBestFit(PAVLHCPHYSTREE ppTree, RTHCPHYS Key, bool fAbove);
RTDECL(int)                     RTAvlHCPhysDestroy(PAVLHCPHYSTREE pTree, PAVLHCPHYSCALLBACK pfnCallBack, void *pvParam);

/** @} */


/** @} */

__END_DECLS

#endif

