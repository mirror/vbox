/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestObjectBase implementation.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>



/*********************************************************************************************************************************
*   RTCRestObjectBase implementation                                                                                             *
*********************************************************************************************************************************/

/** Default constructor. */
RTCRestObjectBase::RTCRestObjectBase()
    : m_fNullIndicator(false)
{
}


/** Copy constructor. */
RTCRestObjectBase::RTCRestObjectBase(RTCRestObjectBase const &a_rThat)
    : m_fNullIndicator(a_rThat.m_fNullIndicator)
{
}


/** Destructor. */
RTCRestObjectBase::~RTCRestObjectBase()
{
    /* nothing to do */
}


int RTCRestObjectBase::setNull()
{
    int rc = resetToDefault();
    m_fNullIndicator = true;
    return rc;
}


void RTCRestObjectBase::setNotNull()
{
    m_fNullIndicator = false;
}


int RTCRestObjectBase::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    /*
     * Just wrap the JSON serialization method.
     */
    RTCRestOutputToString Tmp(a_pDst, RT_BOOL(a_fFlags & kToString_Append));
    serializeAsJson(Tmp);
    return Tmp.finalize() ? VINF_SUCCESS : VERR_NO_MEMORY;
}


RTCString RTCRestObjectBase::toString() const
{
    RTCString strRet;
    toString(&strRet, 0);
    return strRet;
}


int RTCRestObjectBase::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                                  uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    /*
     * Just wrap the JSON serialization method.
     */
    RTJSONVAL hValue = NIL_RTJSONVAL;
    int rc = RTJsonParseFromString(&hValue, a_rValue.c_str(), a_pErrInfo);
    if (RT_SUCCESS(rc))
    {
        RTCRestJsonPrimaryCursor PrimaryCursor(hValue, a_pszName, a_pErrInfo);
        rc = deserializeFromJson(PrimaryCursor.m_Cursor);
    }
    return rc;
}


RTCRestObjectBase::kTypeClass RTCRestObjectBase::typeClass() const
{
    return kTypeClass_Object;
}


/*********************************************************************************************************************************
*   RTCRestBool implementation                                                                                                   *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestBool::createInstance(void)
{
    return new (std::nothrow) RTCRestBool();
}


/** Default constructor. */
RTCRestBool::RTCRestBool()
    : RTCRestObjectBase()
    , m_fValue(false)
{
}


/** Copy constructor. */
RTCRestBool::RTCRestBool(RTCRestBool const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_fValue(a_rThat.m_fValue)
{
}


/** From value constructor. */
RTCRestBool::RTCRestBool(bool fValue)
    : m_fValue(fValue)
{
}


/** Destructor. */
RTCRestBool::~RTCRestBool()
{
    /* nothing to do */
}


/** Copy assignment operator. */
RTCRestBool &RTCRestBool::operator=(RTCRestBool const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_fValue = a_rThat.m_fValue;
    return *this;
}


int RTCRestBool::assignCopy(RTCRestBool const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_fValue = a_rThat.m_fValue;
    return VINF_SUCCESS;
}


void RTCRestBool::assignValue(bool a_fValue)
{
    m_fValue = a_fValue;
    m_fNullIndicator = false;
}


int RTCRestBool::resetToDefault()
{
    m_fValue = false;
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestBool::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf(!m_fNullIndicator ? m_fValue ? "true" : "false" : "null");
    return a_rDst;
}


int RTCRestBool::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);

    if (enmType == RTJSONVALTYPE_TRUE)
    {
        m_fValue = true;
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }

    if (enmType == RTJSONVALTYPE_FALSE)
    {
        m_fValue = false;
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fValue = false;
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for boolean",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestBool::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
    {
        if (!m_fNullIndicator)
        {
            if (m_fValue)
                return a_pDst->assignNoThrow(RT_STR_TUPLE("true"));
            return a_pDst->assignNoThrow(RT_STR_TUPLE("false"));
        }
        return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
    }

    if (!m_fNullIndicator)
    {
        if (m_fValue)
            return a_pDst->appendNoThrow(RT_STR_TUPLE("true"));
        return a_pDst->appendNoThrow(RT_STR_TUPLE("false"));
    }
    return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestBool::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                            uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    if (a_rValue.startsWithWord("true", RTCString::CaseInsensitive))
    {
        m_fValue = true;
        m_fNullIndicator = false;
    }
    else if (a_rValue.startsWithWord("false", RTCString::CaseInsensitive))
    {
        m_fValue = false;
        m_fNullIndicator = false;
    }
    else if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_fValue = false;
        m_fNullIndicator = true;
    }
    else
        return RTErrInfoSetF(a_pErrInfo, VERR_INVALID_PARAMETER, "%s: unable to parse '%s' as bool", a_pszName, a_rValue.c_str());
    return VINF_SUCCESS;
}


RTCRestObjectBase::kTypeClass RTCRestBool::typeClass() const
{
    return kTypeClass_Bool;
}


const char *RTCRestBool::typeName() const
{
    return "bool";
}



/*********************************************************************************************************************************
*   RTCRestInt64 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestInt64::createInstance(void)
{
    return new (std::nothrow) RTCRestInt64();
}


/** Default destructor. */
RTCRestInt64::RTCRestInt64()
    : RTCRestObjectBase()
    , m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt64::RTCRestInt64(RTCRestInt64 const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_iValue(a_rThat.m_iValue)
{
}


/** From value constructor. */
RTCRestInt64::RTCRestInt64(int64_t iValue)
    : RTCRestObjectBase()
    , m_iValue(iValue)
{
}


/** Destructor. */
RTCRestInt64::~RTCRestInt64()
{
    /* nothing to do */
}


/** Copy assignment operator. */
RTCRestInt64 &RTCRestInt64::operator=(RTCRestInt64 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt64::assignCopy(RTCRestInt64 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


void RTCRestInt64::assignValue(int64_t a_iValue)
{
    m_iValue = a_iValue;
    m_fNullIndicator = false;
}


int RTCRestInt64::resetToDefault()
{
    m_iValue = 0;
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestInt64::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
        a_rDst.printf("%RI64", m_iValue);
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestInt64::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_iValue = 0;
    m_fNullIndicator = false;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_INTEGER)
    {
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &m_iValue);
        if (RT_SUCCESS(rc))
            return rc;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 64-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt64::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
    {
        if (!m_fNullIndicator)
            return a_pDst->printfNoThrow("%RI64", m_iValue);
        return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
    }
    if (!m_fNullIndicator)
        return a_pDst->appendPrintfNoThrow("%RI64", m_iValue);
    return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestInt64::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    m_iValue = 0;
    m_fNullIndicator = false;

    int rc = RTStrToInt64Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;

    if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_iValue = 0;
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int64_t", a_pszName, rc, a_rValue.c_str());
}


RTCRestObjectBase::kTypeClass RTCRestInt64::typeClass() const
{
    return kTypeClass_Int64;
}


const char *RTCRestInt64::typeName() const
{
    return "int64_t";
}



/*********************************************************************************************************************************
*   RTCRestInt32 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestInt32::createInstance(void)
{
    return new (std::nothrow) RTCRestInt32();
}


/** Default destructor. */
RTCRestInt32::RTCRestInt32()
    : RTCRestObjectBase()
    , m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt32::RTCRestInt32(RTCRestInt32 const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_iValue(a_rThat.m_iValue)
{
}


/** From value constructor. */
RTCRestInt32::RTCRestInt32(int32_t iValue)
    : RTCRestObjectBase()
    , m_iValue(iValue)
{
}


/** Destructor. */
RTCRestInt32::~RTCRestInt32()
{
    /* nothing to do */
}


/** Copy assignment operator. */
RTCRestInt32 &RTCRestInt32::operator=(RTCRestInt32 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt32::assignCopy(RTCRestInt32 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


int RTCRestInt32::resetToDefault()
{
    m_iValue = 0;
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


void RTCRestInt32::assignValue(int32_t a_iValue)
{
    m_iValue = a_iValue;
    m_fNullIndicator = false;
}


RTCRestOutputBase &RTCRestInt32::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
        a_rDst.printf("%RI32", m_iValue);
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestInt32::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_iValue = 0;
    m_fNullIndicator = false;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_INTEGER)
    {
        int64_t iTmp = m_iValue;
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &iTmp);
        if (RT_SUCCESS(rc))
        {
            m_iValue = (int32_t)iTmp;
            if (m_iValue == iTmp)
                return rc;
            return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_OUT_OF_RANGE, "value %RI64 does not fit in 32 bits", iTmp);
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 32-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt32::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
    {
        if (!m_fNullIndicator)
            return a_pDst->printfNoThrow("%RI32", m_iValue);
        return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
    }
    if (!m_fNullIndicator)
        return a_pDst->appendPrintfNoThrow("%RI32", m_iValue);
    return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestInt32::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    m_iValue = 0;
    m_fNullIndicator = false;

    int rc = RTStrToInt32Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;

    if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_iValue = 0;
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int32_t", a_pszName, rc, a_rValue.c_str());
}


RTCRestObjectBase::kTypeClass RTCRestInt32::typeClass() const
{
    return kTypeClass_Int32;
}


const char *RTCRestInt32::typeName() const
{
    return "int32_t";
}



/*********************************************************************************************************************************
*   RTCRestInt16 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestInt16::createInstance(void)
{
    return new (std::nothrow) RTCRestInt16();
}


/** Default destructor. */
RTCRestInt16::RTCRestInt16()
    : RTCRestObjectBase()
    , m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt16::RTCRestInt16(RTCRestInt16 const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_iValue(a_rThat.m_iValue)
{
}


/** From value constructor. */
RTCRestInt16::RTCRestInt16(int16_t iValue)
    : RTCRestObjectBase()
    , m_iValue(iValue)
{
}


/** Destructor. */
RTCRestInt16::~RTCRestInt16()
{
    /* nothing to do */
}


/** Copy assignment operator. */
RTCRestInt16 &RTCRestInt16::operator=(RTCRestInt16 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt16::assignCopy(RTCRestInt16 const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


void RTCRestInt16::assignValue(int16_t a_iValue)
{
    m_iValue = a_iValue;
    m_fNullIndicator = false;
}


int RTCRestInt16::resetToDefault()
{
    m_iValue = 0;
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestInt16::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
        a_rDst.printf("%RI16", m_iValue);
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestInt16::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_iValue = 0;
    m_fNullIndicator = false;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_INTEGER)
    {
        int64_t iTmp = m_iValue;
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &iTmp);
        if (RT_SUCCESS(rc))
        {
            m_iValue = (int16_t)iTmp;
            if (m_iValue == iTmp)
                return rc;
            return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_OUT_OF_RANGE, "value %RI64 does not fit in 16 bits", iTmp);
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 16-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt16::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
    {
        if (!m_fNullIndicator)
            return a_pDst->printfNoThrow("%RI16", m_iValue);
        return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
    }
    if (!m_fNullIndicator)
        return a_pDst->appendPrintfNoThrow("%RI16", m_iValue);
    return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestInt16::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    m_iValue = 0;
    m_fNullIndicator = false;

    int rc = RTStrToInt16Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;

    if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_iValue = 0;
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int16_t", a_pszName, rc, a_rValue.c_str());
}


RTCRestObjectBase::kTypeClass RTCRestInt16::typeClass() const
{
    return kTypeClass_Int16;
}


const char *RTCRestInt16::typeName() const
{
    return "int16_t";
}



/*********************************************************************************************************************************
*   RTCRestDouble implementation                                                                                                 *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestDouble::createInstance(void)
{
    return new (std::nothrow) RTCRestDouble();
}

/** Default destructor. */
RTCRestDouble::RTCRestDouble()
    : RTCRestObjectBase()
    , m_rdValue(0.0)
{
}


/** Copy constructor. */
RTCRestDouble::RTCRestDouble(RTCRestDouble const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_rdValue(a_rThat.m_rdValue)
{
}


/** From value constructor. */
RTCRestDouble::RTCRestDouble(double rdValue)
    : RTCRestObjectBase()
    , m_rdValue(rdValue)
{
}


/** Destructor. */
RTCRestDouble::~RTCRestDouble()
{
    /* nothing to do */
}


/** Copy assignment operator. */
RTCRestDouble &RTCRestDouble::operator=(RTCRestDouble const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_rdValue = a_rThat.m_rdValue;
    return *this;
}


int RTCRestDouble::assignCopy(RTCRestDouble const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_rdValue = a_rThat.m_rdValue;
    return VINF_SUCCESS;
}


void RTCRestDouble::assignValue(double a_rdValue)
{
    m_rdValue = a_rdValue;
    m_fNullIndicator = false;
}


int RTCRestDouble::resetToDefault()
{
    m_rdValue = 0.0;
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestDouble::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
    {

        /* Just a simple approximation here. */
        /** @todo Not 100% sure printf %g produces the right result for JSON floating point, but it'll have to do for now... */
        char szValue[128];
#ifdef _MSC_VER
        _snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#else
        snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#endif
        a_rDst.printf("%s", szValue);
    }
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestDouble::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_rdValue = 0.0;
    m_fNullIndicator = false;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
    {
        int rc = RTJsonValueQueryNumber(a_rCursor.m_hValue, &m_rdValue);
        if (RT_SUCCESS(rc))
            return rc;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryNumber failed with %Rrc", rc);
    }

    if (enmType == RTJSONVALTYPE_INTEGER)
    {
        int64_t iTmp = 0;
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &iTmp);
        if (RT_SUCCESS(rc))
        {
            m_rdValue = iTmp;
            if (m_rdValue == iTmp)
                return rc;
            return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_OUT_OF_RANGE, "value %RI64 does not fit in a double", iTmp);
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_TRUE)
        m_rdValue = 1.0;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for a double",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestDouble::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!m_fNullIndicator)
    {
        /* Just a simple approximation here. */
        /** @todo Not 100% sure printf %g produces the right result for JSON floating point, but it'll have to do for now... */
        char szValue[128];
#ifdef _MSC_VER
        _snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#else
        snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#endif
        size_t const cchValue = strlen(szValue);

        if (!(a_fFlags & kToString_Append))
            return a_pDst->assignNoThrow(szValue, cchValue);
        return a_pDst->appendNoThrow(szValue, cchValue);
    }

    if (!(a_fFlags & kToString_Append))
        return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
    return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestDouble::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                              uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    m_fNullIndicator = false;

    const char *pszValue = RTStrStripL(a_rValue.c_str());
    errno = 0;
    char *pszNext = NULL;
    m_rdValue = strtod(pszValue, &pszNext);
    if (errno == 0)
        return VINF_SUCCESS;

    if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_rdValue = 0.0;
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    int rc = RTErrConvertFromErrno(errno);
    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as double", a_pszName, rc, a_rValue.c_str());
}


RTCRestObjectBase::kTypeClass RTCRestDouble::typeClass() const
{
    return kTypeClass_Double;
}


const char *RTCRestDouble::typeName() const
{
    return "double";
}



/*********************************************************************************************************************************
*   RTCRestString implementation                                                                                                 *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestString::createInstance(void)
{
    return new (std::nothrow) RTCRestString();
}


/** Default destructor. */
RTCRestString::RTCRestString()
    : RTCString()
    , RTCRestObjectBase()
{
}


/** Copy constructor. */
RTCRestString::RTCRestString(RTCRestString const &a_rThat)
    : RTCString(a_rThat)
    , RTCRestObjectBase(a_rThat)
{
}


/** From value constructor. */
RTCRestString::RTCRestString(RTCString const &a_rThat)
    : RTCString(a_rThat)
{
}


/** From value constructor. */
RTCRestString::RTCRestString(const char *a_pszSrc)
    : RTCString(a_pszSrc)
    , RTCRestObjectBase()
{
}


/** Destructor. */
RTCRestString::~RTCRestString()
{
    /* nothing to do */
}


int RTCRestString::assignCopy(RTCRestString const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    return assignNoThrow(a_rThat);
}


int RTCRestString::assignCopy(RTCString const &a_rThat)
{
    m_fNullIndicator = false;
    return assignNoThrow(a_rThat);
}


int RTCRestString::assignCopy(const char *a_pszThat)
{
    m_fNullIndicator = false;
    return assignNoThrow(a_pszThat);
}


int RTCRestString::setNull()
{
    RTCString::setNull();
    m_fNullIndicator = true;
    return VINF_SUCCESS;
}


int RTCRestString::resetToDefault()
{
    RTCString::setNull();
    m_fNullIndicator = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestString::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
        a_rDst.printf("%RMjs", m_psz);
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestString::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_fNullIndicator = false;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_STRING)
    {
        const char  *pszValue = RTJsonValueGetString(a_rCursor.m_hValue);
        const size_t cchValue = strlen(pszValue);
        int rc = assignNoThrow(pszValue, cchValue);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "no memory for %zu char long string", cchValue);
    }

    RTCString::setNull();

    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for string",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestString::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    /* Note! m_fNullIndicator == true: empty string. */
    if (!(a_fFlags & kToString_Append))
        return a_pDst->assignNoThrow(*this);
    return a_pDst->appendNoThrow(*this);
}


int RTCRestString::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                              uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags); RT_NOREF(a_pszName); RT_NOREF(a_pErrInfo);

    /* Note! Unable to set m_fNullIndicator = true here. */
    m_fNullIndicator = false;
    return assignNoThrow(a_rValue);
}


RTCRestObjectBase::kTypeClass RTCRestString::typeClass() const
{
    return kTypeClass_String;
}


const char *RTCRestString::typeName() const
{
    return "RTCString";
}



/*********************************************************************************************************************************
*   RTCRestStringEnumBase implementation                                                                                         *
*********************************************************************************************************************************/

/** Default constructor. */
RTCRestStringEnumBase::RTCRestStringEnumBase()
    : RTCRestObjectBase()
    , m_iEnumValue(0 /*invalid*/)
    , m_strValue()
{
}


/** Destructor. */
RTCRestStringEnumBase::~RTCRestStringEnumBase()
{
    /* nothing to do */
}


/** Copy constructor. */
RTCRestStringEnumBase::RTCRestStringEnumBase(RTCRestStringEnumBase const &a_rThat)
    : RTCRestObjectBase(a_rThat)
    , m_iEnumValue(a_rThat.m_iEnumValue)
    , m_strValue(a_rThat.m_strValue)
{
}


/** Copy assignment operator. */
RTCRestStringEnumBase &RTCRestStringEnumBase::operator=(RTCRestStringEnumBase const &a_rThat)
{
    RTCRestObjectBase::operator=(a_rThat);
    m_iEnumValue = a_rThat.m_iEnumValue;
    m_strValue = a_rThat.m_strValue;
    return *this;
}


int RTCRestStringEnumBase::assignCopy(RTCRestStringEnumBase const &a_rThat)
{
    m_fNullIndicator = a_rThat.m_fNullIndicator;
    m_iEnumValue = a_rThat.m_iEnumValue;
    return m_strValue.assignNoThrow(a_rThat.m_strValue);
}


int RTCRestStringEnumBase::resetToDefault()
{
    m_iEnumValue = 0; /*invalid*/
    m_strValue.setNull();
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestStringEnumBase::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (!m_fNullIndicator)
        a_rDst.printf("%RMjs", getString());
    else
        a_rDst.printf("null");
    return a_rDst;
}


int RTCRestStringEnumBase::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    m_fNullIndicator = false;
    m_iEnumValue     = 0;

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_STRING)
    {
        const char  *pszValue = RTJsonValueGetString(a_rCursor.m_hValue);
        const size_t cchValue = strlen(pszValue);
        int rc = setByString(pszValue, cchValue);
        if (RT_SUCCESS(rc))
            return rc;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "no memory for %zu char long string", cchValue);
    }

    m_strValue.setNull();
    if (enmType == RTJSONVALTYPE_NULL)
    {
        m_fNullIndicator = true;
        return VINF_SUCCESS;
    }

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for string/enum",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestStringEnumBase::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!m_fNullIndicator)
    {
        if (m_iEnumValue > 0)
        {
            size_t              cEntries  = 0;
            ENUMMAPENTRY const *paEntries = getMappingTable(&cEntries);
            AssertReturn((unsigned)(m_iEnumValue - 1) < cEntries, VERR_REST_INTERNAL_ERROR_3);
            Assert(paEntries[m_iEnumValue - 1].iValue == m_iEnumValue);

            if (a_fFlags & kToString_Append)
                return a_pDst->appendNoThrow(paEntries[m_iEnumValue - 1].pszName, paEntries[m_iEnumValue - 1].cchName);
            return a_pDst->assignNoThrow(paEntries[m_iEnumValue - 1].pszName, paEntries[m_iEnumValue - 1].cchName);
        }
        if (a_fFlags & kToString_Append)
            return a_pDst->appendNoThrow(m_strValue);
        return a_pDst->assignNoThrow(m_strValue);
    }
    if (a_fFlags & kToString_Append)
        return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
    return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestStringEnumBase::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                                      uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    int iEnumValue = stringToEnum(a_rValue);
    if (iEnumValue > 0)
    {
        m_iEnumValue = iEnumValue;
        m_strValue.setNull();
        return VINF_SUCCESS;
    }

    /* No translation.  Check for null... */
    m_iEnumValue = 0;
    if (a_rValue.startsWithWord("null", RTCString::CaseInsensitive))
    {
        m_strValue.setNull();
        setNull();
        return VINF_SUCCESS;
    }

    /* Try copy the string. */
    int rc = m_strValue.assignNoThrow(a_rValue);
    if (RT_SUCCESS(rc))
        return VWRN_NOT_FOUND;

    RT_NOREF(a_pszName, a_pErrInfo, a_fFlags);
    return rc;
}


RTCRestObjectBase::kTypeClass RTCRestStringEnumBase::typeClass(void) const
{
    return kTypeClass_StringEnum;
}


int RTCRestStringEnumBase::setByString(const char *a_pszValue, size_t a_cchValue /*= RTSTR_MAX*/)
{
    if (a_cchValue == RTSTR_MAX)
        a_cchValue = strlen(a_pszValue);
    int iEnumValue = stringToEnum(a_pszValue, a_cchValue);
    if (iEnumValue > 0)
    {
        m_iEnumValue = iEnumValue;
        m_strValue.setNull();
        return VINF_SUCCESS;
    }

    /* No translation. */
    m_iEnumValue = 0;
    int rc = m_strValue.assignNoThrow(a_pszValue, a_cchValue);
    if (RT_SUCCESS(rc))
        return VWRN_NOT_FOUND;
    return rc;
}


int RTCRestStringEnumBase::setByString(RTCString const &a_rValue)
{
    return setByString(a_rValue.c_str(), a_rValue.length());
}


const char *RTCRestStringEnumBase::getString() const
{
    /* We ASSUME a certain mapping table layout here.  */
    if (m_iEnumValue > 0)
    {
        size_t              cEntries  = 0;
        ENUMMAPENTRY const *paEntries = getMappingTable(&cEntries);
        AssertReturn((unsigned)(m_iEnumValue - 1) < cEntries, "<internal-error-#1>");
        Assert(paEntries[m_iEnumValue - 1].iValue == m_iEnumValue);
        return paEntries[m_iEnumValue - 1].pszName;
    }

    AssertReturn(m_iEnumValue == 0, "<internal-error-#2>");
    if (m_strValue.isEmpty())
        return "invalid";

    return m_strValue.c_str();
}


int RTCRestStringEnumBase::stringToEnum(const char *a_pszValue, size_t a_cchValue)
{
    if (a_cchValue == RTSTR_MAX)
        a_cchValue = strlen(a_pszValue);

    size_t              cEntries  = 0;
    ENUMMAPENTRY const *paEntries = getMappingTable(&cEntries);
    for (size_t i = 0; i < cEntries; i++)
        if (   paEntries[i].cchName == a_cchValue
            && memcmp(paEntries[i].pszName, a_pszValue, a_cchValue) == 0)
            return paEntries[i].iValue;
    return 0;
}


int RTCRestStringEnumBase::stringToEnum(RTCString const &a_rStrValue)
{
    return stringToEnum(a_rStrValue.c_str(), a_rStrValue.length());
}


const char *RTCRestStringEnumBase::enumToString(int a_iEnumValue, size_t *a_pcchString)
{
    /* We ASSUME a certain mapping table layout here.  */
    if (a_iEnumValue > 0)
    {
        size_t              cEntries  = 0;
        ENUMMAPENTRY const *paEntries = getMappingTable(&cEntries);
        if ((unsigned)(a_iEnumValue - 1) < cEntries)
        {
            Assert(paEntries[a_iEnumValue - 1].iValue == a_iEnumValue);
            if (a_pcchString)
                *a_pcchString = paEntries[a_iEnumValue - 1].cchName;
            return paEntries[a_iEnumValue - 1].pszName;
        }
    }
    /* Zero is the special invalid value. */
    else if (a_iEnumValue == 0)
    {
        if (a_pcchString)
            *a_pcchString = 7;
        return "invalid";
    }
    return NULL;
}


bool RTCRestStringEnumBase::setWorker(int a_iEnumValue)
{
    /* We ASSUME a certain mapping table layout here.  */
    if (a_iEnumValue > 0)
    {
        size_t              cEntries  = 0;
        ENUMMAPENTRY const *paEntries = getMappingTable(&cEntries);
        AssertReturn((unsigned)(a_iEnumValue - 1) < cEntries, false);
        Assert(paEntries[a_iEnumValue - 1].iValue == a_iEnumValue);
        RT_NOREF(paEntries);
    }
    /* Zero is the special invalid value. */
    else if (a_iEnumValue != 0)
        AssertFailedReturn(false);

    m_iEnumValue = a_iEnumValue;
    m_strValue.setNull();
    return true;
}

