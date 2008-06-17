/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetwork class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxVMSettingsNetwork_h__
#define __VBoxVMSettingsNetwork_h__

#include "VBoxVMSettingsNetwork.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxVMSettingsDlg;
class QIWidgetValidator;

class VBoxVMSettingsNetwork : public QIWithRetranslateUI<QWidget>,
                              public Ui::VBoxVMSettingsNetwork
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetwork (QWidget *aParent = NULL);

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);
    static void putBackToMachine();
    static bool revalidate (QString &aWarning, QString &aTitle);

    void loadListNetworks (const QStringList &aList);

    void getFromAdapter (const CNetworkAdapter &aAdapter);
    void putBackToAdapter();

    void setValidator (QIWidgetValidator *aValidator);

protected:

    void retranslateUi();

private slots:

    static void updateListNetworks();
// #ifdef Q_WS_WIN
//     static void addHostInterface();
//     static void delHostInterface();
// #endif

    void adapterToggled (bool aOn);
    void naTypeChanged (const QString &aString);
    void genMACClicked();
#ifdef Q_WS_X11
    void tapSetupClicked();
    void tapTerminateClicked();
#endif

private:

    QString pageTitle() const;
    void prepareComboboxes();

    static void prepareListNetworks();
// #ifdef Q_WS_WIN
//     static void prepareListInterfaces();
// #endif

    /* Widgets */
    static QTabWidget *mTwAdapters;
// #ifdef Q_WS_WIN
//     static QGroupBox *mGbInterfaces;
//     static QListWidget *mLwInterfaces;
// #endif

    /* Actions */
// #ifdef Q_WS_WIN
//     static QAction *mAddAction;
//     static QAction *mDelAction;
// #endif

    /* Flags */
    static bool mLockNetworkListUpdate;

    /* Lists */
    static QStringList mListNetworks;
// #ifdef Q_WS_WIN
//     static QStringList mListInterface;
//     static QString mNoInterfaces;
// #endif

    QIWidgetValidator *mValidator;
    CNetworkAdapter mAdapter;
// #ifdef Q_WS_WIN
//     QString mInterface_win;
// #endif
};

#endif // __VBoxVMSettingsNetwork_h__

