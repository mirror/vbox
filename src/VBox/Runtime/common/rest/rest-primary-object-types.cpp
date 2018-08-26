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
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/string.h>

#include <stdio.h>




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


int RTCRestObjectBase::toString(RTCString *a_pDst, uint32_t fFlags)
{
    Assert(fFlags == 0);
    RT_NOREF(fFlags);
    /*
     * Just wrap the JSON serialization method.
     */
    RTCRestOutputToString Tmp(a_pDst);
    serializeAsJson(Tmp);
    return Tmp.finalize() ? VINF_SUCCESS : VERR_NO_MEMORY;
}


RTCString RTCRestObjectBase::toString()
{
    RTCString strRet;
    toString(&strRet, 0);
    return strRet;
}



/*********************************************************************************************************************************
*   RTCRestBool implementation                                                                                                   *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestBool::RTCRestBool()
    : m_fValue(false)
{
}


/** Copy constructor. */
RTCRestBool::RTCRestBool(RTCRestBool const &a_rThat)
    : m_fValue(a_rThat.m_fValue)
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


void RTCRestBool::resetToDefault()
{
    m_fValue = false;
}


RTCRestOutputBase &RTCRestBool::serializeAsJson(RTCRestOutputBase &a_rDst)
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


int RTCRestBool::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Be a little careful here to avoid throwing anything. */
    int rc = a_pDst->reserveNoThrow(m_fValue ? sizeof("true") : sizeof("false"));
    if (RT_SUCCESS(rc))
    {
        if (m_fValue)
            a_pDst->assign(RT_STR_TUPLE("true"));
        else
            a_pDst->assign(RT_STR_TUPLE("false"));
    }
    return rc;
}


const char *RTCRestBool::getType()
{
    return "bool";
}


/*********************************************************************************************************************************
*   RTCRestInt64 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestInt64::RTCRestInt64()
    : m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt64::RTCRestInt64(RTCRestInt64 const &a_rThat)
    : m_iValue(a_rThat.m_iValue)
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


void RTCRestInt64::resetToDefault()
{
    m_iValue = 0;
}


RTCRestOutputBase &RTCRestInt64::serializeAsJson(RTCRestOutputBase &a_rDst)
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


int RTCRestInt64::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Be a little careful here to avoid throwing anything. */
    char   szValue[64];
    size_t cchValue = RTStrPrintf(szValue, sizeof(szValue), "%RI64", m_iValue);
    int rc = a_pDst->reserveNoThrow(cchValue + 1);
    if (RT_SUCCESS(rc))
        a_pDst->assign(szValue, cchValue);
    return rc;
}


const char *RTCRestInt64::getType()
{
    return "int64_t";
}



/*********************************************************************************************************************************
*   RTCRestInt32 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestInt32::RTCRestInt32()
    : m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt32::RTCRestInt32(RTCRestInt32 const &a_rThat)
    : m_iValue(a_rThat.m_iValue)
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


void RTCRestInt32::resetToDefault()
{
    m_iValue = 0;
}


RTCRestOutputBase &RTCRestInt32::serializeAsJson(RTCRestOutputBase &a_rDst)
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


int RTCRestInt32::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Be a little careful here to avoid throwing anything. */
    char   szValue[16];
    size_t cchValue = RTStrPrintf(szValue, sizeof(szValue), "%RI32", m_iValue);
    int rc = a_pDst->reserveNoThrow(cchValue + 1);
    if (RT_SUCCESS(rc))
        a_pDst->assign(szValue, cchValue);
    return rc;
}


const char *RTCRestInt32::getType()
{
    return "int32_t";
}



/*********************************************************************************************************************************
*   RTCRestInt16 implementation                                                                                                  *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestInt16::RTCRestInt16()
    : m_iValue(0)
{
}


/** Copy constructor. */
RTCRestInt16::RTCRestInt16(RTCRestInt16 const &a_rThat)
    : m_iValue(a_rThat.m_iValue)
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


void RTCRestInt16::resetToDefault()
{
    m_iValue = 0;
}


RTCRestOutputBase &RTCRestInt16::serializeAsJson(RTCRestOutputBase &a_rDst)
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


int RTCRestInt16::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Be a little careful here to avoid throwing anything. */
    char   szValue[8];
    size_t cchValue = RTStrPrintf(szValue, sizeof(szValue), "%RI16", m_iValue);
    int rc = a_pDst->reserveNoThrow(cchValue + 1);
    if (RT_SUCCESS(rc))
        a_pDst->assign(szValue, cchValue);
    return rc;
}


const char *RTCRestInt16::getType()
{
    return "int16_t";
}



/*********************************************************************************************************************************
*   RTCRestDouble implementation                                                                                                 *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestDouble::RTCRestDouble()
    : m_rdValue(0.0)
{
}


/** Copy constructor. */
RTCRestDouble::RTCRestDouble(RTCRestDouble const &a_rThat)
    : m_rdValue(a_rThat.m_rdValue)
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


void RTCRestDouble::resetToDefault()
{
    m_rdValue = 0.0;
}


RTCRestOutputBase &RTCRestDouble::serializeAsJson(RTCRestOutputBase &a_rDst)
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


int RTCRestDouble::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Just a simple approximation here. */
    /** @todo implement floating point values for json. */
    char szValue[128];
#ifdef _MSC_VER
    _snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#else
    snprintf(szValue, sizeof(szValue), "%g", m_rdValue);
#endif
    size_t const cchValue = strlen(szValue);

    int rc = a_pDst->reserveNoThrow(cchValue + 1);
    if (RT_SUCCESS(rc))
        a_pDst->assign(szValue, cchValue);
    return rc;
}


const char *RTCRestDouble::getType()
{
    return "double";
}



/*********************************************************************************************************************************
*   RTCRestString implementation                                                                                                 *
*********************************************************************************************************************************/

/** Default destructor. */
RTCRestString::RTCRestString()
    : RTCString()
{
}


/** Copy constructor. */
RTCRestString::RTCRestString(RTCRestString const &a_rThat)
    : RTCString(a_rThat)
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


void RTCRestString::resetToDefault()
{
    setNull();
}


RTCRestOutputBase &RTCRestString::serializeAsJson(RTCRestOutputBase &a_rDst)
{
    a_rDst.printf("%RJs", m_psz);
    return a_rDst;
}


int RTCRestString::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_STRING)
    {
        const char  *pszValue = RTJsonValueGetString(a_rCursor.m_hValue);
        const size_t cchValue = strlen(pszValue);
        int rc = reserveNoThrow(cchValue + 1);
        if (RT_SUCCESS(rc))
        {
            assign(pszValue, cchValue);
            return VINF_SUCCESS;
        }
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


int RTCRestString::toString(RTCString *a_pDst, uint32_t fFlags /*= 0*/)
{
    Assert(!fFlags);
    RT_NOREF(fFlags);
    /* Careful as always. */
    if (m_cch)
    {
        int rc = a_pDst->reserveNoThrow(m_cch + 1);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
            return rc;
    }
    a_pDst->assign(*this);
    return VINF_SUCCESS;
}


const char *RTCRestString::getType()
{
    return "RTCString";
}

