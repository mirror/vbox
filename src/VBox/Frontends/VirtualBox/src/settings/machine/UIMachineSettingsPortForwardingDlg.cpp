/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsPortForwardingDlg class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>

/* GUI includes: */
#include "UIMachineSettingsPortForwardingDlg.h"
#include "UIMessageCenter.h"
#include "UIToolBar.h"
#include "QITableView.h"
#include "QIDialogButtonBox.h"
#include "UIIconPool.h"
#include "UIConverter.h"

/* Other VBox includes: */
#include <iprt/cidr.h>

/* External includes: */
#include <math.h>

/* IP validator: */
class IPValidator : public QValidator
{
    Q_OBJECT;

public:

    IPValidator(QObject *pParent) : QValidator(pParent) {}
    ~IPValidator() {}

    QValidator::State validate(QString &strInput, int & /* iPos */) const
    {
        QString strStringToValidate(strInput);
        strStringToValidate.remove(' ');
        QString strDot("\\.");
        QString strDigits("(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)");
        QRegExp intRegExp(QString("^(%1?(%2(%1?(%2(%1?(%2%1?)?)?)?)?)?)?$").arg(strDigits).arg(strDot));
        uint uNetwork, uMask;
        if (strStringToValidate == "..." || RTCidrStrToIPv4(strStringToValidate.toLatin1().constData(), &uNetwork, &uMask) == VINF_SUCCESS)
            return QValidator::Acceptable;
        else if (intRegExp.indexIn(strStringToValidate) != -1)
            return QValidator::Intermediate;
        else
            return QValidator::Invalid;
    }
};

/* Name editor: */
class NameEditor : public QLineEdit
{
    Q_OBJECT;
    Q_PROPERTY(NameData name READ name WRITE setName USER true);

public:

    NameEditor(QWidget *pParent = 0) : QLineEdit(pParent)
    {
        setFrame(false);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setValidator(new QRegExpValidator(QRegExp("[^,]*"), this));
    }

private:

    void setName(NameData name)
    {
        setText(name);
    }

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

public:

    ProtocolEditor(QWidget *pParent = 0) : QComboBox(pParent)
    {
        addItem(gpConverter->toString(KNATProtocol_UDP), QVariant::fromValue(KNATProtocol_UDP));
        addItem(gpConverter->toString(KNATProtocol_TCP), QVariant::fromValue(KNATProtocol_TCP));
    }

private:

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

    KNATProtocol protocol() const
    {
        return itemData(currentIndex()).value<KNATProtocol>();
    }
};

/* IP editor: */
class IPEditor : public QLineEdit
{
    Q_OBJECT;
    Q_PROPERTY(IpData ip READ ip WRITE setIp USER true);

public:

    IPEditor(QWidget *pParent = 0) : QLineEdit(pParent)
    {
        setFrame(false);
        setAlignment(Qt::AlignCenter);
        setValidator(new IPValidator(this));
        setInputMask("000.000.000.000");
    }

private:

    void setIp(IpData ip)
    {
        setText(ip);
    }

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

public:

    PortEditor(QWidget *pParent = 0) : QSpinBox(pParent)
    {
        setFrame(false);
        setRange(0, (1 << (8 * sizeof(ushort))) - 1);
    }

private:

    void setPort(PortData port)
    {
        setValue(port.value());
    }

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

    /* Column names: */
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

    /* Port forwarding model constructor: */
    UIPortForwardingModel(QObject *pParent = 0, const UIPortForwardingDataList &rules = UIPortForwardingDataList())
        : QAbstractTableModel(pParent), m_dataList(rules) {}
    /* Port forwarding model destructor: */
    ~UIPortForwardingModel() {}

    /* The list of chosen rules: */
    const UIPortForwardingDataList& rules() const
    {
        return m_dataList;
    }

    /* Flags for model indexes: */
    Qt::ItemFlags flags(const QModelIndex &index) const
    {
        /* Check index validness: */
        if (!index.isValid())
            return Qt::NoItemFlags;
        /* All columns have similar flags: */
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    /* Row count getter: */
    int rowCount(const QModelIndex & parent = QModelIndex()) const
    {
        Q_UNUSED(parent);
        return m_dataList.size();
    }

    /* Column count getter: */
    int columnCount(const QModelIndex & parent = QModelIndex()) const
    {
        Q_UNUSED(parent);
        return UIPortForwardingDataType_Max;
    }

    /* Get header data: */
    QVariant headerData(int iSection, Qt::Orientation orientation, int iRole) const
    {
        /* Display role for horizontal header: */
        if (iRole == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            /* Switch for different columns: */
            switch (iSection)
            {
                case UIPortForwardingDataType_Name: return tr("Name");
                case UIPortForwardingDataType_Protocol: return tr("Protocol");
                case UIPortForwardingDataType_HostIp: return tr("Host IP");
                case UIPortForwardingDataType_HostPort: return tr("Host Port");
                case UIPortForwardingDataType_GuestIp: return tr("Guest IP");
                case UIPortForwardingDataType_GuestPort: return tr("Guest Port");
                default: break;
            }
        }
        /* Return wrong value: */
        return QVariant();
    }

    /* Get index data: */
    QVariant data(const QModelIndex &index, int iRole) const
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

    /* Set index data: */
    bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole)
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

    /* Add/Copy rule: */
    void addRule(const QModelIndex &index)
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

    /* Delete rule: */
    void delRule(const QModelIndex &index)
    {
        if (!index.isValid())
            return;
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        m_dataList.removeAt(index.row());
        endRemoveRows();
    }

private:

    /* Data container: */
    UIPortForwardingDataList m_dataList;
};

/* Port forwarding dialog constructor: */
UIMachineSettingsPortForwardingDlg::UIMachineSettingsPortForwardingDlg(QWidget *pParent,
                                                                 const UIPortForwardingDataList &rules)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , fIsTableDataChanged(false)
    , m_pTableView(0)
    , m_pToolBar(0)
    , m_pButtonBox(0)
    , m_pModel(0)
    , m_pAddAction(0)
    , m_pCopyAction(0)
    , m_pDelAction(0)
{
#ifdef Q_WS_MAC
    setWindowFlags(Qt::Sheet);
#endif /* Q_WS_MAC */

    /* Set dialog icon: */
    setWindowIcon(UIIconPool::iconSetFull(QSize(32, 32), QSize(16, 16), ":/nw_32px.png", ":/nw_16px.png"));

    /* Create table: */
    m_pTableView = new QITableView(this);
    m_pTableView->setTabKeyNavigation(false);
    m_pTableView->verticalHeader()->hide();
    m_pTableView->verticalHeader()->setDefaultSectionSize((int)(m_pTableView->verticalHeader()->minimumSectionSize() * 1.33));
    m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_pTableView->installEventFilter(this);
    connect(m_pTableView, SIGNAL(sigCurrentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(sltCurrentChanged()));
    connect(m_pTableView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(sltShowTableContexMenu(const QPoint &)));

    /* Create actions: */
    m_pAddAction = new QAction(this);
    m_pCopyAction = new QAction(this);
    m_pDelAction = new QAction(this);
    m_pAddAction->setShortcut(QKeySequence("Ins"));
    m_pDelAction->setShortcut(QKeySequence("Del"));
    m_pAddAction->setIcon(UIIconPool::iconSet(":/controller_add_16px.png", ":/controller_add_disabled_16px.png"));
    m_pCopyAction->setIcon(UIIconPool::iconSet(":/controller_add_16px.png", ":/controller_add_disabled_16px.png"));
    m_pDelAction->setIcon(UIIconPool::iconSet(":/controller_remove_16px.png", ":/controller_remove_disabled_16px.png"));
    connect(m_pAddAction, SIGNAL(triggered(bool)), this, SLOT(sltAddRule()));
    connect(m_pCopyAction, SIGNAL(triggered(bool)), this, SLOT(sltCopyRule()));
    connect(m_pDelAction, SIGNAL(triggered(bool)), this, SLOT(sltDelRule()));

    /* Create toolbar: */
    m_pToolBar = new UIToolBar(this);
    m_pToolBar->setIconSize(QSize(16, 16));
    m_pToolBar->setOrientation(Qt::Vertical);
    m_pToolBar->addAction(m_pAddAction);
    m_pToolBar->addAction(m_pDelAction);

    /* Create table & toolbar layout: */
    QHBoxLayout *pTableAndToolbarLayout = new QHBoxLayout;
    pTableAndToolbarLayout->setSpacing(1);
    pTableAndToolbarLayout->addWidget(m_pTableView);
    pTableAndToolbarLayout->addWidget(m_pToolBar);

    /* Create buttonbox: */
    m_pButtonBox = new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(m_pButtonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_pButtonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout;
    this->setLayout(pMainLayout);
    pMainLayout->addLayout(pTableAndToolbarLayout);
    pMainLayout->addWidget(m_pButtonBox);

    /* Create model: */
    m_pModel = new UIPortForwardingModel(this, rules);
    connect(m_pModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(sltTableDataChanged()));
    connect(m_pModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(sltTableDataChanged()));
    connect(m_pModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)), this, SLOT(sltTableDataChanged()));
    m_pTableView->setModel(m_pModel);

    /* Register delegates editors: */
    if (QAbstractItemDelegate *pAbstractItemDelegate = m_pTableView->itemDelegate())
    {
        if (QStyledItemDelegate *pStyledItemDelegate = qobject_cast<QStyledItemDelegate*>(pAbstractItemDelegate))
        {
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
            QStandardItemEditorCreator<IPEditor> *pIpEditorItemCreator = new QStandardItemEditorCreator<IPEditor>();
            /* Link ip type & editor: */
            pNewItemEditorFactory->registerEditor((QVariant::Type)iIpId, pIpEditorItemCreator);

            /* Register port type: */
            int iPortId = qRegisterMetaType<PortData>();
            /* Register port editor: */
            QStandardItemEditorCreator<PortEditor> *pPortEditorItemCreator = new QStandardItemEditorCreator<PortEditor>();
            /* Link port type & editor: */
            pNewItemEditorFactory->registerEditor((QVariant::Type)iPortId, pPortEditorItemCreator);

            /* Set newly created item editor factory for table delegate: */
            pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);
        }
    }

    /* Retranslate dialog: */
    retranslateUi();

    /* Minimum Size: */
    setMinimumSize(600, 300);
}

/* Port forwarding dialog destructor: */
UIMachineSettingsPortForwardingDlg::~UIMachineSettingsPortForwardingDlg()
{
}

const UIPortForwardingDataList& UIMachineSettingsPortForwardingDlg::rules() const
{
    return m_pModel->rules();
}

/* Add rule slot: */
void UIMachineSettingsPortForwardingDlg::sltAddRule()
{
    m_pModel->addRule(QModelIndex());
    m_pTableView->setFocus();
    m_pTableView->setCurrentIndex(m_pModel->index(m_pModel->rowCount() - 1, 0));
    sltCurrentChanged();
    sltAdjustTable();
}

/* Copy rule slot: */
void UIMachineSettingsPortForwardingDlg::sltCopyRule()
{
    m_pModel->addRule(m_pTableView->currentIndex());
    m_pTableView->setFocus();
    m_pTableView->setCurrentIndex(m_pModel->index(m_pModel->rowCount() - 1, 0));
    sltCurrentChanged();
    sltAdjustTable();
}

/* Del rule slot: */
void UIMachineSettingsPortForwardingDlg::sltDelRule()
{
    m_pModel->delRule(m_pTableView->currentIndex());
    m_pTableView->setFocus();
    sltCurrentChanged();
    sltAdjustTable();
}

/* Table data change handler: */
void UIMachineSettingsPortForwardingDlg::sltTableDataChanged()
{
    fIsTableDataChanged = true;
}

/* Table index-change handler: */
void UIMachineSettingsPortForwardingDlg::sltCurrentChanged()
{
    bool fTableFocused = m_pTableView->hasFocus();
    bool fTableChildrenFocused = m_pTableView->findChildren<QWidget*>().contains(QApplication::focusWidget());
    bool fTableOrChildrenFocused = fTableFocused || fTableChildrenFocused;
    m_pCopyAction->setEnabled(m_pTableView->currentIndex().isValid() && fTableOrChildrenFocused);
    m_pDelAction->setEnabled(m_pTableView->currentIndex().isValid() && fTableOrChildrenFocused);
}

/* Table context-menu handler: */
void UIMachineSettingsPortForwardingDlg::sltShowTableContexMenu(const QPoint &pos)
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

/* Adjusts table column's sizes: */
void UIMachineSettingsPortForwardingDlg::sltAdjustTable()
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

void UIMachineSettingsPortForwardingDlg::accept()
{
    /* Validate table: */
    for (int i = 0; i < m_pModel->rowCount(); ++i)
    {
        if (m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_HostPort), Qt::EditRole).value<PortData>().value() == 0 ||
            m_pModel->data(m_pModel->index(i, UIPortForwardingModel::UIPortForwardingDataType_GuestPort), Qt::EditRole).value<PortData>().value() == 0)
        {
            msgCenter().warnAboutIncorrectPort(this);
            return;
        }
    }
    /* Base class accept() slot: */
    QIWithRetranslateUI<QIDialog>::accept();
}

void UIMachineSettingsPortForwardingDlg::reject()
{
    /* Check if table data was changed: */
    if (fIsTableDataChanged && !msgCenter().confirmCancelingPortForwardingDialog(this))
        return;
    /* Base class reject() slot: */
    QIWithRetranslateUI<QIDialog>::reject();
}

/* UI Retranslation slot: */
void UIMachineSettingsPortForwardingDlg::retranslateUi()
{
    /* Set window title: */
    setWindowTitle(tr("Port Forwarding Rules"));

    /* Table translations: */
    m_pTableView->setWhatsThis(tr("This table contains a list of port forwarding rules."));

    /* Set action's text: */
    m_pAddAction->setText(tr("Insert new rule"));
    m_pCopyAction->setText(tr("Copy selected rule"));
    m_pDelAction->setText(tr("Delete selected rule"));
    m_pAddAction->setWhatsThis(tr("This button adds new port forwarding rule."));
    m_pDelAction->setWhatsThis(tr("This button deletes selected port forwarding rule."));
    m_pAddAction->setToolTip(QString("%1 (%2)").arg(m_pAddAction->text()).arg(m_pAddAction->shortcut().toString()));
    m_pDelAction->setToolTip(QString("%1 (%2)").arg(m_pDelAction->text()).arg(m_pDelAction->shortcut().toString()));
}

/* Extended event-handler: */
bool UIMachineSettingsPortForwardingDlg::eventFilter(QObject *pObj, QEvent *pEvent)
{
    /* Process table: */
    if (pObj == m_pTableView)
    {
        /* Switch for different event-types: */
        switch (pEvent->type())
        {
            case QEvent::FocusIn:
            case QEvent::FocusOut:
                /* Update actions: */
                sltCurrentChanged();
                break;
            case QEvent::Show:
            case QEvent::Resize:
            {
                /* Instant table adjusting: */
                sltAdjustTable();
                /* Delayed table adjusting: */
                QTimer::singleShot(0, this, SLOT(sltAdjustTable()));
                break;
            }
            default:
                break;
        }
    }
    /* Continue with base-class processing: */
    return QIWithRetranslateUI<QIDialog>::eventFilter(pObj, pEvent);
}

#include "UIMachineSettingsPortForwardingDlg.moc"

