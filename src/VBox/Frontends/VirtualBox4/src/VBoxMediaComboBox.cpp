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

#include <qfileinfo.h>
#include <qimage.h>
#include <qdir.h>
#include <qfile.h>
#include <qtooltip.h>
#include <q3listbox.h>
//Added by qt3to4:
#include <QPixmap>

VBoxMediaComboBox::VBoxMediaComboBox (QWidget *aParent, int aType /* = -1 */,
                                      bool aUseEmptyItem /* = false */)
    : QComboBox (aParent),
    mType (aType), mRequiredId (QUuid()), mUseEmptyItem (aUseEmptyItem)
{
    init();
}

void VBoxMediaComboBox::init()
{
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    /* setup enumeration handlers */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &, int)),
             this, SLOT (mediaEnumerated (const VBoxMedia &, int)));

    /* setup update handlers */
    connect (&vboxGlobal(), SIGNAL (mediaAdded (const VBoxMedia &)),
             this, SLOT (mediaAdded (const VBoxMedia &)));
    connect (&vboxGlobal(), SIGNAL (mediaUpdated (const VBoxMedia &)),
             this, SLOT (mediaUpdated (const VBoxMedia &)));
    connect (&vboxGlobal(), SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
             this, SLOT (mediaRemoved (VBoxDefs::DiskType, const QUuid &)));

    connect (this, SIGNAL (activated (int)),
             this, SLOT (processActivated (int)));

    /* in some qt themes embedded list-box is not used by default, so create it */
#warning port me
//    if (!listBox())
//        setListBox (new Q3ListBox (this));
//    if (listBox())
//        connect (listBox(), SIGNAL (onItem (Q3ListBoxItem*)),
//                 this, SLOT (processOnItem (Q3ListBoxItem*)));

    /* cache pixmaps as class members */
    QImage img;
    img = QMessageBox::standardIcon (QMessageBox::Warning).convertToImage();
    if (!img.isNull())
    {
        img = img.smoothScale (14, 14);
        mPmInacc.convertFromImage (img);
    }
    img = QMessageBox::standardIcon (QMessageBox::Critical).convertToImage();
    if (!img.isNull())
    {
        img = img.smoothScale (14, 14);
        mPmError.convertFromImage (img);
    }
}

void VBoxMediaComboBox::refresh()
{
    /* clearing lists */
    clear(), mUuidList.clear(), mTipList.clear();
    /* prepend empty item if used */
    if (mUseEmptyItem)
        appendItem (tr ("<no hard disk>"), QUuid(), tr ("No hard disk"), 0);
    /* load current media list */
    VBoxMediaList list = vboxGlobal().currentMediaList();
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediaEnumerated (*it, 0);
    /* activate item selected during current list loading */
    processActivated (currentItem());
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

    /* search & remove media */
    int index = mUuidList.findIndex (aId);
    if (index != -1)
    {
        removeItem (index);
        mUuidList.remove (mUuidList.at (index));
        mTipList.remove (mTipList.at (index));
        /* emit signal to ensure parent dialog process selection changes */
        emit activated (currentItem());
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
            /* ignoring non-root disks */
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
    /* append list only with free hd */
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
    /* compose item's name */
    QFileInfo fi (aSrc);
    QString name = QString ("%1 (%2)").arg (fi.fileName())
                   .arg (QDir::convertSeparators (fi.dirPath()));
    /* update warning/error icons */
    QPixmap *pixmap = 0;
    if (aStatus == VBoxMedia::Inaccessible)
        pixmap = &mPmInacc;
    else if (aStatus == VBoxMedia::Error)
        pixmap = &mPmError;

    /* search media */
    int index = mUuidList.findIndex (aId);
    /* create or update media */
    if (index == -1)
        appendItem (name, aId, aTip, pixmap);
    else
        replaceItem (index, name, aTip, pixmap);

    /* activate required item if it was updated */
    if (aId == mRequiredId)
        setCurrentItem (aId);
    /* select last added item if there is no item selected */
    else if (currentText().isEmpty())
        QComboBox::setCurrentItem (index == -1 ? count() - 1 : index);
}

void VBoxMediaComboBox::processActivated (int aItem)
{
    mRequiredId = mUuidList.isEmpty() || aItem < 0 ? QUuid() : QUuid (mUuidList [aItem]);
    updateToolTip (aItem);
}

void VBoxMediaComboBox::updateToolTip (int aItem)
{
    /* combobox tooltip attaching */
    QToolTip::remove (this);
    if (!mTipList.isEmpty() && aItem >= 0)
        QToolTip::add (this, mTipList [aItem]);
}

void VBoxMediaComboBox::processOnItem (Q3ListBoxItem* aItem)
{
    /* combobox item's tooltip attaching */
#warning port me
//    int index = listBox()->index (aItem);
//    QToolTip::remove (listBox()->viewport());
//    QToolTip::add (listBox()->viewport(), mTipList [index]);
}

QUuid VBoxMediaComboBox::getId (int aId)
{
    return mUuidList.isEmpty() ? QUuid() :
           aId == -1 ? QUuid (mUuidList [currentItem()]) :
           QUuid (mUuidList [aId]);
}

void VBoxMediaComboBox::appendItem (const QString &aName,
                                    const QUuid   &aId,
                                    const QString &aTip,
                                    QPixmap       *aPixmap)
{
    int currentIndex = currentItem();

    int insertPosition = -1;
    for (int i = 0; i < count(); ++ i)
        /* Searching for the first real (non-null) vdi item
           which have name greater than the item to be inserted.
           This is necessary for sorting items alphabetically. */
        if (text (i).localeAwareCompare (aName) > 0 &&
            !getId (i).isNull())
        {
            insertPosition = i;
            break;
        }

    insertPosition == -1 ? mUuidList.append (aId) :
        mUuidList.insert (insertPosition, aId);

    insertPosition == -1 ? mTipList.append (aId) :
        mTipList.insert (insertPosition, aTip);

    aPixmap ? insertItem (*aPixmap, aName, insertPosition) :
              insertItem (aName, insertPosition);

    if (insertPosition != -1 && currentIndex >= insertPosition)
        QComboBox::setCurrentItem (currentIndex + 1);
}

void VBoxMediaComboBox::replaceItem (int            aNumber,
                                     const QString &aName,
                                     const QString &aTip,
                                     QPixmap       *aPixmap)
{
    aPixmap ? changeItem (*aPixmap, aName, aNumber) : changeItem (aName, aNumber);
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
    int index = mUuidList.findIndex (mRequiredId);
    if (index != -1)
    {
        QComboBox::setCurrentItem (index);
        emit activated (index);
    }
}

void VBoxMediaComboBox::setType (int aType)
{
    mType = aType;
}
