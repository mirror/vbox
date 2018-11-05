/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation - Machine capture settings.
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
 */

#ifndef ____H_CAPTURESETTINGS
#define ____H_CAPTURESETTINGS

#include "CaptureSettingsWrap.h"

namespace settings
{
    struct CaptureSettings;
}

class ICaptureScreenSettings;
class CaptureScreenSettings;

class ATL_NO_VTABLE CaptureSettings :
    public CaptureSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(CaptureSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *parent);
    HRESULT init(Machine *parent, CaptureSettings *that);
    HRESULT initCopy(Machine *parent, CaptureSettings *that);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::CaptureSettings &data);
    HRESULT i_saveSettings(settings::CaptureSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(CaptureSettings *aThat);
    void i_applyDefaults(void);

private:

    // wrapped ICaptureSettings properties
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enable);
    HRESULT getScreens(std::vector<ComPtr<ICaptureScreenSettings> > &aCaptureScreenSettings);

    // wrapped ICaptureSettings methods
    HRESULT getScreenSettings(ULONG uScreenId, ComPtr<ICaptureScreenSettings> &aCaptureScreenSettings);

private:

    /** Map of screen settings objects. The key specifies the screen ID. */
    typedef std::map <ULONG, ComObjPtr<CaptureScreenSettings> > CaptureScreenSettingsMap;

    struct Data;
    Data *m;
};
#endif // ____H_CAPTURESETTINGS

