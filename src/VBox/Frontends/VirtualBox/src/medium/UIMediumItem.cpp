/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumItem class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
# include <QApplication>
# include <QDir>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QIMessageBox.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIMediumItem.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CMachine.h"
# include "CMediumAttachment.h"
# include "CStorageController.h"
# include "CMediumFormat.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


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
                                                              QApplication::translate("UIMediumManager", "Current extension (*.%1)")
                                                              .arg(QFileInfo(location()).suffix()),
                                                              treeWidget(),
                                                              QApplication::translate("UIMediumManager", "Choose the location of this medium"),
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
            msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIMediumManager", "Moving medium..."),
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
    foreach (const QString &strMachineId, medium().curStateMachineIds())
        if (!releaseFrom(strMachineId))
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
    ULONG64 uThisValue = vboxGlobal().parseSize(      text(iColumn));
    ULONG64 uThatValue = vboxGlobal().parseSize(other.text(iColumn));
    return uThisValue && uThatValue ? uThisValue < uThatValue : QTreeWidgetItem::operator<(other);
}

QString UIMediumItem::defaultText() const
{
    return QApplication::translate("UIMediumManager", "%1, %2: %3, %4: %5", "col.1 text, col.2 name: col.2 text, col.3 name: col.3 text")
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
    m_enmType = m_guiMedium.type();
    m_enmVariant = m_guiMedium.mediumVariant();
    m_fHasChildren = m_guiMedium.hasChildren();
    /* Gather medium options data: */
    m_options.m_enmType = m_guiMedium.mediumType();
    m_options.m_strLocation = m_guiMedium.location();
    m_options.m_uLogicalSize = m_guiMedium.logicalSizeInBytes();
    m_options.m_strDescription = m_guiMedium.description();
    /* Gather medium details data: */
    m_details.m_aFields.clear();
    switch (m_enmType)
    {
        case UIMediumDeviceType_HardDisk:
        {
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "Format:");
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "Storage details:");
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "Attached to:");
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "Encrypted with key:");
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "UUID:");

            m_details.m_aFields << hardDiskFormat();
            m_details.m_aFields << details();
            m_details.m_aFields << (usage().isNull() ?
                                    formatFieldText(QApplication::translate("UIMediumManager", "<i>Not&nbsp;Attached</i>"), false) :
                                    formatFieldText(usage()));
            m_details.m_aFields << (encryptionPasswordID().isNull() ?
                                    formatFieldText(QApplication::translate("UIMediumManager", "<i>Not&nbsp;Encrypted</i>"), false) :
                                    formatFieldText(encryptionPasswordID()));
            m_details.m_aFields << id();

            break;
        }
        case UIMediumDeviceType_DVD:
        case UIMediumDeviceType_Floppy:
        {
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "Attached to:");
            m_details.m_aLabels << QApplication::translate("UIMediumManager", "UUID:");

            m_details.m_aFields << (usage().isNull() ?
                                    formatFieldText(QApplication::translate("UIMediumManager", "<i>Not&nbsp;Attached</i>"), false) :
                                    formatFieldText(usage()));
            m_details.m_aFields << id();
            break;
        }
        default:
            break;
    }
}

bool UIMediumItem::releaseFrom(const QString &strMachineId)
{
    /* Open session: */
    CSession session = vboxGlobal().openSession(strMachineId);
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

/* static */
QString UIMediumItem::formatFieldText(const QString &strText, bool fCompact /* = true */,
                                      const QString &strElipsis /* = "middle" */)
{
    QString strCompactString = QString("<compact elipsis=\"%1\">").arg(strElipsis);
    QString strInfo = QString("<nobr>%1%2%3</nobr>")
                              .arg(fCompact ? strCompactString : "")
                              .arg(strText.isEmpty() ? QApplication::translate("UIMediumManager", "--", "no info") : strText)
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
    QString strMediumID = id();

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

    /* Remove UIMedium finally: */
    vboxGlobal().deleteMedium(strMediumID);

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
        msgCenter().showModalProgressDialog(progress, QApplication::translate("UIMediumManager", "Removing medium..."),
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
    QString strMediumID = id();

    /* Close optical-disk: */
    image.Close();
    if (!image.isOk())
    {
        msgCenter().cannotCloseMedium(medium(), image, treeWidget());
        return false;
    }

    /* Remove UIMedium finally: */
    vboxGlobal().deleteMedium(strMediumID);

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
    QString strMediumID = id();

    /* Close floppy-disk: */
    image.Close();
    if (!image.isOk())
    {
        msgCenter().cannotCloseMedium(medium(), image, treeWidget());
        return false;
    }

    /* Remove UIMedium finally: */
    vboxGlobal().deleteMedium(strMediumID);

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
