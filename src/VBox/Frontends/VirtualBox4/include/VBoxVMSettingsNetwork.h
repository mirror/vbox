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

class QIWidgetValidator;
class VBoxVMSettingsDlg;
#ifdef Q_WS_WIN
class QTreeWidget;
class QTreeWidgetItem;
#endif

/*
 * QWidget sub-class which represents one tab-page per each network adapter.
 * It has generated UI part.
 */
class VBoxVMSettingsNetwork : public QIWithRetranslateUI<QWidget>,
                              public Ui::VBoxVMSettingsNetwork
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetwork();

    void getFromAdapter (const CNetworkAdapter &aAdapter);
    void putBackToAdapter();

    QString pageTitle() const;

    void setValidator (QIWidgetValidator *aValidator);
    void setNetworksList (const QStringList &aList);

#ifdef Q_WS_WIN
    void setInterfaceName (const QString &);
    QString interfaceName() const;
#endif

protected:

    void retranslateUi();

private slots:

    void adapterToggled (bool aOn);
    void naTypeChanged (const QString &aString);
    void genMACClicked();
#ifdef Q_WS_X11
    void tapSetupClicked();
    void tapTerminateClicked();
#endif

private:

    void prepareComboboxes();

    CNetworkAdapter mAdapter;
    QIWidgetValidator *mValidator;

#ifdef Q_WS_WIN
    QString mInterfaceName;
#endif
};


#ifdef Q_WS_WIN
/*
 * QGroupBox sub-class which represents network interface list.
 */
class VBoxNIList : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

public:

    VBoxNIList (QWidget *aParent);

    bool isWrongInterface() const;
    void setCurrentInterface (const QString &aName);

signals:

    void listChanged();
    void currentInterfaceChanged (const QString &);

private slots:

    void onCurrentItemChanged (QTreeWidgetItem *aCurrent, QTreeWidgetItem *aPrev = 0);
    void addHostInterface();
    void delHostInterface();

protected:

    void retranslateUi();

private:

    void populateInterfacesList();

    QTreeWidget *mList;

    QAction *mAddAction;
    QAction *mDelAction;
};
#endif


/*
 * QWidget sub-class which represents network settings page itself.
 */
class VBoxVMSettingsNetworkPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);
    static void putBackToMachine();
    static bool revalidate (QString &aWarning, QString &aTitle);

protected slots:

    void updateNetworksList();
#ifdef Q_WS_WIN
    void onCurrentPageChanged (int);
    void onCurrentInterfaceChanged (const QString &);
#endif

protected:

    VBoxVMSettingsNetworkPage (QWidget *aParent);

    void getFrom (const CMachine &aMachine,
                  VBoxVMSettingsDlg *aDlg,
                  const QString &aPath);
    void putBackTo();
    bool validate (QString &aWarning, QString &aTitle);

    void retranslateUi();

    void populateNetworksList();

    static VBoxVMSettingsNetworkPage *mSettings;

    /* Widgets */
    QTabWidget *mTwAdapters;
#ifdef Q_WS_WIN
    VBoxNIList *mNIList;
#endif

    /* Lists */
    QStringList mListNetworks;

    /* Flags */
    bool mLockNetworkListUpdate;
};

#endif // __VBoxVMSettingsNetwork_h__

