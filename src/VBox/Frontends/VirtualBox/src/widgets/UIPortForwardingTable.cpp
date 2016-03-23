/* $Id$ */
/** @file
 * VBox Qt GUI - UIPortForwardingTable class implementation.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHBoxLayout>
# include <QMenu>
# include <QAction>
# include <QHeaderView>
# include <QStyledItemDelegate>
# include <QItemEditorFactory>
# include <QComboBox>
# include <QLineEdit>
# include <QSpinBox>

/* GUI includes: */
# include "UIPortForwardingTable.h"
# include "UIMessageCenter.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIToolBar.h"
# include "QITableView.h"
# include "QIStyledItemDelegate.h"

/* Other VBox includes: */
# include <iprt/cidr.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* External includes: */
#include <math.h>


/* IPv4 validator: */
class IPv4Validator : public QValidator
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    IPv4Validator(QObject *pParent) : QValidator(pParent) {}
    ~IPv4Validator() {}

    /* Handler: Validation stuff: */
    QValidator::State validate(QString &strInput, int& /*iPos*/) const
    {
        QString strStringToValidate(strInput);
        strStringToValidate.remove(' ');
        QString strDot("\\.");
        QString strDigits("(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)");
        QRegExp intRegExp(QString("^(%1?(%2(%1?(%2(%1?(%2%1?)?)?)?)?)?)?$").arg(strDigits).arg(strDot));
        RTNETADDRIPV4 Network, Mask;
        if (strStringToValidate == "..." || RTCidrStrToIPv4(strStringToValidate.toLatin1().constData(), &Network, &Mask) == VINF_SUCCESS)
            return QValidator::Acceptable;
        else if (intRegExp.indexIn(strStringToValidate) != -1)
            return QValidator::Intermediate;
        else
            return QValidator::Invalid;
    }
};

/* IPv6 validator: */
class IPv6Validator : public QValidator
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    IPv6Validator(QObject *pParent) : QValidator(pParent) {}
    ~IPv6Validator() {}

    /* Handler: Validation stuff: */
    QValidator::State validate(QString &strInput, int& /*iPos*/) const
    {
        QString strStringToValidate(strInput);
        strStringToValidate.remove(' ');
        QString strDigits("([0-9a-fA-F]{0,4})");
        QRegExp intRegExp(QString("^%1(:%1(:%1(:%1(:%1(:%1(:%1(:%1)?)?)?)?)?)?)?$").arg(strDigits));
        if (intRegExp.indexIn(strStringToValidate) != -1)
            return QValidator::Acceptable;
        else
            return QValidator::Invalid;
    }
};

/* Name editor: */
class NameEditor : public QLineEdit
{
    Q_OBJECT;
    Q_PROPERTY(NameData name READ name WRITE setName USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /* Constructor: */
    NameEditor(QWidget *pParent = 0) : QLineEdit(pParent)
    {
        setFrame(false);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setValidator(new QRegExpValidator(QRegExp("[^,:]*"), this));
        connect(this, SIGNAL(textEdited(const QString&)), this, SLOT(sltTextEdited(const QString&)));
    }

private slots:

    /** Drops the changed data to listener. */
    void sltTextEdited(const QString&)
    {
        emit sigCommitData(this);
    }

private:

    /* API: Name stuff: */
    void setName(NameData name)
    {
        setText(name);
    }

    /* API: Name stuff: */
    NameData name() const
    {
        return text();
    }
};

/* Protocol editor: */
class ProtocolEditor : public QComboBox
{
    Q_OBJECT;
    Q_PROPERTY(KNATProtocol protocol READ protocol WRITE setProtocol USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /* Constructor: */
    ProtocolEditor(QWidget *pParent = 0) : QComboBox(pParent)
    {
        addItem(gpConverter->toString(KNATProtocol_UDP), QVariant::fromValue(KNATProtocol_UDP));
        addItem(gpConverter->toString(KNATProtocol_TCP), QVariant::fromValue(KNATProtocol_TCP));
        connect(this, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(sltTextEdited(const QString&)));
    }

private slots:

    /** Drops the changed data to listener. */
    void sltTextEdited(const QString&)
    {
        emit sigCommitData(this);
    }

private:

    /* API: Protocol stuff: */
    void setProtocol(KNATProtocol p)
    {
        for (int i = 0; i < count(); ++i)
        {
            if (itemData(i).value<KNATProtocol>() == p)
            {
                setCurrentIndex(i);
                break;
            }
        }
    }

    /* API: Protocol stuff: */
    KNATProtocol protocol() const
    {
        return itemData(currentIndex()).value<KNATProtocol>();
    }
};

/* IPv4 editor: */
class IPv4Editor : public QLineEdit
{
    Q_OBJECT;
    Q_PROPERTY(IpData ip READ ip WRITE setIp USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /* Constructor: */
    IPv4Editor(QWidget *pParent = 0) : QLineEdit(pParent)
    {
        setFrame(false);
        setAlignment(Qt::AlignCenter);
        setValidator(new IPv4Validator(this));
        connect(this, SIGNAL(textEdited(const QString&)), this, SLOT(sltTextEdited(const QString&)));
    }

private slots:

    /** Drops the changed data to listener. */
    void sltTextEdited(const QString&)
    {
        emit sigCommitData(this);
    }

private:

    /* API: IP stuff: */
    void setIp(IpData ip)
    {
        setText(ip);
    }

    /* API: IP stuff: */
    IpData ip() const
    {
        return text() == "..." ? QString() : text();
    }
};

/* IPv6 editor: */
class IPv6Editor : public QLineEdit
{
    Q_OBJECT;
    Q_PROPERTY(IpData ip READ ip WRITE setIp USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /* Constructor: */
    IPv6Editor(QWidget *pParent = 0) : QLineEdit(pParent)
    {
        setFrame(false);
        setAlignment(Qt::AlignCenter);
        setValidator(new IPv6Validator(this));
        connect(this, SIGNAL(textEdited(const QString&)), this, SLOT(sltTextEdited(const QString&)));
    }

private slots:

    /** Drops the changed data to listener. */
    void sltTextEdited(const QString&)
    {
        emit sigCommitData(this);
    }

private:

    /* API: IP stuff: */
    void setIp(IpData ip)
    {
        setText(ip);
    }

    /* API: IP stuff: */
    IpData ip() const
    {
        return text() == "..." ? QString() : text();
    }
};

/* Port editor: */
class PortEditor : public QSpinBox
{
    Q_OBJECT;
    Q_PROPERTY(PortData port READ port WRITE setPort USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /* Constructor: */
    PortEditor(QWidget *pParent = 0) : QSpinBox(pParent)
    {
        setFrame(false);
        setRange(0, (1 << (8 * sizeof(ushort))) - 1);
        connect(this, SIGNAL(valueChanged(const QString&)), this, SLOT(sltTextEdited(const QString&)));
    }

private slots:

    /** Drops the changed data to listener. */
    void sltTextEdited(const QString&)
    {
        emit sigCommitData(this);
    }

private:

    /* API: Port stuff: */
    void setPort(PortData port)
    {
        setValue(port.value());
    }

    /* API: Port stuff: */
    PortData port() const
    {
        return value();
    }
};

/* Port forwarding data model: */
class UIPortForwardingModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    /* Enum: Column names: */
    enum UIPortForwardingDataType
    {
        UIPortForwardingDataType_Name,
        UIPortForwardingDataType_Protocol,
        UIPortForwardingDataType_HostIp,
        UIPortForwardingDataType_HostPort,
        UIPortForwardingDataType_GuestIp,
        UIPortForwardingDataType_GuestPort,
        UIPortForwardingDataType_Max
    };

    /* Constructor: */
    UIPortForwardingModel(QObject *pParent = 0, const UIPortForwardingDataList &rules = UIPortForwardingDataList())
        : QAbstractTableModel(pParent)
        , m_dataList(rules) {}

    /* API: Rule stuff: */
    const UIPortForwardingDataList& rules() const { return m_dataList; }
    void addRule(const QModelIndex &index);
    void delRule(const QModelIndex &index);

    /* API: Index flag stuff: */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    /* API: Index row-count stuff: */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /* API: Index column-count stuff: */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /* API: Header data stuff: */
    QVariant headerData(int iSection, Qt::Orientation orientation, int iRole) const;

    /* API: Index data stuff: */
    QVariant data(const QModelIndex &index, int iRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole);

private:

    /* Variable: Data stuff: */
    UIPortForwardingDataList m_dataList;
};

void UIPortForwardingModel::addRule(const QModelIndex &index)
{
    beginInsertRows(QModelIndex(), m_dataList.size(), m_dataList.size());
    /* Search for existing "Rule [NUMBER]" record: */
    uint uMaxIndex = 0;
    QString strTemplate("Rule %1");
    QRegExp regExp(strTemplate.arg("(\\d+)"));
    for (int i = 0; i < m_dataList.size(); ++i)
        if (regExp.indexIn(m_dataList[i].name) > -1)
            uMaxIndex = regExp.cap(1).toUInt() > uMaxIndex ? regExp.cap(1).toUInt() : uMaxIndex;
    /* If index is valid => copy data: */
    if (index.isValid())
        m_dataList << UIPortForwardingData(strTemplate.arg(++uMaxIndex), m_dataList[index.row()].protocol,
                                           m_dataList[index.row()].hostIp, m_dataList[index.row()].hostPort,
                                           m_dataList[index.row()].guestIp, m_dataList[index.row()].guestPort);
    /* If index is NOT valid => use default values: */
    else
        m_dataList << UIPortForwardingData(strTemplate.arg(++uMaxIndex), KNATProtocol_TCP,
                                           QString(""), 0, QString(""), 0);
    endInsertRows();
}

void UIPortForwardingModel::delRule(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    beginRemoveRows(QModelIndex(), index.row(), index.row());
    m_dataList.removeAt(index.row());
    endRemoveRows();
}

Qt::ItemFlags UIPortForwardingModel::flags(const QModelIndex &index) const
{
    /* Check index validness: */
    if (!index.isValid())
        return Qt::NoItemFlags;
    /* All columns have similar flags: */
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

int UIPortForwardingModel::rowCount(const QModelIndex&) const
{
    return m_dataList.size();
}

int UIPortForwardingModel::columnCount(const QModelIndex&) const
{
    return UIPortForwardingDataType_Max;
}

QVariant UIPortForwardingModel::headerData(int iSection, Qt::Orientation orientation, int iRole) const
{
    /* Display role for horizontal header: */
    if (iRole == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        /* Switch for different columns: */
        switch (iSection)
        {
            case UIPortForwardingDataType_Name: return UIPortForwardingTable::tr("Name");
            case UIPortForwardingDataType_Protocol: return UIPortForwardingTable::tr("Protocol");
            case UIPortForwardingDataType_HostIp: return UIPortForwardingTable::tr("Host IP");
            case UIPortForwardingDataType_HostPort: return UIPortForwardingTable::tr("Host Port");
            case UIPortForwardingDataType_GuestIp: return UIPortForwardingTable::tr("Guest IP");
            case UIPortForwardingDataType_GuestPort: return UIPortForwardingTable::tr("Guest Port");
            default: break;
        }
    }
    /* Return wrong value: */
    return QVariant();
}

QVariant UIPortForwardingModel::data(const QModelIndex &index, int iRole) const
{
    /* Check index validness: */
    if (!index.isValid())
        return QVariant();
    /* Switch for different roles: */
    switch (iRole)
    {
        /* Display role: */
        case Qt::DisplayRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIPortForwardingDataType_Name: return m_dataList[index.row()].name;
                case UIPortForwardingDataType_Protocol: return gpConverter->toString(m_dataList[index.row()].protocol);
                case UIPortForwardingDataType_HostIp: return m_dataList[index.row()].hostIp;
                case UIPortForwardingDataType_HostPort: return m_dataList[index.row()].hostPort.value();
                case UIPortForwardingDataType_GuestIp: return m_dataList[index.row()].guestIp;
                case UIPortForwardingDataType_GuestPort: return m_dataList[index.row()].guestPort.value();
                default: return QVariant();
            }
        }
        /* Edit role: */
        case Qt::EditRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIPortForwardingDataType_Name: return QVariant::fromValue(m_dataList[index.row()].name);
                case UIPortForwardingDataType_Protocol: return QVariant::fromValue(m_dataList[index.row()].protocol);
                case UIPortForwardingDataType_HostIp: return QVariant::fromValue(m_dataList[index.row()].hostIp);
                case UIPortForwardingDataType_HostPort: return QVariant::fromValue(m_dataList[index.row()].hostPort);
                case UIPortForwardingDataType_GuestIp: return QVariant::fromValue(m_dataList[index.row()].guestIp);
                case UIPortForwardingDataType_GuestPort: return QVariant::fromValue(m_dataList[index.row()].guestPort);
                default: return QVariant();
            }
        }
        /* Alignment role: */
        case Qt::TextAlignmentRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIPortForwardingDataType_Name:
                case UIPortForwardingDataType_Protocol:
                case UIPortForwardingDataType_HostPort:
                case UIPortForwardingDataType_GuestPort:
                    return (int)(Qt::AlignLeft | Qt::AlignVCenter);
                case UIPortForwardingDataType_HostIp:
                case UIPortForwardingDataType_GuestIp:
                    return Qt::AlignCenter;
                default: return QVariant();
            }
        }
        case Qt::SizeHintRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIPortForwardingDataType_HostIp:
                case UIPortForwardingDataType_GuestIp:
                    return QSize(QApplication::fontMetrics().width(" 888.888.888.888 "), QApplication::fontMetrics().height());
                default: return QVariant();
            }
        }
        default: break;
    }
    /* Return wrong value: */
    return QVariant();
}

bool UIPortForwardingModel::setData(const QModelIndex &index, const QVariant &value, int iRole /* = Qt::EditRole */)
{
    /* Check index validness: */
    if (!index.isValid() || iRole != Qt::EditRole)
        return false;
    /* Switch for different columns: */
    switch (index.column())
    {
        case UIPortForwardingDataType_Name:
            m_dataList[index.row()].name = value.value<NameData>();
            emit dataChanged(index, index);
            return true;
        case UIPortForwardingDataType_Protocol:
            m_dataList[index.row()].protocol = value.value<KNATProtocol>();
            emit dataChanged(index, index);
            return true;
        case UIPortForwardingDataType_HostIp:
            m_dataList[index.row()].hostIp = value.value<IpData>();
            emit dataChanged(index, index);
            return true;
        case UIPortForwardingDataType_HostPort:
            m_dataList[index.row()].hostPort = value.value<PortData>();
            emit dataChanged(index, index);
            return true;
        case UIPortForwardingDataType_GuestIp:
            m_dataList[index.row()].guestIp = value.value<IpData>();
            emit dataChanged(index, index);
            return true;
        case UIPortForwardingDataType_GuestPort:
            m_dataList[index.row()].guestPort = value.value<PortData>();
            emit dataChanged(index, index);
            return true;
        default: return false;
    }
    /* Return false value: */
    return false;
}


UIPortForwardingTable::UIPortForwardingTable(const UIPortForwardingDataList &rules, bool fIPv6, bool fAllowEmptyGuestIPs)
    : m_fAllowEmptyGuestIPs(fAllowEmptyGuestIPs)
    , m_fIsTableDataChanged(false)
    , m_pTableView(0)
    , m_pToolBar(0)
    , m_pModel(0)
    , m_pAddAction(0)
    , m_pCopyAction(0)
    , m_pDelAction(0)
{
    /* Create layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    {
        /* Configure layout: */
#ifndef Q_WS_WIN
        /* On Windows host that looks ugly, but
         * On Mac OS X and X11 that deserves it's place. */
        pMainLayout->setContentsMargins(0, 0, 0, 0);
#endif /* !Q_WS_WIN */
        pMainLayout->setSpacing(3);
        /* Create model: */
        m_pModel = new UIPortForwardingModel(this, rules);
        {
            /* Configure model: */
            connect(m_pModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(sltTableDataChanged()));
            connect(m_pModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)), this, SLOT(sltTableDataChanged()));
            connect(m_pModel, SIGNAL(rowsRemoved(const QModelIndex&, int, int)), this, SLOT(sltTableDataChanged()));
        }
        /* Create table: */
        m_pTableView = new QITableView;
        {
            /* Configure table: */
            m_pTableView->setModel(m_pModel);
            m_pTableView->setTabKeyNavigation(false);
            m_pTableView->verticalHeader()->hide();
            m_pTableView->verticalHeader()->setDefaultSectionSize((int)(m_pTableView->verticalHeader()->minimumSectionSize() * 1.33));
            m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
            m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
            m_pTableView->installEventFilter(this);
            connect(m_pTableView, SIGNAL(sigCurrentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(sltCurrentChanged()));
            connect(m_pTableView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(sltShowTableContexMenu(const QPoint &)));
        }
        /* Create toolbar: */
        m_pToolBar = new UIToolBar;
        {
            /* Determine icon metric: */
            const QStyle *pStyle = QApplication::style();
            const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
            /* Configure toolbar: */
            m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
            m_pToolBar->setOrientation(Qt::Vertical);
            /* Create 'add' action: */
            m_pAddAction = new QAction(this);
            {
                /* Configure 'add' action: */
                m_pAddAction->setShortcut(QKeySequence("Ins"));
                m_pAddAction->setIcon(UIIconPool::iconSet(":/controller_add_16px.png", ":/controller_add_disabled_16px.png"));
                connect(m_pAddAction, SIGNAL(triggered(bool)), this, SLOT(sltAddRule()));
                m_pToolBar->addAction(m_pAddAction);
            }
            /* Create 'copy' action: */
            m_pCopyAction = new QAction(this);
            {
                /* Configure 'add' action: */
                m_pCopyAction->setIcon(UIIconPool::iconSet(":/controller_add_16px.png", ":/controller_add_disabled_16px.png"));
                connect(m_pCopyAction, SIGNAL(triggered(bool)), this, SLOT(sltCopyRule()));
            }
            /* Create 'del' action: */
            m_pDelAction = new QAction(this);
            {
                /* Configure 'del' action: */
                m_pDelAction->setShortcut(QKeySequence("Del"));
                m_pDelAction->setIcon(UIIconPool::iconSet(":/controller_remove_16px.png", ":/controller_remove_disabled_16px.png"));
                connect(m_pDelAction, SIGNAL(triggered(bool)), this, SLOT(sltDelRule()));
                m_pToolBar->addAction(m_pDelAction);
            }
        }
        /* Add widgets into layout: */
        pMainLayout->addWidget(m_pTableView);
        pMainLayout->addWidget(m_pToolBar);
    }

    /* Reinstall delegate: */
    delete m_pTableView->itemDelegate();
    QIStyledItemDelegate *pStyledItemDelegate = new QIStyledItemDelegate(this);
    m_pTableView->setItemDelegate(pStyledItemDelegate);

    /* Create new item editor factory: */
    QItemEditorFactory *pNewItemEditorFactory = new QItemEditorFactory;

    /* Register name type: */
    int iNameId = qRegisterMetaType<NameData>();
    /* Register name editor: */
    QStandardItemEditorCreator<NameEditor> *pNameEditorItemCreator = new QStandardItemEditorCreator<NameEditor>();
    /* Link name type & editor: */
    pNewItemEditorFactory->registerEditor((QVariant::Type)iNameId, pNameEditorItemCreator);

    /* Register protocol type: */
    int iProtocolId = qRegisterMetaType<KNATProtocol>();
    /* Register protocol editor: */
    QStandardItemEditorCreator<ProtocolEditor> *pProtocolEditorItemCreator = new QStandardItemEditorCreator<ProtocolEditor>();
    /* Link protocol type & editor: */
    pNewItemEditorFactory->registerEditor((QVariant::Type)iProtocolId, pProtocolEditorItemCreator);

    /* Register ip type: */
    int iIpId = qRegisterMetaType<IpData>();
    /* Register ip editor: */
    if (!fIPv6)
    {
        QStandardItemEditorCreator<IPv4Editor> *pIPv4EditorItemCreator = new QStandardItemEditorCreator<IPv4Editor>();
        /* Link ip type & editor: */
        pNewItemEditorFactory->registerEditor((QVariant::Type)iIpId, pIPv4EditorItemCreator);
    }
    else
    {
        QStandardItemEditorCreator<IPv6Editor> *pIPv6EditorItemCreator = new QStandardItemEditorCreator<IPv6Editor>();
        /* Link ip type & editor: */
        pNewItemEditorFactory->registerEditor((QVariant::Type)iIpId, pIPv6EditorItemCreator);
    }

    /* Register port type: */
    int iPortId = qRegisterMetaType<PortData>();
    /* Register port editor: */
    QStandardItemEditorCreator<PortEditor> *pPortEditorItemCreator = new QStandardItemEditorCreator<PortEditor>();
    /* Link port type & editor: */
    pNewItemEditorFactory->registerEditor((QVariant::Type)iPortId, pPortEditorItemCreator);

    /* Set newly created item editor factory for table delegate: */
    pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);

    /* Retranslate dialog: */
    retranslateUi();

    /* Minimum Size: */
    setMinimumSize(600, 250);
}

const UIPortForwardingDataList& UIPortForwardingTable::rules() const
{
    return m_pModel->rules();
}

bool UIPortForwardingTable::validate() const
{
    /* Validate table: */
    QList<NameData> names;
    QList<UIPortForwardingDataUnique> rules;
    for (int i = 0; i < m_pModel->rowCount(); ++i)
    {
        /* Some of variables: */
        const NameData name = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_Name), Qt::EditRole).value<NameData>();
        const KNATProtocol protocol = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_Protocol), Qt::EditRole).value<KNATProtocol>();
        const PortData hostPort = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_HostPort), Qt::EditRole).value<PortData>().value();
        const PortData guestPort = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_GuestPort), Qt::EditRole).value<PortData>().value();
        const IpData hostIp = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_HostIp), Qt::EditRole).value<IpData>();
        const IpData guestIp = m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_GuestIp), Qt::EditRole).value<IpData>();

        /* If at least one port is 'zero': */
        if (hostPort.value() == 0 || guestPort.value() == 0)
            return msgCenter().warnAboutIncorrectPort(window());
        /* If at least one address is incorrect: */
        if (!(   hostIp.trimmed().isEmpty()
              || RTNetIsIPv4AddrStr(hostIp.toUtf8().constData())
              || RTNetIsIPv6AddrStr(hostIp.toUtf8().constData())
              || RTNetStrIsIPv4AddrAny(hostIp.toUtf8().constData())
              || RTNetStrIsIPv6AddrAny(hostIp.toUtf8().constData())))
            return msgCenter().warnAboutIncorrectAddress(window());
        if (!(   guestIp.trimmed().isEmpty()
              || RTNetIsIPv4AddrStr(guestIp.toUtf8().constData())
              || RTNetIsIPv6AddrStr(guestIp.toUtf8().constData())
              || RTNetStrIsIPv4AddrAny(guestIp.toUtf8().constData())
              || RTNetStrIsIPv6AddrAny(guestIp.toUtf8().constData())))
            return msgCenter().warnAboutIncorrectAddress(window());
        /* If empty guest address is not allowed: */
        if (   !m_fAllowEmptyGuestIPs
            && guestIp.isEmpty())
            return msgCenter().warnAboutEmptyGuestAddress(window());

        /* Make sure non of the names were previosly used: */
        if (!names.contains(name))
            names << name;
        else
            return msgCenter().warnAboutNameShouldBeUnique(window());

        /* Make sure non of the rules were previosly used: */
        UIPortForwardingDataUnique rule(protocol, hostPort, hostIp);
        if (!rules.contains(rule))
            rules << rule;
        else
            return msgCenter().warnAboutRulesConflict(window());
    }
    /* True by default: */
    return true;
}

void UIPortForwardingTable::sltAddRule()
{
    m_pModel->addRule(QModelIndex());
    m_pTableView->setFocus();
    m_pTableView->setCurrentIndex(m_pModel->index(m_pModel->rowCount() - 1, 0));
    sltCurrentChanged();
    sltAdjustTable();
}

void UIPortForwardingTable::sltCopyRule()
{
    m_pModel->addRule(m_pTableView->currentIndex());
    m_pTableView->setFocus();
    m_pTableView->setCurrentIndex(m_pModel->index(m_pModel->rowCount() - 1, 0));
    sltCurrentChanged();
    sltAdjustTable();
}

void UIPortForwardingTable::sltDelRule()
{
    m_pModel->delRule(m_pTableView->currentIndex());
    m_pTableView->setFocus();
    sltCurrentChanged();
    sltAdjustTable();
}

void UIPortForwardingTable::sltCurrentChanged()
{
    bool fTableFocused = m_pTableView->hasFocus();
    bool fTableChildFocused = m_pTableView->findChildren<QWidget*>().contains(QApplication::focusWidget());
    bool fTableOrChildFocused = fTableFocused || fTableChildFocused;
    m_pCopyAction->setEnabled(m_pTableView->currentIndex().isValid() && fTableOrChildFocused);
    m_pDelAction->setEnabled(m_pTableView->currentIndex().isValid() && fTableOrChildFocused);
}

void UIPortForwardingTable::sltShowTableContexMenu(const QPoint &pos)
{
    /* Prepare context menu: */
    QMenu menu(m_pTableView);
    /* If some index is currently selected: */
    if (m_pTableView->indexAt(pos).isValid())
    {
        menu.addAction(m_pCopyAction);
        menu.addAction(m_pDelAction);
    }
    /* If no valid index selected: */
    else
    {
        menu.addAction(m_pAddAction);
    }
    menu.exec(m_pTableView->viewport()->mapToGlobal(pos));
}

void UIPortForwardingTable::sltAdjustTable()
{
    m_pTableView->horizontalHeader()->setStretchLastSection(false);
    /* If table is NOT empty: */
    if (m_pModel->rowCount())
    {
        /* Resize table to contents size-hint and emit a spare place for first column: */
        m_pTableView->resizeColumnsToContents();
        uint uFullWidth = m_pTableView->viewport()->width();
        for (uint u = 1; u < UIPortForwardingModel::UIPortForwardingDataType_Max; ++u)
            uFullWidth -= m_pTableView->horizontalHeader()->sectionSize(u);
        m_pTableView->horizontalHeader()->resizeSection(UIPortForwardingModel::UIPortForwardingDataType_Name, uFullWidth);
    }
    /* If table is empty: */
    else
    {
        /* Resize table columns to be equal in size: */
        uint uFullWidth = m_pTableView->viewport()->width();
        for (uint u = 0; u < UIPortForwardingModel::UIPortForwardingDataType_Max; ++u)
            m_pTableView->horizontalHeader()->resizeSection(u, uFullWidth / UIPortForwardingModel::UIPortForwardingDataType_Max);
    }
    m_pTableView->horizontalHeader()->setStretchLastSection(true);
}

void UIPortForwardingTable::retranslateUi()
{
    /* Table translations: */
    m_pTableView->setToolTip(tr("Contains a list of port forwarding rules."));

    /* Set action's text: */
    m_pAddAction->setText(tr("Add New Rule"));
    m_pCopyAction->setText(tr("Copy Selected Rule"));
    m_pDelAction->setText(tr("Remove Selected Rule"));

    m_pAddAction->setWhatsThis(tr("Adds new port forwarding rule."));
    m_pCopyAction->setWhatsThis(tr("Copies selected port forwarding rule."));
    m_pDelAction->setWhatsThis(tr("Removes selected port forwarding rule."));

    m_pAddAction->setToolTip(m_pAddAction->whatsThis());
    m_pCopyAction->setToolTip(m_pCopyAction->whatsThis());
    m_pDelAction->setToolTip(m_pDelAction->whatsThis());
}

bool UIPortForwardingTable::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Process table: */
    if (pObject == m_pTableView)
    {
        /* Process different event-types: */
        switch (pEvent->type())
        {
            case QEvent::Show:
            case QEvent::Resize:
            {
                /* Adjust table: */
                sltAdjustTable();
                break;
            }
            case QEvent::FocusIn:
            case QEvent::FocusOut:
            {
                /* Update actions: */
                sltCurrentChanged();
                break;
            }
            default:
                break;
        }
    }
    /* Call to base-class: */
    return QIWithRetranslateUI<QWidget>::eventFilter(pObject, pEvent);
}

#include "UIPortForwardingTable.moc"

