/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxLineTextEdit class definitions
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

/* VBox includes */
#include "VBoxLineTextEdit.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QPushButton>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QTextStream>
#include <QTextEdit>

////////////////////////////////////////////////////////////////////////////////
// VBoxTextEditor

VBoxTextEditor::VBoxTextEditor (QWidget *aParent /* = NULL */)
  : QIWithRetranslateUI<QIDialog> (aParent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    mainLayout->setMargin (12);

    /* We need a text editor */
    mTextEdit = new QTextEdit (this);
    mainLayout->addWidget (mTextEdit);
    /* and some buttons to interact with */
    mButtonBox = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    mOpenBtn = new QPushButton (this);
    mButtonBox->addButton (mOpenBtn, QDialogButtonBox::ActionRole);
    mainLayout->addWidget (mButtonBox);
    /* Connect the buttons so that they are useful */
    connect (mButtonBox, SIGNAL (accepted()),
             this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()),
             this, SLOT (reject()));
    connect (mOpenBtn, SIGNAL (clicked()),
             this, SLOT (open()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxTextEditor::setText (const QString& aText)
{
    mTextEdit->setText (aText);
}

QString VBoxTextEditor::text() const
{
    return mTextEdit->toPlainText();
}

void VBoxTextEditor::retranslateUi()
{
    setWindowTitle (tr ("Edit text"));
    mOpenBtn->setText (tr ("&Replace..."));
    mOpenBtn->setToolTip (tr ("Replaces the current text with the content of a given file."));
}

void VBoxTextEditor::open()
{
    QString fileName = vboxGlobal().getOpenFileName (vboxGlobal().documentsPath(), tr("Text (*.txt);;All (*.*)"), this, tr("Select a file to open..."));
    if (!fileName.isEmpty())
    {
        QFile file (fileName);
        if (file.open(QFile::ReadOnly))
        {
            QTextStream in (&file);
            mTextEdit->setPlainText (in.readAll());
        }
    }

}

////////////////////////////////////////////////////////////////////////////////
// VBoxLineTextEdit

VBoxLineTextEdit::VBoxLineTextEdit (QWidget *aParent /* = NULL */)
  : QIWithRetranslateUI<QPushButton> (aParent)
{
    connect (this, SIGNAL (clicked()),
             this, SLOT (edit()));

    setFocusPolicy (Qt::StrongFocus);
    retranslateUi();
}

void VBoxLineTextEdit::retranslateUi()
{
    QPushButton::setText (tr ("&Edit"));
}

void VBoxLineTextEdit::edit()
{
    VBoxTextEditor te (this);
    te.setText (mText);
    if (te.exec() == QDialog::Accepted)
        mText = te.text();
}

