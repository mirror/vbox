/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSettingsDialog class declaration
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

#ifndef __VBoxSettingsDialog_h__
#define __VBoxSettingsDialog_h__

/* VBox includes */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "VBoxSettingsDialog.gen.h"

/* Qt forwards */
class QIWidgetValidator;
class QStackedWidget;
class QTimer;

/* VBox forwards*/
class VBoxWarnIconLabel;
class VBoxSettingsSelector;
class VBoxSettingsPage;

/*
 * Base dialog class for both Global & VM settings which
 * encapsulates most of their similar functionalities.
 */
class VBoxSettingsDialog : public QIWithRetranslateUI<QIMainDialog>,
                           public Ui::VBoxSettingsDialog
{
    Q_OBJECT;

public:

    VBoxSettingsDialog (QWidget *aParent = NULL);

    virtual void getFrom() = 0;
    virtual void putBackTo() = 0;

protected slots:

    virtual void revalidate (QIWidgetValidator *aWval);
    void categoryChanged (int aId);

protected:

    virtual void retranslateUi();

    virtual QString dialogTitle() const = 0;
    QString titleExtension() const;

    void setError (const QString &aError);
    void setWarning (const QString &aWarning);

    void addItem (const QString &aBigIcon, const QString &aBigIconDisabled,
                  const QString &aSmallIcon, const QString &aSmallIconDisabled,
                  int aId, const QString &aLink,
                  VBoxSettingsPage* aPrefPage = NULL, int aParentId = -1);

    VBoxSettingsSelector *mSelector;
    QStackedWidget *mStack;

private slots:

    void enableOk (const QIWidgetValidator *aWval);
    void updateWhatsThis (bool aGotFocus = false);
    void whatsThisCandidateDestroyed (QObject *aObj = 0);

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

    VBoxSettingsPage* attachValidator (VBoxSettingsPage *aPage);

    /* Flags */
    bool mPolished;

    /* Error & Warning stuff */
    bool mValid;
    bool mSilent;
    QString mErrorHint;
    QString mWarnHint;
    QString mErrorString;
    QString mWarnString;
    QPixmap mErrorIcon;
    QPixmap mWarnIcon;
    VBoxWarnIconLabel *mIconLabel;

    /* WhatsThis Stuff */
    QTimer *mWhatsThisTimer;
    QWidget *mWhatsThisCandidate;

#ifdef Q_WS_MAC
    QList<QSize> mSizeList;
#endif /* Q_WS_MAC */
};

#endif // __VBoxSettingsDialog_h__

