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
class UIInformationModel;

/** QObject extension
  * used as data-item in information-model in session-information window. */
class UIInformationDataItem : public QObject
{
    Q_OBJECT;

public:

    /** Constructs information data-item of type @a type.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationDataItem(InformationElementType type, const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

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

    /** Holds the instance of model. */
    UIInformationModel *m_pModel;
};

/** UIInformationDataItem extension for the details-element type 'General'. */
class UIInformationDataGeneral : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIInformationDataGeneral(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

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
    UIInformationDataSystem(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

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
    UIInformationDataDisplay(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Storage'. */
class UIInformationDataStorage : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataStorage(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Audio'. */
class UIInformationDataAudio : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataAudio(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Network'. */
class UIInformationDataNetwork : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataNetwork(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Serial ports'. */
class UIInformationDataSerialPorts : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataSerialPorts(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

#ifdef VBOX_WITH_PARALLEL_PORTS
/** UIInformationDataItem extension for the details-element type 'Parallel ports'. */
class UIInformationDataParallelPorts : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataParallelPorts(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};
#endif /* VBOX_WITH_PARALLEL_PORTS */

/** UIInformationDataItem extension for the details-element type 'USB'. */
class UIInformationDataUSB : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataUSB(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'Shared folders'. */
class UIInformationDataSharedFolders : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataSharedFolders(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

protected slots:
    void updateData();
};

/** UIInformationDataItem extension for the details-element type 'runtime attributes'. */
class UIInformationDataRuntimeAttributes : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataRuntimeAttributes(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

/** UIInformationDataItem extension for the details-element type 'network statistics'. */
class UIInformationDataNetworkStatistics : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataNetworkStatistics(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private slots:
    /** Handles processing of statistics. */
    void sltProcessStatistics();

private:

    /** Returns parsed-data of statistics. */
    QString parseStatistics(const QString &strText);

    /** VM statistics counter data map. */
    typedef QMap <QString, QString> DataMapType;
    /** VM statistics counter links map. */
    typedef QMap <QString, QStringList> LinksMapType;
    /** VM statistics counter struct. */
    struct CounterElementType { QString type; DataMapType list; };

    /** VM statistics counter names. */
    DataMapType        m_names;
    /** VM statistics counter values. */
    DataMapType        m_values;
    /** VM statistics counter units. */
    DataMapType        m_units;
    /** VM statistics counter links. */
    LinksMapType       m_links;
    /** VM statistics update timer. */
    QTimer            *m_pTimer;
};

/** UIInformationDataItem extension for the details-element type 'storage statistics'. */
class UIInformationDataStorageStatistics : public UIInformationDataItem
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set. */
    UIInformationDataStorageStatistics(const CMachine &machine, const CConsole &console, UIInformationModel *pModel);

    /** Returns data for item specified by @a idx for the @a role. */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private slots:
    /** Handles processing of statistics. */
    void sltProcessStatistics();

private:

    /** Returns parsed-data of statistics. */
    QString parseStatistics(const QString &strText);

    /** Converts a given storage controller type to the string representation used
     * in statistics. */
    const char *storCtrlType2Str(const KStorageControllerType enmCtrlType) const;

    /** VM statistics counter data map. */
    typedef QMap <QString, QString> DataMapType;
    /** VM statistics counter links map. */
    typedef QMap <QString, QStringList> LinksMapType;
    /** VM statistics counter struct. */
    struct CounterElementType { QString type; DataMapType list; };

    /** VM statistics counter names. */
    DataMapType        m_names;
    /** VM statistics counter values. */
    DataMapType        m_values;
    /** VM statistics counter units. */
    DataMapType        m_units;
    /** VM statistics counter links. */
    LinksMapType       m_links;
    /** VM statistics update timer. */
    QTimer            *m_pTimer;
};

#endif /* !___UIInformationDataItem_h___ */

