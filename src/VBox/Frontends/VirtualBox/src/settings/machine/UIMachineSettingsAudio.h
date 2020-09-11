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

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
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
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing audio data from the cache. */
    bool saveAudioData();

    /** Holds the page data cache instance. */
    UISettingsCacheMachineAudio *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the audio check-box instance. */
        QCheckBox               *m_pCheckBoxAudio;
        /** Holds the audio settings widget instance. */
        QWidget                 *m_pWidgetAudioSettings;
        /** Holds the audio host driver label instance. */
        QLabel                  *m_pLabelAudioHostDriver;
        /** Holds the audio host driver editor instance. */
        UIAudioHostDriverEditor *m_pEditorAudioHostDriver;
        /** Holds the audio host controller label instance. */
        QLabel                  *m_pLabelAudioController;
        /** Holds the audio host controller instance instance. */
        UIAudioControllerEditor *m_pEditorAudioController;
        /** Holds the audio extended label instance. */
        QLabel                  *m_pLabelAudioExtended;
        /** Holds the audio output check-box instance. */
        QCheckBox               *m_pCheckBoxAudioOutput;
        /** Holds the audio input check-box instance. */
        QCheckBox               *m_pCheckBoxAudioInput;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsAudio_h */
