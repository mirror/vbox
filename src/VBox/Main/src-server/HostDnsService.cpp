/* $Id$ */
/** @file
 * Base class fo Host DNS & Co services.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>

#include "HostDnsService.h"
#include <iprt/thread.h>
#include <iprt/semaphore.h>

HostDnsService::HostDnsService(){}

HostDnsService::~HostDnsService () 
{
    int rc = RTCritSectDelete(&m_hCritSect);
    AssertRC(rc);
}


HRESULT HostDnsService::init (void) 
{
    int rc = RTCritSectInit(&m_hCritSect);
    AssertRCReturn(rc, E_FAIL);
    return S_OK;
}

HRESULT HostDnsService::start()
{
    return S_OK;
}

void HostDnsService::stop()
{
}

HRESULT HostDnsService::update()
{
    return S_OK;
}

STDMETHODIMP HostDnsService::COMGETTER(NameServers)(ComSafeArrayOut(BSTR, aNameServers))
{
    RTCritSectEnter(&m_hCritSect);
    com::SafeArray<BSTR> nameServers(m_llNameServers.size());

    Utf8StrListIterator it;
    int i = 0;
    for (it = m_llNameServers.begin(); it != m_llNameServers.end(); ++it, ++i)
        (*it).cloneTo(&nameServers[i]);

    nameServers.detachTo(ComSafeArrayOutArg(aNameServers));

    RTCritSectLeave(&m_hCritSect);

    return S_OK;
}


STDMETHODIMP HostDnsService::COMGETTER(DomainName)(BSTR *aDomainName)
{
    RTCritSectEnter(&m_hCritSect);

    m_DomainName.cloneTo(aDomainName);

    RTCritSectLeave(&m_hCritSect);

    return S_OK;
}


STDMETHODIMP HostDnsService::COMGETTER(SearchStrings)(ComSafeArrayOut(BSTR, aSearchStrings))
{
    RTCritSectEnter(&m_hCritSect);

    com::SafeArray<BSTR> searchString(m_llSearchStrings.size());

    Utf8StrListIterator it;
    int i = 0;
    for (it = m_llSearchStrings.begin(); it != m_llSearchStrings.end(); ++it, ++i)
        (*it).cloneTo(&searchString[i]);


    searchString.detachTo(ComSafeArrayOutArg(aSearchStrings));

    RTCritSectLeave(&m_hCritSect);

    return S_OK;
}


