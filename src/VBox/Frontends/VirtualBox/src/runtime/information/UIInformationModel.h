/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationModel class declaration.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIInformationModel_h___
#define ___UIInformationModel_h___

/* Qt includes: */
#include <QMap>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QAbstractListModel>

/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UIInformationDataItem.h"

/* COM includes: */
# include "CGuest.h"
# include "CConsole.h"
# include "CDisplay.h"
# include "CMachine.h"
# include "COMEnums.h"
# include "CNetworkAdapter.h"
# include "CMachineDebugger.h"
# include "CMediumAttachment.h"
# include "CSystemProperties.h"
# include "CStorageController.h"

/* Forward declarations: */
class UIInformationDataItem;

/** QAbstractListModel extension
  * providing GUI with information-model for view in session-information window. */
class UIInformationModel : public QAbstractListModel
{
    Q_OBJECT;

public:

    /** Constructs information-model passing @a pParent to base-class.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationModel(QObject *pParent, const CMachine &machine, const CConsole &console);

    /** Destructs information-model. */
    ~UIInformationModel();

    /** Returns the row-count for item specified by @a parentIdx. */
    int rowCount(const QModelIndex &parentIdx = QModelIndex()) const;

    /** Returns data for item specified by @a idx for the @a role. */
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const;

    /** Adds the item. */
    void addItem(UIInformationDataItem *pItem);

    /** Updates Data. */
    void updateData(const QModelIndex &idx);

public slots:
    void updateData(UIInformationDataItem *pItem);

private:

    /** Prepares information-model. */
    void prepare();

    /** Returns the list of role-names supported by model. */
    QHash<int, QByteArray> roleNames() const;

    /** Holds the machine instance. */
    CMachine m_machine;
    /** Holds the console instance. */
    CConsole m_console;
    /** Holds the list of instances of information-data items. */
    QList<UIInformationDataItem*> m_list;
};

#endif /* !___UIInformationModel_h___ */

