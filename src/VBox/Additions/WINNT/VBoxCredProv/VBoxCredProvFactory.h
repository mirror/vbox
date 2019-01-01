/* $Id$ */
/** @file
 * VBoxCredentialProvFactory - The VirtualBox Credential Provider Factory.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvFactory_h
#define GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvFactory_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>

class VBoxCredProvFactory : public IClassFactory
{
private:
    /*
     * Make the constructors / destructors private so that
     * this class cannot be instanciated directly by non-friends.
     */
    VBoxCredProvFactory(void);
    virtual ~VBoxCredProvFactory(void);

public:
    /** @name IUnknown methods.
     * @{ */
    IFACEMETHODIMP_(ULONG) AddRef(void);
    IFACEMETHODIMP_(ULONG) Release(void);
    IFACEMETHODIMP         QueryInterface(REFIID interfaceID, void **ppvInterface);
    /** @} */

    /** @name IClassFactory methods.
     * @{ */
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID interfaceID, void **ppvInterface);
    IFACEMETHODIMP LockServer(BOOL fLock);
    /** @} */

private:
    LONG m_cRefs;
    friend HRESULT VBoxCredentialProviderCreate(REFCLSID classID, REFIID interfaceID, void **ppvInterface);
};
#endif /* !GA_INCLUDED_SRC_WINNT_VBoxCredProv_VBoxCredProvFactory_h */

