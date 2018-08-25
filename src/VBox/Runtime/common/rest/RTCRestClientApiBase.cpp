/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientApiBase implementation.
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
#include <iprt/http.h>


/**
 * The destructor.
 */
RTCRestClientApiBase::~RTCRestClientApiBase()
{
    if (m_hHttp != NIL_RTHTTP)
    {
        int rc = RTHttpDestroy(m_hHttp);
        AssertRC(rc);
        m_hHttp = NIL_RTHTTP;
    }
}


int RTCRestClientApiBase::reinitHttpInstance()
{
    if (m_hHttp != NIL_RTHTTP)
        return RTHttpReset(m_hHttp);

    int rc = RTHttpCreate(&m_hHttp);
    if (RT_FAILURE(rc))
        m_hHttp = NIL_RTHTTP;
    return rc;
}

