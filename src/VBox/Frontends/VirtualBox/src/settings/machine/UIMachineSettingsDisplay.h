/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineSettingsDisplay_h___
#define ___UIMachineSettingsDisplay_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsDisplay.gen.h"

/* COM includes: */
#include "CGuestOSType.h"

/* Forward declarations: */
class UIActionPool;
struct UIDataSettingsMachineDisplay;
typedef UISettingsCache<UIDataSettingsMachineDisplay> UISettingsCacheMachineDisplay;


/** Machine settings: Display page. */
class UIMachineSettingsDisplay : public UISettingsPageMachine,
                                 public Ui::UIMachineSettingsDisplay
{
    Q_OBJECT;

public:

    /** Constructs Display settings page. */
    UIMachineSettingsDisplay();
    /** Destructs Display settings page. */
    ~UIMachineSettingsDisplay();

    /* API: Correlation stuff: */
    void setGuestOSType(CGuestOSType comGuestOSType);

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isAcceleration2DVideoSelected() const;
#endif /* VBOX_WITH_VIDEOHWACCEL */

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

    /** Defines TAB order. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

private slots:

    /* Handlers: Screen stuff: */
    void sltHandleVideoMemorySizeSliderChange();
    void sltHandleVideoMemorySizeEditorChange();
    void sltHandleGuestScreenCountSliderChange();
    void sltHandleGuestScreenCountEditorChange();
    void sltHandleGuestScreenScaleSliderChange();
    void sltHandleGuestScreenScaleEditorChange();

    /* Handlers: Video Capture stuff: */
    void sltHandleVideoCaptureCheckboxToggle();
    void sltHandleVideoCaptureFrameSizeComboboxChange();
    void sltHandleVideoCaptureFrameWidthEditorChange();
    void sltHandleVideoCaptureFrameHeightEditorChange();
    void sltHandleVideoCaptureFrameRateSliderChange();
    void sltHandleVideoCaptureFrameRateEditorChange();
    void sltHandleVideoCaptureQualitySliderChange();
    void sltHandleVideoCaptureBitRateEditorChange();

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare routine: Screen tab. */
    void prepareScreenTab();
    /** Prepare routine: Remote Display tab. */
    void prepareRemoteDisplayTab();
    /** Prepare routine: Video Capture tab. */
    void prepareVideoCaptureTab();

    /* Helpers: Video stuff: */
    void checkVRAMRequirements();
    bool shouldWeWarnAboutLowVRAM();
    static int calculatePageStep(int iMax);

    /* Helpers: Video Capture stuff: */
    void lookForCorrespondingFrameSizePreset();
    void updateGuestScreenCount();
    void updateVideoCaptureFileSizeHint();
    static void lookForCorrespondingPreset(QComboBox *pComboBox, const QVariant &data);
    static int calculateBitRate(int iFrameWidth, int iFrameHeight, int iFrameRate, int iQuality);
    static int calculateQuality(int iFrameWidth, int iFrameHeight, int iFrameRate, int iBitRate);

    /* Guest OS type id: */
    CGuestOSType  m_comGuestOSType;
    /* System minimum lower limit of VRAM (MiB). */
    int           m_iMinVRAM;
    /* System maximum limit of VRAM (MiB). */
    int           m_iMaxVRAM;
    /* Upper limit of VRAM in MiB for this dialog. This value is lower than
     * m_maxVRAM to save careless users from setting useless big values. */
    int           m_iMaxVRAMVisible;
    /* Initial VRAM value when the dialog is opened. */
    int           m_iInitialVRAM;
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Specifies whether the guest OS supports 2D video-acceleration: */
    bool          m_f2DVideoAccelerationSupported;
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef VBOX_WITH_CRHGSMI
    /* Specifies whether the guest OS supports WDDM: */
    bool          m_fWddmModeSupported;
#endif /* VBOX_WITH_CRHGSMI */

    /** Holds the page data cache instance. */
    UISettingsCacheMachineDisplay *m_pCache;
};

#endif /* !___UIMachineSettingsDisplay_h___ */

