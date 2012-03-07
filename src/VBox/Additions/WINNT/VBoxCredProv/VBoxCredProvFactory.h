/* $Id$ */
/** @file
 * VBoxCredentialProvFactory - The VirtualBox Credential Provider factory.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBOX_CREDPROV_FACTORY_H__
#define __VBOX_CREDPROV_FACTORY_H__

#include <windows.h>

class VBoxCredProvFactory : public IClassFactory
{
    /**
     * Make the constructors / destructors private so that
     * this class cannot be instanciated directly by non-friends.
     */
    private:

        VBoxCredProvFactory(void);

        virtual ~VBoxCredProvFactory(void);

    public: /* IUknown overrides. */

        IFACEMETHODIMP_(ULONG) AddRef(void);
        IFACEMETHODIMP_(ULONG) Release(void);
        IFACEMETHODIMP         QueryInterface(REFIID interfaceID, void **ppvInterface);

    public: /* IClassFactory overrides. */

        IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter,
                                     REFIID    interfaceID, void **ppvInterface);
        IFACEMETHODIMP LockServer(BOOL bLock);

    private:

        LONG m_cRefCount;
        friend HRESULT VBoxCredentialProviderCreate(REFCLSID rclsid, REFIID riid, void** ppv);
};
#endif /* !__VBOX_CREDPROV_FACTORY_H__ */

