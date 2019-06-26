/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumItem class implementation.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#include <QApplication>
#include <QDir>

/* GUI includes: */
#include "QIFileDialog.h"
#include "QIMessageBox.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMediumItem.h"
#include "UIMessageCenter.h"
#include "UICommon.h"

/* COM includes: */
#include "CMachine.h"
#include "CMediumAttachment.h"
#include "CStorageController.h"
#include "CMediumFormat.h"


/*********************************************************************************************************************************
*   Class UIMediumItem implementation.                                                                                           *
*********************************************************************************************************************************/

UIMediumItem::UIMediumItem(const UIMedium &guiMedium, QITreeWidget *pParent)
    : QITreeWidgetItem(pParent)
    , m_guiMedium(guiMedium)
{
    refresh();
}

UIMediumItem::UIMediumItem(const UIMedium &guiMedium, UIMediumItem *pParent)
    : QITreeWidgetItem(pParent)
    , m_guiMedium(guiMedium)
{
    refresh();
}

UIMediumItem::UIMediumItem(const UIMedium &guiMedium, QITreeWidgetItem *pParent)
    : QITreeWidgetItem(pParent)
    , m_guiMedium(guiMedium)
{
    refresh();
}

bool UIMediumItem::move()
{
    /* Open file-save dialog to choose location for current medium: */
    const QString strFileName = QIFileDialog::getSaveFileName(location(),
                                                              tr("Current extension (*.%1)")
                                                                 .arg(QFileInfo(location()).suffix()),
                                                              treeWidget(),
                                                              tr("Choose the location of this medium"),
                                                                 0, true, true);
    /* Negative if nothing changed: */
    if (strFileName.isNull())
        return false;

    /* Search for corresponding medium: */
    CMedium comMedium = medium().medium();

    /* Try to assign new medium location: */
    if (   comMedium.isOk()
        && strFileName != location())
    {
        /* Prepare move storage progress: */
        CProgress comProgress = comMedium.MoveTo(strFileName);

        /* Show error message if necessary: */
        if (!comMedium.isOk())
        {
            msgCenter().cannotMoveMediumStorage(comMedium, location(),
                                                strFileName, treeWidget());
            /* Negative if failed: */
            return false;
        }
        else
        {
            /* Show move storage progress: */
            msgCenter().showModalProgressDialog(comProgress, tr("Moving medium..."),
                                                ":/progress_media_move_90px.png", treeWidget());

            /* Show error message if necessary: */
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            {
                msgCenter().cannotMoveMediumStorage(comProgress, location(),
                                                    strFileName, treeWidget());
                /* Negative if failed: */
                return false;
            }
        }
    }

    /* Recache item: */
    refreshAll();

    /* Positive: */
    return true;
}

// bool UIMediumItem::copy()
// {
//     /* Show Clone VD wizard: */
//     // UISafePointerWizard pWizard = new UIWizardCloneVD(treeWidget(), medium().medium());
//     // pWizard->prepare();
//     // pWizard->exec();

//     // /* Delete if still exists: */
//     // if (pWizard)
//     //     delete pWizard;

//     // /* True by default: */
//     return true;
// }

bool UIMediumItem::release(bool fInduced /* = false */)
{
    /* Refresh medium and item: */
    m_guiMedium.refresh();
    refresh();

    /* Make sure medium was not released yet: */
    if (medium().curStateMachineIds().isEmpty())
        return true;

    /* Confirm release: */
    if (!msgCenter().confirmMediumRelease(medium(), fInduced, treeWidget()))
        return false;

    /* Release: */
    foreach (const QUuid &uMachineId, medium().curStateMachineIds())
        if (!releaseFrom(uMachineId))
            return false;

    /* True by default: */
    return true;
}

void UIMediumItem::refreshAll()
{
    m_guiMedium.blockAndQueryState();
    refresh();
}

void UIMediumItem::setMedium(const UIMedium &guiMedium)
{
    m_guiMedium = guiMedium;
    refresh();
}

bool UIMediumItem::operator<(const QTreeWidgetItem &other) const
{
    int iColumn = treeWidget()->sortColumn();
    ULONG64 uThisValue = uiCommon().parseSize(      text(iColumn));
    ULONG64 uThatValue = uiCommon().parseSize(other.text(iColumn));
    return uThisValue && uThatValue ? uThisValue < uThatValue : QTreeWidgetItem::operator<(other);
}

bool UIMediumItem::isMediumModifiable() const
{
    if (medium().isNull())
        return false;
    if (m_enmDeviceType == UIMediumDeviceType_DVD || m_enmDeviceType == UIMediumDeviceType_Floppy)
        return false;
    foreach (const QUuid &uMachineId, medium().curStateMachineIds())
    {
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uMachineId.toString());
        if (comMachine.isNull())
            continue;
        if (comMachine.GetState() != KMachineState_PoweredOff &&
            comMachine.GetState() != KMachineState_Aborted)
            return false;
    }
    return true;
}

bool UIMediumItem::isMediumAttachedTo(QUuid uId)
{
   if (medium().isNull())
        return false;
   return medium().curStateMachineIds().contains(uId);
}

bool UIMediumItem::changeMediumType(KMediumType enmOldType, KMediumType enmNewType)
{
    QList<AttachmentCache> attachmentCacheList;
    /* Cache the list of vms the medium is attached to. We will need this for the re-attachment: */
    foreach (const QUuid &uMachineId, medium().curStateMachineIds())
    {
        const CMachine &comMachine = uiCommon().virtualBox().FindMachine(uMachineId.toString());
        if (comMachine.isNull())
            continue;
        CMediumAttachmentVector attachments = comMachine.GetMediumAttachments();
        foreach (const CMediumAttachment &attachment, attachments)
        {
            const CMedium& comMedium = attachment.GetMedium();
            if (comMedium.isNull() || comMedium.GetId() != id())
                continue;
            AttachmentCache attachmentCache;
            attachmentCache.m_uMachineId = uMachineId;
            attachmentCache.m_strControllerName = attachment.GetController();
            attachmentCache.m_port = attachment.GetPort();
            attachmentCache.m_device = attachment.GetDevice();
            attachmentCacheList << attachmentCache;
        }
    }

    /* Detach the medium from all the vms it is already attached to: */
    if (!release(true))
        return false;

    /* Search for corresponding medium: */
    CMedium comMedium = uiCommon().medium(id()).medium();

    /* Attempt to change medium type: */
    comMedium.SetType(enmNewType);
    bool fSuccess = true;
    /* Show error message if necessary: */
    if (!comMedium.isOk() && parentTree())
    {
        msgCenter().cannotChangeMediumType(comMedium, enmOldType, enmNewType, treeWidget());
        fSuccess = false;
    }
    /* Reattach the medium to all the vms it was previously attached: */
    foreach (const AttachmentCache &attachmentCache, attachmentCacheList)
        attachTo(attachmentCache);
    return fSuccess;
}

QString UIMediumItem::defaultText() const
{
    return tr("%1, %2: %3, %4: %5", "col.1 text, col.2 name: col.2 text, col.3 name: col.3 text")
              .arg(text(0))
              .arg(parentTree()->headerItem()->text(1)).arg(text(1))
              .arg(parentTree()->headerItem()->text(2)).arg(text(2));
}

void UIMediumItem::refresh()
{
    /* Fill-in columns: */
    setIcon(0, m_guiMedium.icon());
    setText(0, m_guiMedium.name());
    setText(1, m_guiMedium.logicalSize());
    setText(2, m_guiMedium.size());
    /* All columns get the same tooltip: */
    QString strToolTip = m_guiMedium.toolTip();
    for (int i = 0; i < treeWidget()->columnCount(); ++i)
        setToolTip(i, strToolTip);

    /* Gather medium data: */
    m_fValid =    !m_guiMedium.isNull()
               && m_guiMedium.state() != KMediumState_Inaccessible;
    m_enmDeviceType = m_guiMedium.type();
    m_enmVariant = m_guiMedium.mediumVariant();
    m_fHasChildren = m_guiMedium.hasChildren();
    /* Gather medium options data: */
    m_options.m_enmMediumType = m_guiMedium.mediumType();
    m_options.m_strLocation = m_guiMedium.location();
    m_options.m_uLogicalSize = m_guiMedium.logicalSizeInBytes();
    m_options.m_strDescription = m_guiMedium.description();
    /* Gather medium details data: */
    m_details.m_aFields.clear();
    switch (m_enmDeviceType)
    {
        case UIMediumDeviceType_HardDisk:
        {
            m_details.m_aLabels << tr("Format:");
            m_details.m_aLabels << tr("Storage details:");
            m_details.m_aLabels << tr("Attached to:");
            m_details.m_aLabels << tr("Encrypted with key:");
            m_details.m_aLabels << tr("UUID:");

            m_details.m_aFields << hardDiskFormat();
            m_details.m_aFields << details();
            m_details.m_aFields << (usage().isNull() ?
                                    formatFieldText(tr("<i>Not&nbsp;Attached</i>"), false) :
                                    formatFieldText(usage()));
            m_details.m_aFields << (encryptionPasswordID().isNull() ?
                                    formatFieldText(tr("<i>Not&nbsp;Encrypted</i>"), false) :
                                    formatFieldText(encryptionPasswordID()));
            m_details.m_aFields << id().toString();

            break;
        }
        case UIMediumDeviceType_DVD:
        case UIMediumDeviceType_Floppy:
        {
            m_details.m_aLabels << tr("Attached to:");
            m_details.m_aLabels << tr("UUID:");

            m_details.m_aFields << (usage().isNull() ?
                                    formatFieldText(tr("<i>Not&nbsp;Attached</i>"), false) :
                                    formatFieldText(usage()));
            m_details.m_aFields << id().toString();
            break;
        }
        default:
            break;
    }
}

bool UIMediumItem::releaseFrom(const QUuid &uMachineId)
{
    /* Open session: */
    CSession session = uiCommon().openSession(uMachineId);
    if (session.isNull())
        return false;

    /* Get machine: */
    CMachine machine = session.GetMachine();

    /* Prepare result: */
    bool fSuccess = false;

    /* Release medium from machine: */
    if (releaseFrom(machine))
    {
        /* Save machine settings: */
        machine.SaveSettings();
        if (!machine.isOk())
            msgCenter().cannotSaveMachineSettings(machine, treeWidget());
        else
            fSuccess = true;
    }

    /* Close session: */
    session.UnlockMachine();

    /* Return result: */
    return fSuccess;
}

bool UIMediumItem::attachTo(const AttachmentCache &attachmentCache)
{
    CMedium comMedium = medium().medium();

    if (comMedium.isNull())
        return false;

    /* Open session: */
    CSession session = uiCommon().openSession(attachmentCache.m_uMachineId);
    if (session.isNull())
        return false;

    /* Get machine: */
    CMachine machine = session.GetMachine();

    bool fSuccess = false;
    machine.AttachDevice(attachmentCache.m_strControllerName, attachmentCache.m_port,
                         attachmentCache.m_device, comMedium.GetDeviceType(), comMedium);

    if (machine.isOk())
    {
        machine.SaveSettings();
        if (!machine.isOk())
            msgCenter().cannotSaveMachineSettings(machine, treeWidget());
        else
            fSuccess = true;
    }

    /* Close session: */
    session.UnlockMachine();

    /* Return result: */
    return fSuccess;
}

/* static */
QString UIMediumItem::formatFieldText(const QString &strText, bool fCompact /* = true */,
                                      const QString &strElipsis /* = "middle" */)
{
    QString strCompactString = QString("<compact elipsis=\"%1\">").arg(strElipsis);
    QString strInfo = QString("<nobr>%1%2%3</nobr>")
                              .arg(fCompact ? strCompactString : "")
                              .arg(strText.isEmpty() ? tr("--", "no info") : strText)
                              .arg(fCompact ? "</compact>" : "");
    return strInfo;
}


/*********************************************************************************************************************************
*   Class UIMediumItemHD implementation.                                                                                         *
*********************************************************************************************************************************/

UIMediumItemHD::UIMediumItemHD(const UIMedium &guiMedium, QITreeWidget *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

UIMediumItemHD::UIMediumItemHD(const UIMedium &guiMedium, UIMediumItem *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

UIMediumItemHD::UIMediumItemHD(const UIMedium &guiMedium, QITreeWidgetItem *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

bool UIMediumItemHD::remove()
{
    /* Confirm medium removal: */
    if (!msgCenter().confirmMediumRemoval(medium(), treeWidget()))
        return false;

    /* Remember some of hard-disk attributes: */
    CMedium hardDisk = medium().medium();
#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    QUuid uMediumID = id();
#endif

    /* Propose to remove medium storage: */
    if (!maybeRemoveStorage())
        return false;

    /* Close hard-disk: */
    hardDisk.Close();
    if (!hardDisk.isOk())
    {
        msgCenter().cannotCloseMedium(medium(), hardDisk, treeWidget());
        return false;
    }

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    /* Remove UIMedium finally: */
    uiCommon().deleteMedium(uMediumID);
#endif

    /* True by default: */
    return true;
}

bool UIMediumItemHD::releaseFrom(CMachine comMachine)
{
    /* Enumerate attachments: */
    CMediumAttachmentVector attachments = comMachine.GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        /* Skip non-hard-disks: */
        if (attachment.GetType() != KDeviceType_HardDisk)
            continue;

        /* Skip unrelated hard-disks: */
        if (attachment.GetMedium().GetId() != id())
            continue;

        /* Remember controller: */
        CStorageController controller = comMachine.GetStorageControllerByName(attachment.GetController());

        /* Try to detach device: */
        comMachine.DetachDevice(attachment.GetController(), attachment.GetPort(), attachment.GetDevice());
        if (!comMachine.isOk())
        {
            /* Return failure: */
            msgCenter().cannotDetachDevice(comMachine, UIMediumDeviceType_HardDisk, location(),
                                           StorageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice()),
                                           treeWidget());
            return false;
        }

        /* Return success: */
        return true;
    }

    /* False by default: */
    return false;
}

bool UIMediumItemHD::maybeRemoveStorage()
{
    /* Remember some of hard-disk attributes: */
    CMedium hardDisk = medium().medium();
    QString strLocation = location();

    /* We don't want to try to delete inaccessible storage as it will most likely fail.
     * Note that UIMessageCenter::confirmMediumRemoval() is aware of that and
     * will give a corresponding hint. Therefore, once the code is changed below,
     * the hint should be re-checked for validity. */
    bool fDeleteStorage = false;
    qulonglong uCapability = 0;
    QVector<KMediumFormatCapabilities> capabilities = hardDisk.GetMediumFormat().GetCapabilities();
    foreach (KMediumFormatCapabilities capability, capabilities)
        uCapability |= capability;
    if (state() != KMediumState_Inaccessible && uCapability & KMediumFormatCapabilities_File)
    {
        int rc = msgCenter().confirmDeleteHardDiskStorage(strLocation, treeWidget());
        if (rc == AlertButton_Cancel)
            return false;
        fDeleteStorage = rc == AlertButton_Choice1;
    }

    /* If user wish to delete storage: */
    if (fDeleteStorage)
    {
        /* Prepare delete storage progress: */
        CProgress progress = hardDisk.DeleteStorage();
        if (!hardDisk.isOk())
        {
            msgCenter().cannotDeleteHardDiskStorage(hardDisk, strLocation, treeWidget());
            return false;
        }
        /* Show delete storage progress: */
        msgCenter().showModalProgressDialog(progress, UIMediumItem::tr("Removing medium..."),
                                            ":/progress_media_delete_90px.png", treeWidget());
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            msgCenter().cannotDeleteHardDiskStorage(progress, strLocation, treeWidget());
            return false;
        }
    }

    /* True by default: */
    return true;
}


/*********************************************************************************************************************************
*   Class UIMediumItemCD implementation.                                                                                         *
*********************************************************************************************************************************/

UIMediumItemCD::UIMediumItemCD(const UIMedium &guiMedium, QITreeWidget *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

UIMediumItemCD::UIMediumItemCD(const UIMedium &guiMedium, QITreeWidgetItem *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

bool UIMediumItemCD::remove()
{
    /* Confirm medium removal: */
    if (!msgCenter().confirmMediumRemoval(medium(), treeWidget()))
        return false;

    /* Remember some of optical-disk attributes: */
    CMedium image = medium().medium();
#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    QUuid uMediumID = id();
#endif

    /* Close optical-disk: */
    image.Close();
    if (!image.isOk())
    {
        msgCenter().cannotCloseMedium(medium(), image, treeWidget());
        return false;
    }

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    /* Remove UIMedium finally: */
    uiCommon().deleteMedium(uMediumID);
#endif

    /* True by default: */
    return true;
}

bool UIMediumItemCD::releaseFrom(CMachine comMachine)
{
    /* Enumerate attachments: */
    CMediumAttachmentVector attachments = comMachine.GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        /* Skip non-optical-disks: */
        if (attachment.GetType() != KDeviceType_DVD)
            continue;

        /* Skip unrelated optical-disks: */
        if (attachment.GetMedium().GetId() != id())
            continue;

        /* Try to unmount device: */
        comMachine.MountMedium(attachment.GetController(), attachment.GetPort(), attachment.GetDevice(), CMedium(), false /* force */);
        if (!comMachine.isOk())
        {
            /* Return failure: */
            msgCenter().cannotRemountMedium(comMachine, medium(), false /* mount? */, false /* retry? */, treeWidget());
            return false;
        }

        /* Return success: */
        return true;
    }

    /* Return failure: */
    return false;
}


/*********************************************************************************************************************************
*   Class UIMediumItemFD implementation.                                                                                         *
*********************************************************************************************************************************/

UIMediumItemFD::UIMediumItemFD(const UIMedium &guiMedium, QITreeWidget *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

UIMediumItemFD::UIMediumItemFD(const UIMedium &guiMedium, QITreeWidgetItem *pParent)
    : UIMediumItem(guiMedium, pParent)
{
}

bool UIMediumItemFD::remove()
{
    /* Confirm medium removal: */
    if (!msgCenter().confirmMediumRemoval(medium(), treeWidget()))
        return false;

    /* Remember some of floppy-disk attributes: */
    CMedium image = medium().medium();
#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    QUuid uMediumID = id();
#endif

    /* Close floppy-disk: */
    image.Close();
    if (!image.isOk())
    {
        msgCenter().cannotCloseMedium(medium(), image, treeWidget());
        return false;
    }

#ifndef VBOX_GUI_WITH_NEW_MEDIA_EVENTS
    /* Remove UIMedium finally: */
    uiCommon().deleteMedium(uMediumID);
#endif

    /* True by default: */
    return true;
}

bool UIMediumItemFD::releaseFrom(CMachine comMachine)
{
    /* Enumerate attachments: */
    CMediumAttachmentVector attachments = comMachine.GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        /* Skip non-floppy-disks: */
        if (attachment.GetType() != KDeviceType_Floppy)
            continue;

        /* Skip unrelated floppy-disks: */
        if (attachment.GetMedium().GetId() != id())
            continue;

        /* Try to unmount device: */
        comMachine.MountMedium(attachment.GetController(), attachment.GetPort(), attachment.GetDevice(), CMedium(), false /* force */);
        if (!comMachine.isOk())
        {
            /* Return failure: */
            msgCenter().cannotRemountMedium(comMachine, medium(), false /* mount? */, false /* retry? */, treeWidget());
            return false;
        }

        /* Return success: */
        return true;
    }

    /* Return failure: */
    return false;
}
