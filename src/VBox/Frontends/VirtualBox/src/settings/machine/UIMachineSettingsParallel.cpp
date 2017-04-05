/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsParallel class implementation.
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
# include "UIMachineSettingsParallel.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CParallelPort.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: Parallel Port tab data structure. */
struct UIDataSettingsMachineParallelPort
{
    /** Constructs data. */
    UIDataSettingsMachineParallelPort()
        : m_iSlot(-1)
        , m_fPortEnabled(false)
        , m_uIRQ(0)
        , m_uIOBase(0)
        , m_strPath(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineParallelPort &other) const
    {
        return true
               && (m_iSlot == other.m_iSlot)
               && (m_fPortEnabled == other.m_fPortEnabled)
               && (m_uIRQ == other.m_uIRQ)
               && (m_uIOBase == other.m_uIOBase)
               && (m_strPath == other.m_strPath)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineParallelPort &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineParallelPort &other) const { return !equal(other); }

    /** Holds the parallel port slot number. */
    int      m_iSlot;
    /** Holds whether the parallel port is enabled. */
    bool     m_fPortEnabled;
    /** Holds the parallel port IRQ. */
    ulong    m_uIRQ;
    /** Holds the parallel port IO base. */
    ulong    m_uIOBase;
    /** Holds the parallel port path. */
    QString  m_strPath;
};


/** Machine settings: Parallel page data structure. */
struct UIDataSettingsMachineParallel
{
    /** Constructs data. */
    UIDataSettingsMachineParallel()
        : m_ports(QList<UIDataSettingsMachineParallelPort>())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineParallel &other) const
    {
        return true
               && (m_ports == other.m_ports)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineParallel &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineParallel &other) const { return !equal(other); }

    /** Holds the port list. */
    QList<UIDataSettingsMachineParallelPort> m_ports;
};


/** Machine settings: Parallel Port tab. */
class UIMachineSettingsParallel : public QIWithRetranslateUI<QWidget>,
                                  public Ui::UIMachineSettingsParallel
{
    Q_OBJECT;

public:

    UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent);

    void polishTab();

    void loadPortData(const UIDataSettingsMachineParallelPort &portData);
    void savePortData(UIDataSettingsMachineParallelPort &portData);

    QWidget *setOrderAfter(QWidget *pAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void sltGbParallelToggled(bool fOn);
    void sltCbNumberActivated(const QString &strText);

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    UIMachineSettingsParallelPage *m_pParent;
    int m_iSlot;
};


/*********************************************************************************************************************************
*   Class UIMachineSettingsParallel implementation.                                                                              *
*********************************************************************************************************************************/

UIMachineSettingsParallel::UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent)
    : QIWithRetranslateUI<QWidget>(0)
    , m_pParent(pParent)
    , m_iSlot(-1)
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsParallel::setupUi(this);

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

    /* Setup connections: */
    connect(mGbParallel, SIGNAL(toggled(bool)),
            this, SLOT(sltGbParallelToggled(bool)));
    connect(mCbNumber, SIGNAL(activated(const QString &)),
            this, SLOT(sltCbNumberActivated(const QString &)));

    /* Prepare validation: */
    prepareValidation();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsParallel::polishTab()
{
    /* Polish port page: */
    ulong uIRQ, uIOBase;
    const bool fStd = vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), uIRQ, uIOBase);
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

void UIMachineSettingsParallel::loadPortData(const UIDataSettingsMachineParallelPort &portData)
{
    /* Load port number: */
    m_iSlot = portData.m_iSlot;

    /* Load port data: */
    mGbParallel->setChecked(portData.m_fPortEnabled);
    mCbNumber->setCurrentIndex(mCbNumber->findText(vboxGlobal().toCOMPortName(portData.m_uIRQ, portData.m_uIOBase)));
    mLeIRQ->setText(QString::number(portData.m_uIRQ));
    mLeIOPort->setText("0x" + QString::number(portData.m_uIOBase, 16).toUpper());
    mLePath->setText(portData.m_strPath);

    /* Ensure everything is up-to-date: */
    sltGbParallelToggled(mGbParallel->isChecked());
}

void UIMachineSettingsParallel::savePortData(UIDataSettingsMachineParallelPort &portData)
{
    /* Save port data: */
    portData.m_fPortEnabled = mGbParallel->isChecked();
    portData.m_uIRQ = mLeIRQ->text().toULong(NULL, 0);
    portData.m_uIOBase = mLeIOPort->text().toULong(NULL, 0);
    portData.m_strPath = QDir::toNativeSeparators(mLePath->text());
}

QWidget *UIMachineSettingsParallel::setOrderAfter(QWidget *pAfter)
{
    setTabOrder(pAfter, mGbParallel);
    setTabOrder(mGbParallel, mCbNumber);
    setTabOrder(mCbNumber, mLeIRQ);
    setTabOrder(mLeIRQ, mLeIOPort);
    setTabOrder(mLeIOPort, mLePath);
    return mLePath;
}

QString UIMachineSettingsParallel::pageTitle() const
{
    return QString(tr("Port %1", "parallel ports")).arg(QString("&%1").arg(m_iSlot + 1));
}

bool UIMachineSettingsParallel::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers(mCbNumber->currentText(), a, b);
}

void UIMachineSettingsParallel::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsParallel::retranslateUi(this);

    mCbNumber->setItemText(mCbNumber->count() - 1, vboxGlobal().toCOMPortName(0, 0));
}

void UIMachineSettingsParallel::sltGbParallelToggled(bool fOn)
{
    if (fOn)
        sltCbNumberActivated(mCbNumber->currentText());

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsParallel::sltCbNumberActivated(const QString &strText)
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

void UIMachineSettingsParallel::prepareValidation()
{
    /* Prepare validation: */
    connect(mLeIRQ, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
    connect(mLeIOPort, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
    connect(mLePath, SIGNAL(textChanged(const QString&)), m_pParent, SLOT(revalidate()));
}


/*********************************************************************************************************************************
*   Class UIMachineSettingsParallelPage implementation.                                                                          *
*********************************************************************************************************************************/

UIMachineSettingsParallelPage::UIMachineSettingsParallelPage()
    : m_pTabWidget(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsParallelPage::~UIMachineSettingsParallelPage()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsParallelPage::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsParallelPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old parallel data: */
    UIDataSettingsMachineParallel oldParallelData;

    /* For each port: */
    for (int iSlot = 0; iSlot < m_pTabWidget->count(); ++iSlot)
    {
        /* Prepare old port data: */
        UIDataSettingsMachineParallelPort oldPortData;

        /* Check whether port is valid: */
        const CParallelPort &comPort = m_machine.GetParallelPort(iSlot);
        if (!comPort.isNull())
        {
            /* Gather old port data: */
            oldPortData.m_iSlot = iSlot;
            oldPortData.m_fPortEnabled = comPort.GetEnabled();
            oldPortData.m_uIRQ = comPort.GetIRQ();
            oldPortData.m_uIOBase = comPort.GetIOBase();
            oldPortData.m_strPath = comPort.GetPath();
        }

        /* Cache old port data: */
        oldParallelData.m_ports << oldPortData;
    }

    /* Cache old parallel data: */
    m_pCache->cacheInitialData(oldParallelData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsParallelPage::getFromCache()
{
    /* Setup tab order: */
    AssertPtrReturnVoid(firstWidget());
    setTabOrder(firstWidget(), m_pTabWidget->focusProxy());
    QWidget *pLastFocusWidget = m_pTabWidget->focusProxy();

    /* For each port: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        /* Get port page: */
        UIMachineSettingsParallel *pPage = qobject_cast<UIMachineSettingsParallel*>(m_pTabWidget->widget(iPort));

        /* Load old port data to the page: */
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

void UIMachineSettingsParallelPage::putToCache()
{
    /* Prepare new parallel data: */
    UIDataSettingsMachineParallel newParallelData;

    /* For each port: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        /* Getting port page: */
        UIMachineSettingsParallel *pTab = qobject_cast<UIMachineSettingsParallel*>(m_pTabWidget->widget(iPort));

        /* Prepare new port data: */
        UIDataSettingsMachineParallelPort newPortData;

        /* Gather new port data: */
        pTab->savePortData(newPortData);

        /* Cache new port data: */
        newParallelData.m_ports << newPortData;
    }

    /* Cache new parallel data: */
    m_pCache->cacheCurrentData(newParallelData);
}

void UIMachineSettingsParallelPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is offline & parallel data was changed: */
    if (isMachineOffline() && m_pCache->wasChanged())
    {
        /* For each parallel port: */
        for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
        {
            /* Get old parallel data from the cache: */
            const UIDataSettingsMachineParallelPort &oldPortData = m_pCache->base().m_ports.at(iPort);
            /* Get new parallel data from the cache: */
            const UIDataSettingsMachineParallelPort &newPortData = m_pCache->data().m_ports.at(iPort);

            /* Make sure port data was changed: */
            if (newPortData != oldPortData)
            {
                /* Check if port still valid: */
                CParallelPort comPort = m_machine.GetParallelPort(iPort);
                /* Store new adapter data: */
                if (!comPort.isNull())
                {
                    /* Whether the port is enabled: */
                    if (   comPort.isOk()
                        && newPortData.m_fPortEnabled != oldPortData.m_fPortEnabled)
                        comPort.SetEnabled(newPortData.m_fPortEnabled);
                    /* Port IRQ: */
                    if (   comPort.isOk()
                        && newPortData.m_uIRQ != oldPortData.m_uIRQ)
                        comPort.SetIRQ(newPortData.m_uIRQ);
                    /* Port IO base: */
                    if (   comPort.isOk()
                        && newPortData.m_uIOBase != oldPortData.m_uIOBase)
                        comPort.SetIOBase(newPortData.m_uIOBase);
                    /* Port path: */
                    if (   comPort.isOk()
                        && newPortData.m_strPath != oldPortData.m_strPath)
                        comPort.SetPath(newPortData.m_strPath);
                }
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsParallelPage::validate(QList<UIValidationMessage> &messages)
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
        UIMachineSettingsParallel *pPage = static_cast<UIMachineSettingsParallel*>(pTab);
        if (!pPage->mGbParallel->isChecked())
            continue;

        /* Prepare message: */
        UIValidationMessage message;
        message.first = vboxGlobal().removeAccelMark(m_pTabWidget->tabText(m_pTabWidget->indexOf(pTab)));

        /* Check the port attribute emptiness & uniqueness: */
        const QString strIRQ(pPage->mLeIRQ->text());
        const QString strIOPort(pPage->mLeIOPort->text());
        const QString strPath(pPage->mLePath->text());
        const QPair<QString, QString> pair(strIRQ, strIOPort);

        if (strIRQ.isEmpty())
        {
            message.second << UIMachineSettingsParallel::tr("No IRQ is currently specified.");
            fPass = false;
        }
        if (strIOPort.isEmpty())
        {
            message.second << UIMachineSettingsParallel::tr("No I/O port is currently specified.");
            fPass = false;
        }
        if (ports.contains(pair))
        {
            message.second << UIMachineSettingsParallel::tr("Two or more ports have the same settings.");
            fPass = false;
        }
        if (strPath.isEmpty())
        {
            message.second << UIMachineSettingsParallel::tr("No port path is currently specified.");
            fPass = false;
        }
        if (paths.contains(strPath))
        {
            message.second << UIMachineSettingsParallel::tr("There are currently duplicate port paths specified.");
            fPass = false;
        }

        ports << pair;
        paths << strPath;

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIMachineSettingsParallelPage::retranslateUi()
{
    for (int i = 0; i < m_pTabWidget->count(); ++i)
    {
        UIMachineSettingsParallel *pPage =
            static_cast<UIMachineSettingsParallel*>(m_pTabWidget->widget(i));
        m_pTabWidget->setTabText(i, pPage->pageTitle());
    }
}

void UIMachineSettingsParallelPage::polishPage()
{
    /* Get the count of parallel port tabs: */
    for (int iPort = 0; iPort < m_pTabWidget->count(); ++iPort)
    {
        m_pTabWidget->setTabEnabled(iPort,
                                    isMachineOffline() ||
                                    (isMachineInValidMode() &&
                                     m_pCache->base().m_ports.size() > iPort &&
                                     m_pCache->base().m_ports.at(iPort).m_fPortEnabled));
        UIMachineSettingsParallel *pTab = qobject_cast<UIMachineSettingsParallel*>(m_pTabWidget->widget(iPort));
        pTab->polishTab();
    }
}

void UIMachineSettingsParallelPage::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineParallel;
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
            const ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetParallelPortCount();

            /* Create corresponding port tabs: */
            for (ulong uPort = 0; uPort < uCount; ++uPort)
            {
                /* Create port tab: */
                UIMachineSettingsParallel *pTab = new UIMachineSettingsParallel(this);
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

void UIMachineSettingsParallelPage::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

# include "UIMachineSettingsParallel.moc"

