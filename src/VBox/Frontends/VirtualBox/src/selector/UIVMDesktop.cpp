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

/* Global includes */
#include <QDir>
#include <QLabel>
#include <QScrollArea>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>

#include <iprt/assert.h>

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

/*
 * Todo:
 * - dynamically change size of preview window!
 */

//#ifdef Q_WS_MAC
# define DARWIN_USE_TOOLBAR
//#endif /* Q_WS_MAC */

static const QString sTableTpl = "<table border=0 cellspacing=1 cellpadding=0>%1</table>";
static const QString sSectionItemTpl1 = "<tr><td width=40%><nobr><i>%1</i></nobr></td><td/><td/></tr>";
static const QString sSectionItemTpl2 = "<tr><td><nobr>%1:</nobr></td><td/><td>%2</td></tr>";
static const QString sSectionItemTpl3 = "<tr><td width=40%><nobr>%1</nobr></td><td/><td/></tr>";
static const QString sSectionItemTpl4 = "<tr><td>%1</td><td/><td/></tr>";

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
        Section_General,
        Section_System,
        Section_Preview,
        Section_Display,
        Section_Storage,
        Section_Audio,
        Section_Network,
        Section_Serial,
#ifdef VBOX_WITH_PARALLEL_PORTS
        Section_Parallel,
#endif /* VBOX_WITH_PARALLEL_PORTS */
        Section_USB,
        Section_SharedFolders,
        Section_Description,
        Section_End
    };
    typedef QMap<Section, UIPopupBox*> UIDetailsBlock;
    typedef QVector<UIDetailsBlock> UIDetailsSet;

public:

    UIDetailsPagePrivate(QWidget *pParent, QAction *pRefreshAction = 0);
    ~UIDetailsPagePrivate();

    void setMachines(const QList<CMachine> &machines);
    void setText(const QString &aText);
    void setErrorText(const QString &aText);

signals:

    void linkClicked(const QString &url);

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

    void sltLinkClicked(const QUrl &url) { emit linkClicked(url.toString()); }

    void sltPopupToggled(bool fPopupOpened);

private:

    void prepareDetails();
    void cleanupDetails();

    void prepareTextPage();

    void prepareErrorPage();

    void prepareSet();
    void prepareBlock(int iBlockNumber);
    void prepareSection(UIDetailsBlock &block, int iBlockNumber, Section section);
    void updateSet();

    void saveSectionSetting();

    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);

    /* Common variables: */
    CVirtualBox m_vbox;
    int m_cMachineCount;
    QVector<CMachine> m_machines;
    QVector<bool> m_changeable;

    /* Details-view variables: */
    QScrollArea *m_pScrollArea;
    QWidget *m_pDetails;
    QMap<Section, QString> m_sectionSettings;
    QMap<Section, QString> m_sectionNames;
    QMap<Section, bool> m_sectionOpened;
    QMap<Section, QAction*> m_actions;
    QList<Section> m_sections;
    UIDetailsSet m_set;
    bool m_fUSBAvailable;

    /* Free text: */
    QRichTextBrowser *m_pText;

    /* Error text: */
    QWidget *m_pErrBox;
    QLabel *m_pErrLabel;
    QTextBrowser *m_pErrText;
    QToolButton *m_pRefreshButton;
    QAction *m_pRefreshAction;
};

UIDetailsPagePrivate::UIDetailsPagePrivate(QWidget *pParent, QAction *pRefreshAction /* = 0 */)
    : QIWithRetranslateUI<QStackedWidget>(pParent)
    , m_vbox(vboxGlobal().virtualBox())
    , m_cMachineCount(0)
    , m_pScrollArea(0)
    , m_pDetails(0)
    , m_fUSBAvailable(true)
    , m_pText(0)
    , m_pErrBox(0), m_pErrLabel(0), m_pErrText(0)
    , m_pRefreshButton(0), m_pRefreshAction(pRefreshAction)
{
    /* Check that refresh action was passed: */
    Assert(m_pRefreshAction);
}

UIDetailsPagePrivate::~UIDetailsPagePrivate()
{
    /* Cleanup details: */
    cleanupDetails();
}

void UIDetailsPagePrivate::setMachines(const QList<CMachine> &machines)
{
    /* Prepare variables: */
    bool fCountChanged = machines.size() != m_cMachineCount;
    m_cMachineCount = machines.size();
    /* Recreate corresponding vectors: */
    m_machines.clear();
    m_machines.resize(m_cMachineCount);
    m_changeable.clear();
    m_changeable.resize(m_cMachineCount);
    /* Fetch passed VMs: */
    for (int i = 0; i < machines.size(); ++i)
    {
        /* Get current VM: */
        const CMachine &machine = machines[i];
        /* Assign corresponding vector values: */
        m_machines[i] = machine;
        m_changeable[i] = machine.isNull() || !machine.GetAccessible() ? false :
                          machine.GetState() != KMachineState_Stuck &&
                          machine.GetState() != KMachineState_Saved /* for now! */;
    }

    /* Prepare machine details page if necessary: */
    prepareDetails();

    /* If count was changed: */
    if (fCountChanged)
    {
        /* Recreate set of blocks of sections: */
        prepareSet();
    }
    /* If count was NOT changed: */
    else
    {
        /* Update set of blocks of sections: */
        updateSet();
    }

    /* Select corresponding widget: */
    setCurrentIndex(indexOf(m_pScrollArea));
}

void UIDetailsPagePrivate::setText(const QString &aText)
{
    /* Clear machine maps: */
    m_machines.clear();
    m_machines.resize(0);
    m_changeable.clear();
    m_changeable.resize(0);

    /* Prepare text page if necessary: */
    prepareTextPage();

    /* Assign corresponding text: */
    m_pText->setText(aText);

    /* Select corresponding widget: */
    setCurrentIndex(indexOf(m_pText));
}

void UIDetailsPagePrivate::setErrorText(const QString &aText)
{
    /* Clear machine maps: */
    m_machines.clear();
    m_machines.resize(0);
    m_changeable.clear();
    m_changeable.resize(0);

    /* Prepare error page if necessary: */
    prepareErrorPage();

    /* Assign corresponding error: */
    m_pErrText->setText(aText);

    /* Select corresponding widget: */
    setCurrentIndex(indexOf(m_pErrBox));
}

void UIDetailsPagePrivate::retranslateUi()
{
    /* Translate context menu actions: */
    {
        if (m_actions.contains(Section_General))
            m_actions[Section_General]->setText(tr("General", "details report"));
        if (m_actions.contains(Section_System))
            m_actions[Section_System]->setText(tr("System", "details report"));
        if (m_actions.contains(Section_Preview))
            m_actions[Section_Preview]->setText(tr("Preview", "details report"));
        if (m_actions.contains(Section_Display))
            m_actions[Section_Display]->setText(tr("Display", "details report"));
        if (m_actions.contains(Section_Storage))
            m_actions[Section_Storage]->setText(tr("Storage", "details report"));
        if (m_actions.contains(Section_Audio))
            m_actions[Section_Audio]->setText(tr("Audio", "details report"));
        if (m_actions.contains(Section_Network))
            m_actions[Section_Network]->setText(tr("Network", "details report"));
        if (m_actions.contains(Section_Serial))
            m_actions[Section_Serial]->setText(tr("Serial Ports", "details report"));
#ifdef VBOX_WITH_PARALLEL_PORTS
        if (m_actions.contains(Section_Parallel))
            m_actions[Section_Parallel]->setText(tr("Parallel Ports", "details report"));
#endif /* VBOX_WITH_PARALLEL_PORTS */
        if (m_actions.contains(Section_USB))
            m_actions[Section_USB]->setText(tr("USB", "details report"));
        if (m_actions.contains(Section_SharedFolders))
            m_actions[Section_SharedFolders]->setText(tr("Shared Folders", "details report"));
        if (m_actions.contains(Section_Description))
            m_actions[Section_Description]->setText(tr("Description", "details report"));
    }

    /* Translate set of blocks of sections: */
    {
        /* For every existing block: */
        for (int i = 0; i < m_cMachineCount; ++i)
        {
            /* For every existing section: */
            for (int j = 0; j < m_sections.size(); ++j)
            {
                /* Assign corresponding action text as section title: */
                m_set[i][m_sections[j]]->setTitle(m_actions[m_sections[j]]->text());
            }
        }
    }

    /* Translate error-label text: */
    if (m_pErrLabel)
        m_pErrLabel->setText(tr("The selected virtual machine is <i>inaccessible</i>. "
                                "Please inspect the error message shown below and press the "
                                "<b>Refresh</b> button if you want to repeat the accessibility check:"));

    /* Translate refresh button & action text: */
    if (m_pRefreshAction && m_pRefreshButton)
    {
        m_pRefreshButton->setText(m_pRefreshAction->text());
        m_pRefreshButton->setIcon(m_pRefreshAction->icon());
        m_pRefreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
}

void UIDetailsPagePrivate::sltUpdateGeneral()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_General]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_General]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;
            if (machine.GetAccessible())
            {
                item = sSectionItemTpl2.arg(tr("Name", "details report"), machine.GetName())
                     + sSectionItemTpl2.arg(tr("OS Type", "details report"), vboxGlobal().vmGuestOSTypeDescription(machine.GetOSTypeId()));
            }
            else
            {
                item = QString(sSectionItemTpl1).arg(tr("Information inaccessible", "details report"));
            }
            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateSystem()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_System]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_System]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;
            if (machine.GetAccessible())
            {
                item = sSectionItemTpl2.arg(tr("Base Memory", "details report"), tr("<nobr>%1 MB</nobr>", "details report"))
                                       .arg(machine.GetMemorySize());

                int cCPU = machine.GetCPUCount();
                if (cCPU > 1)
                    item += sSectionItemTpl2.arg(tr("Processors", "details report"), tr("<nobr>%1</nobr>", "details report"))
                                            .arg(cCPU);

                int iCPUExecCap = machine.GetCPUExecutionCap();
                if (iCPUExecCap < 100)
                    item += sSectionItemTpl2.arg(tr("Execution Cap", "details report"), tr("<nobr>%1%</nobr>", "details report"))
                                            .arg(iCPUExecCap);

                /* Boot order: */
                QStringList bootOrder;
                for (ulong i = 1; i <= m_vbox.GetSystemProperties().GetMaxBootPosition(); ++i)
                {
                    KDeviceType device = machine.GetBootOrder(i);
                    if (device == KDeviceType_Null)
                        continue;
                    bootOrder << vboxGlobal().toString(device);
                }
                if (bootOrder.isEmpty())
                    bootOrder << vboxGlobal().toString(KDeviceType_Null);

                item += sSectionItemTpl2.arg(tr("Boot Order", "details report"), bootOrder.join(", "));

#ifdef VBOX_WITH_FULL_DETAILS_REPORT
                /* BIOS Settings holder: */
                const CBIOSSettings &biosSettings = machine.GetBIOSSettings();
                QStringList bios;

                /* ACPI: */
                if (biosSettings.GetACPIEnabled())
                    bios << tr("ACPI", "details report");

                /* IO APIC: */
                if (biosSettings.GetIOAPICEnabled())
                    bios << tr("IO APIC", "details report");

                if (!bios.isEmpty())
                    item += sSectionItemTpl2.arg(tr("BIOS", "details report"), bios.join(", "));
#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

                QStringList accel;
                if (m_vbox.GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx))
                {
                    /* VT-x/AMD-V: */
                    if (machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
                    {
                        accel << tr("VT-x/AMD-V", "details report");

                        /* Nested Paging (only when hw virt is enabled): */
                        if (machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                            accel << tr("Nested Paging", "details report");
                    }
                }

                /* PAE/NX: */
                if (machine.GetCPUProperty(KCPUPropertyType_PAE))
                    accel << tr("PAE/NX", "details report");

                if (!accel.isEmpty())
                    item += sSectionItemTpl2.arg(tr("Acceleration", "details report"), accel.join(", "));
            }
            else
            {
                item = QString(sSectionItemTpl1).arg(tr("Information inaccessible", "details report"));
            }
            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdatePreview()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    UIVMPreviewWindow *pPreview = qobject_cast<UIVMPreviewWindow*>(block[Section_Preview]->contentWidget());
    AssertMsg(pPreview, ("Content widget should be valid!"));

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        /* Update preview widget: */
        pPreview->setMachine(machine);
    }
}

void UIDetailsPagePrivate::sltUpdateDisplay()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Display]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Display]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            /* Video tab: */
            QString item = QString(sSectionItemTpl2).arg(tr("Video Memory", "details report"), tr("<nobr>%1 MB</nobr>", "details report"))
                                                    .arg(machine.GetVRAMSize());

            int cGuestScreens = machine.GetMonitorCount();
            if (cGuestScreens > 1)
                item += QString(sSectionItemTpl2).arg(tr("Screens", "details report")).arg(cGuestScreens);

            QStringList accel;
#ifdef VBOX_WITH_VIDEOHWACCEL
            if (machine.GetAccelerate2DVideoEnabled())
                accel << tr("2D Video", "details report");
#endif /* VBOX_WITH_VIDEOHWACCEL */
            if (machine.GetAccelerate3DEnabled())
                accel << tr("3D", "details report");

            if (!accel.isEmpty())
                item += sSectionItemTpl2.arg(tr("Acceleration", "details report"), accel.join(", "));

            /* VRDE tab: */
            CVRDEServer srv = machine.GetVRDEServer();
            if (!srv.isNull())
            {
                if (srv.GetEnabled())
                    item += QString(sSectionItemTpl2).arg(tr("Remote Desktop Server Port", "details report (VRDE Server)"))
                                                     .arg(srv.GetVRDEProperty("TCP/Ports"));
                else
                    item += QString(sSectionItemTpl2).arg(tr("Remote Desktop Server", "details report (VRDE Server)"))
                                                     .arg(tr("Disabled", "details report (VRDE Server)"));
            }

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateStorage()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Storage]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Storage]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;
            /* Iterate over the all machine controllers: */
            const CStorageControllerVector &controllers = machine.GetStorageControllers();
            for (int i = 0; i < controllers.size(); ++i)
            {
                /* Get current controller: */
                const CStorageController &controller = controllers[i];
                /* Add controller information: */
                item += QString(sSectionItemTpl3).arg(controller.GetName());

                /* Populate sorted map with attachments information: */
                QMap<StorageSlot,QString> attachmentsMap;
                const CMediumAttachmentVector &attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
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
                                                                                       attachment.GetDevice())) +
                                                     strDeviceType)
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
                item = QString(sSectionItemTpl1).arg(tr("Not Attached", "details report (Storage)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateAudio()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Audio]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Audio]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            const CAudioAdapter &audio = machine.GetAudioAdapter();
            if (audio.GetEnabled())
                item = QString(sSectionItemTpl2).arg(tr("Host Driver", "details report (audio)"),
                                                     vboxGlobal().toString(audio.GetAudioDriver())) +
                       QString(sSectionItemTpl2).arg(tr("Controller", "details report (audio)"),
                                                     vboxGlobal().toString(audio.GetAudioController()));
            else
                item = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (audio)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateNetwork()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Network]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Network]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3);
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CNetworkAdapter &adapter = machine.GetNetworkAdapter(slot);
                if (adapter.GetEnabled())
                {
                    KNetworkAttachmentType type = adapter.GetAttachmentType();
                    QString attType = vboxGlobal().toString(adapter.GetAdapterType())
                                      .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                    /* Don't use the adapter type string for types that have
                     * an additional symbolic network/interface name field,
                     * use this name instead: */
                    if (type == KNetworkAttachmentType_Bridged)
                        attType = attType.arg(tr("Bridged adapter, %1", "details report (network)").arg(adapter.GetBridgedInterface()));
                    else if (type == KNetworkAttachmentType_Internal)
                        attType = attType.arg(tr("Internal network, '%1'", "details report (network)").arg(adapter.GetInternalNetwork()));
                    else if (type == KNetworkAttachmentType_HostOnly)
                        attType = attType.arg(tr("Host-only adapter, '%1'", "details report (network)").arg(adapter.GetHostOnlyInterface()));
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

                    item += QString(sSectionItemTpl2).arg(tr("Adapter %1", "details report (network)").arg(adapter.GetSlot() + 1))
                                                     .arg(attType);
                }
            }
            if (item.isNull())
                item = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (network)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateSerialPorts()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Serial]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Serial]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetSerialPortCount();
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CSerialPort &port = machine.GetSerialPort(slot);
                if (port.GetEnabled())
                {
                    KPortMode mode = port.GetHostMode();
                    QString data = vboxGlobal().toCOMPortName(port.GetIRQ(), port.GetIOBase()) + ", ";
                    if (mode == KPortMode_HostPipe || mode == KPortMode_HostDevice || mode == KPortMode_RawFile)
                        data += QString("%1 (<nobr>%2</nobr>)").arg(vboxGlobal().toString(mode)).arg(QDir::toNativeSeparators(port.GetPath()));
                    else
                        data += vboxGlobal().toString(mode);

                    item += QString(sSectionItemTpl2).arg(tr("Port %1", "details report (serial ports)").arg(port.GetSlot() + 1))
                                                     .arg(data);
                }
            }
            if (item.isNull())
                item = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (serial ports)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

#ifdef VBOX_WITH_PARALLEL_PORTS
void UIDetailsPagePrivate::sltUpdateParallelPorts()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Parallel]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Parallel]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            ulong count = m_vbox.GetSystemProperties().GetParallelPortCount();
            for (ulong slot = 0; slot < count; slot ++)
            {
                const CParallelPort &port = machine.GetParallelPort(slot);
                if (port.GetEnabled())
                {
                    QString data = vboxGlobal().toLPTPortName(port.GetIRQ(), port.GetIOBase()) +
                                   QString(" (<nobr>%1</nobr>)").arg(QDir::toNativeSeparators(port.GetPath()));

                    item += QString(sSectionItemTpl2).arg(tr("Port %1", "details report (parallel ports)").arg(port.GetSlot() + 1))
                                                     .arg(data);
                }
            }
            if (item.isNull())
                item = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (parallel ports)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}
#endif /* VBOX_WITH_PARALLEL_PORTS */

void UIDetailsPagePrivate::sltUpdateUSB()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_USB]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_USB]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            const CUSBController &ctl = machine.GetUSBController();
            if (!ctl.isNull() && ctl.GetProxyAvailable())
            {
                m_fUSBAvailable = true;
                /* The USB controller may be unavailable (i.e. in VirtualBox OSE): */
                if (ctl.GetEnabled())
                {
                    const CUSBDeviceFilterVector &coll = ctl.GetDeviceFilters();
                    uint active = 0;
                    for (int i = 0; i < coll.size(); ++i)
                        if (coll[i].GetActive())
                            ++active;

                    item = QString(sSectionItemTpl2).arg(tr("Device Filters", "details report (USB)"),
                                                         tr("%1 (%2 active)", "details report (USB)").arg(coll.size()).arg(active));
                }
                else
                    item = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (USB)"));

                pLabel->setText(sTableTpl.arg(item));
            }
            else
            {
                m_fUSBAvailable = false;
                /* Fully hide when USB is not available: */
                block[Section_USB]->hide();
            }
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateSharedFolders()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_SharedFolders]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_SharedFolders]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;

            ulong count = machine.GetSharedFolders().size();
            if (count > 0)
            {
                item = QString(sSectionItemTpl2).arg(tr("Shared Folders", "details report (shared folders)")).arg(count);
            }
            else
                item = QString(sSectionItemTpl1).arg(tr("None", "details report (shared folders)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltUpdateDescription()
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get current block number: */
    int iBlockNumber = pSender->property("block-number").toInt();
    /* Get current machine: */
    CMachine &machine = m_machines[iBlockNumber];
    AssertMsg(!machine.isNull(), ("Machine should be valid!\n"));
    /* Get details block: */
    UIDetailsBlock &block = m_set[iBlockNumber];
    /* Get corresponding content widget: */
    QILabel *pLabel = qobject_cast<QILabel*>(block[Section_Description]->contentWidget());
    AssertMsg(pLabel, ("Content widget should be valid!"));

    /* Enable link: */
    block[Section_Description]->setTitleLinkEnabled(m_changeable[iBlockNumber]);

    /* Update if content widget is visible: */
    if (pSender->isOpen())
    {
        if (!machine.isNull())
        {
            QString item;
            const QString &strDesc = machine.GetDescription();
            if (!strDesc.isEmpty())
                item = QString(sSectionItemTpl4).arg(strDesc);
            else
                item = QString(sSectionItemTpl1).arg(tr("None", "details report (description)"));

            pLabel->setText(sTableTpl.arg(item));
        }
        else
            pLabel->clear();
    }
}

void UIDetailsPagePrivate::sltContextMenuRequested(const QPoint &pos)
{
    /* Populate list of allowed actions: */
    QList<QAction*> actions;
    for (int i = 0; i < m_sections.size(); ++i)
        actions << m_actions[m_sections[i]];
    /* Restrict USB action if USB is NOT available: */
    if (!m_fUSBAvailable)
        actions.removeOne(m_actions[Section_USB]);
    /* Popup menu to show/hide sections: */
    QAction *pReturn = QMenu::exec(actions, m_pDetails->mapToGlobal(pos), 0);
    /* If some action was toggled: */
    if (pReturn)
    {
        /* Get corresponding section type: */
        Section section = static_cast<Section>(pReturn->data().toInt());
        /* Enumerate all the available blocks: */
        for (int i = 0; i < m_cMachineCount; ++i)
        {
            /* Get current popup: */
            UIPopupBox *pPopup = m_set[i][section];
            /* Show/hide popup if necessary: */
            if (pReturn->isChecked())
                pPopup->show();
            else
                pPopup->hide();
        }
    }
}

void UIDetailsPagePrivate::sltPopupToggled(bool fPopupOpened)
{
    /* Get current sender: */
    UIPopupBox *pSender = sender() && sender()->inherits("UIPopupBox") ? qobject_cast<UIPopupBox*>(sender()) : 0;
    AssertMsg(pSender, ("Sender should be valid!\n"));
    /* Get section type: */
    Section section = static_cast<Section>(pSender->property("section-type").toInt());
    /* Update the state of corresponding map: */
    m_sectionOpened[section] = fPopupOpened;
    /* Open/Close all the blocks: */
    for (int i = 0; i < m_cMachineCount; ++i)
        m_set[i][section]->setOpen(fPopupOpened);
}

void UIDetailsPagePrivate::prepareSet()
{
    /* Which sections should be available: */
    m_sections.clear();
    if (m_cMachineCount == 1)
    {
        m_sections << Section_General
                   << Section_System
                   << Section_Preview
                   << Section_Display
                   << Section_Storage
                   << Section_Audio
                   << Section_Network
                   << Section_Serial
#ifdef VBOX_WITH_PARALLEL_PORTS
                   << Section_Parallel
#endif /* VBOX_WITH_PARALLEL_PORTS */
                   << Section_USB
                   << Section_SharedFolders
                   << Section_Description;
    }
    else
    {
        m_sections << Section_General
                   << Section_System
                   << Section_Preview;
    }

    /* Recreate details set: */
    m_set.clear();
    m_set.resize(m_cMachineCount);
    /* Re-create details widget: */
    if (m_pDetails)
        delete m_pDetails;
    m_pDetails = new QWidget(m_pScrollArea);
    m_pScrollArea->setWidget(m_pDetails);
    /* Configure the context-menu rules, which allows to show/hide the boxes: */
    m_pDetails->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pDetails, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltContextMenuRequested(const QPoint&)));
    /* Configure details widget layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(m_pDetails);
    pMainLayout->setContentsMargins(gsLeftMargin, gsTopMargin, gsRightMargin, gsBottomMargin);

    /* Prepare set content (blocks): */
    for (int i = 0; i < m_cMachineCount; ++i)
        prepareBlock(i);

    /* Layout set content: */
    pMainLayout->addStretch(1);

    retranslateUi();
}

void UIDetailsPagePrivate::prepareBlock(int iBlockNumber)
{
    /* Prepare new block: */
    UIDetailsBlock& block = m_set[iBlockNumber];

    /* Prepare block content (sections): */
    for(int i = 0; i < m_sections.size(); ++i)
        prepareSection(block, iBlockNumber, m_sections[i]);

    /* Layout block content: */
    QVBoxLayout *pMainLayout = qobject_cast<QVBoxLayout*>(m_pDetails->layout());
    QHBoxLayout *tt1 = new QHBoxLayout;
    QVBoxLayout *tt2 = new QVBoxLayout;
    if (m_sections.contains(Section_General))
        tt2->addWidget(block[Section_General]);
    if (m_sections.contains(Section_System))
        tt2->addWidget(block[Section_System]);
    tt2->addStretch(1);
    tt1->addLayout(tt2);
    QVBoxLayout *tt3 = new QVBoxLayout;
    if (m_sections.contains(Section_Preview))
        tt3->addWidget(block[Section_Preview]);
    tt3->addStretch(1);
    tt1->addLayout(tt3);
    pMainLayout->addLayout(tt1);
    if (m_sections.contains(Section_Display))
        pMainLayout->addWidget(block[Section_Display]);
    if (m_sections.contains(Section_Storage))
        pMainLayout->addWidget(block[Section_Storage]);
    if (m_sections.contains(Section_Audio))
        pMainLayout->addWidget(block[Section_Audio]);
    if (m_sections.contains(Section_Network))
        pMainLayout->addWidget(block[Section_Network]);
    if (m_sections.contains(Section_Serial))
        pMainLayout->addWidget(block[Section_Serial]);
#ifdef VBOX_WITH_PARALLEL_PORTS
    if (m_sections.contains(Section_Parallel))
        pMainLayout->addWidget(block[Section_Parallel]);
#endif /* VBOX_WITH_PARALLEL_PORTS */
    if (m_sections.contains(Section_USB))
        pMainLayout->addWidget(block[Section_USB]);
    if (m_sections.contains(Section_SharedFolders))
        pMainLayout->addWidget(block[Section_SharedFolders]);
    if (m_sections.contains(Section_Description))
        pMainLayout->addWidget(block[Section_Description]);
}

void UIDetailsPagePrivate::prepareSection(UIDetailsBlock &block, int iBlockNumber, Section section)
{
    /* Prepare new section (popup box): */
    UIPopupBox *pPopup = block[section] = new UIPopupBox(m_pDetails);
    connect(pPopup, SIGNAL(titleClicked(const QString &)), this, SIGNAL(linkClicked(const QString &)));
    connect(pPopup, SIGNAL(toggled(bool)), this, SLOT(sltPopupToggled(bool)));
    pPopup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    pPopup->setProperty("block-number", iBlockNumber);
    pPopup->setProperty("section-type", static_cast<int>(section));
    if (!m_machines[iBlockNumber].GetAccessible())
        pPopup->setWarningIcon(UIIconPool::iconSet(":/state_aborted_16px.png"));

    /* Configure the popup box: */
    switch (section)
    {
        case Section_General:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/machine_16px.png"));
            pPopup->setTitleLink("#general");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateGeneral()));
            break;
        }
        case Section_System:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/chipset_16px.png"));
            pPopup->setTitleLink("#system");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateSystem()));
            break;
        }
        case Section_Preview:
        {
            UIVMPreviewWindow *pWidget = new UIVMPreviewWindow(pPopup);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/machine_16px.png"));
            pPopup->setContentWidget(pWidget);
            pPopup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            /* Make sure the width is always the same, regardless if the preview is shown or not: */
            pPopup->setFixedWidth(pPopup->sizeHint().width());
            pWidget->updateGeometry();
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdatePreview()));
            break;
        }
        case Section_Display:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/vrdp_16px.png"));
            pPopup->setTitleLink("#display");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateDisplay()));
            break;
        }
        case Section_Storage:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/attachment_16px.png"));
            pPopup->setTitleLink("#storage");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateStorage()));
            break;
        }
        case Section_Audio:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/sound_16px.png"));
            pPopup->setTitleLink("#audio");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateAudio()));
            break;
        }
        case Section_Network:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/nw_16px.png"));
            pPopup->setTitleLink("#network");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateNetwork()));
            break;
        }
        case Section_Serial:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/serial_port_16px.png"));
            pPopup->setTitleLink("#serialPorts");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateSerialPorts()));
            break;
        }
#ifdef VBOX_WITH_PARALLEL_PORTS
        case Section_Parallel:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/parallel_port_16px.png"));
            pPopup->setTitleLink("#parallelPorts");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateParallelPorts()));
            break;
        }
#endif /* VBOX_WITH_PARALLEL_PORTS */
        case Section_USB:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/usb_16px.png"));
            pPopup->setTitleLink("#usb");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateUSB()));
            break;
        }
        case Section_SharedFolders:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/shared_folder_16px.png"));
            pPopup->setTitleLink("#sfolders");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateSharedFolders()));
            break;
        }
        case Section_Description:
        {
            QILabel *pLabel = new QILabel(pPopup);
            pLabel->setWordWrap(true);
            pPopup->setTitleIcon(UIIconPool::iconSet(":/description_16px.png"));
            pPopup->setTitleLink("#general%%mTeDescription");
            pPopup->setContentWidget(pLabel);
            connect(pPopup, SIGNAL(sigUpdateContentWidget()), this, SLOT(sltUpdateDescription()));
            break;
        }
        default:
            break;
    }

    /* Show/hide and open section: */
    if (m_actions[section]->isChecked())
        pPopup->show();
    else
        pPopup->hide();
    pPopup->setOpen(m_sectionOpened[section]);

    /* Call for update: */
    pPopup->callForUpdateContentWidget();
}

void UIDetailsPagePrivate::updateSet()
{
    /* For every block of set: */
    for (int i = 0; i < m_cMachineCount; ++i)
    {
        /* For every section of block: */
        for (int j = 0; j < m_sections.size(); ++j)
        {
            /* Call for update: */
            m_set[i][m_sections[j]]->callForUpdateContentWidget();
        }
    }
}

void UIDetailsPagePrivate::prepareDetails()
{
    if (m_pScrollArea)
        return;

    /* Prepare scroll area: */
    m_pScrollArea = new QScrollArea(this);
    m_pScrollArea->setFrameStyle(QFrame::NoFrame);
    m_pScrollArea->setWidgetResizable(true);
    addWidget(m_pScrollArea);

    /* Prepare section names: */
    m_sectionNames[Section_General]       = "general";
    m_sectionNames[Section_System]        = "system";
    m_sectionNames[Section_Preview]       = "preview";
    m_sectionNames[Section_Display]       = "display";
    m_sectionNames[Section_Storage]       = "storage";
    m_sectionNames[Section_Audio]         = "audio";
    m_sectionNames[Section_Network]       = "network";
    m_sectionNames[Section_Serial]        = "serialPorts";
#ifdef VBOX_WITH_PARALLEL_PORTS
    m_sectionNames[Section_Parallel]      = "parallelPorts";
#endif /* VBOX_WITH_PARALLEL_PORTS */
    m_sectionNames[Section_USB]           = "usb";
    m_sectionNames[Section_SharedFolders] = "sharedFolders";
    m_sectionNames[Section_Description]   = "description";

    /* Prepare context menu actions: */
    for (int i = 0; i < Section_End; ++i)
    {
        Section section = static_cast<Section>(i);
        m_actions[section] = new QAction(m_pScrollArea);
        QAction *pAction = m_actions[section];
        pAction->setData(i);
        pAction->setCheckable(true);
    }

    /* Load section configuration from extra data: */
    QStringList values = vboxGlobal().virtualBox().GetExtraDataStringList(VBoxDefs::GUI_DetailsPageBoxes,
                                                                          /* Default keys: */
                                                                          QStringList()
                                                                          << m_sectionNames.value(Section_General)
                                                                          << m_sectionNames.value(Section_System)
                                                                          << m_sectionNames.value(Section_Preview)
                                                                          << m_sectionNames.value(Section_Display)
                                                                          << m_sectionNames.value(Section_Storage)
                                                                          << m_sectionNames.value(Section_Audio)
                                                                          << m_sectionNames.value(Section_Network)
                                                                          << m_sectionNames.value(Section_USB)
                                                                          << m_sectionNames.value(Section_SharedFolders)
                                                                          << m_sectionNames.value(Section_Description));
    /* Parse loaded section configuration: */
    for (int i = 0; i < values.size(); ++i)
    {
        /* Get current section setting: */
        QString strSectionSetting = values[i];

        /* Is this section opened? */
        bool fSectionOpened = !strSectionSetting.endsWith("Closed");

        /* Get current section: */
        Section section = m_sectionNames.key(fSectionOpened ? strSectionSetting : strSectionSetting.remove("Closed"), Section_End);

        /* Assign values: */
        if (section != Section_End)
        {
            m_sectionSettings[section] = strSectionSetting;
            m_actions[section]->setChecked(true);
            m_sectionOpened[section] = fSectionOpened;
        }
    }
}

void UIDetailsPagePrivate::cleanupDetails()
{
    if (!m_pScrollArea)
        return;

    /* Update loaded section configuration with current values: */
    for (int i = 0; i < Section_End; ++i)
    {
        /* Get current section: */
        Section section = static_cast<Section>(i);

        /* Process only existing sections: */
        if (!m_sections.contains(section))
            continue;

        /* Compose section key to save: */
        QString strSectionSetting = !m_actions[section]->isChecked() ? QString() :
                                    m_sectionOpened[section] ? m_sectionNames[section] :
                                    m_sectionNames[section] + "Closed";

        /* Update corresponding setting: */
        m_sectionSettings[section] = strSectionSetting;
    }
    /* Save section configuration to extra data: */
    vboxGlobal().virtualBox().SetExtraDataStringList(VBoxDefs::GUI_DetailsPageBoxes, m_sectionSettings.values());
}

void UIDetailsPagePrivate::prepareTextPage()
{
    if (m_pText)
        return;

    /* Create normal text page: */
    m_pText = new QRichTextBrowser(this);
    m_pText->setFocusPolicy(Qt::StrongFocus);
    m_pText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    /* Make "transparent": */
    m_pText->setFrameShape(QFrame::NoFrame);
    m_pText->viewport()->setAutoFillBackground(false);
    m_pText->setOpenLinks(false);

    connect(m_pText, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(sltLinkClicked(const QUrl &)));

    addWidget(m_pText);

    retranslateUi();
}

void UIDetailsPagePrivate::prepareErrorPage()
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

    if (m_pRefreshAction)
    {
        m_pRefreshButton = new QToolButton(m_pErrBox);
        m_pRefreshButton->setFocusPolicy(Qt::StrongFocus);

        QHBoxLayout *hLayout = new QHBoxLayout();
        vLayout->addLayout(hLayout);
        hLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
        hLayout->addWidget(m_pRefreshButton);

        connect(m_pRefreshButton, SIGNAL(clicked()),
                m_pRefreshAction, SIGNAL(triggered()));
    }

    vLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

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

void UIVMDesktop::updateDetails(UIVMItem * /* pVMItem */, const QList<CMachine> &machines)
{
//    KMachineState state = pVMItem->state();
//    bool fRunning = pVMItem->sessionState() != KSessionState_Closed;
//    bool fModifyEnabled = !fRunning && state != KMachineState_Saved;
    m_pDetails->setMachines(machines);
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

