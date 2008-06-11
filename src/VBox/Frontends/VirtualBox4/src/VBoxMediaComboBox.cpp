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

/* static // compose item name */
QString VBoxMediaComboBox::fullItemName (const QString &aSrc)
{
    QFileInfo fi (aSrc);
    return QString ("%1 (%2)").arg (fi.fileName())
               .arg (QDir::convertSeparators (fi.absolutePath()));
}


VBoxMediaComboBox::VBoxMediaComboBox (QWidget *aParent, int aType /* = -1 */,
                                      bool aUseEmptyItem /* = false */)
    : QComboBox (aParent)
    , mType (aType), mUseEmptyItem (aUseEmptyItem)
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

    /* Setup other connections */
    connect (this, SIGNAL (activated (int)),
             this, SLOT (processActivated (int)));
    connect (this, SIGNAL (currentIndexChanged (int)),
             this, SLOT (processIndexChanged (int)));

    /* In some qt themes embedded list-box is not used by default, so create it */
    // @todo (dsen): check it for qt4
    // if (!listBox())
    //     setListBox (new Q3ListBox (this));
    if (view())
        connect (view(), SIGNAL (entered (const QModelIndex&)),
                 this, SLOT (processOnItem (const QModelIndex&)));

    /* Cache pixmaps as class members */
    QIcon icon;
    icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this);
    if (!icon.isNull())
        mPmInacc = icon.pixmap (14, 14);
    icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxCritical, this);
    if (!icon.isNull())
        mPmError = icon.pixmap (14, 14);
}

void VBoxMediaComboBox::refresh()
{
    /* Clearing lists */
    clear(), mUuidList.clear(), mTipList.clear();
    /* Prepend empty item if used */
    if (mUseEmptyItem)
        newItem (tr ("<no hard disk>"), QUuid(), tr ("No hard disk"), 0);
    /* Load current media list */
    VBoxMediaList list = vboxGlobal().currentMediaList();
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediaEnumerated (*it, 0);
    /* Activate item selected during current list loading */
    processIndexChanged (currentIndex());
}


void VBoxMediaComboBox::setUseEmptyItem (bool aUseEmptyItem)
{
    mUseEmptyItem = aUseEmptyItem;
}

void VBoxMediaComboBox::setBelongsTo (const QUuid &aMachineId)
{
    mMachineId = aMachineId;
}

void VBoxMediaComboBox::setCurrentItem (const QUuid &aId)
{
    mRequiredId = aId;
    int index = mUuidList.indexOf (mRequiredId);
    if (index != -1)
        setCurrentIndex (index);
}

void VBoxMediaComboBox::setType (int aType)
{
    mType = aType;
}


QUuid VBoxMediaComboBox::getId (int aId /* = -1 */) const
{
    return aId == -1 && currentIndex() >= 0 && currentIndex() < mUuidList.size() ?
           mUuidList [currentIndex()] :
           aId < 0 || aId >= mUuidList.size() ? QUuid() :
           mUuidList [aId];
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
    }
}


void VBoxMediaComboBox::processActivated (int aIndex)
{
    mRequiredId = aIndex < 0 || aIndex >= mUuidList.size() ?
        QUuid() : mUuidList [aIndex];
}

void VBoxMediaComboBox::processIndexChanged (int aIndex)
{
    /* Combobox tooltip re-attaching */
    setToolTip (QString::null);
    if (aIndex >= 0 && aIndex < mTipList.size())
        setToolTip (mTipList [aIndex]);
}

void VBoxMediaComboBox::processOnItem (const QModelIndex &aIndex)
{
    /* Combobox item's tooltip attaching */
    int index = aIndex.row();
    view()->viewport()->setToolTip (QString::null);
    view()->viewport()->setToolTip (mTipList [index]);
}


void VBoxMediaComboBox::processMedia (const VBoxMedia &aMedia)
{
    if (mType == -1 || !(aMedia.type & mType))
        return;

    switch (aMedia.type)
    {
        case VBoxDefs::HD:
        {
            CHardDisk hd = aMedia.disk;
            /* Ignoring non-root disks */
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
    CDVDImage cd = aMedia.disk;
    QString src = cd.GetFilePath();
    QUuid mediaId = cd.GetId();
    QString toolTip = VBoxDiskImageManagerDlg::composeCdToolTip (cd, aMedia.status);
    updateShortcut (src, mediaId, toolTip, aMedia.status);
}

void VBoxMediaComboBox::processFdMedia (const VBoxMedia &aMedia)
{
    CFloppyImage fd = aMedia.disk;
    QString src = fd.GetFilePath();
    QUuid mediaId = fd.GetId();
    QString toolTip = VBoxDiskImageManagerDlg::composeFdToolTip (fd, aMedia.status);
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
    index == -1 ? newItem (name, aId, aTip, pixmap) :
                  updItem (index, name, aTip, pixmap);

    /* Activate required item if it was updated */
    if (aId == mRequiredId)
        setCurrentItem (aId);
    /* Activate first item in list if current is not required */
    else if (getId() != mRequiredId)
        setCurrentIndex (0);
    /* Update current item if it was not really changed */
    processIndexChanged (currentIndex());
}

void VBoxMediaComboBox::newItem (const QString &aName,
                                 const QUuid   &aId,
                                 const QString &aTip,
                                 QPixmap       *aPixmap)
{
    int initCount = count();

    int insertPosition = initCount;
    for (int i = 0; i < initCount; ++ i)
        /* Searching for the first real (non-null) vdi item
           which have name greater than the item to be inserted.
           This is necessary for sorting items alphabetically. */
        if (itemText (i).localeAwareCompare (aName) > 0 &&
            !getId (i).isNull())
        {
            insertPosition = i;
            break;
        }

    insertPosition == initCount ? mUuidList.append (aId) :
        mUuidList.insert (insertPosition, aId);

    insertPosition == initCount ? mTipList.append (aTip) :
        mTipList.insert (insertPosition, aTip);

    aPixmap ? insertItem (insertPosition, *aPixmap, aName) :
              insertItem (insertPosition, aName);
}

void VBoxMediaComboBox::updItem (int            aIndex,
                                 const QString &aNewName,
                                 const QString &aNewTip,
                                 QPixmap       *aNewPix)
{
    setItemText (aIndex, aNewName);
    if (aNewPix)
        setItemIcon (aIndex, *aNewPix);
    mTipList [aIndex] = aNewTip;
}

