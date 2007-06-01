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
    enum Icon {
        NoIcon = QMessageBox::NoIcon,
        Information = QMessageBox::Information,
        Warning = QMessageBox::Warning,
        Critical = QMessageBox::Critical,
		Question = QMessageBox::Question
    };
    enum {
        NoButton = 0, Ok = 1, Cancel = 2, Yes = 3, No = 4, Abort = 5,
        Retry = 6, Ignore = 7, YesAll = 8, NoAll = 9, ButtonMask = 0xff,
        Default = 0x100, Escape = 0x200, FlagMask = 0x300
    };

    QIMessageBox (
        const QString &caption, const QString &text,
        Icon icon, int button0, int button1 = 0, int button2 = 0,
        QWidget *parent = 0, const char *name = 0, bool modal = TRUE,
        WFlags f = WStyle_DialogBorder
    );

    QString flagText() const { return cbflag->isShown() ? cbflag->text() : QString::null; }
    void setFlagText (const QString &text);

    bool isFlagChecked() const { return cbflag->isChecked(); }
    void setFlagChecked (bool checked) { cbflag->setChecked (checked); }

    QString detailsText () const { return dtext->text(); }
    void setDetailsText (const QString &text);

    bool isDetailsShown() const { return dbox->isShown(); }
    void setDetailsShown (bool shown);

private:

    QPushButton *createButton (QWidget *parent, int button);

private slots:

    void done0() { done (b0 & ButtonMask); }
    void done1() { done (b1 & ButtonMask); }
    void done2() { done (b2 & ButtonMask); }

    void reject() {
        QDialog::reject();
        if (bescape)
            setResult (bescape & ButtonMask);
    }

private:

    int b0, b1, b2, bescape;
    QLabel *licon, *ltext;
    QPushButton *pb0, *pb1, *pb2;
    QVBox *message;
    QCheckBox *cbflag;
    QVBox *dbox;
    QTextEdit *dtext;
    QSpacerItem *spacer;
};

#endif
