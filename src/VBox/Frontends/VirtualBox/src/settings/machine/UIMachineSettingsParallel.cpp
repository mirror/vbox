/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsParallel class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "UIMachineSettingsParallel.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"
#include "QITabWidget.h"

#include <QDir>

/* UIMachineSettingsParallel stuff */
UIMachineSettingsParallel::UIMachineSettingsParallel()
    : QIWithRetranslateUI<QWidget> (0)
    , mValidator(0)
    , m_iSlot(-1)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsParallel::setupUi (this);

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

void UIMachineSettingsParallel::fetchPortData(const UIParallelPortData &data)
{
    /* Load port slot number: */
    m_iSlot = data.m_iSlot;

    /* Fetch port data: */
    mGbParallel->setChecked(data.m_fPortEnabled);
    mCbNumber->setCurrentIndex(mCbNumber->findText(vboxGlobal().toCOMPortName(data.m_uIRQ, data.m_uIOBase)));
    mLeIRQ->setText(QString::number(data.m_uIRQ));
    mLeIOPort->setText("0x" + QString::number(data.m_uIOBase, 16).toUpper());
    mLePath->setText(data.m_strPath);

    /* Ensure everything is up-to-date */
    mGbParallelToggled(mGbParallel->isChecked());
}

void UIMachineSettingsParallel::uploadPortData(UIParallelPortData &data)
{
    /* Upload port data: */
    data.m_fPortEnabled = mGbParallel->isChecked();
    data.m_uIRQ = mLeIRQ->text().toULong(NULL, 0);
    data.m_uIOBase = mLeIOPort->text().toULong(NULL, 0);
    data.m_strPath = QDir::toNativeSeparators(mLePath->text());
}

void UIMachineSettingsParallel::setValidator (QIWidgetValidator *aVal)
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

QWidget* UIMachineSettingsParallel::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mGbParallel);
    setTabOrder (mGbParallel, mCbNumber);
    setTabOrder (mCbNumber, mLeIRQ);
    setTabOrder (mLeIRQ, mLeIOPort);
    setTabOrder (mLeIOPort, mLePath);
    return mLePath;
}

QString UIMachineSettingsParallel::pageTitle() const
{
    return QString(tr("Port %1", "parallel ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsParallel::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mCbNumber->currentText(), a, b);
}

void UIMachineSettingsParallel::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsParallel::retranslateUi (this);

    mCbNumber->setItemText (mCbNumber->count() - 1, vboxGlobal().toCOMPortName (0, 0));
}

void UIMachineSettingsParallel::mGbParallelToggled (bool aOn)
{
    if (aOn)
        mCbNumberActivated (mCbNumber->currentText());
    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsParallel::mCbNumberActivated (const QString &aText)
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


/* UIMachineSettingsParallelPage stuff */
UIMachineSettingsParallelPage::UIMachineSettingsParallelPage()
    : mValidator(0)
    , mTabWidget(0)
{
    /* TabWidget creation */
    mTabWidget = new QITabWidget (this);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 5, 0, 5);
    layout->addWidget (mTabWidget);
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsParallelPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Load port data: */
    ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetParallelPortCount();
    for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Get port: */
        const CParallelPort &port = m_machine.GetParallelPort(uSlot);

        /* Prepare port's data container: */
        UIParallelPortData data;

        /* Load options: */
        data.m_iSlot = uSlot;
        data.m_fPortEnabled = port.GetEnabled();
        data.m_uIRQ = port.GetIRQ();
        data.m_uIOBase = port.GetIOBase();
        data.m_strPath = port.GetPath();

        /* Append adapter's data container: */
        m_cache.m_items << data;
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsParallelPage::getFromCache()
{
    Assert(m_pFirstWidget);
    setTabOrder(m_pFirstWidget, mTabWidget->focusProxy());
    QWidget *pLastFocusWidget = mTabWidget->focusProxy();

    /* Apply internal variables data to QWidget(s): */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Creating port's page: */
        UIMachineSettingsParallel *pPage = new UIMachineSettingsParallel;

        /* Loading port's data into page: */
        pPage->fetchPortData(m_cache.m_items[iSlot]);

        /* Attach port's page to Tab Widget: */
        mTabWidget->addTab(pPage, pPage->pageTitle());

        /* Setup page validation: */
        pPage->setValidator(mValidator);

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Revalidate if possible: */
    if (mValidator) mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsParallelPage::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Getting adapter's page: */
        UIMachineSettingsParallel *pPage = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iSlot));

        /* Loading Adapter's data from page: */
        pPage->uploadPortData(m_cache.m_items[iSlot]);
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsParallelPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Gather corresponding values from internal variables: */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Get adapter: */
        CParallelPort port = m_machine.GetParallelPort(iSlot);

        /* Get cached data for this slot: */
        const UIParallelPortData &data = m_cache.m_items[iSlot];

        /* Save options: */
        port.SetIRQ(data.m_uIRQ);
        port.SetIOBase(data.m_uIOBase);
        port.SetPath(data.m_strPath);
        port.SetEnabled(data.m_fPortEnabled);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsParallelPage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool UIMachineSettingsParallelPage::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;
    QStringList ports;
    QStringList paths;

    int index = 0;
    for (; index < mTabWidget->count(); ++ index)
    {
        QWidget *tab = mTabWidget->widget (index);
        UIMachineSettingsParallel *page =
            static_cast<UIMachineSettingsParallel*> (tab);

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

void UIMachineSettingsParallelPage::retranslateUi()
{
    for (int i = 0; i < mTabWidget->count(); ++ i)
    {
        UIMachineSettingsParallel *page =
            static_cast<UIMachineSettingsParallel*> (mTabWidget->widget (i));
        mTabWidget->setTabText (i, page->pageTitle());
    }
}

