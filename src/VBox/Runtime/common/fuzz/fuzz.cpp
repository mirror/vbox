/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, core.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
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
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/vfs.h>


#define RTFUZZCTX_MAGIC UINT32_C(0xdeadc0de) /** @todo */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the internal fuzzer state. */
typedef struct RTFUZZCTXINT *PRTFUZZCTXINT;
/** Pointer to a fuzzed mutation. */
typedef struct RTFUZZMUTATION *PRTFUZZMUTATION;
/** Pointer to a fuzzed mutation pointer. */
typedef PRTFUZZMUTATION *PPRTFUZZMUTATION;
/** Pointer to a const mutation. */
typedef const struct RTFUZZMUTATION *PCRTFUZZMUTATION;


/**
 * Mutator preparation callback.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   offStart            Where the mutation should start.
 * @param   pMutationParent     The parent mutation to start working from.
 * @param   ppMutation          Where to store the created mutation on success.
 */
typedef DECLCALLBACK(int) FNRTFUZZCTXMUTATORPREP(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                 PPRTFUZZMUTATION ppMutation);
/** Pointer to a mutator preparation callback. */
typedef FNRTFUZZCTXMUTATORPREP *PFNRTFUZZCTXMUTATORPREP;


/**
 * Mutator execution callback.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pMutation           The mutation to work on.
 * @param   pvMutation          Mutation dependent data.
 * @param   pbBuf               The buffer to work on.
 * @param   cbBuf               Size of the remaining buffer.
 */
typedef DECLCALLBACK(int) FNRTFUZZCTXMUTATOREXEC(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                 uint8_t *pbBuf, size_t cbBuf);
/** Pointer to a mutator execution callback. */
typedef FNRTFUZZCTXMUTATOREXEC *PFNRTFUZZCTXMUTATOREXEC;


/**
 * Mutator export callback.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pMutation           The mutation to work on.
 * @param   pvMutation          Mutation dependent data.
 * @param   pfnExport           The export callback.
 * @param   pvUser              Opaque user data to pass to the export callback.
 */
typedef DECLCALLBACK(int) FNRTFUZZCTXMUTATOREXPORT(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                   PFNRTFUZZCTXEXPORT pfnExport, void *pvUser);
/** Pointer to a mutator export callback. */
typedef FNRTFUZZCTXMUTATOREXPORT *PFNRTFUZZCTXMUTATOREXPORT;


/**
 * Mutator import callback.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pMutation           The mutation to work on.
 * @param   pvMutation          Mutation dependent data.
 * @param   pfnExport           The import callback.
 * @param   pvUser              Opaque user data to pass to the import callback.
 */
typedef DECLCALLBACK(int) FNRTFUZZCTXMUTATORIMPORT(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, void *pvMutation,
                                                   PFNRTFUZZCTXIMPORT pfnImport, void *pvUser);
/** Pointer to a mutator import callback. */
typedef FNRTFUZZCTXMUTATORIMPORT *PFNRTFUZZCTXMUTATORIMPORT;


/**
 * A fuzzing mutator descriptor.
 */
typedef struct RTFUZZMUTATOR
{
    /** Id of the mutator. */
    const char                  *pszId;
    /** Mutator description. */
    const char                  *pszDesc;
    /** Mutator index. */
    uint32_t                    uMutator;
    /** Additional flags for the mutator, controlling the behavior. */
    uint64_t                    fFlags;
    /** The preparation callback. */
    PFNRTFUZZCTXMUTATORPREP     pfnPrep;
    /** The execution callback. */
    PFNRTFUZZCTXMUTATOREXEC     pfnExec;
    /** The export callback. */
    PFNRTFUZZCTXMUTATOREXPORT   pfnExport;
    /** The import callback. */
    PFNRTFUZZCTXMUTATORIMPORT   pfnImport;
} RTFUZZMUTATOR;
/** Pointer to a fuzzing mutator descriptor. */
typedef RTFUZZMUTATOR *PRTFUZZMUTATOR;
/** Pointer to a const fuzzing mutator descriptor. */
typedef const RTFUZZMUTATOR *PCRTFUZZMUTATOR;

/** The special corpus mutator. */
#define RTFUZZMUTATOR_ID_CORPUS             UINT32_C(0xffffffff)

/** Mutator always works from the end of the buffer (no starting offset generation). */
#define RTFUZZMUTATOR_F_END_OF_BUF          RT_BIT_64(0)
/** Default flags. */
#define RTFUZZMUTATOR_F_DEFAULT             (0)


/**
 * A fuzzed mutation.
 */
typedef struct RTFUZZMUTATION
{
    /** The AVL tree core. */
    AVLU64NODECORE              Core;
    /** Magic identifying this structure. */
    uint32_t                    u32Magic;
    /** Reference counter. */
    volatile uint32_t           cRefs;
    /** The fuzzer this mutation belongs to. */
    PRTFUZZCTXINT               pFuzzer;
    /** Parent mutation (reference is held), NULL means root or original data. */
    PCRTFUZZMUTATION            pMutationParent;
    /** Mutation level. */
    uint32_t                    iLvl;
    /** The mutator causing this mutation, NULL if original input data. */
    PCRTFUZZMUTATOR             pMutator;
    /** Byte offset where the mutation starts. */
    uint64_t                    offMutation;
    /** Size of the generated input data in bytes after the mutation was applied. */
    size_t                      cbInput;
    /** Size of the mutation dependent data. */
    size_t                      cbMutation;
    /** Mutation dependent data, variable in size. */
    uint8_t                     abMutation[1];
} RTFUZZMUTATION;


/**
 * A fuzzing input seed.
 */
typedef struct RTFUZZINPUTINT
{
    /** Magic identifying this structure. */
    uint32_t                    u32Magic;
    /** Reference counter. */
    volatile uint32_t           cRefs;
    /** The fuzzer this input belongs to. */
    PRTFUZZCTXINT               pFuzzer;
    /** The top mutation to work from (reference held). */
    PRTFUZZMUTATION             pMutationTop;
    /** Fuzzer context type dependent data. */
    union
    {
        /** Blob data. */
        struct
        {
            /** Size to allocate initially. */
            size_t              cbAlloc;
            /** Input data size. */
            size_t              cbInput;
            /** Pointer to the input data if created. */
            void                *pvInput;
        } Blob;
        /** Stream state. */
        struct
        {
            /** Number of bytes seen so far. */
            size_t              cbSeen;
        } Stream;
    } u;
    /** Array holding all the individual mutations, variable in size. */
    PCRTFUZZMUTATION            apMutations[1];
} RTFUZZINPUTINT;
/** Pointer to the internal input state. */
typedef RTFUZZINPUTINT *PRTFUZZINPUTINT;
/** Pointer to an internal input state pointer. */
typedef PRTFUZZINPUTINT *PPRTFUZZINPUTINT;


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
    /** Fuzzing context type. */
    RTFUZZCTXTYPE               enmType;
    /** Semaphore protecting the mutations tree. */
    RTSEMRW                     hSemRwMutations;
    /** The AVL tree for indexing the mutations (keyed by counter). */
    AVLU64TREE                  TreeMutations;
    /** Number of inputs currently in the tree. */
    volatile uint64_t           cMutations;
    /** The maximum size of one input seed to generate. */
    size_t                      cbInputMax;
    /** Behavioral flags. */
    uint32_t                    fFlagsBehavioral;
    /** Number of enabled mutators. */
    uint32_t                    cMutators;
    /** Pointer to the mutator descriptors. */
    PRTFUZZMUTATOR              paMutators;
    /** Total number of bytes of memory currently allocated in total for this context. */
    volatile size_t             cbMemTotal;
} RTFUZZCTXINT;


/**
 * The fuzzer state to be exported - all members are stored in little endian form.
 */
typedef struct RTFUZZCTXSTATE
{
    /** Magic value for identification. */
    uint32_t                    u32Magic;
    /** Context type. */
    uint32_t                    uCtxType;
    /** Size of the PRNG state following in bytes. */
    uint32_t                    cbPrng;
    /** Number of mutator descriptors following. */
    uint32_t                    cMutators;
    /** Number of mutation descriptors following. */
    uint32_t                    cMutations;
    /** Behavioral flags. */
    uint32_t                    fFlagsBehavioral;
    /** Maximum input size to generate. */
    uint64_t                    cbInputMax;
} RTFUZZCTXSTATE;
/** Pointer to a fuzzing context state. */
typedef RTFUZZCTXSTATE *PRTFUZZCTXSTATE;

/** BLOB context type. */
#define RTFUZZCTX_STATE_TYPE_BLOB         UINT32_C(0)
/** Stream context type. */
#define RTFUZZCTX_STATE_TYPE_STREAM       UINT32_C(1)


/**
 * The fuzzer mutation state to be exported - all members are stored in little endian form.
 */
typedef struct RTFUZZMUTATIONSTATE
{
    /** The mutation identifier. */
    uint64_t                    u64Id;
    /** The mutation identifier of the parent, 0 for no parent. */
    uint64_t                    u64IdParent;
    /** The byte offset where the mutation starts. */
    uint64_t                    u64OffMutation;
    /** Size of input data after mutation was applied. */
    uint64_t                    cbInput;
    /** Size of mutation dependent data following. */
    uint64_t                    cbMutation;
    /** The mutator ID. */
    uint32_t                    u32IdMutator;
    /** The mutation level. */
    uint32_t                    iLvl;
    /** Magic value for identification. */
    uint32_t                    u32Magic;
} RTFUZZMUTATIONSTATE;


/**
 * Fuzzing context memory header.
 */
typedef struct RTFUZZMEMHDR
{
    /** Size of the memory area following. */
    size_t                      cb;
#if HC_ARCH_BITS == 32
    /** Some padding. */
    uint32_t                    uPadding0;
#elif HC_ARCH_BITS == 64
#else
    /** Some padding. */
    uint64_t                    uPadding0;
# error "Port me"
#endif
} RTFUZZMEMHDR;
/** Pointer to a memory header. */
typedef RTFUZZMEMHDR *PRTFUZZMEMHDR;


/**
 * Fuzzing context export AVL arguments.
 */
typedef struct RTFUZZEXPORTARGS
{
    /** Pointer to the export callback. */
    PFNRTFUZZCTXEXPORT pfnExport;
    /** Opaque user data to pass to the callback. */
    void               *pvUser;
} RTFUZZEXPORTARGS;
/** Pointer to the export arguments. */
typedef RTFUZZEXPORTARGS *PRTFUZZEXPORTARGS;
/** Pointer to the constant export arguments. */
typedef const RTFUZZEXPORTARGS *PCRTFUZZEXPORTARGS;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlipPrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                     PPRTFUZZMUTATION ppMutation);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplacePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                         PPRTFUZZMUTATION ppMutation);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsertAppendPrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                                      PPRTFUZZMUTATION ppMutation);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteDeletePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                        PPRTFUZZMUTATION ppMutation);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDeletePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                                PPRTFUZZMUTATION ppMutation);

static DECLCALLBACK(int) rtFuzzCtxMutatorCorpusExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                    uint8_t *pbBuf, size_t cbBuf);
static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlipExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                     uint8_t *pbBuf, size_t cbBuf);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplaceExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                         uint8_t *pbBuf, size_t cbBuf);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsertAppendExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                                      uint8_t *pbBuf, size_t cbBuf);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteDeleteExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                        uint8_t *pbBuf, size_t cbBuf);
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDeleteExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                                uint8_t *pbBuf, size_t cbBuf);

static DECLCALLBACK(int) rtFuzzCtxMutatorExportDefault(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                       PFNRTFUZZCTXEXPORT pfnExport, void *pvUser);
static DECLCALLBACK(int) rtFuzzCtxMutatorImportDefault(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, void *pvMutation,
                                                      PFNRTFUZZCTXIMPORT pfnImport, void *pvUser);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * The special corpus mutator for the original data.
 */
static RTFUZZMUTATOR const g_MutatorCorpus =
{
    /** pszId */
    "Corpus",
    /** pszDesc */
    "Special mutator, which is assigned to the initial corpus",
    /** uMutator. */
    RTFUZZMUTATOR_ID_CORPUS,
    /** fFlags */
    RTFUZZMUTATOR_F_DEFAULT,
    /** pfnPrep */
    NULL,
    /** pfnExec */
    rtFuzzCtxMutatorCorpusExec,
    /** pfnExport */
    rtFuzzCtxMutatorExportDefault,
    /** pfnImport */
    rtFuzzCtxMutatorImportDefault
};

/**
 * Array of all available mutators.
 */
static RTFUZZMUTATOR const g_aMutators[] =
{
    /* pszId         pszDesc                                          uMutator     fFlags                      pfnPrep                                       pfnExec                                       pfnExport                                       pfnImport */
    { "BitFlip",     "Flips a single bit in the input",               0,           RTFUZZMUTATOR_F_DEFAULT,    rtFuzzCtxMutatorBitFlipPrep,                  rtFuzzCtxMutatorBitFlipExec,                  rtFuzzCtxMutatorExportDefault, rtFuzzCtxMutatorImportDefault },
    { "ByteReplace", "Replaces a single byte in the input",           1,           RTFUZZMUTATOR_F_DEFAULT,    rtFuzzCtxMutatorByteReplacePrep,              rtFuzzCtxMutatorByteReplaceExec,              rtFuzzCtxMutatorExportDefault, rtFuzzCtxMutatorImportDefault },
    { "ByteSeqIns",  "Inserts a byte sequence in the input",          2,           RTFUZZMUTATOR_F_DEFAULT,    rtFuzzCtxMutatorByteSequenceInsertAppendPrep, rtFuzzCtxMutatorByteSequenceInsertAppendExec, rtFuzzCtxMutatorExportDefault, rtFuzzCtxMutatorImportDefault },
    { "ByteSeqApp",  "Appends a byte sequence to the input",          3,           RTFUZZMUTATOR_F_END_OF_BUF, rtFuzzCtxMutatorByteSequenceInsertAppendPrep, rtFuzzCtxMutatorByteSequenceInsertAppendExec, rtFuzzCtxMutatorExportDefault, rtFuzzCtxMutatorImportDefault },
    { "ByteDelete",  "Deletes a single byte sequence from the input", 4,           RTFUZZMUTATOR_F_DEFAULT,    rtFuzzCtxMutatorByteDeletePrep,               rtFuzzCtxMutatorByteDeleteExec,               NULL,                          NULL                          },
    { "ByteSeqDel",  "Deletes a byte sequence from the input",        5,           RTFUZZMUTATOR_F_DEFAULT,    rtFuzzCtxMutatorByteSequenceDeletePrep,       rtFuzzCtxMutatorByteSequenceDeleteExec,       NULL,                          NULL                          }
};


/**
 * Allocates the given number of bytes.
 *
 * @returns Pointer to the allocated memory
 * @param   pThis               The fuzzer context instance.
 * @param   cb                  How much to allocate.
 */
static void *rtFuzzCtxMemoryAlloc(PRTFUZZCTXINT pThis, size_t cb)
{
    PRTFUZZMEMHDR pMemHdr = (PRTFUZZMEMHDR)RTMemAllocZ(cb + sizeof(RTFUZZMEMHDR));
    if (RT_LIKELY(pMemHdr))
    {
        pMemHdr->cb = cb;
        size_t cbIgn = ASMAtomicAddZ(&pThis->cbMemTotal, cb + sizeof(RTFUZZMEMHDR)); RT_NOREF(cbIgn);
        return pMemHdr + 1;
    }

    return NULL;
}


/**
 * Frees the given memory.
 *
 * @returns nothing.
 * @param   pThis               The fuzzer context instance.
 * @param   pv                  Pointer to the memory area to free.
 */
static void rtFuzzCtxMemoryFree(PRTFUZZCTXINT pThis, void *pv)
{
    PRTFUZZMEMHDR pMemHdr = ((PRTFUZZMEMHDR)pv) - 1;

    size_t cbIgn = ASMAtomicSubZ(&pThis->cbMemTotal, pMemHdr->cb + sizeof(RTFUZZMEMHDR)); RT_NOREF(cbIgn);
    RTMemFree(pMemHdr);
}


/**
 * Retains an external reference to the given mutation.
 *
 * @returns New reference count on success.
 * @param   pMutation           The mutation to retain.
 */
static uint32_t rtFuzzMutationRetain(PRTFUZZMUTATION pMutation)
{
    uint32_t cRefs = ASMAtomicIncU32(&pMutation->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pMutation));
    return cRefs;
}


#if 0 /* unused */
/**
 * Releases an external reference from the given mutation.
 *
 * @returns New reference count on success.
 * @param   pMutation           The mutation to retain.
 */
static uint32_t rtFuzzMutationRelease(PRTFUZZMUTATION pMutation)
{
    uint32_t cRefs = ASMAtomicDecU32(&pMutation->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pMutation));
    return cRefs;
}
#endif


/**
 * Adds the given mutation to the corpus of the given fuzzer context.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzer context instance.
 * @param   pMutation           The mutation to add.
 */
static int rtFuzzCtxMutationAdd(PRTFUZZCTXINT pThis, PRTFUZZMUTATION pMutation)
{
    int rc = VINF_SUCCESS;

    pMutation->Core.Key = ASMAtomicIncU64(&pThis->cMutations);
    rc = RTSemRWRequestWrite(pThis->hSemRwMutations, RT_INDEFINITE_WAIT);
    AssertRC(rc); RT_NOREF(rc);
    bool fIns = RTAvlU64Insert(&pThis->TreeMutations, &pMutation->Core);
    Assert(fIns); RT_NOREF(fIns);
    rc = RTSemRWReleaseWrite(pThis->hSemRwMutations);
    AssertRC(rc); RT_NOREF(rc);

    return rc;
}


/**
 * Returns a random mutation from the corpus of the given fuzzer context.
 *
 * @returns Pointer to a randomly picked mutation (reference count is increased).
 * @param   pThis               The fuzzer context instance.
 */
static PRTFUZZMUTATION rtFuzzCtxMutationPickRnd(PRTFUZZCTXINT pThis)
{
    uint64_t idxMutation = RTRandAdvU64Ex(pThis->hRand, 0, ASMAtomicReadU64(&pThis->cMutations));

    int rc = RTSemRWRequestRead(pThis->hSemRwMutations, RT_INDEFINITE_WAIT);
    AssertRC(rc); RT_NOREF(rc);

    /*
     * Using best fit getter here as there might be a racing mutation insertion and the mutation counter has increased
     * already but the mutation is not yet in the tree.
     */
    PRTFUZZMUTATION pMutation = (PRTFUZZMUTATION)RTAvlU64GetBestFit(&pThis->TreeMutations, idxMutation, false /*fAbove*/);
    AssertPtr(pMutation);

    /* Increase reference count of the mutation. */
    rtFuzzMutationRetain(pMutation);
    rc = RTSemRWReleaseRead(pThis->hSemRwMutations);
    AssertRC(rc); RT_NOREF(rc);

    return pMutation;
}


/**
 * Creates a new mutation capable of holding the additional number of bytes.
 *
 * @returns Pointer to the newly created mutation or NULL if out of memory.
 * @param   pThis               The fuzzer context instance.
 * @param   offMutation         The starting offset for the mutation.
 * @param   pMutationParent     The parent mutation, can be NULL.
 * @param   cbAdditional        Additional number of bytes to allocate after the core structure.
 * @param   ppvMutation         Where to store the pointer to the mutation dependent data on success.
 */
static PRTFUZZMUTATION rtFuzzMutationCreate(PRTFUZZCTXINT pThis, uint64_t offMutation, PRTFUZZMUTATION pMutationParent,
                                            size_t cbAdditional, void **ppvMutation)
{
    PRTFUZZMUTATION pMutation = (PRTFUZZMUTATION)rtFuzzCtxMemoryAlloc(pThis, sizeof(RTFUZZMUTATION) + cbAdditional);
    if (RT_LIKELY(pMutation))
    {
        pMutation->u32Magic        = 0; /** @todo */
        pMutation->pFuzzer         = pThis;
        pMutation->cRefs           = 1;
        pMutation->iLvl            = 0;
        pMutation->offMutation     = offMutation;
        pMutation->pMutationParent = pMutationParent;
        pMutation->cbMutation      = cbAdditional;

        if (pMutationParent)
            pMutation->iLvl = pMutationParent->iLvl + 1;
        if (ppvMutation)
            *ppvMutation = &pMutation->abMutation[0];
    }

    return pMutation;
}


/**
 * Destroys the given mutation.
 *
 * @returns nothing.
 * @param   pMutation           The mutation to destroy.
 */
static void rtFuzzMutationDestroy(PRTFUZZMUTATION pMutation)
{
    rtFuzzCtxMemoryFree(pMutation->pFuzzer, pMutation);
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
 * Default mutator export callback (just writing the raw data).
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorExportDefault(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                       PFNRTFUZZCTXEXPORT pfnExport, void *pvUser)
{
    return pfnExport(pThis, pvMutation, pMutation->cbMutation, pvUser);
}


/**
 * Default mutator import callback (just reading the raw data).
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorImportDefault(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, void *pvMutation,
                                                       PFNRTFUZZCTXIMPORT pfnImport, void *pvUser)
{
    return pfnImport(pThis, pvMutation, pMutation->cbMutation, NULL, pvUser);
}


static DECLCALLBACK(int) rtFuzzCtxMutatorCorpusExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                    uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis, cbBuf, pvMutation);
    memcpy(pbBuf, pvMutation, pMutation->cbInput);
    return VINF_SUCCESS;
}


/**
 * Mutator callback - flips a single bit in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlipPrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                     PPRTFUZZMUTATION ppMutation)
{
    int rc = VINF_SUCCESS;
    uint8_t *pidxBitFlip = 0;
    PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, offStart, pMutationParent, sizeof(*pidxBitFlip), (void **)&pidxBitFlip);
    if (RT_LIKELY(pMutation))
    {
        pMutation->cbInput = pMutationParent->cbInput; /* Bit flips don't change the input size. */
        *pidxBitFlip = (uint8_t)RTRandAdvU32Ex(pThis->hRand, 0, sizeof(uint8_t) * 8 - 1);
        *ppMutation = pMutation;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


static DECLCALLBACK(int) rtFuzzCtxMutatorBitFlipExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                     uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis, cbBuf, pMutation);
    uint8_t idxBitFlip = *(uint8_t *)pvMutation;
    ASMBitToggle(pbBuf, idxBitFlip);
    return VINF_SUCCESS;
}


/**
 * Mutator callback - replaces a single byte in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplacePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                         PPRTFUZZMUTATION ppMutation)
{
    int rc = VINF_SUCCESS;
    uint8_t *pbReplace = 0;
    PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, offStart, pMutationParent, sizeof(*pbReplace), (void **)&pbReplace);
    if (RT_LIKELY(pMutation))
    {
        pMutation->cbInput = pMutationParent->cbInput; /* Byte replacements don't change the input size. */
        RTRandAdvBytes(pThis->hRand, pbReplace, 1); /** @todo Filter out same values. */
        *ppMutation = pMutation;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


static DECLCALLBACK(int) rtFuzzCtxMutatorByteReplaceExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                         uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis, cbBuf, pMutation);
    uint8_t bReplace = *(uint8_t *)pvMutation;
    *pbBuf = bReplace;
    return VINF_SUCCESS;
}


/**
 * Mutator callback - inserts a byte sequence into the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsertAppendPrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                                      PPRTFUZZMUTATION ppMutation)
{
    int rc = VINF_SUCCESS;
    if (pMutationParent->cbInput < pThis->cbInputMax)
    {
        size_t cbInputMutated = (size_t)RTRandAdvU64Ex(pThis->hRand, pMutationParent->cbInput + 1, pThis->cbInputMax);
        size_t cbInsert = cbInputMutated - pMutationParent->cbInput;
        uint8_t *pbAdd = NULL;

        PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, offStart, pMutationParent, cbInsert, (void **)&pbAdd);
        if (RT_LIKELY(pMutation))
        {
            pMutation->cbInput = cbInputMutated;
            RTRandAdvBytes(pThis->hRand, pbAdd, cbInsert);
            *ppMutation = pMutation;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceInsertAppendExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                                      uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis);
    size_t cbInsert = pMutation->cbInput - pMutation->pMutationParent->cbInput;

    /* Move any remaining data to the end. */
    if (cbBuf)
        memmove(pbBuf + cbInsert, pbBuf, cbBuf);

    memcpy(pbBuf, pvMutation, cbInsert);
    return VINF_SUCCESS;
}


/**
 * Mutator callback - deletes a single byte in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteDeletePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                        PPRTFUZZMUTATION ppMutation)
{
    int rc = VINF_SUCCESS;
    if (pMutationParent->cbInput - offStart >= 1)
    {
        PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, offStart, pMutationParent, 0 /*cbAdditional*/, NULL);
        if (RT_LIKELY(pMutation))
        {
            pMutation->cbInput = pMutationParent->cbInput - 1;
            *ppMutation = pMutation;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


static DECLCALLBACK(int) rtFuzzCtxMutatorByteDeleteExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                        uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis, pMutation, pvMutation);

    /* Just move the residual data to the front. */
    memmove(pbBuf, pbBuf + 1, cbBuf - 1);
    return VINF_SUCCESS;
}



/**
 * Mutator callback - deletes a byte sequence in the input.
 */
static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDeletePrep(PRTFUZZCTXINT pThis, uint64_t offStart, PRTFUZZMUTATION pMutationParent,
                                                                PPRTFUZZMUTATION ppMutation)
{
    int rc = VINF_SUCCESS;
    if (pMutationParent->cbInput > 1)
    {
        size_t cbInputMutated = (size_t)RTRandAdvU64Ex(pThis->hRand, offStart, pMutationParent->cbInput - 1);

        PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, offStart, pMutationParent, 0 /*cbAdditional*/, NULL);
        if (RT_LIKELY(pMutation))
        {
            pMutation->cbInput = cbInputMutated;
            *ppMutation = pMutation;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


static DECLCALLBACK(int) rtFuzzCtxMutatorByteSequenceDeleteExec(PRTFUZZCTXINT pThis, PCRTFUZZMUTATION pMutation, const void *pvMutation,
                                                                uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF(pThis, pvMutation);
    Assert(pMutation->pMutationParent->cbInput > pMutation->cbInput);
    size_t cbDel = pMutation->pMutationParent->cbInput - pMutation->cbInput;

    /* Just move the residual data to the front. */
    memmove(pbBuf, pbBuf + cbDel, cbBuf - cbDel);
    return VINF_SUCCESS;
}


/**
 * Creates an empty fuzzing context.
 *
 * @returns IPRT status code.
 * @param   ppThis              Where to store the pointer to the internal fuzzing context instance on success.
 * @param   enmType             Fuzzing context type.
 */
static int rtFuzzCtxCreateEmpty(PRTFUZZCTXINT *ppThis, RTFUZZCTXTYPE enmType)
{
    int rc;
    PRTFUZZCTXINT pThis = (PRTFUZZCTXINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->u32Magic         = RTFUZZCTX_MAGIC;
        pThis->cRefs            = 1;
        pThis->enmType          = enmType;
        pThis->TreeMutations    = NULL;
        pThis->cbInputMax       = UINT32_MAX;
        pThis->cMutations       = 0;
        pThis->fFlagsBehavioral = 0;
        pThis->cbMemTotal       = 0;

        /* Copy the default mutator descriptors over. */
        pThis->paMutators = (PRTFUZZMUTATOR)RTMemAllocZ(RT_ELEMENTS(g_aMutators) * sizeof(RTFUZZMUTATOR));
        if (RT_LIKELY(pThis->paMutators))
        {
            pThis->cMutators = RT_ELEMENTS(g_aMutators);
            memcpy(&pThis->paMutators[0], &g_aMutators[0], RT_ELEMENTS(g_aMutators) * sizeof(RTFUZZMUTATOR));

            rc = RTSemRWCreate(&pThis->hSemRwMutations);
            if (RT_SUCCESS(rc))
            {
                rc = RTRandAdvCreateParkMiller(&pThis->hRand);
                if (RT_SUCCESS(rc))
                {
                    RTRandAdvSeed(pThis->hRand, RTTimeSystemNanoTS());
                    *ppThis = pThis;
                    return VINF_SUCCESS;
                }

                RTSemRWDestroy(pThis->hSemRwMutations);
            }
        }
        else
            rc = VERR_NO_MEMORY;

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Destroys the given fuzzing input.
 *
 * @returns nothing.
 * @param   pThis               The fuzzing input to destroy.
 */
static void rtFuzzInputDestroy(PRTFUZZINPUTINT pThis)
{
    PRTFUZZCTXINT pFuzzer = pThis->pFuzzer;

    if (   pFuzzer->enmType == RTFUZZCTXTYPE_BLOB
        && pThis->u.Blob.pvInput)
        rtFuzzCtxMemoryFree(pFuzzer, pThis->u.Blob.pvInput);

    rtFuzzCtxMemoryFree(pFuzzer, pThis);
    RTFuzzCtxRelease(pFuzzer);
}


/**
 * Creates the final input data applying all accumulated mutations.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing input to finalize.
 */
static int rtFuzzInputDataFinalize(PRTFUZZINPUTINT pThis)
{
    Assert(!pThis->u.Blob.pvInput);

    int rc = VINF_SUCCESS;
    uint8_t *pbBuf = (uint8_t *)rtFuzzCtxMemoryAlloc(pThis->pFuzzer, pThis->u.Blob.cbAlloc);
    if (RT_LIKELY(pbBuf))
    {
        pThis->u.Blob.pvInput = pbBuf;
        pThis->u.Blob.cbInput = pThis->pMutationTop->cbInput;

        size_t cbInputNow = pThis->apMutations[0]->cbInput;
        for (uint32_t i = 0; i < pThis->pMutationTop->iLvl + 1; i++)
        {
            PCRTFUZZMUTATION pMutation = pThis->apMutations[i];
            pMutation->pMutator->pfnExec(pThis->pFuzzer, pMutation, (void *)&pMutation->abMutation[0],
                                         pbBuf + pMutation->offMutation,
                                         cbInputNow - pMutation->offMutation);

            cbInputNow = pMutation->cbInput;
        }

        Assert(cbInputNow == pThis->u.Blob.cbInput);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int) RTFuzzCtxCreate(PRTFUZZCTX phFuzzCtx, RTFUZZCTXTYPE enmType)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);

    return rtFuzzCtxCreateEmpty(phFuzzCtx, enmType);
}


RTDECL(int) RTFuzzCtxCreateFromState(PRTFUZZCTX phFuzzCtx, PFNRTFUZZCTXIMPORT pfnImport, void *pvUser)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnImport, VERR_INVALID_POINTER);

#if 0
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
#else
    RT_NOREF(pvUser);
    return VERR_NOT_IMPLEMENTED;
#endif
}


RTDECL(int) RTFuzzCtxCreateFromStateMem(PRTFUZZCTX phFuzzCtx, const void *pvState, size_t cbState)
{
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvState, VERR_INVALID_POINTER);
    AssertPtrReturn(cbState, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
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
        rc = RTFuzzCtxCreateFromStateMem(phFuzzCtx, pv, cb);
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


/**
 * Fuzzing context export callback for a single mutation.
 */
static DECLCALLBACK(int) rtFuzzCtxStateExportMutations(PAVLU64NODECORE pCore, void *pvParam)
{
    PRTFUZZMUTATION pMutation = (PRTFUZZMUTATION)pCore;
    PCRTFUZZMUTATOR pMutator = pMutation->pMutator;
    PCRTFUZZEXPORTARGS pArgs = (PCRTFUZZEXPORTARGS)pvParam;
    RTFUZZMUTATIONSTATE MutationState;

    MutationState.u64Id           = RT_H2LE_U64(pMutation->Core.Key);
    if (pMutation->pMutationParent)
        MutationState.u64IdParent = RT_H2LE_U64(pMutation->pMutationParent->Core.Key);
    else
        MutationState.u64IdParent = RT_H2LE_U64(0);
    MutationState.u64OffMutation  = RT_H2LE_U64(pMutation->offMutation);
    MutationState.cbInput         = RT_H2LE_U64((uint64_t)pMutation->cbInput);
    MutationState.cbMutation      = RT_H2LE_U64((uint64_t)pMutation->cbMutation);
    MutationState.u32IdMutator    = RT_H2LE_U32(pMutator->uMutator);
    MutationState.iLvl            = RT_H2LE_U32(pMutation->iLvl);
    MutationState.u32Magic        = RT_H2LE_U32(pMutation->u32Magic);

    int rc = pArgs->pfnExport(pMutation->pFuzzer, &MutationState, sizeof(MutationState), pArgs->pvUser);
    if (   RT_SUCCESS(rc)
        && pMutator->pfnExport)
        rc = pMutator->pfnExport(pMutation->pFuzzer, pMutation, &pMutation->abMutation[0], pArgs->pfnExport, pArgs->pvUser);
    return rc;
}


RTDECL(int) RTFuzzCtxStateExport(RTFUZZCTX hFuzzCtx, PFNRTFUZZCTXEXPORT pfnExport, void *pvUser)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnExport, VERR_INVALID_POINTER);

    char aszPrngExport[_4K]; /* Should be plenty of room here. */
    size_t cbPrng = sizeof(aszPrngExport);
    int rc = RTRandAdvSaveState(pThis->hRand, &aszPrngExport[0], &cbPrng);
    if (RT_SUCCESS(rc))
    {
        RTFUZZCTXSTATE StateExport;

        StateExport.u32Magic         = RT_H2LE_U32(RTFUZZCTX_MAGIC);
        switch (pThis->enmType)
        {
            case RTFUZZCTXTYPE_BLOB:
                StateExport.uCtxType = RT_H2LE_U32(RTFUZZCTX_STATE_TYPE_BLOB);
                break;
            case RTFUZZCTXTYPE_STREAM:
                StateExport.uCtxType = RT_H2LE_U32(RTFUZZCTX_STATE_TYPE_STREAM);
                break;
            default:
                AssertFailed();
                break;
        }
        StateExport.cbPrng           = RT_H2LE_U32((uint32_t)cbPrng);
        StateExport.cMutations       = RT_H2LE_U32(pThis->cMutations);
        StateExport.cMutators        = RT_H2LE_U32(pThis->cMutators);
        StateExport.fFlagsBehavioral = RT_H2LE_U32(pThis->fFlagsBehavioral);
        StateExport.cbInputMax       = RT_H2LE_U64(pThis->cbInputMax);

        /* Write the context state and PRNG state first. */
        rc = pfnExport(pThis, &StateExport, sizeof(StateExport), pvUser);
        if (RT_SUCCESS(rc))
            rc = pfnExport(pThis, &aszPrngExport[0], cbPrng, pvUser);
        if (RT_SUCCESS(rc))
        {
            /* Write the mutator descriptors next. */
            for (uint32_t i = 0; i < pThis->cMutators && RT_SUCCESS(rc); i++)
            {
                PRTFUZZMUTATOR pMutator = &pThis->paMutators[i];
                uint32_t cchId = (uint32_t)strlen(pMutator->pszId) + 1;
                uint32_t cchIdW = RT_H2LE_U32(cchId);

                rc = pfnExport(pThis, &cchIdW, sizeof(cchIdW), pvUser);
                if (RT_SUCCESS(rc))
                    rc = pfnExport(pThis, &pMutator->pszId[0], cchId, pvUser);
            }
        }

        /* Write the mutations last. */
        if (RT_SUCCESS(rc))
        {
            RTFUZZEXPORTARGS Args;

            Args.pfnExport = pfnExport;
            Args.pvUser    = pvUser;
            rc = RTAvlU64DoWithAll(&pThis->TreeMutations, true /*fFromLeft*/, rtFuzzCtxStateExportMutations, &Args);
        }
    }

    return rc;
}


RTDECL(int) RTFuzzCtxStateExportToMem(RTFUZZCTX hFuzzCtx, void **ppvState, size_t *pcbState)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvState, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbState, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Export to file callback.
 */
static DECLCALLBACK(int) rtFuzzCtxStateExportFile(RTFUZZCTX hFuzzCtx, const void *pvBuf, size_t cbWrite, void *pvUser)
{
    RT_NOREF(hFuzzCtx);

    RTFILE hFile = (RTFILE)pvUser;
    return RTFileWrite(hFile, pvBuf, cbWrite, NULL);
}


RTDECL(int) RTFuzzCtxStateExportToFile(RTFUZZCTX hFuzzCtx, const char *pszFilename)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        rc = RTFuzzCtxStateExport(hFuzzCtx, rtFuzzCtxStateExportFile, hFile);
        RTFileClose(hFile);
        if (RT_FAILURE(rc))
            RTFileDelete(pszFilename);
    }

    return rc;
}


RTDECL(int) RTFuzzCtxCorpusInputAdd(RTFUZZCTX hFuzzCtx, const void *pvInput, size_t cbInput)
{
    PRTFUZZCTXINT pThis = hFuzzCtx;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pvInput, VERR_INVALID_POINTER);
    AssertReturn(cbInput, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    void *pvCorpus = NULL;
    PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, 0, NULL, cbInput, &pvCorpus);
    if (RT_LIKELY(pMutation))
    {
        pMutation->pMutator = &g_MutatorCorpus;
        pMutation->cbInput  = cbInput;
        memcpy(pvCorpus, pvInput, cbInput);
        rc = rtFuzzCtxMutationAdd(pThis, pMutation);
        if (RT_FAILURE(rc))
            rtFuzzMutationDestroy(pMutation);
    }
    else
        rc = VERR_NO_MEMORY;

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
    void *pvCorpus = NULL;
    int rc = RTVfsFileGetSize(hVfsFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        PRTFUZZMUTATION pMutation = rtFuzzMutationCreate(pThis, 0, NULL, cbFile, &pvCorpus);
        if (RT_LIKELY(pMutation))
        {
            pMutation->pMutator = &g_MutatorCorpus;
            pMutation->cbInput  = cbFile;
            rc = RTVfsFileRead(hVfsFile, pvCorpus, cbFile, NULL);
            if (RT_SUCCESS(rc))
                rc = rtFuzzCtxMutationAdd(pThis, pMutation);

            if (RT_FAILURE(rc))
                rtFuzzMutationDestroy(pMutation);
        }
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
    PRTFUZZMUTATION pMutationParent = rtFuzzCtxMutationPickRnd(pThis);
    do
    {
        uint32_t idxMutator = RTRandAdvU32Ex(pThis->hRand, 0, pThis->cMutators - 1);
        PCRTFUZZMUTATOR pMutator = &pThis->paMutators[idxMutator];
        PRTFUZZMUTATION pMutation = NULL;

        uint64_t offStart = 0;
        if (!(pMutator->fFlags & RTFUZZMUTATOR_F_END_OF_BUF))
            offStart = RTRandAdvU64Ex(pThis->hRand, 0, pMutationParent->cbInput);
        else
            offStart = pMutationParent->cbInput;

        rc = pMutator->pfnPrep(pThis, offStart, pMutationParent, &pMutation);
        if (   RT_SUCCESS(rc)
            && VALID_PTR(pMutation))
        {
            pMutation->pMutator = pMutator;

            if (pThis->fFlagsBehavioral & RTFUZZCTX_F_BEHAVIORAL_ADD_INPUT_AUTOMATICALLY_TO_CORPUS)
                rtFuzzCtxMutationAdd(pThis, pMutation);

            /* Create a new input. */
            PRTFUZZINPUTINT pInput = (PRTFUZZINPUTINT)rtFuzzCtxMemoryAlloc(pThis, RT_UOFFSETOF_DYN(RTFUZZINPUTINT, apMutations[pMutation->iLvl + 1]));
            if (RT_LIKELY(pInput))
            {
                pInput->u32Magic     = 0; /** @todo */
                pInput->cRefs        = 1;
                pInput->pFuzzer      = pThis;
                pInput->pMutationTop = pMutation;
                RTFuzzCtxRetain(pThis);

                /* Traverse the mutations top to bottom and insert into the array. */
                uint32_t idx = pMutation->iLvl + 1;
                PCRTFUZZMUTATION pMutationCur = pMutation;
                size_t cbAlloc = 0;
                while (idx > 0)
                {
                    pInput->apMutations[idx - 1] = pMutationCur;
                    cbAlloc = RT_MAX(cbAlloc, pMutationCur->cbInput);
                    pMutationCur = pMutationCur->pMutationParent;
                    idx--;
                }

                pInput->u.Blob.cbAlloc = cbAlloc;
                *phFuzzInput = pInput;
                return rc;
            }
            else
                rc = VERR_NO_MEMORY;
        }
    } while (++cTries <= 50);

    if (RT_SUCCESS(rc))
        rc = VERR_INVALID_STATE;

    return rc;
}


RTDECL(int) RTFuzzInputQueryBlobData(RTFUZZINPUT hFuzzInput, void **ppv, size_t *pcb)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->pFuzzer->enmType == RTFUZZCTXTYPE_BLOB, VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;
    if (!pThis->u.Blob.pvInput)
        rc = rtFuzzInputDataFinalize(pThis);

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pThis->u.Blob.pvInput);
        Assert(pThis->u.Blob.cbInput > 0);

        *ppv = pThis->u.Blob.pvInput;
        *pcb = pThis->u.Blob.cbInput;
    }

    return rc;
}


RTDECL(int) RTFuzzInputMutateStreamData(RTFUZZINPUT hFuzzInput, void *pvBuf, size_t cbBuf)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->pFuzzer->enmType == RTFUZZCTXTYPE_STREAM, VERR_INVALID_STATE);

    RT_NOREF(pvBuf, cbBuf);
    return VERR_NOT_IMPLEMENTED;
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


RTDECL(int) RTFuzzInputQueryDigestString(RTFUZZINPUT hFuzzInput, char *pszDigest, size_t cchDigest)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->pFuzzer->enmType == RTFUZZCTXTYPE_BLOB, VERR_INVALID_STATE);
    AssertPtrReturn(pszDigest, VERR_INVALID_POINTER);
    AssertReturn(cchDigest >= RTMD5_STRING_LEN + 1, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    if (!pThis->u.Blob.pvInput)
        rc = rtFuzzInputDataFinalize(pThis);

    if (RT_SUCCESS(rc))
    {
        uint8_t abHash[RTMD5_HASH_SIZE];
        RTMd5(pThis->u.Blob.pvInput, pThis->u.Blob.cbInput, &abHash[0]);
        rc = RTMd5ToString(&abHash[0], pszDigest, cchDigest);
    }

    return rc;
}


RTDECL(int) RTFuzzInputWriteToFile(RTFUZZINPUT hFuzzInput, const char *pszFilename)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->pFuzzer->enmType == RTFUZZCTXTYPE_BLOB, VERR_INVALID_STATE);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    if (!pThis->u.Blob.pvInput)
        rc = rtFuzzInputDataFinalize(pThis);

    if (RT_SUCCESS(rc))
    {
        RTFILE hFile;
        rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileWrite(hFile, pThis->u.Blob.pvInput, pThis->u.Blob.cbInput, NULL);
            AssertRC(rc);
            RTFileClose(hFile);

            if (RT_FAILURE(rc))
                RTFileDelete(pszFilename);
        }
    }

    return rc;
}


RTDECL(int) RTFuzzInputAddToCtxCorpus(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtFuzzCtxMutationAdd(pThis->pFuzzer, pThis->pMutationTop);
}


RTDECL(int) RTFuzzInputRemoveFromCtxCorpus(RTFUZZINPUT hFuzzInput)
{
    PRTFUZZINPUTINT pThis = hFuzzInput;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

#if 0
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
#endif

    return VERR_NOT_IMPLEMENTED;
}

