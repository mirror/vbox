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
# include <QFrame>
# include <QHBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QLabel>
# include <QLineEdit>
# include <QPlainTextEdit>
# include <QTextCursor>
# include <QRadioButton>
# include <QScrollArea>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIVMLogPage.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIVMFilterLineEdit definition.                                                                                               *
*********************************************************************************************************************************/

/** UIVMFilterLineEdit class is used to display and modify the list of filter terms.
 *  the terms are displayed as words with spaces in between and it is possible to
 *  remove these terms one by one by selecting them or completely by the clearAll button
 *  located on the right side of the line edit: */
class UIVMFilterLineEdit : public QLineEdit
{
    Q_OBJECT;

signals:

    void sigFilterTermRemoved(QString removedString);
    void sigClearAll();

public:

    UIVMFilterLineEdit(QWidget *parent = 0);
    void addFilterTerm(const QString& filterTermString);
    void clearAll();

protected:

    /* Delete mouseDoubleClick and mouseMoveEvent implementations of the base class */
    virtual void        mouseDoubleClickEvent(QMouseEvent *) /* override */{}
    virtual void        mouseMoveEvent(QMouseEvent *) /* override */{}
    /* Override the mousePressEvent to control how selection is done: */
    virtual void        mousePressEvent(QMouseEvent * event) /* override */;
    virtual void        mouseReleaseEvent(QMouseEvent *){}
    virtual void        paintEvent(QPaintEvent *event) /* override */;
    virtual void        resizeEvent(QResizeEvent *event) /* override */;

private slots:

    /* Nofifies the listeners that selected word (filter term) has been removed: */
    void sltRemoveFilterTerm();
    /* The whole content is removed. Listeners are notified: */
    void sltClearAll();

private:

    void          createButtons();
    QIToolButton *m_pRemoveTermButton;
    QIToolButton *m_pClearAllButton;
    const int     m_iRemoveTermButtonSize;
    int           m_iTrailingSpaceCount;
};


/*********************************************************************************************************************************
*   UIVMFilterLineEdit implementation.                                                                                           *
*********************************************************************************************************************************/

UIVMFilterLineEdit::UIVMFilterLineEdit(QWidget *parent /*= 0*/)
    :QLineEdit(parent)
    , m_pRemoveTermButton(0)
    , m_pClearAllButton(0)
    , m_iRemoveTermButtonSize(16)
    , m_iTrailingSpaceCount(1)
{
    setReadOnly(true);
    home(false);
    createButtons();
    /** Try to guess the width of the space between filter terms so that remove button
        we display when a term is selected does not hide the next/previous word: */
    int spaceWidth = fontMetrics().width(' ');
    if (spaceWidth != 0)
        m_iTrailingSpaceCount = (m_iRemoveTermButtonSize / spaceWidth) + 1;
}

void UIVMFilterLineEdit::addFilterTerm(const QString& filterTermString)
{
    if (text().isEmpty())
        insert(filterTermString);
    else
    {
        QString newString(filterTermString);
        QString space(m_iTrailingSpaceCount, QChar(' '));
        insert(newString.prepend(space));
    }
}

void UIVMFilterLineEdit::clearAll()
{
    if (text().isEmpty())
        return;
    sltClearAll();
}

void UIVMFilterLineEdit::mousePressEvent(QMouseEvent * event)
{
    /* Simulate double mouse click to select a word with a single click: */
    QLineEdit::mouseDoubleClickEvent(event);
}

void UIVMFilterLineEdit::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);

    if (!m_pClearAllButton || !m_pRemoveTermButton)
        createButtons();
    int clearButtonSize = height();

    int deltaHeight = 0.5 * (height() - m_pClearAllButton->height());
    m_pClearAllButton->setGeometry(width() - clearButtonSize, deltaHeight, clearButtonSize, clearButtonSize);
}

void UIVMFilterLineEdit::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);
    int clearButtonSize = height();
    /* If we have a selected term move the m_pRemoveTermButton to the end of the
       or start of the word (depending on the location of the word within line edit itself: */
    if (hasSelectedText())
    {
        int deltaHeight = 0.5 * (height() - m_pClearAllButton->height());
        m_pRemoveTermButton->show();
        int buttonSize = m_iRemoveTermButtonSize;
        int charWidth = fontMetrics().width('x');
        int buttonLeft = cursorRect().right() - 0.5 * charWidth;
        /* If buttonLeft is in far right of the line edit, move the
           button to left side of the selected word: */
        if (buttonLeft + buttonSize  >=  width() - clearButtonSize)
        {
            int selectionWidth = charWidth * selectedText().length();
            buttonLeft -= (selectionWidth + buttonSize);
        }
        m_pRemoveTermButton->setGeometry(buttonLeft, deltaHeight, buttonSize, buttonSize);
    }
    else
        m_pRemoveTermButton->hide();
}

void UIVMFilterLineEdit::sltRemoveFilterTerm()
{
    if (!hasSelectedText())
        return;
    emit sigFilterTermRemoved(selectedText());
    /* Remove the string from text() including the trailing space: */
    setText(text().remove(selectionStart(), selectedText().length() + m_iTrailingSpaceCount));
}

void UIVMFilterLineEdit::sltClearAll()
{
    /* Check if we have some text to avoid recursive calls: */
    if (text().isEmpty())
        return;

    clear();
    emit sigClearAll();
}

void UIVMFilterLineEdit::createButtons()
{
    if (!m_pRemoveTermButton)
    {
        m_pRemoveTermButton = new QIToolButton(this);
        if (m_pRemoveTermButton)
        {
            m_pRemoveTermButton->setIcon(UIIconPool::iconSet(":/log_viewer_delete_filter_16px.png"));
            m_pRemoveTermButton->hide();
            connect(m_pRemoveTermButton, &QIToolButton::clicked, this, &UIVMFilterLineEdit::sltRemoveFilterTerm);
        }
    }

    if (!m_pClearAllButton)
    {
        m_pClearAllButton = new QIToolButton(this);
        if (m_pClearAllButton)
        {
            m_pClearAllButton->setIcon(UIIconPool::iconSet(":/log_viewer_delete_all_filters_16px.png"));
            connect(m_pClearAllButton, &QIToolButton::clicked, this, &UIVMFilterLineEdit::sltClearAll);
        }
    }
    if (!m_pRemoveTermButton && !m_pClearAllButton)
        setMinimumHeight(qMax(m_pRemoveTermButton->minimumHeight(), m_pClearAllButton->minimumHeight()));
    else if (!m_pRemoveTermButton)
        setMinimumHeight(m_pRemoveTermButton->minimumHeight());
    else if (!m_pClearAllButton)
        setMinimumHeight(m_pClearAllButton->minimumHeight());
}


/*********************************************************************************************************************************
*   UIVMLogViewerFilterPanel implementation.                                                                                     *
*********************************************************************************************************************************/

UIVMLogViewerFilterPanel::UIVMLogViewerFilterPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : UIVMLogViewerPanel(pParent, pViewer)
    , m_pFilterLabel(0)
    , m_pFilterComboBox(0)
    , m_pButtonGroup(0)
    , m_pAndRadioButton(0)
    , m_pOrRadioButton(0)
    , m_pRadioButtonContainer(0)
    , m_pAddFilterTermButton(0)
    , m_eFilterOperatorButton(AndButton)
    , m_pFilterTermsLineEdit(0)
    , m_pResultLabel(0)
    , m_iUnfilteredLineCount(0)
    , m_iFilteredLineCount(0)
{
    prepare();
}

void UIVMLogViewerFilterPanel::applyFilter(const int iCurrentIndex /* = 0 */)
{
    Q_UNUSED(iCurrentIndex);
    filter();
    retranslateUi();
}

void UIVMLogViewerFilterPanel::filter()
{
    if (!viewer())
        return;
    QPlainTextEdit *pCurrentTextEdit = textEdit();
    if (!pCurrentTextEdit)
        return;

    UIVMLogPage *logPage = viewer()->currentLogPage();
    if (!logPage)
        return;
    /* Check if we have to reapply the filter. If not
       restore line counts etc. and return */
    if (!logPage->shouldFilterBeApplied(m_filterTermSet, (int)m_eFilterOperatorButton))
    {
        m_iFilteredLineCount = logPage->filteredLineCount();
        m_iUnfilteredLineCount = logPage->unfilteredLineCount();
        emit sigFilterApplied(!logPage->isFiltered() /* isOriginalLog */);
        return;
    }


    const QString* originalLogString = logString();
    m_iUnfilteredLineCount = 0;
    m_iFilteredLineCount = 0;
    if (!originalLogString || originalLogString->isNull())
        return;
    QTextDocument *document = textDocument();
    if (!document)
        return;
    QStringList stringLines = originalLogString->split("\n");
    m_iUnfilteredLineCount = stringLines.size();

    if (m_filterTermSet.empty())
    {
        document->setPlainText(*originalLogString);
        emit sigFilterApplied(true /* isOriginalLog */);
        m_iFilteredLineCount = document->lineCount();
        logPage->setFilterParameters(m_filterTermSet, (int)m_eFilterOperatorButton,
                                     m_iFilteredLineCount, m_iUnfilteredLineCount);
        return;
    }

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

    document->setPlainText(strFilteredText);
    m_iFilteredLineCount = document->lineCount();

    /* Move the cursor position to end: */
    QTextCursor cursor = pCurrentTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    pCurrentTextEdit->setTextCursor(cursor);

    emit sigFilterApplied(false /* isOriginalLog */);
    logPage->setFilterParameters(m_filterTermSet, (int)m_eFilterOperatorButton,
                                 m_iFilteredLineCount, m_iUnfilteredLineCount);
}


bool UIVMLogViewerFilterPanel::applyFilterTermsToString(const QString& string)
{
    /* Number of the filter terms contained with the @p string: */
    int hitCount = 0;

    for (QSet<QString>::const_iterator iterator = m_filterTermSet.begin();
        iterator != m_filterTermSet.end(); ++iterator)
    {
        const QString& filterTerm = *iterator;
        const QRegExp rxFilterExp(filterTerm, Qt::CaseInsensitive);
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
    if (hitCount == m_filterTermSet.size())
        return true;
    return false;
}


void UIVMLogViewerFilterPanel::sltAddFilterTerm()
{
    if (!m_pFilterComboBox)
        return;
    if (m_pFilterComboBox->currentText().isEmpty())
        return;

    /* Continue only if the term is new. */
    if (m_filterTermSet.contains(m_pFilterComboBox->currentText()))
        return;
    m_filterTermSet.insert(m_pFilterComboBox->currentText());

    /* Add the new filter term to line edit: */
    if (m_pFilterTermsLineEdit)
        m_pFilterTermsLineEdit->addFilterTerm(m_pFilterComboBox->currentText());

    /* Clear the content of the combo box: */
    m_pFilterComboBox->setCurrentText(QString());
    applyFilter();
}

void UIVMLogViewerFilterPanel::sltClearFilterTerms()
{
    if (m_filterTermSet.empty())
        return;
    m_filterTermSet.clear();
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
    m_filterTermSet.remove(termString);
    applyFilter();
}

void UIVMLogViewerFilterPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    prepareRadioButtonGroup();

    m_pFilterComboBox = new QComboBox;
    if (m_pFilterComboBox)
    {
        m_pFilterComboBox->setEditable(true);
        QStringList strFilterPresets;
        strFilterPresets << "" << "GUI" << "NAT" << "AHCI" << "VD" << "Audio" << "VUSB" << "SUP" << "PGM" << "HDA"
                         << "HM" << "VMM" << "GIM" << "CPUM";
        strFilterPresets.sort();
        m_pFilterComboBox->addItems(strFilterPresets);
        mainLayout()->addWidget(m_pFilterComboBox,1);
    }

    m_pAddFilterTermButton = new QIToolButton;
    if (m_pAddFilterTermButton)
    {
        m_pAddFilterTermButton->setIcon(UIIconPool::iconSet(":/log_viewer_filter_add_16px.png"));
        mainLayout()->addWidget(m_pAddFilterTermButton,0);
    }

    m_pFilterTermsLineEdit = new UIVMFilterLineEdit;
    if (m_pFilterTermsLineEdit)
    {
        mainLayout()->addWidget(m_pFilterTermsLineEdit, 4);
        m_pFilterTermsLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum /*vertical */);
    }

    m_pResultLabel = new QLabel;
    if (m_pResultLabel)
    {
        mainLayout()->addWidget(m_pResultLabel,0);
    }
}

void UIVMLogViewerFilterPanel::prepareRadioButtonGroup()
{
    /* Create radio-button container: */
    m_pRadioButtonContainer = new QFrame;
    if (m_pRadioButtonContainer)
    {
        /* Configure container: */
        m_pRadioButtonContainer->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

        /* Create container layout: */
        QHBoxLayout *pContainerLayout = new QHBoxLayout(m_pRadioButtonContainer);
        if (pContainerLayout)
        {
            /* Configure layout: */
#ifdef VBOX_WS_MAC
            pContainerLayout->setContentsMargins(5, 0, 0, 5);
            pContainerLayout->setSpacing(5);
#else
            pContainerLayout->setContentsMargins(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2, 0,
                                                 qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2, 0);
            pContainerLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif

            /* Create button-group: */
            m_pButtonGroup = new QButtonGroup(this);
            if (m_pButtonGroup)
            {
                /* Create 'Or' radio-button: */
                m_pOrRadioButton = new QRadioButton;
                if (m_pOrRadioButton)
                {
                    /* Configure radio-button: */
                    m_pButtonGroup->addButton(m_pOrRadioButton, static_cast<int>(OrButton));
                    m_pOrRadioButton->setChecked(true);
                    m_pOrRadioButton->setText("Or");

                    /* Add into layout: */
                    pContainerLayout->addWidget(m_pOrRadioButton);
                }

                /* Create 'And' radio-button: */
                m_pAndRadioButton = new QRadioButton;
                if (m_pAndRadioButton)
                {
                    /* Configure radio-button: */
                    m_pButtonGroup->addButton(m_pAndRadioButton, static_cast<int>(AndButton));
                    m_pAndRadioButton->setText("And");

                    /* Add into layout: */
                    pContainerLayout->addWidget(m_pAndRadioButton);
                }
            }
        }

        /* Add into layout: */
        mainLayout()->addWidget(m_pRadioButtonContainer);
    }

    /* Initialize other related stuff: */
    m_eFilterOperatorButton = OrButton;
}

void UIVMLogViewerFilterPanel::prepareConnections()
{
    connect(m_pAddFilterTermButton, &QIToolButton::clicked, this,  &UIVMLogViewerFilterPanel::sltAddFilterTerm);
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
    UIVMLogViewerPanel::retranslateUi();
    m_pFilterComboBox->setToolTip(UIVMLogViewerWidget::tr("Enter filtering string here."));
    m_pAddFilterTermButton->setToolTip(UIVMLogViewerWidget::tr("Add filter term. (Enter)"));
    m_pResultLabel->setText(UIVMLogViewerWidget::tr("Showing %1/%2").arg(m_iFilteredLineCount).arg(m_iUnfilteredLineCount));
    m_pFilterTermsLineEdit->setToolTip(UIVMLogViewerWidget::tr("The filter terms list. Select one to remove or click the button on the right side to remove them all."));
    m_pRadioButtonContainer->setToolTip(UIVMLogViewerWidget::tr("The type of boolean operator for filter operation."));
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
                if (isHidden())
                    show();
                m_pFilterComboBox->setFocus();
                return true;
            }
            else if (pKeyEvent->key() == Qt::Key_Return && m_pFilterComboBox && m_pFilterComboBox->hasFocus())
                sltAddFilterTerm();

            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return UIVMLogViewerPanel::eventFilter(pObject, pEvent);
}

/** Handles the Qt show @a pEvent. */
void UIVMLogViewerFilterPanel::showEvent(QShowEvent *pEvent)
{
    UIVMLogViewerPanel::showEvent(pEvent);
    /* Set focus to combo-box: */
    m_pFilterComboBox->setFocus();
}

#include "UIVMLogViewerFilterPanel.moc"
