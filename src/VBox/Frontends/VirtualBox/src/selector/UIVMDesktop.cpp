/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMDesktop class implementation
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "QILabel.h"
#include "UIBar.h"
#include "UIIconPool.h"
#include "UIImageTools.h"
#include "UIPopupBox.h"
#include "UISpacerWidgets.h"
#include "UISpecialControls.h"
#include "UIVMDesktop.h"
#include "UIVMItem.h"
#include "UIVMPreviewWindow.h"
#include "UIVirtualBoxEventHandler.h"
#include "VBoxSnapshotsWgt.h"
#include "UIToolBar.h"

#include "VBoxUtils.h"

/* Global includes */
#include <QDir>
#include <QLabel>
#include <QScrollArea>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>

#include <iprt/assert.h>

/*
 * Todo:
 * - dynamically change size of preview window!
 */

//#ifdef Q_WS_MAC
# define DARWIN_USE_TOOLBAR
//#endif /* Q_WS_MAC */

static const QString sTableTpl =
"<table border=0 cellspacing=1 cellpadding=0>%1</table>";
static const QString sSectionItemTpl1 =
"<tr><td width=40%><nobr><i>%1</i></nobr></td><td/><td/></tr>";
static const QString sSectionItemTpl2 =
"<tr><td><nobr>%1:</nobr></td><td/><td>%2</td></tr>";
//        "<tr><td width=40%><nobr>%1:</nobr></td><td/><td>%2</td></tr>";
static const QString sSectionItemTpl3 =
"<tr><td width=40%><nobr>%1</nobr></td><td/><td/></tr>";
static const QString sSectionItemTpl4 =
"<tr><td>%1</td><td/><td/></tr>";

#ifdef Q_WS_MAC
static const int gsLeftMargin = 5;
static const int gsTopMargin = 5;
static const int gsRightMargin = 5;
static const int gsBottomMargin = 5;
#else /* Q_WS_MAC */
static const int gsLeftMargin = 0;
static const int gsTopMargin = 5;
static const int gsRightMargin = 5;
static const int gsBottomMargin = 5;
#endif /* !Q_WS_MAC */

/**
 *  UIDescriptionPagePrivate
 */
class UIDetailsPagePrivate : public QIWithRetranslateUI<QStackedWidget>
{
    Q_OBJECT;

    enum Section
    {
        GeneralSec,
        SystemSec,
        PreviewSec,
        DisplaySec,
        StorageSec,
        AudioSec,
        NetworkSec,
        SerialPortsSec,
#ifdef VBOX_WITH_PARALLEL_PORTS
        ParallelPortsSec,
#endif /* VBOX_WITH_PARALLEL_PORTS */
        USBSec,
        SharedFoldersSec,
        DescriptionSec,
        EndSec
    };

public:

    UIDetailsPagePrivate(QWidget *aParent, QAction *aRefreshAction = 0);
    ~UIDetailsPagePrivate();

    void setMachine(const CMachine& machine);

    void setText(const QString &aText)
    {
        m_machine = CMachine();
        createTextPage();
        m_pText->setText(aText);
        setCurrentIndex(indexOf(m_pText));
    }

    void setErrorText(const QString &aText)
    {
        m_machine = CMachine();
        createErrPage();
        m_pErrText->setText(aText);
        setCurrentIndex(indexOf(m_pErrBox));
    }

signals:

    void linkClicked(const QString &aURL);

protected:

    void retranslateUi();

private slots:

    void sltUpdateGeneral();
    void sltUpdatePreview();
    void sltUpdateSystem();
    void sltUpdateDisplay();
    void sltUpdateStorage();
    void sltUpdateAudio();
    void sltUpdateNetwork();
    void sltUpdateSerialPorts();
#ifdef VBOX_WITH_PARALLEL_PORTS
    void sltUpdateParallelPorts();
#endif /* VBOX_WITH_PARALLEL_PORTS */
    void sltUpdateUSB();
    void sltUpdateSharedFolders();
    void sltUpdateDescription();

    void sltContextMenuRequested(const QPoint &pos);

    void gotLinkClicked(const QUrl &aURL)
    {
        emit linkClicked(aURL.toString());
    }

private:

    void createTextPage();
    void createErrPage();

    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);

    /* Private member vars */
    CVirtualBox m_vbox;
    CMachine m_machine;

    /* Details view */
    QWidget *m_pDetails;
    QHash<Section, QString> m_EDStrs;
    QHash<Section, QAction*> m_actions;
    QHash<Section, UIPopupBox*> m_secBoxes;
    bool m_fChangeable;
    bool m_fUSBAvailable;

    /* Free text */
    QRichTextBrowser *m_pText;

    /* Error text */
    QWidget *m_pErrBox;
    QLabel *m_pErrLabel;
    QTextBrowser *m_pErrText;
    QToolButton *mRefreshButton;
    QAction *mRefreshAction;
};

UIDetailsPagePrivate::UIDetailsPagePrivate(QWidget *aParent,
                                           QAction *aRefreshAction /* = 0 */)
    : QIWithRetranslateUI<QStackedWidget>(aParent)
    , m_vbox(vboxGlobal().virtualBox())
    , m_fChangeable(false)
    , m_fUSBAvailable(true)
    , m_pText(0)
    , m_pErrBox(0), m_pErrLabel(0), m_pErrText(0)
    , mRefreshButton(0)
    , mRefreshAction(aRefreshAction)
{
    Assert(mRefreshAction);

    /* The names for the extra data list */
    m_EDStrs[GeneralSec]       = "general";
    m_EDStrs[SystemSec]        = "system";
    m_EDStrs[PreviewSec]       = "preview";
    m_EDStrs[DisplaySec]       = "display";
    m_EDStrs[StorageSec]       = "storage";
    m_EDStrs[AudioSec]         = "audio";
    m_EDStrs[NetworkSec]       = "network";
    m_EDStrs[SerialPortsSec]   = "serialPorts";
#ifdef VBOX_WITH_PARALLEL_PORTS
    m_EDStrs[ParallelPortsSec] = "parallelPorts";
#endif /* VBOX_WITH_PARALLEL_PORTS */
    m_EDStrs[USBSec]           = "usb";
    m_EDStrs[SharedFoldersSec] = "sharedFolders";
    m_EDStrs[DescriptionSec]   = "description";

    QScrollArea *pSArea = new QScrollArea(this);
    pSArea->setFrameStyle(QFrame::NoFrame);
    m_pDetails = new QWidget(this);
    QVBoxLayout *pMainLayout = new QVBoxLayout(m_pDetails);
    pMainLayout->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);

    /* Create the context menu, which allows showing/hiding of the boxes */
    m_pDetails->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pDetails, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltContextMenuRequested(const QPoint&)));
    for(int i = 0; i < EndSec; ++i)
    {
        /* Create the context menu actions */
        QAction *pAction = new QAction(this);
        pAction->setData(i);
        pAction->setCheckable(true);
        m_actions[static_cast<Section>(i)] = pAction;

        /* Create the popup boxes */
        UIPopupBox *pPopup = new UIPopupBox(this);
        connect(pPopup, SIGNAL(titleClicked(const QString)),
                this, SIGNAL(linkClicked(const QString)));
        pPopup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_secBoxes[static_cast<Section>(i)] = pPopup;
    }

    /* General */
    {
        QILabel *pLabel= new QILabel(m_secBoxes.value(GeneralSec));
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateGeneral()));
        m_secBoxes.value(GeneralSec)->setTitleIcon(UIIconPool::iconSet(":/machine_16px.png"));
        m_secBoxes.value(GeneralSec)->setTitleLink("#general");
        m_secBoxes.value(GeneralSec)->setContentWidget(pLabel);
        m_secBoxes.value(GeneralSec)->hide();
    }

    /* System */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateSystem()));
        m_secBoxes.value(SystemSec)->setTitleIcon(UIIconPool::iconSet(":/chipset_16px.png"));
        m_secBoxes.value(SystemSec)->setTitleLink("#system");
        m_secBoxes.value(SystemSec)->setContentWidget(pLabel);
        m_secBoxes.value(SystemSec)->hide();
    }

    /* Preview */
    {
        UIVMPreviewWindow *pWidget = new UIVMPreviewWindow(this);
        m_secBoxes.value(PreviewSec)->setTitleIcon(UIIconPool::iconSet(":/machine_16px.png"));
        m_secBoxes.value(PreviewSec)->setContentWidget(pWidget);
        m_secBoxes.value(PreviewSec)->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        /* Make sure the width is always the same, regardless if the preview is
         * shown or not. */
        m_secBoxes.value(PreviewSec)->setFixedWidth(m_secBoxes.value(PreviewSec)->sizeHint().width());
        m_secBoxes.value(PreviewSec)->hide();
    }

    QHBoxLayout *tt1 = new QHBoxLayout();
    QVBoxLayout *tt2 = new QVBoxLayout();
    tt2->addWidget(m_secBoxes.value(GeneralSec));
    tt2->addWidget(m_secBoxes.value(SystemSec));
    tt2->addStretch(1);
    tt1->addLayout(tt2);
    QVBoxLayout *tt3 = new QVBoxLayout();
    tt3->addWidget(m_secBoxes.value(PreviewSec));
    tt3->addStretch(1);
    tt1->addLayout(tt3);
    pMainLayout->addLayout(tt1);

    /* Display */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateDisplay()));
        m_secBoxes.value(DisplaySec)->setTitleIcon(UIIconPool::iconSet(":/vrdp_16px.png"));
        m_secBoxes.value(DisplaySec)->setTitleLink("#display");
        m_secBoxes.value(DisplaySec)->setContentWidget(pLabel);
        m_secBoxes.value(DisplaySec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(DisplaySec));
    }

    /* Storage */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateStorage()));
        m_secBoxes.value(StorageSec)->setTitleIcon(UIIconPool::iconSet(":/attachment_16px.png"));
        m_secBoxes.value(StorageSec)->setTitleLink("#storage");
        m_secBoxes.value(StorageSec)->setContentWidget(pLabel);
        m_secBoxes.value(StorageSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(StorageSec));
    }

    /* Audio */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateAudio()));
        m_secBoxes.value(AudioSec)->setTitleIcon(UIIconPool::iconSet(":/sound_16px.png"));
        m_secBoxes.value(AudioSec)->setTitleLink("#audio");
        m_secBoxes.value(AudioSec)->setContentWidget(pLabel);
        m_secBoxes.value(AudioSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(AudioSec));
    }

    /* Network */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateNetwork()));
        m_secBoxes.value(NetworkSec)->setTitleIcon(UIIconPool::iconSet(":/nw_16px.png"));
        m_secBoxes.value(NetworkSec)->setTitleLink("#network");
        m_secBoxes.value(NetworkSec)->setContentWidget(pLabel);
        m_secBoxes.value(NetworkSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(NetworkSec));
    }

    /* Serial Ports */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateSerialPorts()));
        m_secBoxes.value(SerialPortsSec)->setTitleIcon(UIIconPool::iconSet(":/serial_port_16px.png"));
        m_secBoxes.value(SerialPortsSec)->setTitleLink("#serialPorts");
        m_secBoxes.value(SerialPortsSec)->setContentWidget(pLabel);
        m_secBoxes.value(SerialPortsSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(SerialPortsSec));
    }

#ifdef VBOX_WITH_PARALLEL_PORTS
    /* Parallel Ports */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateParallelPorts()));
        m_secBoxes.value(ParallelPortsSec)->setTitleIcon(UIIconPool::iconSet(":/parallel_port_16px.png"));
        m_secBoxes.value(ParallelPortsSec)->setTitleLink("#parallelPorts");
        m_secBoxes.value(ParallelPortsSec)->setContentWidget(pLabel);
        m_secBoxes.value(ParallelPortsSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(ParallelPortsSec));
    }
#endif /* VBOX_WITH_PARALLEL_PORTS */

    /* USB */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateUSB()));
        m_secBoxes.value(USBSec)->setTitleIcon(UIIconPool::iconSet(":/usb_16px.png"));
        m_secBoxes.value(USBSec)->setTitleLink("#usb");
        m_secBoxes.value(USBSec)->setContentWidget(pLabel);
        m_secBoxes.value(USBSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(USBSec));
    }

    /* Shared Folders */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateSharedFolders()));
        m_secBoxes.value(SharedFoldersSec)->setTitleIcon(UIIconPool::iconSet(":/shared_folder_16px.png"));
        m_secBoxes.value(SharedFoldersSec)->setTitleLink("#sfolders");
        m_secBoxes.value(SharedFoldersSec)->setContentWidget(pLabel);
        m_secBoxes.value(SharedFoldersSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(SharedFoldersSec));
    }

    /* Description */
    {
        QILabel *pLabel= new QILabel(this);
        pLabel->setWordWrap(true);
        connect(pLabel, SIGNAL(shown()),
                this, SLOT(sltUpdateDescription()));
        m_secBoxes.value(DescriptionSec)->setTitleIcon(UIIconPool::iconSet(":/description_16px.png"));
        m_secBoxes.value(DescriptionSec)->setTitleLink("#general%%mTeDescription");
        m_secBoxes.value(DescriptionSec)->setContentWidget(pLabel);
        m_secBoxes.value(DescriptionSec)->hide();
        pMainLayout->addWidget(m_secBoxes.value(DescriptionSec));
    }

    pMainLayout->addStretch(1);

    pSArea->setWidget(m_pDetails);
    pSArea->setWidgetResizable(true);
    addWidget(pSArea);

    /* Read the configuration from extra data. Defaults are provided in place. */
    QStringList boxes = vboxGlobal().virtualBox().GetExtraDataStringList(VBoxDefs::GUI_DetailsPageBoxes,
                                                                         /* Defaults */
                                                                         QStringList()
                                                                         << m_EDStrs.value(GeneralSec)
                                                                         << m_EDStrs.value(SystemSec)
                                                                         << m_EDStrs.value(PreviewSec)
                                                                         << m_EDStrs.value(DisplaySec)
                                                                         << m_EDStrs.value(StorageSec)
                                                                         << m_EDStrs.value(AudioSec)
                                                                         << m_EDStrs.value(NetworkSec)
                                                                         << m_EDStrs.value(USBSec)
                                                                         << m_EDStrs.value(SharedFoldersSec)
                                                                         << m_EDStrs.value(DescriptionSec));
    for (int i = 0; i < boxes.size(); ++i)
    {
        QString strED = boxes.value(i);
        bool fOpen = !strED.endsWith("Closed");
        Section sec = m_EDStrs.key(fOpen ? strED : strED.remove("Closed"), EndSec);
        if (sec != EndSec)
        {
            m_actions[sec]->setChecked(true);
            m_secBoxes.value(sec)->setOpen(fOpen);
            m_secBoxes.value(sec)->show();
        }
    }

    retranslateUi();
}

UIDetailsPagePrivate::~UIDetailsPagePrivate()
{
    /* Save the configuration to extra data at destruction */
    QStringList boxes;
    for (int i = 0; i < EndSec; ++i)
    {
        if (m_actions[static_cast<Section>(i)]->isChecked())
        {
            QString strED = m_EDStrs.value(static_cast<Section>(i));
            if (!m_secBoxes.value(static_cast<Section>(i))->isOpen())
                strED += "Closed";
            boxes << strED;
        }
    }
    vboxGlobal().virtualBox().SetExtraDataStringList(VBoxDefs::GUI_DetailsPageBoxes,
                                                     boxes);
}

void UIDetailsPagePrivate::retranslateUi()
{
    if (m_pErrLabel)
        m_pErrLabel->setText(tr(
            "The selected virtual machine is <i>inaccessible</i>. Please "
            "inspect the error message shown below and press the "
            "<b>Refresh</b> button if you want to repeat the accessibility "
            "check:"));

    if (mRefreshAction && mRefreshButton)
    {
        mRefreshButton->setText(mRefreshAction->text());
        mRefreshButton->setIcon(mRefreshAction->icon());
        mRefreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    /* General */
    {
        m_secBoxes.value(GeneralSec)->setTitle(tr("General", "details report"));
        m_actions.value(GeneralSec)->setText(m_secBoxes.value(GeneralSec)->title());
    }

    /* System */
    {
        m_secBoxes.value(SystemSec)->setTitle(tr("System", "details report"));
        m_actions.value(SystemSec)->setText(m_secBoxes.value(SystemSec)->title());
    }

    /* Preview */
    {
        m_secBoxes.value(PreviewSec)->setTitle(tr("Preview", "details report"));
        m_actions.value(PreviewSec)->setText(m_secBoxes.value(PreviewSec)->title());
    }

    /* Display */
    {
        m_secBoxes.value(DisplaySec)->setTitle(tr("Display", "details report"));
        m_actions.value(DisplaySec)->setText(m_secBoxes.value(DisplaySec)->title());
    }

    /* Storage */
    {
        m_secBoxes.value(StorageSec)->setTitle(tr("Storage", "details report"));
        m_actions.value(StorageSec)->setText(m_secBoxes.value(StorageSec)->title());
    }

    /* Audio */
    {
        m_secBoxes.value(AudioSec)->setTitle(tr("Audio", "details report"));
        m_actions.value(AudioSec)->setText(m_secBoxes.value(AudioSec)->title());
    }

    /* Network */
    {
        m_secBoxes.value(NetworkSec)->setTitle(tr("Network", "details report"));
        m_actions.value(NetworkSec)->setText(m_secBoxes.value(NetworkSec)->title());
    }

    /* Serial Ports */
    {
        m_secBoxes.value(SerialPortsSec)->setTitle(tr("Serial Ports", "details report"));
        m_actions.value(SerialPortsSec)->setText(m_secBoxes.value(SerialPortsSec)->title());
    }

#ifdef VBOX_WITH_PARALLEL_PORTS
    /* Parallel Ports */
    {
        m_secBoxes.value(ParallelPortsSec)->setTitle(tr("Parallel Ports", "details report"));
        m_actions.value(ParallelPortsSec)->setText(m_secBoxes.value(ParallelPortsSec)->title());
    }
#endif /* VBOX_WITH_PARALLEL_PORTS */

    /* USB */
    {
        m_secBoxes.value(USBSec)->setTitle(tr("USB", "details report"));
        m_actions.value(USBSec)->setText(m_secBoxes.value(USBSec)->title());
    }

    /* Shared Folders */
    {
        m_secBoxes.value(SharedFoldersSec)->setTitle(tr("Shared Folders", "details report"));
        m_actions.value(SharedFoldersSec)->setText(m_secBoxes.value(SharedFoldersSec)->title());
    }

    /* Description */
    {
        m_secBoxes.value(DescriptionSec)->setTitle(tr("Description", "details report"));
        m_actions.value(DescriptionSec)->setText(m_secBoxes.value(DescriptionSec)->title());
    }
}

void UIDetailsPagePrivate::sltUpdateGeneral()
{
    m_secBoxes.value(GeneralSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(GeneralSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item = sSectionItemTpl2.arg(tr("Name", "details report"),
                                                m_machine.GetName())
                + sSectionItemTpl2.arg(tr("OS Type", "details report"),
                                       vboxGlobal().vmGuestOSTypeDescription(m_machine.GetOSTypeId()));

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);

        }else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateSystem()
{
    m_secBoxes.value(SystemSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(SystemSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item = sSectionItemTpl2.arg(tr("Base Memory", "details report"),
                                                tr("<nobr>%1 MB</nobr>", "details report"))
                .arg(m_machine.GetMemorySize());

            int cCPU = m_machine.GetCPUCount();
            if (cCPU > 1)
                item += sSectionItemTpl2.arg(tr("Processors", "details report"),
                                             tr("<nobr>%1</nobr>", "details report"))
                    .arg(cCPU);

#ifdef VBOX_WITH_FULL_DETAILS_REPORT
            /* Execution Cap
             * is NOT such important attribute to be reflected in details page... */
            int iCPUExecCap = m_machine.GetCPUExecutionCap();
            if (iCPUExecCap < 100)
                item += sSectionItemTpl2.arg(tr("Execution Cap", "details report"),
                                             tr("<nobr>%1%</nobr>", "details report"))
                    .arg(iCPUExecCap);
#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

            /* Boot order */
            QStringList bootOrder;
            for (ulong i = 1; i <= m_vbox.GetSystemProperties().GetMaxBootPosition(); ++i)
            {
                KDeviceType device = m_machine.GetBootOrder(i);
                if (device == KDeviceType_Null)
                    continue;
                bootOrder << vboxGlobal().toString(device);
            }
            if (bootOrder.isEmpty())
                bootOrder << vboxGlobal().toString(KDeviceType_Null);

            item += sSectionItemTpl2.arg(tr("Boot Order", "details report"), bootOrder.join(", "));

#ifdef VBOX_WITH_FULL_DETAILS_REPORT
            /* BIOS Settings holder */
            const CBIOSSettings &biosSettings = m_machine.GetBIOSSettings();
            QStringList bios;
            /* ACPI */
            if (biosSettings.GetACPIEnabled())
                bios << tr("ACPI", "details report");

            /* IO APIC */
            if (biosSettings.GetIOAPICEnabled())
                bios << tr("IO APIC", "details report");

            if (!bios.isEmpty())
                item += sSectionItemTpl2.arg(tr("BIOS", "details report"), bios.join(", "));
#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

            QStringList accel;
            if (m_vbox.GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx))
            {
                /* VT-x/AMD-V */
                if (m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
                {
                    accel << tr("VT-x/AMD-V", "details report");

                    /* Nested Paging (only when hw virt is enabled) */
                    if (m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                        accel << tr("Nested Paging", "details report");
                }
            }
            /* PAE/NX */
            if (m_machine.GetCPUProperty(KCPUPropertyType_PAE))
                accel << tr("PAE/NX", "details report");

            if (!accel.isEmpty())
                item += sSectionItemTpl2.arg(tr("Acceleration", "details report"), accel.join(", "));

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdatePreview()
{
    qobject_cast<UIVMPreviewWindow*>(m_secBoxes.value(PreviewSec)->contentWidget())->setMachine(m_machine);
}

void UIDetailsPagePrivate::sltUpdateDisplay()
{
    m_secBoxes.value(DisplaySec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(DisplaySec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            /* Video tab */
            QString item = QString(sSectionItemTpl2)
                .arg(tr("Video Memory", "details report"),
                     tr("<nobr>%1 MB</nobr>", "details report"))
                .arg(m_machine.GetVRAMSize());

            int cGuestScreens = m_machine.GetMonitorCount();
            if (cGuestScreens > 1)
            {
                item += QString(sSectionItemTpl2)
                    .arg(tr("Screens", "details report"))
                    .arg(cGuestScreens);
            }

            QStringList accel;
#ifdef VBOX_WITH_VIDEOHWACCEL
            if (m_machine.GetAccelerate2DVideoEnabled())
                accel << tr("2D Video", "details report");
#endif /* VBOX_WITH_VIDEOHWACCEL */
            if (m_machine.GetAccelerate3DEnabled())
                accel << tr("3D", "details report");

            if (!accel.isEmpty())
                item += sSectionItemTpl2.arg(tr("Acceleration", "details report"), accel.join(", "));

            /* VRDE tab */
            CVRDEServer srv = m_machine.GetVRDEServer();
            if (!srv.isNull())
            {
                if (srv.GetEnabled())
                    item += QString(sSectionItemTpl2)
                        .arg(tr("Remote Desktop Server Port", "details report (VRDE Server)"))
                        .arg(srv.GetVRDEProperty("TCP/Ports"));
                else
                    item += QString(sSectionItemTpl2)
                        .arg(tr("Remote Desktop Server", "details report (VRDE Server)"))
                        .arg(tr("Disabled", "details report (VRDE Server)"));
            }

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateStorage()
{
    m_secBoxes.value(StorageSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(StorageSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;
            /* Iterate over the all machine controllers: */
            const CStorageControllerVector &controllers = m_machine.GetStorageControllers();
            for (int i = 0; i < controllers.size(); ++i)
            {
                /* Get current controller: */
                const CStorageController &controller = controllers[i];
                /* Add controller information: */
                item += QString(sSectionItemTpl3).arg(controller.GetName());

                /* Populate sorted map with attachments information: */
                QMap<StorageSlot,QString> attachmentsMap;
                const CMediumAttachmentVector &attachments = m_machine.GetMediumAttachmentsOfController(controller.GetName());
                for (int j = 0; j < attachments.size(); ++j)
                {
                    /* Get current attachment: */
                    const CMediumAttachment &attachment = attachments[j];
                    /* Prepare current storage slot: */
                    StorageSlot attachmentSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice());
                    /* Append 'device slot name' with 'device type name' for CD/DVD devices only: */
                    QString strDeviceType = attachment.GetType() == KDeviceType_DVD ? tr("(CD/DVD)") : QString();
                    if (!strDeviceType.isNull())
                        strDeviceType.prepend(' ');
                    /* Prepare current medium object: */
                    const CMedium &medium = attachment.GetMedium();
                    /* Prepare information about current medium & attachment: */
                    QString strAttachmentInfo = !attachment.isOk() ? QString() :
                        QString(sSectionItemTpl2)
                        .arg(QString("&nbsp;&nbsp;") +
                             vboxGlobal().toString(StorageSlot(controller.GetBus(),
                                                               attachment.GetPort(),
                                                               attachment.GetDevice())) + strDeviceType)
                        .arg(vboxGlobal().details(medium, false));
                    /* Insert that attachment into map: */
                    if (!strAttachmentInfo.isNull())
                        attachmentsMap.insert(attachmentSlot, strAttachmentInfo);
                }

                /* Iterate over the sorted map with attachments information: */
                QMapIterator<StorageSlot,QString> it(attachmentsMap);
                while (it.hasNext())
                {
                    /* Add controller information: */
                    it.next();
                    item += it.value();
                }
            }

            if (item.isNull())
            {
                item = QString(sSectionItemTpl1)
                    .arg(tr("Not Attached", "details report (Storage)"));
            }

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateAudio()
{
    m_secBoxes.value(AudioSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(AudioSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            const CAudioAdapter &audio = m_machine.GetAudioAdapter();
            if (audio.GetEnabled())
                item = QString(sSectionItemTpl2)
                    .arg(tr("Host Driver", "details report (audio)"),
                         vboxGlobal().toString(audio.GetAudioDriver())) +
                    QString(sSectionItemTpl2)
                    .arg(tr("Controller", "details report (audio)"),
                         vboxGlobal().toString(audio.GetAudioController()));
            else
                item = QString(sSectionItemTpl1)
                    .arg(tr("Disabled", "details report (audio)"));

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateNetwork()
{
    m_secBoxes.value(NetworkSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(NetworkSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3);
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CNetworkAdapter &adapter = m_machine.GetNetworkAdapter(slot);
                if (adapter.GetEnabled())
                {
                    KNetworkAttachmentType type = adapter.GetAttachmentType();
                    QString attType = vboxGlobal().toString(adapter.GetAdapterType())
                        .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                    /* don't use the adapter type string for types that have
                     * an additional symbolic network/interface name field, use
                     * this name instead */
                    if (type == KNetworkAttachmentType_Bridged)
                        attType = attType.arg(tr("Bridged adapter, %1",
                                                 "details report (network)").arg(adapter.GetBridgedInterface()));
                    else if (type == KNetworkAttachmentType_Internal)
                        attType = attType.arg(tr("Internal network, '%1'",
                                                 "details report (network)").arg(adapter.GetInternalNetwork()));
                    else if (type == KNetworkAttachmentType_HostOnly)
                        attType = attType.arg(tr("Host-only adapter, '%1'",
                                                 "details report (network)").arg(adapter.GetHostOnlyInterface()));
                    else if (type == KNetworkAttachmentType_Generic)
                    {
                        QString strGenericDriverProperties(summarizeGenericProperties(adapter));
                        attType = strGenericDriverProperties.isNull() ?
                                  attType.arg(tr("Generic driver, '%1'", "details report (network)").arg(adapter.GetGenericDriver())) :
                                  attType.arg(tr("Generic driver, '%1' {&nbsp;%2&nbsp;}", "details report (network)")
                                              .arg(adapter.GetGenericDriver(), strGenericDriverProperties));
                    }
                    else
                        attType = attType.arg(vboxGlobal().toString(type));

                    item += QString(sSectionItemTpl2)
                        .arg(tr("Adapter %1", "details report (network)")
                             .arg(adapter.GetSlot() + 1))
                        .arg(attType);
                }
            }
            if (item.isNull())
            {
                item = QString(sSectionItemTpl1)
                    .arg(tr("Disabled", "details report (network)"));
            }

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateSerialPorts()
{
    m_secBoxes.value(SerialPortsSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(SerialPortsSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetSerialPortCount();
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CSerialPort &port = m_machine.GetSerialPort(slot);
                if (port.GetEnabled())
                {
                    KPortMode mode = port.GetHostMode();
                    QString data =
                        vboxGlobal().toCOMPortName(port.GetIRQ(), port.GetIOBase()) + ", ";
                    if (mode == KPortMode_HostPipe ||
                        mode == KPortMode_HostDevice ||
                        mode == KPortMode_RawFile)
                        data += QString("%1 (<nobr>%2</nobr>)")
                            .arg(vboxGlobal().toString(mode))
                            .arg(QDir::toNativeSeparators(port.GetPath()));
                    else
                        data += vboxGlobal().toString(mode);

                    item += QString(sSectionItemTpl2)
                        .arg(tr("Port %1", "details report (serial ports)")
                             .arg(port.GetSlot() + 1))
                        .arg(data);
                }
            }
            if (item.isNull())
            {
                item = QString(sSectionItemTpl1)
                    .arg(tr("Disabled", "details report (serial ports)"));
            }

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

#ifdef VBOX_WITH_PARALLEL_PORTS
void UIDetailsPagePrivate::sltUpdateParallelPorts()
{
    m_secBoxes.value(ParallelPortsSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(ParallelPortsSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetParallelPortCount();
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CParallelPort &port = m_machine.GetParallelPort(slot);
                if (port.GetEnabled())
                {
                    QString data =
                        vboxGlobal().toLPTPortName(port.GetIRQ(), port.GetIOBase()) +
                        QString(" (<nobr>%1</nobr>)")
                        .arg(QDir::toNativeSeparators(port.GetPath()));

                    item += QString(sSectionItemTpl2)
                        .arg(tr("Port %1", "details report (parallel ports)")
                             .arg(port.GetSlot() + 1))
                        .arg(data);
                }
            }
            if (item.isNull())
            {
                item = QString(sSectionItemTpl1)
                    .arg(tr("Disabled", "details report (parallel ports)"));
            }

            /* Currently disabled */
            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}
#endif /* VBOX_WITH_PARALLEL_PORTS */

void UIDetailsPagePrivate::sltUpdateUSB()
{
    m_secBoxes.value(USBSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(USBSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            const CUSBController &ctl = m_machine.GetUSBController();
            if (   !ctl.isNull()
                && ctl.GetProxyAvailable())
            {
                m_fUSBAvailable = true;
                /* the USB controller may be unavailable (i.e. in VirtualBox OSE) */
                if (ctl.GetEnabled())
                {
                    const CUSBDeviceFilterVector &coll = ctl.GetDeviceFilters();
                    uint active = 0;
                    for (int i = 0; i < coll.size(); ++i)
                        if (coll[i].GetActive())
                            active ++;

                    item = QString(sSectionItemTpl2)
                        .arg(tr("Device Filters", "details report (USB)"),
                             tr("%1 (%2 active)", "details report (USB)")
                             .arg(coll.size()).arg(active));
                }
                else
                    item = QString(sSectionItemTpl1)
                        .arg(tr("Disabled", "details report (USB)"));

                QString table = sTableTpl.arg(item);
                pLabel->setText(table);
            } else
            {
                m_fUSBAvailable = false;
                /* Fully hide when USB is not available */
                m_secBoxes.value(USBSec)->hide();
            }
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateSharedFolders()
{
    m_secBoxes.value(SharedFoldersSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(SharedFoldersSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;

            ulong count = m_machine.GetSharedFolders().size();
            if (count > 0)
            {
                item = QString(sSectionItemTpl2)
                    .arg(tr("Shared Folders", "details report (shared folders)"))
                    .arg(count);
            }
            else
                item = QString(sSectionItemTpl1)
                    .arg(tr("None", "details report (shared folders)"));

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);
        } else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltUpdateDescription()
{
    m_secBoxes.value(DescriptionSec)->setTitleLinkEnabled(m_fChangeable);
    QILabel *pLabel = qobject_cast<QILabel*>(m_secBoxes.value(DescriptionSec)->contentWidget());
    if (pLabel->isVisible())
    {
        if (!m_machine.isNull())
        {
            QString item;
            const QString &strDesc = m_machine.GetDescription();
            if (!strDesc.isEmpty())
                item = QString(sSectionItemTpl4)
                    .arg(strDesc);
            else
                item = QString(sSectionItemTpl1)
                    .arg(tr("None", "details report (description)"));

            QString table = sTableTpl.arg(item);
            pLabel->setText(table);

        }else
            pLabel->setText("");
    }
}

void UIDetailsPagePrivate::sltContextMenuRequested(const QPoint &pos)
{
    QList<QAction*> actions = m_actions.values();
    if (!m_fUSBAvailable)
        actions.removeOne(m_actions.value(USBSec));
    QAction *pReturn = QMenu::exec(actions, m_pDetails->mapToGlobal(pos), 0);
    if (pReturn)
    {
        UIPopupBox *pBox = m_secBoxes.value(static_cast<Section>(pReturn->data().toInt()));
        if (pReturn->isChecked())
            pBox->show();
        else
            pBox->hide();
    }
}

void UIDetailsPagePrivate::setMachine(const CMachine& machine)
{
    m_machine = machine;
    m_fChangeable = m_machine.isNull() ? false :
                    m_machine.GetState() != KMachineState_Stuck &&
                    m_machine.GetState() != KMachineState_Saved /* for now! */;

    sltUpdateGeneral();
    sltUpdateSystem();
    sltUpdatePreview();
    sltUpdateDisplay();
    sltUpdateStorage();
    sltUpdateAudio();
    sltUpdateNetwork();
    sltUpdateSerialPorts();
#ifdef VBOX_WITH_PARALLEL_PORTS
    sltUpdateParallelPorts();
#endif /* VBOX_WITH_PARALLEL_PORTS */
    sltUpdateUSB();
    sltUpdateSharedFolders();
    sltUpdateDescription();

    setCurrentIndex(0);
}

void UIDetailsPagePrivate::createTextPage()
{
    if (m_pText)
        return;

    /* Create normal text page */
    m_pText = new QRichTextBrowser(this);
    m_pText->setFocusPolicy(Qt::StrongFocus);
    m_pText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    /* Make "transparent" */
    m_pText->setFrameShape(QFrame::NoFrame);
    m_pText->viewport()->setAutoFillBackground(false);
    m_pText->setOpenLinks(false);

    connect(m_pText, SIGNAL(anchorClicked(const QUrl &)),
            this, SLOT(gotLinkClicked(const QUrl &)));

    addWidget(m_pText);
}

void UIDetailsPagePrivate::createErrPage()
{
    if (m_pErrBox)
        return;

    /* Create inaccessible details page */
    m_pErrBox = new QWidget();

    QVBoxLayout *vLayout = new QVBoxLayout(m_pErrBox);
    vLayout->setSpacing(10);
    vLayout->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);

    m_pErrLabel = new QLabel(m_pErrBox);
    m_pErrLabel->setWordWrap(true);
    m_pErrLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    vLayout->addWidget(m_pErrLabel);

    m_pErrText = new QTextBrowser(m_pErrBox);
    m_pErrText->setFocusPolicy(Qt::StrongFocus);
    m_pErrText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    vLayout->addWidget(m_pErrText);

    if (mRefreshAction)
    {
        mRefreshButton = new QToolButton(m_pErrBox);
        mRefreshButton->setFocusPolicy(Qt::StrongFocus);

        QHBoxLayout *hLayout = new QHBoxLayout();
        vLayout->addLayout(hLayout);
        hLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
                                                 QSizePolicy::Minimum));
        hLayout->addWidget(mRefreshButton);

        connect(mRefreshButton, SIGNAL(clicked()),
                mRefreshAction, SIGNAL(triggered()));
    }

    vLayout->addItem(new QSpacerItem(0, 0,
                                     QSizePolicy::Minimum,
                                     QSizePolicy::Expanding));

    addWidget(m_pErrBox);

    retranslateUi();
}

/**
 *  Return a text summary of the properties of a generic network adapter
 */
/* static */
QString UIDetailsPagePrivate::summarizeGenericProperties(const CNetworkAdapter &adapter)
{
    QVector<QString> names;
    QVector<QString> props;
    props = adapter.GetProperties(QString(), names);
    QString strResult;
    for (int i = 0; i < names.size(); ++i)
    {
        strResult += names[i] + "=" + props[i];
        if (i < names.size() - 1)
            strResult += ", ";
    }
    return strResult;
}

/**
 *  UIDescriptionPagePrivate
 */
class UIDescriptionPagePrivate : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIDescriptionPagePrivate(QWidget *pParent = 0);
    ~UIDescriptionPagePrivate() {}

    void setMachineItem(UIVMItem *pVMItem);

    void updateState();

signals:

    void linkClicked(const QString &aURL);

protected:

    void retranslateUi();

private slots:

    void goToSettings();

private:

    UIVMItem *m_pVMItem;

    QToolButton *m_pEditBtn;
    QTextBrowser *m_pBrowser;
    QLabel *m_pLabel;
};

UIDescriptionPagePrivate::UIDescriptionPagePrivate(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pVMItem(0)
    , m_pEditBtn(0)
    , m_pBrowser(0)
    , m_pLabel(0)
{
    /* Main layout */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setSpacing(10);
    VBoxGlobal::setLayoutMargin(pMainLayout, 0);

    /* m_pBrowser */
    m_pBrowser = new QTextBrowser(this);
    m_pBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pBrowser->setFocusPolicy(Qt::StrongFocus);
    m_pBrowser->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    pMainLayout->addWidget(m_pBrowser);
    /* hidden by default */
    m_pBrowser->setHidden(true);

    m_pLabel = new QLabel(this);
    m_pLabel->setFrameStyle(m_pBrowser->frameStyle());
    m_pLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pLabel->setAlignment(Qt::AlignCenter);
    m_pLabel->setWordWrap(true);
    pMainLayout->addWidget(m_pLabel);
    /* always disabled */
    m_pLabel->setEnabled(false);

    /* button layout */
    QHBoxLayout *hBtnLayout = new QHBoxLayout();
    pMainLayout->addLayout(hBtnLayout);
    hBtnLayout->setSpacing(10);
    hBtnLayout->addItem(new QSpacerItem(0, 0,
                                        QSizePolicy::Expanding,
                                        QSizePolicy::Minimum));

    /* button */
    m_pEditBtn = new QToolButton(this);
    m_pEditBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_pEditBtn->setFocusPolicy(Qt::StrongFocus);
    m_pEditBtn->setIcon(UIIconPool::iconSet(":/edit_description_16px.png",
                                            ":/edit_description_disabled_16px.png"));
    m_pEditBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_pEditBtn, SIGNAL(clicked()), this, SLOT(goToSettings()));
    hBtnLayout->addWidget(m_pEditBtn);

    pMainLayout->addItem(new QSpacerItem(0, 0,
                                         QSizePolicy::Expanding,
                                         QSizePolicy::Minimum));

    /* apply language settings */
    retranslateUi();

    updateState();
}

/**
 * The machine list @a pVMItem is used to access cached machine data w/o making
 * unnecessary RPC calls.
 */
void UIDescriptionPagePrivate::setMachineItem(UIVMItem *pVMItem)
{
    m_pVMItem = pVMItem;

    QString text = pVMItem ? pVMItem->machine().GetDescription() : QString::null;

    if (!text.isEmpty())
    {
        m_pLabel->setHidden(true);
        m_pBrowser->setText(text);
        m_pBrowser->setVisible(true);
    }
    else
    {
        m_pBrowser->setHidden(true);
        m_pBrowser->clear();
        m_pLabel->setVisible(true);
    }

    /* check initial machine and session states */
    updateState();
}

void UIDescriptionPagePrivate::retranslateUi()
{
    m_pLabel->setText(tr("No description. Press the Edit button below to add it."));

    m_pEditBtn->setText(tr("Edit"));
    m_pEditBtn->setShortcut(QKeySequence("Ctrl+E"));
    m_pEditBtn->setToolTip(tr("Edit (Ctrl+E)"));
    m_pEditBtn->adjustSize();
    m_pEditBtn->updateGeometry();
}

/**
 * Called by the parent from machineStateChanged() and sessionStateChanged()
 * signal handlers. We cannot connect to these signals ourselves because we
 * use the UIVMListBoxItem which needs to be properly updated by the parent
 * first.
 */
void UIDescriptionPagePrivate::updateState()
{
    /// @todo disabling the edit button for a saved VM will not be necessary
    /// when we implement the selective VM Settings dialog, where only fields
    /// that can be changed in the saved state, can be changed.

    if (m_pVMItem)
    {
        bool saved = m_pVMItem->machineState() == KMachineState_Saved;
        bool busy = m_pVMItem->sessionState() != KSessionState_Unlocked;
        m_pEditBtn->setEnabled(!saved && !busy);
    }
    else
        m_pEditBtn->setEnabled(false);
}

void UIDescriptionPagePrivate::goToSettings()
{
    emit linkClicked("#general%%mTeDescription");
}

/**
 *  UIVMDesktop
 */
enum
{
    Dtls = 0,
    Snap,
//    Desc
};

UIVMDesktop::UIVMDesktop(UIToolBar *pToolBar, QAction *pRefreshAction, QWidget *pParent /* = 0 */)
  : QIWithRetranslateUI<QWidget>(pParent)
{
    m_pHeaderBtn = new UITexturedSegmentedButton(2);
    m_pHeaderBtn->setIcon(Dtls, UIIconPool::iconSet(":/settings_16px.png"));
    m_pHeaderBtn->setIcon(Snap, UIIconPool::iconSet(":/take_snapshot_16px.png",
                                                    ":/take_snapshot_dis_16px.png"));
//    m_pHeaderBtn->setIcon(Desc, UIIconPool::iconSet(":/description_16px.png",
//                                                    ":/description_disabled_16px.png"));
    m_pHeaderBtn->animateClick(0);

    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    /* The header to select the different pages. Has different styles on the
     * different platforms. */
#ifdef DARWIN_USE_TOOLBAR
    if (pToolBar)
    {
        pToolBar->addWidget(new UIHorizontalSpacerWidget(this));
        pToolBar->addWidget(m_pHeaderBtn);
        QWidget *pSpace = new QWidget(this);
        /* We need a little bit more space for the beta label. */
        if (vboxGlobal().isBeta())
            pSpace->setFixedSize(28, 1);
        else
            pSpace->setFixedSize(10, 1);
        pToolBar->addWidget(pSpace);
        pToolBar->updateLayout();
    } else
#else /* DARWIN_USE_TOOLBAR */
        NOREF(pToolBar);
#endif /* !DARWIN_USE_TOOLBAR */
    {
        UIBar *pBar = new UIBar(this);
#ifndef Q_WS_MAC
//        pBar->setFixedHeight(65);
#endif /* !Q_WS_MAC */
        pBar->setContentWidget(m_pHeaderBtn);
        pMainLayout->addWidget(pBar);
    }

    /* Add the pages */
    QStackedLayout *pStack = new QStackedLayout(pMainLayout);

    m_pDetails = new UIDetailsPagePrivate(this, pRefreshAction);
    connect(m_pDetails, SIGNAL(linkClicked(const QString&)),
            this, SIGNAL(linkClicked(const QString&)));
    pStack->addWidget(m_pDetails);
    m_pSnapshotsPage = new VBoxSnapshotsWgt(this);
    m_pSnapshotsPage->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);
    pStack->addWidget(m_pSnapshotsPage);
//    m_pDescription = new UIDescriptionPagePrivate(this);
//    connect(m_pDescription, SIGNAL(linkClicked(const QString&)),
//            this, SIGNAL(linkClicked(const QString&)));
//    m_pDescription->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);
//    pStack->addWidget(m_pDescription);

    /* Connect the header buttons with the stack layout. */
    connect(m_pHeaderBtn, SIGNAL(clicked(int)),
            pStack, SLOT(setCurrentIndex(int)));

    retranslateUi();
}

void UIVMDesktop::retranslateUi()
{
    m_pHeaderBtn->setTitle(Dtls, tr("&Details"));
}

void UIVMDesktop::updateDetails(UIVMItem * /* pVMItem */, const CMachine& machine)
{
//    KMachineState state = pVMItem->state();
//    bool fRunning = pVMItem->sessionState() != KSessionState_Closed;
//    bool fModifyEnabled = !fRunning && state != KMachineState_Saved;
    m_pDetails->setMachine(machine);
}

void UIVMDesktop::updateDetailsText(const QString &strText)
{
    m_pDetails->setText(strText);
}

void UIVMDesktop::updateDetailsErrorText(const QString &strText)
{
    m_pDetails->setErrorText(strText);
}

void UIVMDesktop::updateSnapshots(UIVMItem *pVMItem, const CMachine& machine)
{
    /* Update the snapshots header name */
    QString name = tr("&Snapshots");
    if (pVMItem)
    {
        ULONG count = pVMItem->snapshotCount();
        if (count)
            name += QString(" (%1)").arg(count);
    }
    m_pHeaderBtn->setTitle(Snap, name);
    /* Refresh the snapshots widget */
    if (!machine.isNull())
    {
        m_pHeaderBtn->setEnabled(Snap, true);
        m_pSnapshotsPage->setMachine(machine);
    } else
    {
        m_pHeaderBtn->animateClick(Dtls);
        m_pHeaderBtn->setEnabled(Snap, false);
    }
}

//void UIVMDesktop::updateDescription(UIVMItem *pVMItem, const CMachine& machine)
//{
//    /* Update the description header name */
//    QString name = tr("D&escription");
//    if (pVMItem)
//    {
//         if(!machine.GetDescription().isEmpty())
//             name += " *";
//    }
//    m_pHeaderBtn->setTitle(Desc, name);
//    /* refresh the description widget */
//    if (!machine.isNull())
//    {
//        m_pHeaderBtn->setEnabled(Desc, true);
//        m_pDescription->setMachineItem(pVMItem);
//    } else
//    {
//        m_pHeaderBtn->animateClick(Dtls);
//        m_pHeaderBtn->setEnabled(Desc, false);
//    }
//}
//
//void UIVMDesktop::updateDescriptionState()
//{
//    m_pDescription->updateState();
//}

#include "UIVMDesktop.moc"

