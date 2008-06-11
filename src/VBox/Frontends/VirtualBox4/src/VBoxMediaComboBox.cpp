/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMediaComboBox class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxMediaComboBox.h"
#include "VBoxDiskImageManagerDlg.h"

#include <QFileInfo>
#include <QDir>
#include <QPixmap>

VBoxMediaComboBox::VBoxMediaComboBox (QWidget *aParent, int aType /* = -1 */,
                                      bool aUseEmptyItem /* = false */)
    : QComboBox (aParent)
    , mType (aType), mRequiredId (QUuid()), mUseEmptyItem (aUseEmptyItem)
{
    init();
}

void VBoxMediaComboBox::init()
{
    /* Setup default size policy */
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);

    /* Setup enumeration handlers */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &, int)),
             this, SLOT (mediaEnumerated (const VBoxMedia &, int)));

    /* Setup update handlers */
    connect (&vboxGlobal(), SIGNAL (mediaAdded (const VBoxMedia &)),
             this, SLOT (mediaAdded (const VBoxMedia &)));
    connect (&vboxGlobal(), SIGNAL (mediaUpdated (const VBoxMedia &)),
             this, SLOT (mediaUpdated (const VBoxMedia &)));
    connect (&vboxGlobal(), SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
             this, SLOT (mediaRemoved (VBoxDefs::DiskType, const QUuid &)));

    /* Setup connections */
    connect (this, SIGNAL (activated (int)),
             this, SLOT (processActivated (int)));

    /* In some qt themes embedded list-box is not used by default, so create it */
    // @todo (dsen): check it for qt4
    // if (!listBox())
    //     setListBox (new Q3ListBox (this));
    if (view())
        connect (view(), SIGNAL (entered (const QModelIndex&)),
                 this, SLOT (processOnItem (const QModelIndex&)));

    /* cache pixmaps as class members */
    QIcon icon;
    icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this);
    if (!icon.isNull())
        mPmInacc = icon.pixmap (14, 14);
    icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxCritical, this);
    if (!icon.isNull())
        mPmError = icon.pixmap (14, 14);
}

QString VBoxMediaComboBox::fullItemName (const QString &aSrc)
{
    /* Compose item's name */
    QFileInfo fi (aSrc);
    return QString ("%1 (%2)").arg (fi.fileName())
               .arg (QDir::convertSeparators (fi.absolutePath()));
}

void VBoxMediaComboBox::refresh()
{
    /* Clearing lists */
    clear(), mUuidList.clear(), mTipList.clear();
    /* Prepend empty item if used */
    if (mUseEmptyItem)
        appendItem (tr ("<no hard disk>"), QUuid(), tr ("No hard disk"), 0);
    /* Load current media list */
    VBoxMediaList list = vboxGlobal().currentMediaList();
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediaEnumerated (*it, 0);
    /* Activate item selected during current list loading */
    processActivated (currentIndex());
}


void VBoxMediaComboBox::mediaEnumStarted()
{
    refresh();
}

void VBoxMediaComboBox::mediaEnumerated (const VBoxMedia &aMedia, int /*aIndex*/)
{
    processMedia (aMedia);
}


void VBoxMediaComboBox::mediaAdded (const VBoxMedia &aMedia)
{
    processMedia (aMedia);
}

void VBoxMediaComboBox::mediaUpdated (const VBoxMedia &aMedia)
{
    processMedia (aMedia);
}

void VBoxMediaComboBox::mediaRemoved (VBoxDefs::DiskType aType,
                                      const QUuid &aId)
{
    if (mType == -1 || !(aType & mType))
        return;

    /* Search & remove media */
    int index = mUuidList.indexOf (aId);
    if (index != -1)
    {
        removeItem (index);
        mUuidList.removeAt (index);
        mTipList.removeAt (index);
        /* emit signal to ensure parent dialog process selection changes */
        emit activated (currentIndex());
    }
}


void VBoxMediaComboBox::processMedia (const VBoxMedia &aMedia)
{
    if (mType == -1 || !(aMedia.type & mType))
        return;

    switch (aMedia.type)
    {
        case VBoxDefs::HD:
        {
            /* Ignoring non-root disks */
            CHardDisk hd = aMedia.disk;
            if (hd.GetParent().isNull())
                processHdMedia (aMedia);
            break;
        }
        case VBoxDefs::CD:
            processCdMedia (aMedia);
            break;
        case VBoxDefs::FD:
            processFdMedia (aMedia);
            break;
        default:
            AssertFailed();
    }
}

void VBoxMediaComboBox::processHdMedia (const VBoxMedia &aMedia)
{
    QUuid mediaId;
    QString toolTip;
    CHardDisk hd = aMedia.disk;
    QString src = hd.GetLocation();
    QUuid machineId = hd.GetMachineId();
    /* Append list only with free hd */
    if (machineId.isNull() || machineId == mMachineId)
    {
        mediaId = hd.GetId();
        toolTip = VBoxDiskImageManagerDlg::composeHdToolTip (hd, aMedia.status);
    }
    if (!mediaId.isNull())
        updateShortcut (src, mediaId, toolTip, aMedia.status);
}

void VBoxMediaComboBox::processCdMedia (const VBoxMedia &aMedia)
{
    CDVDImage dvd = aMedia.disk;
    QString src = dvd.GetFilePath();
    QUuid mediaId = dvd.GetId();
    QString toolTip = VBoxDiskImageManagerDlg::composeCdToolTip (dvd, aMedia.status);
    updateShortcut (src, mediaId, toolTip, aMedia.status);
}

void VBoxMediaComboBox::processFdMedia (const VBoxMedia &aMedia)
{
    CFloppyImage floppy = aMedia.disk;
    QString src = floppy.GetFilePath();
    QUuid mediaId = floppy.GetId();
    QString toolTip = VBoxDiskImageManagerDlg::composeFdToolTip (floppy, aMedia.status);
    updateShortcut (src, mediaId, toolTip, aMedia.status);
}

void VBoxMediaComboBox::updateShortcut (const QString &aSrc,
                                        const QUuid &aId,
                                        const QString &aTip,
                                        VBoxMedia::Status aStatus)
{
    /* Compose item's name */
    QString name = fullItemName (aSrc);
    /* Update warning/error icons */
    QPixmap *pixmap = 0;
    if (aStatus == VBoxMedia::Inaccessible)
        pixmap = &mPmInacc;
    else if (aStatus == VBoxMedia::Error)
        pixmap = &mPmError;

    /* Search media */
    int index = mUuidList.indexOf (aId);
    /* Create or update media */
    if (index == -1)
        appendItem (name, aId, aTip, pixmap);
    else
        replaceItem (index, name, aTip, pixmap);

    /* Activate required item if it was updated */
    if (aId == mRequiredId)
        setCurrentItem (aId);
    /* Select last added item if there is no item selected */
    else if (currentText().isEmpty())
        QComboBox::setCurrentIndex (index == -1 ? count() - 1 : index);
}

void VBoxMediaComboBox::processActivated (int aItem)
{
    mRequiredId = mUuidList.isEmpty() || aItem < 0 ? QUuid() : QUuid (mUuidList [aItem]);
    updateToolTip (aItem);
}

void VBoxMediaComboBox::updateToolTip (int aItem)
{
    /* Combobox tooltip re-attaching */
    setToolTip (QString::null);
    if (!mTipList.isEmpty() && aItem >= 0)
        setToolTip (mTipList [aItem]);
}

void VBoxMediaComboBox::processOnItem (const QModelIndex &aIndex)
{
    /* Combobox item's tooltip attaching */
    int index = aIndex.row();
    view()->viewport()->setToolTip (QString::null);
    view()->viewport()->setToolTip (mTipList [index]);
}

QUuid VBoxMediaComboBox::getId (int aId) const
{
    return mUuidList.isEmpty() ? QUuid() :
           aId == -1 ? QUuid (mUuidList [currentIndex()]) :
           QUuid (mUuidList [aId]);
}

void VBoxMediaComboBox::appendItem (const QString &aName,
                                    const QUuid   &aId,
                                    const QString &aTip,
                                    QPixmap       *aPixmap)
{
    int currentInd = currentIndex();

    int insertPosition = count();
    for (int i = 0; i < count(); ++ i)
        /* Searching for the first real (non-null) vdi item
           which have name greater than the item to be inserted.
           This is necessary for sorting items alphabetically. */
        if (itemText (i).localeAwareCompare (aName) > 0 &&
            !getId (i).isNull())
        {
            insertPosition = i;
            break;
        }

    insertPosition == count() ? mUuidList.append (aId) :
        mUuidList.insert (insertPosition, aId);

    insertPosition == count() ? mTipList.append (aTip) :
        mTipList.insert (insertPosition, aTip);

    aPixmap ? insertItem (insertPosition, *aPixmap, aName) :
              insertItem (insertPosition, aName);

    if (insertPosition != count() && currentInd >= insertPosition)
        QComboBox::setCurrentIndex (currentInd + 1);
}

void VBoxMediaComboBox::replaceItem (int            aNumber,
                                     const QString &aName,
                                     const QString &aTip,
                                     QPixmap       *aPixmap)
{
    setItemText (aNumber, aName);
    if (aPixmap)
        setItemIcon (aNumber, *aPixmap);
    mTipList [aNumber] = aTip;
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

void VBoxMediaComboBox::setCurrentItem (const QUuid &aId)
{
    mRequiredId = aId;
    int index = mUuidList.indexOf (mRequiredId);
    if (index != -1)
    {
        QComboBox::setCurrentIndex (index);
        emit activated (index);
    }
}

void VBoxMediaComboBox::setType (int aType)
{
    mType = aType;
}

