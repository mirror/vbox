/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGlobalSettingsDlg class declaration
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

#ifndef __VBoxGlobalSettingsDlg_h__
#define __VBoxGlobalSettingsDlg_h__

#include "VBoxGlobalSettingsDlg.gen.h"
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"
#include "VBoxGlobalSettings.h"

class QIWidgetValidator;
class VBoxMediaComboBox;
class VBoxWarnIconLabel;

class QTimer;

class VBoxGlobalSettingsDlg : public QIWithRetranslateUI<QIMainDialog>,
                              public Ui::VBoxGlobalSettingsDlg
{
    Q_OBJECT;

public:

    VBoxGlobalSettingsDlg (QWidget *aParent);

    void getFrom (const CSystemProperties &aProps,
                  const VBoxGlobalSettings &aGs);
    COMResult putBackTo (CSystemProperties &aProps,
                         VBoxGlobalSettings &aGs);

protected:

    void retranslateUi();

private slots:

    void enableOk (const QIWidgetValidator*);
    void revalidate (QIWidgetValidator *aWval);
    void settingsGroupChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrev);
    void updateWhatsThis (bool aGotFocus = false);
    void whatsThisCandidateDestroyed (QObject *aObj = NULL);

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

    QString pagePath (QWidget *aPage);
    void setWarning (const QString &aWarning);
    void updateMediaShortcuts();

    /* Common */
    QString  mWarnString;
    VBoxWarnIconLabel *mWarnIconLabel;

    /* Flags */
    bool mPolished;
    bool mValid;

    /* WhatsThis Stuff */
    QTimer  *mWhatsThisTimer;
    QWidget *mWhatsThisCandidate;
};

#endif // __VBoxGlobalSettingsDlg_h__

