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

#include <qcombobox.h>
#include <quuid.h>

struct VBoxMedia;
typedef QValueList <VBoxMedia> VBoxMediaList;

class VBoxMediaComboBox : public QComboBox
{
    Q_OBJECT

public:

    VBoxMediaComboBox (QWidget *aParent = 0,
                       const char *aName = 0, int aType = 0);
    ~VBoxMediaComboBox () {}

    void refresh();
    QUuid getId();
    void appendItem (const QString&, const QUuid&);
    void removeLastItem();
    void setRequiredItem (const QUuid&);
    void setUseEmptyItem (bool);
    void setReadyForRefresh();
    void setBelongsTo (const QUuid&);
    QUuid getBelongsTo();
    void loadShortCuts (const QStringList&, const QStringList&);

protected slots:

    void updateShortcuts (const VBoxMediaList &);

protected:

    int         mType;
    QStringList mUuidList;
    QUuid       mMachineId;
    QUuid       mRequiredId;
    bool        mUseEmptyItem;
    bool        mToBeRefreshed;
    bool        mWasEnabled;
};

#endif /* __VBoxMediaComboBox_h__ */
