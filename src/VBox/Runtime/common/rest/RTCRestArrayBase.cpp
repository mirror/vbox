/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestArrayBase implementation.
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
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/string.h>



/**
 * Default destructor.
 */
RTCRestArrayBase::RTCRestArrayBase()
    : RTCRestObjectBase()
    , m_papElements(NULL)
    , m_cElements(0)
    , m_cCapacity(0)
{
}


#if 0 /* should not be used */
/**
 * Copy constructor.
 */
RTCRestArrayBase::RTCRestArrayBase(RTCRestArrayBase const &a_rThat);
#endif

/**
 * Destructor.
 */
RTCRestArrayBase::~RTCRestArrayBase()
{
    clear();

    if (m_papElements)
    {
        RTMemFree(m_papElements);
        m_papElements = NULL;
        m_cCapacity = 0;
    }
}


#if 0 /* should not be used */
/**
 * Copy assignment operator.
 */
RTCRestArrayBase &RTCRestArrayBase::operator=(RTCRestArrayBase const &a_rThat);
#endif


/*********************************************************************************************************************************
*   Overridden methods                                                                                                           *
*********************************************************************************************************************************/

int RTCRestArrayBase::resetToDefault()
{
    /* The default state of an array is empty. At least for now. */
    clear();
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestArrayBase::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("[\n");
    unsigned const uOldIndent = a_rDst.incrementIndent();

    for (size_t i = 0; i < m_cElements; i++)
    {
        m_papElements[i]->serializeAsJson(a_rDst);
        if (i < m_cElements)
            a_rDst.printf(",\n");
        else
            a_rDst.printf("\n");
    }

    a_rDst.setIndent(uOldIndent);
    a_rDst.printf("]");
    return a_rDst;
}


int RTCRestArrayBase::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    /*
     * Make sure the object starts out with an empty map.
     */
    if (m_cElements > 0)
        clear();

    /*
     * Iterate the array values.
     */
    RTJSONIT hIterator;
    int rcRet = RTJsonIteratorBeginArray(a_rCursor.m_hValue, &hIterator);
    if (RT_SUCCESS(rcRet))
    {
        for (size_t idxName = 0;; idxName++)
        {
            RTCRestJsonCursor SubCursor(a_rCursor);
            int rc = RTJsonIteratorQueryValue(hIterator, &SubCursor.m_hValue, &SubCursor.m_pszName);
            if (RT_SUCCESS(rc))
            {
                RTCRestObjectBase *pObj = createValue();
                if (pObj)
                {
                    char szName[32];
                    RTStrPrintf(szName, sizeof(szName), "[%u]", idxName);
                    SubCursor.m_pszName = szName;

                    rc = pObj->deserializeFromJson(SubCursor);
                    if (RT_SUCCESS(rc))
                    { /* likely */ }
                    else if (RT_SUCCESS(rcRet))
                        rcRet = rc;

                    rc = insertWorker(~(size_t)0, pObj, false /*a_fReplace*/);
                    if (RT_SUCCESS(rc))
                    { /* likely */ }
                    else
                    {
                        rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "Array insert failed (index %zu): %Rrc",
                                                               idxName, rc);
                        delete pObj;
                    }
                }
                else
                    rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "Failed to create new value object");
            }
            else
                rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonIteratorQueryValue failed: %Rrc", rc);

            /*
             * Advance.
             */
            rc = RTJsonIteratorNext(hIterator);
            if (RT_SUCCESS(rc))
            { /* likely */ }
            else if (rc == VERR_JSON_ITERATOR_END)
                break;
            else
            {
                rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonIteratorNext failed: %Rrc", rc);
                break;
            }
        }

        RTJsonIteratorFree(hIterator);
    }
    else
        rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rcRet, "RTJsonIteratorBeginArray failed: %Rrc", rcRet);
    return rcRet;

}


int RTCRestArrayBase::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    int rc;
    if (m_cElements)
    {
        static char const s_szSep[kCollectionFormat_Mask + 1] = ",, \t|,,";
        char const chSep = s_szSep[a_fFlags & kCollectionFormat_Mask];

        rc = m_papElements[0]->toString(a_pDst, a_fFlags);
        for (size_t i = 1; RT_SUCCESS(rc) && i < m_cElements; i++)
        {
            rc = a_pDst->appendNoThrow(chSep);
            if (RT_SUCCESS(rc))
                rc = m_papElements[i]->toString(a_pDst, a_fFlags | kToString_Append);
        }
    }
    else
    {
        a_pDst->setNull();
        rc = VINF_SUCCESS;
    }
    return rc;
}


int RTCRestArrayBase::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                                 uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    /** @todo proper fromString implementation for arrays. */
    return RTCRestObjectBase::fromString(a_rValue, a_pszName, a_pErrInfo, a_fFlags);
}



/*********************************************************************************************************************************
*   Array methods                                                                                                                *
*********************************************************************************************************************************/

void RTCRestArrayBase::clear()
{
    size_t i = m_cElements;
    while (i-- > 0)
    {
        delete m_papElements[i];
        m_papElements[i] = NULL;
    }
    m_cElements = 0;
}


bool RTCRestArrayBase::removeAt(size_t a_idx)
{
    if (a_idx == ~(size_t)0)
        a_idx = m_cElements - 1;
    if (a_idx < m_cElements)
    {
        delete m_papElements[a_idx];
        m_papElements[a_idx] = NULL;

        m_cElements--;
        if (a_idx < m_cElements)
            memmove(&m_papElements[a_idx], &m_papElements[a_idx + 1], (m_cElements - a_idx) * sizeof(m_papElements[0]));

        m_cElements--;
    }
    return false;
}


int RTCRestArrayBase::ensureCapacity(size_t a_cEnsureCapacity)
{
    if (m_cCapacity < a_cEnsureCapacity)
    {
        if (a_cEnsureCapacity < 512)
            a_cEnsureCapacity = RT_ALIGN_Z(a_cEnsureCapacity, 16);
        else if (a_cEnsureCapacity < 16384)
            a_cEnsureCapacity = RT_ALIGN_Z(a_cEnsureCapacity, 128);
        else
            a_cEnsureCapacity = RT_ALIGN_Z(a_cEnsureCapacity, 512);

        void *pvNew = RTMemRealloc(m_papElements, sizeof(m_papElements[0]) * a_cEnsureCapacity);
        if (pvNew)
        {
            m_papElements = (RTCRestObjectBase **)pvNew;
            memset(m_papElements, 0, (a_cEnsureCapacity - m_cCapacity) * sizeof(sizeof(m_papElements[0])));
            m_cCapacity   = a_cEnsureCapacity;
        }
        else
            return VERR_NO_MEMORY;
    }
    return VINF_SUCCESS;
}


int RTCRestArrayBase::copyArrayWorker(RTCRestArrayBase const &a_rThat, bool a_fThrow)
{
    int rc;
    clear();
    if (a_rThat.m_cElements == 0)
        rc = VINF_SUCCESS;
    else
    {
        rc = ensureCapacity(a_rThat.m_cElements);
        if (RT_SUCCESS(rc))
        {
            for (size_t i = 0; i < a_rThat.m_cElements; i++)
            {
                rc = insertCopyWorker(i, *a_rThat.m_papElements[i], false);
                if (RT_SUCCESS(rc))
                { /* likely */ }
                else if (a_fThrow)
                    throw std::bad_alloc();
                else
                    return rc;
            }
        }
    }
    return rc;
}


int RTCRestArrayBase::insertWorker(size_t a_idx, RTCRestObjectBase *a_pValue, bool a_fReplace)
{
    AssertPtrReturn(a_pValue, VERR_INVALID_POINTER);

    if (a_idx == ~(size_t)0)
        a_idx = m_cElements;

    if (a_idx <= m_cElements)
    {
        if (a_idx == m_cElements || !a_fReplace)
        {
            /* Make sure we've got array space. */
            if (m_cElements + 1 < m_cCapacity)
            { /* kind of likely */ }
            else
            {
                int rc = ensureCapacity(m_cElements + 1);
                if (RT_SUCCESS(rc))
                { /* likely */ }
                else
                    return rc;
            }

            /* Shift following elements before inserting. */
            if (a_idx < m_cElements)
                memmove(&m_papElements[a_idx + 1], &m_papElements[a_idx], (m_cElements - a_idx) * sizeof(m_papElements[0]));
            m_papElements[a_idx] = a_pValue;
            m_cElements++;
            return VINF_SUCCESS;
        }

        /* Replace element. */
        delete m_papElements[a_idx];
        m_papElements[a_idx] = a_pValue;
        return VWRN_ALREADY_EXISTS;
    }
    return VERR_OUT_OF_RANGE;
}


int RTCRestArrayBase::insertCopyWorker(size_t a_idx, RTCRestObjectBase const &a_rValue, bool a_fReplace)
{
    int rc;
    RTCRestObjectBase *pValueCopy = createValueCopy(&a_rValue);
    if (pValueCopy)
    {
        rc = insertWorker(a_idx, pValueCopy, a_fReplace);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
            delete pValueCopy;
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

