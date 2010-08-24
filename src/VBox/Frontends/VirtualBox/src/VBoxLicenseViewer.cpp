/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxLicenseViewer class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#include "VBoxLicenseViewer.h"
#include "QIDialogButtonBox.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QFile>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

VBoxLicenseViewer::VBoxLicenseViewer (const QString &aFilePath)
    : QIWithRetranslateUI<QDialog> ()
      , mFilePath (aFilePath)
      , mLicenseText (0)
      , mAgreeButton (0)
      , mDisagreeButton (0)
{
#ifndef Q_WS_WIN
    /* Application icon. On Win32, it's built-in to the executable. */
    setWindowIcon (QIcon (":/VirtualBox_48px.png"));
#endif

    mLicenseText = new QTextBrowser (this);
    mAgreeButton = new QPushButton (this);
    mDisagreeButton = new QPushButton (this);
    QDialogButtonBox *dbb = new QIDialogButtonBox (this);
    dbb->addButton (mAgreeButton, QDialogButtonBox::AcceptRole);
    dbb->addButton (mDisagreeButton, QDialogButtonBox::RejectRole);

    connect (mLicenseText->verticalScrollBar(), SIGNAL (valueChanged (int)),
             SLOT (onScrollBarMoving (int)));
    connect (mAgreeButton, SIGNAL (clicked()), SLOT (accept()));
    connect (mDisagreeButton, SIGNAL (clicked()), SLOT (reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    mainLayout->setSpacing (10);
    VBoxGlobal::setLayoutMargin (mainLayout, 10);
    mainLayout->addWidget (mLicenseText);
    mainLayout->addWidget (dbb);

    mLicenseText->verticalScrollBar()->installEventFilter (this);

    resize (600, 450);

    retranslateUi();
}

int VBoxLicenseViewer::exec()
{
    /* read & show the license file */
    QFile file (mFilePath);
    if (file.open (QIODevice::ReadOnly))
    {
        mLicenseText->setText (file.readAll());
        return QDialog::exec();
    }
    else
    {
        vboxProblem().cannotOpenLicenseFile (this, mFilePath);
        return QDialog::Rejected;
    }
}

void VBoxLicenseViewer::retranslateUi()
{
    setWindowTitle (tr ("VirtualBox License"));

    mAgreeButton->setText (tr ("I &Agree"));
    mDisagreeButton->setText (tr ("I &Disagree"));
}

void VBoxLicenseViewer::onScrollBarMoving (int aValue)
{
    if (aValue == mLicenseText->verticalScrollBar()->maximum())
        unlockButtons();
}

void VBoxLicenseViewer::unlockButtons()
{
    mAgreeButton->setEnabled (true);
    mDisagreeButton->setEnabled (true);
}

void VBoxLicenseViewer::showEvent (QShowEvent *aEvent)
{
    QDialog::showEvent (aEvent);
    bool isScrollBarHidden = mLicenseText->verticalScrollBar()->isHidden()
        && !(windowState() & Qt::WindowMinimized);
    mAgreeButton->setEnabled (isScrollBarHidden);
    mDisagreeButton->setEnabled (isScrollBarHidden);
}

bool VBoxLicenseViewer::eventFilter (QObject *aObject, QEvent *aEvent)
{
    switch (aEvent->type())
    {
        case QEvent::Hide:
            if (aObject == mLicenseText->verticalScrollBar())
                /* Doesn't work on wm's like ion3 where the window starts
                 * maximized: isActiveWindow() */
                unlockButtons();
        default:
            break;
    }
    return QDialog::eventFilter (aObject, aEvent);
}

