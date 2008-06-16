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
#include "VBoxRegistrationDlg.gen.h"
#include "COMDefs.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QUrl>

class VBoxNetworkFramework;

class VBoxRegistrationDlg : public QIWithRetranslateUI2<QIAbstractWizard>,
                            public Ui::VBoxRegistrationDlg
{
    Q_OBJECT;

public:

    VBoxRegistrationDlg (VBoxRegistrationDlg **aSelf, QWidget *aParent = 0,
                         Qt::WindowFlags aFlags = 0);
   ~VBoxRegistrationDlg();

    static bool hasToBeShown();

protected:

    void retranslateUi();

private slots:

    void accept();
    void reject();
    void handshake();
    void registration();
    void processTimeout();
    void onNetBegin (int aStatus);
    void onNetData (const QByteArray &aData);
    void onNetEnd (const QByteArray &aData);
    void onNetError (const QString &aError);

    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);
    void onPageShow();

private:

    void postRequest (const QString &aHost, const QString &aPath);
    void abortRegisterRequest (const QString &aReason);
    QString getPlatform();
    void finish();

    VBoxRegistrationDlg **mSelf;
    QIWidgetValidator    *mWvalReg;
    QUrl                  mUrl;
    QString               mKey;
    QTimer               *mTimeout;
    bool                  mHandshake;
    bool                  mSuicide;
    VBoxNetworkFramework *mNetfw;
};

#endif // __VBoxRegistrationDlg_h__

