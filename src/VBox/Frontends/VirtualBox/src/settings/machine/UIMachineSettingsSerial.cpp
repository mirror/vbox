/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSerial class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

/* GUI includes: */
#include "QITabWidget.h"
#include "QIWidgetValidator.h"
#include "UIConverter.h"
#include "UIMachineSettingsSerial.h"
#include "UIErrorString.h"
#include "UICommon.h"

/* COM includes: */
#include "CSerialPort.h"


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
    UIDataSettingsMachineSerial() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSerial & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSerial & /* other */) const { return false; }
};


/** Machine settings: Serial Port tab. */
class UIMachineSettingsSerial : public QIWithRetranslateUI<QWidget>
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
    /** Handles port mode change to item with certain @a iIndex. */
    void sltHandlePortModeChange(int iIndex);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Populates combo-boxes. */
    void populateComboboxes();

    /** Holds the parent page reference. */
    UIMachineSettingsSerialPage *m_pParent;

    /** Holds the port slot number. */
    int        m_iSlot;
    /** Holds the port mode. */
    KPortMode  m_enmPortMode;

    /** @name Widgets
     * @{ */
        /** Holds the port check-box instance. */
        QCheckBox *m_pCheckBoxPort;
        /** Holds the port settings widget instance. */
        QWidget   *m_pWidgetPortSettings;
        /** Holds the number label instance. */
        QLabel    *m_pLabelNumber;
        /** Holds the number combo instance. */
        QComboBox *m_pComboNumber;
        /** Holds the IRQ label instance. */
        QLabel    *m_pLabelIRQ;
        /** Holds the IRQ editor instance. */
        QLineEdit *m_pLineEditIRQ;
        /** Holds the IO port label instance. */
        QLabel    *m_pLabelIOPort;
        /** Holds the IO port editor instance. */
        QLineEdit *m_pLineEditIOPort;
        /** Holds the mode label instance. */
        QLabel    *m_pLabelMode;
        /** Holds the mode combo instance. */
        QComboBox *m_pComboMode;
        /** Holds the pipe check-box instance. */
        QCheckBox *m_pCheckBoxPipe;
        /** Holds the path label instance. */
        QLabel    *m_pLabelPath;
        /** Holds the path editor instance. */
        QLineEdit *m_pEditorPath;
    /** @} */

    friend class UIMachineSettingsSerialPage;
};


/*********************************************************************************************************************************
*   Class UIMachineSettingsSerial implementation.                                                                                *
*********************************************************************************************************************************/

UIMachineSettingsSerial::UIMachineSettingsSerial(UIMachineSettingsSerialPage *pParent)
    : QIWithRetranslateUI<QWidget>(0)
    , m_pParent(pParent)
    , m_iSlot(-1)
    , m_enmPortMode(KPortMode_Max)
    , m_pCheckBoxPort(0)
    , m_pWidgetPortSettings(0)
    , m_pLabelNumber(0)
    , m_pComboNumber(0)
    , m_pLabelIRQ(0)
    , m_pLineEditIRQ(0)
    , m_pLabelIOPort(0)
    , m_pLineEditIOPort(0)
    , m_pLabelMode(0)
    , m_pComboMode(0)
    , m_pCheckBoxPipe(0)
    , m_pLabelPath(0)
    , m_pEditorPath(0)
{
    prepare();
}

void UIMachineSettingsSerial::polishTab()
{
    /* Polish port page: */
    ulong uIRQ, uIOBase;
    const bool fStd = uiCommon().toCOMPortNumbers(m_pComboNumber->currentText(), uIRQ, uIOBase);
    const KPortMode enmMode = m_pComboMode->currentData().value<KPortMode>();
    m_pCheckBoxPort->setEnabled(m_pParent->isMachineOffline());
    m_pLabelNumber->setEnabled(m_pParent->isMachineOffline());
    m_pComboNumber->setEnabled(m_pParent->isMachineOffline());
    m_pLabelIRQ->setEnabled(m_pParent->isMachineOffline());
    m_pLineEditIRQ->setEnabled(!fStd && m_pParent->isMachineOffline());
    m_pLabelIOPort->setEnabled(m_pParent->isMachineOffline());
    m_pLineEditIOPort->setEnabled(!fStd && m_pParent->isMachineOffline());
    m_pLabelMode->setEnabled(m_pParent->isMachineOffline());
    m_pComboMode->setEnabled(m_pParent->isMachineOffline());
    m_pCheckBoxPipe->setEnabled(   (enmMode == KPortMode_HostPipe || enmMode == KPortMode_TCP)
                        && m_pParent->isMachineOffline());
    m_pLabelPath->setEnabled(   enmMode != KPortMode_Disconnected
                        && m_pParent->isMachineOffline());
    m_pEditorPath->setEnabled(   enmMode != KPortMode_Disconnected
                        && m_pParent->isMachineOffline());
}

void UIMachineSettingsSerial::loadPortData(const UIDataSettingsMachineSerialPort &portData)
{
    /* Load port number: */
    m_iSlot = portData.m_iSlot;

    /* Load port data: */
    m_pCheckBoxPort->setChecked(portData.m_fPortEnabled);
    m_pComboNumber->setCurrentIndex(m_pComboNumber->findText(uiCommon().toCOMPortName(portData.m_uIRQ, portData.m_uIOBase)));
    m_pLineEditIRQ->setText(QString::number(portData.m_uIRQ));
    m_pLineEditIOPort->setText("0x" + QString::number(portData.m_uIOBase, 16).toUpper());
    m_enmPortMode = portData.m_hostMode;
    m_pCheckBoxPipe->setChecked(!portData.m_fServer);
    m_pEditorPath->setText(portData.m_strPath);

    /* Repopulate combo-boxes content: */
    populateComboboxes();
    /* Ensure everything is up-to-date */
    sltGbSerialToggled(m_pCheckBoxPort->isChecked());
}

void UIMachineSettingsSerial::savePortData(UIDataSettingsMachineSerialPort &portData)
{
    /* Save port data: */
    portData.m_fPortEnabled = m_pCheckBoxPort->isChecked();
    portData.m_uIRQ = m_pLineEditIRQ->text().toULong(NULL, 0);
    portData.m_uIOBase = m_pLineEditIOPort->text().toULong(NULL, 0);
    portData.m_fServer = !m_pCheckBoxPipe->isChecked();
    portData.m_hostMode = m_pComboMode->currentData().value<KPortMode>();
    portData.m_strPath = QDir::toNativeSeparators(m_pEditorPath->text());
}

QWidget *UIMachineSettingsSerial::setOrderAfter(QWidget *pAfter)
{
    setTabOrder(pAfter, m_pCheckBoxPort);
    setTabOrder(m_pCheckBoxPort, m_pComboNumber);
    setTabOrder(m_pComboNumber, m_pLineEditIRQ);
    setTabOrder(m_pLineEditIRQ, m_pLineEditIOPort);
    setTabOrder(m_pLineEditIOPort, m_pComboMode);
    setTabOrder(m_pComboMode, m_pCheckBoxPipe);
    setTabOrder(m_pCheckBoxPipe, m_pEditorPath);
    return m_pEditorPath;
}

QString UIMachineSettingsSerial::pageTitle() const
{
    return QString(tr("Port %1", "serial ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsSerial::isUserDefined()
{
    ulong a, b;
    return !uiCommon().toCOMPortNumbers(m_pComboNumber->currentText(), a, b);
}

void UIMachineSettingsSerial::retranslateUi()
{
    m_pCheckBoxPort->setWhatsThis(tr("When checked, enables the given serial port of the virtual machine."));
    m_pCheckBoxPort->setText(tr("&Enable Serial Port"));
    m_pLabelNumber->setText(tr("Port &Number:"));
    m_pComboNumber->setWhatsThis(tr("Selects the serial port number. You can choose one of the standard serial ports or "
                                      "select <b>User-defined</b> and specify port parameters manually."));
    m_pLabelIRQ->setText(tr("&IRQ:"));
    m_pLineEditIRQ->setWhatsThis(tr("Holds the IRQ number of this serial port. This should be a whole number between <tt>0</tt> "
                                    "and <tt>255</tt>. Values greater than <tt>15</tt> may only be used if the <b>I/O APIC</b> "
                                    "setting is enabled for this virtual machine."));
    m_pLabelIOPort->setText(tr("I/O Po&rt:"));
    m_pLineEditIOPort->setWhatsThis(tr("Holds the base I/O port address of this serial port. Valid values are integer numbers in "
                                       "range from <tt>0</tt> to <tt>0xFFFF</tt>."));
    m_pLabelMode->setText(tr("Port &Mode:"));
    m_pComboMode->setWhatsThis(tr("Selects the working mode of this serial port. If you select <b>Disconnected</b>, the guest "
                                     "OS will detect the serial port but will not be able to operate it."));
    m_pCheckBoxPipe->setWhatsThis(tr("When checked, the virtual machine will assume that the pipe or socket specified in the "
                                     "<b>Path/Address</b> field exists and try to use it. Otherwise, the pipe or socket will be "
                                     "created by the virtual machine when it starts."));
    m_pCheckBoxPipe->setText(tr("&Connect to existing pipe/socket"));
    m_pLabelPath->setText(tr("&Path/Address:"));
    m_pEditorPath->setWhatsThis(tr("<p>In <b>Host Pipe</b> mode: Holds the path to the serial port's pipe on the host. "
                                     "Examples: \"\\\\.\\pipe\\myvbox\" or \"/tmp/myvbox\", for Windows and UNIX-like systems "
                                     "respectively.</p><p>In <b>Host Device</b> mode: Holds the host serial device name. "
                                     "Examples: \"COM1\" or \"/dev/ttyS0\".</p><p>In <b>Raw File</b> mode: Holds the file-path "
                                     "on the host system, where the serial output will be dumped.</p><p>In <b>TCP</b> mode: Holds "
                                     "the TCP \"port\" when in server mode, or \"hostname:port\" when in client mode."));

    m_pComboNumber->setItemText(m_pComboNumber->count() - 1, uiCommon().toCOMPortName(0, 0));

    /* Translate combo-boxes content: */
    populateComboboxes();
}

void UIMachineSettingsSerial::sltGbSerialToggled(bool fOn)
{
    /* Update availability: */
    m_pWidgetPortSettings->setEnabled(m_pCheckBoxPort->isChecked());

    if (fOn)
    {
        sltCbNumberActivated(m_pComboNumber->currentText());
        sltHandlePortModeChange(m_pComboMode->currentIndex());
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::sltCbNumberActivated(const QString &strText)
{
    ulong uIRQ, uIOBase;
    bool fStd = uiCommon().toCOMPortNumbers(strText, uIRQ, uIOBase);

    m_pLineEditIRQ->setEnabled(!fStd);
    m_pLineEditIOPort->setEnabled(!fStd);
    if (fStd)
    {
        m_pLineEditIRQ->setText(QString::number(uIRQ));
        m_pLineEditIOPort->setText("0x" + QString::number(uIOBase, 16).toUpper());
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::sltHandlePortModeChange(int iIndex)
{
    const KPortMode enmMode = m_pComboMode->itemData(iIndex).value<KPortMode>();
    m_pCheckBoxPipe->setEnabled(enmMode == KPortMode_HostPipe || enmMode == KPortMode_TCP);
    m_pEditorPath->setEnabled(enmMode != KPortMode_Disconnected);
    m_pLabelPath->setEnabled(enmMode != KPortMode_Disconnected);

    /* Revalidate: */
    m_pParent->revalidate();
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
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setRowStretch(2, 1);

        /* Prepare port check-box: */
        m_pCheckBoxPort = new QCheckBox(this);
        if (m_pCheckBoxPort)
            pMainLayout->addWidget(m_pCheckBoxPort, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pMainLayout->addItem(pSpacerItem, 1, 0);

        /* Prepare adapter settings widget: */
        m_pWidgetPortSettings = new QWidget(this);
        if (m_pWidgetPortSettings)
        {
            /* Prepare adapter settings widget layout: */
            QGridLayout *pLayoutPortSettings = new QGridLayout(m_pWidgetPortSettings);
            if (pLayoutPortSettings)
            {
                pLayoutPortSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutPortSettings->setColumnStretch(6, 1);

                /* Prepare number label: */
                m_pLabelNumber = new QLabel(m_pWidgetPortSettings);
                if (m_pLabelNumber)
                {
                    m_pLabelNumber->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutPortSettings->addWidget(m_pLabelNumber, 0, 0);
                }
                /* Prepare number combo: */
                m_pComboNumber = new QComboBox(m_pWidgetPortSettings);
                if (m_pComboNumber)
                {
                    m_pLabelNumber->setBuddy(m_pComboNumber);
                    m_pComboNumber->insertItem(0, uiCommon().toCOMPortName(0, 0));
                    m_pComboNumber->insertItems(0, uiCommon().COMPortNames());
                    pLayoutPortSettings->addWidget(m_pComboNumber, 0, 1);
                }
                /* Prepare IRQ label: */
                m_pLabelIRQ = new QLabel(m_pWidgetPortSettings);
                if (m_pLabelIRQ)
                    pLayoutPortSettings->addWidget(m_pLabelIRQ, 0, 2);
                /* Prepare IRQ label: */
                m_pLineEditIRQ = new QLineEdit(m_pWidgetPortSettings);
                if (m_pLineEditIRQ)
                {
                    m_pLabelIRQ->setBuddy(m_pLineEditIRQ);
                    m_pLineEditIRQ->setFixedWidth(m_pLineEditIRQ->fontMetrics().width("8888"));
                    m_pLineEditIRQ->setValidator(new QIULongValidator(0, 255, this));
                    pLayoutPortSettings->addWidget(m_pLineEditIRQ, 0, 3);
                }
                /* Prepare IO port label: */
                m_pLabelIOPort = new QLabel(m_pWidgetPortSettings);
                if (m_pLabelIOPort)
                    pLayoutPortSettings->addWidget(m_pLabelIOPort, 0, 4);
                /* Prepare IO port label: */
                m_pLineEditIOPort = new QLineEdit(m_pWidgetPortSettings);
                if (m_pLineEditIOPort)
                {
                    m_pLabelIOPort->setBuddy(m_pLineEditIOPort);
                    m_pLineEditIOPort->setFixedWidth(m_pLineEditIOPort->fontMetrics().width("8888888"));
                    m_pLineEditIOPort->setValidator(new QIULongValidator(0, 0xFFFF, this));
                    pLayoutPortSettings->addWidget(m_pLineEditIOPort, 0, 5);
                }

                /* Prepare mode label: */
                m_pLabelMode = new QLabel(m_pWidgetPortSettings);
                if (m_pLabelMode)
                {
                    m_pLabelMode->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutPortSettings->addWidget(m_pLabelMode, 1, 0);
                }
                /* Prepare mode combo: */
                m_pComboMode = new QComboBox(m_pWidgetPortSettings);
                if (m_pComboMode)
                {
                    m_pLabelMode->setBuddy(m_pComboMode);
                    pLayoutPortSettings->addWidget(m_pComboMode, 1, 1);
                }

                /* Prepare pipe check-box: */
                m_pCheckBoxPipe = new QCheckBox(m_pWidgetPortSettings);
                if (m_pCheckBoxPipe)
                    pLayoutPortSettings->addWidget(m_pCheckBoxPipe, 2, 1, 1, 5);

                /* Prepare path label: */
                m_pLabelPath = new QLabel(m_pWidgetPortSettings);
                if (m_pLabelPath)
                {
                    m_pLabelPath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutPortSettings->addWidget(m_pLabelPath, 3, 0);
                }
                /* Prepare path editor: */
                m_pEditorPath = new QLineEdit(m_pWidgetPortSettings);
                if (m_pEditorPath)
                {
                    m_pLabelPath->setBuddy(m_pEditorPath);
                    m_pEditorPath->setValidator(new QRegExpValidator(QRegExp(".+"), this));
                    pLayoutPortSettings->addWidget(m_pEditorPath, 3, 1, 1, 6);
                }
            }

            pMainLayout->addWidget(m_pWidgetPortSettings, 1, 1);
        }
    }
}

void UIMachineSettingsSerial::prepareConnections()
{
    connect(m_pCheckBoxPort, &QCheckBox::toggled, this, &UIMachineSettingsSerial::sltGbSerialToggled);
    connect(m_pComboNumber, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::activated),
            this, &UIMachineSettingsSerial::sltCbNumberActivated);
    connect(m_pLineEditIRQ, &QLineEdit::textChanged, m_pParent, &UIMachineSettingsSerialPage::revalidate);
    connect(m_pLineEditIOPort, &QLineEdit::textChanged, m_pParent, &UIMachineSettingsSerialPage::revalidate);
    connect(m_pComboMode, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &UIMachineSettingsSerial::sltHandlePortModeChange);
    connect(m_pEditorPath, &QLineEdit::textChanged, m_pParent, &UIMachineSettingsSerialPage::revalidate);
}

void UIMachineSettingsSerial::populateComboboxes()
{
    /* Port mode: */
    {
        /* Clear the port mode combo-box: */
        m_pComboMode->clear();

        /* Load currently supported port moded: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KPortMode> supportedModes = comProperties.GetSupportedPortModes();
        /* Take currently requested mode into account if it's sane: */
        if (!supportedModes.contains(m_enmPortMode) && m_enmPortMode != KPortMode_Max)
            supportedModes.prepend(m_enmPortMode);

        /* Populate port modes: */
        int iPortModeIndex = 0;
        foreach (const KPortMode &enmMode, supportedModes)
        {
            m_pComboMode->insertItem(iPortModeIndex, gpConverter->toString(enmMode));
            m_pComboMode->setItemData(iPortModeIndex, QVariant::fromValue(enmMode));
            m_pComboMode->setItemData(iPortModeIndex, m_pComboMode->itemText(iPortModeIndex), Qt::ToolTipRole);
            ++iPortModeIndex;
        }

        /* Choose requested port mode: */
        const int iIndex = m_pComboMode->findData(m_enmPortMode);
        m_pComboMode->setCurrentIndex(iIndex != -1 ? iIndex : 0);
    }
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

    /* Prepare old serial data: */
    UIDataSettingsMachineSerial oldSerialData;

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Prepare old port data: */
        UIDataSettingsMachineSerialPort oldPortData;

        /* Check whether port is valid: */
        const CSerialPort &comPort = m_machine.GetSerialPort(iSlot);
        if (!comPort.isNull())
        {
            /* Gather old port data: */
            oldPortData.m_iSlot = iSlot;
            oldPortData.m_fPortEnabled = comPort.GetEnabled();
            oldPortData.m_uIRQ = comPort.GetIRQ();
            oldPortData.m_uIOBase = comPort.GetIOBase();
            oldPortData.m_hostMode = comPort.GetHostMode();
            oldPortData.m_fServer = comPort.GetServer();
            oldPortData.m_strPath = comPort.GetPath();
        }

        /* Cache old port data: */
        m_pCache->child(iSlot).cacheInitialData(oldPortData);
    }

    /* Cache old serial data: */
    m_pCache->cacheInitialData(oldSerialData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSerialPage::getFromCache()
{
    /* Setup tab order: */
    AssertPtrReturnVoid(firstWidget());
    setTabOrder(firstWidget(), m_pTabWidget->focusProxy());
    QWidget *pLastFocusWidget = m_pTabWidget->focusProxy();

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Get port page: */
        UIMachineSettingsSerial *pPage = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));

        /* Load old port data from the cache: */
        pPage->loadPortData(m_pCache->child(iSlot).base());

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
    /* Prepare new serial data: */
    UIDataSettingsMachineSerial newSerialData;

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Getting port page: */
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));

        /* Prepare new port data: */
        UIDataSettingsMachineSerialPort newPortData;

        /* Gather new port data: */
        pTab->savePortData(newPortData);

        /* Cache new port data: */
        m_pCache->child(iSlot).cacheCurrentData(newPortData);
    }

    /* Cache new serial data: */
    m_pCache->cacheCurrentData(newSerialData);
}

void UIMachineSettingsSerialPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update serial data and failing state: */
    setFailed(!saveSerialData());

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
        if (!pPage->m_pCheckBoxPort->isChecked())
            continue;

        /* Prepare message: */
        UIValidationMessage message;
        message.first = uiCommon().removeAccelMark(m_pTabWidget->tabText(m_pTabWidget->indexOf(pTab)));

        /* Check the port attribute emptiness & uniqueness: */
        const QString strIRQ(pPage->m_pLineEditIRQ->text());
        const QString strIOPort(pPage->m_pLineEditIOPort->text());
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

        const KPortMode enmMode = pPage->m_pComboMode->currentData().value<KPortMode>();
        if (enmMode != KPortMode_Disconnected)
        {
            const QString strPath(pPage->m_pEditorPath->text());

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
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        m_pTabWidget->setTabEnabled(iSlot,
                                    isMachineOffline() ||
                                    (isMachineInValidMode() &&
                                     m_pCache->childCount() > iSlot &&
                                     m_pCache->child(iSlot).base().m_fPortEnabled));
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(m_pTabWidget->widget(iSlot));
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
        /* Creating tab-widget: */
        m_pTabWidget = new QITabWidget;
        AssertPtrReturnVoid(m_pTabWidget);
        {
            /* How many ports to display: */
            const ulong uCount = uiCommon().virtualBox().GetSystemProperties().GetSerialPortCount();

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

bool UIMachineSettingsSerialPage::saveSerialData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save serial settings from the cache: */
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
    /* Prepare result: */
    bool fSuccess = true;
    /* Save adapter settings from the cache: */
    if (fSuccess && m_pCache->child(iSlot).wasChanged())
    {
        /* Get old serial data from the cache: */
        const UIDataSettingsMachineSerialPort &oldPortData = m_pCache->child(iSlot).base();
        /* Get new serial data from the cache: */
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
            /* Save port IO base: */
            if (fSuccess && isMachineOffline() && newPortData.m_uIOBase != oldPortData.m_uIOBase)
            {
                comPort.SetIOBase(newPortData.m_uIOBase);
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
