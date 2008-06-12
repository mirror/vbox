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
#include "VBoxVMSettingsDlg.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>

QTabWidget* VBoxVMSettingsParallel::mTabWidget = 0;

VBoxVMSettingsParallel::VBoxVMSettingsParallel()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsParallel::setupUi (this);

    /* Setup validation */
    mLeIRQ->setValidator (new QIULongValidator (0, 255, this));
    mLeIOPort->setValidator (new QIULongValidator (0, 0xFFFF, this));
    mLePath->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    /* Setup constraints */
    mLeIRQ->setMaximumWidth (mLeIRQ->fontMetrics().width ("888888"));
    mLeIRQ->setMinimumWidth (mLeIRQ->minimumWidth());
    mLeIOPort->setMaximumWidth (mLeIOPort->fontMetrics().width ("8888888"));
    mLeIOPort->setMinimumWidth (mLeIOPort->minimumWidth());

    /* Set initial values */
    mCbNumber->insertItem (0, vboxGlobal().toCOMPortName (0, 0));
    mCbNumber->insertItems (0, vboxGlobal().COMPortNames());

    /* Setup connections */
    connect (mGbParallel, SIGNAL (toggled (bool)),
             this, SLOT (mGbParallelToggled (bool)));
    connect (mCbNumber, SIGNAL (activated (const QString &)),
             this, SLOT (mCbNumberActivated (const QString &)));
}

void VBoxVMSettingsParallel::getFromMachine (const CMachine &aMachine,
                                             QWidget *aPage,
                                             VBoxVMSettingsDlg *aDlg,
                                             const QString &aPath)
{
    /* TabWidget creation */
    mTabWidget = new QTabWidget (aPage);
    mTabWidget->setSizePolicy (QSizePolicy::Expanding,
                               QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 5, 0, 0);
    layout->addWidget (mTabWidget);
    layout->addStretch();

    /* Tab pages loading */
    ulong count = vboxGlobal().virtualBox().
                  GetSystemProperties().GetParallelPortCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CParallelPort port = aMachine.GetParallelPort (slot);
        VBoxVMSettingsParallel *page = new VBoxVMSettingsParallel();
        page->getFromPort (port);
        QString pageTitle = QString (tr ("Port %1", "parallel ports"))
                                     .arg (port.GetSlot());
        mTabWidget->addTab (page, pageTitle);

        /* Setup validation. */
        QIWidgetValidator *wval =
            new QIWidgetValidator (QString ("%1: %2").arg (aPath, pageTitle),
                                   aPage, mTabWidget);
        connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
                 aDlg, SLOT (enableOk (const QIWidgetValidator *)));
        connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
                 aDlg, SLOT (revalidate (QIWidgetValidator *)));

        connect (page->mGbParallel, SIGNAL (toggled (bool)),
                 wval, SLOT (revalidate()));
        connect (page->mLeIRQ, SIGNAL (textChanged (const QString &)),
                 wval, SLOT (revalidate()));
        connect (page->mLeIOPort, SIGNAL (textChanged (const QString &)),
                 wval, SLOT (revalidate()));

        wval->revalidate();
    }
}

void VBoxVMSettingsParallel::putBackToMachine()
{
    for (int index = 0; index < mTabWidget->count(); ++ index)
    {
        VBoxVMSettingsParallel *page =
            (VBoxVMSettingsParallel*) mTabWidget->widget (index);
        Assert (page);
        page->putBackToPort();
    }
}

void VBoxVMSettingsParallel::mGbParallelToggled (bool aOn)
{
    if (aOn)
        mCbNumberActivated (mCbNumber->currentText());
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
    mPort.SetPath (QDir::convertSeparators (mLePath->text()));
}

bool VBoxVMSettingsParallel::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mCbNumber->currentText(), a, b);
}

bool VBoxVMSettingsParallel::revalidate (QString &aWarning, QString &aTitle)
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
                aWarning = tr ("Duplicate port number is selected ");
                aTitle += ": " +
                    mTabWidget->tabText (mTabWidget->indexOf (tab));
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
                    tr ("Port path is not specified ") :
                    tr ("Duplicate port path is entered ");
                aTitle += ": " +
                    mTabWidget->tabText (mTabWidget->indexOf (tab));
                break;
            }
            paths << path;
        }
    }

    return valid;
}

