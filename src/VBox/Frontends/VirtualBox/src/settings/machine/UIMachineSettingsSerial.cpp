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
    UIDataSettingsMachineSerial() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSerial & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSerial & /* other */) const { return false; }
};


/** Machine settings: Serial Port tab. */
class UIMachineSettingsSerial : public QIWithRetranslateUI<QWidget>,
                                public Ui::UIMachineSettingsSerial
{
    Q_OBJECT;

public:

    UIMachineSettingsSerial(UIMachineSettingsSerialPage *pParent);

    void polishTab();

    void fetchPortData(const UISettingsCacheMachineSerialPort &data);
    void uploadPortData(UISettingsCacheMachineSerialPort &data);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbSerialToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);
    void mCbModeActivated (const QString &aText);

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    UIMachineSettingsSerialPage *m_pParent;
    int m_iSlot;
};


/* UIMachineSettingsSerial stuff */
UIMachineSettingsSerial::UIMachineSettingsSerial(UIMachineSettingsSerialPage *pParent)
    : QIWithRetranslateUI<QWidget> (0)
    , m_pParent(pParent)
    , m_iSlot(-1)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsSerial::setupUi (this);

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
    mCbMode->addItem (""); /* KPortMode_TCP */

    /* Setup connections */
    connect (mGbSerial, SIGNAL (toggled (bool)),
             this, SLOT (mGbSerialToggled (bool)));
    connect (mCbNumber, SIGNAL (activated (const QString &)),
             this, SLOT (mCbNumberActivated (const QString &)));
    connect (mCbMode, SIGNAL (activated (const QString &)),
             this, SLOT (mCbModeActivated (const QString &)));

    /* Prepare validation: */
    prepareValidation();

    /* Applying language settings */
    retranslateUi();
}

void UIMachineSettingsSerial::polishTab()
{
    ulong uIRQ, uIOBase;
    bool fStd = vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), uIRQ, uIOBase);
    KPortMode mode = gpConverter->fromString<KPortMode>(mCbMode->currentText());

    mGbSerial->setEnabled(m_pParent->isMachineOffline());
    mLbNumber->setEnabled(m_pParent->isMachineOffline());
    mCbNumber->setEnabled(m_pParent->isMachineOffline());
    mLbIRQ->setEnabled(m_pParent->isMachineOffline());
    mLeIRQ->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbIOPort->setEnabled(m_pParent->isMachineOffline());
    mLeIOPort->setEnabled(!fStd && m_pParent->isMachineOffline());
    mLbMode->setEnabled(m_pParent->isMachineOffline());
    mCbMode->setEnabled(m_pParent->isMachineOffline());
    mCbPipe->setEnabled((mode == KPortMode_HostPipe || mode == KPortMode_TCP)
      && m_pParent->isMachineOffline());
    mLbPath->setEnabled(m_pParent->isMachineOffline());
    mLePath->setEnabled(mode != KPortMode_Disconnected && m_pParent->isMachineOffline());
}

void UIMachineSettingsSerial::fetchPortData(const UISettingsCacheMachineSerialPort &portCache)
{
    /* Get port data: */
    const UIDataSettingsMachineSerialPort &portData = portCache.base();

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
    mGbSerialToggled(mGbSerial->isChecked());
}

void UIMachineSettingsSerial::uploadPortData(UISettingsCacheMachineSerialPort &portCache)
{
    /* Prepare port data: */
    UIDataSettingsMachineSerialPort portData = portCache.base();

    /* Save port data: */
    portData.m_fPortEnabled = mGbSerial->isChecked();
    portData.m_uIRQ = mLeIRQ->text().toULong(NULL, 0);
    portData.m_uIOBase = mLeIOPort->text().toULong (NULL, 0);
    portData.m_fServer = !mCbPipe->isChecked();
    portData.m_hostMode = gpConverter->fromString<KPortMode>(mCbMode->currentText());
    portData.m_strPath = QDir::toNativeSeparators(mLePath->text());

    /* Cache port data to port cache: */
    portCache.cacheCurrentData(portData);
}

QWidget* UIMachineSettingsSerial::setOrderAfter (QWidget *aAfter)
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

QString UIMachineSettingsSerial::pageTitle() const
{
    return QString(tr("Port %1", "serial ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsSerial::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mCbNumber->currentText(), a, b);
}

void UIMachineSettingsSerial::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsSerial::retranslateUi (this);

    mCbNumber->setItemText (mCbNumber->count() - 1, vboxGlobal().toCOMPortName (0, 0));

    mCbMode->setItemText (4, gpConverter->toString (KPortMode_TCP));
    mCbMode->setItemText (3, gpConverter->toString (KPortMode_RawFile));
    mCbMode->setItemText (2, gpConverter->toString (KPortMode_HostDevice));
    mCbMode->setItemText (1, gpConverter->toString (KPortMode_HostPipe));
    mCbMode->setItemText (0, gpConverter->toString (KPortMode_Disconnected));
}

void UIMachineSettingsSerial::mGbSerialToggled (bool aOn)
{
    if (aOn)
    {
        mCbNumberActivated (mCbNumber->currentText());
        mCbModeActivated (mCbMode->currentText());
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::mCbNumberActivated (const QString &aText)
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

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsSerial::mCbModeActivated (const QString &aText)
{
    KPortMode mode = gpConverter->fromString<KPortMode> (aText);
    mCbPipe->setEnabled (mode == KPortMode_HostPipe || mode == KPortMode_TCP);
    mLePath->setEnabled (mode != KPortMode_Disconnected);

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


/* UIMachineSettingsSerialPage stuff */
UIMachineSettingsSerialPage::UIMachineSettingsSerialPage()
    : mTabWidget(0)
    , m_pCache(new UISettingsCacheMachineSerial)
{
    /* TabWidget creation */
    mTabWidget = new QITabWidget (this);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 5, 0, 5);
    layout->addWidget (mTabWidget);

    /* How many ports to display: */
    ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
    /* Add corresponding tab pages to parent tab widget: */
    for (ulong uPort = 0; uPort < uCount; ++uPort)
    {
        /* Creating port page: */
        UIMachineSettingsSerial *pPage = new UIMachineSettingsSerial(this);
        mTabWidget->addTab(pPage, pPage->pageTitle());
    }
}

UIMachineSettingsSerialPage::~UIMachineSettingsSerialPage()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
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

    /* For each serial port: */
    for (int iSlot = 0; iSlot < mTabWidget->count(); ++iSlot)
    {
        /* Prepare port data: */
        UIDataSettingsMachineSerialPort portData;

        /* Check if port is valid: */
        const CSerialPort &port = m_machine.GetSerialPort(iSlot);
        if (!port.isNull())
        {
            /* Gather options: */
            portData.m_iSlot = iSlot;
            portData.m_fPortEnabled = port.GetEnabled();
            portData.m_uIRQ = port.GetIRQ();
            portData.m_uIOBase = port.GetIOBase();
            portData.m_hostMode = port.GetHostMode();
            portData.m_fServer = port.GetServer();
            portData.m_strPath = port.GetPath();
        }

        /* Cache port data: */
        m_pCache->child(iSlot).cacheInitialData(portData);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSerialPage::getFromCache()
{
    /* Setup tab order: */
    Assert(firstWidget());
    setTabOrder(firstWidget(), mTabWidget->focusProxy());
    QWidget *pLastFocusWidget = mTabWidget->focusProxy();

    /* For each serial port: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        /* Get port page: */
        UIMachineSettingsSerial *pPage = qobject_cast<UIMachineSettingsSerial*>(mTabWidget->widget(iPort));

        /* Load port data to page: */
        pPage->fetchPortData(m_pCache->child(iPort));

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSerialPage::putToCache()
{
    /* For each serial port: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        /* Getting port page: */
        UIMachineSettingsSerial *pPage = qobject_cast<UIMachineSettingsSerial*>(mTabWidget->widget(iPort));

        /* Gather & cache port data: */
        pPage->uploadPortData(m_pCache->child(iPort));
    }
}

void UIMachineSettingsSerialPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if ports data was changed: */
    if (m_pCache->wasChanged())
    {
        /* For each serial port: */
        for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
        {
            /* Check if port data was changed: */
            const UISettingsCacheMachineSerialPort &portCache = m_pCache->child(iPort);
            if (portCache.wasChanged())
            {
                /* Check if port still valid: */
                CSerialPort port = m_machine.GetSerialPort(iPort);
                if (!port.isNull())
                {
                    /* Get port data: */
                    const UIDataSettingsMachineSerialPort &portData = portCache.data();

                    /* Store adapter data: */
                    if (isMachineOffline())
                    {
                        port.SetEnabled(portData.m_fPortEnabled);
                        port.SetIRQ(portData.m_uIRQ);
                        port.SetIOBase(portData.m_uIOBase);
                        port.SetServer(portData.m_fServer);
                        port.SetPath(portData.m_strPath);
                        /* This *must* be last. The host mode will be changed to disconnected if
                         * some of the necessary settings above will not meet the requirements for
                         * the selected mode. */
                        port.SetHostMode(portData.m_hostMode);
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
    for (int iIndex = 0; iIndex < mTabWidget->count(); ++iIndex)
    {
        /* Get current tab/page: */
        QWidget *pTab = mTabWidget->widget(iIndex);
        UIMachineSettingsSerial *page = static_cast<UIMachineSettingsSerial*>(pTab);
        if (!page->mGbSerial->isChecked())
            continue;

        /* Prepare message: */
        UIValidationMessage message;
        message.first = vboxGlobal().removeAccelMark(mTabWidget->tabText(mTabWidget->indexOf(pTab)));

        /* Check the port attribute emptiness & uniqueness: */
        const QString strIRQ(page->mLeIRQ->text());
        const QString strIOPort(page->mLeIOPort->text());
        QPair<QString, QString> pair(strIRQ, strIOPort);

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

        KPortMode mode = gpConverter->fromString<KPortMode>(page->mCbMode->currentText());
        if (mode != KPortMode_Disconnected)
        {
            const QString strPath(page->mLePath->text());

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
    for (int i = 0; i < mTabWidget->count(); ++ i)
    {
        UIMachineSettingsSerial *page =
            static_cast<UIMachineSettingsSerial*> (mTabWidget->widget (i));
        mTabWidget->setTabText (i, page->pageTitle());
    }
}

void UIMachineSettingsSerialPage::polishPage()
{
    /* Get the count of serial port tabs: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        mTabWidget->setTabEnabled(iPort,
                                  isMachineOffline() ||
                                  (isMachineInValidMode() && m_pCache->child(iPort).base().m_fPortEnabled));
        UIMachineSettingsSerial *pTab = qobject_cast<UIMachineSettingsSerial*>(mTabWidget->widget(iPort));
        pTab->polishTab();
    }
}

# include "UIMachineSettingsSerial.moc"

