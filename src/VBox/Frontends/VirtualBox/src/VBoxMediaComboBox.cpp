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
#include "VBoxGlobal.h"

#include <qfileinfo.h>
#include <qdir.h>
#include <qfile.h>

VBoxMediaComboBox::VBoxMediaComboBox (QWidget *aParent, const char *aName, int aType)
    : QComboBox (aParent , aName),
    mType(aType), mRequiredId(QUuid()), mUseEmptyItem(false), mToBeRefreshed(false)
{
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    /* Media shortcuts creating */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMediaList &)),
             this, SLOT (updateShortcuts (const VBoxMediaList &)));
}

void VBoxMediaComboBox::updateShortcuts (const VBoxMediaList &aList)
{
    /* check&reset refresh flag */
    if (!mToBeRefreshed)
        return;
    else
        mToBeRefreshed = false;
    /* combo-box clearing */
    clear();
    mUuidList.clear();
    /* prepend empty item if used */
    if (mUseEmptyItem)
        appendItem (tr ("<no hard disk>"), QUuid());
    int activatedItem = 0;
    /* processing all medias */
    VBoxMediaList::const_iterator it;
    for (it = aList.begin(); it != aList.end(); ++ it)
    {
        const VBoxMedia& media = *it;
        if (!(media.type & mType))
            continue;
        QString src;
        QUuid   mediaId;
        switch (media.type)
        {
            case VBoxDefs::HD:
            {
                CHardDisk hd = media.disk;
                src = hd.GetLocation();
                QUuid machineId = hd.GetMachineId();
                /* append list only with free hd */
                if (machineId.isNull() || machineId == mMachineId)
                    mediaId = hd.GetId();
                break;
            }
            case VBoxDefs::CD:
            {
                CDVDImage dvd = media.disk;
                src = dvd.GetFilePath();
                mediaId = dvd.GetId();
                break;
            }
            case VBoxDefs::FD:
            {
                CFloppyImage floppy = media.disk;
                src = floppy.GetFilePath();
                mediaId = floppy.GetId();
                break;
            }
            default:
                AssertFailed();
        }
        if (!mediaId.isNull())
        {
            QFileInfo fi (src);
            appendItem (fi.fileName() + " (" +
                        QDir::convertSeparators (fi.dirPath()) + ")", mediaId);
            if (mediaId == mRequiredId)
            {
                activatedItem = count() - 1;
                setCurrentItem (activatedItem);
            }
        }
    }
    emit activated (activatedItem);
    if (mWasEnabled)
        setEnabled (true);
}

void VBoxMediaComboBox::refresh()
{
    if (mToBeRefreshed) return;
    setReadyForRefresh();
    vboxGlobal().startEnumeratingMedia();
}

QUuid VBoxMediaComboBox::getId()
{
    return mUuidList.isEmpty() ? QUuid() : QUuid (mUuidList[currentItem()]);
}

void VBoxMediaComboBox::appendItem (const QString &aName, const QUuid &aId)
{
    insertItem(aName);
    mUuidList << aId;
}

void VBoxMediaComboBox::removeLastItem()
{
    if (count())
        removeItem (count() - 1);
    mUuidList.pop_back();
}

void VBoxMediaComboBox::setRequiredItem (const QUuid &aId)
{
    mRequiredId = aId;
}

void VBoxMediaComboBox::setUseEmptyItem (bool aUseEmptyItem)
{
    mUseEmptyItem = aUseEmptyItem;
}

void VBoxMediaComboBox::setReadyForRefresh()
{
    mToBeRefreshed = true;
    /* clearing media combobox */
    clear(), mUuidList.clear();
    /* blocking media combobox */
    mWasEnabled = isEnabled();
    setEnabled (false);
    insertItem (tr ("...enumerating media..."));
}

void VBoxMediaComboBox::setBelongsTo (const QUuid &aMachineId)
{
    mMachineId = aMachineId;
}

QUuid VBoxMediaComboBox::getBelongsTo()
{
    return mMachineId;
}

void VBoxMediaComboBox::loadShortCuts (const QStringList& aNameList,
                                       const QStringList& aKeyList)
{
    /* clearing media combobox */
    clear(), mUuidList.clear();
    /* load shortcuts from string lists */
    QStringList::const_iterator itName = aNameList.begin();
    QStringList::const_iterator itKey  = aKeyList.begin();
    int activatedItem = 0;
    for (;itName != aNameList.end() && itKey != aKeyList.end();)
    {
        appendItem (*itName, *itKey);
        if (*itKey == mRequiredId)
        {
            activatedItem = count() - 1;
            setCurrentItem (activatedItem);
        }
        ++itName; ++itKey;
    }
    emit activated (activatedItem);
}
