/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsAudio class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsAudio_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsAudio_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class UIAudioControllerEditor;
class UIAudioHostDriverEditor;
struct UIDataSettingsMachineAudio;
typedef UISettingsCache<UIDataSettingsMachineAudio> UISettingsCacheMachineAudio;

/** Machine settings: Audio page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsAudio : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Audio settings page. */
    UIMachineSettingsAudio();
    /** Destructs Audio settings page. */
    ~UIMachineSettingsAudio();

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */;

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() /* override */;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing audio data from the cache. */
    bool saveAudioData();

    /** Holds the page data cache instance. */
    UISettingsCacheMachineAudio *m_pCache;

    QCheckBox *m_pCheckBoxAudio;
    QCheckBox *m_pCheckBoxAudioOutput;
    QCheckBox *m_pCheckBoxAudioInput;

    /** @name Widgets
     * @{ */
        QLabel *m_pAudioHostDriverLabel;
        QLabel *m_pAudioControllerLabel;
        QLabel *m_pLabelAudioExtended;
        QWidget *m_pContainerAudioSubOptions;
        UIAudioHostDriverEditor *m_pAudioHostDriverEditor;
        UIAudioControllerEditor *m_pAudioControllerEditor;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsAudio_h */
