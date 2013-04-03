/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMessageBox class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIMessageBox_h__
#define __QIMessageBox_h__

/* Qt includes: */
#include <QCheckBox>
#include <QMessageBox>
#include <QTextEdit>

/* GUI includes: */
#include "QIDialog.h"

/* Forward declarations: */
class QCloseEvent;
class QLabel;
class QPushButton;
class QSpacerItem;
class QIArrowSplitter;
class QIDialogButtonBox;
class QILabel;

/* Button type enumerator: */
enum AlertButton
{
    AlertButton_NoButton      =  0x0,  /* 00000000 00000000 */
    AlertButton_Ok            =  0x1,  /* 00000000 00000001 */
    AlertButton_Cancel        =  0x2,  /* 00000000 00000010 */
    AlertButton_Yes           =  0x4,  /* 00000000 00000100 */
    AlertButton_No            =  0x8,  /* 00000000 00001000 */
    AlertButton_Ignore        = 0x10,  /* 00000000 00010000 */
    AlertButton_Copy          = 0x20,  /* 00000000 00100000 */
    AlertButtonMask           = 0xFF   /* 00000000 11111111 */
};

/* Button option enumerator: */
enum AlertButtonOption
{
    AlertButtonOption_Default = 0x100, /* 00000001 00000000 */
    AlertButtonOption_Escape  = 0x200, /* 00000010 00000000 */
    AlertButtonOptionMask     = 0x300  /* 00000011 00000000 */
};

/* Alert option enumerator: */
enum AlertOption
{
    AlertOption_CheckBox      = 0x400, /* 00000100 00000000 */
    AlertOptionMask           = 0xFC00 /* 11111100 00000000 */
};

/* Icon type enumerator: */
enum AlertIconType
{
    AlertIconType_NoIcon = QMessageBox::NoIcon,
    AlertIconType_Information = QMessageBox::Information,
    AlertIconType_Warning = QMessageBox::Warning,
    AlertIconType_Critical = QMessageBox::Critical,
    AlertIconType_Question = QMessageBox::Question,
    AlertIconType_GuruMeditation
};

/* QIDialog extension representing GUI alerts: */
class QIMessageBox : public QIDialog
{
    Q_OBJECT;

public:

    /* Constructor: */
    QIMessageBox(const QString &strCaption, const QString &strMessage, AlertIconType icon,
                 int iButton1 = 0, int iButton2 = 0, int iButton3 = 0, QWidget *pParent = 0);

    QString buttonText (int aButton) const;
    void setButtonText (int aButton, const QString &aText);

    QString flagText() const { return mFlagCB->isVisible() ? mFlagCB->text() : QString::null; }
    void setFlagText (const QString &aText);

    bool isFlagChecked() const { return mFlagCB->isChecked(); }
    void setFlagChecked (bool aChecked) { mFlagCB->setChecked (aChecked); }

    QString detailsText () const { return mDetailsText->toHtml(); }
    void setDetailsText (const QString &aText);

    QPixmap standardPixmap(AlertIconType aIcon);

private:

    QPushButton *createButton (int aButton);

    void closeEvent (QCloseEvent *e);
    void polishEvent(QShowEvent *pPolishEvent);

    void refreshDetails();
    void setDetailsShown (bool aShown);

private slots:

    void sltUpdateSize();

    void detailsBack();
    void detailsNext();

    void done0() { m_fDone = true; done (mButton1 & AlertButtonMask); }
    void done1() { m_fDone = true; done (mButton2 & AlertButtonMask); }
    void done2() { m_fDone = true; done (mButton3 & AlertButtonMask); }

    void reject();

    void copy() const;

private:

    int mButton1, mButton2, mButton3, mButtonEsc;
    QLabel *mIconLabel;
    QILabel *mTextLabel;
    QPushButton *mButton0PB, *mButton1PB, *mButton2PB;
    QCheckBox *mFlagCB, *mFlagCB_Main, *mFlagCB_Details;
    QWidget *mDetailsVBox;
    QIArrowSplitter *mDetailsSplitter;
    QTextEdit *mDetailsText;
    QSpacerItem *mSpacer;
    QIDialogButtonBox *mButtonBox;
    QString m_strMessage;
    QList < QPair <QString, QString> > mDetailsList;
    int m_iDetailsIndex;
    bool m_fDone : 1;
};

#endif

