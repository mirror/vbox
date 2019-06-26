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
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialog.h"
#include "QIDialogButtonBox.h"
#include "QITableView.h"
#include "QIWithRetranslateUI.h"
#include "UIFormEditorWidget.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CBooleanFormValue.h"
#include "CChoiceFormValue.h"
#include "CFormValue.h"
#include "CRangedIntegerFormValue.h"
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


/** Class used to hold text data. */
class TextData
{
public:

    /** Constructs null text data. */
    TextData() {}
    /** Constructs text data on the basis of passed @a strText and @a index. */
    TextData(const QString &strText, const QModelIndex index = QModelIndex())
        : m_strText(strText), m_index(index) {}
    /** Constructs text data on the basis of @a another text data. */
    TextData(const TextData &another)
        : m_strText(another.text()), m_index(another.index()) {}

    /** Assigns values of @a another text to this one. */
    TextData &operator=(const TextData &another)
    {
        m_strText = another.text();
        m_index = another.index();
        return *this;
    }

    /** Returns text value. */
    QString text() const { return m_strText; }

    /** Defines model @a index. */
    void setIndex(const QModelIndex &index) { m_index = index; }
    /** Returns model index. */
    QModelIndex index() const { return m_index; }

private:

    /** Holds text value. */
    QString      m_strText;
    /** Holds model index. */
    QModelIndex  m_index;
};
Q_DECLARE_METATYPE(TextData);


/** Class used to hold choice data. */
class ChoiceData
{
public:

    /** Constructs null choice data. */
    ChoiceData()
        : m_iSelectedIndex(-1) {}
    /** Constructs choice data on the basis of passed @a values and @a iSelectedIndex. */
    ChoiceData(const QVector<QString> &values, int iSelectedIndex)
        : m_values(values), m_iSelectedIndex(iSelectedIndex) {}
    /** Constructs choice data on the basis of @a another choice data. */
    ChoiceData(const ChoiceData &another)
        : m_values(another.values()), m_iSelectedIndex(another.selectedIndex()) {}

    /** Assigns values of @a another choice to this one. */
    ChoiceData &operator=(const ChoiceData &another)
    {
        m_values = another.values();
        m_iSelectedIndex = another.selectedIndex();
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


/** Class used to hold ranged-integer data. */
class RangedIntegerData
{
public:

    /** Constructs null ranged-integer data. */
    RangedIntegerData()
        : m_iMinimum(-1), m_iMaximum(-1)
        , m_iInteger(-1), m_strSuffix(QString()) {}
    /** Constructs ranged-integer data on the basis of passed @a iMinimum, @a iMaximum, @a iInteger and @a strSuffix. */
    RangedIntegerData(int iMinimum, int iMaximum, int iInteger, const QString strSuffix)
        : m_iMinimum(iMinimum), m_iMaximum(iMaximum)
        , m_iInteger(iInteger), m_strSuffix(strSuffix) {}
    /** Constructs ranged-integer data on the basis of @a another ranged-integer data. */
    RangedIntegerData(const RangedIntegerData &another)
        : m_iMinimum(another.minimum()), m_iMaximum(another.maximum())
        , m_iInteger(another.integer()), m_strSuffix(another.suffix()) {}

    /** Assigns values of @a another ranged-integer to this one. */
    RangedIntegerData &operator=(const RangedIntegerData &another)
    {
        m_iMinimum = another.minimum();
        m_iMaximum = another.maximum();
        m_iInteger = another.integer();
        m_strSuffix = another.suffix();
        return *this;
    }

    /** Returns minimum value. */
    int minimum() const { return m_iMinimum; }
    /** Returns maximum value. */
    int maximum() const { return m_iMaximum; }
    /** Returns current value. */
    int integer() const { return m_iInteger; }
    /** Returns suffix value. */
    QString suffix() const { return m_strSuffix; }

private:

    /** Holds minimum value. */
    int      m_iMinimum;
    /** Holds maximum value. */
    int      m_iMaximum;
    /** Holds current value. */
    int      m_iInteger;
    /** Holds suffix value. */
    QString  m_strSuffix;
};
Q_DECLARE_METATYPE(RangedIntegerData);


/** QWidget extension used as dummy TextData editor.
  * It's not actually an editor, but Edit... button instead which opens
  * real editor passing stored model index received from TextData value. */
class TextEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(TextData text READ text WRITE setText USER true);

public:

    /** Constructs TextData editor passing @a pParent to the base-class. */
    TextEditor(QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles button click. */
    void sltHandleButtonClick();

private:

    /** Prepares all. */
    void prepare();

    /** Defines @a text. */
    void setText(const TextData &text);
    /** Returns text. */
    TextData text() const;

    /** Holds the button instance. */
    QPushButton *m_pButton;
    /** Holds the multiline text. */
    QString      m_strMultilineText;
    /** Holds the model index. */
    QModelIndex  m_index;
};


/** QComboBox extension used as ChoiceData editor. */
class ChoiceEditor : public QComboBox
{
    Q_OBJECT;
    Q_PROPERTY(ChoiceData choice READ choice WRITE setChoice USER true);

public:

    /** Constructs ChoiceData editor passing @a pParent to the base-class. */
    ChoiceEditor(QWidget *pParent = 0);

private:

    /** Defines the @a choice. */
    void setChoice(const ChoiceData &choice);
    /** Returns the choice. */
    ChoiceData choice() const;
};


/** QSpinBox extension used as RangedIntegerData editor. */
class RangedIntegerEditor : public QSpinBox
{
    Q_OBJECT;
    Q_PROPERTY(RangedIntegerData rangedInteger READ rangedInteger WRITE setRangedInteger USER true);

public:

    /** Constructs RangedIntegerData editor passing @a pParent to the base-class. */
    RangedIntegerEditor(QWidget *pParent = 0);

private:

    /** Defines @a rangedInteger. */
    void setRangedInteger(const RangedIntegerData &rangedInteger);
    /** Returns ranged-integer. */
    RangedIntegerData rangedInteger() const;

    /** Holds the unchanged suffix. */
    QString  m_strSuffix;
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

    /** Returns whether cached string value is multiline. */
    bool isMultilineString() const;
    /** Returns value cast to text. */
    TextData toText() const;
    /** Defines @a text value. */
    void setText(const TextData &text);
    /** Returns value cast to string. */
    QString toString() const;
    /** Defines @a strString value. */
    void setString(const QString &strString);

    /** Returns value cast to choice. */
    ChoiceData toChoice() const;
    /** Defines @a choice value. */
    void setChoice(const ChoiceData &choice);

    /** Returns value cast to ranged-integer. */
    RangedIntegerData toRangedInteger() const;
    /** Defines @a rangedInteger value. */
    void setRangedInteger(const RangedIntegerData &rangedInteger);

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
    bool               m_fBool;
    /** Holds whether cached string value is multiline. */
    bool               m_fMultilineString;
    /** Holds cached text value. */
    TextData           m_text;
    /** Holds cached string value. */
    QString            m_strString;
    /** Holds cached choice value. */
    ChoiceData         m_choice;
    /** Holds cached ranged-integer value. */
    RangedIntegerData  m_rangedInteger;

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

    /** Creates actual TextData editor for specified @a index. */
    void createTextDataEditor(const QModelIndex &index);

private:

    /** Prepares all. */
    void prepare();

    /** Return the parent table-view reference. */
    QITableView *parentTable() const;

    /** Updates row generation values. */
    void updateGeneration();

    /** Returns icon hint for specified @a strItemName. */
    QIcon iconHint(const QString &strItemName) const;

    /** Holds the Form Editor row list. */
    QList<UIFormEditorRow*>  m_dataList;

    /** Holds the hardcoded icon name map. */
    QMap<QString, QIcon>  m_icons;
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
*   Class TextEditor implementation.                                                                                             *
*********************************************************************************************************************************/

TextEditor::TextEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pButton(0)
{
    prepare();
}

void TextEditor::retranslateUi()
{
    m_pButton->setText(UIFormEditorWidget::tr("Edit..."));
}

void TextEditor::sltHandleButtonClick()
{
    /* Redirect the edit call if possible: */
    do
    {
        /* Get the view: */
        if (   !parent()
            || !parent()->parent())
            break;
        UIFormEditorView *pView = qobject_cast<UIFormEditorView*>(parent()->parent());

        /* Get the proxy model: */
        if (   !pView
            || !pView->model())
            break;
        UIFormEditorProxyModel *pProxyModel = qobject_cast<UIFormEditorProxyModel*>(pView->model());

        /* Get the source model: */
        if (   !pProxyModel
            || !pProxyModel->sourceModel())
            break;
        UIFormEditorModel *pSourceModel = qobject_cast<UIFormEditorModel*>(pProxyModel->sourceModel());

        /* Execute the call: */
        if (!pSourceModel)
            break;
        pSourceModel->createTextDataEditor(m_index);
    }
    while (0);
}

void TextEditor::prepare()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0 ,0);
        /* Create button: */
        m_pButton = new QPushButton(this);
        if (m_pButton)
        {
            connect(m_pButton, &QPushButton::clicked, this, &TextEditor::sltHandleButtonClick);
            pLayout->addWidget(m_pButton);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void TextEditor::setText(const TextData &text)
{
    m_strMultilineText = text.text();
    m_index = text.index();
}

TextData TextEditor::text() const
{
    return TextData(m_strMultilineText, m_index);
}


/*********************************************************************************************************************************
*   Class ChoiceEditor implementation.                                                                                           *
*********************************************************************************************************************************/

ChoiceEditor::ChoiceEditor(QWidget *pParent /* = 0 */)
    : QComboBox(pParent)
{
}

void ChoiceEditor::setChoice(const ChoiceData &choice)
{
    addItems(choice.values().toList());
    setCurrentIndex(choice.selectedIndex());
}

ChoiceData ChoiceEditor::choice() const
{
    QVector<QString> choices(count());
    for (int i = 0; i < count(); ++i)
        choices[i] = itemText(i);
    return ChoiceData(choices, currentIndex());
}


/*********************************************************************************************************************************
*   Class RangedIntegerEditor implementation.                                                                                    *
*********************************************************************************************************************************/

RangedIntegerEditor::RangedIntegerEditor(QWidget *pParent /* = 0 */)
    : QSpinBox(pParent)
{
}

void RangedIntegerEditor::setRangedInteger(const RangedIntegerData &rangedInteger)
{
    setMinimum(rangedInteger.minimum());
    setMaximum(rangedInteger.maximum());
    setValue(rangedInteger.integer());
    m_strSuffix = rangedInteger.suffix();
    setSuffix(m_strSuffix.isEmpty() ? QString() :
              QString(" %1").arg(QApplication::translate("UICommon", m_strSuffix.toUtf8().constData())));
}

RangedIntegerData RangedIntegerEditor::rangedInteger() const
{
    return RangedIntegerData(minimum(), maximum(), value(), m_strSuffix);
}


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
    , m_fMultilineString(false)
    , m_text(TextData())
    , m_strString(QString())
    , m_choice(ChoiceData())
    , m_rangedInteger(RangedIntegerData())
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
                                            ":/progress_reading_appliance_90px.png",
                                            0 /* parent */, 0 /* duration */);

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAssignFormValue(comProgress);
        else
            updateValueCells();
    }
}

bool UIFormEditorRow::isMultilineString() const
{
    AssertReturn(valueType() == KFormValueType_String, false);
    return m_fMultilineString;
}

TextData UIFormEditorRow::toText() const
{
    AssertReturn(valueType() == KFormValueType_String, TextData());
    return m_text;
}

void UIFormEditorRow::setText(const TextData &text)
{
    AssertReturnVoid(valueType() == KFormValueType_String);
    CStringFormValue comValue(m_comValue);
    CProgress comProgress = comValue.SetString(text.text());

    /* Show error message if necessary: */
    if (!comValue.isOk())
        msgCenter().cannotAssignFormValue(comValue);
    else
    {
        /* Show "Acquire export form" progress: */
        msgCenter().showModalProgressDialog(comProgress, UIFormEditorWidget::tr("Assign value..."),
                                            ":/progress_reading_appliance_90px.png",
                                            0 /* parent */, 0 /* duration */);

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
                                            ":/progress_reading_appliance_90px.png",
                                            0 /* parent */, 0 /* duration */);

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
                                            ":/progress_reading_appliance_90px.png",
                                            0 /* parent */, 0 /* duration */);

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAssignFormValue(comProgress);
        else
            updateValueCells();
    }
}

RangedIntegerData UIFormEditorRow::toRangedInteger() const
{
    AssertReturn(valueType() == KFormValueType_RangedInteger, RangedIntegerData());
    return m_rangedInteger;
}

void UIFormEditorRow::setRangedInteger(const RangedIntegerData &rangedInteger)
{
    AssertReturnVoid(valueType() == KFormValueType_RangedInteger);
    CRangedIntegerFormValue comValue(m_comValue);
    CProgress comProgress = comValue.SetInteger(rangedInteger.integer());

    /* Show error message if necessary: */
    if (!comValue.isOk())
        msgCenter().cannotAssignFormValue(comValue);
    else
    {
        /* Show "Acquire export form" progress: */
        msgCenter().showModalProgressDialog(comProgress, UIFormEditorWidget::tr("Assign value..."),
                                            ":/progress_reading_appliance_90px.png",
                                            0 /* parent */, 0 /* duration */);

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
            m_fMultilineString = comValue.GetMultiline();
            const QString strString = comValue.GetString();
            if (m_fMultilineString)
                m_text = TextData(strString);
            else
                m_strString = strString;
            m_cells[UIFormEditorDataType_Value]->setText(strString);
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
        case KFormValueType_RangedInteger:
        {
            CRangedIntegerFormValue comValue(m_comValue);
            const int iMinimum = comValue.GetMinimum();
            const int iMaximum = comValue.GetMaximum();
            const int iInteger = comValue.GetInteger();
            const QString strSuffix = comValue.GetSuffix();
            m_rangedInteger = RangedIntegerData(iMinimum, iMaximum, iInteger, strSuffix);
            m_cells[UIFormEditorDataType_Value]->setText(  m_rangedInteger.suffix().isEmpty()
                                                         ? QString::number(m_rangedInteger.integer())
                                                         : QString("%1 %2").arg(m_rangedInteger.integer())
                                                                           .arg(m_rangedInteger.suffix()));
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
    prepare();
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
                        {
                            if (value.canConvert<TextData>())
                                m_dataList[index.row()]->setText(value.value<TextData>());
                            else
                                m_dataList[index.row()]->setString(value.toString());
                            emit dataChanged(index, index);
                            updateGeneration();
                            return true;
                        }
                        case KFormValueType_Choice:
                        {
                            m_dataList[index.row()]->setChoice(value.value<ChoiceData>());
                            emit dataChanged(index, index);
                            updateGeneration();
                            return true;
                        }
                        case KFormValueType_RangedInteger:
                        {
                            m_dataList[index.row()]->setRangedInteger(value.value<RangedIntegerData>());
                            emit dataChanged(index, index);
                            updateGeneration();
                            return true;
                        }
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
        /* Decoration role: */
        case Qt::DecorationRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIFormEditorDataType_Name: return iconHint(m_dataList[index.row()]->nameToString());
                default: return QVariant();
            }
        }
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
                        {
                            if (m_dataList[index.row()]->isMultilineString())
                            {
                                TextData td = m_dataList[index.row()]->toText();
                                td.setIndex(index);
                                return QVariant::fromValue(td);
                            }
                            else
                                return QVariant::fromValue(m_dataList[index.row()]->toString());
                        }
                        case KFormValueType_Choice:
                            return QVariant::fromValue(m_dataList[index.row()]->toChoice());
                        case KFormValueType_RangedInteger:
                            return QVariant::fromValue(m_dataList[index.row()]->toRangedInteger());
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

void UIFormEditorModel::createTextDataEditor(const QModelIndex &index)
{
    /* Create dialog on-the-fly: */
    QPointer<QIDialog> pDialog = new QIDialog(parentTable());
    if (pDialog)
    {
        /* We will need that pointer: */
        QTextEdit *pEditor = 0;
        /* Create layout: */
        QVBoxLayout *pLayout = new QVBoxLayout(pDialog);
        if (pLayout)
        {
            /* Create text-editor: */
            pEditor = new QTextEdit;
            if (pEditor)
            {
                const TextData td = data(index, Qt::EditRole).value<TextData>();
                pEditor->setPlainText(td.text());
                pLayout->addWidget(pEditor);
            }
            /* Create button-box: */
            QIDialogButtonBox *pBox = new QIDialogButtonBox;
            if (pBox)
            {
                pBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
                connect(pBox, &QIDialogButtonBox::accepted, pDialog.data(), &QIDialog::accept);
                connect(pBox, &QIDialogButtonBox::rejected, pDialog.data(), &QIDialog::reject);
                pLayout->addWidget(pBox);
            }
        }
        /* Execute the dialog: */
        if (pDialog->execute() == QDialog::Accepted)
        {
            const TextData td = TextData(pEditor->toPlainText(), index);
            setData(index, QVariant::fromValue(td));
        }
        /* Cleanup: */
        delete pDialog;
    }
}

void UIFormEditorModel::prepare()
{
    /* Prepare hardcoded icons map: */
    m_icons["Name"]                = UIIconPool::iconSet(":/name_16px.png");
    m_icons["Display Name"]        = UIIconPool::iconSet(":/name_16px.png");
    m_icons["Type"]                = UIIconPool::iconSet(":/os_type_16px.png");
    m_icons["Version"]             = UIIconPool::iconSet(":/os_version_16px.png");
    m_icons["CPU"]                 = UIIconPool::iconSet(":/cpu_16px.png");
    m_icons["Memory"]              = UIIconPool::iconSet(":/ram_16px.png");
    m_icons["Description"]         = UIIconPool::iconSet(":/description_16px.png");
    m_icons["Bucket"]              = UIIconPool::iconSet(":/bucket_16px.png");
    m_icons["Keep Object"]         = UIIconPool::iconSet(":/keep_object_16px.png");
    m_icons["Launch VM"]           = UIIconPool::iconSet(":/launch_vm_16px.png");
    m_icons["Availability Domain"] = UIIconPool::iconSet(":/availability_domain_16px.png");
    m_icons["Shape"]               = UIIconPool::iconSet(":/shape_16px.png");
    m_icons["Disk Size"]           = UIIconPool::iconSet(":/disk_size_16px.png");
    m_icons["VCN"]                 = UIIconPool::iconSet(":/vcn_16px.png");
    m_icons["Subnet"]              = UIIconPool::iconSet(":/subnet_16px.png");
    m_icons["Assign Public IP"]    = UIIconPool::iconSet(":/assign_public_ip_16px.png");
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

QIcon UIFormEditorModel::iconHint(const QString &strItemName) const
{
    return m_icons.value(strItemName, UIIconPool::iconSet(":/session_info_16px.png"));
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
                        /* Register TextEditor as the TextData editor: */
                        int iTextId = qRegisterMetaType<TextData>();
                        QStandardItemEditorCreator<TextEditor> *pTextEditorItemCreator = new QStandardItemEditorCreator<TextEditor>();
                        pNewItemEditorFactory->registerEditor((QVariant::Type)iTextId, pTextEditorItemCreator);

                        /* Register ChoiceEditor as the ChoiceData editor: */
                        int iChoiceId = qRegisterMetaType<ChoiceData>();
                        QStandardItemEditorCreator<ChoiceEditor> *pChoiceEditorItemCreator = new QStandardItemEditorCreator<ChoiceEditor>();
                        pNewItemEditorFactory->registerEditor((QVariant::Type)iChoiceId, pChoiceEditorItemCreator);

                        /* Register RangedIntegerEditor as the RangedIntegerData editor: */
                        int iRangedIntegerId = qRegisterMetaType<RangedIntegerData>();
                        QStandardItemEditorCreator<RangedIntegerEditor> *pRangedIntegerEditorItemCreator = new QStandardItemEditorCreator<RangedIntegerEditor>();
                        pNewItemEditorFactory->registerEditor((QVariant::Type)iRangedIntegerId, pRangedIntegerEditorItemCreator);

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
