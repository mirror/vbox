/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsParallel class implementation
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

#include "VBoxVMSettingsParallel.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"

#include <QDir>

/* VBoxVMSettingsParallel stuff */
VBoxVMSettingsParallel::VBoxVMSettingsParallel()
    : QIWithRetranslateUI<QWidget> (0)
    , mValidator (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsParallel::setupUi (this);

    /* Setup validation */
    mLeIRQ->setValidator (new QIULongValidator (0, 255, this));
    mLeIOPort->setValidator (new QIULongValidator (0, 0xFFFF, this));
    mLePath->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    /* Setup constraints */
    mLeIRQ->setFixedWidth (mLeIRQ->fontMetrics().width ("8888"));
    mLeIOPort->setFixedWidth (mLeIOPort->fontMetrics().width ("8888888"));

    /* Set initial values */
    /* Note: If you change one of the following don't forget retranslateUi. */
    mCbNumber->insertItem (0, vboxGlobal().toCOMPortName (0, 0));
    mCbNumber->insertItems (0, vboxGlobal().COMPortNames());

    /* Setup connections */
    connect (mGbParallel, SIGNAL (toggled (bool)),
             this, SLOT (mGbParallelToggled (bool)));
    connect (mCbNumber, SIGNAL (activated (const QString &)),
             this, SLOT (mCbNumberActivated (const QString &)));

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsParallel::getFromPort (const CParallelPort &aPort)
{
    mPort = aPort;

    mGbParallel->setChecked (mPort.GetEnabled());
    ulong IRQ = mPort.GetIRQ();
    ulong IOBase = mPort.GetIOBase();
    mCbNumber->setCurrentIndex (mCbNumber->
        findText (vboxGlobal().toCOMPortName (IRQ, IOBase)));
    mLeIRQ->setText (QString::number (IRQ));
    mLeIOPort->setText ("0x" + QString::number (IOBase, 16).toUpper());
    mLePath->setText (mPort.GetPath());

    /* Ensure everything is up-to-date */
    mGbParallelToggled (mGbParallel->isChecked());
}

void VBoxVMSettingsParallel::putBackToPort()
{
    mPort.SetEnabled (mGbParallel->isChecked());
    mPort.SetIRQ (mLeIRQ->text().toULong (NULL, 0));
    mPort.SetIOBase (mLeIOPort->text().toULong (NULL, 0));
    mPort.SetPath (QDir::toNativeSeparators (mLePath->text()));
}

void VBoxVMSettingsParallel::setValidator (QIWidgetValidator *aVal)
{
    Assert (aVal);
    mValidator = aVal;
    connect (mLeIRQ, SIGNAL (textChanged (const QString &)),
             mValidator, SLOT (revalidate()));
    connect (mLeIOPort, SIGNAL (textChanged (const QString &)),
             mValidator, SLOT (revalidate()));
    connect (mLePath, SIGNAL (textChanged (const QString &)),
             mValidator, SLOT (revalidate()));
    mValidator->revalidate();
}

QWidget* VBoxVMSettingsParallel::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mGbParallel);
    setTabOrder (mGbParallel, mCbNumber);
    setTabOrder (mCbNumber, mLeIRQ);
    setTabOrder (mLeIRQ, mLeIOPort);
    setTabOrder (mLeIOPort, mLePath);
    return mLePath;
}

QString VBoxVMSettingsParallel::pageTitle() const
{
    QString pageTitle;
    if (!mPort.isNull())
        pageTitle = QString (tr ("Port %1", "parallel ports"))
            .arg (QString ("&%1").arg (mPort.GetSlot() + 1));
    return pageTitle;
}

bool VBoxVMSettingsParallel::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mCbNumber->currentText(), a, b);
}

void VBoxVMSettingsParallel::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsParallel::retranslateUi (this);

    mCbNumber->setItemText (mCbNumber->count() - 1, vboxGlobal().toCOMPortName (0, 0));
}

void VBoxVMSettingsParallel::mGbParallelToggled (bool aOn)
{
    if (aOn)
        mCbNumberActivated (mCbNumber->currentText());
    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsParallel::mCbNumberActivated (const QString &aText)
{
    ulong IRQ, IOBase;
    bool std = vboxGlobal().toCOMPortNumbers (aText, IRQ, IOBase);

    mLeIRQ->setEnabled (!std);
    mLeIOPort->setEnabled (!std);
    if (std)
    {
        mLeIRQ->setText (QString::number (IRQ));
        mLeIOPort->setText ("0x" + QString::number (IOBase, 16).toUpper());
    }
}


/* VBoxVMSettingsParallelPage stuff */
VBoxVMSettingsParallelPage::VBoxVMSettingsParallelPage()
    : mValidator (0)
{
    /* TabWidget creation */
    mTabWidget = new QTabWidget (this);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 5, 0, 5);
    layout->addWidget (mTabWidget);
}

void VBoxVMSettingsParallelPage::getFrom (const CMachine &aMachine)
{
    Assert (mFirstWidget);
    setTabOrder (mFirstWidget, mTabWidget->focusProxy());
    QWidget *lastFocusWidget = mTabWidget->focusProxy();

    /* Tab pages loading */
    ulong count = vboxGlobal().virtualBox().
                  GetSystemProperties().GetParallelPortCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CParallelPort port = aMachine.GetParallelPort (slot);
        VBoxVMSettingsParallel *page = new VBoxVMSettingsParallel();
        page->getFromPort (port);
        mTabWidget->addTab (page, page->pageTitle());
        Assert (mValidator);
        page->setValidator (mValidator);
        lastFocusWidget = page->setOrderAfter (lastFocusWidget);
    }
}

void VBoxVMSettingsParallelPage::putBackTo()
{
    for (int index = 0; index < mTabWidget->count(); ++ index)
    {
        VBoxVMSettingsParallel *page =
            (VBoxVMSettingsParallel*) mTabWidget->widget (index);
        Assert (page);
        page->putBackToPort();
    }
}

void VBoxVMSettingsParallelPage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsParallelPage::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;
    QStringList ports;
    QStringList paths;

    int index = 0;
    for (; index < mTabWidget->count(); ++ index)
    {
        QWidget *tab = mTabWidget->widget (index);
        VBoxVMSettingsParallel *page =
            static_cast<VBoxVMSettingsParallel*> (tab);

        /* Check the predefined port number unicity */
        if (page->mGbParallel->isChecked() && !page->isUserDefined())
        {
            QString port = page->mCbNumber->currentText();
            valid = !ports.contains (port);
            if (!valid)
            {
                aWarning = tr ("Duplicate port number selected ");
                aTitle += ": " +
                    vboxGlobal().removeAccelMark (mTabWidget->tabText (mTabWidget->indexOf (tab)));
                break;
            }
            ports << port;
        }

        /* Check the port path emptiness & unicity */
        if (page->mGbParallel->isChecked())
        {
            QString path = page->mLePath->text();
            valid = !path.isEmpty() && !paths.contains (path);
            if (!valid)
            {
                aWarning = path.isEmpty() ?
                    tr ("Port path not specified ") :
                    tr ("Duplicate port path entered ");
                aTitle += ": " +
                    vboxGlobal().removeAccelMark (mTabWidget->tabText (mTabWidget->indexOf (tab)));
                break;
            }
            paths << path;
        }
    }

    return valid;
}

void VBoxVMSettingsParallelPage::retranslateUi()
{
    for (int i = 0; i < mTabWidget->count(); ++ i)
    {
        VBoxVMSettingsParallel *page =
            static_cast<VBoxVMSettingsParallel*> (mTabWidget->widget (i));
        mTabWidget->setTabText (i, page->pageTitle());
    }
}

