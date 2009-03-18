/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMessageBox class declaration
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

#ifndef __QIMessageBox_h__
#define __QIMessageBox_h__

/* VBox includes */
#include "QIDialog.h"

/* Qt includes */
#include <QCheckBox>
#include <QMessageBox>
#include <QTextEdit>

/* VBox forwards */
class QIArrowSplitter;
class QIDialogButtonBox;
class QILabel;

/* Qt forwards */
class QCloseEvent;
class QLabel;
class QPushButton;
class QSpacerItem;

/** @class QIMessageBox
 *
 *  The QIMessageBox class is a message box similar to QMessageBox.
 *  It partly implements the QMessageBox interface and adds some enhanced
 *  functionality.
 */
class QIMessageBox : public QIDialog
{
    Q_OBJECT;

public:

    // for compatibility with QMessageBox
    enum Icon
    {
        NoIcon = QMessageBox::NoIcon,
        Information = QMessageBox::Information,
        Warning = QMessageBox::Warning,
        Critical = QMessageBox::Critical,
        Question = QMessageBox::Question,
        GuruMeditation,
    };

    enum
    {
        NoButton = 0, Ok = 1, Cancel = 2, Yes = 3, No = 4, Abort = 5,
        Retry = 6, Ignore = 7, YesAll = 8, NoAll = 9,
        ButtonMask = 0xFF,

        Default = 0x100, Escape = 0x200,
        FlagMask = 0x300
    };

    QIMessageBox (const QString &aCaption, const QString &aText,
                  Icon aIcon, int aButton0, int aButton1 = 0, int aButton2 = 0,
                  QWidget *aParent = 0, const char *aName = 0, bool aModal = TRUE);

    QString buttonText (int aButton) const;
    void setButtonText (int aButton, const QString &aText);

    QString flagText() const { return mFlagCB->isVisible() ? mFlagCB->text() : QString::null; }
    void setFlagText (const QString &aText);

    bool isFlagChecked() const { return mFlagCB->isChecked(); }
    void setFlagChecked (bool aChecked) { mFlagCB->setChecked (aChecked); }

    QString detailsText () const { return mDetailsText->toHtml(); }
    void setDetailsText (const QString &aText);

    QPixmap standardPixmap (QIMessageBox::Icon aIcon);

private:

    QPushButton *createButton (int aButton);

    void closeEvent (QCloseEvent *e);
    void showEvent (QShowEvent *e);

    void refreshDetails();
    void setDetailsShown (bool aShown);

private slots:

    void detailsBack();
    void detailsNext();

    void done0() { mWasDone = true; done (mButton0 & ButtonMask); }
    void done1() { mWasDone = true; done (mButton1 & ButtonMask); }
    void done2() { mWasDone = true; done (mButton2 & ButtonMask); }

    void reject();

private:

    int mButton0, mButton1, mButton2, mButtonEsc;
    QLabel *mIconLabel;
    QILabel *mTextLabel;
    QPushButton *mButton0PB, *mButton1PB, *mButton2PB;
    QCheckBox *mFlagCB, *mFlagCB_Main, *mFlagCB_Details;
    QWidget *mDetailsVBox;
    QIArrowSplitter *mDetailsSplitter;
    QTextEdit *mDetailsText;
    QSpacerItem *mSpacer;
    QIDialogButtonBox *mButtonBox;
    QString mText;
    QList < QPair <QString, QString> > mDetailsList;
    int mDetailsIndex;
    bool mWasDone : 1;
    bool mWasPolished : 1;
};

#endif

