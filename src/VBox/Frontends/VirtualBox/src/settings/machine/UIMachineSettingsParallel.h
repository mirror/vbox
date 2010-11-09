/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsParallel class declaration
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

#ifndef __UIMachineSettingsParallel_h__
#define __UIMachineSettingsParallel_h__

#include "UISettingsPage.h"
#include "UIMachineSettingsParallel.gen.h"
#include "COMDefs.h"

class QITabWidget;

struct UIParallelPortData
{
    int m_iSlot;
    bool m_fPortEnabled;
    ulong m_uIRQ;
    ulong m_uIOBase;
    QString m_strPath;
};

/* Machine settings / Parallel page / Cache: */
struct UISettingsCacheMachineParallel
{
    QList<UIParallelPortData> m_items;
};

class UIMachineSettingsParallel : public QIWithRetranslateUI<QWidget>,
                               public Ui::UIMachineSettingsParallel
{
    Q_OBJECT;

public:

    UIMachineSettingsParallel();

    void fetchPortData(const UIParallelPortData &data);
    void uploadPortData(UIParallelPortData &data);

    void setValidator (QIWidgetValidator *aVal);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbParallelToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);

private:

    QIWidgetValidator *mValidator;
    int m_iSlot;
};

/* Machine settings / Parallel page: */
class UIMachineSettingsParallelPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    UIMachineSettingsParallelPage();

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
    bool revalidate (QString &aWarning, QString &aTitle);

    void retranslateUi();

private:

    QIWidgetValidator *mValidator;
    QITabWidget *mTabWidget;

    /* Internals: */
    UISettingsCacheMachineParallel m_cache;
};

#endif // __UIMachineSettingsParallel_h__

