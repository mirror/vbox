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
    UIDataSettingsMachineParallel() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineParallel & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineParallel & /* other */) const { return false; }
};


/** Machine settings: Parallel Port tab. */
class UIMachineSettingsParallel : public QIWithRetranslateUI<QWidget>,
                                  public Ui::UIMachineSettingsParallel
{
    Q_OBJECT;

public:

    UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent);

    void polishTab();

    void fetchPortData(const UISettingsCacheMachineParallelPort &portCache);
    void uploadPortData(UISettingsCacheMachineParallelPort &portCache);

    QWidget* setOrderAfter (QWidget *aAfter);

    QString pageTitle() const;
    bool isUserDefined();

protected:

    void retranslateUi();

private slots:

    void mGbParallelToggled (bool aOn);
    void mCbNumberActivated (const QString &aText);

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    UIMachineSettingsParallelPage *m_pParent;
    int m_iSlot;
};


/* UIMachineSettingsParallel stuff */
UIMachineSettingsParallel::UIMachineSettingsParallel(UIMachineSettingsParallelPage *pParent)
    : QIWithRetranslateUI<QWidget> (0)
    , m_pParent(pParent)
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

    /* Prepare validation: */
    prepareValidation();

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

void UIMachineSettingsParallel::fetchPortData(const UISettingsCacheMachineParallelPort &portCache)
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

void UIMachineSettingsParallel::uploadPortData(UISettingsCacheMachineParallelPort &portCache)
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

    /* Revalidate: */
    m_pParent->revalidate();
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


/* UIMachineSettingsParallelPage stuff */
UIMachineSettingsParallelPage::UIMachineSettingsParallelPage()
    : mTabWidget(0)
    , m_pCache(new UISettingsCacheMachineParallel)
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

UIMachineSettingsParallelPage::~UIMachineSettingsParallelPage()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
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
        m_pCache->child(iSlot).cacheInitialData(portData);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

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

void UIMachineSettingsParallelPage::putToCache()
{
    /* For each parallel port: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        /* Getting port page: */
        UIMachineSettingsParallel *pPage = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iPort));

        /* Gather & cache port data: */
        pPage->uploadPortData(m_pCache->child(iPort));
    }
}

void UIMachineSettingsParallelPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if ports data was changed: */
    if (m_pCache->wasChanged())
    {
        /* For each parallel port: */
        for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
        {
            /* Check if port data was changed: */
            const UISettingsCacheMachineParallelPort &portCache = m_pCache->child(iPort);
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

bool UIMachineSettingsParallelPage::validate(QList<UIValidationMessage> &messages)
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
        UIMachineSettingsParallel *pPage = static_cast<UIMachineSettingsParallel*>(pTab);
        if (!pPage->mGbParallel->isChecked())
            continue;

        /* Prepare message: */
        UIValidationMessage message;
        message.first = vboxGlobal().removeAccelMark(mTabWidget->tabText(mTabWidget->indexOf(pTab)));

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
    for (int i = 0; i < mTabWidget->count(); ++i)
    {
        UIMachineSettingsParallel *pPage =
            static_cast<UIMachineSettingsParallel*>(mTabWidget->widget(i));
        mTabWidget->setTabText(i, pPage->pageTitle());
    }
}

void UIMachineSettingsParallelPage::polishPage()
{
    /* Get the count of parallel port tabs: */
    for (int iPort = 0; iPort < mTabWidget->count(); ++iPort)
    {
        mTabWidget->setTabEnabled(iPort,
                                  isMachineOffline() ||
                                  (isMachineInValidMode() && m_pCache->child(iPort).base().m_fPortEnabled));
        UIMachineSettingsParallel *pTab = qobject_cast<UIMachineSettingsParallel*>(mTabWidget->widget(iPort));
        pTab->polishTab();
    }
}

# include "UIMachineSettingsParallel.moc"

