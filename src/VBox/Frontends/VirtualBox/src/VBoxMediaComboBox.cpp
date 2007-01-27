/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMediaComboBox class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "VBoxMediaComboBox.h"
#include "VBoxDiskImageManagerDlg.h"
#include "VBoxGlobal.h"

#include <qfileinfo.h>
#include <qimage.h>
#include <qdir.h>
#include <qfile.h>
#include <qtooltip.h>
#include <qlistbox.h>

VBoxMediaComboBox::VBoxMediaComboBox (QWidget *aParent, const char *aName, int aType)
    : QComboBox (aParent , aName),
    mType (aType), mRequiredId (QUuid()), mUseEmptyItem (false), mToBeRefreshed (false)
{
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    /* Media shortcuts creating */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &)),
             this, SLOT (mediaEnumerated (const VBoxMedia &)));
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMediaList &)),
             this, SLOT (listEnumerated (const VBoxMediaList &)));
    connect (this, SIGNAL (activated (int)),
             this, SLOT (updateToolTip (int)));
    connect (listBox(), SIGNAL (onItem (QListBoxItem*)),
             this, SLOT (processOnItem (QListBoxItem*)));
}

void VBoxMediaComboBox::loadCleanContent()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    switch (mType)
    {
        /* load hd list */
        case VBoxDefs::HD:
        {
            CHardDiskEnumerator en = vbox.GetHardDisks().Enumerate();
            while (en.HasMore())
            {
                QUuid mediaId;
                QString toolTip;
                CHardDisk hd = en.GetNext();
                QString src = hd.GetLocation();
                QUuid machineId = hd.GetMachineId();
                /* append list only with free hd */
                if (machineId.isNull() || machineId == mMachineId)
                {
                    mediaId = hd.GetId();
                    toolTip = VBoxDiskImageManagerDlg::composeHdToolTip (hd, VBoxMedia::Unknown);
                }
                if (!mediaId.isNull()) updateMedia (src, mediaId, toolTip, VBoxMedia::Unknown);
            }
            break;
        }
        /* load cd list */
        case VBoxDefs::CD:
        {
            CDVDImageEnumerator en = vbox.GetDVDImages().Enumerate();
            while (en.HasMore())
            {
                QUuid mediaId;
                QString toolTip;
                CDVDImage cd = en.GetNext();
                QString src = cd.GetFilePath();
                mediaId = cd.GetId();
                toolTip = VBoxDiskImageManagerDlg::composeCdToolTip (cd, VBoxMedia::Unknown);
                updateMedia (src, mediaId, toolTip, VBoxMedia::Unknown);
            }
            break;
        }
        /* load fd list */
        case VBoxDefs::FD:
        {
            CFloppyImageEnumerator en = vbox.GetFloppyImages().Enumerate();
            while (en.HasMore())
            {
                QUuid mediaId;
                QString toolTip;
                CFloppyImage fd = en.GetNext();
                QString src = fd.GetFilePath();
                mediaId = fd.GetId();
                toolTip = VBoxDiskImageManagerDlg::composeFdToolTip (fd, VBoxMedia::Unknown);
                updateMedia (src, mediaId, toolTip, VBoxMedia::Unknown);
            }
            break;
        }
        default:
            AssertFailed();
    }
}

void VBoxMediaComboBox::mediaEnumerated (const VBoxMedia &aMedia)
{
    if (!mToBeRefreshed) return;
    if (!(aMedia.type & mType))
        return;

    QString src;
    QUuid   mediaId;
    QString toolTip;

    switch (aMedia.type)
    {
        case VBoxDefs::HD:
        {
            CHardDisk hd = aMedia.disk;
            src = hd.GetLocation();
            QUuid machineId = hd.GetMachineId();
            /* append list only with free hd */
            if (machineId.isNull() || machineId == mMachineId)
            {
                mediaId = hd.GetId();
                toolTip = VBoxDiskImageManagerDlg::composeHdToolTip (hd, aMedia.status);
            }
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage dvd = aMedia.disk;
            src = dvd.GetFilePath();
            mediaId = dvd.GetId();
            toolTip = VBoxDiskImageManagerDlg::composeCdToolTip (dvd, aMedia.status);
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage floppy = aMedia.disk;
            src = floppy.GetFilePath();
            mediaId = floppy.GetId();
            toolTip = VBoxDiskImageManagerDlg::composeFdToolTip (floppy, aMedia.status);
            break;
        }
        default:
            AssertFailed();
    }
    int updatedMedia = -1;
    if (!mediaId.isNull())
        updatedMedia = updateMedia (src, mediaId, toolTip, aMedia.status);
    if (updatedMedia == -1) return;

    /* update warning/error icons */
    /// @todo (r=dmik) cache pixmaps as class members
    QPixmap pixmap;
    QImage img;
    if (aMedia.status == VBoxMedia::Inaccessible)
        img = QMessageBox::standardIcon (QMessageBox::Warning).convertToImage();
    else if (aMedia.status == VBoxMedia::Error)
        img = QMessageBox::standardIcon (QMessageBox::Critical).convertToImage();
    if (!img.isNull())
    {
        img = img.smoothScale (14, 14);
        pixmap.convertFromImage (img);
    }
    if (!pixmap.isNull())
        changeItem (pixmap, text (updatedMedia), updatedMedia); 
}

int VBoxMediaComboBox::updateMedia (const QString &aSrc,
                                    const QUuid   &aId,
                                    const QString &aTip,
                                    VBoxMedia::Status /* aStatus */)
{
    /* search & update media */
    QFileInfo fi (aSrc);
    int index = mUuidList.findIndex (aId);
    QString name = QString ("%1 (%2)").arg (fi.fileName())
                   .arg (QDir::convertSeparators (fi.dirPath()));
    index == -1 ? appendItem (name, aId, aTip) : replaceItem (index, name, aTip);
    if (aId == mRequiredId)
    {
        int activatedItem = index == -1 ? count() - 1 : index;
        setCurrentItem (activatedItem);
        emit activated (activatedItem);
    }
    else
        updateToolTip (currentItem());
    return index == -1 ? count() - 1 : index;
}

void VBoxMediaComboBox::listEnumerated (const VBoxMediaList & /*aList*/)
{
    emit activated (currentItem());
    mToBeRefreshed = false;
}

void VBoxMediaComboBox::updateToolTip (int aItem)
{
    /* combobox tooltip attaching */
    QToolTip::remove (this);
    QToolTip::add (this, mTipList [aItem]);
}

void VBoxMediaComboBox::processOnItem (QListBoxItem* aItem)
{
    /* combobox item's tooltip attaching */
    int index = listBox()->index (aItem);
    QToolTip::remove (listBox()->viewport());
    QToolTip::add (listBox()->viewport(), mTipList [index]);
}

void VBoxMediaComboBox::setReadyForRefresh()
{
    mToBeRefreshed = true;
    /* clearing media combobox */
    clear(), mUuidList.clear(), mTipList.clear();
    /* prepend empty item if used */
    if (mUseEmptyItem)
        appendItem (tr ("<no hard disk>"), QUuid(), tr ("No hard disk"));
    /* load all non-enumerated shortcuts */
    if (!vboxGlobal().isInEnumeratingProcess()) loadCleanContent();
    /* update already enumerated shortcuts */
    VBoxMediaList list = vboxGlobal().currentMediaList();
    for (VBoxMediaList::const_iterator it = list.begin(); it != list.end(); ++ it)
        mediaEnumerated (*it);
}

void VBoxMediaComboBox::refresh()
{
    if (mToBeRefreshed) return;
    setReadyForRefresh();
    vboxGlobal().startEnumeratingMedia();
}

QUuid VBoxMediaComboBox::getId()
{
    return mUuidList.isEmpty() ? QUuid() : QUuid (mUuidList [currentItem()]);
}

void VBoxMediaComboBox::appendItem (const QString &aName,
                                    const QUuid   &aId,
                                    const QString &aTip)
{
    insertItem (aName);
    mUuidList << aId;
    mTipList  << aTip;
}

void VBoxMediaComboBox::replaceItem (int aNumber,
                                     const QString &aName,
                                     const QString &aTip)
{
    changeItem (aName, aNumber);
    mTipList [aNumber] = aTip;
}

void VBoxMediaComboBox::removeLastItem()
{
    if (count())
        removeItem (count() - 1);
    mUuidList.pop_back();
    mTipList.pop_back();
}

void VBoxMediaComboBox::setRequiredItem (const QUuid &aId)
{
    mRequiredId = aId;
}

void VBoxMediaComboBox::setUseEmptyItem (bool aUseEmptyItem)
{
    mUseEmptyItem = aUseEmptyItem;
}

void VBoxMediaComboBox::setBelongsTo (const QUuid &aMachineId)
{
    mMachineId = aMachineId;
}

QUuid VBoxMediaComboBox::getBelongsTo()
{
    return mMachineId;
}

void VBoxMediaComboBox::setCurrentItem (int aIndex)
{
    QComboBox::setCurrentItem (aIndex);
    updateToolTip (aIndex);
}
