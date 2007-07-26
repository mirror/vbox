/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIMessageBox class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __QIMessageBox_h__
#define __QIMessageBox_h__

#include <qdialog.h>
#include <qvbox.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <qtextedit.h>

class QLabel;
class QPushButton;
class QSpacerItem;

class QIMessageBox : public QDialog
{
    Q_OBJECT

public:

    // for compatibility with QMessageBox
    enum Icon
    {
        NoIcon = QMessageBox::NoIcon,
        Information = QMessageBox::Information,
        Warning = QMessageBox::Warning,
        Critical = QMessageBox::Critical,
		Question = QMessageBox::Question
    };

    enum
    {
        NoButton = 0, Ok = 1, Cancel = 2, Yes = 3, No = 4, Abort = 5,
        Retry = 6, Ignore = 7, YesAll = 8, NoAll = 9, ButtonMask = 0xff,
        Default = 0x100, Escape = 0x200, FlagMask = 0x300
    };

    QIMessageBox (const QString &aCaption, const QString &aText,
                  Icon aIcon, int aButton0, int aButton1 = 0, int aButton2 = 0,
                  QWidget *aParent = 0, const char *aName = 0, bool aModal = TRUE,
                  WFlags aFlags = WStyle_DialogBorder);

    QString flagText() const { return mFlagCB->isShown() ? mFlagCB->text() : QString::null; }
    void setFlagText (const QString &aText);

    bool isFlagChecked() const { return mFlagCB->isChecked(); }
    void setFlagChecked (bool aChecked) { mFlagCB->setChecked (aChecked); }

    QString detailsText () const { return mDetailsText->text(); }
    void setDetailsText (const QString &aText);

    bool isDetailsShown() const { return mDetailsVBox->isShown(); }
    void setDetailsShown (bool aShown);

private:

    QPushButton *createButton (QWidget *aParent, int aButton);

private slots:

    void done0() { done (mButton0 & ButtonMask); }
    void done1() { done (mButton1 & ButtonMask); }
    void done2() { done (mButton2 & ButtonMask); }

    void reject() {
        QDialog::reject();
        if (mButtonEsc)
            setResult (mButtonEsc & ButtonMask);
    }

private:

    int mButton0, mButton1, mButton2, mButtonEsc;
    QLabel *mIconLabel, *mTextLabel;
    QPushButton *mButton0PB, *mButton1PB, *mButton2PB;
    QVBox *mMessageVBox;
    QCheckBox *mFlagCB, *mFlagCB_Main, *mFlagCB_Details;
    QVBox *mDetailsVBox;
    QTextEdit *mDetailsText;
    QSpacerItem *mSpacer;
};

#endif
