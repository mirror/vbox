/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMediaComboBox class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxMediaComboBox_h__
#define __VBoxMediaComboBox_h__

#include "VBoxGlobal.h"

#include <qcombobox.h>
//Added by qt3to4:
#include <QPixmap>

class Q3ListBoxItem;

class VBoxMediaComboBox : public QComboBox
{
    Q_OBJECT

public:

    VBoxMediaComboBox (QWidget *aParent, int aType = -1,
                       bool aUseEmptyItem = false);
    ~VBoxMediaComboBox() {}

    void  refresh();
    void  setUseEmptyItem (bool);
    void  setBelongsTo (const QUuid &);
    QUuid getId();
    QUuid getBelongsTo();
    void  setCurrentItem (const QUuid &);
    void  setType (int);

protected slots:

    void mediaEnumStarted();
    void mediaEnumerated (const VBoxMedia &, int);
    void mediaAdded (const VBoxMedia &);
    void mediaUpdated (const VBoxMedia &);
    void mediaRemoved (VBoxDefs::DiskType, const QUuid &);
    void processOnItem (Q3ListBoxItem *);
    void processActivated (int);

protected:

    void init();
    void updateToolTip (int);
    void processMedia (const VBoxMedia &);
    void processHdMedia (const VBoxMedia &);
    void processCdMedia (const VBoxMedia &);
    void processFdMedia (const VBoxMedia &);
    void appendItem (const QString &, const QUuid &,
                     const QString &, QPixmap *);
    void replaceItem (int, const QString &,
                      const QString &, QPixmap *);
    void updateShortcut (const QString &, const QUuid &, const QString &,
                          VBoxMedia::Status);

    int         mType;
    QStringList mUuidList;
    QStringList mTipList;
    QUuid       mMachineId;
    QUuid       mRequiredId;
    bool        mUseEmptyItem;
    QPixmap     mPmInacc;
    QPixmap     mPmError;
};

#endif /* __VBoxMediaComboBox_h__ */
