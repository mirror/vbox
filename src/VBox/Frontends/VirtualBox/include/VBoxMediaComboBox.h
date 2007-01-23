/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMediaComboBox class declaration
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

#ifndef __VBoxMediaComboBox_h__
#define __VBoxMediaComboBox_h__

#include "VBoxGlobal.h"

#include <qcombobox.h>
#include <quuid.h>

class QListBoxItem;

class VBoxMediaComboBox : public QComboBox
{
    Q_OBJECT

public:

    VBoxMediaComboBox (QWidget *aParent = 0,
                       const char *aName = 0, int aType = 0);
    ~VBoxMediaComboBox() {}

    void  refresh();
    void  appendItem (const QString &, const QUuid &, const QString &);
    void  replaceItem (int, const QString &, const QString &);
    void  removeLastItem();
    void  setReadyForRefresh();
    void  setRequiredItem (const QUuid &);
    void  setUseEmptyItem (bool);
    void  setBelongsTo (const QUuid &);
    QUuid getId();
    QUuid getBelongsTo();
    void setCurrentItem (int);

protected slots:

    void mediaEnumerated (const VBoxMedia &);
    void listEnumerated (const VBoxMediaList &);
    void processOnItem (QListBoxItem *);
    void updateToolTip (int);

protected:

    void loadCleanContent();
    int updateMedia (const QString &, const QUuid &, const QString &,
                     VBoxMedia::Status);

    int         mType;
    QStringList mUuidList;
    QStringList mTipList;
    QUuid       mMachineId;
    QUuid       mRequiredId;
    bool        mUseEmptyItem;
    bool        mToBeRefreshed;
};

#endif /* __VBoxMediaComboBox_h__ */
