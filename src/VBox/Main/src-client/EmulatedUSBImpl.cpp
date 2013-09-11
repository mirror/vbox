/* $Id$ */
/** @file
 *
 * Emulated USB manager implementation.
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

#include "EmulatedUSBImpl.h"
#include "ConsoleImpl.h"
#include "Logging.h"

#include <VBox/vmm/pdmusb.h>


/*
 * Emulated USB webcam device instance.
 */
typedef std::map <Utf8Str, Utf8Str> EUSBSettingsMap;

typedef enum EUSBDEVICESTATUS
{
    EUSBDEVICE_CREATED,
    EUSBDEVICE_ATTACHING,
    EUSBDEVICE_ATTACHED
} EUSBDEVICESTATUS;

class EUSBWEBCAM /* : public EUSBDEVICE */
{
    private:
        int32_t volatile mcRefs;

        RTUUID mUuid;

        Utf8Str mPath;
        Utf8Str mSettings;

        EUSBSettingsMap mDevSettings;
        EUSBSettingsMap mDrvSettings;

        static DECLCALLBACK(int) emulatedWebcamAttach(PUVM pUVM, EUSBWEBCAM *pThis);
        static DECLCALLBACK(int) emulatedWebcamDetach(PUVM pUVM, EUSBWEBCAM *pThis);

        HRESULT settingsParse(void);

        ~EUSBWEBCAM()
        {
        }

    public:
        EUSBWEBCAM()
            :
            mcRefs(1),
            enmStatus(EUSBDEVICE_CREATED)
        {
            RT_ZERO(mUuid);
        }

        int32_t AddRef(void)
        {
            return ASMAtomicIncS32(&mcRefs);
        }

        void Release(void)
        {
            int32_t c = ASMAtomicDecS32(&mcRefs);
            if (c == 0)
            {
                delete this;
            }
        }

        HRESULT Initialize(Console *pConsole,
                           const com::Utf8Str *aPath,
                           const com::Utf8Str *aSettings);
        HRESULT Attach(Console *pConsole,
                       PUVM pUVM);
        HRESULT Detach(Console *pConsole,
                       PUVM pUVM);

        EUSBDEVICESTATUS enmStatus;
};


/* static */ DECLCALLBACK(int) EUSBWEBCAM::emulatedWebcamAttach(PUVM pUVM, EUSBWEBCAM *pThis)
{
    EUSBSettingsMap::const_iterator it;

    PCFGMNODE pInstance = CFGMR3CreateTree(pUVM);
    PCFGMNODE pConfig;
    CFGMR3InsertNode(pInstance,   "Config", &pConfig);
    for (it = pThis->mDevSettings.begin(); it != pThis->mDevSettings.end(); ++it)
        CFGMR3InsertString(pConfig, it->first.c_str(), it->second.c_str());

    PCFGMNODE pLunL0;
    CFGMR3InsertNode(pInstance,   "LUN#0", &pLunL0);
    CFGMR3InsertString(pLunL0,      "Driver", "HostWebcam");
    CFGMR3InsertNode(pLunL0,        "Config", &pConfig);
    CFGMR3InsertString(pConfig,       "DevicePath", pThis->mPath.c_str());
    for (it = pThis->mDrvSettings.begin(); it != pThis->mDrvSettings.end(); ++it)
        CFGMR3InsertString(pConfig, it->first.c_str(), it->second.c_str());

    int rc = PDMR3UsbCreateEmulatedDevice(pUVM, "Webcam", pInstance, &pThis->mUuid);
    LogRel(("PDMR3UsbCreateEmulatedDevice %Rrc\n", rc));
    if (RT_FAILURE(rc) && pInstance)
        CFGMR3RemoveNode(pInstance);
    return rc;
}

/* static */ DECLCALLBACK(int) EUSBWEBCAM::emulatedWebcamDetach(PUVM pUVM, EUSBWEBCAM *pThis)
{
    return PDMR3UsbDetachDevice(pUVM, &pThis->mUuid);
}

HRESULT EUSBWEBCAM::Initialize(Console *pConsole,
                               const com::Utf8Str *aPath,
                               const com::Utf8Str *aSettings)
{
    HRESULT hrc = S_OK;

    int vrc = RTUuidCreate(&mUuid);
    if (RT_SUCCESS(vrc))
    {
        hrc = mPath.assignEx(*aPath);
        if (SUCCEEDED(hrc))
        {
            hrc = mSettings.assignEx(*aSettings);
        }

        if (SUCCEEDED(hrc))
        {
            hrc = settingsParse();
        }
    }

    if (SUCCEEDED(hrc) && RT_FAILURE(vrc))
    {
        LogFlowThisFunc(("%Rrc\n", vrc));
        hrc = pConsole->setError(VBOX_E_IPRT_ERROR,
                                 "Init emulated USB webcam (%Rrc)", vrc);
    }

    return hrc;
}

HRESULT EUSBWEBCAM::settingsParse(void)
{
    HRESULT hrc = S_OK;

    return hrc;
}

HRESULT EUSBWEBCAM::Attach(Console *pConsole,
                           PUVM pUVM)
{
    HRESULT hrc = S_OK;

    int  vrc = VMR3ReqCallWaitU(pUVM, 0 /* idDstCpu (saved state, see #6232) */,
                                (PFNRT)emulatedWebcamAttach, 2,
                                pUVM, this);

    if (SUCCEEDED(hrc) && RT_FAILURE(vrc))
    {
        LogFlowThisFunc(("%Rrc\n", vrc));
        hrc = pConsole->setError(VBOX_E_IPRT_ERROR,
                                 "Attach emulated USB webcam (%Rrc)", vrc);
    }

    return hrc;
}

HRESULT EUSBWEBCAM::Detach(Console *pConsole,
                           PUVM pUVM)
{
    HRESULT hrc = S_OK;

    int vrc = VMR3ReqCallWaitU(pUVM, 0 /* idDstCpu (saved state, see #6232) */,
                              (PFNRT)emulatedWebcamDetach, 2,
                              pUVM, this);

    if (SUCCEEDED(hrc) && RT_FAILURE(vrc))
    {
        LogFlowThisFunc(("%Rrc\n", vrc));
        hrc = pConsole->setError(VBOX_E_IPRT_ERROR,
                                 "Detach emulated USB webcam (%Rrc)", vrc);
    }

    return hrc;
}


/*
 * EmulatedUSB implementation.
 */
DEFINE_EMPTY_CTOR_DTOR(EmulatedUSB)

HRESULT EmulatedUSB::FinalConstruct()
{
    return BaseFinalConstruct();
}

void EmulatedUSB::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

/*
 * Initializes the instance.
 *
 * @param pConsole   The owner.
 */
HRESULT EmulatedUSB::init(ComObjPtr<Console> pConsole)
{
    LogFlowThisFunc(("\n"));

    ComAssertRet(!pConsole.isNull(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m.pConsole = pConsole;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/*
 * Uninitializes the instance.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void EmulatedUSB::uninit()
{
    LogFlowThisFunc(("\n"));

    m.pConsole.setNull();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    WebcamsMap::iterator it = m.webcams.begin();
    while (it != m.webcams.end())
    {
        EUSBWEBCAM *p = it->second;
        m.webcams.erase(it++);
        p->Release();
    }
    alock.release();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

HRESULT EmulatedUSB::getWebcams(std::vector<com::Utf8Str> &aWebcams)
{
    HRESULT hrc = S_OK;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    try
    {
        aWebcams.resize(m.webcams.size());
    }
    catch (std::bad_alloc &)
    {
        hrc = E_OUTOFMEMORY;
    }
    catch (...)
    {
        hrc = E_FAIL;
    }

    if (SUCCEEDED(hrc))
    {
        size_t i;
        WebcamsMap::const_iterator it;
        for (i = 0, it = m.webcams.begin(); it != m.webcams.end(); ++it)
            aWebcams[i++] = it->first;
    }

    return hrc;
}

static const Utf8Str s_pathDefault(".0");

HRESULT EmulatedUSB::webcamAttach(const com::Utf8Str &aPath,
                                  const com::Utf8Str &aSettings)
{
    HRESULT hrc = S_OK;

    const Utf8Str &path = aPath.isEmpty() || aPath == "."? s_pathDefault: aPath;

    Console::SafeVMPtr ptrVM(m.pConsole);
    if (ptrVM.isOk())
    {
        EUSBWEBCAM *p = new EUSBWEBCAM();
        if (p)
        {
            hrc = p->Initialize(m.pConsole, &path, &aSettings);
            if (SUCCEEDED(hrc))
            {
                AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                WebcamsMap::const_iterator it = m.webcams.find(path);
                if (it == m.webcams.end())
                {
                    p->AddRef();
                    try
                    {
                        m.webcams[path] = p;
                    }
                    catch (std::bad_alloc &)
                    {
                        hrc = E_OUTOFMEMORY;
                    }
                    catch (...)
                    {
                        hrc = E_FAIL;
                    }
                    p->enmStatus = EUSBDEVICE_ATTACHING;
                }
                else
                {
                    hrc = E_FAIL;
                }
            }

            if (SUCCEEDED(hrc))
            {
                hrc = p->Attach(m.pConsole, ptrVM.rawUVM());
            }

            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            if (SUCCEEDED(hrc))
            {
                p->enmStatus = EUSBDEVICE_ATTACHED;
            }
            else
            {
                if (p->enmStatus != EUSBDEVICE_CREATED)
                {
                    m.webcams.erase(path);
                }
            }
            alock.release();

            p->Release();
        }
        else
        {
            hrc = E_OUTOFMEMORY;
        }
    }
    else
    {
        hrc = VBOX_E_INVALID_VM_STATE;
    }

    return hrc;
}

HRESULT EmulatedUSB::webcamDetach(const com::Utf8Str &aPath)
{
    HRESULT hrc = S_OK;

    const Utf8Str &path = aPath.isEmpty() || aPath == "."? s_pathDefault: aPath;

    Console::SafeVMPtr ptrVM(m.pConsole);
    if (ptrVM.isOk())
    {
        EUSBWEBCAM *p = NULL;

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        WebcamsMap::iterator it = m.webcams.find(path);
        if (it != m.webcams.end())
        {
            if (it->second->enmStatus == EUSBDEVICE_ATTACHED)
            {
                p = it->second;
                m.webcams.erase(it);
            }
        }
        alock.release();

        if (p)
        {
            hrc = p->Detach(m.pConsole, ptrVM.rawUVM());
            p->Release();
        }
        else
        {
            hrc = E_INVALIDARG;
        }
    }
    else
    {
        hrc = VBOX_E_INVALID_VM_STATE;
    }

    return hrc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
