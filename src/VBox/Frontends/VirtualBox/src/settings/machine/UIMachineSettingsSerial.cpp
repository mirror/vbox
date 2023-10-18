/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSerial class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QVBoxLayout>

/* GUI includes: */
#include "QITabWidget.h"
#include "UICommon.h"
#include "UIErrorString.h"
#include "UIMachineSettingsSerial.h"
#include "UISerialSettingsEditor.h"
#include "UITranslator.h"

/* COM includes: */
#include "CPlatformProperties.h"
#include "CSerialPort.h"


/** Machine settings: Serial Port tab data structure. */
struct UIDataSettingsMachineSerialPort
{
    /** Constructs data. */
    UIDataSettingsMachineSerialPort()
        : m_iSlot(-1)
        , m_fPortEnabled(false)
        , m_uIRQ(0)
        , m_uIOAddress(0)
        , m_hostMode(KPortMode_Disconnected)
        , m_fServer(false)
        , m_strPath(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineSerialPort &other) const
    {
        return true
               && (m_iSlot == other.m_iSlot)
               && (m_fPortEnabled == other.m_fPortEnabled)
               && (m_uIRQ == other.m_uIRQ)
               && (m_uIOAddress == other.m_uIOAddress)
               && (m_hostMode == other.m_hostMode)
               && (m_fServer == other.m_fServer)
               && (m_strPath == other.m_strPath)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSerialPort &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSerialPort &other) const { return !equal(other); }

    /** Holds the serial port slot number. */
    int        m_iSlot;
    /** Holds whether the serial port is enabled. */
    bool       m_fPortEnabled;
    /** Holds the serial port IRQ. */
    ulong      m_uIRQ;
    /** Holds the serial port IO address. */
    ulong      m_uIOAddress;
    /** Holds the serial port host mode. */
    KPortMode  m_hostMode;
    /** Holds whether the serial port is server. */
    bool       m_fServer;
    /** Holds the serial port path. */
    QString    m_strPath;
};


/** Machine settings: Serial page data structure. */
struct UIDataSettingsMachineSerial
{
    /** Constructs data. */
    UIDataSettingsMachineSerial() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSerial & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSerial & /* other */) const { return false; }
};


/** Machine settings: Serial Port tab. */
class UIMachineSettingsSerial : public UIEditor
{
    Q_OBJECT;

signals:

    /** Notifies about port changed. */
    void sigPortChanged();

    /** Notifies about path changed. */
    void sigPathChanged();

    /** Notifies about validity changed. */
    void sigValidityChanged();

public:

    /** Constructs tab passing @a pParent to the base-class.
      * @param  pParentPage  Holds the parent page reference allowing to access some of API there. */
    UIMachineSettingsSerial(QITabWidget *pParent, UIMachineSettingsSerialPage *pParentPage);

    /** Loads port data from @a portCache. */
    void getPortDataFromCache(const UISettingsCacheMachineSerialPort &portCache);
    /** Saves port data to @a portCache. */
    void putPortDataToCache(UISettingsCacheMachineSerialPort &portCache);

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Configures tab order according to passed @a pWidget. */
    QWidget *setOrderAfter(QWidget *pWidget);

    /** Returns tab title. */
    QString tabTitle() const;
    /** Returns whether port is enabled. */
    bool isPortEnabled() const;
    /** Returns IRQ. */
    QString irq() const;
    /** Returns IO address. */
    QString ioAddress() const;
    /** Returns path. */
    QString path() const;

    /** Performs tab polishing. */
    void polishTab();

protected:

    /** Handles translation event. */
    void retranslateUi() {}

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Holds the parent page reference. */
    UIMachineSettingsSerialPage *m_pParentPage;

    /** Holds the port slot number. */
    int  m_iSlot;

    /** Holds the serial settings editor instance. */
    UISerialSettingsEditor *m_pEditorSerialSettings;
};


/*********************************************************************************************************************************
*   Class UIMachineSettingsSerial implementation.                                                                                *
*********************************************************************************************************************************/

UIMachineSettingsSerial::UIMachineSettingsSerial(QITabWidget *pParent, UIMachineSettingsSerialPage *pParentPage)
    : UIEditor(pParent)
    , m_pParentPage(pParentPage)
    , m_iSlot(-1)
    , m_pEditorSerialSettings(0)
{
    prepare();
}

void UIMachineSettingsSerial::getPortDataFromCache(const UISettingsCacheMachineSerialPort &portCache)
{
    /* Get old data: */
    const UIDataSettingsMachineSerialPort &oldPortData = portCache.base();

    /* Load port number: */
    m_iSlot = oldPortData.m_iSlot;

    if (m_pEditorSerialSettings)
    {
        /* Load port data: */
        m_pEditorSerialSettings->setPortByIRQAndIOAddress(oldPortData.m_uIRQ, oldPortData.m_uIOAddress);
        m_pEditorSerialSettings->setIRQ(oldPortData.m_uIRQ);
        m_pEditorSerialSettings->setIOAddress(oldPortData.m_uIOAddress);
        m_pEditorSerialSettings->setHostMode(oldPortData.m_hostMode);
        m_pEditorSerialSettings->setServerEnabled(oldPortData.m_fServer);
        m_pEditorSerialSettings->setPath(oldPortData.m_strPath);
        // Should be done in th end to finalize availability:
        m_pEditorSerialSettings->setPortEnabled(oldPortData.m_fPortEnabled);
    }
}

void UIMachineSettingsSerial::putPortDataToCache(UISettingsCacheMachineSerialPort &portCache)
{
    /* Prepare new data: */
    UIDataSettingsMachineSerialPort newPortData;

    /* Save port number: */
    newPortData.m_iSlot = m_iSlot;

    if (m_pEditorSerialSettings)
    {
        /* Save port data: */
        newPortData.m_fPortEnabled = m_pEditorSerialSettings->isPortEnabled();
        newPortData.m_uIRQ = m_pEditorSerialSettings->irq();
        newPortData.m_uIOAddress = m_pEditorSerialSettings->ioAddress();
        newPortData.m_fServer = m_pEditorSerialSettings->isServerEnabled();
        newPortData.m_hostMode = m_pEditorSerialSettings->hostMode();
        newPortData.m_strPath = m_pEditorSerialSettings->path();
    }

    /* Cache new data: */
    portCache.cacheCurrentData(newPortData);
}

bool UIMachineSettingsSerial::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;
    message.first = UITranslator::removeAccelMark(tabTitle());

    /* Validate enabled port only: */
    if (   m_pEditorSerialSettings
        && m_pEditorSerialSettings->isPortEnabled())
    {
        /* Check the port attribute emptiness & uniqueness: */
        const QString strIRQ = irq();
        const QString strIOAddress = ioAddress();
        const QPair<QString, QString> port = qMakePair(strIRQ, strIOAddress);

        if (strIRQ.isEmpty())
        {
            message.second << UIMachineSettingsSerial::tr("No IRQ is currently specified.");
            fPass = false;
        }
        if (strIOAddress.isEmpty())
        {
            message.second << UIMachineSettingsSerial::tr("No I/O port is currently specified.");
            fPass = false;
        }
        if (   !strIRQ.isEmpty()
            && !strIOAddress.isEmpty())
        {
            QVector<QPair<QString, QString> > ports;
            if (m_pParentPage)
            {
                ports = m_pParentPage->ports();
                ports.removeAt(m_iSlot);
            }
            if (ports.contains(port))
            {
                message.second << UIMachineSettingsSerial::tr("Two or more ports have the same settings.");
                fPass = false;
            }
        }

        const KPortMode enmMode = m_pEditorSerialSettings->hostMode();
        if (enmMode != KPortMode_Disconnected)
        {
            const QString strPath = m_pEditorSerialSettings->path();

            if (strPath.isEmpty())
            {
                message.second << UIMachineSettingsSerial::tr("No port path is currently specified.");
                fPass = false;
            }
            else
            {
                QVector<QString> paths;
                if (m_pParentPage)
                {
                    paths = m_pParentPage->paths();
                    paths.removeAt(m_iSlot);
                }
                if (paths.contains(strPath))
                {
                    message.second << UIMachineSettingsSerial::tr("There are currently duplicate port paths specified.");
                    fPass = false;
                }
            }
        }
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

QWidget *UIMachineSettingsSerial::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pEditorSerialSettings);
    return m_pEditorSerialSettings;
}

QString UIMachineSettingsSerial::tabTitle() const
{
    return QString(tr("Port %1", "serial ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsSerial::isPortEnabled() const
{
    return m_pEditorSerialSettings->isPortEnabled();
}

QString UIMachineSettingsSerial::irq() const
{
    return QString::number(m_pEditorSerialSettings->irq());
}

QString UIMachineSettingsSerial::ioAddress() const
{
    return QString::number(m_pEditorSerialSettings->ioAddress());
}

QString UIMachineSettingsSerial::path() const
{
    return m_pEditorSerialSettings->path();
}

void UIMachineSettingsSerial::polishTab()
{
    if (   m_pEditorSerialSettings
        && m_pParentPage)
    {
        /* Polish port page: */
        const bool fStd = m_pEditorSerialSettings->isPortStandardOne();
        const KPortMode enmMode = m_pEditorSerialSettings->hostMode();
        m_pEditorSerialSettings->setPortOptionsAvailable(m_pParentPage->isMachineOffline());
        m_pEditorSerialSettings->setIRQAndIOAddressOptionsAvailable(!fStd && m_pParentPage->isMachineOffline());
        m_pEditorSerialSettings->setHostModeOptionsAvailable(m_pParentPage->isMachineOffline());
        m_pEditorSerialSettings->setPipeOptionsAvailable(   (enmMode == KPortMode_HostPipe || enmMode == KPortMode_TCP)
                                                         && m_pParentPage->isMachineOffline());
        m_pEditorSerialSettings->setPathOptionsAvailable(   enmMode != KPortMode_Disconnected
                                                         && m_pParentPage->isMachineOffline());
    }
}

void UIMachineSettingsSerial::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsSerial::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
#ifdef VBOX_WS_MAC
            /* On Mac OS X we can do a bit of smoothness: */
            int iLeft, iTop, iRight, iBottom;
            pLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
            pLayout->setContentsMargins(iLeft / 2, iTop / 2, iRight / 2, iBottom / 2);
#endif

        /* Prepare settings editor: */
        m_pEditorSerialSettings = new UISerialSettingsEditor(this);
        if (m_pEditorSerialSettings)
        {
            addEditor(m_pEditorSerialSettings);
            pLayout->addWidget(m_pEditorSerialSettings);
        }

        pLayout->addStretch();
    }
}

void UIMachineSettingsSerial::prepareConnections()
{
    if (m_pEditorSerialSettings)
    {
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigPortAvailabilityChanged,
                this, &UIMachineSettingsSerial::sigPortChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigPortAvailabilityChanged,
                this, &UIMachineSettingsSerial::sigPathChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigStandardPortOptionChanged,
                this, &UIMachineSettingsSerial::sigValidityChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigPortIRQChanged,
                this, &UIMachineSettingsSerial::sigPortChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigPortIOAddressChanged,
                this, &UIMachineSettingsSerial::sigPortChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigModeChanged,
                this, &UIMachineSettingsSerial::sigValidityChanged);
        connect(m_pEditorSerialSettings, &UISerialSettingsEditor::sigPathChanged,
                this, &UIMachineSettingsSerial::sigPathChanged);
    }
}


/*********************************************************************************************************************************
*   Class UIMachineSettingsSerialPage implementation.                                                                            *
*********************************************************************************************************************************/

UIMachineSettingsSerialPage::UIMachineSettingsSerialPage()
    : m_pCache(0)
    , m_pTabWidget(0)
{
    prepare();
}

UIMachineSettingsSerialPage::~UIMachineSettingsSerialPage()
{
    cleanup();
}

bool UIMachineSettingsSerialPage::changed() const
{
    return m_pCache ? m_pCache->wasChanged() : false;
}

void UIMachineSettingsSerialPage::loadToCacheFrom(QVariant &data)
{
    /* Sanity check: */
    if (   !m_pCache
        || !m_pTabWidget)
        return;

    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache lists: */
    refreshPorts();
    refreshPaths();

    /* Prepare old data: */
    UIDataSettingsMachineSerial oldSerialData;

    /* For each serial port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Prepare old data: */
        UIDataSettingsMachineSerialPort oldPortData;

        /* Check whether port is valid: */
        const CSerialPort &comPort = m_machine.GetSerialPort(iSlot);
        if (!comPort.isNull())
        {
            /* Gather old data: */
            oldPortData.m_iSlot = iSlot;
            oldPortData.m_fPortEnabled = comPort.GetEnabled();
            oldPortData.m_uIRQ = comPort.GetIRQ();
            oldPortData.m_uIOAddress = comPort.GetIOAddress();
            oldPortData.m_hostMode = comPort.GetHostMode();
            oldPortData.m_fServer = comPort.GetServer();
            oldPortData.m_strPath = comPort.GetPath();
        }

        /* Cache old data: */
        m_pCache->child(iSlot).cacheInitialData(oldPortData);
    }

    /* Cache old data: */
    m_pCache->cacheInitialData(oldSerialData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSerialPage::getFromCache()
{
    /* Sanity check: */
    if (   !m_pCache
        || !m_pTabWidget)
        return;

    /* Setup tab order: */
    AssertPtrReturnVoid(firstWidget());
    setTabOrder(firstWidget(), m_pTabWidget->focusProxy());
    QWidget *pLastFocusWidget = m_pTabWidget->focusProxy();

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Get port page: */
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);

        /* Load old data from cache: */
        pTab->getPortDataFromCache(m_pCache->child(iSlot));

        /* Setup tab order: */
        pLastFocusWidget = pTab->setOrderAfter(pLastFocusWidget);
    }

    /* Apply language settings: */
    retranslateUi();

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSerialPage::putToCache()
{
    /* Sanity check: */
    if (   !m_pCache
        || !m_pTabWidget)
        return;

    /* Prepare new data: */
    UIDataSettingsMachineSerial newSerialData;

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Getting port page: */
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);

        /* Gather new data: */
        pTab->putPortDataToCache(m_pCache->child(iSlot));
    }

    /* Cache new data: */
    m_pCache->cacheCurrentData(newSerialData);
}

void UIMachineSettingsSerialPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsSerialPage::validate(QList<UIValidationMessage> &messages)
{
    /* Sanity check: */
    if (!m_pTabWidget)
        return false;

    /* Pass by default: */
    bool fValid = true;

    /* Delegate validation to adapter tabs: */
    for (int iIndex = 0; iIndex < m_pTabWidget->count(); ++iIndex)
    {
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iIndex));
        AssertPtrReturn(pTab, false);
        if (!pTab->validate(messages))
            fValid = false;
    }

    /* Return result: */
    return fValid;
}

void UIMachineSettingsSerialPage::retranslateUi()
{
    /* Sanity check: */
    if (!m_pTabWidget)
        return;

    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);
        m_pTabWidget->setTabText(iSlot, pTab->tabTitle());
    }
}

void UIMachineSettingsSerialPage::polishPage()
{
    /* Sanity check: */
    if (   !m_pCache
        || !m_pTabWidget)
        return;

    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        m_pTabWidget->setTabEnabled(iSlot,
                                    isMachineOffline() ||
                                    (isMachineInValidMode() &&
                                     m_pCache->childCount() > iSlot &&
                                     m_pCache->child(iSlot).base().m_fPortEnabled));
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);
        pTab->polishTab();
    }
}

void UIMachineSettingsSerialPage::sltHandlePortChange()
{
    refreshPorts();
    revalidate();
}

void UIMachineSettingsSerialPage::sltHandlePathChange()
{
    refreshPaths();
    revalidate();
}

void UIMachineSettingsSerialPage::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineSerial;
    AssertPtrReturnVoid(m_pCache);

    /* Create main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Creating tab-widget: */
        m_pTabWidget = new QITabWidget;
        if (m_pTabWidget)
        {
            /* How many ports to display: */
            const ulong uCount = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86).GetSerialPortCount();

            /* Create corresponding port tabs: */
            for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
            {
                /* Create port tab: */
                UIMachineSettingsSerial *pTab = new UIMachineSettingsSerial(m_pTabWidget, this);
                if (pTab)
                {
                    /* Tab connections: */
                    connect(pTab, &UIMachineSettingsSerial::sigPortChanged,
                            this, &UIMachineSettingsSerialPage::sltHandlePortChange);
                    connect(pTab, &UIMachineSettingsSerial::sigPathChanged,
                            this, &UIMachineSettingsSerialPage::sltHandlePathChange);
                    connect(pTab, &UIMachineSettingsSerial::sigValidityChanged,
                            this, &UIMachineSettingsSerialPage::revalidate);

                    /* Add tab into tab-widget: */
                    addEditor(pTab);
                    m_pTabWidget->addTab(pTab, pTab->tabTitle());
                }
            }

            /* Add tab-widget into layout: */
            pLayoutMain->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsSerialPage::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsSerialPage::refreshPorts()
{
    /* Sanity check: */
    if (!m_pTabWidget)
        return;

    /* Reload port list: */
    m_ports.clear();
    m_ports.resize(m_pTabWidget->count());
    /* Append port list with data from all the tabs: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);
        m_ports[iSlot] = pTab->isPortEnabled() ? qMakePair(pTab->irq(), pTab->ioAddress()) : qMakePair(QString(), QString());
    }
}

void UIMachineSettingsSerialPage::refreshPaths()
{
    /* Sanity check: */
    if (!m_pTabWidget)
        return;

    /* Reload path list: */
    m_paths.clear();
    m_paths.resize(m_pTabWidget->count());
    /* Append path list with data from all the tabs: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
        AssertPtrReturnVoid(pTab);
        m_paths[iSlot] = pTab->isPortEnabled() ? pTab->path() : QString();
    }
}

bool UIMachineSettingsSerialPage::saveData()
{
    /* Sanity check: */
    if (   !m_pCache
        || !m_pTabWidget)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save serial settings from cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* For each port: */
        for (int iSlot = 0; fSuccess && iSlot < m_pTabWidget->count(); ++iSlot)
            fSuccess = savePortData(iSlot);
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsSerialPage::savePortData(int iSlot)
{
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save adapter settings from cache: */
    if (fSuccess && m_pCache->child(iSlot).wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsMachineSerialPort &oldPortData = m_pCache->child(iSlot).base();
        /* Get new data from cache: */
        const UIDataSettingsMachineSerialPort &newPortData = m_pCache->child(iSlot).data();

        /* Get serial port for further activities: */
        CSerialPort comPort = m_machine.GetSerialPort(iSlot);
        fSuccess = m_machine.isOk() && comPort.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            // This *must* be first.
            // If the requested host mode is changed to disconnected we should do it first.
            // That allows to automatically fulfill the requirements for some of the settings below.
            /* Save port host mode: */
            if (   fSuccess && isMachineOffline()
                && newPortData.m_hostMode != oldPortData.m_hostMode
                && newPortData.m_hostMode == KPortMode_Disconnected)
            {
                comPort.SetHostMode(newPortData.m_hostMode);
                fSuccess = comPort.isOk();
            }
            /* Save whether the port is enabled: */
            if (fSuccess && isMachineOffline() && newPortData.m_fPortEnabled != oldPortData.m_fPortEnabled)
            {
                comPort.SetEnabled(newPortData.m_fPortEnabled);
                fSuccess = comPort.isOk();
            }
            /* Save port IRQ: */
            if (fSuccess && isMachineOffline() && newPortData.m_uIRQ != oldPortData.m_uIRQ)
            {
                comPort.SetIRQ(newPortData.m_uIRQ);
                fSuccess = comPort.isOk();
            }
            /* Save port IO address: */
            if (fSuccess && isMachineOffline() && newPortData.m_uIOAddress != oldPortData.m_uIOAddress)
            {
                comPort.SetIOAddress(newPortData.m_uIOAddress);
                fSuccess = comPort.isOk();
            }
            /* Save whether the port is server: */
            if (fSuccess && isMachineOffline() && newPortData.m_fServer != oldPortData.m_fServer)
            {
                comPort.SetServer(newPortData.m_fServer);
                fSuccess = comPort.isOk();
            }
            /* Save port path: */
            if (fSuccess && isMachineOffline() && newPortData.m_strPath != oldPortData.m_strPath)
            {
                comPort.SetPath(newPortData.m_strPath);
                fSuccess = comPort.isOk();
            }
            // This *must* be last.
            // The host mode will be changed to disconnected if some of the necessary
            // settings above will not meet the requirements for the selected mode.
            /* Save port host mode: */
            if (   fSuccess && isMachineOffline()
                && newPortData.m_hostMode != oldPortData.m_hostMode
                && newPortData.m_hostMode != KPortMode_Disconnected)
            {
                comPort.SetHostMode(newPortData.m_hostMode);
                fSuccess = comPort.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(comPort));
        }
    }
    /* Return result: */
    return fSuccess;
}

# include "UIMachineSettingsSerial.moc"
