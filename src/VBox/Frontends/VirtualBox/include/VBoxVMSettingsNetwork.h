/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetwork class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

/* VBox Includes */
#include "COMDefs.h"
#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsNetwork.gen.h"

/* VBox Forwardes */
class VBoxVMSettingsNetworkPage;
class VBoxVMSettingsNetworkDetails;

class VBoxVMSettingsNetwork : public QIWithRetranslateUI <QWidget>,
                              public Ui::VBoxVMSettingsNetwork
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetwork (VBoxVMSettingsNetworkPage *aParent);

    void getFromAdapter (const CNetworkAdapter &aAdapter);
    void putBackToAdapter();

    void setValidator (QIWidgetValidator *aValidator);
    bool revalidate (QString &aWarning, QString &aTitle);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    QString currentName (KNetworkAttachmentType aType = KNetworkAttachmentType_Null) const;

protected:

    void retranslateUi();

private slots:

    void updateAttachmentInfo();
    void detailsClicked();

private:

    void populateComboboxes();
    KNetworkAttachmentType attachmentType() const;

    VBoxVMSettingsNetworkPage *mParent;
    VBoxVMSettingsNetworkDetails *mDetails;
    CNetworkAdapter mAdapter;
    QIWidgetValidator *mValidator;
};

class VBoxVMSettingsNetworkPage : public VBoxSettingsPage
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetworkPage();

    QStringList intList (bool aRefresh = false);
    QStringList brgList (bool aRefresh = false);
    QStringList hoiList (bool aRefresh = false);

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aValidator);
    bool revalidate (QString &aWarning, QString &aTitle);

    void retranslateUi();

private:

    QIWidgetValidator *mValidator;
    QTabWidget *mTwAdapters;

    QStringList mIntList;
    QStringList mBrgList;
    QStringList mHoiList;
};

#endif // __VBoxVMSettingsNetwork_h__

