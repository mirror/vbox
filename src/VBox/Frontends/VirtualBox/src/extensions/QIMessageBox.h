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
#include <QMessageBox>

/* GUI includes: */
#include "QIDialog.h"

/* Forward declarations: */
class QShowEvent;
class QCloseEvent;
class QLabel;
class QTextEdit;
class QCheckBox;
class QPushButton;
class QILabel;
class QIArrowSplitter;
class QIDialogButtonBox;

/* Button type enumerator: */
enum AlertButton
{
    AlertButton_NoButton      =  0x0,  /* 00000000 00000000 */
    AlertButton_Ok            =  0x1,  /* 00000000 00000001 */
    AlertButton_Cancel        =  0x2,  /* 00000000 00000010 */
    AlertButton_Choice1       =  0x4,  /* 00000000 00000100 */
    AlertButton_Choice2       =  0x8,  /* 00000000 00001000 */
    AlertButton_Copy          = 0x10,  /* 00000000 00010000 */
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
    AlertOption_AutoConfirmed = 0x400, /* 00000100 00000000 */
    AlertOption_CheckBox      = 0x800, /* 00001000 00000000 */
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
    QIMessageBox(const QString &strCaption, const QString &strMessage, AlertIconType iconType,
                 int iButton1 = 0, int iButton2 = 0, int iButton3 = 0, QWidget *pParent = 0);

    /* API: Details stuff: */
    QString detailsText() const;
    void setDetailsText(const QString &strText);

    /* API: Flag stuff: */
    bool flagChecked() const;
    void setFlagChecked(bool fChecked);
    QString flagText() const;
    void setFlagText(const QString &strText);

    /* API: Button stuff: */
    QString buttonText(int iButton) const;
    void setButtonText(int iButton, const QString &strText);

private slots:

    /* Handler: Reject slot reimplementation: */
    void reject();

    /* Handlers: Done slot variants for up to three buttons: */
    void done1() { m_fDone = true; done(m_iButton1 & AlertButtonMask); }
    void done2() { m_fDone = true; done(m_iButton2 & AlertButtonMask); }
    void done3() { m_fDone = true; done(m_iButton3 & AlertButtonMask); }

    /* Handler: Copy button stuff: */
    void copy() const;

    /* Handlers: Details navigation stuff: */
    void detailsBack();
    void detailsNext();

    /* Handler: Update stuff: */
    void sltUpdateSize();

private:

    /* Helpers: Prepare stuff: */
    void prepareContent();
    QPushButton* createButton(int iButton);

    /* Handler: Event-processing stuff: */
    void polishEvent(QShowEvent *pPolishEvent);
    void closeEvent(QCloseEvent *pCloseEvent);

    /* Helpers: Update stuff: */
    void updateDetailsContainer();
    void updateDetailsPage();
    void updateCheckBox();

    /* Static helper: Standard pixmap stuff: */
    static QPixmap standardPixmap(AlertIconType iconType, QWidget *pWidget = 0);

    /* Variables: */
    int m_iButton1, m_iButton2, m_iButton3, m_iButtonEsc;
    AlertIconType m_iconType;
    QLabel *m_pIconLabel;
    QILabel *m_pTextLabel;
    QPushButton *m_pButton1, *m_pButton2, *m_pButton3;
    QCheckBox *m_pFlagCheckBox;
    QIArrowSplitter *m_pDetailsContainer;
    QTextEdit *m_pDetailsTextView;
    QIDialogButtonBox *m_pButtonBox;
    QString m_strMessage;
    QList<QPair<QString, QString> > m_details;
    int m_iDetailsIndex;
    bool m_fDone : 1;
};

#endif

