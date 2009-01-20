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

#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsNetwork.gen.h"
#include "COMDefs.h"

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
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

    QWidget* setOrderAfter (QWidget *aAfter);

    void setNetworksList (const QStringList &aList);

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    void setInterfaceName (const QString &);
    QString interfaceName() const;
#endif

protected:

    void retranslateUi();

private slots:

    void adapterToggled (bool aOn);
    void naTypeChanged (const QString &aString);
    void genMACClicked();
#if defined (Q_WS_X11) && !defined (VBOX_WITH_NETFLT)
    void tapSetupClicked();
    void tapTerminateClicked();
#endif

private:

    void prepareComboboxes();

    void setTapEnabled (bool aEnabled);
    void setTapVisible (bool aVisible);

    CNetworkAdapter mAdapter;
    QIWidgetValidator *mValidator;

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    QString mInterfaceName;
#endif
};


#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
/*
 * QGroupBox sub-class which represents network interface list.
 */
class VBoxNIList : public QIWithRetranslateUI<QWidget>
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

    QILabelSeparator *mLbTitle;
    QTreeWidget      *mList;

# if defined (Q_WS_WIN) && !defined(VBOX_WITH_NETFLT)
    QAction *mAddAction;
    QAction *mDelAction;
# endif
};
#endif /* Q_WS_WIN || VBOX_WITH_NETFLT */


/*
 * QWidget sub-class which represents network settings page itself.
 */
class VBoxVMSettingsNetworkPage : public VBoxSettingsPage
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetworkPage();

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void retranslateUi();

private slots:

    void updateNetworksList();
#if defined (VBOX_WITH_NETFLT)
    void updateInterfaceList();
#endif
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    void onCurrentPageChanged (int);
    void onCurrentInterfaceChanged (const QString &);
#endif

private:

    void populateNetworksList();

    /* Widgets */
    QTabWidget *mTwAdapters;
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    VBoxNIList *mNIList;
#endif

    /* Widget Validator*/
    QIWidgetValidator *mValidator;

    /* Lists */
    QStringList mListNetworks;

    /* Flags */
    bool mLockNetworkListUpdate;
};

#endif // __VBoxVMSettingsNetwork_h__

