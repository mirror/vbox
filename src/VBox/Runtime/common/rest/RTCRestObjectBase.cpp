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



/*static*/ int
RTCRestObjectBase::deserialize_RTCString_FromJson(RTCRestJsonCursor const &a_rCursor, RTCString *a_pDst)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_STRING)
    {
        const char  *pszValue = RTJsonValueGetString(a_rCursor.m_hValue);
        const size_t cchValue = strlen(pszValue);
        int rc = a_pDst->reserveNoThrow(cchValue + 1);
        if (RT_SUCCESS(rc))
        {
            *a_pDst = pszValue;
            return VINF_SUCCESS;
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "no memory for %zu char long string", cchValue);
    }

    if (enmType == RTJSONVALTYPE_NULL) /** @todo RTJSONVALTYPE_NULL for strings??? */
    {
        a_pDst->setNull();
        return VINF_SUCCESS;
    }

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for string",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


/*static*/ int
RTCRestObjectBase::deserialize_int64_t_FromJson(RTCRestJsonCursor const &a_rCursor, int64_t *a_piDst)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
    {
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, a_piDst);
        if (RT_SUCCESS(rc))
            return rc;
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        *a_piDst = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        *a_piDst = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 64-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


/*static*/ int
RTCRestObjectBase::deserialize_int32_t_FromJson(RTCRestJsonCursor const &a_rCursor, int32_t *a_piDst)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
    {
        int64_t iTmp = *a_piDst;
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &iTmp);
        if (RT_SUCCESS(rc))
        {
            *a_piDst = (int32_t)iTmp;
            if (*a_piDst == iTmp)
                return rc;
            return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_OUT_OF_RANGE, "value %RI64 does not fit in 32 bits", iTmp);
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        *a_piDst = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        *a_piDst = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 32-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


/*static*/ int
RTCRestObjectBase::deserialize_int16_t_FromJson(RTCRestJsonCursor const &a_rCursor, int16_t *a_piDst)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    if (enmType == RTJSONVALTYPE_NUMBER)
    {
        int64_t iTmp = *a_piDst;
        int rc = RTJsonValueQueryInteger(a_rCursor.m_hValue, &iTmp);
        if (RT_SUCCESS(rc))
        {
            *a_piDst = (int16_t)iTmp;
            if (*a_piDst == iTmp)
                return rc;
            return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_OUT_OF_RANGE, "value %RI64 does not fit in 16 bits", iTmp);
        }
        return a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonValueQueryInteger failed with %Rrc", rc);
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL || enmType == RTJSONVALTYPE_FALSE)
        *a_piDst = 0;
    else if (enmType == RTJSONVALTYPE_TRUE)
        *a_piDst = 1;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for 16-bit integer",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


/*static*/ int
RTCRestObjectBase::deserialize_bool_FromJson(RTCRestJsonCursor const &a_rCursor, bool *pfDst)
{
    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);

    if (enmType == RTJSONVALTYPE_TRUE)
    {
        *pfDst = true;
        return VINF_SUCCESS;
    }

    if (enmType == RTJSONVALTYPE_FALSE)
    {
        *pfDst = false;
        return VINF_SUCCESS;
    }

    /* This is probably non-sense... */
    if (enmType == RTJSONVALTYPE_NULL)
        *pfDst = false;

    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "wrong JSON type %s for boolean",
                                          RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
}


/*static*/ int
RTCRestObjectBase::deserialize_double_FromJson(RTCRestJsonCursor const &a_rCursor, double *a_prdDst)
{
    AssertMsgFailed(("RTJson needs to be taught double values."));
    RT_NOREF(a_prdDst);
    /** @todo implement floating point values for json. */
    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_NOT_IMPLEMENTED, "double is not supported yet");
}


/*static*/ RTCRestOutputBase &
RTCRestObjectBase::serialize_double_AsJson(RTCRestOutputBase &a_rDst, double const *a_prdValue)
{
    /* Just a simple approximation here. */
    /** @todo implement floating point values for json. */
    double const rdValue = *a_prdValue;
    char szTmp[128];
#ifdef _MSC_VER
    _snprintf(szTmp, sizeof(szTmp), "%g", rdValue);
#else
    snprintf(szTmp, sizeof(szTmp), "%g", rdValue);
#endif
    a_rDst.printf("%s", szTmp);
    return a_rDst;
}

