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

    typedef QMap <QUuid, QUuid> BaseToDiffMap;

    VBoxMediaComboBox (QWidget *aParent);

    void refresh();
    void repopulate();

    QUuid id (int = -1) const;
    QString location (int = -1) const;

    void setCurrentItem (const QUuid &aItemId);
    void setType (VBoxDefs::MediaType aMediaType);
    void setMachineId (const QUuid &aMachineId = QUuid());

    void setShowDiffs (bool aShowDiffs);
    bool showDiffs() const { return mShowDiffs; }

protected slots:

    void mediumEnumStarted();
    void mediumEnumerated (const VBoxMedium &);

    void mediumAdded (const VBoxMedium &);
    void mediumUpdated (const VBoxMedium &);
    void mediumRemoved (VBoxDefs::MediaType, const QUuid &);

    void processActivated (int aIndex);
//    void processIndexChanged (int aIndex);

    void processOnItem (const QModelIndex &aIndex);

protected:

    void addNoMediaItem();

    void updateToolTip (int);

    void appendItem (const VBoxMedium &);
    void replaceItem (int, const VBoxMedium &);

    bool findMediaIndex (const QUuid &aId, int &aIndex);

    VBoxDefs::MediaType mType;

    /** Obtruncated VBoxMedium structure. */
    struct Medium
    {
        Medium() {}
        Medium (const QUuid &aId, const QString &aLocation,
                const QString aToolTip)
            : id (aId), location (aLocation), toolTip (aToolTip) {}

        QUuid id;
        QString location;
        QString toolTip;
    };

    typedef QVector <Medium> Media;
    Media mMedia;

    QUuid mLastId;

    bool mShowDiffs : 1;

    QUuid mMachineId;
};

#endif /* __VBoxMediaComboBox_h__ */
