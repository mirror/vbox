/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSerial class declaration
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

#ifndef __UIMachineSettingsSerial_h__
#define __UIMachineSettingsSerial_h__

#include "UISettingsPage.h"
#include "UIMachineSettingsSerial.gen.h"
#include "COMDefs.h"

class QITabWidget;

/* Machine settings / Network page / Port data: */
struct UISerialPortData
{
    int m_iSlot;
    bool m_fPortEnabled;
    ulong m_uIRQ;
    ulong m_uIOBase;
    KPortMode m_hostMode;
    bool m_fServer;
    QString m_strPath;
};

/* Machine settings / Serial page / Cache: */
struct UISettingsCacheMachineSerial
{
    QList<UISerialPortData> m_items;
};

class UIMachineSettingsSerial : public QIWithRetranslateUI<QWidget>,
                             public Ui::UIMachineSettingsSerial
{
    Q_OBJECT;

public:

    UIMachineSettingsSerial();

    void fetchPortData(const UISerialPortData &data);
    void uploadPortData(UISerialPortData &data);

    void setValidator (QIWidgetValidator *aVal);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbSerialToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);
    void mCbModeActivated (const QString &aText);

private:

    QIWidgetValidator *mValidator;
    int m_iSlot;
};

/* Machine settings / Serial page: */
class UIMachineSettingsSerialPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    UIMachineSettingsSerialPage();

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

    /* Cache: */
    UISettingsCacheMachineSerial m_cache;
};

#endif // __UIMachineSettingsSerial_h__

