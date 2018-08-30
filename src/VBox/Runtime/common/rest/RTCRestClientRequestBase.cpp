/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientRequestBase implementation.
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

#include <iprt/assert.h>
#include <iprt/err.h>


/**
 * Default constructor.
 */
RTCRestClientRequestBase::RTCRestClientRequestBase()
    : m_fIsSet(0)
    , m_fErrorSet(0)
{
}


/**
 * Copy constructor.
 */
RTCRestClientRequestBase::RTCRestClientRequestBase(RTCRestClientRequestBase const &a_rThat)
    : m_fIsSet(a_rThat.m_fIsSet)
    , m_fErrorSet(a_rThat.m_fErrorSet)
{
}


/**
 * Destructor
 */
RTCRestClientRequestBase::~RTCRestClientRequestBase()
{
    /* nothing to do */
}


/**
 * Copy assignment operator.
 */
RTCRestClientRequestBase &RTCRestClientRequestBase::operator=(RTCRestClientRequestBase const &a_rThat)
{
    m_fIsSet    = a_rThat.m_fIsSet;
    m_fErrorSet = a_rThat.m_fErrorSet;
    return *this;
}


int RTCRestClientRequestBase::doPathParameters(RTCString *a_pStrPath, const char *a_pszPathTemplate, size_t a_cchPathTemplate,
                                               PATHREPLACEENTRY *a_paPathParams, size_t a_cPathParams) const
{
    int rc = a_pStrPath->assignNoThrow(a_pszPathTemplate, a_cchPathTemplate);
    AssertRCReturn(rc, rc);

    /* Locate the sub-string to replace with values first: */
    for (size_t i = 0; i < a_cPathParams; i++)
    {
        char const *psz = strstr(a_pszPathTemplate, a_paPathParams[i].pszName);
        AssertReturn(psz, VERR_INTERNAL_ERROR_5);
        a_paPathParams[i].offName = psz - a_pszPathTemplate;
    }

    /* Replace with actual values: */
    RTCString strTmpVal;
    for (size_t i = 0; i < a_cPathParams; i++)
    {
        rc = a_paPathParams[i].pObj->toString(&strTmpVal, a_paPathParams[i].fFlags);
        AssertRCReturn(rc, rc);

        /* Replace. */
        ssize_t cchAdjust = strTmpVal.length() - a_paPathParams[i].cchName;
        rc = a_pStrPath->replaceNoThrow(a_paPathParams[i].offName, a_paPathParams[i].cchName, strTmpVal);
        AssertRCReturn(rc, rc);

        /* Adjust subsequent fields. */
        if (cchAdjust != 0)
            for (size_t j = i + 1; j < a_cPathParams; j++)
                if (a_paPathParams[j].offName > a_paPathParams[i].offName)
                    a_paPathParams[j].offName += cchAdjust;
    }

    return VINF_SUCCESS;
}

