//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Modifications (c) 2009 Sun Microsystems, Inc.
//

#ifndef ___dll_h
#define ___dll_h

#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/log.h>

#include <VBox/Log.h>

extern HINSTANCE g_hinst;

LONG DllAddRef();
LONG DllRelease();

extern HRESULT VBoxCredProv_CreateInstance(REFIID riid, void** ppv);

class CClassFactory : public IClassFactory
{
    public:
        // IUnknown
        STDMETHOD_(ULONG, AddRef)()
        {
            return _cRef++;
        }

        STDMETHOD_(ULONG, Release)()
        {
            LONG cRef = _cRef--;
            if (!cRef)
            {
                delete this;
            }
            return cRef;
        }

        STDMETHOD (QueryInterface)(REFIID riid, void** ppv)
        {
            HRESULT hr;
            if (ppv != NULL)
            {
                if (IID_IClassFactory == riid || IID_IUnknown == riid)
                {
                    *ppv = static_cast<IUnknown*>(this);
                    reinterpret_cast<IUnknown*>(*ppv)->AddRef();
                    hr = S_OK;
                }
                else
                {
                    *ppv = NULL;
                    hr = E_NOINTERFACE;
                }
            }
            else
            {
                hr = E_INVALIDARG;
            }
            return hr;
        }

        // IClassFactory
        STDMETHOD (CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv)
        {
            HRESULT hr;
            if (!pUnkOuter)
            {
                hr = VBoxCredProv_CreateInstance(riid, ppv);
            }
            else
            {
                hr = CLASS_E_NOAGGREGATION;
            }
            return hr;
        }

        STDMETHOD (LockServer)(BOOL bLock)
        {
            if (bLock)
            {
                DllAddRef();
            }
            else
            {
                DllRelease();
            }
            return S_OK;
        }

    private:

        CClassFactory() : _cRef(1) {}
        ~CClassFactory(){}

    private:

        LONG _cRef;
        friend HRESULT CClassFactory_CreateInstance(REFCLSID rclsid, REFIID riid, void** ppv);
};

#endif /* ___dll_h */
