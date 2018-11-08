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
    struct CaptureScreenSettings;
}

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

    int i_getDefaultFileName(Utf8Str &strFile);
    bool i_canChangeSettings(void);
    void i_onSettingsChanged(void);

private:

    /** Map of screen settings objects. The key specifies the screen ID. */
    typedef std::map <uint32_t, ComObjPtr<CaptureScreenSettings> > CaptureScreenSettingsMap;

    void i_reset(void);
    int i_syncToMachineDisplays(void);
    int i_createScreenObj(CaptureScreenSettingsMap &screenSettingsMap, uint32_t uScreenId, const settings::CaptureScreenSettings &data);
    int i_destroyScreenObj(CaptureScreenSettingsMap &screenSettingsMap, uint32_t uScreenId);
    int i_destroyAllScreenObj(CaptureScreenSettingsMap &screenSettingsMap);

private:

    // wrapped ICaptureSettings properties
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enable);
    HRESULT getScreens(std::vector<ComPtr<ICaptureScreenSettings> > &aCaptureScreenSettings);

    // wrapped ICaptureSettings methods
    HRESULT getScreenSettings(ULONG uScreenId, ComPtr<ICaptureScreenSettings> &aCaptureScreenSettings);

private:

    struct Data;
    Data *m;
};
#endif // ____H_CAPTURESETTINGS

