/* $Id$ */
/** @file
 * VirtualBox COM class implementation - Machine recording screen settings.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_RecordingSettingsImpl_h
#define MAIN_INCLUDED_RecordingSettingsImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "RecordingSettingsWrap.h"

namespace settings
{
    struct RecordingSettings;
    struct RecordingScreenSettings;
}

class RecordingScreenSettings;

class ATL_NO_VTABLE RecordingSettings
    : public RecordingSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(RecordingSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *parent);
    HRESULT init(Machine *parent, RecordingSettings *aThat);
    HRESULT initCopy(Machine *parent, RecordingSettings *aThat);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::RecordingSettings &data);
    HRESULT i_saveSettings(settings::RecordingSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(RecordingSettings *aThat);
    void i_applyDefaults(void);

    int i_getDefaultFilename(Utf8Str &strFile, bool fWithFileExtension);
    bool i_canChangeSettings(void);
    void i_onSettingsChanged(void);

private:

    /** Map of screen settings objects. The key specifies the screen ID. */
    typedef std::map <uint32_t, ComObjPtr<RecordingScreenSettings> > RecordScreenSettingsMap;

    void i_reset(void);
    int i_syncToMachineDisplays(void);
    int i_createScreenObj(RecordScreenSettingsMap &screenSettingsMap, uint32_t uScreenId, const settings::RecordingScreenSettings &data);
    int i_destroyScreenObj(RecordScreenSettingsMap &screenSettingsMap, uint32_t uScreenId);
    int i_destroyAllScreenObj(RecordScreenSettingsMap &screenSettingsMap);

private:

    // wrapped IRecordingSettings properties
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enable);
    HRESULT getScreens(std::vector<ComPtr<IRecordingScreenSettings> > &aRecordScreenSettings);

    // wrapped IRecordingSettings methods
    HRESULT getScreenSettings(ULONG uScreenId, ComPtr<IRecordingScreenSettings> &aRecordScreenSettings);

private:

    struct Data;
    Data *m;
};

#endif /* !MAIN_INCLUDED_RecordingSettingsImpl_h */

