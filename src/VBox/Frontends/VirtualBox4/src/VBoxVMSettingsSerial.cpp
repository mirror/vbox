/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSerial class implementation
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

#include "VBoxVMSettingsSerial.h"
#include "VBoxVMSettingsDlg.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>

QTabWidget* VBoxVMSettingsSerial::mTabWidget = 0;

VBoxVMSettingsSerial::VBoxVMSettingsSerial(QWidget* aParent /* = NULL */)
    : QIWithRetranslateUI<QWidget> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSerial::setupUi (this);

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
    /* Note: If you change one of the following don't forget retranslateUi. */
    mCbNumber->insertItem (0, vboxGlobal().toCOMPortName (0, 0));
    mCbNumber->insertItems (0, vboxGlobal().COMPortNames());

    mCbMode->addItem (""); /* KPortMode_Disconnected */
    mCbMode->addItem (""); /* KPortMode_HostPipe */
    mCbMode->addItem (""); /* KPortMode_HostDevice */

    /* Setup connections */
    connect (mGbSerial, SIGNAL (toggled (bool)),
             this, SLOT (mGbSerialToggled (bool)));
    connect (mCbNumber, SIGNAL (activated (const QString &)),
             this, SLOT (mCbNumberActivated (const QString &)));
    connect (mCbMode, SIGNAL (activated (const QString &)),
             this, SLOT (mCbModeActivated (const QString &)));
    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsSerial::getFromMachine (const CMachine &aMachine,
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
                  GetSystemProperties().GetSerialPortCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CSerialPort port = aMachine.GetSerialPort (slot);
        VBoxVMSettingsSerial *page = new VBoxVMSettingsSerial();
        page->getFromPort (port);
        mTabWidget->addTab (page, page->pageTitle());

        /* Setup validation. */
        QIWidgetValidator *wval =
            new QIWidgetValidator (QString ("%1: %2").arg (aPath, page->pageTitle()),
                                   aPage, mTabWidget);
        connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
                 aDlg, SLOT (enableOk (const QIWidgetValidator *)));
        connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
                 aDlg, SLOT (revalidate (QIWidgetValidator *)));

        connect (page->mGbSerial, SIGNAL (toggled (bool)),
                 wval, SLOT (revalidate()));
        connect (page->mLeIRQ, SIGNAL (textChanged (const QString &)),
                 wval, SLOT (revalidate()));
        connect (page->mLeIOPort, SIGNAL (textChanged (const QString &)),
                 wval, SLOT (revalidate()));
        connect (page->mCbMode, SIGNAL (activated (const QString &)),
                 wval, SLOT (revalidate()));

        wval->revalidate();
    }
}

void VBoxVMSettingsSerial::putBackToMachine()
{
    for (int index = 0; index < mTabWidget->count(); ++ index)
    {
        VBoxVMSettingsSerial *page =
            (VBoxVMSettingsSerial*) mTabWidget->widget (index);
        Assert (page);
        page->putBackToPort();
    }
}

void VBoxVMSettingsSerial::mGbSerialToggled (bool aOn)
{
    if (aOn)
    {
        mCbNumberActivated (mCbNumber->currentText());
        mCbModeActivated (mCbMode->currentText());
    }
}

void VBoxVMSettingsSerial::mCbNumberActivated (const QString &aText)
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

void VBoxVMSettingsSerial::mCbModeActivated (const QString &aText)
{
    KPortMode mode = vboxGlobal().toPortMode (aText);
    mCbPipe->setEnabled (mode == KPortMode_HostPipe);
    mLePath->setEnabled (mode != KPortMode_Disconnected);
}

QString VBoxVMSettingsSerial::pageTitle() const
{
    QString pageTitle;
    if (!mPort.isNull())
    {
        pageTitle = QString (tr ("Port %1", "serial ports"))
            .arg (mPort.GetSlot());
    }
    return pageTitle;
}

void VBoxVMSettingsSerial::getFromPort (const CSerialPort &aPort)
{
    mPort = aPort;

    mGbSerial->setChecked (mPort.GetEnabled());
    ulong IRQ = mPort.GetIRQ();
    ulong IOBase = mPort.GetIOBase();
    mCbNumber->setCurrentIndex (mCbNumber->
        findText (vboxGlobal().toCOMPortName (IRQ, IOBase)));
    mLeIRQ->setText (QString::number (IRQ));
    mLeIOPort->setText ("0x" + QString::number (IOBase, 16).toUpper());
    mCbMode->setCurrentIndex (mCbMode->
        findText (vboxGlobal().toString (mPort.GetHostMode())));
    mCbPipe->setChecked (mPort.GetServer());
    mLePath->setText (mPort.GetPath());

    /* Ensure everything is up-to-date */
    mGbSerialToggled (mGbSerial->isChecked());
}

void VBoxVMSettingsSerial::putBackToPort()
{
    mPort.SetEnabled (mGbSerial->isChecked());
    mPort.SetIRQ (mLeIRQ->text().toULong (NULL, 0));
    mPort.SetIOBase (mLeIOPort->text().toULong (NULL, 0));
    mPort.SetHostMode (vboxGlobal().toPortMode (mCbMode->currentText()));
    mPort.SetServer (mCbPipe->isChecked());
    mPort.SetPath (QDir::convertSeparators (mLePath->text()));
}

bool VBoxVMSettingsSerial::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mCbNumber->currentText(), a, b);
}


void VBoxVMSettingsSerial::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsSerial::retranslateUi (this);

    mTabWidget->setTabText (mTabWidget->indexOf (this), pageTitle());

    mCbNumber->setItemText (mCbNumber->count() - 1, vboxGlobal().toCOMPortName (0, 0));

    mCbMode->setItemText (2, vboxGlobal().toString (KPortMode_HostDevice));
    mCbMode->setItemText (1, vboxGlobal().toString (KPortMode_HostPipe));
    mCbMode->setItemText (0, vboxGlobal().toString (KPortMode_Disconnected));
}


bool VBoxVMSettingsSerial::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;
    QStringList ports;
    QStringList paths;

    int index = 0;
    for (; index < mTabWidget->count(); ++ index)
    {
        QWidget *tab = mTabWidget->widget (index);
        VBoxVMSettingsSerial *page =
            static_cast<VBoxVMSettingsSerial*> (tab);

        /* Check the predefined port number unicity */
        if (page->mGbSerial->isChecked() && !page->isUserDefined())
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
        KPortMode mode =
            vboxGlobal().toPortMode (page->mCbMode->currentText());
        if (mode != KPortMode_Disconnected)
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

