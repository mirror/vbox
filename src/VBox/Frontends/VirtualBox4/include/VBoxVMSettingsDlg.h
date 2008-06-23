/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsDlg class declaration
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

#ifndef __VBoxVMSettingsDlg_h__
#define __VBoxVMSettingsDlg_h__

#include "VBoxVMSettingsDlg.gen.h"
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class QIWidgetValidator;
class VBoxMediaComboBox;
class VBoxWarnIconLabel;

class QTimer;

class VBoxVMSettingsDlg : public QIWithRetranslateUI<QIMainDialog>,
                          public Ui::VBoxVMSettingsDlg
{
    Q_OBJECT;

public:

    VBoxVMSettingsDlg (QWidget *aParent, const QString &aCategory,
                                         const QString &aControl);

    void getFromMachine (const CMachine &aMachine);
    COMResult putBackToMachine();

protected:

    void retranslateUi();

private slots:

    void enableOk (const QIWidgetValidator*);
    void revalidate (QIWidgetValidator *aWval);
    void onMediaEnumerationDone();
    void settingsGroupChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrev);
    void updateWhatsThis (bool gotFocus = false);
    void resetFirstRunFlag();
    void whatsThisCandidateDestroyed (QObject *aObj = NULL);

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

    QString pagePath (QWidget *aPage);
    void setWarning (const QString &aWarning);

    QString dialogTitle() const;

    void updateAvailability();

    /* Common */
    CMachine mMachine;
    QString  mWarnString;
    VBoxWarnIconLabel *mWarnIconLabel;

    /* Flags */
    bool mPolished;
    bool mAllowResetFirstRunFlag;
    bool mResetFirstRunFlag;
    bool mValid;

    /* WhatsThis Stuff */
    QTimer  *mWhatsThisTimer;
    QWidget *mWhatsThisCandidate;
};

#endif // __VBoxVMSettingsDlg_h__

