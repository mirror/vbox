/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsGeneral class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#include "UISettingsPage.h"
#include "UIMachineSettingsGeneral.gen.h"
#include "COMDefs.h"

/* Machine settings / General page / Cache: */
struct UISettingsCacheMachineGeneral
{
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

/* Machine settings / General page: */
class UIMachineSettingsGeneral : public UISettingsPageMachine,
                              public Ui::UIMachineSettingsGeneral
{
    Q_OBJECT;

public:

    UIMachineSettingsGeneral();

    bool is64BitOSTypeSelected() const;

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isWindowsOSTypeSelected() const;
#endif

#ifdef VBOX_WITH_CRHGSMI
    bool isWddmSupportedForOSType() const;
#endif

protected:

    /* Load data to cashe from corresponding external object(s),
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

    void setValidator (QIWidgetValidator *aVal);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private:

    void showEvent (QShowEvent *aEvent);

    QIWidgetValidator *mValidator;

    /* Cache: */
    UISettingsCacheMachineGeneral m_cache;
};

#endif // __UIMachineSettingsGeneral_h__

