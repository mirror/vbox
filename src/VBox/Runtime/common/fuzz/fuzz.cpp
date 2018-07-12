/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, core.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/fuzz.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/vfs.h>


#define RTFUZZCTX_MAGIC UINT32_C(0xdeadc0de) /** @todo */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the internal fuzzer state. */
typedef struct RTFUZZCTXINT *PRTFUZZCTXINT;

/**
 * A fuzzing input seed.
 */
typedef struct RTFUZZINPUTINT
{
    /** The AVL tree core. */
    AVLU64NODECORE              Core;
    /** Node for the global list. */
    RTLISTNODE                  NdInputs;
    /** Reference counter. */
    volatile uint32_t           cRefs;
/** @todo add magic here (unused padding space on 64-bit hosts). */
    /** The fuzzer this input belongs to. */
    PRTFUZZCTXINT               pFuzzer;
    /** Complete MD5 hash of the input data. */
    uint8_t                     abMd5Hash[RTMD5_HASH_SIZE];
    /** Size of the input data. */
    size_t                      cbInput;
    /** Input data - variable in size. */
    uint8_t                     abInput[1];
} RTFUZZINPUTINT;
/** Pointer to the internal input state. */
typedef RTFUZZINPUTINT *PRTFUZZINPUTINT;
/** Pointer to an internal input state pointer. */
typedef PRTFUZZINPUTINT *PPRTFUZZINPUTINT;


/**
 * Intermediate indexing structure.
 */
typedef struct RTFUZZINTERMEDIATE
{
    /** The AVL tree core. */
    AVLU64NODECORE              Core;
    /** The AVL tree for indexing the input seeds (keyed by the lower half of the MD5). */
    AVLU64TREE                  TreeSeedsLow;
} RTFUZZINTERMEDIATE;
/** Pointer to an intermediate indexing state. */
typedef RTFUZZINTERMEDIATE *PRTFUZZINTERMEDIATE;
/** Pointer to an intermediate indexing state pointer. */
typedef PRTFUZZINTERMEDIATE *PPRTFUZZINTERMEDIATE;

/**
 * The fuzzer state.
 */
typedef struct RTFUZZCTXINT
{
    /** Magic value for identification. */
    uint32_t                    u32Magic;
    /** Reference counter. */
    volatile uint32_t           cRefs;
    /** The random number generator. */
    RTRAND                      hRand;
    /** The AVL tree for indexing the input seeds (keyed by the upper half of the MD5). */
    AVLU64TREE                  TreeSeedsHigh;
    /** Sequential list of all inputs. */
    RTLISTANCHOR                LstInputs;
    /** Number of inputs currently in the tree. */
    uint32_t                    cInputs;
    /** The maximum size of one input seed to generate. */
    size_t                      cbInputMax;
    /** Behavioral flags. */
    uint32_t                    fFlagsBehavioral;
} RTFUZZCTXINT;


/**
 * Available mutators enum.
 */
typedef enum RTFUZZCTXMUTATOR
{
    /** Invalid mutator, not used. */
    RTFUZZCTXMUTATOR_INVALID = 0,
    /** Flips a single bit in the input. */
    RTFUZZCTXMUTATOR_BIT_FLIP,
    /** Replaces a single byte in the input. */
    RTFUZZCTXMUTATOR_BYTE_REPLACE,
    /** Inserts a byte sequence into the input. */
    RTFUZZCTXMUTATOR_BYTE_SEQUENCE_INSERT,
    /** Appends a byte sequence to the input. */
    RTFUZZCTXMUTATOR_BYTE_SEQUENCE_APPEND,
    /** Deletes a single byte from the input. */
    RTFUZZCTXMUTATOR_BYTE_DELETE,
    /** Deletes a sequence of bytes from the input. */
    RTFUZZCTXMUTATOR_BYTE_SEQUENCE_DELETE,
    /** Last valid mutator. */
    RTFUZZCTXMUTATOR_LAST = RTFUZZCTXMUTATOR_BYTE_SEQUENCE_DELETE,
    /** 32bit hack. */
    RTFUZZCTXMUTATOR_32BIT_HACK = 0x7fffffff
} RTFUZZCTXMUTATOR;
/** Pointer to a mutator enum. */
typedef RTFUZZCTXMUTATOR *PRTFUZZCTXMUTATOR;


/**
 * The fuzzer state to be exported - all members are stored in little endian form.
 */
typedef struct RTFUZZCTXSTATE
{
    /** Magic value for identification. */
    uint32_t                    u32Magic;
    /** Size of the PRNG state following in bytes. */
    uint32_t                    cbPrng;
    /** Number of input descriptors following. */
    uint32_t                    cInputs;
    /** Behavioral flags. */
    uint32_t                    fFlagsBehavioral;
    /** Maximum input size to generate. */
    uint64_t                    cbInputMax;
} RTFUZZCTXSTATE;
/** Pointer to a fuzzing context state. */
typedef RTFUZZCTXSTATE *PRTFUZZCTXSTATE;


/**
 * Mutator callback.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pvBuf               The input buffer to mutate.
 * @param   cbBuf               Size of the buffer in bytes.
 * @param   ppInputMutated      Where to store the pointer to the mutated input success.
 */
typedef DECLCALLBACK(int) FNRTFUZZCTXMUTATOR(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated);
/** Pointer to a mutator callback. */
typedef FNRTFUZZCTXMUTATOR *PFNRTFUZZCTXMUTATOR;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlip(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                 PPRTFUZZINPUTINT ppInputMutated);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplace(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                     PPRTFUZZINPUTINT ppInputMutated);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsert(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                            PPRTFUZZINPUTINT ppInputMutated);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceAppend(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                            PPRTFUZZINPUTINT ppInputMutated);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteDelete(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                    PPRTFUZZINPUTINT ppInputMutated);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDelete(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf,
                                                            PPRTFUZZINPUTINT ppInputMutated);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Array of all available mutators.
 */
static PFNRTFUZZCTXMUTATOR const g_apfnMutators[] =
{
    NULL,
    rtFuzzCtxMutatorBitFlip,
    rtFuzzCtxMutatorByteReplace,
    rtFuzzCtxMutatorByteSequenceInsert,
    rtFuzzCtxMutatorByteSequenceAppend,
    rtFuzzCtxMutatorByteDelete,
    rtFuzzCtxMutatorByteSequenceDelete,
    NULL
};



/**
 * Tries to locate an input seed with the given input MD5 seed.
 *
 * @returns Pointer to the input seed on success or NULL if not found.
 * @param   pThis               The fuzzer context instance.
 * @param   pbMd5Hash           The MD5 hash to search for.
 * @param   fExact              Flag whether to search for an exact match or return the next best fit if no match exists.
 * @param   ppIntermediate      Where to store the pointer to the intermediate layer upon success, optional.
 */
static PRTFUZZINPUTINT rtFuzzCtxInputLocate(PRTFUZZCTXINT pThis, uint8_t *pbMd5Hash, bool fExact, PPRTFUZZINTERMEDIATE ppIntermediate)
{
    PRTFUZZINPUTINT pInput = NULL;
    uint64_t u64Md5High = *(uint64_t *)&pbMd5Hash[RTMD5_HASH_SIZE / 2];
    uint64_t u64Md5Low = *(uint64_t *)&pbMd5Hash[0];
    PRTFUZZINTERMEDIATE pIntermediate = (PRTFUZZINTERMEDIATE)RTAvlU64Get(&pThis->TreeSeedsHigh, u64Md5High);
    if (!fExact && !pIntermediate)
        pIntermediate = (PRTFUZZINTERMEDIATE)RTAvlU64GetBestFit(&pThis->TreeSeedsHigh, u64Md5High, true /*fAbove*/);
    if (!fExact && !pIntermediate)
        pIntermediate = (PRTFUZZINTERMEDIATE)RTAvlU64GetBestFit(&pThis->TreeSeedsHigh, u64Md5High, false /*fAbove*/);

    if (pIntermediate)
    {
        /* 2nd level lookup. */
        pInput = (PRTFUZZINPUTINT)RTAvlU64Get(&pIntermediate->TreeSeedsLow, u64Md5Low);
        if (!fExact && !pInput)
            pInput = (PRTFUZZINPUTINT)RTAvlU64GetBestFit(&pIntermediate->TreeSeedsLow, u64Md5Low, true /*fAbove*/);
        if (!fExact && !pInput)
            pInput = (PRTFUZZINPUTINT)RTAvlU64GetBestFit(&pIntermediate->TreeSeedsLow, u64Md5Low, false /*fAbove*/);
    }

    if (ppIntermediate)
        *ppIntermediate = pIntermediate;

    return pInput;
}


/**
 * Adds the given input to the corpus of the given fuzzer context.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pInput              The input to add.
 */
static int rtFuzzCtxInputAdd(PRTFUZZCTXINT pThis, PRTFUZZINPUTINT pInput)
{
    int rc = VINF_SUCCESS;
    uint64_t u64Md5High = *(uint64_t *)&pInput->abMd5Hash[RTMD5_HASH_SIZE / 2];
    uint64_t u64Md5Low = *(uint64_t *)&pInput->abMd5Hash[0];

    pInput->Core.Key = u64Md5Low;
    PRTFUZZINTERMEDIATE pIntermediate = (PRTFUZZINTERMEDIATE)RTAvlU64Get(&pThis->TreeSeedsHigh, u64Md5High);
    if (!pIntermediate)
    {
        pIntermediate = (PRTFUZZINTERMEDIATE)RTMemAllocZ(sizeof(*pIntermediate));
        if (RT_LIKELY(pIntermediate))
        {
            pIntermediate->Core.Key = u64Md5High;
            pIntermediate->TreeSeedsLow = NULL;
            bool fIns = RTAvlU64Insert(&pThis->TreeSeedsHigh, &pIntermediate->Core);
            Assert(fIns); RT_NOREF(fIns);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pIntermediate);
        bool fIns = RTAvlU64Insert(&pIntermediate->TreeSeedsLow, &pInput->Core);
        if (!fIns)
            rc = VERR_ALREADY_EXISTS;
        else
        {
            RTListAppend(&pThis->LstInputs, &pInput->NdInputs);
            pThis->cInputs++;
            RTFuzzInputRetain(pInput);
        }
    }

    return rc;
}


/**
 * Returns a random input from the corpus of the given fuzzer context.
 *
 * @returns Pointer to a randomly picked input.
 * @param   pThis               The fuzzer context instance.
 */
static PRTFUZZINPUTINT rtFuzzCtxInputPickRnd(PRTFUZZCTXINT pThis)
{
    /* Generate a random MD5 hash and do a non exact localisation. */
    uint8_t abDigestRnd[RTMD5_HASH_SIZE];
    RTRandAdvBytes(pThis->hRand, &abDigestRnd[0], sizeof(abDigestRnd));

    return rtFuzzCtxInputLocate(pThis, &abDigestRnd[0], false /*fExact*/, NULL /*ppIntermediate*/);
}


/**
 * Clones a given input.
 *
 * @returns Pointer to the cloned input or NULL if out of memory.
 * @param   pThis               The fuzzer context instance.
 * @param   pvBuf               The buffer to clone.
 * @param   cbBuf               Size of the buffer in bytes.
 */
static PRTFUZZINPUTINT rtFuzzCtxInputClone(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf)
{
    PRTFUZZINPUTINT pInpClone = (PRTFUZZINPUTINT)RTMemAllocZ(RT_UOFFSETOF_DYN(RTFUZZINPUTINT, abInput[cbBuf]));
    if (RT_LIKELY(pInpClone))
    {
        pInpClone->cRefs    = 1;
        pInpClone->pFuzzer  = pThis,
        pInpClone->cbInput  = cbBuf;
        memcpy(&pInpClone->abInput[0], pvBuf, cbBuf);
    }

    return pInpClone;
}


/**
 * Creates an empty input seed capable of holding the given number of bytes.
 *
 * @returns Pointer to the newly created input seed.
 * @param   pThis               The fuzzer context instance.
 * @param   cbInput             Input seed size in bytes.
 */
static PRTFUZZINPUTINT rtFuzzCtxInputCreate(PRTFUZZCTXINT pThis, size_t cbInput)
{
    PRTFUZZINPUTINT pInput = (PRTFUZZINPUTINT)RTMemAllocZ(RT_UOFFSETOF_DYN(RTFUZZINPUTINT, abInput[cbInput]));
    if (RT_LIKELY(pInput))
    {
        pInput->pFuzzer = pThis;
        pInput->cRefs   = 1;
        pInput->cbInput = cbInput;
    }

    return pInput;
}


/**
 * Destroys the given input.
 *
 * @returns nothing.
 * @param   pInput              The input to destroy.
 */
static void rtFuzzInputDestroy(PRTFUZZINPUTINT pInput)
{
    RTMemFree(pInput);
}


/**
 * Destorys the given fuzzer context freeing all allocated resources.
 *
 * @returns nothing.
 * @param   pThis               The fuzzer context instance.
 */
static void rtFuzzCtxDestroy(PRTFUZZCTXINT pThis)
{
    RT_NOREF(pThis);
}


/**
 * Mutator callback - flips a single bit in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlip(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputClone(pThis, pvBuf, cbBuf);
    if (RT_LIKELY(pInputMutated))
    {
        int32_t iBit = RTRandAdvS32Ex(pThis->hRand, 0, (uint32_t)cbBuf * 8 - 1);
        ASMBitToggle(&pInputMutated->abInput[0], iBit);
        RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
        *ppInputMutated = pInputMutated;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Mutator callback - replaces a single byte in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplace(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputClone(pThis, pvBuf, cbBuf);
    if (RT_LIKELY(pInputMutated))
    {
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        uint32_t offByte = RTRandAdvU32Ex(pThis->hRand, 0, (uint32_t)cbBuf - 1);
        RTRandAdvBytes(pThis->hRand, &pInputMutated->abInput[offByte], 1);
        if (pbBuf[offByte] != pInputMutated->abInput[offByte])
        {
            RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
            *ppInputMutated = pInputMutated;
        }
        else
            RTMemFree(pInputMutated);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Mutator callback - inserts a byte sequence into the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsert(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    if (cbBuf < pThis->cbInputMax)
    {
        size_t cbInputMutated = RTRandAdvU32Ex(pThis->hRand, (uint32_t)cbBuf + 1, (uint32_t)pThis->cbInputMax);
        size_t cbInsert = cbInputMutated - cbBuf;
        uint32_t offInsert = RTRandAdvU32Ex(pThis->hRand, 0, (uint32_t)cbBuf);
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputCreate(pThis, cbInputMutated);
        if (RT_LIKELY(pInputMutated))
        {
            if (offInsert)
                memcpy(&pInputMutated->abInput[0], pbBuf, offInsert);
            RTRandAdvBytes(pThis->hRand, &pInputMutated->abInput[offInsert], cbInsert);
            memcpy(&pInputMutated->abInput[offInsert + cbInsert], &pbBuf[offInsert], cbBuf - offInsert);
            RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
            *ppInputMutated = pInputMutated;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * Mutator callback - appends a byte sequence to the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceAppend(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    if (cbBuf < pThis->cbInputMax)
    {
        size_t cbInputMutated = RTRandAdvU32Ex(pThis->hRand, (uint32_t)cbBuf + 1, (uint32_t)pThis->cbInputMax);
        size_t cbInsert = cbInputMutated - cbBuf;
        PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputCreate(pThis, cbInputMutated);
        if (RT_LIKELY(pInputMutated))
        {
            memcpy(&pInputMutated->abInput[0], pvBuf, cbBuf);
            RTRandAdvBytes(pThis->hRand, &pInputMutated->abInput[cbBuf], cbInsert);
            RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
            *ppInputMutated = pInputMutated;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * Mutator callback - deletes a single byte in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteDelete(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    if (cbBuf > 1)
    {
        uint32_t offDelete = RTRandAdvU32Ex(pThis->hRand, 0, (uint32_t)cbBuf - 1);
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputCreate(pThis, cbBuf - 1);
        if (RT_LIKELY(pInputMutated))
        {
            if (offDelete)
                memcpy(&pInputMutated->abInput[0], pbBuf, offDelete);
            if (offDelete < cbBuf - 1)
                memcpy(&pInputMutated->abInput[offDelete], &pbBuf[offDelete + 1], cbBuf - offDelete - 1);
            RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
            *ppInputMutated = pInputMutated;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * Mutator callback - deletes a byte sequence in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDelete(PRTFUZZCTXINT pThis, const void *pvBuf, size_t cbBuf, PPRTFUZZINPUTINT ppInputMutated)
{
    int rc = VINF_SUCCESS;
    if (cbBuf > 1)
    {
        size_t cbInputMutated = RTRandAdvU32Ex(pThis->hRand, 0, (uint32_t)cbBuf - 1);
        size_t cbDel = cbBuf - cbInputMutated;
        uint32_t offDel = RTRandAdvU32Ex(pThis->hRand, 0, (uint32_t)(cbBuf - cbDel));
        uint8_t *pbBuf = (uint8_t *)pvBuf;

        PRTFUZZINPUTINT pInputMutated = rtFuzzCtxInputCreate(pThis, cbInputMutated);
        if (RT_LIKELY(pInputMutated))
        {
            if (offDel)
                memcpy(&pInputMutated->abInput[0], pbBuf, offDel);
            memcpy(&pInputMutated->abInput[offDel], &pbBuf[offDel + cbDel], cbBuf - offDel - cbDel);
            RTMd5(&pInputMutated->abInput[0], pInputMutated->cbInput, &pInputMutated->abMd5Hash[0]);
            *ppInputMutated = pInputMutated;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


static PRTFUZZCTXINT rtFuzzCtxCreateEmpty(void)
{
    PRTFUZZCTXINT pThis = (PRTFUZZCTXINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->u32Magic         = RTFUZZCTX_MAGIC;
        pThis->cRefs            = 1;
        pThis->TreeSeedsHigh    = NULL;
        pThis->cbInputMax       = UINT32_MAX;
        pThis->cInputs          = 0;
        pThis->fFlagsBehavioral = 0;
        RTListInit(&pThis->LstInputs);

        int rc = RTRandAdvCreateParkMiller(&pThis->hRand);
        if (RT_SUCCESS(rc))
        {
            RTRandAdvSeed(pThis->hRand, RTTimeSystemNanoTS());
            return pThis;
        }

        RTMemFree(pThis);
    }

    return NULL;
}


RTDECL(int) RTFuzzCtxCreate(PRTFUZZCTX phFuzzCtx)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PRTFUZZCTXINT pThis = rtFuzzCtxCreateEmpty();
    if (RT_LIKELY(pThis))
    {
        *phFuzzCtx = pThis;
        return VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int) RTFuzzCtxCreateFromState(PRTFUZZCTX phFuzzCtx, const void *pvState, size_t cbState)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvState, VERR_INVALID_POINTER);
    AssertReturn(cbState > 0, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    if (cbState >= sizeof(RTFUZZCTXSTATE))
    {
        RTFUZZCTXSTATE StateImport;

        memcpy(&StateImport, pvState, sizeof(RTFUZZCTXSTATE));
        if (   RT_LE2H_U32(StateImport.u32Magic) == RTFUZZCTX_MAGIC
            && RT_LE2H_U32(StateImport.cbPrng) <= cbState - sizeof(RTFUZZCTXSTATE))
        {
            PRTFUZZCTXINT pThis = rtFuzzCtxCreateEmpty();
            if (RT_LIKELY(pThis))
            {
                pThis->cbInputMax       = (size_t)RT_LE2H_U64(StateImport.cbInputMax);
                pThis->fFlagsBehavioral = RT_LE2H_U32(StateImport.fFlagsBehavioral);

                uint8_t *pbState = (uint8_t *)pvState;
                uint32_t cInputs = RT_LE2H_U32(StateImport.cInputs);
                rc = RTRandAdvRestoreState(pThis->hRand, (const char *)&pbState[sizeof(RTFUZZCTXSTATE)]);
                if (RT_SUCCESS(rc))
                {
                    /* Go through the inputs and add them. */
                    pbState += sizeof(RTFUZZCTXSTATE) + RT_LE2H_U32(StateImport.cbPrng);
                    cbState -= sizeof(RTFUZZCTXSTATE) + RT_LE2H_U32(StateImport.cbPrng);

                    uint32_t idx = 0;
                    while (   idx < cInputs
                           && RT_SUCCESS(rc))
                    {
                        size_t cbInput = 0;
                        if (cbState >= sizeof(uint32_t))
                        {
                            memcpy(&cbInput, pbState, sizeof(uint32_t));
                            cbInput = RT_LE2H_U32(cbInput);
                            pbState += sizeof(uint32_t);
                        }

                        if (   cbInput
                            && cbInput <= cbState)
                        {
                            PRTFUZZINPUTINT pInput = rtFuzzCtxInputCreate(pThis, cbInput);
                            if (RT_LIKELY(pInput))
                            {
                                memcpy(&pInput->abInput[0], pbState, cbInput);
                                RTMd5(&pInput->abInput[0], pInput->cbInput, &pInput->abMd5Hash[0]);
                                rc = rtFuzzCtxInputAdd(pThis, pInput);
                                if (RT_FAILURE(rc))
                                    RTMemFree(pInput);
                                pbState += cbInput;
                            }
                        }
                        else
                            rc = VERR_INVALID_STATE;

                        idx++;
                    }

                    if (RT_SUCCESS(rc))
                    {
                        *phFuzzCtx = pThis;
                        return VINF_SUCCESS;
                    }
                }

                rtFuzzCtxDestroy(pThis);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_INVALID_MAGIC;
    }
    else
        rc = VERR_INVALID_MAGIC;

    return rc;
}


RTDECL(int) RTFuzzCtxCreateFromStateFile(PRTFUZZCTX phFuzzCtx, const char *pszFilename)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    void *pv = NULL;
    size_t cb = 0;
    int rc = RTFileReadAll(pszFilename, &pv, &cb);
    if (RT_SUCCESS(rc))
    {
        rc = RTFuzzCtxCreateFromState(phFuzzCtx, pv, cb);
        RTFileReadAllFree(pv, cb);
    }

    return rc;
}


RTDECL(uint32_t) RTFuzzCtxRetain(RTFUZZCTX hFuzzCtx)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;

    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


RTDECL(uint32_t) RTFuzzCtxRelease(RTFUZZCTX hFuzzCtx)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    if (pThis == NIL_RTFUZZCTX)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtFuzzCtxDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTFuzzCtxStateExport(RTFUZZCTX hFuzzCtx, void **ppvState, size_t *pcbState)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvState, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbState, VERR_INVALID_POINTER);

    char aszPrngExport[_4K]; /* Should be plenty of room here. */
    size_t cbPrng = sizeof(aszPrngExport);
    int rc = RTRandAdvSaveState(pThis->hRand, &aszPrngExport[0], &cbPrng);
    if (RT_SUCCESS(rc))
    {
        RTFUZZCTXSTATE StateExport;

        StateExport.u32Magic         = RT_H2LE_U32(RTFUZZCTX_MAGIC);
        StateExport.cbPrng           = RT_H2LE_U32((uint32_t)cbPrng);
        StateExport.cInputs          = RT_H2LE_U32(pThis->cInputs);
        StateExport.fFlagsBehavioral = RT_H2LE_U32(pThis->fFlagsBehavioral);
        StateExport.cbInputMax       = RT_H2LE_U64(pThis->cbInputMax);

        /* Estimate the size of the required state. */
        size_t cbState =   sizeof(StateExport)
                         + cbPrng
                         + pThis->cInputs * ((pThis->cbInputMax < _1M ? pThis->cbInputMax : _64K) + sizeof(uint32_t)); /* For the size indicator before each input. */
        uint8_t *pbState = (uint8_t *)RTMemAllocZ(cbState);
        if (RT_LIKELY(pbState))
        {
            size_t offState = 0;
            memcpy(pbState, &StateExport, sizeof(StateExport));
            offState += sizeof(StateExport);
            memcpy(&pbState[offState], &aszPrngExport[0], cbPrng);
            offState += cbPrng;

            /* Export each input. */
            PRTFUZZINPUTINT pIt;
            RTListForEach(&pThis->LstInputs, pIt, RTFUZZINPUTINT, NdInputs)
            {
                /* Ensure buffer size. */
                if (offState + pIt->cbInput + sizeof(uint32_t) > cbState)
                {
                    uint8_t *pbStateNew = (uint8_t *)RTMemRealloc(pbState, cbState + pIt->cbInput + sizeof(uint32_t));
                    if (RT_LIKELY(pbStateNew))
                    {
                        pbState = pbStateNew;
                        cbState += pIt->cbInput + sizeof(uint32_t);
                    }
                    else
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                }

                *(uint32_t *)&pbState[offState] = RT_H2LE_U32((uint32_t)pIt->cbInput);
                offState += sizeof(uint32_t);
                memcpy(&pbState[offState], &pIt->abInput[0], pIt->cbInput);
                offState += pIt->cbInput;
            }

            if (RT_SUCCESS(rc))
            {
                *ppvState = pbState;
                *pcbState = offState;
            }
            else
                RTMemFree(pbState);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


RTDECL(int) RTFuzzCtxStateExportToFile(RTFUZZCTX hFuzzCtx, const char *pszFilename)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    void *pvState = NULL;
    size_t cbState = 0;
    int rc = RTFuzzCtxStateExport(hFuzzCtx, &pvState, &cbState);
    if (RT_SUCCESS(rc))
    {
        RTFILE hFile;

        rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileWrite(hFile, pvState, cbState, NULL);
            RTFileClose(hFile);
            if (RT_FAILURE(rc))
                RTFileDelete(pszFilename);
        }

        RTMemFree(pvState);
    }

    return rc;
}


RTDECL(int) RTFuzzCtxCorpusInputAdd(RTFUZZCTX hFuzzCtx, const void *pvInput, size_t cbInput)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pvInput, VERR_INVALID_POINTER);
    AssertReturn(cbInput, VERR_INVALID_POINTER);

    /* Generate MD5 checksum and try to locate input. */
    int rc = VINF_SUCCESS;
    uint8_t abDigest[RTMD5_HASH_SIZE];
    RTMd5(pvInput, cbInput, &abDigest[0]);

    PRTFUZZINPUTINT pInput = rtFuzzCtxInputLocate(pThis, &abDigest[0], true /*fExact*/, NULL /*ppIntermediate*/);
    if (!pInput)
    {
        pInput = (PRTFUZZINPUTINT)RTMemAllocZ(RT_UOFFSETOF_DYN(RTFUZZINPUTINT, abInput[cbInput]));
        if (RT_LIKELY(pInput))
        {
            pInput->cRefs   = 1;
            pInput->pFuzzer = pThis;
            pInput->cbInput = cbInput;
            memcpy(&pInput->abInput[0], pvInput, cbInput);
            memcpy(&pInput->abMd5Hash[0], &abDigest[0], sizeof(abDigest));
            rc = rtFuzzCtxInputAdd(pThis, pInput);
            if (RT_FAILURE(rc))
                RTMemFree(pInput);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_ALREADY_EXISTS;

    return rc;
}


RTDECL(int) RTFuzzCtxCorpusInputAddFromFile(RTFUZZCTX hFuzzCtx, const char *pszFilename)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    void *pv = NULL;
    size_t cb = 0;
    int rc = RTFileReadAll(pszFilename, &pv, &cb);
    if (RT_SUCCESS(rc))
    {
        rc = RTFuzzCtxCorpusInputAdd(hFuzzCtx, pv, cb);
        RTFileReadAllFree(pv, cb);
    }

    return rc;
}


RTDECL(int) RTFuzzCtxCorpusInputAddFromVfsFile(RTFUZZCTX hFuzzCtx, RTVFSFILE hVfsFile)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(hVfsFile != NIL_RTVFSFILE, VERR_INVALID_HANDLE);

    uint64_t cbFile = 0;
    int rc = RTVfsFileGetSize(hVfsFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        PRTFUZZINPUTINT pInput = (PRTFUZZINPUTINT)RTMemAllocZ(RT_UOFFSETOF_DYN(RTFUZZINPUTINT, abInput[cbFile]));
        if (RT_LIKELY(pInput))
        {
            pInput->cRefs   = 1;
            pInput->pFuzzer = pThis;
            pInput->cbInput = cbFile;

            rc = RTVfsFileRead(hVfsFile, &pInput->abInput[0], cbFile, NULL);
            if (RT_SUCCESS(rc))
            {
                /* Generate MD5 checksum and try to locate input. */
                uint8_t abDigest[RTMD5_HASH_SIZE];
                RTMd5(&pInput->abInput[0], cbFile, &abDigest[0]);

                if (!rtFuzzCtxInputLocate(pThis, &abDigest[0], true /*fExact*/, NULL /*ppIntermediate*/))
                {
                    memcpy(&pInput->abMd5Hash[0], &abDigest[0], sizeof(abDigest));
                    rc = rtFuzzCtxInputAdd(pThis, pInput);
                }
                else
                    rc = VERR_ALREADY_EXISTS;
            }

            if (RT_FAILURE(rc))
                RTMemFree(pInput);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


RTDECL(int) RTFuzzCtxCorpusInputAddFromDirPath(RTFUZZCTX hFuzzCtx, const char *pszDirPath)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDirPath, VERR_INVALID_POINTER);

    RTDIR hDir;
    int rc = RTDirOpen(&hDir, pszDirPath);
    if (RT_SUCCESS(rc))
    {
        for (;;)
        {
            RTDIRENTRY DirEntry;
            rc = RTDirRead(hDir, &DirEntry, NULL);
            if (RT_FAILURE(rc))
                break;

            /* Skip '.', '..' and other non-files. */
            if (   DirEntry.enmType != RTDIRENTRYTYPE_UNKNOWN
                && DirEntry.enmType != RTDIRENTRYTYPE_FILE)
                continue;
            if (RTDirEntryIsStdDotLink(&DirEntry))
                continue;

            /* Compose the full path, result 'unknown' entries and skip non-files. */
            char szFile[RTPATH_MAX];
            RT_ZERO(szFile);
            rc = RTPathJoin(szFile, sizeof(szFile), pszDirPath, DirEntry.szName);
            if (RT_FAILURE(rc))
                break;

            if (DirEntry.enmType == RTDIRENTRYTYPE_UNKNOWN)
            {
                RTDirQueryUnknownType(szFile, false, &DirEntry.enmType);
                if (DirEntry.enmType != RTDIRENTRYTYPE_FILE)
                    continue;
            }

            /* Okay, it's a file we can add. */
            rc = RTFuzzCtxCorpusInputAddFromFile(hFuzzCtx, szFile);
            if (RT_FAILURE(rc))
                break;
        }
        if (rc == VERR_NO_MORE_FILES)
            rc = VINF_SUCCESS;
        RTDirClose(hDir);
    }

    return rc;
}


RTDECL(int) RTFuzzCtxCfgSetInputSeedMaximum(RTFUZZCTX hFuzzCtx, size_t cbMax)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    pThis->cbInputMax = cbMax;
    return VINF_SUCCESS;
}


RTDECL(size_t) RTFuzzCtxCfgGetInputSeedMaximum(RTFUZZCTX hFuzzCtx)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, 0);

    return pThis->cbInputMax;
}


RTDECL(int) RTFuzzCtxCfgSetBehavioralFlags(RTFUZZCTX hFuzzCtx, uint32_t fFlags)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!(fFlags & ~RTFUZZCTX_F_BEHAVIORAL_VALID), VERR_INVALID_PARAMETER);

    pThis->fFlagsBehavioral = fFlags;
    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTFuzzCfgGetBehavioralFlags(RTFUZZCTX hFuzzCtx)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, 0);

    return pThis->fFlagsBehavioral;
}


RTDECL(int) RTFuzzCtxCfgSetTmpDirectory(RTFUZZCTX hFuzzCtx, const char *pszPathTmp)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPathTmp, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(const char *) RTFuzzCtxCfgGetTmpDirectory(RTFUZZCTX hFuzzCtx)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, NULL);

    return NULL;
}


RTDECL(int) RTFuzzCtxReseed(RTFUZZCTX hFuzzCtx, uint64_t uSeed)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    RTRandAdvSeed(pThis->hRand, uSeed);
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzCtxInputGenerate(RTFUZZCTX hFuzzCtx, PRTFUZZINPUT phFuzzInput)
{
    int rc = VINF_SUCCESS;
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(phFuzzInput, VERR_INVALID_POINTER);

    uint32_t cTries = 0;
    PRTFUZZINPUTINT pSrc = rtFuzzCtxInputPickRnd(pThis);
    do
    {
        RTFUZZCTXMUTATOR enmMutator = (RTFUZZCTXMUTATOR)RTRandAdvU32Ex(pThis->hRand, 1, RTFUZZCTXMUTATOR_LAST);
        PRTFUZZINPUTINT pInput = NULL;
        rc = g_apfnMutators[enmMutator](pThis, &pSrc->abInput[0], pSrc->cbInput, &pInput);
        if (   RT_SUCCESS(rc)
            && VALID_PTR(pInput))
        {
            if (pThis->fFlagsBehavioral & RTFUZZCTX_F_BEHAVIORAL_ADD_INPUT_AUTOMATICALLY_TO_CORPUS)
                rtFuzzCtxInputAdd(pThis, pInput);
            *phFuzzInput = pInput;
            return rc;
        }
    } while (++cTries <= 50);

    if (RT_SUCCESS(rc))
        rc = VERR_INVALID_STATE;

    return rc;
}


RTDECL(int) RTFuzzCtxMutateBuffer(RTFUZZCTX hFuzzCtx, void *pvBuf, size_t cbBuf, PRTFUZZINPUT phFuzzInput)
{
    int rc = VINF_SUCCESS;
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(phFuzzInput, VERR_INVALID_POINTER);

    uint32_t cTries = 0;
    do
    {
        RTFUZZCTXMUTATOR enmMutator = (RTFUZZCTXMUTATOR)RTRandAdvU32Ex(pThis->hRand, 1, RTFUZZCTXMUTATOR_LAST);
        PRTFUZZINPUTINT pInput = NULL;
        rc = g_apfnMutators[enmMutator](pThis, pvBuf, cbBuf, &pInput);
        if (   RT_SUCCESS(rc)
            && VALID_PTR(pInput))
        {
            *phFuzzInput = pInput;
            return rc;
        }
    } while (++cTries <= 50);

    if (RT_SUCCESS(rc))
        rc = VERR_INVALID_STATE;

    return rc;
}


RTDECL(uint32_t) RTFuzzInputRetain(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;

    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


RTDECL(uint32_t) RTFuzzInputRelease(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    if (pThis == NIL_RTFUZZINPUT)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtFuzzInputDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTFuzzInputQueryData(RTFUZZINPUT hFuzzInput, void **ppv, size_t *pcb)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);

    *ppv = &pThis->abInput[0];
    *pcb = pThis->cbInput;
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzInputQueryDigestString(RTFUZZINPUT hFuzzInput, char *pszDigest, size_t cchDigest)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszDigest, VERR_INVALID_POINTER);
    AssertReturn(cchDigest >= RTMD5_STRING_LEN + 1, VERR_INVALID_PARAMETER);

    return RTMd5ToString(&pThis->abMd5Hash[0], pszDigest, cchDigest);
}


RTDECL(int) RTFuzzInputWriteToFile(RTFUZZINPUT hFuzzInput, const char *pszFilename)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(hFile, &pThis->abInput[0], pThis->cbInput, NULL);
        AssertRC(rc);
        RTFileClose(hFile);

        if (RT_FAILURE(rc))
            RTFileDelete(pszFilename);
    }

    return rc;
}


RTDECL(int) RTFuzzInputAddToCtxCorpus(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtFuzzCtxInputAdd(pThis->pFuzzer, pThis);
}


RTDECL(int) RTFuzzInputRemoveFromCtxCorpus(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    PRTFUZZINTERMEDIATE pIntermediate = NULL;
    PRTFUZZINPUTINT pInputLoc = rtFuzzCtxInputLocate(pThis->pFuzzer, &pThis->abMd5Hash[0], true /*fExact*/,
                                                     &pIntermediate);
    if (pInputLoc)
    {
        AssertPtr(pIntermediate);
        Assert(pInputLoc == pThis);

        uint64_t u64Md5Low = *(uint64_t *)&pThis->abMd5Hash[0];
        RTAvlU64Remove(&pIntermediate->TreeSeedsLow, u64Md5Low);
        RTFuzzInputRelease(hFuzzInput);
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

