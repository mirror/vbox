/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsParallel class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDir>

/* GUI includes: */
#include "UIMachineSettingsParallel.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"
#include "QITabWidget.h"

/* COM includes: */
#include "CParallelPort.h"

/* UIMachineSettingsParallel stuff */
UIMachineSettingsParallel::UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent)
    : QIWithRetranslateUI<QWidget> (0)
    , m_pParent(pParent)
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

void UIMachineSettingsParallel::polishTab()
{
    /* Polish port page: */
    ulong uIRQ, uIOBase;
    bool fStd = vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), uIRQ, uIOBase);
    mGbParallel->setEnabled(m_pParent->isMachineOffline());
    mLbNumber->setEnabled(m_pParent->isMachineOffline());
    mCbNumber->setEnabled(m_pParent->isMachineOffline());
    mLbIRQ->setEnabled(m_pParent->isMachineOffline());
    mLeIRQ->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbIOPort->setEnabled(m_pParent->isMachineOffline());
    mLeIOPort->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbPath->setEnabled(m_pParent->isMachineOffline());
    mLePath->setEnabled(m_pParent->isMachineOffline());
}

void UIMachineSettingsParallel::fetchPortData(const UICacheSettingsMachineParallelPort &portCache)
{
    /* Get port data: */
    const UIDataSettingsMachineParallelPort &portData = portCache.base();

    /* Load port number: */
    m_iSlot = portData.m_iSlot;

    /* Load port data: */
    mGbParallel->setChecked(portData.m_fPortEnabled);
    mCbNumber->setCurrentIndex(mCbNumber->findText(vboxGlobal().toCOMPortName(portData.m_uIRQ, portData.m_uIOBase)));
    mLeIRQ->setText(QString::number(portData.m_uIRQ));
    mLeIOPort->setText("0x" + QString::number(portData.m_uIOBase, 16).toUpper());
    mLePath->setText(portData.m_strPath);

    /* Ensure everything is up-to-date */
    mGbParallelToggled(mGbParallel->isChecked());
}

void UIMachineSettingsParallel::uploadPortData(UICacheSettingsMachineParallelPort &portCache)
{
    /* Prepare port data: */
    UIDataSettingsMachineParallelPort portData = portCache.base();

    /* Save port data: */
    portData.m_fPortEnabled = mGbParallel->isChecked();
    portData.m_uIRQ = mLeIRQ->text().toULong(NULL, 0);
    portData.m_uIOBase = mLeIOPort->text().toULong(NULL, 0);
    portData.m_strPath = QDir::toNativeSeparators(mLePath->text());

    /* Cache port data: */
    portCache.cacheCurrentData(portData);
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

    /* How many ports to display: */
    ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetParallelPortCount();
    /* Add corresponding tab pages to parent tab widget: */
    for (ulong uPort = 0; uPort < uCount; ++uPort)
    {
        /* Creating port page: */
        UIMachineSettingsParallel *pPage = new UIMachineSettingsParallel(this);
        mTabWidget->addTab(pPage, pPage->pageTitle());
    }
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsParallelPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* For each parallel port: */
    for (int iSlot = 0; iSlot < mTabWidget->count(); ++iSlot)
    {
        /* Prepare port data: */
        UIDataSettingsMachineParallelPort portData;

        /* Check if port is valid: */
        const CParallelPort &port = m_machine.GetParallelPort(iSlot);
        if (!port.isNull())
        {
            /* Gather options: */
            portData.m_iSlot = iSlot;
            portData.m_fPortEnabled = port.GetEnabled();
            portData.m_uIRQ = port.GetIRQ();
            portData.m_uIOBase = port.GetIOBase();
            portData.m_strPath = port.GetPath();
        }

        /* Cache port data: */
        m_cache.child(iSlot).cacheInitialData(portData);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsParallelPage::getFromCache()
{
    /* Setup tab order: */
    Assert(firstWidget());
    setTabOrder(firstWidget(), mTabWidget->focusProxy());
    QWidget *pLastFocusWidget = mTabWidget->focusProxy();

    /* For each parallel port: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        /* Get port page: */
        UIMachineSettingsParallel *pPage = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iPort));

        /* Load port data to page: */
        pPage->fetchPortData(m_cache.child(iPort));

        /* Setup page validation: */
        pPage->setValidator(mValidator);

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Polish page finally: */
    polishPage();

    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsParallelPage::putToCache()
{
    /* For each parallel port: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        /* Getting port page: */
        UIMachineSettingsParallel *pPage = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iPort));

        /* Gather & cache port data: */
        pPage->uploadPortData(m_cache.child(iPort));
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsParallelPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if ports data was changed: */
    if (m_cache.wasChanged())
    {
        /* For each parallel port: */
        for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
        {
            /* Check if port data was changed: */
            const UICacheSettingsMachineParallelPort &portCache = m_cache.child(iPort);
            if (portCache.wasChanged())
            {
                /* Check if port still valid: */
                CParallelPort port = m_machine.GetParallelPort(iPort);
                if (!port.isNull())
                {
                    /* Get port data from cache: */
                    const UIDataSettingsMachineParallelPort &portData = portCache.data();

                    /* Store adapter data: */
                    if (isMachineOffline())
                    {
                        port.SetIRQ(portData.m_uIRQ);
                        port.SetIOBase(portData.m_uIOBase);
                        port.SetPath(portData.m_strPath);
                        port.SetEnabled(portData.m_fPortEnabled);
                    }
                }
            }
        }
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

void UIMachineSettingsParallelPage::polishPage()
{
    /* Get the count of parallel port tabs: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        mTabWidget->setTabEnabled(iPort,
                                  isMachineOffline() ||
                                  (isMachineInValidMode() && m_cache.child(iPort).base().m_fPortEnabled));
        UIMachineSettingsParallel *pTab = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iPort));
        pTab->polishTab();
    }
}

