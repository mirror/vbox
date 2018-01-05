/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
# include <QButtonGroup>
# include <QComboBox>
# include <QHBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QLabel>
# include <QLineEdit>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QTextCursor>
# include <QToolButton>
# include <QRadioButton>
# include <QScrollArea>

/* GUI includes: */
# include "UIIconPool.h"
# include "UISpecialControls.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* UIVMFilterLineEdit class is used to display and modify the list of filter terms.
   the terms are displayed as words with spaces in between and it is possible to
   remove these terms one by one by selecting them or completely by the clearAll button
   located on the right side of the line edit: */
class UIVMFilterLineEdit : public QLineEdit
{
    Q_OBJECT;

signals:

    void sigFilterTermRemoved(QString removedString);
    void sigClearAll();

public:

    UIVMFilterLineEdit(QWidget *parent = 0)
        :QLineEdit(parent)
        , m_pRemoveTermButton(0)
    {
        setReadOnly(true);
        home(false);
        createButtons();
    }

    void addFilterTerm(const QString& filterTermString)
    {
        if (text().isEmpty())
            insert(filterTermString);
        else
        {
            QString newString(filterTermString);
            insert(newString.prepend(' '));
        }
    }

    void clearAll()
    {
        if (text().isEmpty())
            return;
        sltClearAll();
    }

protected:

    /* Overload the mouseXXXEvent to control how selection is made: */
    virtual void        mouseDoubleClickEvent(QMouseEvent *){}
    virtual void        mouseMoveEvent(QMouseEvent *){}
    virtual void        mousePressEvent(QMouseEvent * event)
    {
        /* Simulate double mouse click to select a word with a single click. */
        QLineEdit::mouseDoubleClickEvent(event);
    }
    virtual void        mouseReleaseEvent(QMouseEvent *){}
    virtual void paintEvent(QPaintEvent *event)
    {
        QLineEdit::paintEvent(event);
        int clearButtonSize = height();
        m_pClearAllButton->setGeometry(width() - clearButtonSize, 0, clearButtonSize, clearButtonSize);
        if (hasSelectedText())
        {
            m_pRemoveTermButton->show();
            int buttonY = 0.5 * (height() - 16);
            int buttonSize = 16;
            int charWidth = fontMetrics().width('x');

            int buttonLeft = cursorRect().right() - 0.5 * charWidth;

            if (buttonLeft + buttonSize  >=  width() - clearButtonSize)
            {
                int selectionWidth = charWidth * selectedText().length();
                buttonLeft -= (selectionWidth + buttonSize);
            }

            m_pRemoveTermButton->setGeometry(buttonLeft, buttonY, buttonSize, buttonSize);
        }
        else
            m_pRemoveTermButton->hide();
    }

private slots:

    /* Nofifies the listeners that selected word (filter term) has been removed. */
    void sltRemoveFilterTerm()
    {
        if(!hasSelectedText())
            return;
        emit sigFilterTermRemoved(selectedText());
        /* Remove the string from text() including the trailing space: */
        setText(text().remove(selectionStart(), selectedText().length()+1));
    }

    /* The whole content is removed. Listeners are notified. */
    void sltClearAll()
    {
        if(text().isEmpty())
            return;

        clear();
        emit sigClearAll();
    }

private:

    void createButtons()
    {
        m_pRemoveTermButton = new QToolButton(this);
        AssertReturnVoid(m_pRemoveTermButton);
        m_pRemoveTermButton->setIcon(m_pRemoveTermButton->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        m_pRemoveTermButton->hide();

        m_pClearAllButton = new QToolButton(this);
        AssertReturnVoid(m_pClearAllButton);
        m_pClearAllButton->setIcon(m_pRemoveTermButton->style()->standardIcon(QStyle::SP_LineEditClearButton));

        connect(m_pRemoveTermButton, &QToolButton::clicked, this, &UIVMFilterLineEdit::sltRemoveFilterTerm);
        connect(m_pClearAllButton, &QToolButton::clicked, this, &UIVMFilterLineEdit::sltClearAll);
    }

    QToolButton *m_pRemoveTermButton;
    QToolButton *m_pClearAllButton;
};

UIVMLogViewerFilterPanel::UIVMLogViewerFilterPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pViewer(pViewer)
    , m_pMainLayout(0)
    , m_pCloseButton(0)
    , m_pFilterLabel(0)
    , m_pFilterComboBox(0)
    , m_pButtonGroup(0)
    , m_pAndRadioButton(0)
    , m_pOrRadioButton(0)
    , m_pAddFilterTermButton(0)
    , m_eFilterOperatorButton(AndButton)
    , m_pFilterTermsLineEdit(0)
{
    prepare();
}

void UIVMLogViewerFilterPanel::applyFilter(const int iCurrentIndex /* = 0 */)
{
    Q_UNUSED(iCurrentIndex);
    QPlainTextEdit *pCurrentPage = m_pViewer->currentLogPage();
    AssertReturnVoid(pCurrentPage);
    const QString& strInputText = m_pViewer->currentLog();
    if (strInputText.isNull())
        return;
    if (m_filterTermList.empty())
    {
        QTextDocument *document = pCurrentPage->document();
        if (document)
            document->setPlainText(strInputText);
        emit sigFilterApplied();
        return;
    }
    QStringList stringLines = strInputText.split("\n");
    /* Prepare filter-data: */
    QString strFilteredText;
    int count = 0;
    for (int lineIdx = 0; lineIdx < stringLines.size(); ++lineIdx)
    {
        const QString& currentLineString = stringLines[lineIdx];
        if (currentLineString.isEmpty())
            continue;
        if (applyFilterTermsToString(currentLineString))
        {
            strFilteredText.append(currentLineString).append("\n");
            ++count;
        }
    }

    QTextDocument *document = pCurrentPage->document();
    if (document)
        document->setPlainText(strFilteredText);

    /* Move the cursor position to end: */
    QTextCursor cursor = pCurrentPage->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    pCurrentPage->setTextCursor(cursor);

    emit sigFilterApplied();
}


bool UIVMLogViewerFilterPanel::applyFilterTermsToString(const QString& string)
{
    /* Number of the filter terms contained with the @p string: */
    int hitCount = 0;
    for (int i = 0; i < m_filterTermList.size(); ++i)
    {
        const QRegExp rxFilterExp(m_filterTermList.at(i), Qt::CaseInsensitive);
        /* Disregard empty and invalid filter terms: */
        if (rxFilterExp.isEmpty() || !rxFilterExp.isValid())
            continue;

        if (string.contains(rxFilterExp))
        {
            ++hitCount;
            /* Early return */
            if (m_eFilterOperatorButton == OrButton)
                return true;
        }

        /* Early return */
        if (!string.contains(rxFilterExp) && m_eFilterOperatorButton == AndButton )
            return false;
    }
    /* All the terms are found within the @p string. To catch AND case: */
    if (hitCount == m_filterTermList.size())
        return true;
    return false;
}


void UIVMLogViewerFilterPanel::sltAddFilterTerm()
{
    if(!m_pFilterComboBox)
        return;
    if(m_pFilterComboBox->currentText().isEmpty())
        return;

    /* Continue only if the term is new. */
    if (m_filterTermList.contains(m_pFilterComboBox->currentText()))
        return;
    m_filterTermList.push_back(m_pFilterComboBox->currentText());

    /* Add the new filter term to line edit: */
    if (m_pFilterTermsLineEdit)
        m_pFilterTermsLineEdit->addFilterTerm(m_pFilterComboBox->currentText());

    /* Clear the content of the combo box: */
    m_pFilterComboBox->setCurrentText(QString());
    applyFilter();
}

void UIVMLogViewerFilterPanel::sltClearFilterTerms()
{
    if(m_filterTermList.empty())
        return;
    m_filterTermList.clear();
    applyFilter();
    if (m_pFilterTermsLineEdit)
        m_pFilterTermsLineEdit->clearAll();
}

void UIVMLogViewerFilterPanel::sltOperatorButtonChanged(int buttonId)
{
    if (buttonId < 0 || buttonId >= ButtonEnd)
        return;
    m_eFilterOperatorButton = static_cast<FilterOperatorButton>(buttonId);
    applyFilter();
}

void UIVMLogViewerFilterPanel::sltRemoveFilterTerm(const QString &termString)
{
    QStringList newList;
    for (QStringList::iterator iterator = m_filterTermList.begin();
        iterator != m_filterTermList.end(); ++iterator)
    {
        if ((*iterator) != termString)
            newList.push_back(*iterator);
    }
    m_filterTermList = newList;
    applyFilter();
}

void UIVMLogViewerFilterPanel::prepare()
{
    prepareWidgets();
    prepareConnections();
    retranslateUi();
}

void UIVMLogViewerFilterPanel::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(4);

    m_pCloseButton = new UIMiniCancelButton(this);
    AssertPtrReturnVoid(m_pCloseButton);
    {
        m_pMainLayout->addWidget(m_pCloseButton);
    }

    m_pFilterTermsLineEdit = new UIVMFilterLineEdit(this);
    AssertPtrReturnVoid(m_pFilterTermsLineEdit);
    {
        m_pMainLayout->addWidget(m_pFilterTermsLineEdit, 4);
        m_pFilterTermsLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum /*vertical */);
    }

    m_pButtonGroup = new QButtonGroup(this);
    AssertPtrReturnVoid(m_pButtonGroup);
    {
        m_pAndRadioButton = new QRadioButton(this);
        m_pOrRadioButton =  new QRadioButton(this);
        AssertPtrReturnVoid(m_pAndRadioButton);
        AssertPtrReturnVoid(m_pOrRadioButton);
        m_pButtonGroup->addButton(m_pAndRadioButton, static_cast<int>(AndButton));
        m_pButtonGroup->addButton(m_pOrRadioButton, static_cast<int>(OrButton));

        m_pOrRadioButton->setText("Or");
        m_pAndRadioButton->setText("And");

        m_pOrRadioButton->setChecked(true);
        m_eFilterOperatorButton = OrButton;

        m_pMainLayout->addWidget(m_pOrRadioButton);
        m_pMainLayout->addWidget(m_pAndRadioButton);
    }

    m_pFilterComboBox = new QComboBox(this);
    AssertPtrReturnVoid(m_pFilterComboBox);
    {
        m_pFilterComboBox->setEditable(true);
        QStringList strFilterPresets;
        strFilterPresets << "" << "GUI" << "NAT" << "AHCI" << "VD" << "Audio" << "VUSB" << "SUP" << "PGM" << "HDA"
                         << "HM" << "VMM" << "GIM" << "CPUM";
        strFilterPresets.sort();
        m_pFilterComboBox->addItems(strFilterPresets);
        m_pMainLayout->addWidget(m_pFilterComboBox,1);
    }

    m_pAddFilterTermButton = new QPushButton(this);
    AssertPtrReturnVoid(m_pAddFilterTermButton);
    {
        m_pMainLayout->addWidget(m_pAddFilterTermButton,0);
    }

        /* Create filter-label: */
//         m_pFilterLabel = new QLabel(this);
//         AssertPtrReturnVoid(m_pFilterLabel);
//         {
//             /* Configure filter-label: */
//             m_pFilterLabel->setBuddy(m_pFilterComboBox);
// #ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS
//             QFont font = m_pFilterLabel->font();
//             font.setPointSize(::darwinSmallFontSize());
//             m_pFilterLabel->setFont(font);
// #endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */
//             /* Add filter-label to main-layout: */
//             m_pMainLayout->addWidget(m_pFilterLabel);
//         }
}

void UIVMLogViewerFilterPanel::prepareConnections()
{
    connect(m_pCloseButton, &UIMiniCancelButton::clicked, this, &UIVMLogViewerFilterPanel::hide);
    connect(m_pAddFilterTermButton, &QPushButton::clicked, this,  &UIVMLogViewerFilterPanel::sltAddFilterTerm);
    connect(m_pButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, &UIVMLogViewerFilterPanel::sltOperatorButtonChanged);
    connect(m_pFilterComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIVMLogViewerFilterPanel::sltAddFilterTerm);
    connect(m_pFilterTermsLineEdit, &UIVMFilterLineEdit::sigFilterTermRemoved,
            this, &UIVMLogViewerFilterPanel::sltRemoveFilterTerm);
    connect(m_pFilterTermsLineEdit, &UIVMFilterLineEdit::sigClearAll,
            this, &UIVMLogViewerFilterPanel::sltClearFilterTerms);
}

void UIVMLogViewerFilterPanel::retranslateUi()
{
    m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel"));
    //m_pFilterLabel->setText(UIVMLogViewerWidget::tr("Filter"));
    m_pFilterComboBox->setToolTip(UIVMLogViewerWidget::tr("Enter filtering string here"));
    m_pAddFilterTermButton->setText(UIVMLogViewerWidget::tr("Add"));
    m_pAddFilterTermButton->setToolTip(UIVMLogViewerWidget::tr("Add filter term"));
}

bool UIVMLogViewerFilterPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Process key press only: */
    case QEvent::KeyPress:
        {
            /* Cast to corresponding key press event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            /* Handle Ctrl+T key combination as a shortcut to focus search field: */
            if (pKeyEvent->QInputEvent::modifiers() == Qt::ControlModifier &&
                pKeyEvent->key() == Qt::Key_T)
            {
                if (m_pViewer->currentLogPage())
                {
                    if (isHidden())
                        show();
                    m_pFilterComboBox->setFocus();
                    return true;
                }
            }
            else if (pKeyEvent->key() == Qt::Key_Return && m_pFilterComboBox && m_pFilterComboBox->hasFocus())
                sltAddFilterTerm();

            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

/** Handles the Qt show @a pEvent. */
void UIVMLogViewerFilterPanel::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::showEvent(pEvent);
    /* Set focus to combo-box: */
    m_pFilterComboBox->setFocus();
}

/** Handles the Qt hide @a pEvent. */
void UIVMLogViewerFilterPanel::hideEvent(QHideEvent *pEvent)
{
    /* Get focused widget: */
    QWidget *pFocus = QApplication::focusWidget();
    /* If focus-widget is valid and child-widget of search-panel,
     * focus next child-widget in line: */
    if (pFocus && pFocus->parent() == this)
        focusNextPrevChild(true);
    /* Call to base-class: */
    QWidget::hideEvent(pEvent);
}

#include "UIVMLogViewerFilterPanel.moc"
