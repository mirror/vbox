/* $Id$ */
/** @file
 * VBox Qt GUI - UIFormEditorWidget class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <QComboBox>
#include <QEvent>
#include <QHeaderView>
#include <QItemEditorFactory>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITableView.h"
#include "UIFormEditorWidget.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CBooleanFormValue.h"
#include "CChoiceFormValue.h"
#include "CFormValue.h"
#include "CStringFormValue.h"
#include "CVirtualSystemDescriptionForm.h"

/* VirtualBox interface declarations: */
#include <VBox/com/VirtualBox.h>


/** Form Editor data types. */
enum UIFormEditorDataType
{
    UIFormEditorDataType_Name,
    UIFormEditorDataType_Value,
    UIFormEditorDataType_Max
};


/** QComboBox extension used as Port editor. */
class ChoiceEditor : public QComboBox
{
    Q_OBJECT;
    Q_PROPERTY(ChoiceData choice READ choice WRITE setChoice USER true);

public:

    /** Constructs Port-editor passing @a pParent to the base-class. */
    ChoiceEditor(QWidget *pParent = 0)
        : QComboBox(pParent) {}

private:

    /** Defines the @a choice. */
    void setChoice(ChoiceData choice)
    {
        addItems(choice.choices().toList());
        setCurrentIndex(choice.selectedChoice());
    }

    /** Returns the choice. */
    ChoiceData choice() const
    {
        QVector<QString> choices(count());
        for (int i = 0; i < count(); ++i)
            choices[i] = itemText(i);
        return ChoiceData(choices, currentIndex());
    }
};


/** QITableViewCell extension used as Form Editor table-view cell. */
class UIFormEditorCell : public QITableViewCell
{
    Q_OBJECT;

public:

    /** Constructs table cell on the basis of certain @a strText, passing @a pParent to the base-class. */
    UIFormEditorCell(QITableViewRow *pParent, const QString &strText);

    /** Returns the cell text. */
    virtual QString text() const /* override */ { return m_strText; }

private:

    /** Holds the cell text. */
    QString  m_strText;
};


/** QITableViewRow extension used as Form Editor table-view row. */
class UIFormEditorRow : public QITableViewRow
{
    Q_OBJECT;

public:

    /** Constructs table row on the basis of certain @a comValue, passing @a pParent to the base-class. */
    UIFormEditorRow(QITableView *pParent, const CFormValue &comValue);
    /** Destructs table row. */
    virtual ~UIFormEditorRow() /* override */;

    /** Returns value type. */
    KFormValueType valueType() const { return m_enmValueType; }

    /** Returns the row name as string. */
    QString nameToString() const;
    /** Returns the row value as string. */
    QString valueToString() const;

    /** Returns value cast to bool. */
    bool toBool() const;

    /** Returns value cast to string. */
    QString toString() const;

    /** Returns value cast to choice. */
    ChoiceData toChoice() const;

protected:

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child item with @a iIndex. */
    virtual QITableViewCell *childItem(int iIndex) const /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Holds the row value. */
    CFormValue  m_comValue;

    /** Holds the value type. */
    KFormValueType  m_enmValueType;

    /** Holds the cell instances. */
    QVector<UIFormEditorCell*>  m_cells;
};


/** QAbstractTableModel subclass used as Form Editor data model. */
class UIFormEditorModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    /** Constructs Form Editor model passing @a pParent to the base-class. */
    UIFormEditorModel(QITableView *pParent);
    /** Destructs Port Forwarding model. */
    virtual ~UIFormEditorModel() /* override */;

    /** Defines form @a values. */
    void setFormValues(const CFormValueVector &values);

    /** Returns the number of children. */
    int childCount() const;
    /** Returns the child item with @a iIndex. */
    QITableViewRow *childItem(int iIndex) const;

    /** Returns flags for item with certain @a index. */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    /** Returns row count of certain @a parent. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /** Returns column count of certain @a parent. */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /** Returns header data for @a iSection, @a enmOrientation and @a iRole specified. */
    QVariant headerData(int iSection, Qt::Orientation enmOrientation, int iRole) const;

    /** Defines the @a iRole data for item with @a index as @a value. */
    bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole);
    /** Returns the @a iRole data for item with @a index. */
    QVariant data(const QModelIndex &index, int iRole) const;

private:

    /** Return the parent table-view reference. */
    QITableView *parentTable() const;

    /** Holds the Form Editor row list. */
    QList<UIFormEditorRow*>  m_dataList;
};


/** QITableView extension used as Form Editor table-view. */
class UIFormEditorView : public QITableView
{
    Q_OBJECT;

public:

    /** Constructs Form Editor table-view. */
    UIFormEditorView(QWidget *pParent = 0);

protected:

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child item with @a iIndex. */
    virtual QITableViewRow *childItem(int iIndex) const /* override */;
};


/*********************************************************************************************************************************
*   Class UIFormEditorCell implementation.                                                                                       *
*********************************************************************************************************************************/

UIFormEditorCell::UIFormEditorCell(QITableViewRow *pParent, const QString &strText)
    : QITableViewCell(pParent)
    , m_strText(strText)
{
}


/*********************************************************************************************************************************
*   Class UIFormEditorRow implementation.                                                                                        *
*********************************************************************************************************************************/

UIFormEditorRow::UIFormEditorRow(QITableView *pParent, const CFormValue &comValue)
    : QITableViewRow(pParent)
    , m_comValue(comValue)
    , m_enmValueType(KFormValueType_Max)
{
    prepare();
}

UIFormEditorRow::~UIFormEditorRow()
{
    cleanup();
}

QString UIFormEditorRow::nameToString() const
{
    return m_cells.at(UIFormEditorDataType_Name)->text();
}

QString UIFormEditorRow::valueToString() const
{
    return m_cells.at(UIFormEditorDataType_Value)->text();
}

bool UIFormEditorRow::toBool() const
{
    AssertReturn(valueType() == KFormValueType_Boolean, false);
    CBooleanFormValue comValue(m_comValue);
    return comValue.GetSelected();
}

QString UIFormEditorRow::toString() const
{
    AssertReturn(valueType() == KFormValueType_String, QString());
    CStringFormValue comValue(m_comValue);
    return comValue.GetString();
}

ChoiceData UIFormEditorRow::toChoice() const
{
    AssertReturn(valueType() == KFormValueType_Choice, ChoiceData());
    CChoiceFormValue comValue(m_comValue);
    return ChoiceData(comValue.GetValues(), comValue.GetSelectedIndex());
}

int UIFormEditorRow::childCount() const
{
    /* Return cell count: */
    return UIFormEditorDataType_Max;
}

QITableViewCell *UIFormEditorRow::childItem(int iIndex) const
{
    /* Make sure index within the bounds: */
    AssertReturn(iIndex >= 0 && iIndex < m_cells.size(), 0);
    /* Return corresponding cell: */
    return m_cells.at(iIndex);
}

void UIFormEditorRow::prepare()
{
    /* Cache value type: */
    m_enmValueType = m_comValue.GetType();

    /* Create cells on the basis of variables we have: */
    m_cells.resize(UIFormEditorDataType_Max);
    m_cells[UIFormEditorDataType_Name] = new UIFormEditorCell(this, m_comValue.GetLabel());
    switch (m_enmValueType)
    {
        case KFormValueType_Boolean:
        {
            CBooleanFormValue comValue(m_comValue);
            m_cells[UIFormEditorDataType_Value] = new UIFormEditorCell(this, comValue.GetSelected() ? "True" : "False");
            break;
        }
        case KFormValueType_String:
        {
            CStringFormValue comValue(m_comValue);
            m_cells[UIFormEditorDataType_Value] = new UIFormEditorCell(this, comValue.GetString());
            break;
        }
        case KFormValueType_Choice:
        {
            CChoiceFormValue comValue(m_comValue);
            const QVector<QString> values = comValue.GetValues();
            m_cells[UIFormEditorDataType_Value] = new UIFormEditorCell(this, values.at(comValue.GetSelectedIndex()));
            break;
        }
        default:
            break;
    }
}

void UIFormEditorRow::cleanup()
{
    /* Destroy cells: */
    qDeleteAll(m_cells);
    m_cells.clear();
}


/*********************************************************************************************************************************
*   Class UIFormEditorModel implementation.                                                                                      *
*********************************************************************************************************************************/

UIFormEditorModel::UIFormEditorModel(QITableView *pParent)
    : QAbstractTableModel(pParent)
{
}

UIFormEditorModel::~UIFormEditorModel()
{
    /* Delete the cached data: */
    qDeleteAll(m_dataList);
    m_dataList.clear();
}

void UIFormEditorModel::setFormValues(const CFormValueVector &values)
{
    /* Delete old lines: */
    beginRemoveRows(QModelIndex(), 0, m_dataList.size());
    qDeleteAll(m_dataList);
    m_dataList.clear();
    endRemoveRows();

    /* Add new lines: */
    beginInsertRows(QModelIndex(), 0, values.size());
    foreach (const CFormValue &comValue, values)
        m_dataList << new UIFormEditorRow(parentTable(), comValue);
    endInsertRows();
}

int UIFormEditorModel::childCount() const
{
    return rowCount();
}

QITableViewRow *UIFormEditorModel::childItem(int iIndex) const
{
    /* Make sure index within the bounds: */
    AssertReturn(iIndex >= 0 && iIndex < m_dataList.size(), 0);
    /* Return corresponding row: */
    return m_dataList[iIndex];
}

Qt::ItemFlags UIFormEditorModel::flags(const QModelIndex &index) const
{
    /* Check index validness: */
    if (!index.isValid())
        return Qt::NoItemFlags;
    /* Switch for different columns: */
    switch (index.column())
    {
        case UIFormEditorDataType_Name:
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        case UIFormEditorDataType_Value:
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        default:
            return Qt::NoItemFlags;
    }
}

int UIFormEditorModel::rowCount(const QModelIndex &) const
{
    return m_dataList.size();
}

int UIFormEditorModel::columnCount(const QModelIndex &) const
{
    return UIFormEditorDataType_Max;
}

QVariant UIFormEditorModel::headerData(int iSection, Qt::Orientation enmOrientation, int iRole) const
{
    /* Check argument validness: */
    if (iRole != Qt::DisplayRole || enmOrientation != Qt::Horizontal)
        return QVariant();
    /* Switch for different columns: */
    switch (iSection)
    {
        case UIFormEditorDataType_Name:
            return UIFormEditorWidget::tr("Name");
        case UIFormEditorDataType_Value:
            return UIFormEditorWidget::tr("Value");
        default:
            return QVariant();
    }
}

bool UIFormEditorModel::setData(const QModelIndex &index, const QVariant &value, int iRole /* = Qt::EditRole */)
{
    Q_UNUSED(value);

    /* Check index validness: */
    if (!index.isValid() || iRole != Qt::EditRole)
        return false;
    /* Return wrong value: */
    return false;
}

QVariant UIFormEditorModel::data(const QModelIndex &index, int iRole) const
{
    /* Check index validness: */
    if (!index.isValid())
        return QVariant();
    /* Switch for different roles: */
    switch (iRole)
    {
        /* Checkstate role: */
        case Qt::CheckStateRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIFormEditorDataType_Value:
                    return   m_dataList[index.row()]->valueType() == KFormValueType_Boolean
                           ? m_dataList[index.row()]->toBool()
                           ? Qt::Checked
                           : Qt::Unchecked
                           : QVariant();
                default:
                    return QVariant();
            }
        }
        /* Display role: */
        case Qt::DisplayRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIFormEditorDataType_Name:
                    return m_dataList[index.row()]->nameToString();
                case UIFormEditorDataType_Value:
                    return   m_dataList[index.row()]->valueType() != KFormValueType_Boolean
                           ? m_dataList[index.row()]->valueToString()
                           : QVariant();
                default:
                    return QVariant();
            }
        }
        /* Edit role: */
        case Qt::EditRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIFormEditorDataType_Value:
                {
                    switch (m_dataList[index.row()]->valueType())
                    {
                        case KFormValueType_String:
                            return QVariant::fromValue(m_dataList[index.row()]->toString());
                        case KFormValueType_Choice:
                            return QVariant::fromValue(m_dataList[index.row()]->toChoice());
                        default:
                            return QVariant();
                    }
                }
                default:
                    return QVariant();
            }
        }
        /* Alignment role: */
        case Qt::TextAlignmentRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIFormEditorDataType_Name:
                    return (int)(Qt::AlignLeft | Qt::AlignVCenter);
                case UIFormEditorDataType_Value:
                    return   m_dataList[index.row()]->valueType() != KFormValueType_Boolean
                           ? (int)(Qt::AlignLeft | Qt::AlignVCenter)
                           : (int)(Qt::AlignCenter);
                default:
                    return QVariant();
            }
        }
        default:
            return QVariant();
    }
}

QITableView *UIFormEditorModel::parentTable() const
{
    return qobject_cast<QITableView*>(parent());
}


/*********************************************************************************************************************************
*   Class UIFormEditorView implementation.                                                                                       *
*********************************************************************************************************************************/

UIFormEditorView::UIFormEditorView(QWidget * /* pParent = 0 */)
{
}

int UIFormEditorView::childCount() const
{
    /* Redirect request to model: */
    AssertPtrReturn(model(), 0);
    return qobject_cast<UIFormEditorModel*>(model())->childCount();
}

QITableViewRow *UIFormEditorView::childItem(int iIndex) const
{
    /* Redirect request to model: */
    AssertPtrReturn(model(), 0);
    return qobject_cast<UIFormEditorModel*>(model())->childItem(iIndex);
}


/*********************************************************************************************************************************
*   Class UIFormEditorWidget implementation.                                                                                     *
*********************************************************************************************************************************/

UIFormEditorWidget::UIFormEditorWidget(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pTableView(0)
    , m_pTableModel(0)
{
    prepare();
}

void UIFormEditorWidget::setVirtualSystemDescriptionForm(const CVirtualSystemDescriptionForm &comForm)
{
    AssertPtrReturnVoid(m_pTableModel);
    /// @todo add some check..
    m_pTableModel->setFormValues(comForm.GetValues());
    adjustTable();
}

bool UIFormEditorWidget::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Process events for table only: */
    if (pObject != m_pTableView)
        return QWidget::eventFilter(pObject, pEvent);

    /* Process different event-types: */
    switch (pEvent->type())
    {
        case QEvent::Show:
        case QEvent::Resize:
        {
            /* Adjust table: */
            adjustTable();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

void UIFormEditorWidget::prepare()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create view: */
        m_pTableView = new UIFormEditorView(this);
        if (m_pTableView)
        {
            m_pTableView->setTabKeyNavigation(false);
            m_pTableView->verticalHeader()->hide();
            m_pTableView->verticalHeader()->setDefaultSectionSize((int)(m_pTableView->verticalHeader()->minimumSectionSize() * 1.33));
            m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
            m_pTableView->installEventFilter(this);

            /* Create model: */
            m_pTableModel = new UIFormEditorModel(m_pTableView);
            if (m_pTableModel)
                m_pTableView->setModel(m_pTableModel);

            /* We certainly have abstract item delegate: */
            QAbstractItemDelegate *pAbstractItemDelegate = m_pTableView->itemDelegate();
            if (pAbstractItemDelegate)
            {
                /* But is this also styled item delegate? */
                QStyledItemDelegate *pStyledItemDelegate = qobject_cast<QStyledItemDelegate*>(pAbstractItemDelegate);
                if (pStyledItemDelegate)
                {
                    /* Create new item editor factory: */
                    QItemEditorFactory *pNewItemEditorFactory = new QItemEditorFactory;
                    if (pNewItemEditorFactory)
                    {
                        /* Register ChoiceEditor as the ChoiceData editor: */
                        int iChoiceId = qRegisterMetaType<ChoiceData>();
                        QStandardItemEditorCreator<ChoiceEditor> *pChoiceEditorItemCreator = new QStandardItemEditorCreator<ChoiceEditor>();
                        pNewItemEditorFactory->registerEditor((QVariant::Type)iChoiceId, pChoiceEditorItemCreator);

                        /* Set newly created item editor factory for table delegate: */
                        pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);
                    }
                }
            }

            /* Add into layout: */
            pLayout->addWidget(m_pTableView);
        }
    }
}

void UIFormEditorWidget::adjustTable()
{
    m_pTableView->horizontalHeader()->setStretchLastSection(false);
    /* If table is NOT empty: */
    if (m_pTableModel->rowCount())
    {
        /* Resize table to contents size-hint and emit a spare place for first column: */
        m_pTableView->resizeColumnsToContents();
        const int iFullWidth = m_pTableView->viewport()->width();
        const int iNameWidth = m_pTableView->horizontalHeader()->sectionSize(UIFormEditorDataType_Name);
        const int iValueWidth = qMax(0, iFullWidth - iNameWidth);
        m_pTableView->horizontalHeader()->resizeSection(UIFormEditorDataType_Value, iValueWidth);
    }
    /* If table is empty: */
    else
    {
        /* Resize table columns to be equal in size: */
        const int iFullWidth = m_pTableView->viewport()->width();
        m_pTableView->horizontalHeader()->resizeSection(UIFormEditorDataType_Name, iFullWidth / 2);
        m_pTableView->horizontalHeader()->resizeSection(UIFormEditorDataType_Value, iFullWidth / 2);
    }
    m_pTableView->horizontalHeader()->setStretchLastSection(true);
}


#include "UIFormEditorWidget.moc"
