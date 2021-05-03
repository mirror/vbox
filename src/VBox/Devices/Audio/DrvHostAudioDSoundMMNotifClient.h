/* $Id$ */
/** @file
 * Host audio driver - DSound - Implementation of the IMMNotificationClient interface to detect audio endpoint changes.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_DrvHostAudioDSoundMMNotifClient_h
#define VBOX_INCLUDED_SRC_Audio_DrvHostAudioDSoundMMNotifClient_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/win/windows.h>

/* Should fix warning in include\ks.h. */
#ifndef _WIN64
# ifdef RT_ARCH_X86
#  define _WIN64 1
# else
#  define _WIN64 0
# endif
#endif

#include <Mmdeviceapi.h>

#include <VBox/vmm/pdmaudioifs.h>


class DrvHostAudioDSoundMMNotifClient : public IMMNotificationClient
{
public:

    DrvHostAudioDSoundMMNotifClient(PPDMIHOSTAUDIOPORT pInterface);
    virtual ~DrvHostAudioDSoundMMNotifClient();

    HRESULT Initialize();

    HRESULT Register(void);
    void    Unregister(void);

    /** @name IUnknown interface
     * @{ */
    IFACEMETHODIMP_(ULONG) Release();
    /** @} */

private:

    bool                        m_fRegisteredClient;
    IMMDeviceEnumerator        *m_pEnum;
    IMMDevice                  *m_pEndpoint;

    long                        m_cRef;

    PPDMIHOSTAUDIOPORT           m_pIAudioNotifyFromHost;

    HRESULT AttachToDefaultEndpoint();
    void    DetachFromEndpoint();

    void    doCallback(void);

    /** @name IMMNotificationClient interface
     * @{ */
    IFACEMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);
    IFACEMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId);
    IFACEMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId);
    IFACEMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);
    IFACEMETHODIMP OnPropertyValueChanged(LPCWSTR /*pwstrDeviceId*/, const PROPERTYKEY /*key*/) { return S_OK; }
    /** @} */

    /** @name IUnknown interface
     * @{ */
    IFACEMETHODIMP QueryInterface(const IID& iid, void** ppUnk);
    IFACEMETHODIMP_(ULONG) AddRef();
    /** @} */
};
#endif /* !VBOX_INCLUDED_SRC_Audio_DrvHostAudioDSoundMMNotifClient_h */

