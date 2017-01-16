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

    /** Constructs information-model passing @a pParent to the base-class.
      * @param  machine  Brings the machine reference.
      * @param  console  Brings the machine console reference. */
    UIInformationModel(QObject *pParent, const CMachine &machine, const CConsole &console);
    /** Destructs information-model. */
    ~UIInformationModel();

    /** Returns the row-count for item specified by the @a parentIdx. */
    int rowCount(const QModelIndex &parentIdx = QModelIndex()) const;

    /** Returns the data for item specified by the @a index and the @a role. */
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const;

    /** Adds the @a pItem into the model. */
    void addItem(UIInformationDataItem *pItem);

    /** Updates the data for item specified by the @a index. */
    void updateData(const QModelIndex &idx);

public slots:

    /** Updates the data for the specified @a pItem. */
    void updateData(UIInformationDataItem *pItem);

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Returns the list of role-names supported by model. */
    QHash<int, QByteArray> roleNames() const;

    /** Holds the machine reference. */
    CMachine m_machine;
    /** Holds the machine console reference. */
    CConsole m_console;
    /** Holds the list of instances of information-data items. */
    QList<UIInformationDataItem*> m_list;
};

#endif /* !___UIInformationModel_h___ */

