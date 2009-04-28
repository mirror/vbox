/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxRegistrationDlg class declaration
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

#ifndef __VBoxRegistrationDlg_h__
#define __VBoxRegistrationDlg_h__

#include "QIAbstractWizard.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"
#include "VBoxRegistrationDlg.gen.h"

/* Qt includes */
#include <QUrl>

class QIHttp;

class VBoxRegistrationDlg : public QIWithRetranslateUI <QIAbstractWizard>,
                            public Ui::VBoxRegistrationDlg
{
    Q_OBJECT;

public:

    static bool hasToBeShown();

    VBoxRegistrationDlg (VBoxRegistrationDlg **aSelf, QWidget *aParent = 0);
   ~VBoxRegistrationDlg();

protected:

    void retranslateUi();

private slots:

    void tuneRadioButton (const QString &aNewText);

    void accept();
    void reject();
    void reinit();

    void handshakeStart();
    void handshakeResponse (bool aError);

    void registrationStart();
    void registrationResponse (bool aError);

    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);
    void onPageShow();

private:

    void postRequest (const QUrl &aUrl);
    void abortRequest (const QString &aReason);
    void finish();

    VBoxRegistrationDlg **mSelf;
    QIWidgetValidator    *mWvalReg;
    QUrl                  mUrl;
    QIHttp               *mHttp;
    QString               mKey;
};

#endif // __VBoxRegistrationDlg_h__

