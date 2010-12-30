/** @file
 *
 *  COM Events Helper routines.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxComEvents.h"
// for IIDs
#include "VirtualBoxImpl.h"

ComEventsHelper::ComEventsHelper()
{
}

ComEventsHelper::~ComEventsHelper()
{
}

HRESULT ComEventsHelper::init(const com::Guid &aGuid)
{
    HRESULT            hr = 0;
    CComPtr<ITypeLib>  ptlib;
    CComPtr<ITypeInfo> ptinfo;
    int i;


    hr = ::LoadRegTypeLib(LIBID_VirtualBox, kTypeLibraryMajorVersion, kTypeLibraryMinorVersion, LOCALE_SYSTEM_DEFAULT, &ptlib);
    if (FAILED(hr))
        return hr;

    hr = ptlib->GetTypeInfoOfGuid(aGuid.ref(), &ptinfo);
    if (FAILED(hr))
        return hr;

    CComTypeAttr       ptypeattr(ptinfo);
    hr = ptinfo->GetTypeAttr(&ptypeattr);
    if (FAILED(hr))
        return hr;

    int cFuncs = ptypeattr->cFuncs;

    for (i=0; i<cFuncs; i++)
    {
        CComFuncDesc  pfuncdesc(ptinfo);
        DWORD         hContext; // help context
        BSTR          fName;

        hr = ptinfo->GetFuncDesc(i, &pfuncdesc);
        if (FAILED(hr))
           break;

        hr = ptinfo->GetDocumentation(pfuncdesc->memid, &fName, NULL, &hContext, NULL);
        if (FAILED(hr))
           break;

        /* We only allow firing event callbacks */
        if (_wcsnicmp(fName, L"On", 2) == 0)
        {
            DISPID did;

            hr = ::DispGetIDsOfNames(ptinfo, &fName, 1, &did);
            evMap.insert(ComEventsMap::value_type(com::Utf8Str(fName), did));

        }
        SysFreeString(fName);

        pfuncdesc.Release();
    }

   return hr;
}

HRESULT ComEventsHelper::lookup(com::Utf8Str &aName, DISPID *did)
{
   ComEventsMap::const_iterator it = evMap.find(aName);

   if (it != evMap.end())
   {
       *did = it->second;
       return S_OK;
   }
   else
   {
       *did = 0;
       return VBOX_E_OBJECT_NOT_FOUND;
   }
}


HRESULT ComEventsHelper::fire(IDispatch* aObj, ComEventDesc& event, CComVariant* result)
{
     int argc = event.mArgc;
     CComVariant* args = event.mArgs;
     DISPPARAMS disp = { args, NULL, argc, 0};
     DISPID           dispid;

     HRESULT          hr = lookup(event.mName, &dispid);

     if (FAILED(hr))
         return hr;

     hr = aObj->Invoke(dispid, IID_NULL,
                       LOCALE_USER_DEFAULT,
                       DISPATCH_METHOD,
                       &disp, result,
                       NULL, NULL);

     return hr;
}
