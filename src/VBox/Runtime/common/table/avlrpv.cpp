/* $Id$ */
/** @file
 * IPRT - AVL tree, void *, range, unique keys.
 */

/*
 * Copyright (C) 2001-2010 knut st. osmundsen (bird-src-spam@anduin.net)
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

#ifndef NOFILEID
static const char szFileId[] = "Id: kAVLPVInt.c,v 1.5 2003/02/13 02:02:35 bird Exp $";
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/*
 * AVL configuration.
 */
#define KAVL_FN(a)                  RTAvlrPV##a
#define KAVL_MAX_STACK              27  /* Up to 2^24 nodes. */
#define KAVL_CHECK_FOR_EQUAL_INSERT 1   /* No duplicate keys! */
#define KAVLNODECORE                AVLRPVNODECORE
#define PKAVLNODECORE               PAVLRPVNODECORE
#define PPKAVLNODECORE              PPAVLRPVNODECORE
#define KAVLKEY                     AVLRPVKEY
#define PKAVLKEY                    PAVLRPVKEY
#define KAVLENUMDATA                AVLRPVENUMDATA
#define PKAVLENUMDATA               PAVLRPVENUMDATA
#define PKAVLCALLBACK               PAVLRPVCALLBACK
#define KAVL_RANGE                  1


/*
 * AVL Compare macros
 */
#define KAVL_G(key1, key2)          ( (uintptr_t)(key1) >  (uintptr_t)(key2) )
#define KAVL_E(key1, key2)          ( (uintptr_t)(key1) == (uintptr_t)(key2) )
#define KAVL_NE(key1, key2)         ( (uintptr_t)(key1) != (uintptr_t)(key2) )
#define KAVL_R_IS_IDENTICAL(key1B, key2B, key1E, key2E)     ( (uintptr_t)(key1B) == (uintptr_t)(key2B) && (uintptr_t)(key1E) == (uintptr_t)(key2E) )
#define KAVL_R_IS_INTERSECTING(key1B, key2B, key1E, key2E)  ( (uintptr_t)(key1B) <= (uintptr_t)(key2E) && (uintptr_t)(key1E) >= (uintptr_t)(key2B) )
#define KAVL_R_IS_IN_RANGE(key1B, key1E, key2)              KAVL_R_IS_INTERSECTING(key1B, key2, key1E, key2)


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/avl.h>
#include <iprt/assert.h>
#include <iprt/err.h>

/*
 * Include the code.
 */
#define SSToDS(ptr) ptr
#define KMAX RT_MAX
#define kASSERT Assert
#include "avl_Base.cpp.h"
#include "avl_Get.cpp.h"
#include "avl_GetBestFit.cpp.h"
#include "avl_Range.cpp.h"
#include "avl_RemoveBestFit.cpp.h"
#include "avl_DoWithAll.cpp.h"
#include "avl_Destroy.cpp.h"

