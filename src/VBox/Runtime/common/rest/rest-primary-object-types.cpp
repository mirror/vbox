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
{
}


/** Destructor. */
RTCRestObjectBase::~RTCRestObjectBase()
{
    /* nothing to do */
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


/*********************************************************************************************************************************
*   RTCRestBool implementation                                                                                                   *
*********************************************************************************************************************************/

/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestBool::createInstance(void)
{
    return new (std::nothrow) RTCRestBool();
}


/** Default destructor. */
RTCRestBool::RTCRestBool()
    : m_fValue(false)
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
    m_fValue = a_rThat.m_fValue;
    return *this;
}


int RTCRestBool::assignCopy(RTCRestBool const &a_rThat)
{
    m_fValue = a_rThat.m_fValue;
    return VINF_SUCCESS;
}


int RTCRestBool::resetToDefault()
{
    m_fValue = false;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestBool::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf(m_fValue ? "true" : "false");
    return a_rDst;
}


int RTCRestBool::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);

    if (enmType == RTJSONVALTYPE_TRUE)
    {
        m_fValue = true;
        return VINF_SUCCESS;
    }

    if (enmType == RTJSONVALTYPE_FALSE)
    {
        m_fValue = false;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL)
        m_fValue = false;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for boolean",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestBool::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
    {
        if (m_fValue)
            return a_pDst->assignNoThrow(RT_STR_TUPLE("true"));
        return a_pDst->assignNoThrow(RT_STR_TUPLE("false"));
    }
    if (m_fValue)
        return a_pDst->appendNoThrow(RT_STR_TUPLE("true"));
    return a_pDst->appendNoThrow(RT_STR_TUPLE("false"));
}


int RTCRestBool::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                            uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    if (a_rValue.startsWithWord("true", RTCString::CaseInsensitive))
        m_fValue = true;
    else if (a_rValue.startsWithWord("false", RTCString::CaseInsensitive))
        m_fValue = false;
    else
        return RTErrInfoSetF(a_pErrInfo, VERR_INVALID_PARAMETER, "%s: unable to parse '%s' as bool", a_pszName, a_rValue.c_str());
    return VINF_SUCCESS;
}


const char *RTCRestBool::getType()
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
    : m_iValue(0)
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
    : m_iValue(iValue)
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
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt64::assignCopy(RTCRestInt64 const &a_rThat)
{
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


int RTCRestInt64::resetToDefault()
{
    m_iValue = 0;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestInt64::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("%RI64", m_iValue);
    return a_rDst;
}


int RTCRestInt64::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
    {
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &m_iValue);
        if (RT_SUCCESS(rc))
            return rc;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        m_iValue = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 64-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt64::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
        return a_pDst->printfNoThrow("%RI64", m_iValue);
    return a_pDst->appendPrintfNoThrow("%RI64", m_iValue);
}


int RTCRestInt64::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    int rc = RTStrToInt64Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;
    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int64_t", a_pszName, rc, a_rValue.c_str());
}


const char *RTCRestInt64::getType()
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
    : m_iValue(0)
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
    : m_iValue(iValue)
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
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt32::assignCopy(RTCRestInt32 const &a_rThat)
{
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


int RTCRestInt32::resetToDefault()
{
    m_iValue = 0;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestInt32::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("%RI32", m_iValue);
    return a_rDst;
}


int RTCRestInt32::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
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

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        m_iValue = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 32-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt32::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
        return a_pDst->printfNoThrow("%RI32", m_iValue);
    return a_pDst->appendPrintfNoThrow("%RI32", m_iValue);
}


int RTCRestInt32::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    int rc = RTStrToInt32Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;
    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int32_t", a_pszName, rc, a_rValue.c_str());
}


const char *RTCRestInt32::getType()
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
    : m_iValue(0)
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
    : m_iValue(iValue)
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
    m_iValue = a_rThat.m_iValue;
    return *this;
}


int RTCRestInt16::assignCopy(RTCRestInt16 const &a_rThat)
{
    m_iValue = a_rThat.m_iValue;
    return VINF_SUCCESS;
}


int RTCRestInt16::resetToDefault()
{
    m_iValue = 0;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestInt16::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("%RI16", m_iValue);
    return a_rDst;
}


int RTCRestInt16::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
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

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        m_iValue = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        m_iValue = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 16-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestInt16::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
        return a_pDst->printfNoThrow("%RI16", m_iValue);
    return a_pDst->appendPrintfNoThrow("%RI16", m_iValue);
}


int RTCRestInt16::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                             uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    int rc = RTStrToInt16Full(RTStrStripL(a_rValue.c_str()), 10, &m_iValue);
    if (rc == VINF_SUCCESS || rc == VERR_TRAILING_SPACES)
        return VINF_SUCCESS;
    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as int16_t", a_pszName, rc, a_rValue.c_str());
}


const char *RTCRestInt16::getType()
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
    : m_rdValue(0.0)
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
    : m_rdValue(rdValue)
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
    m_rdValue = a_rThat.m_rdValue;
    return *this;
}


int RTCRestDouble::assignCopy(RTCRestDouble const &a_rThat)
{
    m_rdValue = a_rThat.m_rdValue;
    return VINF_SUCCESS;
}


int RTCRestDouble::resetToDefault()
{
    m_rdValue = 0.0;
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestDouble::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    /* Just a simple approximation here. */
    /** @todo implement floating point values for json. */
    char szValue[128];
#ifdef _MSC_VER
    _snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#else
    snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#endif
    a_rDst.printf("%s", szValue);
    return a_rDst;
}


int RTCRestDouble::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    AssertMsgFailed(("RTJson needs to be taught double values."));
    /** @todo implement floating point values for json. */
    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_NOT_IMPLEMENTED, "double is not supported yet");
}


int RTCRestDouble::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    /* Just a simple approximation here. */
    /** @todo implement floating point values for json. */
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


int RTCRestDouble::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                              uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags);

    const char *pszValue = RTStrStripL(a_rValue.c_str());
    errno = 0;
    char *pszNext = NULL;
    m_rdValue = strtod(pszValue, &pszNext);
    if (errno == 0)
        return VINF_SUCCESS;
    int rc = RTErrConvertFromErrno(errno);
    return RTErrInfoSetF(a_pErrInfo, rc, "%s: error %Rrc parsing '%s' as double", a_pszName, rc, a_rValue.c_str());
}


const char *RTCRestDouble::getType()
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
{
}


/** Destructor. */
RTCRestString::~RTCRestString()
{
    /* nothing to do */
}


int RTCRestString::assignCopy(RTCString const &a_rThat)
{
    return assignNoThrow(a_rThat);
}


int RTCRestString::assignCopy(const char *a_pszThat)
{
    return assignNoThrow(a_pszThat);
}


int RTCRestString::resetToDefault()
{
    setNull();
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestString::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("%RMjs", m_psz);
    return a_rDst;
}


int RTCRestString::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
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

    if (enmType == RTJSONVALTYPE_NULL) /** @todo RTJSONVALTYPE_NULL for strings??? */
    {
        setNull();
        return VINF_SUCCESS;
    }

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for string",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


int RTCRestString::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (!(a_fFlags & kToString_Append))
        return a_pDst->assignNoThrow(*this);
    return a_pDst->appendNoThrow(*this);
}


int RTCRestString::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                              uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_fFlags); RT_NOREF(a_pszName); RT_NOREF(a_pErrInfo);

    return assignNoThrow(a_rValue);
}


const char *RTCRestString::getType()
{
    return "RTCString";
}

