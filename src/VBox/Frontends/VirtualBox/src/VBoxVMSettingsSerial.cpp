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
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"

#include <QDir>

/* VBoxVMSettingsSerial stuff */
VBoxVMSettingsSerial::VBoxVMSettingsSerial()
    : QIWithRetranslateUI<QWidget> (0)
    , mValidator (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSerial::setupUi (this);

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

    mCbMode->addItem (""); /* KPortMode_Disconnected */
    mCbMode->addItem (""); /* KPortMode_HostPipe */
    mCbMode->addItem (""); /* KPortMode_HostDevice */
    mCbMode->addItem (""); /* KPortMode_RawFile */

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
    mPort.SetServer (mCbPipe->isChecked());
    mPort.SetPath (QDir::toNativeSeparators (mLePath->text()));
    /* This *must* be last. The host mode will be changed to disconnected if
     * some of the necessary settings above will not meet the requirements for
     * the selected mode. */
    mPort.SetHostMode (vboxGlobal().toPortMode (mCbMode->currentText()));
}

void VBoxVMSettingsSerial::setValidator (QIWidgetValidator *aVal)
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

QWidget* VBoxVMSettingsSerial::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mGbSerial);
    setTabOrder (mGbSerial, mCbNumber);
    setTabOrder (mCbNumber, mLeIRQ);
    setTabOrder (mLeIRQ, mLeIOPort);
    setTabOrder (mLeIOPort, mCbMode);
    setTabOrder (mCbMode, mCbPipe);
    setTabOrder (mCbPipe, mLePath);
    return mLePath;
}

QString VBoxVMSettingsSerial::pageTitle() const
{
    QString pageTitle;
    if (!mPort.isNull())
    {
        pageTitle = QString (tr ("Port %1", "serial ports"))
            .arg (QString ("&%1").arg (mPort.GetSlot() + 1));
    }
    return pageTitle;
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

    mCbNumber->setItemText (mCbNumber->count() - 1, vboxGlobal().toCOMPortName (0, 0));

    mCbMode->setItemText (3, vboxGlobal().toString (KPortMode_RawFile));
    mCbMode->setItemText (2, vboxGlobal().toString (KPortMode_HostDevice));
    mCbMode->setItemText (1, vboxGlobal().toString (KPortMode_HostPipe));
    mCbMode->setItemText (0, vboxGlobal().toString (KPortMode_Disconnected));
}

void VBoxVMSettingsSerial::mGbSerialToggled (bool aOn)
{
    if (aOn)
    {
        mCbNumberActivated (mCbNumber->currentText());
        mCbModeActivated (mCbMode->currentText());
    }
    if (mValidator)
        mValidator->revalidate();
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
    mLbPath->setEnabled (mode != KPortMode_Disconnected);
    mLePath->setEnabled (mode != KPortMode_Disconnected);
    if (mValidator)
        mValidator->revalidate();
}


/* VBoxVMSettingsSerialPage stuff */
VBoxVMSettingsSerialPage::VBoxVMSettingsSerialPage()
    : mValidator (0)
{
    /* TabWidget creation */
    mTabWidget = new QTabWidget (this);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 5, 0, 5);
    layout->addWidget (mTabWidget);

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsSerialPage::getFrom (const CMachine &aMachine)
{
    Assert (mFirstWidget);
    setTabOrder (mFirstWidget, mTabWidget->focusProxy());
    QWidget *lastFocusWidget = mTabWidget->focusProxy();

    /* Tab pages loading */
    ulong count = vboxGlobal().virtualBox().
                  GetSystemProperties().GetSerialPortCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CSerialPort port = aMachine.GetSerialPort (slot);
        VBoxVMSettingsSerial *page = new VBoxVMSettingsSerial();
        page->getFromPort (port);
        mTabWidget->addTab (page, page->pageTitle());
        Assert (mValidator);
        page->setValidator (mValidator);
        lastFocusWidget = page->setOrderAfter (lastFocusWidget);
    }
}

void VBoxVMSettingsSerialPage::putBackTo()
{
    for (int index = 0; index < mTabWidget->count(); ++ index)
    {
        VBoxVMSettingsSerial *page =
            (VBoxVMSettingsSerial*) mTabWidget->widget (index);
        Assert (page);
        page->putBackToPort();
    }
}

void VBoxVMSettingsSerialPage::setValidator (QIWidgetValidator * aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsSerialPage::revalidate (QString &aWarning, QString &aTitle)
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
                    vboxGlobal().removeAccelMark (mTabWidget->tabText (mTabWidget->indexOf (tab)));
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
                if (!page->mGbSerial->isChecked())
                    page->mCbMode->setCurrentIndex (KPortMode_Disconnected);
                else
                {
                    aWarning = path.isEmpty() ?
                        tr ("Port path is not specified ") :
                        tr ("Duplicate port path is entered ");
                    aTitle += ": " +
                        vboxGlobal().removeAccelMark (mTabWidget->tabText (mTabWidget->indexOf (tab)));
                    break;
                }
            }
            paths << path;
        }
    }

    return valid;
}

void VBoxVMSettingsSerialPage::retranslateUi()
{
    for (int i = 0; i < mTabWidget->count(); ++ i)
    {
        VBoxVMSettingsSerial *page =
            static_cast<VBoxVMSettingsSerial*> (mTabWidget->widget (i));
        mTabWidget->setTabText (i, page->pageTitle());
    }
}

