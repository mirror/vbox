/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsGeneral class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsGeneral_h__
#define __UIMachineSettingsGeneral_h__

/* Local includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsGeneral.gen.h"

/* Machine settings / General page / Data: */
struct UIDataSettingsMachineGeneral
{
    /* Default constructor: */
    UIDataSettingsMachineGeneral()
        : m_strName(QString())
        , m_strGuestOsTypeId(QString())
        , m_fSaveMountedAtRuntime(false)
        , m_fShowMiniToolBar(false)
        , m_fMiniToolBarAtTop(false)
        , m_strSnapshotsFolder(QString())
        , m_strSnapshotsHomeDir(QString())
        , m_clipboardMode(KClipboardMode_Disabled)
        , m_strDescription(QString()) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineGeneral &other) const
    {
        return (m_strName == other.m_strName) &&
               (m_strGuestOsTypeId == other.m_strGuestOsTypeId) &&
               (m_fSaveMountedAtRuntime == other.m_fSaveMountedAtRuntime) &&
               (m_fShowMiniToolBar == other.m_fShowMiniToolBar) &&
               (m_fMiniToolBarAtTop == other.m_fMiniToolBarAtTop) &&
               (m_strSnapshotsFolder == other.m_strSnapshotsFolder) &&
               (m_strSnapshotsHomeDir == other.m_strSnapshotsHomeDir) &&
               (m_clipboardMode == other.m_clipboardMode) &&
               (m_strDescription == other.m_strDescription);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineGeneral &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineGeneral &other) const { return !equal(other); }
    /* Variables: */
    QString m_strName;
    QString m_strGuestOsTypeId;
    bool m_fSaveMountedAtRuntime;
    bool m_fShowMiniToolBar;
    bool m_fMiniToolBarAtTop;
    QString m_strSnapshotsFolder;
    QString m_strSnapshotsHomeDir;
    KClipboardMode m_clipboardMode;
    QString m_strDescription;
};
typedef UISettingsCache<UIDataSettingsMachineGeneral> UICacheSettingsMachineGeneral;

/* Machine settings / General page: */
class UIMachineSettingsGeneral : public UISettingsPageMachine,
                              public Ui::UIMachineSettingsGeneral
{
    Q_OBJECT;

public:

    UIMachineSettingsGeneral();

    CGuestOSType guestOSType() const;
    void setHWVirtExEnabled(bool fEnabled);
    bool is64BitOSTypeSelected() const;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isWindowsOSTypeSelected() const;
#endif /* VBOX_WITH_VIDEOHWACCEL */

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Page changed: */
    bool changed() const { return m_cache.wasChanged(); }

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate(QString &strWarning, QString &strTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private:

    void polishPage();

    QIWidgetValidator *mValidator;
    bool m_fHWVirtExEnabled;

    /* Cache: */
    UICacheSettingsMachineGeneral m_cache;
};

#endif // __UIMachineSettingsGeneral_h__

