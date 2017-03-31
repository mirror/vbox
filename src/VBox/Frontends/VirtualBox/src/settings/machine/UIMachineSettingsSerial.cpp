/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSerial class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>

/* GUI includes: */
# include "QITabWidget.h"
# include "QIWidgetValidator.h"
# include "UIConverter.h"
# include "UIMachineSettingsSerial.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CSerialPort.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: Serial Port tab data structure. */
struct UIDataSettingsMachineSerialPort
{
    /** Constructs data. */
    UIDataSettingsMachineSerialPort()
        : m_iSlot(-1)
        , m_fPortEnabled(false)
        , m_uIRQ(0)
        , m_uIOBase(0)
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
               && (m_uIOBase == other.m_uIOBase)
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
    /** Holds the serial port IO base. */
    ulong      m_uIOBase;
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
    UIDataSettingsMachineSerial()
        : m_ports(QList<UIDataSettingsMachineSerialPort>())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineSerial &other) const
    {
        return true
               && (m_ports == other.m_ports)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSerial &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSerial &other) const { return !equal(other); }

    /** Holds the port list. */
    QList<UIDataSettingsMachineSerialPort> m_ports;
};


/** Machine settings: Serial Port tab. */
class UIMachineSettingsSerial : public QIWithRetranslateUI<QWidget>,
                                public Ui::UIMachineSettingsSerial
{
    Q_OBJECT;

public:

    UIMachineSettingsSerial(UIMachineSettingsSerialPage *pParent);

    void polishTab();

    void loadPortData(const UIDataSettingsMachineSerialPort &portData);
    void savePortData(UIDataSettingsMachineSerialPort &portData);

    QWidget *setOrderAfter(QWidget *pAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void sltGbSerialToggled(bool fOn);
    void sltCbNumberActivated(const QString &strText);
    void sltCbModeActivated(const QString &strText);

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    UIMachineSettingsSerialPage *m_pParent;
    int m_iSlot;
};


/*********************************************************************************************************************************
*   Class UIMachineSettingsSerial implementation.                                                                                *
*********************************************************************************************************************************/

UIMachineSettingsSerial::UIMachineSettingsSerial(UIMachineSettingsSerialPage *pParent)
    : QIWithRetranslateUI<QWidget>(0)
    , m_pParent(pParent)
    , m_iSlot(-1)
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsSerial::setupUi(this);

    /* Setup validation: */
    mLeIRQ->setValidator(new QIULongValidator(0, 255, this));
    mLeIOPort->setValidator(new QIULongValidator(0, 0xFFFF, this));
    mLePath->setValidator(new QRegExpValidator(QRegExp(".+"), this));

    /* Setup constraints: */
    mLeIRQ->setFixedWidth(mLeIRQ->fontMetrics().width("8888"));
    mLeIOPort->setFixedWidth(mLeIOPort->fontMetrics().width("8888888"));

    /* Set initial values: */
    /* Note: If you change one of the following don't forget retranslateUi. */
    mCbNumber->insertItem(0, vboxGlobal().toCOMPortName(0, 0));
    mCbNumber->insertItems(0, vboxGlobal().COMPortNames());

    mCbMode->addItem(""); /* KPortMode_Disconnected */
    mCbMode->addItem(""); /* KPortMode_HostPipe */
    mCbMode->addItem(""); /* KPortMode_HostDevice */
    mCbMode->addItem(""); /* KPortMode_RawFile */
    mCbMode->addItem(""); /* KPortMode_TCP */

    /* Setup connections: */
    connect(mGbSerial, SIGNAL(toggled(bool)),
            this, SLOT(sltGbSerialToggled(bool)));
    connect(mCbNumber, SIGNAL(activated(const QString &)),
            this, SLOT(sltCbNumberActivated(const QString &)));
    connect(mCbMode, SIGNAL(activated(const QString &)),
            this, SLOT(sltCbModeActivated(const QString &)));

    /* Prepare validation: */
    prepareValidation();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsSerial::polishTab()
{
    /* Polish port page: */
    ulong uIRQ, uIOBase;
    const bool fStd = vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), uIRQ, uIOBase);
    const KPortMode enmMode = gpConverter->fromString<KPortMode>(mCbMode->currentText());
    mGbSerial->setEnabled(m_pParent->isMachineOffline());
    mLbNumber->setEnabled(m_pParent->isMachineOffline());
    mCbNumber->setEnabled(m_pParent->isMachineOffline());
    mLbIRQ->setEnabled(m_pParent->isMachineOffline());
    mLeIRQ->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbIOPort->setEnabled(m_pParent->isMachineOffline());
    mLeIOPort->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbMode->setEnabled(m_pParent->isMachineOffline());
    mCbMode->setEnabled(m_pParent->isMachineOffline());
    mCbPipe->setEnabled(   (enmMode == KPortMode_HostPipe || enmMode == KPortMode_TCP)
                        && m_pParent->isMachineOffline());
    mLbPath->setEnabled(m_pParent->isMachineOffline());
    mLePath->setEnabled(enmMode != KPortMode_Disconnected && m_pParent->isMachineOffline());
}

void UIMachineSettingsSerial::loadPortData(const UIDataSettingsMachineSerialPort &portData)
{
    /* Load port number: */
    m_iSlot = portData.m_iSlot;

    /* Load port data: */
    mGbSerial->setChecked(portData.m_fPortEnabled);
    mCbNumber->setCurrentIndex(mCbNumber->findText(vboxGlobal().toCOMPortName(portData.m_uIRQ, portData.m_uIOBase)));
    mLeIRQ->setText(QString::number(portData.m_uIRQ));
    mLeIOPort->setText("0x" + QString::number(portData.m_uIOBase, 16).toUpper());
    mCbMode->setCurrentIndex(mCbMode->findText(gpConverter->toString(portData.m_hostMode)));
    mCbPipe->setChecked(!portData.m_fServer);
    mLePath->setText(portData.m_strPath);

    /* Ensure everything is up-to-date */
    sltGbSerialToggled(mGbSerial->isChecked());
}

void UIMachineSettingsSerial::savePortData(UIDataSettingsMachineSerialPort &portData)
{
    /* Save port data: */
    portData.m_fPortEnabled = mGbSerial->isChecked();
    portData.m_uIRQ = mLeIRQ->text().toULong(NULL, 0);
    portData.m_uIOBase = mLeIOPort->text().toULong(NULL, 0);
    portData.m_fServer = !mCbPipe->isChecked();
    portData.m_hostMode = gpConverter->fromString<KPortMode>(mCbMode->currentText());
    portData.m_strPath = QDir::toNativeSeparators(mLePath->text());
}

QWidget *UIMachineSettingsSerial::setOrderAfter(QWidget *pAfter)
{
    setTabOrder(pAfter, mGbSerial);
    setTabOrder(mGbSerial, mCbNumber);
    setTabOrder(mCbNumber, mLeIRQ);
    setTabOrder(mLeIRQ, mLeIOPort);
    setTabOrder(mLeIOPort, mCbMode);
    setTabOrder(mCbMode, mCbPipe);
    setTabOrder(mCbPipe, mLePath);
    return mLePath;
}

QString UIMachineSettingsSerial::pageTitle() const
{
    return QString(tr("Port %1", "serial ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsSerial::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), a, b);
}

void UIMachineSettingsSerial::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSerial::retranslateUi(this);

    mCbNumber->setItemText(mCbNumber->count() - 1, vboxGlobal().toCOMPortName(0, 0));

    mCbMode->setItemText(4, gpConverter->toString(KPortMode_TCP));
    mCbMode->setItemText(3, gpConverter->toString(KPortMode_RawFile));
    mCbMode->setItemText(2, gpConverter->toString(KPortMode_HostDevice));
    mCbMode->setItemText(1, gpConverter->toString(KPortMode_HostPipe));
    mCbMode->setItemText(0, gpConverter->toString(KPortMode_Disconnected));
}

void UIMachineSettingsSerial::sltGbSerialToggled(bool fOn)
{
    if (fOn)
    {
        sltCbNumberActivated(mCbNumber->currentText());
        sltCbModeActivated(mCbMode->currentText());
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::sltCbNumberActivated(const QString &strText)
{
    ulong uIRQ, uIOBase;
    bool fStd = vboxGlobal().toCOMPortNumbers(strText, uIRQ, uIOBase);

    mLeIRQ->setEnabled(!fStd);
    mLeIOPort->setEnabled(!fStd);
    if (fStd)
    {
        mLeIRQ->setText(QString::number(uIRQ));
        mLeIOPort->setText("0x" + QString::number(uIOBase, 16).toUpper());
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::sltCbModeActivated(const QString &strText)
{
    KPortMode enmMode = gpConverter->fromString<KPortMode>(strText);
    mCbPipe->setEnabled(enmMode == KPortMode_HostPipe || enmMode == KPortMode_TCP);
    mLePath->setEnabled(enmMode != KPortMode_Disconnected);

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::prepareValidation()
{
    /* Prepare validation: */
    connect(mLeIRQ, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
    connect(mLeIOPort, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
    connect(mLePath, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
}


/*********************************************************************************************************************************
*   Class UIMachineSettingsSerialPage implementation.                                                                            *
*********************************************************************************************************************************/

UIMachineSettingsSerialPage::UIMachineSettingsSerialPage()
    : m_pTabWidget(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsSerialPage::~UIMachineSettingsSerialPage()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsSerialPage::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsSerialPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare initial data: */
    UIDataSettingsMachineSerial initialData;

    /* For each serial port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Prepare port data: */
        UIDataSettingsMachineSerialPort initialPortData;

        /* Check if port is valid: */
        const CSerialPort &port = m_machine.GetSerialPort(iSlot);
        if (!port.isNull())
        {
            /* Gather options: */
            initialPortData.m_iSlot = iSlot;
            initialPortData.m_fPortEnabled = port.GetEnabled();
            initialPortData.m_uIRQ = port.GetIRQ();
            initialPortData.m_uIOBase = port.GetIOBase();
            initialPortData.m_hostMode = port.GetHostMode();
            initialPortData.m_fServer = port.GetServer();
            initialPortData.m_strPath = port.GetPath();
        }

        /* Append initial port data: */
        initialData.m_ports << initialPortData;
    }

    /* Cache initial data: */
    m_pCache->cacheInitialData(initialData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSerialPage::getFromCache()
{
    /* Setup tab order: */
    AssertPtrReturnVoid(firstWidget());
    setTabOrder(firstWidget(), m_pTabWidget->focusProxy());
    QWidget *pLastFocusWidget = m_pTabWidget->focusProxy();

    /* For each serial port: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        /* Get port page: */
        UIMachineSettingsSerial *pPage = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iPort));

        /* Load port data to page: */
        pPage->loadPortData(m_pCache->base().m_ports.at(iPort));

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
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
    /* Prepare current data: */
    UIDataSettingsMachineSerial currentData;

    /* For each serial port: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        /* Getting port page: */
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iPort));

        /* Prepare current port data: */
        UIDataSettingsMachineSerialPort currentPortData;

        /* Gather current port data: */
        pTab->savePortData(currentPortData);

        /* Cache current port data: */
        currentData.m_ports << currentPortData;
    }

    /* Cache current data: */
    m_pCache->cacheCurrentData(currentData);
}

void UIMachineSettingsSerialPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if ports data was changed: */
    if (m_pCache->wasChanged())
    {
        /* For each serial port: */
        for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
        {
            /* Check if port data was changed: */
            const UIDataSettingsMachineSerialPort &initialPortData = m_pCache->base().m_ports.at(iPort);
            const UIDataSettingsMachineSerialPort &currentPortData = m_pCache->data().m_ports.at(iPort);
            if (currentPortData != initialPortData)
            {
                /* Check if port still valid: */
                CSerialPort port = m_machine.GetSerialPort(iPort);
                if (!port.isNull())
                {
                    /* Store adapter data: */
                    if (isMachineOffline())
                    {
                        /* Whether the port is enabled: */
                        if (   port.isOk()
                            && currentPortData.m_fPortEnabled != initialPortData.m_fPortEnabled)
                            port.SetEnabled(currentPortData.m_fPortEnabled);
                        /* Port IRQ: */
                        if (   port.isOk()
                            && currentPortData.m_uIRQ != initialPortData.m_uIRQ)
                            port.SetIRQ(currentPortData.m_uIRQ);
                        /* Port IO base: */
                        if (   port.isOk()
                            && currentPortData.m_uIOBase != initialPortData.m_uIOBase)
                            port.SetIOBase(currentPortData.m_uIOBase);
                        /* Whether the port is server: */
                        if (   port.isOk()
                            && currentPortData.m_fServer != initialPortData.m_fServer)
                            port.SetServer(currentPortData.m_fServer);
                        /* Port path: */
                        if (   port.isOk()
                            && currentPortData.m_strPath != initialPortData.m_strPath)
                            port.SetPath(currentPortData.m_strPath);
                        /* This *must* be last. The host mode will be changed to disconnected if
                         * some of the necessary settings above will not meet the requirements for
                         * the selected mode. */
                        if (   port.isOk()
                            && currentPortData.m_hostMode != initialPortData.m_hostMode)
                            port.SetHostMode(currentPortData.m_hostMode);
                    }
                }
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsSerialPage::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Validation stuff: */
    QList<QPair<QString, QString> > ports;
    QStringList paths;

    /* Validate all the ports: */
    for (int iIndex = 0; iIndex < m_pTabWidget->count(); ++iIndex)
    {
        /* Get current tab/page: */
        QWidget *pTab = m_pTabWidget->widget(iIndex);
        UIMachineSettingsSerial *pPage = static_cast<UIMachineSettingsSerial*>(pTab);
        if (!pPage->mGbSerial->isChecked())
            continue;

        /* Prepare message: */
        UIValidationMessage message;
        message.first = vboxGlobal().removeAccelMark(m_pTabWidget->tabText(m_pTabWidget->indexOf(pTab)));

        /* Check the port attribute emptiness & uniqueness: */
        const QString strIRQ(pPage->mLeIRQ->text());
        const QString strIOPort(pPage->mLeIOPort->text());
        const QPair<QString, QString> pair(strIRQ, strIOPort);

        if (strIRQ.isEmpty())
        {
            message.second << UIMachineSettingsSerial::tr("No IRQ is currently specified.");
            fPass = false;
        }
        if (strIOPort.isEmpty())
        {
            message.second << UIMachineSettingsSerial::tr("No I/O port is currently specified.");
            fPass = false;
        }
        if (ports.contains(pair))
        {
            message.second << UIMachineSettingsSerial::tr("Two or more ports have the same settings.");
            fPass = false;
        }

        ports << pair;

        const KPortMode enmMode = gpConverter->fromString<KPortMode>(pPage->mCbMode->currentText());
        if (enmMode != KPortMode_Disconnected)
        {
            const QString strPath(pPage->mLePath->text());

            if (strPath.isEmpty())
            {
                message.second << UIMachineSettingsSerial::tr("No port path is currently specified.");
                fPass = false;
            }
            if (paths.contains(strPath))
            {
                message.second << UIMachineSettingsSerial::tr("There are currently duplicate port paths specified.");
                fPass = false;
            }

            paths << strPath;
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIMachineSettingsSerialPage::retranslateUi()
{
    for (int i = 0; i < m_pTabWidget->count(); ++i)
    {
        UIMachineSettingsSerial *pPage =
            static_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(i));
        m_pTabWidget->setTabText(i, pPage->pageTitle());
    }
}

void UIMachineSettingsSerialPage::polishPage()
{
    /* Get the count of serial port tabs: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        m_pTabWidget->setTabEnabled(iPort,
                                    isMachineOffline() ||
                                    (isMachineInValidMode() &&
                                     m_pCache->base().m_ports.size() > iPort &&
                                     m_pCache->base().m_ports.at(iPort).m_fPortEnabled));
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iPort));
        pTab->polishTab();
    }
}

void UIMachineSettingsSerialPage::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineSerial;
    AssertPtrReturnVoid(m_pCache);

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pMainLayout);
    {
        /* Configure layout: */
        pMainLayout->setContentsMargins(0, 5, 0, 5);

        /* Creating tab-widget: */
        m_pTabWidget = new QITabWidget;
        AssertPtrReturnVoid(m_pTabWidget);
        {
            /* How many ports to display: */
            const ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();

            /* Create corresponding port tabs: */
            for (ulong uPort = 0; uPort < uCount; ++uPort)
            {
                /* Create port tab: */
                UIMachineSettingsSerial *pTab = new UIMachineSettingsSerial(this);
                AssertPtrReturnVoid(pTab);
                {
                    /* Add tab into tab-widget: */
                    m_pTabWidget->addTab(pTab, pTab->pageTitle());
                }
            }

            /* Add tab-widget into layout: */
            pMainLayout->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsSerialPage::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

# include "UIMachineSettingsSerial.moc"

