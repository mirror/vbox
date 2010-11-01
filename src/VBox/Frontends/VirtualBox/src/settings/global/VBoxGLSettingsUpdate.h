/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsUpdate class declaration
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

#ifndef __VBoxGLSettingsUpdate_h__
#define __VBoxGLSettingsUpdate_h__

#include "UISettingsPage.h"
#include "VBoxGLSettingsUpdate.gen.h"
#include "VBoxUpdateDlg.h"

/* Global settings / Update page / Cache: */
struct UISettingsCacheGlobalUpdate
{
    bool m_fCheckEnabled;
    VBoxUpdateData::PeriodType m_periodIndex;
    VBoxUpdateData::BranchType m_branchIndex;
    QString m_strDate;
};

/* Global settings / Update page: */
class VBoxGLSettingsUpdate : public UISettingsPageGlobal,
                             public Ui::VBoxGLSettingsUpdate
{
    Q_OBJECT;

public:

    VBoxGLSettingsUpdate();

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

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void toggleUpdater (bool aOn);
    void activatedPeriod (int aIndex);
    void toggledBranch();

private:

    void showEvent (QShowEvent *aEvent);

    VBoxUpdateData::PeriodType periodType() const;
    VBoxUpdateData::BranchType branchType() const;

    bool mSettingsChanged;
    QRadioButton *mLastChosen;

    /* Cache: */
    UISettingsCacheGlobalUpdate m_cache;
};

#endif // __VBoxGLSettingsUpdate_h__

