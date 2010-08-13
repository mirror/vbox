/** @file
 * STL-like vector implementation in C
 * @note  functions in this file are inline to prevent warnings about
 *        unused static functions.  I assume that in this day and age a
 *        compiler makes its own decisions about whether to actually
 *        inline a function.
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define VECTOR_CONCAT(a, b) a ## b
#define VECTOR_XCONCAT(a, b) VECTOR_CONCAT(a, b)
/** A publicly visible (method, type, etc) name relating to the vector type */
#define VECTOR_PUBLIC(NAME) VECTOR_XCONCAT(VECTOR_TYPENAME, _ ## NAME)
/** An implementation-private (method, type, etc) name */
#define VECTOR_INTERNAL(NAME) VECTOR_XCONCAT(VECTOR_TYPENAME, _internal_ ## NAME)
/** The size of a vector element */
#define VECTOR_ELEMENT_SIZE sizeof(VECTOR_TYPE)
/** The unit by which the vector capacity is increased */
#define VECTOR_ALLOC_UNIT VECTOR_ELEMENT_SIZE * 16

#ifndef VECTOR_TYPE
/** VECTOR_TYPE must be defined to the type that the vector will contain.  The
 * macro will be undef-ed again by this header. */
# error You must define VECTOR_TYPE before including this header!
#endif
#ifndef VECTOR_TYPENAME
/** VECTOR_TYPENAME must be defined to the typename for the vector.  The
 * macro will be undef-ed again by this header. */
# error You must define VECTOR_TYPENAME before including this header!
#endif
#ifndef VECTOR_ALLOCATOR
/** VECTOR_ALLOCATOR can be defined to an alternative allocator for the
 * vector's memory.  The allocator must be a function with realloc semantics.
 * The macro will be undef-ed again by this header. */
# define VECTOR_ALLOCATOR RTMemRealloc
#endif
#ifndef VECTOR_DESTRUCTOR
/** VECTOR_DESTRUCTOR can be defined to be a routine to clean up vector
 * elements before they are freed.  It must return void and take a pointer to
 * an element as a parameter.  The macro will be undef-ed again by this header.
 */
# define VECTOR_DESTRUCTOR VECTOR_INTERNAL(empty_destructor)
#endif

/** Structure describing the vector itself */
typedef struct VECTOR_TYPENAME
{
    /** The beginning of the allocated memory */
    VECTOR_TYPE *mBegin;
    /** Pointer to just after the end of the last element */
    VECTOR_TYPE *mEnd;
    /** Pointer to just after the end of the allocated memory */
    VECTOR_TYPE *mCapacity;
} VECTOR_TYPENAME;

/*** Private methods ***/

/** Destructor that does nothing. */
static inline void VECTOR_INTERNAL(empty_destructor)(VECTOR_TYPENAME *pSelf)
{
    (void) pSelf;
}

/** Expand an existing vector */
static inline int VECTOR_INTERNAL(expand)(VECTOR_TYPENAME *pSelf)
{
    size_t cNewCap = pSelf->mCapacity - pSelf->mBegin + VECTOR_ALLOC_UNIT;
    size_t cOffEnd = pSelf->mEnd - pSelf->mBegin;
    void *pRealloc = VECTOR_ALLOCATOR(pSelf->mBegin, cNewCap);
    if (!pRealloc)
        return 0;
    pSelf->mBegin = (VECTOR_TYPE *)pRealloc;
    pSelf->mEnd = pSelf->mBegin + cOffEnd;
    pSelf->mCapacity = pSelf->mBegin + cNewCap / VECTOR_ELEMENT_SIZE;
    memset(pSelf->mEnd, 0, pSelf->mCapacity - pSelf->mEnd);
    return 1;
}

/** Expand an existing vector */
static inline void VECTOR_INTERNAL(destruct_all)(VECTOR_TYPENAME *pSelf)
{
    VECTOR_TYPE *pIter;
    for (pIter = pSelf->mBegin; pIter < pSelf->mEnd; ++pIter)
        VECTOR_DESTRUCTOR(pIter);
}

/*** Public methods ***/

/** Initialise a newly allocated vector.  The vector will be zeroed on failure.
 */
static inline int VECTOR_PUBLIC(init)(VECTOR_TYPENAME *pSelf)
{
    memset(pSelf, 0, sizeof(*pSelf));
    return VECTOR_INTERNAL(expand)(pSelf);
}

/** Clean up a vector so that the memory can be returned.  For convenience,
 * this may be used safely on a zeroed vector structure. */
static inline void VECTOR_PUBLIC(cleanup)(VECTOR_TYPENAME *pSelf)
{
    if (pSelf->mBegin)
    {
        VECTOR_INTERNAL(destruct_all)(pSelf);
        VECTOR_ALLOCATOR(pSelf->mBegin, 0);
    }
    pSelf->mBegin = pSelf->mEnd = pSelf->mCapacity = NULL;
}

/** Add a value onto the end of a vector */
static inline int VECTOR_PUBLIC(push_back)(VECTOR_TYPENAME *pSelf,
                                    VECTOR_TYPE *pElement)
{
    if (pSelf->mEnd == pSelf->mCapacity && !VECTOR_INTERNAL(expand)(pSelf))
        return 0;
    if (pElement)
        *pSelf->mEnd = *pElement;
    ++pSelf->mEnd;
    return 1;
}

/** Reset the vector */
static inline void VECTOR_PUBLIC(clear)(VECTOR_TYPENAME *pSelf)
{
    VECTOR_INTERNAL(destruct_all)(pSelf);
    memset(pSelf->mBegin, 0, pSelf->mEnd - pSelf->mBegin);
    pSelf->mEnd = pSelf->mBegin;
}

/** Number of elements in the vector */
static inline size_t VECTOR_PUBLIC(size)(VECTOR_TYPENAME *pSelf)
{
    return (pSelf->mEnd - pSelf->mBegin) / VECTOR_ELEMENT_SIZE;
}

/*** Iterators ***/

/** Non-constant iterator over the vector */
typedef VECTOR_TYPE *VECTOR_PUBLIC(iterator);

/** Initialise a newly allocated iterator */
static inline void VECTOR_PUBLIC(iter_init)(VECTOR_PUBLIC(iterator) *pSelf,
                                     const VECTOR_PUBLIC(iterator) *pOther)
{
    *pSelf = *pOther;
}

/** Dereference an iterator */
static inline VECTOR_TYPE *VECTOR_PUBLIC(iter_target)(VECTOR_PUBLIC(iterator) *pSelf)
{
    return *pSelf;
}

/** Increment an iterator */
static inline void VECTOR_PUBLIC(iter_incr)(VECTOR_PUBLIC(iterator) *pSelf)
{
    ++*pSelf;
}

/** Get the special "begin" iterator for a vector */
static inline const VECTOR_PUBLIC(iterator) *VECTOR_PUBLIC(begin)
                                                 (VECTOR_TYPENAME *pSelf)
{
    return &pSelf->mBegin;
}

/** Get the special "end" iterator for a vector */
static inline const VECTOR_PUBLIC(iterator) *VECTOR_PUBLIC(end)
                                                 (VECTOR_TYPENAME *pSelf)
{
    return &pSelf->mEnd;
}

/** Test whether an iterator is less than another. */
static inline int VECTOR_PUBLIC(iter_lt)(const VECTOR_PUBLIC(iterator) *pFirst,
                                         const VECTOR_PUBLIC(iterator) *pSecond)
{
    return *pFirst < *pSecond;
}

/** Test whether an iterator is equal to another.  The special values
 * ITERATOR_BEGIN and ITERATOR_END are recognised. */
static inline int VECTOR_PUBLIC(iter_eq)(const VECTOR_PUBLIC(iterator) *pFirst,
                                         const VECTOR_PUBLIC(iterator) *pSecond)
{
    return *pFirst == *pSecond;
}

/* We need to undefine anything we have defined (and for convenience we also
 * undefine our "parameter" macros) as this header may be included multiple
 * times in one source file with different parameters. */
#undef VECTOR_CONCAT
#undef VECTOR_XCONCAT
#undef VECTOR_PUBLIC
#undef VECTOR_INTERN
#undef VECTOR_ELEMENT_SIZE
#undef VECTOR_ALLOC_UNIT
#undef VECTOR_TYPE
#undef VECTOR_TYPENAME
#undef VECTOR_ALLOCATOR
#undef VECTOR_DESTRUCTOR
