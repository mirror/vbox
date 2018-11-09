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

#ifndef ____H_RecordSettings
#define ____H_RecordSettings

#include "RecordSettingsWrap.h"

namespace settings
{
    struct RecordSettings;
    struct RecordScreenSettings;
}

class RecordScreenSettings;

class ATL_NO_VTABLE RecordSettings :
    public RecordSettingsWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(RecordSettings)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *parent);
    HRESULT init(Machine *parent, RecordSettings *that);
    HRESULT initCopy(Machine *parent, RecordSettings *that);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::RecordSettings &data);
    HRESULT i_saveSettings(settings::RecordSettings &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(RecordSettings *aThat);
    void i_applyDefaults(void);

    int i_getDefaultFileName(Utf8Str &strFile);
    bool i_canChangeSettings(void);
    void i_onSettingsChanged(void);

private:

    /** Map of screen settings objects. The key specifies the screen ID. */
    typedef std::map <uint32_t, ComObjPtr<RecordScreenSettings> > RecordScreenSettingsMap;

    void i_reset(void);
    int i_syncToMachineDisplays(void);
    int i_createScreenObj(RecordScreenSettingsMap &screenSettingsMap, uint32_t uScreenId, const settings::RecordScreenSettings &data);
    int i_destroyScreenObj(RecordScreenSettingsMap &screenSettingsMap, uint32_t uScreenId);
    int i_destroyAllScreenObj(RecordScreenSettingsMap &screenSettingsMap);

private:

    // wrapped IRecordSettings properties
    HRESULT getEnabled(BOOL *enabled);
    HRESULT setEnabled(BOOL enable);
    HRESULT getScreens(std::vector<ComPtr<IRecordScreenSettings> > &aRecordScreenSettings);

    // wrapped IRecordSettings methods
    HRESULT getScreenSettings(ULONG uScreenId, ComPtr<IRecordScreenSettings> &aRecordScreenSettings);

private:

    struct Data;
    Data *m;
};
#endif // ____H_RecordSettings

