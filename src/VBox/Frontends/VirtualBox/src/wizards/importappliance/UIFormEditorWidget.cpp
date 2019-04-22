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
#include <QSortFilterProxyModel>
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


/** Class used to hold choice data. */
class ChoiceData
{
public:

    /** Constructs null choice data. */
    ChoiceData() {}
    /** Constructs choice data on the basis of passed @a values and @a iSelectedIndex. */
    ChoiceData(const QVector<QString> &values, int iSelectedIndex)
        : m_values(values), m_iSelectedIndex(iSelectedIndex) {}
    /** Constructs choice data on the basis of another @a choice data. */
    ChoiceData(const ChoiceData &choice)
        : m_values(choice.values()), m_iSelectedIndex(choice.selectedIndex()) {}

    /** Assigns values of @a other choice to this one. */
    ChoiceData &operator=(const ChoiceData &other)
    {
        m_values = other.values();
        m_iSelectedIndex = other.selectedIndex();
        return *this;
    }

    /** Returns values vector. */
    QVector<QString> values() const { return m_values; }
    /** Returns selected index. */
    int selectedIndex() const { return m_iSelectedIndex; }
    /** Returns selected value. */
    QString selectedValue() const
    {
        return   m_iSelectedIndex >= 0 && m_iSelectedIndex < m_values.size()
               ? m_values.at(m_iSelectedIndex) : QString();
    }

private:

    /** Holds values vector. */
    QVector<QString>  m_values;
    /** Holds selected index. */
    int               m_iSelectedIndex;
};
Q_DECLARE_METATYPE(ChoiceData);


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
    void setChoice(const ChoiceData &choice)
    {
        addItems(choice.values().toList());
        setCurrentIndex(choice.selectedIndex());
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
    UIFormEditorCell(QITableViewRow *pParent, const QString &strText = QString());

    /** Returns the cell text. */
    virtual QString text() const /* override */ { return m_strText; }

    /** Defines the cell @a strText. */
    void setText(const QString &strText) { m_strText = strText; }

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

    /** Returns whether the row is visible. */
    bool isVisible() const;

    /** Returns value cast to bool. */
    bool toBool() const;
    /** Defines @a fBool value. */
    void setBool(bool fBool);

    /** Returns value cast to string. */
    QString toString() const;
    /** Defines @a strString value. */
    void setString(const QString &strString);

    /** Returns value cast to choice. */
    ChoiceData toChoice() const;
    /** Defines @a choice value. */
    void setChoice(const ChoiceData &choice);

    /** Updates value cells. */
    void updateValueCells();

    /** Check whether generation value is changed. */
    bool isGenerationChanged() const;

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

    /** Holds current generation value. */
    int  m_iGeneration;

    /** Holds cached bool value. */
    bool        m_fBool;
    /** Holds cached string value. */
    QString     m_strString;
    /** Holds cached choice value. */
    ChoiceData  m_choice;

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

    /** Returns the index of the item in the model specified by the given @a iRow, @a iColumn and @a parentIdx. */
    virtual QModelIndex index(int iRow, int iColumn, const QModelIndex &parentIdx = QModelIndex()) const /* override */;

    /** Returns flags for item with certain @a index. */
    virtual Qt::ItemFlags flags(const QModelIndex &index) const /* override */;

    /** Returns row count of certain @a parent. */
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    /** Returns column count of certain @a parent. */
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;

    /** Returns header data for @a iSection, @a enmOrientation and @a iRole specified. */
    virtual QVariant headerData(int iSection, Qt::Orientation enmOrientation, int iRole) const /* override */;

    /** Defines the @a iRole data for item with @a index as @a value. */
    virtual bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole) /* override */;
    /** Returns the @a iRole data for item with @a index. */
    virtual QVariant data(const QModelIndex &index, int iRole) const /* override */;

private:

    /** Return the parent table-view reference. */
    QITableView *parentTable() const;

    /** Updates row generation values. */
    void updateGeneration();

    /** Holds the Form Editor row list. */
    QList<UIFormEditorRow*>  m_dataList;
};


/** QSortFilterProxyModel subclass used as the Form Editor proxy-model. */
class UIFormEditorProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT;

public:

    /** Constructs the Form Editor proxy-model passing @a pParent to the base-class. */
    UIFormEditorProxyModel(QObject *pParent = 0);

protected:

    /** Returns whether item in the row indicated by the given @a iSourceRow and @a srcParenIdx should be included in the model. */
    virtual bool filterAcceptsRow(int iSourceRow, const QModelIndex &srcParenIdx) const /* override */;
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

UIFormEditorCell::UIFormEditorCell(QITableViewRow *pParent, const QString &strText /* = QString() */)
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
    , m_iGeneration(0)
    , m_fBool(false)
    , m_strString(QString())
    , m_choice(ChoiceData())
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

bool UIFormEditorRow::isVisible() const
{
    return m_comValue.GetVisible();
}

bool UIFormEditorRow::toBool() const
{
    AssertReturn(valueType() == KFormValueType_Boolean, false);
    return m_fBool;
}

void UIFormEditorRow::setBool(bool fBool)
{
    AssertReturnVoid(valueType() == KFormValueType_Boolean);
    CBooleanFormValue comValue(m_comValue);
    CProgress comProgress = comValue.SetSelected(fBool);

    /* Show error message if necessary: */
    if (!comValue.isOk())
        msgCenter().cannotAssignFormValue(comValue);
    else
    {
        /* Show "Acquire export form" progress: */
        msgCenter().showModalProgressDialog(comProgress, UIFormEditorWidget::tr("Assign value..."),
                                            ":/progress_reading_appliance_90px.png");

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAssignFormValue(comProgress);
        else
            updateValueCells();
    }
}

QString UIFormEditorRow::toString() const
{
    AssertReturn(valueType() == KFormValueType_String, QString());
    return m_strString;
}

void UIFormEditorRow::setString(const QString &strString)
{
    AssertReturnVoid(valueType() == KFormValueType_String);
    CStringFormValue comValue(m_comValue);
    CProgress comProgress = comValue.SetString(strString);

    /* Show error message if necessary: */
    if (!comValue.isOk())
        msgCenter().cannotAssignFormValue(comValue);
    else
    {
        /* Show "Acquire export form" progress: */
        msgCenter().showModalProgressDialog(comProgress, UIFormEditorWidget::tr("Assign value..."),
                                            ":/progress_reading_appliance_90px.png");

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAssignFormValue(comProgress);
        else
            updateValueCells();
    }
}

ChoiceData UIFormEditorRow::toChoice() const
{
    AssertReturn(valueType() == KFormValueType_Choice, ChoiceData());
    return m_choice;
}

void UIFormEditorRow::setChoice(const ChoiceData &choice)
{
    /* Do nothing for empty choices: */
    if (choice.selectedIndex() == -1)
        return;

    AssertReturnVoid(valueType() == KFormValueType_Choice);
    CChoiceFormValue comValue(m_comValue);
    CProgress comProgress = comValue.SetSelectedIndex(choice.selectedIndex());

    /* Show error message if necessary: */
    if (!comValue.isOk())
        msgCenter().cannotAssignFormValue(comValue);
    else
    {
        /* Show "Acquire export form" progress: */
        msgCenter().showModalProgressDialog(comProgress, UIFormEditorWidget::tr("Assign value..."),
                                            ":/progress_reading_appliance_90px.png");

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAssignFormValue(comProgress);
        else
            updateValueCells();
    }
}

void UIFormEditorRow::updateValueCells()
{
    m_iGeneration = m_comValue.GetGeneration();
    /// @todo check for errors

    switch (m_enmValueType)
    {
        case KFormValueType_Boolean:
        {
            CBooleanFormValue comValue(m_comValue);
            m_fBool = comValue.GetSelected();
            m_cells[UIFormEditorDataType_Value]->setText(m_fBool ? "True" : "False");
            /// @todo check for errors
            break;
        }
        case KFormValueType_String:
        {
            CStringFormValue comValue(m_comValue);
            m_strString = comValue.GetString();
            m_cells[UIFormEditorDataType_Value]->setText(m_strString);
            /// @todo check for errors
            break;
        }
        case KFormValueType_Choice:
        {
            CChoiceFormValue comValue(m_comValue);
            const QVector<QString> values = comValue.GetValues();
            const int iSelectedIndex = comValue.GetSelectedIndex();
            m_choice = ChoiceData(values, iSelectedIndex);
            m_cells[UIFormEditorDataType_Value]->setText(m_choice.selectedValue());
            /// @todo check for errors
            break;
        }
        default:
            break;
    }
}

bool UIFormEditorRow::isGenerationChanged() const
{
    const int iGeneration = m_comValue.GetGeneration();
    /// @todo check for errors
    return m_iGeneration != iGeneration;
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
    /// @todo check for errors

    /* Create cells on the basis of variables we have: */
    m_cells.resize(UIFormEditorDataType_Max);
    const QString strName = m_comValue.GetLabel();
    /// @todo check for errors
    m_cells[UIFormEditorDataType_Name] = new UIFormEditorCell(this, strName);
    m_cells[UIFormEditorDataType_Value] = new UIFormEditorCell(this);
    updateValueCells();
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
    beginInsertRows(QModelIndex(), 0, values.size() - 1);
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

QModelIndex UIFormEditorModel::index(int iRow, int iColumn, const QModelIndex &parentIdx /* = QModelIndex() */) const
{
    /* No index for unknown items: */
    if (!hasIndex(iRow, iColumn, parentIdx))
        return QModelIndex();

    /* Provide index users with packed item pointer: */
    UIFormEditorRow *pItem = iRow >= 0 && iRow < m_dataList.size() ? m_dataList.at(iRow) : 0;
    return pItem ? createIndex(iRow, iColumn, pItem) : QModelIndex();
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
            return   m_dataList[index.row()]->valueType() != KFormValueType_Boolean
                   ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
                   : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
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
    /* Check index validness: */
    if (!index.isValid())
        return false;
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
                {
                    if (m_dataList[index.row()]->valueType() == KFormValueType_Boolean)
                    {
                        const Qt::CheckState enmCheckState = static_cast<Qt::CheckState>(value.toInt());
                        m_dataList[index.row()]->setBool(enmCheckState == Qt::Checked);
                        emit dataChanged(index, index);
                        updateGeneration();
                        return true;
                    }
                    else
                        return false;
                }
                default:
                    return false;
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
                            m_dataList[index.row()]->setString(value.toString());
                            emit dataChanged(index, index);
                            updateGeneration();
                            return true;
                        case KFormValueType_Choice:
                            m_dataList[index.row()]->setChoice(value.value<ChoiceData>());
                            emit dataChanged(index, index);
                            updateGeneration();
                            return true;
                        default:
                            return false;
                    }
                }
                default:
                    return false;
            }
        }
        default:
            return false;
    }
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

void UIFormEditorModel::updateGeneration()
{
    for (int i = 0; i < m_dataList.size(); ++i)
    {
        UIFormEditorRow *pRow = m_dataList.at(i);
        if (pRow->isGenerationChanged())
        {
            pRow->updateValueCells();
            const QModelIndex changedIndex = index(i, 1);
            emit dataChanged(changedIndex, changedIndex);
        }
    }
}


/*********************************************************************************************************************************
*   Class UIFormEditorProxyModel implementation.                                                                                 *
*********************************************************************************************************************************/

UIFormEditorProxyModel::UIFormEditorProxyModel(QObject *pParent /* = 0 */)
    : QSortFilterProxyModel(pParent)
{
}

bool UIFormEditorProxyModel::filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const
{
    /* Acquire actual index of source model: */
    QModelIndex i = sourceModel()->index(iSourceRow, 0, sourceParent);
    if (i.isValid())
    {
        /* Get packed item pointer: */
        UIFormEditorRow *pItem = static_cast<UIFormEditorRow*>(i.internalPointer());
        /* Filter invisible items: */
        if (!pItem->isVisible())
            return false;
    }
    return true;
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
            {
                /* Create proxy-model: */
                UIFormEditorProxyModel *pProxyModel = new UIFormEditorProxyModel(m_pTableView);
                if (pProxyModel)
                {
                    pProxyModel->setSourceModel(m_pTableModel);
                    m_pTableView->setModel(pProxyModel);
                }
            }

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
