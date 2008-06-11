/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMediaComboBox class declaration
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

#ifndef __VBoxMediaComboBox_h__
#define __VBoxMediaComboBox_h__

#include "VBoxGlobal.h"

#include <QComboBox>
#include <QPixmap>

class VBoxMediaComboBox : public QComboBox
{
    Q_OBJECT

public:

    static QString fullItemName (const QString &aSrc);

    VBoxMediaComboBox (QWidget *aParent, int aType = -1,
                       bool aUseEmptyItem = false);

    void  refresh();

    void  setUseEmptyItem (bool aUse);
    void  setBelongsTo (const QUuid &aMachineId);
    void  setCurrentItem (const QUuid &aId);
    void  setType (int aImageType);

    QUuid getId (int aId = -1) const;

protected slots:

    void mediaEnumStarted();
    void mediaEnumerated (const VBoxMedia &, int);

    void mediaAdded (const VBoxMedia &);
    void mediaUpdated (const VBoxMedia &);
    void mediaRemoved (VBoxDefs::DiskType, const QUuid &);

    void processActivated (int aIndex);
    void processIndexChanged (int aIndex);

    void processOnItem (const QModelIndex &aIndex);

protected:

    void processMedia (const VBoxMedia &);
    void processHdMedia (const VBoxMedia &);
    void processCdMedia (const VBoxMedia &);
    void processFdMedia (const VBoxMedia &);

    void updateShortcut (const QString &aSrc, const QUuid &aId,
                         const QString &aToolTip, VBoxMedia::Status aStatus);

    void newItem (const QString &aName, const QUuid &aId,
                  const QString &aToolTip, QPixmap *aPixmap);
    void updItem (int aIndex, const QString &aNewName,
                  const QString &aNewTip, QPixmap *aNewPix);

    QList<QUuid> mUuidList;
    QList<QString> mTipList;

    int         mType;
    QUuid       mMachineId;
    QUuid       mRequiredId;
    bool        mUseEmptyItem;
    QPixmap     mPmInacc;
    QPixmap     mPmError;
};

#endif /* __VBoxMediaComboBox_h__ */
