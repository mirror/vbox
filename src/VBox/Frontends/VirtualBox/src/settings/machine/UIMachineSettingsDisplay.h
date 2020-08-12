/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* COM includes: */
#include "CGuestOSType.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QGridLayout;
class QSpinBox;
class QStackedLayout;
class QIAdvancedSlider;
class QITabWidget;
class UIActionPool;
struct UIDataSettingsMachineDisplay;
class UIFilePathSelector;
class UIFilmContainer;
class UIGraphicsControllerEditor;
class UIScaleFactorEditor;
class UIVideoMemoryEditor;
typedef UISettingsCache<UIDataSettingsMachineDisplay> UISettingsCacheMachineDisplay;

/** Machine settings: Display page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsDisplay : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Display settings page. */
    UIMachineSettingsDisplay();
    /** Destructs Display settings page. */
    ~UIMachineSettingsDisplay();

    /** Defines @a comGuestOSType. */
    void setGuestOSType(CGuestOSType comGuestOSType);

#ifdef VBOX_WITH_3D_ACCELERATION
    /** Returns whether 3D Acceleration is enabled. */
    bool isAcceleration3DSelected() const;
#endif

    /** Returns recommended graphics controller type. */
    KGraphicsControllerType graphicsControllerTypeRecommended() const;
    /** Returns current graphics controller type. */
    KGraphicsControllerType graphicsControllerTypeCurrent() const;

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

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Defines TAB order for passed @a pWidget. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

private slots:

    /** Handles Guest Screen count slider change. */
    void sltHandleGuestScreenCountSliderChange();
    /** Handles Guest Screen count editor change. */
    void sltHandleGuestScreenCountEditorChange();
    /** Handles Graphics Controller combo change. */
    void sltHandleGraphicsControllerComboChange();
#ifdef VBOX_WITH_3D_ACCELERATION
    /** Handles 3D Acceleration check-box change. */
    void sltHandle3DAccelerationCheckboxChange();
#endif

    /** Handles recording toggle. */
    void sltHandleRecordingCheckboxToggle();
    /** Handles recording frame size change. */
    void sltHandleRecordingVideoFrameSizeComboboxChange();
    /** Handles recording frame width change. */
    void sltHandleRecordingVideoFrameWidthEditorChange();
    /** Handles recording frame height change. */
    void sltHandleRecordingVideoFrameHeightEditorChange();
    /** Handles recording frame rate slider change. */
    void sltHandleRecordingVideoFrameRateSliderChange();
    /** Handles recording frame rate editor change. */
    void sltHandleRecordingVideoFrameRateEditorChange();
    /** Handles recording quality slider change. */
    void sltHandleRecordingVideoQualitySliderChange();
    /** Handles recording bit-rate editor change. */
    void sltHandleRecordingVideoBitRateEditorChange();
    void sltHandleRecordingComboBoxChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares 'Screen' tab. */
    void prepareTabScreen();
    /** Prepares 'Remote Display' tab. */
    void prepareTabRemoteDisplay();
    /** Prepares 'Recording' tab. */
    void prepareTabRecording();
    /** Prepares connections. */
    void prepareConnections();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Returns whether the VRAM requirements are important. */
    bool shouldWeWarnAboutLowVRAM();

    /** Searches for corresponding frame size preset. */
    void lookForCorrespondingFrameSizePreset();
    /** Updates guest-screen count. */
    void updateGuestScreenCount();
    /** Updates recording file size hint. */
    void updateRecordingFileSizeHint();
    /** Searches for the @a data field in corresponding @a pComboBox. */
    static void lookForCorrespondingPreset(QComboBox *pComboBox, const QVariant &data);
    /** Calculates recording video bit-rate for passed @a iFrameWidth, @a iFrameHeight, @a iFrameRate and @a iQuality. */
    static int calculateBitRate(int iFrameWidth, int iFrameHeight, int iFrameRate, int iQuality);
    /** Calculates recording video quality for passed @a iFrameWidth, @a iFrameHeight, @a iFrameRate and @a iBitRate. */
    static int calculateQuality(int iFrameWidth, int iFrameHeight, int iFrameRate, int iBitRate);
    /** Saves existing display data from the cache. */
    bool saveDisplayData();
    /** Saves existing 'Screen' data from the cache. */
    bool saveScreenData();
    /** Saves existing 'Remote Display' data from the cache. */
    bool saveRemoteDisplayData();
    /** Saves existing 'Recording' data from the cache. */
    bool saveRecordingData();
    /** Decide which of the recording related widgets are to be disabled/enabled. */
    void enableDisableRecordingWidgets();

    /** Holds the guest OS type ID. */
    CGuestOSType  m_comGuestOSType;
#ifdef VBOX_WITH_3D_ACCELERATION
    /** Holds whether the guest OS supports WDDM. */
    bool          m_fWddmModeSupported;
#endif
    /** Holds recommended graphics controller type. */
    KGraphicsControllerType  m_enmGraphicsControllerTypeRecommended;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineDisplay *m_pCache;

    /** @name Widgets
     * @{ */
       QCheckBox *m_pCheckbox3D;
       QCheckBox *m_pCheckboxRemoteDisplay;
       QCheckBox *m_pCheckboxMultipleConn;
       QCheckBox *m_pCheckboxVideoCapture;
       QComboBox *m_pComboRemoteDisplayAuthMethod;
       QComboBox *m_pComboBoxCaptureMode;
       QComboBox *m_pComboVideoCaptureSize;
       QLabel *m_pLabelVideoScreenCountMin;
       QLabel *m_pLabelVideoScreenCountMax;
       QLabel *m_pLabelVideoCaptureFrameRateMin;
       QLabel *m_pLabelVideoCaptureFrameRateMax;
       QLabel *m_pLabelVideoCaptureQualityMin;
       QLabel *m_pLabelVideoCaptureQualityMed;
       QLabel *m_pLabelVideoCaptureQualityMax;
       QLabel *m_pLabelAudioCaptureQualityMin;
       QLabel *m_pLabelAudioCaptureQualityMed;
       QLabel *m_pLabelAudioCaptureQualityMax;
       QLabel *m_pVideoMemoryLabel;
       QLabel *m_pLabelVideoScreenCount;
       QLabel *m_pGraphicsControllerLabel;
       QLabel *m_pLabelVideoOptions;
       QLabel *m_pLabelRemoteDisplayOptions;
       QLabel *m_pLabelCaptureMode;
       QLabel *m_pLabelVideoCapturePath;
       QLabel *m_pLabelVideoCaptureSizeHint;
       QLabel *m_pLabelVideoCaptureSize;
       QLabel *m_pLabelVideoCaptureFrameRate;
       QLabel *m_pLabelVideoCaptureRate;
       QLabel *m_pAudioCaptureQualityLabel;
       QLabel *m_pLabelVideoCaptureScreens;
       QLabel *m_pLabelGuestScreenScaleFactorEditor;
       QLabel *m_pLabelRemoteDisplayPort;
       QLabel *m_pLabelRemoteDisplayAuthMethod;
       QLabel *m_pLabelRemoteDisplayTimeout;
       QLineEdit *m_pEditorRemoteDisplayPort;
       QLineEdit *m_pEditorRemoteDisplayTimeout;
       QSpinBox *m_pEditorVideoScreenCount;
       QSpinBox *m_pEditorVideoCaptureWidth;
       QSpinBox *m_pEditorVideoCaptureFrameRate;
       QSpinBox *m_pEditorVideoCaptureHeight;
       QSpinBox *m_pEditorVideoCaptureBitRate;
       UIGraphicsControllerEditor *m_pGraphicsControllerEditor;
       UIScaleFactorEditor *m_pScaleFactorEditor;
       UIVideoMemoryEditor *m_pVideoMemoryEditor;
       UIFilePathSelector *m_pEditorVideoCapturePath;
       UIFilmContainer *m_pScrollerVideoCaptureScreens;
       QIAdvancedSlider *m_pSliderAudioCaptureQuality;
       QIAdvancedSlider *m_pSliderVideoScreenCount;
       QIAdvancedSlider *m_pSliderVideoCaptureFrameRate;
       QIAdvancedSlider *m_pSliderVideoCaptureQuality;
       QITabWidget *m_pTabWidget;
       QWidget *m_pContainerRemoteDisplay;
       QWidget *m_pContainerRemoteDisplayOptions;
       QWidget *m_pContainerVideoCapture;
       QWidget *m_pContainerSliderVideoCaptureFrameRate;
       QWidget *m_pContainerSliderVideoCaptureQuality;
       QWidget *m_pContainerSliderAudioCaptureQuality;
       QWidget *m_pTabVideo;
       QWidget *m_pTabRemoteDisplay;
       QWidget *m_pTabVideoCapture;
       QGridLayout *m_pContainerLayoutSliderVideoCaptureQuality;
       QStackedLayout *m_pLayout3D;
   /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h */
