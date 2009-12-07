/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxLineTextEdit class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef __VBoxLineTextEdit_h__
#define __VBoxLineTextEdit_h__

/* VBox includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QPushButton>

/* Qt forward declarations */
class QTextEdit;
class QDialogButtonBox;

////////////////////////////////////////////////////////////////////////////////
// VBoxTextEditor

class VBoxTextEditor: public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:
    VBoxTextEditor (QWidget *aParent = NULL);

    void setText (const QString& aText);
    QString text() const;

protected:
    void retranslateUi();

private slots:
    void open();

private:
    /* Private member vars */
    QTextEdit *mTextEdit;
    QDialogButtonBox *mButtonBox;
    QPushButton *mOpenBtn;
};

////////////////////////////////////////////////////////////////////////////////
// VBoxLineTextEdit

class VBoxLineTextEdit: public QIWithRetranslateUI<QPushButton>
{
    Q_OBJECT;

public:
    VBoxLineTextEdit (QWidget *aParent = NULL);

    void setText (const QString& aText) { mText = aText; }
    QString text() const { return mText; }

protected:
    void retranslateUi();

private slots:
    void edit();

private:
    /* Private member vars */
    QString mText;
};

#endif /* __VBoxLineTextEdit_h__ */

