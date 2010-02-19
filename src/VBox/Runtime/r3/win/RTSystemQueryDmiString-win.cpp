/* $Id$ */
/** @file
 * IPRT - RTSystemQueryDmiString, windows ring-3.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>


HRESULT rtSystemInitializeDmiLookup()
{
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        hr = CoInitializeSecurity(NULL, 
                                  -1,                          /* COM authentication. */
                                  NULL,                        /* Which authentication services. */
                                  NULL,                        /* Reserved. */
                                  RPC_C_AUTHN_LEVEL_DEFAULT,   /* Default authentication. */ 
                                  RPC_C_IMP_LEVEL_IMPERSONATE, /* Default impersonation. */
                                  NULL,                        /* Authentication info. */
                                  EOAC_NONE,                   /* Additional capabilities. */
                                  NULL);                       /* Reserved. */
    }
    return hr;
}


void rtSystemShutdownDmiLookup()
{
    CoUninitialize();
}


HRESULT rtSystemConnectToDmiServer(IWbemLocator *pLocator, const char *pszServer, IWbemServices **ppServices)
{
    AssertPtr(pLocator);
    AssertPtrNull(pszServer);
    AssertPtr(ppServices);

    HRESULT hr = pLocator->ConnectServer(_bstr_t(TEXT(pszServer)),         
                                         NULL,                    
                                         NULL,                   
                                         0,                      
                                         NULL,                   
                                         0,                      
                                         0,                      
                                         ppServices);
    if (SUCCEEDED(hr))
    {
        hr = CoSetProxyBlanket(*ppServices,                  
                               RPC_C_AUTHN_WINNT,          
                               RPC_C_AUTHZ_NONE,            
                               NULL,                        
                               RPC_C_AUTHN_LEVEL_CALL,     
                               RPC_C_IMP_LEVEL_IMPERSONATE,
                               NULL,                       
                               EOAC_NONE);             
    }
    return hr;
}


RTDECL(int) RTSystemQueryDmiString(RTSYSDMISTR enmString, char *pszBuf, size_t cbBuf)
{
    AssertPtrReturn(pszBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf > 0, VERR_INVALID_PARAMETER);
    *pszBuf = '\0';
    AssertReturn(enmString > RTSYSDMISTR_INVALID && enmString < RTSYSDMISTR_END, VERR_INVALID_PARAMETER);

    HRESULT hr = rtSystemInitializeDmiLookup();
    if (FAILED(hr))
        return VERR_NOT_SUPPORTED;

    IWbemLocator *pLoc;
    hr = CoCreateInstance(CLSID_WbemLocator,             
                          0, 
                          CLSCTX_INPROC_SERVER, 
                          IID_IWbemLocator, (LPVOID *)&pLoc);
    int rc = VINF_SUCCESS;
    if (SUCCEEDED(hr))
    {
        IWbemServices *pServices;
        hr = rtSystemConnectToDmiServer(pLoc, "ROOT\\CIMV2", &pServices);
        if (SUCCEEDED(hr))
        {
            IEnumWbemClassObject *pEnum;
            hr = pServices->CreateInstanceEnum(L"Win32_ComputerSystemProduct", 0, NULL, &pEnum);
            if (SUCCEEDED(hr))
            {
                IWbemClassObject *pObj;
                ULONG uCount;
    
                do
                {
                    hr = pEnum->Next(WBEM_INFINITE,
                                     1,
                                     &pObj,
                                     &uCount);            
                    if (   SUCCEEDED(hr)
                        && uCount > 0)
                    {
                        const char *pszPropName;
                        switch (enmString)
                        {
                            case RTSYSDMISTR_PRODUCT_NAME:      pszPropName = "Name"; break;
                            case RTSYSDMISTR_PRODUCT_VERSION:   pszPropName = "Version"; break;
                            case RTSYSDMISTR_PRODUCT_UUID:      pszPropName = "UUID"; break;
                            case RTSYSDMISTR_PRODUCT_SERIAL:    pszPropName = "IdentifyingNumber"; break;
                            default:
                                rc = VERR_NOT_SUPPORTED;
                        }
        
                        if (RT_SUCCESS(rc))
                        {
                            _variant_t v;
                            hr = pObj->Get(_bstr_t(TEXT(pszPropName)), 0, &v, 0, 0);
                            if (   SUCCEEDED(hr)
                                && V_VT(&v) == VT_BSTR)
                            {
                                RTStrPrintf(pszBuf, cbBuf, "%s", (char*)_bstr_t(v.bstrVal));
                                VariantClear(&v);
                            }
                        }
                        pObj->Release();
                    }
                } while(hr != WBEM_S_FALSE);
                pEnum->Release();
            }
            pServices->Release();
        }
        pLoc->Release();
    }

    rtSystemShutdownDmiLookup();
    if (FAILED(hr))
        rc = VERR_NOT_FOUND; /** @todo Find a better error, since neither of the RTErrConvert* can do the conversion here. */
    return rc;
}
RT_EXPORT_SYMBOL(RTSystemQueryDmiString);


