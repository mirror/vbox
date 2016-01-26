/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationDataItem class declaration.
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

#ifndef ___UIInformationDataItem_h___
#define ___UIInformationDataItem_h___

/* Qt includes: */
#include <QIcon>
#include <QModelIndex>

/* GUI includes: */
#include "UIThreadPool.h"
#include "UIExtraDataDefs.h"
#include "UIInformationDataItem.h"

/* COM includes: */
/* COMEnums.h should be first header included for enum definitions: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CConsole.h"
#include "CDisplay.h"
#include "CNetworkAdapter.h"
#include "CMachineDebugger.h"
#include "CMediumAttachment.h"
#include "CSystemProperties.h"
#include "CStorageController.h"

/* Forward declarations: */
class CNetworkAdapter;
class QTextLayout;

/** QObject extension
  * used as data-item in information-model in session-information window. */
class UIInformationDataItem : public QObject
{
    Q_OBJECT;

public:

    /** Constructs information data-item of type @a type.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationDataItem(InformationElementType type, const CMachine &machine, const CConsole &console);

    /** Destructs information data-item. */
    ~UIInformationDataItem();

    /** Returns type of information data-item. */
    InformationElementType elementType() const { return m_type; }

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

protected:

    /** Holds the type of information data-item. */
    InformationElementType m_type;

    /** Holds the pixmap of information data-item. */
    QPixmap m_pixmap;

    /** Holds the name of information data-item. */
    QString m_strName;

    /** Holds the machine reference. */
    CMachine m_machine;

    /** Holds the machine console reference. */
    CConsole m_console;
};

/** UIInformationDataItem extension for the details-element type 'General'. */
class UIInformationDataGeneral : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIInformationDataGeneral(const CMachine &machine, const CConsole &console);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'System'. */
class UIInformationDataSystem : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIInformationDataSystem(const CMachine &machine, const CConsole &console);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'System'. */
class UIInformationDataDisplay : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIInformationDataDisplay(const CMachine &machine, const CConsole &console);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Storage'. */
class UIInformationDataStorage : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataStorage(const CMachine &machine, const CConsole &console);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

#endif /* !___UIInformationDataItem_h___ */

