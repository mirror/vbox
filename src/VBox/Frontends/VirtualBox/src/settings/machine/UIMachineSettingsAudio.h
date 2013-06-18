/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsAudio class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsAudio_h__
#define __UIMachineSettingsAudio_h__

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsAudio.gen.h"

/* Machine settings / Audio page / Data: */
struct UIDataSettingsMachineAudio
{
    /* Constructor: */
    UIDataSettingsMachineAudio()
        : m_fAudioEnabled(false)
        , m_audioDriverType(KAudioDriverType_Null)
        , m_audioControllerType(KAudioControllerType_AC97) {}

    /* Helpers: Compare functions/operators: */
    bool equal(const UIDataSettingsMachineAudio &other) const
    {
        return (m_fAudioEnabled == other.m_fAudioEnabled) &&
               (m_audioDriverType == other.m_audioDriverType) &&
               (m_audioControllerType == other.m_audioControllerType);
    }
    bool operator==(const UIDataSettingsMachineAudio &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineAudio &other) const { return !equal(other); }

    /* Variables: */
    bool m_fAudioEnabled;
    KAudioDriverType m_audioDriverType;
    KAudioControllerType m_audioControllerType;
};
typedef UISettingsCache<UIDataSettingsMachineAudio> UICacheSettingsMachineAudio;

/* Machine settings / Audio page: */
class UIMachineSettingsAudio : public UISettingsPageMachine, public Ui::UIMachineSettingsAudio
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMachineSettingsAudio();

protected:

    /* API: Cache stuff: */
    bool changed() const { return m_cache.wasChanged(); }

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* API: Focus-order stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Polish stuff: */
    void polishPage();

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareComboboxes();

    /* Cache: */
    UICacheSettingsMachineAudio m_cache;
};

#endif // __UIMachineSettingsAudio_h__

